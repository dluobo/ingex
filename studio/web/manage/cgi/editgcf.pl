#!/usr/bin/perl -wT

#
# $Id: editgcf.pl,v 1.2 2011/11/28 16:43:42 john_f Exp $
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


if (!defined param('id'))
{
    return_error_page("Missing 'id' parameter");
}
elsif (!defined param('type'))
{
    return_error_page("Missing 'type' parameter");
}
elsif (param('type') ne "recloc")
{
    return_error_page("Only type 'recloc' is supported");
}

my $rlcId = param('id');
my $rlc;
my $errorMessage;

if (defined param('Cancel'))
{
    redirect_to_page("generalcf.pl");
}
elsif (defined param('Reset'))
{
    Delete_all();
    $rlc = prodautodb::load_recording_location($dbh, $rlcId) or
        return_error_page("failed to find recording location with id=$rlcId from database: $prodautodb::errstr");
}
elsif (defined param('Done'))
{
    $rlc = prodautodb::load_recording_location($dbh, $rlcId) or
        return_error_page("failed to load recording location with id=$rlcId from database: $prodautodb::errstr");

    if (!defined param('name') || param('name') =~ /^\s*$/)
    {
        $errorMessage = "Error: Empty name"; 
    }
    else
    {
        $rlc->{'NAME'} = trim(param('name'));
        
        update_recording_location($dbh, $rlc) or 
            return_error_page("failed to update recording location to database: $prodautodb::errstr");
        
        redirect_to_page("generalcf.pl");
    }
}
else
{
    $rlc = prodautodb::load_recording_location($dbh, $rlcId) or
        return_error_page("failed to load recording location with id=$rlcId from database: $prodautodb::errstr");
}


my $page = construct_page(get_edit_recloc_content($rlc, $errorMessage)) or
    return_error_page("failed to fill in content for source config page");
   
print header('text/html; charset=utf-8');
print $page;

exit(0);



sub get_edit_recloc_content
{
    my ($rlc, $message) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1('Edit recording location'));

    if (defined $message)
    {
        push(@pageContent, p({-class=>"error"}, $message));
    }
    
    push(@pageContent, start_form({-method=>'POST', -action=>'editgcf.pl'}));

    push(@pageContent, hidden('id', $rlc->{'ID'}));
    push(@pageContent, hidden('type', 'recloc'));

    push(@pageContent, p(textfield('name', $rlc->{'NAME'})));
    
    push(@pageContent,
        p(
            submit("Done"), 
            submit("Reset"), 
            submit("Cancel")
        )
    );

    push(@pageContent, end_form);

    
    return join('', @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


