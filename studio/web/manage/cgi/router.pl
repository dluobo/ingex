#!/usr/bin/perl -wT

#
# $Id: router.pl,v 1.1 2007/09/11 14:08:46 stuart_hc Exp $
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
    
    

my $rocfs = load_router_configs($dbh) 
    or return_error_page("failed to load router configs: $prodautodb::errstr");


my $page = construct_page(get_page_content($rocfs)) 
    or return_error_page("failed to fill in content for router page");
   
print header;
print $page;

exit(0);

    
sub get_page_content
{
    my ($rocfs) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1("Router configurations"));

    push(@pageContent, p(a({-href=>"createrocf.pl"}, "Create new router configuration")));

    
    my $rocfIndex = 0;
    foreach my $rocf (
        sort { $a->{"config"}->{"NAME"} cmp $b->{"config"}->{"NAME"} } 
        (@{ $rocfs })
    )
    {    
        my $rocfHTML = htmlutil::get_router_config($rocf);
        
        push(@pageContent, 
            div({-id=>"RouterConfig-$rocfIndex"}),
            h2("$rocf->{'config'}->{'NAME'}"),
            p(
                small(a({-href=>"editrocf.pl?id=$rocf->{'config'}->{'ID'}"}, "Edit")),
                small(a({-href=>"deleterocf.pl?id=$rocf->{'config'}->{'ID'}"}, "Delete")),
            ), 
            $rocfHTML);

        $rocfIndex++;
    }
        
    return join("",@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


