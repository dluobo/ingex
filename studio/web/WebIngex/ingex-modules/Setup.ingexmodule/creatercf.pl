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


if (defined param("Cancel"))
{
    redirect_to_page("recorder.pl");
}


my $rcfs = load_recorder_configs($dbh) 
    or return_error_page("failed to load recorder configs for recorder: $prodautodb::errstr");
    
    
my $errorMessage;

if (defined param("Create"))
{
    if (!($errorMessage = validate_params($rcfs)))
    {
        my $rcf = create_recorder_config(trim(param("name")), , get_template($rcfs));

        my $rcfId = save_recorder_config($dbh, $rcf) 
            or return_create_page("failed to save recorder config to database: $prodautodb::errstr", $rcfs);
        
        redirect_to_page("editrcf.pl?id=$rcfId");
    }
}

return_create_page($errorMessage, $rcfs);


sub get_template
{
    my ($rcfs) = @_;

    foreach my $rcf (@{ $rcfs })
    {
        if ($rcf->{"config"}->{"ID"} == param("template"))
        {
            return $rcf;
        }
    }
    
    return undef;
}

sub validate_params
{
    my ($rcfs) = @_;

    return "Error: Empty name" if (!defined param("name") || param("name") =~ /^\s*$/);
    
    return "Error: Undefined template param" if (!defined param("template"));

    my $template = get_template($rcfs);
    return "Error: unknown recorder config template " . param("template")
        if (!defined $template);
    
    return undef;
}

sub return_create_page
{
    my ($errorMessage, $rcfs) = @_;
    
    my $page = get_create_content($errorMessage, $rcfs) or
        return_error_page("failed to fill in content for create recorder config page");
       
    print header('text/html; charset=utf-8');
    print encode_utf8($page);
    
    exit(0);
}

sub get_create_content
{
    my ($message, $rcfs) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1("Create new recorder configuration"));

    if (defined $message)
    {
        push(@pageContent, p({-class=>"error"}, $message));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','creatercf')"}));

    
    my @tableRows;
    
    push(@tableRows,  
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), textfield({-id=>"creatercfNameCallout", -name=>"name"})]),
            td([div({-class=>"propHeading1"}, "Template:"), 
                get_recorder_config_popup("template", $rcfs, undef),
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


