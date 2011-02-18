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
use Text::Wrap;
use logDb;
use ingexconfig;
use POSIX;

use constant SECTION_COLOUR		=> 8;	#ÊCuepoint colour to represent new programme sections [Black]

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

my $sectionLabel;
my $sectionStartOffs;
my $sectionEndOffs;

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
my $pdf;
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
	my ($metadata, $data) = @_;
	
	my $comments;
	my $endPos;
	
	my ( $fh, $filename ) = tempfile( DIR => $tempdir, SUFFIX => '.pdf' );	# create unique temp file

	$pdf = new PDF::Create(					'filename'     => $filename,
	                                        'Author'       => '',
	                                        'Title'        => 'PDF Logsheet',
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
	my @srtPos = sort{$a <=> $b} (keys %{$timePos});	# ordered position list
	
	# calculate total run duration
	my $startLogPos = $srtPos[0];
	my $lastPos = $srtPos[-1];
	my $endLogPos = $lastPos + $metadata->{$timePos->{$lastPos}}->{'length'};
	my $totalLen = $endLogPos - $startLogPos;
	my $firstPkgId = $timePos->{$startLogPos};
	
	add_titles( 'Project: '.$metadata->{$firstPkgId}->{'projname'}, $metadata->{$firstPkgId}->{'date'}, $totalLen);
	
	foreach(@srtPos){
		$sectionLabel = '';
		$sectionStartOffs = 0;
		$sectionEndOffs = 0;

		$pkgId = $timePos->{$_};	# lookup package for this time
		$pkgInfo = $data->{$pkgId};
	    
        syslog('info', "Package position for $pkgId: ".$metadata->{$pkgId}->{'position'});

		add_metadata($pkgInfo->{'description'}, $pkgInfo->{'organisation'}, $pkgInfo->{'location'});
	    take_title($metadata->{$pkgId}->{'position'}, $metadata->{$pkgId}->{'length'});
	    my $takeEndPos = $metadata->{$pkgId}->{'length'};
	
	    # Add any cue point user comments
	    if($pkgInfo->{'comments'}){
	    	my @comments = @{$pkgInfo->{'comments'}};
	    
		    comments_title();
		   
			for($i=0; $i<scalar(@comments); $i++){
				$comment = $comments[$i];
				$endPos = (($i+1) < scalar(@comments)) ? $comments[$i+1]->{'position'} + $takeStartPos : $takeEndPos + $takeStartPos;
				syslog('info', "logging for $pkgId ".$comment->{'value'});
				
				if($comment->{'colour'} == SECTION_COLOUR) {
					if(!$sectionStartOffs){		# First section
						$sectionStartOffs = $textOffs+5;
						$sectionLabel = $comment->{'value'}
					}
					else{
						$sectionEndOffs = $textOffs+12;
						mark_section($sectionLabel, $sectionStartOffs, $sectionEndOffs);
						$sectionStartOffs = $textOffs+5;
						$sectionLabel = $comment->{'value'}
					}
				}
				
				add_comment($comment->{'position'}, $comment->{'value'}, $endPos);
			}
			
			# close any open sections
			if($sectionLabel){
				$sectionEndOffs = $textOffs+12;
				mark_section($sectionLabel, $sectionStartOffs, $sectionEndOffs);
				$sectionStartOffs = 0;
				$sectionEndOffs = 0;
				$sectionLabel = '';
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
	
	# close any open sections
	if($sectionLabel){
		$sectionEndOffs = $textOffs+12;
		mark_section($sectionLabel, $sectionStartOffs, $sectionEndOffs);
		$sectionStartOffs = 755;
		$sectionEndOffs = 0;
	}
	
	# Add a page which inherits its attributes from $a4
	$page = $a4->new_page;
	$textOffs = 750;
	add_page_num();
}

# add titles and metadata
sub add_titles{
	my($title, $date, $duration) = @_;
	
	# Add icon
	eval{
		my $imgref = $pdf->image("/srv/www/htdocs/ingex/img/logo2-big.jpg");
		$page->image('image' => $imgref, 'xpos'=>413, 'ypos'=>$textOffs, 'xscale'=>0.2, 'yscale'=>0.2);
	};
	if ($@){
		syslog('err', "Failed to load icon: $@");
	}
	
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
	my ($position, $text, $absEndPos) = @_;
	
	# calculate absolute time (not relative) and create a string
	my $absPos = $takeStartPos + $position;
	my $duration = $absEndPos - $absPos;
	my $posStr = get_timecode_string($absPos);
	my $durationStr = get_timecode_string($duration, 3);
	
	$page->stringl($font_des, 10, 100,  $textOffs, "$posStr");
	$page->stringl($font_des, 10, 200,  $textOffs, "$text");
	$page->stringl($font_des, 10, 510,  $textOffs, "$durationStr");
	
	cr();
}

# mark a programme section on LHS of sheet
sub mark_section{
	my ($sectionName, $sectionStartOffs, $sectionEndOffs) = @_;
	
	my $labelOffs = $sectionEndOffs-2+($sectionStartOffs-$sectionEndOffs)/2;
	
	# draw a bracket and label
	$page->newpath();
	$page->moveto(93, $sectionStartOffs);
	$page->lineto(90,$sectionStartOffs);
	$page->lineto(90,$sectionEndOffs);
	$page->lineto(93,$sectionEndOffs);
	$page->stroke();
	
	my $paragraph = wrapped(12, $sectionName);
	my $startOffs = $sectionEndOffs-2+($sectionStartOffs-$sectionEndOffs)/2;
	my $borderOffs = $sectionEndOffs-2;
	
	print_multi($paragraph, $startOffs, $borderOffs)
}

# print multiline text between 2 points
sub print_multi {
	my ($paragraph, $startOffs, $borderOffs) = @_;
	my $clipped;
	
	# calculate max permissable lines in the space we have...
	my $maxLines = floor(($startOffs-$borderOffs)/10)+1;
	
	# clip excess lines
	my $lines = scalar @$paragraph;
	($lines>$maxLines) and $lines = $maxLines and $clipped = 1;
	
	# calculate start point
	$startOffs += (($lines-1)*5);
	
	for($l=0; $l<$lines; $l++){
		my $line = @$paragraph[$l];
		($l+1 == $lines) and $clipped and $line = $line."...";
		$page->string($font_des, 10, 85, $startOffs, $line, 'r');
		$startOffs -= 10;
	}
}

# return wrapped version of paragraph with line breaks
sub wrapped {
	my ($columns, $textin) = @_;
		
	$Text::Wrap::columns = $columns;
	$Text::Wrap::separator = ":::";
	
	my $text = wrap("","",$textin);
	my @paragraph = split(/:::/, $text);
	
	return \@paragraph;
}

# add title for the recording
sub take_title{
	my ($startpos, $duration) = @_;
	
	$takeStartPos = $startpos;
	my $endTime = $takeStartPos + $duration;
	$takeNum++;
	my $endStr = get_timecode_string($endTime);
	my $startStr = get_timecode_string($startpos);
	$page->string($font_des, 10, 306, $textOffs, "[ Record Start Time: $startStr     Out Time: $endStr ]", 'c');

	cr();
}

# add title for the cue point comments
sub comments_title{
	hl();
	
	$page->stringl($font_title, 10, 100,  $textOffs, "Time");
	$page->stringl($font_title, 10, 200,  $textOffs, "Comments");
	$page->stringl($font_title, 10, 510,  $textOffs, "Duration");

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
	my ($position, $length) = @_;

	my ( $hour, $min, $sec, $fr );

	$hour = int( $position / ( 60 * 60 * 25 ) );
	$min = int( ( $position % ( 60 * 60 * 25 ) ) / ( 60 * 25 ) );
	$sec = int( ( ( $position % ( 60 * 60 * 25 ) ) % ( 60 * 25 ) ) / 25 );
	$fr = int( ( ( ( $position % ( 60 * 60 * 25 ) ) % ( 60 * 25 ) ) % 25 ) );

	!$length and return sprintf( "%02d:%02d:%02d:%02d", $hour, $min, $sec, $fr );
	$length == 1 and return sprintf( "%02d", $fr );
	$length == 2 and return sprintf( "%02d:%02d", $sec, $fr );
	$length == 3 and return sprintf( "%02d:%02d:%02d", $min, $sec, $fr );
	$length == 4 and return sprintf( "%02d:%02d:%02d:%02d", $hour, $min, $sec, $fr );
}

# add a page number
sub add_page_num
{
	$pageNum++;
	$page->string($font_des, 10, 306, 50, "$pageNum", 'c');
}
