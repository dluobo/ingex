#!/usr/bin/perl -wT

#
# $Id: multicamcf.pl,v 1.1 2007/02/01 09:02:48 philipn Exp $
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
    
    

my $mccfs = load_multicam_configs($dbh) 
    or return_error_page("failed to load multicam configs: $prodautodb::errstr");


my $page = construct_page(get_page_content(\$mccfs)) 
    or return_error_page("failed to fill in content for multicam config page");
   
print header;
print $page;

exit(0);

    
sub get_page_content
{
    my ($mccfs) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1("Multi-camera clip definitions"));

    
    push(@pageContent, p(a({-href=>"createmccf.pl"}, "Create new")));
    
    
    # list of multi-cam configs

    my @mccfList;
    my $mccfConfig;
    my $mccfIndex = 0;
    my $mccf;
    
    foreach $mccf (@{$$mccfs})
    {    
        $mccfConfig = $mccf->{"config"};
        
        push(@mccfList, 
            a({-href=>"#RecorderConfig-$mccfIndex"}, $mccfConfig->{"NAME"}),
            br());
            
       $mccfIndex++;
    }

    push(@pageContent, p(@mccfList)); 

    
    # each multi-cam config
    
    $mccfIndex = 0;
    foreach $mccf (@{$$mccfs})
    {    
        my $mccfHTML = htmlutil::get_multicam_config($mccf);
        
        $mccfConfig = $mccf->{"config"};
        
        push(@pageContent, div({-id=>"RecorderConfig-$mccfIndex"},
            h2($mccf->{"config"}->{"NAME"}),
            p(
                small(a({-href=>"editmccf.pl?id=$mccf->{'config'}->{'ID'}"}, "Edit")),
                small(a({-href=>"deletemccf.pl?id=$mccf->{'config'}->{'ID'}"}, "Delete")),
                ), 
            $mccfHTML));

        $mccfIndex++;
    }
        
    return join("",@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


