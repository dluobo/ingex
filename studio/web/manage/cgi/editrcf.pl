#!/usr/bin/perl -wT

#
# $Id: editrcf.pl,v 1.3 2010/07/14 13:06:37 john_f Exp $
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


if (!defined param('id'))
{
    return_error_page("Missing 'id' parameter");
}

if (defined param("Cancel"))
{
    redirect_to_page("recorder.pl");
}


my $rcfId = param('id');
my $errorMessage;

my $vrs = load_video_resolutions($dbh) 
    or return_error_page("failed to load video resolutions: $prodautodb::errstr");

my $rcf = load_recorder_config($dbh, $rcfId) or
    return_error_page("failed to find recorder config with id=$rcfId from database: $prodautodb::errstr");

 
if (defined param('Reset'))
{
    Delete_all();
}
elsif (defined param("Done"))
{
    if (!($errorMessage = validate_params($rcf)))
    {
        $rcf->{"config"}->{"NAME"} = param("name");
        
        foreach my $recParam (@{ $rcf->{"parameters"} })
        {
            $recParam->{"VALUE"} = param("param-$recParam->{'ID'}");
        }
 
        if (!update_recorder_config($dbh, $rcf))
        {
            # do same as a reset
            Delete_all();
            $rcf = load_recorder_config($dbh, $rcfId) or
                return_error_page("failed to find recorder config with id=$rcfId from database: $prodautodb::errstr");
            
            $errorMessage = "failed to update recorder config to database: $prodautodb::errstr";
        }
        else
        {
            $rcf = load_recorder_config($dbh, $rcfId) or
                return_error_page("failed to reload saved recorder config from database: $prodautodb::errstr");
    
            redirect_to_page("recorder.pl");
        }
        
    }
}


return_edit_page($rcf, $vrs, $errorMessage);





sub validate_params
{
    my ($rcf) = @_;
    
    return "Error: empty name" if (!defined param("name") || param("name") =~ /^\s*$/);

    foreach my $recParam (@{ $rcf->{"parameters"} })
    {
        return "Error: missing recorder parameter $recParam->{'NAME'}" 
            if (!defined param("param-$recParam->{'ID'}"));
    }
    
    return undef;
}

sub return_edit_page
{
    my ($rcf, $vrs, $errorMessage) = @_;

    my $page = construct_page(get_edit_content($rcf, $vrs, $errorMessage)) or
        return_error_page("failed to fill in content for edit recorder config page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_edit_content
{
    my ($rcf, $vrs, $errorMessage) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1('Edit recorder configuration'));

    if (defined $errorMessage)
    {
        push(@pageContent, p({-class=>"error"}, $errorMessage));
    }
    
    push(@pageContent, start_form({-method=>'POST', -action=>'editrcf.pl'}));

    push(@pageContent, hidden('id', $rcf->{'config'}->{'ID'}));

    my @topRows;

    push(@topRows,  
        Tr({-align=>'left', -valign=>'top'}, [
            td([
                div({-class=>"propHeading1"}, 'Name:'), 
                textfield('name', $rcf->{'config'}->{'NAME'})
            ]),
        ])
    );

    my @configRows;
    push(@configRows,
        Tr({-align=>"left", -valign=>"top"}, 
           th(["Name", "Value"]),
        )
    );
    foreach my $rp (sort { $a->{"NAME"} cmp $b->{"NAME"} } (@{ $rcf->{"parameters"} }))
    {
        my $valueField;
        if ($rp->{"NAME"} eq "MXF_RESOLUTION" ||
            $rp->{"NAME"} eq "ENCODE1_RESOLUTION" ||
            $rp->{"NAME"} eq "ENCODE2_RESOLUTION" ||
            $rp->{"NAME"} eq "QUAD_RESOLUTION")
        {
            $valueField = get_video_resolution_popup("param-$rp->{'ID'}",
                $vrs, $rp->{"VALUE"});
        }
        else
        {
            $valueField = textfield(
                -name => "param-$rp->{'ID'}", 
                -value => $rp->{"VALUE"},
                -size => 20,
                -maxlength => 250,
            );
        }
        
        push(@configRows,
            Tr({-align=>"left", -valign=>"top"}, 
                td([$rp->{"NAME"}, $valueField]),
            )
        );
    }
    
    push(@topRows,  
        Tr({-align=>'left', -valign=>'top'}, [
            td([div({-class=>"propHeading1"}, "Config:"), 
                table({-class=>"borderTable"}, @configRows),
            ]),
        ]),
    );

    
    push(@pageContent, table({-class=>"noBorderTable"}, @topRows));

    
    push(@pageContent, 
        p(
            submit("Done"), 
            submit("Reset"), 
            submit("Cancel"),
        )
    );

    push(@pageContent, end_form);

    
    return join('', @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


