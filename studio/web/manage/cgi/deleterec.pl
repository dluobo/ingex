#!/usr/bin/perl -wT

#
# $Id: deleterec.pl,v 1.1 2007/09/11 14:08:46 stuart_hc Exp $
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

    
if (!defined param("id"))
{
    return_error_page("Missing 'id' parameter");
}


my $recId = param("id");
my $rec;


if (defined param("Cancel"))
{
    redirect_to_page("recorder.pl");
}
elsif (defined param("Delete"))
{
    delete_recorder($dbh, $recId) or
        return_error_page("failed to delete recorder with id=$recId from database: $prodautodb::errstr");    

    redirect_to_page("recorder.pl");
}
else
{
    $rec = load_recorder($dbh, $recId) or
        return_error_page("failed to find recorder with id=$recId from database: $prodautodb::errstr");
}


my $page = construct_page(get_delete_content($rec)) or
    return_error_page("failed to fill in content for delete recorder page");
   
print header;
print $page;

exit(0);





sub get_delete_content
{
    my ($rec) = @_;
    
    my @pageContent;

    push(@pageContent, h1("Delete recorder"),
        start_form({-method=>"POST", -action=>"deleterec.pl"}),
        hidden(-name=>"id", -default=>$rec->{"ID"}),
        p("Please confirm"),
        submit("Delete"),
        span(" "),
        submit("Cancel"),
        end_form,
        h2($rec->{"NAME"})
    );
    
    
    return join("",@pageContent);
}



END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


