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
use Switch;
use lib ".";
use lib "..";
use lib "../../ingex-config";
use IngexJSON;

my $friendlyName = "File Copy";
my $pollTime = 2000;
my $appliesTo = ["Recorder"];
my $queryType = "http";
my $queryLoc = ":7010";
my $sortOrder = 250;

my $fields = {"recordings"=>"copyRecs","endpoints"=>"copyEndpoints","current"=>"copyCurrent","progress"=>"copyProgress"};

switch ($ARGV[0]){
	case "monitorInfo" { monitorInfo(); }
	case "basicInfo" { basicInfo(); }
}

exit(1);

sub monitorInfo
{
	my %monitorInfo;
	$monitorInfo{"friendlyName"}=$friendlyName;
	$monitorInfo{"pollTime"}=$pollTime;
	$monitorInfo{"fields"}=$fields;
	$monitorInfo{"appliesTo"}=$appliesTo;
	$monitorInfo{"queryType"}=$queryType;
	$monitorInfo{"queryLoc"}=$queryLoc;
	$monitorInfo{"sortOrder"}=$sortOrder;
	print hashToJSON(%monitorInfo);
}

sub basicInfo
{
	print "This is a descriptor for an http monitor. I can return no data I'm afraid...\n";
}