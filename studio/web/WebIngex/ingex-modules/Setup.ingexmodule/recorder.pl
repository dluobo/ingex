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
    
    
my $vrs = load_video_resolutions($dbh)
    or return_error_page("failed to load video resolutions: $prodautodb::errstr");

my $fmts = load_file_formats($dbh)
	or return_error_page("failed to load file formats: $prodautodb::errstr");

my $ops = load_ops($dbh)
	or return_error_page("failed to load operational patterns: $prodautodb::errstr");

my $recs = load_recorders($dbh) 
    or return_error_page("failed to load recorders: $prodautodb::errstr");

my $rcfs = load_recorder_configs($dbh) 
    or return_error_page("failed to load recorder configs: $prodautodb::errstr");


my $page = get_page_content($recs, $rcfs, $vrs) 
    or return_error_page("failed to fill in content for recorder page");
   
print header('text/html; charset=utf-8');
print encode_utf8($page);

exit(0);

    
sub get_page_content
{
    my ($recs, $rcfs, $vrs) = @_;

    my @pageContent;


    # list of recorders
    
    push(@pageContent, p({-id=>"recorderAboutCallout", -class=>"infoBox"}, "Info"));
    
    push(@pageContent, h1("Recorders"));

    push(@pageContent, p(a({-id=>"recorderCreateCallout", -href=>"javascript:getContent('createrec')"}, "Create new")));
    
    my @recorderRows;
    push(@recorderRows, 
        Tr({-class=>"simpleTable", -align=>'left', -valign=>'top'}, [
           th(['Name'])
        ]));

    foreach my $rec (
        sort { $a->{"recorder"}->{"NAME"} cmp $b->{"recorder"}->{"NAME"} } 
        (@{ $recs })
    )
    {
        my $recIndex = $rec->{"recorder"}->{"ID"};

        push(@recorderRows, 
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"},
                td([a({-href=>"javascript:show('Recorder-$recIndex',true)"}, $rec->{"recorder"}->{"NAME"})])
            ),
        );
    }

    push(@pageContent, table({-border=>0, -cellspacing=>3,-cellpadding=>3}, @recorderRows));


    # list of recorder configs

    push(@pageContent, h1("Recorder configurations"));

    push(@pageContent, p(a({-id=>"recorderConfigCreateCallout", -href=>"javascript:getContent('creatercf')"}, "Create new")));
    
    my @recorderConfigRows;
    push(@recorderConfigRows, 
        Tr({-class=>"simpleTable", -align=>'left', -valign=>'top'}, [
           th(['Name'])
        ]));

    foreach my $rcf (
        sort { $a->{"config"}->{"NAME"} cmp $b->{"config"}->{"NAME"} } 
        (@{ $rcfs })
    )
    {    
        my $rcfIndex = $rcf->{"config"}->{"ID"};

        push(@recorderConfigRows, 
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"},
                td([a({-href=>"javascript:show('RecorderConfig-$rcfIndex',true)"}, $rcf->{"config"}->{"NAME"})])
            ),
        );
    }
    
    push(@pageContent, table({-border=>0, -cellspacing=>3,-cellpadding=>3}, @recorderConfigRows));


    # detail for each recorder

    foreach my $rec (
        sort { $a->{"recorder"}->{"NAME"} cmp $b->{"recorder"}->{"NAME"} } 
        (@{ $recs })
    )
    {    
        my $recHTML = htmlutil::get_recorder($rec);
        my $recIndex = $rec->{"recorder"}->{"ID"};
        
        push(@pageContent,
            div({-id=>"Recorder-$recIndex",-class=>"hidden simpleBlock"},
                h2("$rec->{'recorder'}->{'NAME'}"),
                p(
                    small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('editrec','id=$recIndex')"}, "Edit")),
                    small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('deleterec','id=$recIndex')"}, "Delete")),
                ), 
                $recHTML
            )
        );
    }


    # detail for each config

    foreach my $rcf (
        sort { $a->{"config"}->{"NAME"} cmp $b->{"config"}->{"NAME"} } 
        (@{ $rcfs })
    )
    {    
        my $rcfHTML = htmlutil::get_recorder_config($rcf, $vrs, $fmts, $ops);
        my $rcfIndex = $rcf->{"config"}->{"ID"};
        
        push(@pageContent,
            div({-id=>"RecorderConfig-$rcfIndex",-class=>"hidden simpleBlock"},
                h2("$rcf->{'config'}->{'NAME'}"),
                p(
                    small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('editrcf','id=$rcfIndex')"}, "Edit")),
                    ($rcfIndex == 1 ? # 1 == default config which cannot be deleted
                        ""
                        : small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('deletercf','id=$rcfIndex')"}, "Delete"))),
                ), 
                $rcfHTML
            )
        );
    }


    return join("",@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


