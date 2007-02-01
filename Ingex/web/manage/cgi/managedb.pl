#!/usr/bin/perl -wT

#
# $Id: managedb.pl,v 1.1 2007/02/01 09:02:48 philipn Exp $
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
use datautil;
use htmlutil;




my $page = construct_page(get_page_content()) 
    or return_error_page("failed to fill in content for manage database page");
   
print header;
print $page;

exit(0);



sub get_page_content
{
    my @pageContent;
    
    push(@pageContent, h1("Manage database"));


    push(@pageContent, h2("Download configuration backup"));
    
    push(@pageContent, p(a({-href=>"configbackup.pl"}, "Download")));
    
    
    return join("",@pageContent);
}


