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

    
if (!defined param('id'))
{
    return_error_page("Missing 'id' parameter");
}


my $scfId = param('id');
my $scf;

if (defined param('Cancel'))
{
    redirect_to_page("sourcecf.pl");
}
elsif (defined param('Delete'))
{
    db::delete_source_config($dbh, $scfId) or
        return_error_page("failed to delete source config with id=$scfId from database: $prodautodb::errstr");    

    redirect_to_page("sourcecf.pl");
}
else
{
    $scf = db::load_source_config($dbh, $scfId) or
        return_error_page("failed to find source config with id=$scfId from database: $prodautodb::errstr");
}

my $page = get_delete_content($scf) or
    return_error_page("failed to fill in content for delete source config page");
   
print header('text/html; charset=utf-8');
print encode_utf8($page);

exit(0);



sub get_delete_content
{
    my ($scf) = @_;
    
    my @pageContent;

    my $scfHTML = htmlutil::get_source_config($scf);
    
    push(@pageContent, h1('Delete source configuration'),
        start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','deletescf')"}),
        hidden(-name=>"id", -default=>$scf->{'config'}->{'ID'}),
        p("Please confirm"),
        submit({-onclick=>"whichPressed=this.name", -name=>"Delete"}),
        span(' '),
        submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"}),
        end_form,
        h2($scf->{'config'}->{'NAME'}),
        $scfHTML);
    
    
    return join('',@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


