#!/usr/bin/perl -wT

#
# $Id: recorder.pl,v 1.2 2008/05/16 17:00:47 john_f Exp $
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
    
    push(@pageContent, h1("Recorders"));

    
    push(@pageContent, p(a({-href=>"createrec.pl"}, "Create new recorder")));
    
    
    my @recorderRows;
    foreach my $rec (@{ $recs })
    {    
        my @configRows;

        foreach my $rcf (@{ $rcfs })
        {    
            my $rcfConfig = $rcf->{"config"};
            
            if ($rec->{"ID"} == $rcfConfig->{"RECORDER_ID"})
            {
                if (defined $rec->{"CONF_ID"} && $rec->{"CONF_ID"} == $rcfConfig->{"ID"})
                {
                    unshift(@configRows,
                        Tr({-align=>"left", -valign=>"top"}, [
                            td([div({-class=>"propHeading2"}, "config:"), 
                                a({-href=>"#RecorderConfig-$rcfConfig->{'ID'}"}, $rcfConfig->{"NAME"}),
                            ]),
                        ])
                    );
                }
                else
                {
                    push(@configRows,
                        Tr({-align=>"left", -valign=>"top"}, [
                            td([div({-class=>"propHeading2"}, "alternative:"), 
                                a({-href=>"#RecorderConfig-$rcfConfig->{'ID'}"}, $rcfConfig->{"NAME"}),
                            ]),
                        ])
                    );
                }
            }
        }
        
        if (!defined $rec->{"CONF_ID"})
        {
            # not connected
            unshift(@configRows,
                Tr({-align=>"left", -valign=>"top"}, [
                    td([div({-class=>"propHeading2"}, "config:"), i("not set")]), 
                ])
            );
        }
        
        push(@recorderRows, 
            Tr({-align=>"left", -valign=>"top"},
                td(b($rec->{"NAME"})),
                td(
                    small(a({-href=>"editrec.pl?id=$rec->{'ID'}"}, "Edit")),
                    small(a({-href=>"deleterec.pl?id=$rec->{'ID'}"}, "Delete")),
                    small(a({-href=>"creatercf.pl?recid=$rec->{'ID'}"}, "Create new config")),
                ),
            ),
            Tr({-align=>"left", -valign=>"top"},
                td(""),
                td(table({-border=>0, -cellspacing=>3,-cellpadding=>3}, @configRows)),
            ),
        );
    }
    
    push(@pageContent, table({-border=>0, -cellspacing=>3,-cellpadding=>3}, @recorderRows));


    # each recorder config

    push(@pageContent, hr());
    push(@pageContent, h2("Recorder configurations"));
    
    foreach my $rcf (
        sort { $a->{"config"}->{"RECORDER_NAME"} cmp $b->{"config"}->{"RECORDER_NAME"} } 
        (@{ $rcfs })
    )
    {    
        my $rcfHTML = htmlutil::get_recorder_config($rcf, $vrs);
        
        my $rcfConfig = $rcf->{"config"};
        
        push(@pageContent, 
            div({-id=>"RecorderConfig-$rcfConfig->{'ID'}"}),
            h2("$rcf->{'config'}->{'RECORDER_NAME'} - $rcf->{'config'}->{'NAME'}"),
            p(
                small(a({-href=>"editrcf.pl?id=$rcf->{'config'}->{'ID'}"}, "Edit")),
                small(a({-href=>"deletercf.pl?id=$rcf->{'config'}->{'ID'}"}, "Delete")),
            ), 
            $rcfHTML);
    }
        
    return join("",@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


