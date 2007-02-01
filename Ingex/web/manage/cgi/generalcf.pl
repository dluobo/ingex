#!/usr/bin/perl -wT

#
# $Id: generalcf.pl,v 1.1 2007/02/01 09:02:48 philipn Exp $
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


my $rcls = prodautodb::load_recording_locations($dbh) or 
    return_error_page("failed to load recorder locations: $prodautodb::errstr");


my $page = construct_page(get_page_content($rcls)) or
    return_error_page("failed to fill in content for general configuration page");
   
print header;
print $page;

exit(0);

    
sub get_page_content
{
    my ($rcls) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1('General configurations'));


    push(@pageContent, h2('Recording locations'));

    push(@pageContent, p(a({-href=>'creategcf.pl?type=recloc'}, 'Create new')));
    
 
    my @tableRows;
    push(@tableRows, 
        Tr({-align=>'left', -valign=>'top'}, [
           th(['Name',
            '',
            '']),
        ]));
    foreach my $rcl (@{$rcls})
    {    
        if ($rcl->{'ID'} == 1) # id=1 is a reserved value ("Unspecified")
        {
            push(@tableRows, 
                Tr({-align=>'left', -valign=>'top'}, [
                   td([$rcl->{'NAME'},
                    '',
                    '']),
                ]));
        }
        else
        {
            push(@tableRows, 
                Tr({-align=>'left', -valign=>'top'}, [
                   td([$rcl->{'NAME'},
                    a(({-href=>"editgcf.pl?type=recloc&id=$rcl->{'ID'}"}, 'Edit')),
                    a(({-href=>"deletegcf.pl?type=recloc&id=$rcl->{'ID'}"}, 'Delete'))]),
                ]));
        }
    }

    push(@pageContent, table({-border=>0, -cellspacing=>3,-cellpadding=>3},
        @tableRows));

    
    
    return join('',@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


