#!/usr/bin/perl -wT

# Copyright (C) 2008  British Broadcasting Corporation
# Author: Rowan de Pomerai <rdepom@users.sourceforge.net>
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
 
use strict;

use CGI::Pretty qw(:standard);

use lib ".";
use lib "../../ingex-config";
use htmlutil; use ingexhtmlutil;;



my $page = get_page_content() 
    or return_error_page("failed to fill in content for index page");
   
print header;
print $page;

exit(0);



sub get_page_content
{
	my @content;
	
	push (@content, p({-id=>"setupAboutCallout", -class=>"infoBox"}, "Info"));
	push (@content, h1("Setup"));
	push (@content, p("<< Select a setup category from the menu"));
	#push (@content, p({-class => 'warningText'}, "Setup incomplete! Please fill out compulsory sections"));
#	push (@content, "<div class=\'displayOptions\'><h5>Display Options:</h5> <a href=\'javascript:toggleTC()\'>Toggle timecode frame display</a><br><a href=\'javascript:toggleHelp()\'>Hide contextual help items</a></div>");
	
    return join("", @content);
}


