#!/usr/bin/perl -wT

#
# $Id: createscf.pl,v 1.1 2007/02/01 09:02:47 philipn Exp $
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



if (defined param("Cancel"))
{
    redirect_to_page("sourcecf.pl");
}


my $recLocs = prodautodb::load_recording_locations($dbh) 
    or return_error_page("failed to load recording locations: $prodautodb::errstr");

my $errorMessage;


if (defined param("Create"))
{
    if (!($errorMessage = validate_params()))
    {
        my $name = trim(param("name"));
        my $numVideo = param("numvideo") || 0;
        my $numAudio = param("numaudio") || 0;

        
        my $scf;
        if (param("type") == $prodautodb::sourceType{"Tape"})
        {
            my $tapeNum = trim(param("tapenum"));
            
            $scf = create_source_config(param("type"), $name, $tapeNum, $numVideo, $numAudio);
        }
        else
        {
            my $recLocId;
            foreach my $recLoc (@{$recLocs})
            {
                if ($recLoc->{"ID"} eq param("recloc"))
                {
                    $recLocId = $recLoc->{"ID"};
                    last;
                }
            }
            if (!defined $recLocId)
            {
                # shouldn't return error if validation passed
                return_error_page("failed to match recording location with id");
            }
            
            $scf = create_source_config(param("type"), $name, $recLocId, $numVideo, $numAudio);
        }
        
        my $scfId = save_source_config($dbh, $scf) 
            or return_create_page($recLocs, "failed to save source config to database: $prodautodb::errstr");
        
        $scf = load_source_config($dbh, $scfId)
            or return_error_page($recLocs, "failed to reload saved source config from database: $prodautodb::errstr");

        redirect_to_page("editscf.pl?id=$scfId");
    }
}


return_create_page($recLocs, $errorMessage);




sub validate_params
{
    return "Error: Empty name" if (!defined param("name") || param("name") =~ /^\s*$/);
    
    return "Error: Unknown type" if (!defined param("type") 
        || param("type") != $prodautodb::sourceType{"Tape"} 
            && param("type") != $prodautodb::sourceType{"LiveRecording"});
            
    return "Error: Empty tape number" if (param("type") == $prodautodb::sourceType{"Tape"} 
        && (!defined param("tapenum") || param("tapenum") =~ /^\s*$/));

    if (param("type") == $prodautodb::sourceType{"LiveRecording"})
    {
        my $found = 0;
        foreach my $recLoc (@{$recLocs})
        {
            if ($recLoc->{"ID"} eq param("recloc"))
            {
                $found = 1;
                last;
            }
        }
        return "Error: Unknown recording location" if (!$found);
    }
    
    return "Error: No video or audio tracks" if (!param("numvideo") && !param("numaudio"));
    return "Error: Invalid video track count" if (param("numvideo") && param("numvideo") !~ /^\d+$/);
    return "Error: Invalid audio track count" if (param("numaudio") && param("numaudio") !~ /^\d+$/);

    return undef;
}

sub return_create_page
{
    my ($recLocs, $errorMessage) = @_;
    
    my $page = construct_page(get_create_content($recLocs, $errorMessage)) 
        or return_error_page("failed to fill in content for create source config page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_create_content
{
    my ($recLocs, $message) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1("Create new source group definition"));

    if (defined $message)
    {
        push(@pageContent, p({-class=>"error"}, $message));
    }
    
    push(@pageContent, start_form({-method=>"POST", -action=>"createscf.pl"}));

    push(@pageContent, p("Name", textfield("name")));

    push(@pageContent, 
        p(
            "Type", 
            popup_menu(
                -name=>"type",
                -values=>[
                    $prodautodb::sourceType{"LiveRecording"}, 
                    $prodautodb::sourceType{"Tape"}
                ],
                -default=>$prodautodb::sourceType{"LiveRecording"},
                -labels=>{
                    $prodautodb::sourceType{"LiveRecording"} => "LiveRecording", 
                    $prodautodb::sourceType{"Tape"} => "Tape"
                }
            )
        )
    );

    my @values;
    my %labels;
    foreach my $recLoc (@{$recLocs})
    {
        push(@values, $recLoc->{"ID"});
        $labels{$recLoc->{"ID"}} = $recLoc->{"NAME"};
    }
    push(@pageContent, 
        p(
            "Recording location", 
            popup_menu(
                -name=>"recloc",
                -values=>\@values,
                -labels=>\%labels
            )
        )
    );

    push(@pageContent, p("Tape number", textfield("tapenum")));

    push(@pageContent, 
        p(
            "# Video tracks", 
            textfield(
                -name => "numvideo",
                -default => 1
            )
        )
    );

    push(@pageContent, 
        p(
            "# Audio tracks", 
            textfield(
                -name => "numaudio",
                -default => 4
            )
        )
    );

    push(@pageContent, 
        submit("Create"), 
        submit("Cancel")
    );

    push(@pageContent, end_form);

    
    return join("", @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


