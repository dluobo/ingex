#!/usr/bin/perl -wT

#
# $Id: recorder.pl,v 1.3 2010/07/14 13:06:37 john_f Exp $
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
    
    
my $vrs = load_video_resolutions($dbh) 
    or return_error_page("failed to load video resolutions: $prodautodb::errstr");

my $recs = load_recorders($dbh) 
    or return_error_page("failed to load recorders: $prodautodb::errstr");

my $rcfs = load_recorder_configs($dbh) 
    or return_error_page("failed to load recorder configs: $prodautodb::errstr");


my $page = construct_page(get_page_content($recs, $rcfs, $vrs)) 
    or return_error_page("failed to fill in content for recorder page");
   
print header;
print $page;

exit(0);

    
sub get_page_content
{
    my ($recs, $rcfs, $vrs) = @_;
    
    my @pageContent;


    # list of recorders

    push(@pageContent, h1("Recorders"));

    push(@pageContent, p(small(a({-href=>"createrec.pl"}, "Create new"))));

    my @recorderRows;
    foreach my $rec (
        sort { $a->{"recorder"}->{"NAME"} cmp $b->{"recorder"}->{"NAME"} } 
        (@{ $recs })
    )
    {    
        push(@recorderRows, 
            Tr({-align=>"left", -valign=>"top"},
                td(a({-href=>"#Recorder-$rec->{'recorder'}->{'ID'}"}, $rec->{"recorder"}->{"NAME"})),
            ),
        );
    }
    
    push(@pageContent, table({-border=>0, -cellspacing=>3,-cellpadding=>3}, @recorderRows));


    # list of recorder configs

    push(@pageContent, h1("Recorder configurations"));

    push(@pageContent, p(small(a({-href=>"creatercf.pl"}, "Create new"))));

    my @recorderConfigRows;
    foreach my $rcf (
        sort { $a->{"config"}->{"NAME"} cmp $b->{"config"}->{"NAME"} } 
        (@{ $rcfs })
    )
    {    
        push(@recorderConfigRows, 
            Tr({-align=>"left", -valign=>"top"},
                td(a({-href=>"#RecorderConfig-$rcf->{'config'}->{'ID'}"}, $rcf->{"config"}->{"NAME"})),
            ),
        );
    }
    
    push(@pageContent, table({-border=>0, -cellspacing=>3,-cellpadding=>3}, @recorderConfigRows));


    # detail for each recorder

    push(@pageContent, hr());
    push(@pageContent, h2("Recorder details"));
    
    foreach my $rec (
        sort { $a->{"recorder"}->{"NAME"} cmp $b->{"recorder"}->{"NAME"} } 
        (@{ $recs })
    )
    {    
        my $recHTML = htmlutil::get_recorder($rec);
        
        push(@pageContent, 
            div({-id=>"Recorder-$rec->{'recorder'}->{'ID'}"}),
            h2("$rec->{'recorder'}->{'NAME'}"),
            p(
                small(a({-href=>"editrec.pl?id=$rec->{'recorder'}->{'ID'}"}, "Edit")),
                small(a({-href=>"deleterec.pl?id=$rec->{'recorder'}->{'ID'}"}, "Delete")),
            ), 
            $recHTML);
    }
        
    # detail for each recorder config

    push(@pageContent, hr());
    push(@pageContent, h2("Recorder configuration details"));
    
    foreach my $rcf (
        sort { $a->{"config"}->{"NAME"} cmp $b->{"config"}->{"NAME"} } 
        (@{ $rcfs })
    )
    {    
        my $rcfHTML = htmlutil::get_recorder_config($rcf, $vrs);
        
        push(@pageContent, 
            div({-id=>"RecorderConfig-$rcf->{'config'}->{'ID'}"}),
            h2("$rcf->{'config'}->{'NAME'}"),
            p(
                small(a({-href=>"editrcf.pl?id=$rcf->{'config'}->{'ID'}"}, "Edit")),
                ($rcf->{"config"}->{"ID"} == 1 ? # 1 == default config which cannot be deleted
                    ""
                    : small(a({-href=>"deletercf.pl?id=$rcf->{'config'}->{'ID'}"}, "Delete"))),
            ), 
            $rcfHTML);
    }
        
    return join("",@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


