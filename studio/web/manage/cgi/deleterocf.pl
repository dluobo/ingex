#!/usr/bin/perl -wT

#
# $Id: deleterocf.pl,v 1.2 2011/11/28 16:43:42 john_f Exp $
#
# 
#
# Copyright (C) 2007  Philip de Nier <philipn@users.sourceforge.net>
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


my $rocfId = param('id');
my $rocf;


if (defined param('Cancel'))
{
    redirect_to_page("router.pl");
}
elsif (defined param('Delete'))
{
    delete_router_config($dbh, $rocfId) or
        return_error_page("failed to delete router config with id=$rocfId from database: $prodautodb::errstr");    

    redirect_to_page("router.pl");
}
else
{
    $rocf = load_router_config($dbh, $rocfId) or
        return_error_page("failed to find router config with id=$rocfId from database: $prodautodb::errstr");
}


my $page = construct_page(get_delete_content($rocf)) or
    return_error_page("failed to fill in content for delete router config page");
   
print header('text/html; charset=utf-8');
print $page;

exit(0);





sub get_delete_content
{
    my ($rocf) = @_;
    
    my @pageContent;

    my $rocfHTML = htmlutil::get_router_config($rocf);
    
    push(@pageContent, h1('Delete router configuration'),
        start_form({-method=>'POST', -action=>'deleterocf.pl'}),
        hidden(-name=>"id", -default=>$rocf->{'config'}->{'ID'}),
        p("Please confirm"),
        submit("Delete"),
        span(' '),
        submit("Cancel"),
        end_form,
        h2($rocf->{'config'}->{'NAME'}),
        $rocfHTML);
    
    
    return join('',@pageContent);
}



END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


