#!/usr/bin/perl -wT

# Copyright (C) 2010  British Broadcasting Corporation
# Author: Sean Casey <seantc@users.sourceforge.net>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

##########################################################################################
#
# createPDFLog script - creates a pdf log sheet for editing purposes from supplied package 
# IDs containing any metadata logged during recording and timestamps
#
##########################################################################################

use warnings;


#!/usr/bin/perl -wT

# Copyright (C) 2010  British Broadcasting Corporation
# Author: Sean Casey <seantc@users.sourceforge.net>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

	# TODO:
	# CURRENTLY ONLY SUPPORTS 1 DAY
	

use lib ".";
use lib "../../ingex-config";

use XML::Simple;
use File::Temp qw(mktemp tempfile);
use JSON::XS;
use CGI ':standard';
use PDF::Create;
use logDb;
use ingexconfig;

# perl logging
use Sys::Syslog;
openlog('create_pdf', 'cons, pid', 'user');

syslog('info', "Generating PDF Log Sheet");

# unix tmp directory
my $tempdir = '/tmp';

# ingex db [materials db]
my $dbh = prodautodb::connect(
		$ingexConfig{"db_host"}, 
		$ingexConfig{"db_name"},
		$ingexConfig{"db_user"}, 
		$ingexConfig{"db_password"})
 	or die("DB Unavailable");

print header;

# get parameters from json
my $jsonStr = param('jsonIn');
my $json = decode_json($jsonStr);

# get package ids
my @pkgIds = ();
my @PkgNodes = @{$json->{'Root'}->[0]->{'ClipSettings'}};
foreach(@PkgNodes){
	push @pkgIds, $_->{'PkgID'};
}

# test data
#my @pkgIds = (4017, 4021, 4023, 4030, 4032, 4032, 4046, 4049,549,33);

my $logDb = logDb->new();
my $metadata = $logDb->get_metadata($dbh, \@pkgIds);


# only process distinct package ids
@pkgIds = ();
my ($pkgId, $pkgInfo);
while(($pkgId, $pkgInfo) = each(%{$metadata})) {
	push @pkgIds, $pkgId;
}

my $commentGroups = $logDb->get_user_comments($dbh, \@pkgIds);

my $takeStartPos;
my $takeNum = 0;
my $pageNum = 0;
my $id;
my $a4;
my $page;	# current page we are working on
my $textOffs;

# Doc Fonts
my $font_des;
my $font_title;

# generate the pdf
my $filename = write_pdf($metadata, $commentGroups );
my $results;
$results{"filename"} = $filename;
$json = encode_json( \%results );

print "ok~$json";	# success

$dbh->disconnect();
exit (0);


# generate the pdf from supplied metadata
sub write_pdf {
	my ($metadata, $data ) = @_;
	
	
	my $comments;
	
	my ( $fh, $filename ) = tempfile( DIR => $tempdir, SUFFIX => '.pdf' );	# create unique temp file

	my $pdf = new PDF::Create(				'filename'     => $filename,
	                                        'Author'       => 'John Doe',
	                                        'Title'        => 'Sample PDF',
	                                        'CreationDate' => [ localtime ], );
	                   
	# Prepare fonts
	$font_des = $pdf->font('BaseFont' => 'Helvetica');
	$font_title = $pdf->font('BaseFont' => 'Helvetica-Bold');
	                     
	# add a A4 sized page
	$a4 = $pdf->new_page('MediaBox' => $pdf->get_page_size('A4'));
	
	# new page
	new_page();
	
	# sort package ids by incrementing time positions:
	my $timePos;
	foreach( keys(%{$data})) {
		my $pos = $metadata->{$_}->{'position'};
		my $pkgid = $_;
		$timePos->{$pos} = $pkgid;		# lookup table for pakage ids ordered by time
	}
	my @srtPos = sort{$a cmp $b} (keys %{$timePos});	# ordered position list
	
	# calculate total run duration
	my $startLogPos = $srtPos[0];
	my $lastPos = $srtPos[-1];
	my $endLogPos = $lastPos + $metadata->{$timePos->{$lastPos}}->{'length'};
	my $totalLen = $endLogPos - $startLogPos;
	my $firstPkgId = $timePos->{$startLogPos};
	
	add_titles( 'Project: '.$metadata->{$firstPkgId}->{'projname'}, $metadata->{$firstPkgId}->{'date'}, $totalLen);
	
	foreach(@srtPos){
		$pkgId = $timePos->{$_};	# lookup package for this time
		$pkgInfo = $data->{$pkgId};
	    
        syslog('info', "Package position for $pkgId: ".$metadata->{$pkgId}->{'position'});

		add_metadata($pkgInfo->{'description'}, $pkgInfo->{'organisation'}, $pkgInfo->{'location'});
	    take_title($metadata->{$pkgId}->{'position'}, $metadata->{$pkgId}->{'length'});
	    
	    # Add any cue point user comments
	    if($pkgInfo->{'comments'}){
	    	my @comments = @{$pkgInfo->{'comments'}};
	    
		    comments_title();
		    
			foreach(@comments){
	            syslog('info', "logging for $pkgId ".$_->{'value'});
				add_comment($_->{'position'}, $_->{'value'});
			}
		}
		
		new_take();
	}
	
	# Close system log
	closelog();

	# Close the file and write the PDF
	$pdf->close;	
	return $filename;
}

# start a new page
sub new_page{
	# Add a page which inherits its attributes from $a4
	$page = $a4->new_page;
	$textOffs = 750;
	add_page_num();
}

# add titles and metadata
sub add_titles{
	my($title, $date, $duration) = @_;
	my $durationStr = get_timecode_string($duration);
	$page->stringl($font_des, 20, 100, $textOffs, "$title");
	$textOffs -= 25;
	($date) = ($date =~ /(\d*-\d*-\d*)/);
	$page->stringl($font_des, 12, 100, $textOffs, "Recording Date: $date     Total Duration: $durationStr");
	$textOffs -= 15;
	$page->stringl($font_des, 12, 100, $textOffs, "Log Sheet");
	$textOffs -= 10;
	$page->line(100, $textOffs, 400, $textOffs); 
	$textOffs -= 30;
}

# add a separator line from last take and add title
sub new_take{
	hl();
	$textOffs -= 20;
}

# add a description for take
sub add_metadata{
	my ($description, $organisation, $location) = @_;
	
#	$page->stringl($font_title, 10, 100,  $textOffs, "Description:");
#	$page->stringl($font_des, 10, 200,  $textOffs, "$description");
	if ($description eq '' || !defined $description){$description = '[No description]';}
	$page->string($font_title, 10, 306,  $textOffs, "$description", 'c');
	cr();
	
#	$page->stringl($font_des, 10, 100,  $textOffs, "Organisation:");
#	$page->stringl($font_des, 10, 200,  $textOffs, "$organisation");
#	cr();
#	
#	$page->stringl($font_des, 10, 100,  $textOffs, "Location:");
#	$page->stringl($font_des, 10, 200,  $textOffs, "$location");
#	cr();
}

# add a cue point comment
sub add_comment{
	my ($position, $text) = @_;
	
	# calculate absolute time (not relative) and create a string
	my $absPos = $takeStartPos + $position;
	my $posStr = get_timecode_string($absPos);
	
	$page->stringl($font_des, 10, 100,  $textOffs, "$posStr");
	$page->stringl($font_des, 10, 200,  $textOffs, "$text");
	
	cr();
}

# add title for the recording
sub take_title{
	my ($startpos, $duration) = @_;
	
	$takeStartPos = $startpos;
	my $endTime = $takeStartPos + $duration;
	$takeNum++;
	my $endStr = get_timecode_string($endTime);
	my $startStr = get_timecode_string($startpos);
	$page->string($font_des, 10, 306, $textOffs, "[ Start Time: $startStr     Out Time: $endStr ]", 'c');

	cr();
}

# add title for the cue point comments
sub comments_title{
	hl();
	
	$page->stringl($font_title, 10, 100,  $textOffs, "Time");
	$page->stringl($font_title, 10, 200,  $textOffs, "Comments");

	cr();
}

# carriage return
sub cr{
	$textOffs -= 12;
	
	if ($textOffs <= 100){
		new_page();
	}
}

# horizontal line
sub hl{
	$page->line(100, $textOffs+6, 550, $textOffs+6); 
	$textOffs -= 6; # 1/2 line
}

# borrowed from db.pm        
sub get_timecode_string 
{
	my ($position) = @_;

	my ( $hour, $min, $sec, $fr );

	$hour = int( $position / ( 60 * 60 * 25 ) );
	$min = int( ( $position % ( 60 * 60 * 25 ) ) / ( 60 * 25 ) );
	$sec = int( ( ( $position % ( 60 * 60 * 25 ) ) % ( 60 * 25 ) ) / 25 );
	$fr = int( ( ( ( $position % ( 60 * 60 * 25 ) ) % ( 60 * 25 ) ) % 25 ) );

	return sprintf( "%02d:%02d:%02d:%02d", $hour, $min, $sec, $fr );
}

# add a page number
sub add_page_num
{
	$pageNum++;
	$page->string($font_des, 10, 306, 50, "$pageNum", 'c');
}
