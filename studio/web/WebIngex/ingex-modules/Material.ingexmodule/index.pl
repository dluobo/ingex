#!/usr/bin/perl -wT

# Copyright (C) 2009  British Broadcasting Corporation
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


#
# Materials page - displays a browsable tree containing a heirachal view of all materials.
#
# Materials can be filtered depending on a number of user-selectable parameters and selected materials
# can be packaged and exported to avid and final cut formats.
#

use strict;

use lib ".";
use lib "../../ingex-config";

use Template;
use prodautodb;
use db;
use ingexhtmlutil;
use ingexconfig;
use materialconfig;

use CGI::Pretty qw(:standard);

my $dbh = prodautodb::connect(
		$ingexConfig{"db_host"}, 
		$ingexConfig{"db_name"},
		$ingexConfig{"db_user"}, 
		$ingexConfig{"db_password"})
 	or die();

my $main_browser = get_main_browser();

print header;
print $main_browser;

exit(0);


#
# Create html for main browser panel containing the media files
# Generates a tree/grid browser using ext components
#
sub get_main_browser {
	# get video resolution formats
	my $vresIds = prodautodb::load_video_resolutions($dbh)
	  	or return_error_page(
			"failed to load video resolutions from the database: $prodautodb::errstr"
	  	);

	my $val      = "";
	my $name     = "";
	my $fmt_opts = "<OPTION value='-1'>[any resolution]</OPTION>";

	foreach my $vres ( @{$vresIds} ) {
		$val      = $vres->{'ID'};
		$name     = $vres->{'NAME'};
		$fmt_opts = $fmt_opts . "<OPTION value='$val'>$name</OPTION>";
	}

	# get project names
	my $projnames = db::load_projects($dbh)
	  	or return_error_page(
			"failed to load video resolutions from the database: $prodautodb::errstr"
	  	);

	my $id = "";
	$name = "";
	my $proj_opts = "<OPTION value='-1'>[any project]</OPTION>";

	foreach my $proj ( @{$projnames} ) {
		$id        = $proj->{'ID'};
		$name      = $proj->{'NAME'};
		$proj_opts = $proj_opts . "<OPTION value='$id'>$name</OPTION>";
	}

	# get export directories
	my $dest_directories = '';
	my $defaultSendTo;
	my @allExportDirs = get_all_avid_aaf_export_dirs();

	foreach my $exportDir (@allExportDirs) {
		if ( -e $exportDir->[1] ) {
			my $label = $exportDir->[0] . " ($exportDir->[1])";
			my $value = $exportDir->[0];
			$dest_directories .= "<option value='$value'>$label</option>";
			if ( !defined $defaultSendTo ) {
				$defaultSendTo = $value;
			}
		}
	}

	my $content;

	# html content for main page body
	my $file;
	if($ingexConfig{"drag_drop"} eq 'true') {
		$file = 'page_body_dnd.html'
	}
	else {
		$file = 'page_body.html';
	}
	
    my $vars = {
    	proj_opts			=> $proj_opts,
    	fmt_opts			=> $fmt_opts,
    	defaultSendTo		=> $defaultSendTo,
    	dest_directories	=> $dest_directories
    };
    
	my $template = Template->new({
    	INCLUDE_PATH => 'html:html/templates'
    });
           
    $template->process($file, $vars, \$content)
    	|| die $template->error();

	
	return $content;
}

END {
	prodautodb::disconnect($dbh) if ($dbh);
}

