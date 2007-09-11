#!/usr/bin/perl -wT

#
# $Id: proxydf.pl,v 1.1 2007/09/11 14:08:46 stuart_hc Exp $
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
    
    

my $pxdfs = load_proxy_defs($dbh) 
    or return_error_page("failed to load proxy definitions: $prodautodb::errstr");


my $page = construct_page(get_page_content($pxdfs)) 
    or return_error_page("failed to fill in content for proxy definitions page");
   
print header;
print $page;

exit(0);

    
sub get_page_content
{
    my ($pxdfs) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1("Proxy definitions"));

    
    push(@pageContent, p(a({-href=>"createproxydf.pl"}, "Create new")));
    
    
    # list of proxy defs

    my @pxdfList;
    my $pxdfIndex = 0;
    
    foreach my $pxdf ( @{ $pxdfs } )
    {    
        push(@pxdfList, 
            a({-href=>"#ProxyDef-$pxdfIndex"}, $pxdf->{"def"}->{"NAME"}),
                br());
            
       $pxdfIndex++;
    }

    push(@pageContent, p(@pxdfList)); 

    
    # each proxy def
    
    $pxdfIndex = 0;
    foreach my $pxdf ( @ { $pxdfs } )
    {    
        my $pxdfHTML = htmlutil::get_proxy_def($pxdf);
        
        push(@pageContent, div({-id=>"ProxyDef-$pxdfIndex"},
            h2($pxdf->{"def"}->{"NAME"}),
            p(
                small(a({-href=>"editproxydf.pl?id=$pxdf->{'def'}->{'ID'}"}, "Edit")),
                small(a({-href=>"deleteproxydf.pl?id=$pxdf->{'def'}->{'ID'}"}, "Delete")),
            ), 
            $pxdfHTML
        ));

        $pxdfIndex++;
    }
        
    return join("",@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


