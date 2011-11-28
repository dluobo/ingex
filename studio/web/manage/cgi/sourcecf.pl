#!/usr/bin/perl -wT

#
# $Id: sourcecf.pl,v 1.2 2011/11/28 16:43:42 john_f Exp $
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
use htmlutil;



my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}) 
    or return_error_page("failed to connect to database");


my $scfs = load_source_configs($dbh) 
    or return_error_page("failed to load source configs: $prodautodb::errstr");

    
my $page = construct_page(get_page_content(\$scfs)) 
    or return_error_page("failed to fill in content for source config page");
   
print header('text/html; charset=utf-8');
print $page;

exit(0);



sub get_page_content
{
    my ($scfs) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1("Source group definitions"));

    
    push(@pageContent, p(a({-href=>"createscf.pl"}, "Create new")));
    
    
    # list of source configs

    my @scfList;
    my $scfConfig;
    my $scfIndex = 0;
    my $scf;
    
    foreach $scf (@{$$scfs})
    {    
        $scfConfig = $scf->{"config"};
        
        push(@scfList, 
            a({-href=>"#SourceConfig-$scfIndex"}, $scfConfig->{"NAME"}),
            br());
            
       $scfIndex++;
    }

    push(@pageContent, p(@scfList)); 

    
    # each source config
    
    $scfIndex = 0;
    foreach $scf (@{$$scfs})
    {    
        my $scfHTML = htmlutil::get_source_config($scf);
        
        $scfConfig = $scf->{"config"};
        
        push(@pageContent, div({-id=>"SourceConfig-$scfIndex"},
            h2($scf->{"config"}->{"NAME"}),
            p(
                small(a({-href=>"editscf.pl?id=$scf->{'config'}->{'ID'}"}, "Edit")),
                small(a({-href=>"deletescf.pl?id=$scf->{'config'}->{'ID'}"}, "Delete")),
            ), 
            $scfHTML));

        $scfIndex++;
    }
        
    return join("", @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


