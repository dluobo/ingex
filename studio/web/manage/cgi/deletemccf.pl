#!/usr/bin/perl -wT

#
# $Id: deletemccf.pl,v 1.2 2011/11/28 16:43:42 john_f Exp $
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


my $mccfId = param('id');
my $mccf;


if (defined param('Cancel'))
{
    redirect_to_page("multicamcf.pl");
}
elsif (defined param('Delete'))
{
    delete_multicam_config($dbh, $mccfId) or
        return_error_page("failed to delete multi-camera config with id=$mccfId from database: $prodautodb::errstr");    

    redirect_to_page("multicamcf.pl");
}
else
{
    $mccf = load_multicam_config($dbh, $mccfId) or
        return_error_page("failed to find multi-camera config with id=$mccfId from database: $prodautodb::errstr");
}


my $page = construct_page(get_delete_content($mccf)) or
    return_error_page("failed to fill in content for delete multi-camera config page");
   
print header('text/html; charset=utf-8');
print $page;

exit(0);





sub get_delete_content
{
    my ($mccf) = @_;
    
    my @pageContent;

    my $mccfHTML = htmlutil::get_multicam_config($mccf);
    
    push(@pageContent, h1("Delete multi-camera configuration"),
        start_form({-method=>"POST", -action=>"deletemccf.pl"}),
        hidden(-name=>"id", -default=>$mccf->{"config"}->{"ID"}),
        p("Please confirm"),
        submit("Delete"),
        span(" "),
        submit("Cancel"),
        end_form,
        h2($mccf->{"config"}->{"NAME"}),
        $mccfHTML);
    
    
    return join("",@pageContent);
}



END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


