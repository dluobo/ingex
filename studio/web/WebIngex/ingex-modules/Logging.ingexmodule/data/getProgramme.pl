#!/usr/bin/perl -wT

# Copyright (C) 2008  British Broadcasting Corporation
# Author: Rowan de Pomerai <rdepom@users.sourceforge.net>
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

use lib ".";
use lib "..";
use lib "../../../ingex-config";
use ingexconfig;
use ingexhtmlutil;
use prodautodb;
use JSON::XS;
use ILutil;

my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"})
    or die();

my $allItems;
if(defined param('progid')) {
	$allItems = getItems(param('progid'));
}

#convert the database to JSON object
my $encodedJson = encode_json($allItems);

print header('text/html; charset=utf-8');
print $encodedJson;
prodautodb::disconnect($dbh) if ($dbh);
exit(0);

sub getItems
{
	my ($progId) = @_;
	my @items;
	my %i;
	my $itemsArray = load_items($dbh,$progId) 
	    or return_error_page("failed to load items: $prodautodb::errstr");

	foreach my $item (@$itemsArray) {
		%i = %$item;
		my $takes = getTakes($i{'ID'});
		my $newItem = {
			orderIndex=>$i{'ORDERINDEX'},
			itemName=>$i{'ITEMNAME'},
			sequence=>$i{'SEQUENCE'},
			databaseId=>$i{'ID'},
			uiProvider=>'item',
			children=>$takes
		};
		push(@items,$newItem);
	}
	
	return \@items;
}

sub getTakes
{
	my ($itemId) = @_;
	my @takes;
	my %t;
	my $takesArray = load_takes($dbh,$itemId) 
	    or return_error_page("failed to load takes: $prodautodb::errstr");
		
	foreach my $take (@$takesArray) {
		%t = %$take;
		#$t{'EDITRATE'} is of form (000,000) where there is a varying number of digits
		#split into frameVals[1]=framerate Numerator, frameVals[2]=framerateDenominator
		my @frameVals = split /(\d+),(\d+)/, $t{'EDITRATE'}, 2;
		my $editrate = ($frameVals[1] / $frameVals[2]);
		my $newTake = {
			databaseId=>$t{'ID'},
			takeNo=>$t{'TAKENO'},
			location=>$t{'LOCATION'},
			date=>$t{'DATE'},
			inpoint=>toTC($t{'START'}, $editrate),
			out=>toTC($t{'START'}+$t{'LENGTH'}, $editrate),
			duration=>toTC($t{'LENGTH'}, $editrate),
			result=>$t{'RESULT'},
			comment=>$t{'COMMENT'},
			uiProvider=>'take',
			leaf=>'true'
		};
		push(@takes,$newTake);
	}
	
	return \@takes;
}
