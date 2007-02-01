#!/usr/bin/perl -wT

#
# $Id: deletescf.pl,v 1.1 2007/02/01 09:02:47 philipn Exp $
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


my $scfId = param('id');
my $scf;

if (defined param('Cancel'))
{
    redirect_to_page("sourcecf.pl");
}
elsif (defined param('Delete'))
{
    prodautodb::delete_source_config($dbh, $scfId) or
        return_error_page("failed to delete source config with id=$scfId from database: $prodautodb::errstr");    

    redirect_to_page("sourcecf.pl");
}
else
{
    $scf = prodautodb::load_source_config($dbh, $scfId) or
        return_error_page("failed to find source config with id=$scfId from database: $prodautodb::errstr");
}

my $page = construct_page(get_delete_content($scf)) or
    return_error_page("failed to fill in content for delete source config page");
   
print header;
print $page;

exit(0);



sub get_delete_content
{
    my ($scf) = @_;
    
    my @pageContent;

    my $scfHTML = htmlutil::get_source_config($scf);
    
    push(@pageContent, h1('Delete source configuration'),
        start_form({-method=>'POST', -action=>'deletescf.pl'}),
        hidden(-name=>"id", -default=>$scf->{'config'}->{'ID'}),
        p("Please confirm"),
        submit("Delete"),
        span(' '),
        submit("Cancel"),
        end_form,
        h2($scf->{'config'}->{'NAME'}),
        $scfHTML);
    
    
    return join('',@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


