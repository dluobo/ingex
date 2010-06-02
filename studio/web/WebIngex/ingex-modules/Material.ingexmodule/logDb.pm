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

##########################################################################################
#
# logDb class - supporting sql functions for create pdf log script 
#
##########################################################################################

package logDb;

use lib ".";
use lib "../../ingex-config";

use warnings;
use prodautodb;
use db;
my $sth;
my $utils;

# constructor
sub new {
	my ($type) = shift;
	my %params = @_;	# parameters passed to the constructor - none required
	my $self = {};
	
	return bless $self, $type;
}

# return comments and cue points associated with a user id
sub get_user_comments {
	my ($self, $dbh, $pkgids) = @_;

	my $pkgs;
	
	#iterrate through each package id
	foreach(@{$pkgids}){	
	
		my $nextpkg = getClipMetadata($self, $_, $dbh);

		$pkgs->{$_} = $nextpkg;
	}
	
	return $pkgs;
}

# get additional package metadata
sub get_metadata {
	my ($self, $dbh, $pkgids) = @_;
	
	my $out;
	$pkgId = join(',', @{$pkgids});
	my $condition = "($pkgId)"; 
			
	my $query = "
			select DISTINCT ON(file_pkg.pkg_creation_date) 
			        material_pkg.pkg_identifier as ID, 
			        file_pkg.pkg_identifier as fileID, 
			        source_pkg.pkg_identifier as sourceID,
			        source_pkg.pkg_name as tapeID,
					projectname.pjn_name as projName,
			        essence.eds_identifier AS desID,
			        essence.eds_file_location AS fname,
			        file_pkg.pkg_source_config_name AS src_conf_name,
			        file_scp.scp_position AS position, file_scp.scp_length AS length, 
			        file_pkg.pkg_creation_date AS date, 
			        essence.eds_video_resolution_id AS resolution  
			        
			        from 
			        sourceclip AS   source_scp,
			        track AS        source_trk,
			        package AS      source_pkg,
			        sourceclip AS           file_scp,
			        track AS        file_trk,                               
			        package AS     file_pkg
						join projectname on file_pkg.pkg_project_name_id = projectname.pjn_identifier,
			        sourceclip AS        material_scp, 
			        track AS      material_trk,                  
			        package AS material_pkg, 
			        essencedescriptor AS essence  
			
			        where
			 
			        source_trk.trk_identifier = source_scp.scp_track_id AND 
			        source_pkg.pkg_identifier = source_trk.trk_package_id AND 
			
			        file_scp.scp_source_package_uid = source_pkg.pkg_uid AND 
			        file_trk.trk_identifier = file_scp.scp_track_id AND 
			        file_pkg.pkg_identifier = file_trk.trk_package_id AND
			 
			        material_scp.scp_source_package_uid = file_pkg.pkg_uid AND 
			        material_trk.trk_identifier = material_scp.scp_track_id AND 
			        material_pkg.pkg_identifier = material_trk.trk_package_id AND 
			
			        file_trk.trk_data_def = 1 AND
			        material_pkg.pkg_descriptor_id IS NULL AND 
			        
			        essence.eds_identifier = file_pkg.pkg_descriptor_id AND 
			        
			        material_pkg.pkg_identifier
			        	IN $condition;
	        	";
			        	
	my $sth = $dbh->prepare($query);

	$sth->execute
		or die("Error looking up source package: ".$@);
		
	while(my $row = $sth->fetchrow_hashref()){

		my $pkg;
		
		$pkg->{'date'} = $row->{"DATE"};
		$pkg->{'position'} = $row->{"POSITION"};
		$pkg->{'length'} = $row->{"LENGTH"};
		$pkg->{'projname'} = $row->{"PROJNAME"};
		$pkg->{'fileid'} = $row->{"FILEID"};
		$pkg->{'tapeid'} = $row->{"TAPEID"};
		
		$out->{$row->{'ID'}} = $pkg;
	}
	
	$sth->finish;
	return $out;
}

#
# get cut points and extra metadata associated with clip id
# from review test->utils class
#
sub getClipMetadata
{
	my ($self, $pkgId, $dbh) = @_;	# the materials package
	
	my $value;
	my $type;
	my $data;
	my @comments;
	my @sources;
	
	my @pkgId = split(' ', $pkgId);
	@pkgId = (@pkgId);
	$pkgId = join(',', @pkgId);
	$data->{"PkgId"} = $pkgId;
	my $condition = "($pkgId)"; 
	
	my $query = "
            SELECT DISTINCT
            	uct_name AS name,
            	uct_value AS value,
            	uct_position AS position, 
            	uct_colour AS colour 
            FROM 
            	usercomment
            WHERE 
            	uct_package_id IN $condition
            ORDER BY
            	position ASC
            ";
	my $sth = $dbh->prepare( $query );
	$sth->execute;
	
	while(my $row = $sth->fetchrow_hashref()){
		$type = $row->{"NAME"};
		$value = $row->{"VALUE"};
		
		SWITCH: {
			if($type eq "Shoot Date"){
				$data->{"date"} = $value;
				last SWITCH;
			}
			if($type eq "Descript"){
				$data->{"description"} = $value;
				last SWITCH;
			}
			if($type eq "Location"){
				$data->{"location"} = $value;
				last SWITCH;
			}
			if($type eq "Organization"){
				$data->{"organisation"} = $value;
				last SWITCH;
			}
			if($type eq "Source"){
				push(@sources, $value);
				last SWITCH;
			}
			if($type eq "Comment"){
				my $comment;
				$comment->{"position"} = $row->{"POSITION"};
				$comment->{"colour"} = $row->{"COLOUR"};
				$comment->{"value"} = $row->{"VALUE"};
				push(@comments, $comment);
				last SWITCH;
			}
		}
	}
	
	$data->{"source"} = \@sources;
	
	if(scalar @comments){
		$data->{"comments"} = \@comments;
	}
		
	return $data;
}


1;
