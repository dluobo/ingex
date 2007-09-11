#!/usr/bin/perl -wT

#
# $Id: configbackup.pl,v 1.1 2007/09/11 14:08:46 stuart_hc Exp $
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




my @dump;


prodautodb::dump_table(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}, 
        \@dump, 
        'recordinglocation') 
    or return_error_page("failed to dump the recording location table");
prodautodb::dump_table( 
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}, 
        \@dump, 
        'sourceconfig') 
    or return_error_page("failed to dump the source configtable");
prodautodb::dump_table( 
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}, 
        \@dump, 
        'sourcetrackconfig') 
    or return_error_page("failed to dump the source track config table");
prodautodb::dump_table( 
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}, 
        \@dump, 
        'recorder') 
    or return_error_page("failed to dump the recorder table");
prodautodb::dump_table( 
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}, 
        \@dump, 
        'recorderconfig') 
    or return_error_page("failed to dump the recorder config table");
prodautodb::dump_table( 
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}, 
        \@dump, 
        'recorderinputconfig') 
    or return_error_page("failed to dump the recorder input config table");
prodautodb::dump_table( 
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}, 
        \@dump, 
        'recorderinputtrackconfig') 
    or return_error_page("failed to dump the recorder input track config table");
prodautodb::dump_table( 
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}, 
        \@dump, 
        'multicameraclipdef') 
    or return_error_page("failed to dump the multi camera clip definition table");
prodautodb::dump_table( 
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}, 
        \@dump, 
        'multicameratrackdef') 
    or return_error_page("failed to dump the multi camera track def table");
prodautodb::dump_table( 
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}, 
        \@dump, 
        'multicameraselectordef') 
    or return_error_page("failed to dump the multi camera clip selector table");


my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = gmtime(time);
$year += 1900;
$mon += 1;

print "Content-Type:application/x-download\n";  
print "Content-Disposition:attachment;filename=configbackup-$year$mon$mday$hour$min$sec.sql\n\n";
print @dump; 

exit(0);



