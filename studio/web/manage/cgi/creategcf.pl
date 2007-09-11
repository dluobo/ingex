#!/usr/bin/perl -wT

#
# $Id: creategcf.pl,v 1.1 2007/09/11 14:08:46 stuart_hc Exp $
#
# 
#
# Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#
 

use strict;

use CGI::Pretty qw(:standard);

use lib ".";
use config;
use prodautodb;
use datautil;
use htmlutil;


my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}) 
    or return_error_page("failed to connect to database");


my $errorMessage;

if (defined param('Cancel'))
{
    redirect_to_page("generalcf.pl");
}
elsif (defined param('Create'))
{
    if (param('type') eq 'recloc')
    {
        if (!($errorMessage = validate_recloc_params()))
        {
            my $name = trim(param('name'));
            
            my $rlcId = prodautodb::save_recording_location($dbh, $name) or 
                return_create_recloc_page("failed to save recording location '$name' to database: $prodautodb::errstr");
            
            my $rlc = prodautodb::load_recording_location($dbh, $rlcId) or
                return_error_page("failed to reload saved recording location from database: $prodautodb::errstr");
    
            redirect_to_page("generalcf.pl");
        }
    }
    else
    {
        return_error_page("unknown general configuration type");
    }
}

return_create_recloc_page($errorMessage);




sub validate_recloc_params
{
    return "Error: Empty name" if (!defined param('name') || param('name') =~ /^\s*$/);

    return undef;
}

sub return_create_recloc_page
{
    my ($errorMessage) = @_;
    
    my $page = construct_page(get_create_recloc_content($errorMessage)) or
        return_error_page("failed to fill in content for create recording location config page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_create_recloc_content
{
    my ($message) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1('Create new recording location'));

    if (defined $message)
    {
        push(@pageContent, p({-class=>"error"}, $message));
    }
    
    push(@pageContent, start_form({-method=>'POST', -action=>'creategcf.pl'}));

    push(@pageContent, hidden('type', 'recloc'));

    push(@pageContent, p('Name', textfield('name')));

    push(@pageContent, submit("Create"), span(' '), submit("Cancel"));

    push(@pageContent, end_form);

    
    return join('', @pageContent);
}



END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


