#!/usr/bin/perl -wT

# Copyright (C) 2008  British Broadcasting Corporation
# Author: Philip de Nier <philipn@users.sourceforge.net>
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
use Encode;

use lib ".";
use lib "../../ingex-config";
use ingexconfig;
use prodautodb;
use db;
use datautil;
use htmlutil; use ingexhtmlutil;;



my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}) 
    or die();


my $errorMessage;

if (defined param("Cancel"))
{
    redirect_to_page("recorder.pl");
}
elsif (defined param("Create"))
{
    if (!($errorMessage = validate_params()))
    {
        my $rec = create_recorder(trim(param("name")),
                                  param("numinputs"), param("numtracks"));

        my $recId = save_recorder($dbh, $rec) 
            or return_create_page("failed to save recorder to database: $prodautodb::errstr");
        
        redirect_to_page("editrec.pl?id=$recId");
    }
}

return_create_page($errorMessage);



sub validate_params
{
    return "Error: Empty name" if (!defined param("name") || param("name") =~ /^\s*$/);
    
    return "Error: Invalid number of inputs" if (!param("numinputs") || 
        param("numinputs") !~ /^\d+$/);
        
    return "Error: Invalid number of tracks per input" if (!param("numtracks") || 
        param("numtracks") !~ /^\d+$/);
    
    return undef;
}

sub return_create_page
{
    my ($errorMessage) = @_;
    
    my $page = get_create_content($errorMessage) or
        return_error_page("failed to fill in content for create recorder page");
       
    print header('text/html; charset=utf-8');
    print encode_utf8($page);
    
    exit(0);
}

sub get_num_tracks
{
    # use previous if available
    if (defined param("numtracks"))
    {
        return param("numtracks");
    }

    return 5;
}

sub get_num_inputs
{
    # use previous if available
    if (defined param("numinputs"))
    {
        return param("numinputs");
    }
    
    return 1;
}

sub get_create_content
{
    my ($message) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1("Create new recorder"));

    if (defined $message)
    {
        push(@pageContent, p({-class=>"error"}, $message));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','createrec')"}));

    
    my @tableRows;
    push(@tableRows,  
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), textfield({-id=>"createrecNameCallout", -name=>"name"})]),
            td([div({-class=>"propHeading1"}, "# Inputs:"), 
                textfield(
                	-id => "createrecNumInputsCallout",
                    -name => "numinputs", 
                    -default => get_num_inputs(),
                    -override => 1
                )
            ]),
            td([div({-class=>"propHeading1"}, "# Tracks per inputs:"), 
                textfield(
                	-id => "createrecNumTracksCallout",
                    -name => "numtracks", 
                    -default => get_num_tracks(),
                    -override => 1
                )
            ]),
        ])
    );


    push(@pageContent, table({-class=>"noBorderTable"}, @tableRows));
    
    
    push(@pageContent, 
        p(
            submit({-onclick=>"whichPressed=this.name", -name=>"Create"}), 
            span(" "), submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"})
        )
    );
    
    push(@pageContent, end_form);

    
    return join("", @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


