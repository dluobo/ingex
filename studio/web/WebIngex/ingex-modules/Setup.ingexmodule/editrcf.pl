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

my $fmts = load_file_formats($dbh)
	or return_error_page("failed to load file formats: $prodautodb::errstr");

my $ops = load_ops($dbh)
	or return_error_page("failed to load operational patterns: $prodautodb::errstr");

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


return_edit_page($rcf, $vrs, $fmts, $ops, $errorMessage);





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
    my ($rcf, $vrs, $fmts, $ops, $errorMessage) = @_;

    my $page = get_edit_content($rcf, $vrs, $fmts, $ops, $errorMessage) or
        return_error_page("failed to fill in content for edit recorder config page");
       
    print header('text/html; charset=utf-8');
    print encode_utf8($page);
    
    exit(0);
}

sub get_edit_content
{
    my ($rcf, $vrs, $fmts, $ops, $errorMessage) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1('Edit recorder configuration'));

    if (defined $errorMessage)
    {
        push(@pageContent, p({-class=>"error"}, $errorMessage));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','editrcf')"}));

    push(@pageContent, hidden('id', $rcf->{'config'}->{'ID'}));

    my @topRows;

    push(@topRows,  
        Tr({-class=>"simpleTable", -align=>'left', -valign=>'top'}, [
            td([
                div({-class=>"propHeading1"}, 'Name:'), 
                textfield({
                	-id=>"editrcfNameCallout",
                	-name=>'name', 
                	-value=>$rcf->{'config'}->{'NAME'}
                })
            ]),
        ])
    );

    my @paramRows;
    push(@paramRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, 
           th(["Name", "Value"]),
        )
    );
    foreach my $rp (sort { $a->{"NAME"} cmp $b->{"NAME"} } (@{ $rcf->{"parameters"} }))
    {
        my $valueField;
        if ($rp->{"NAME"} eq "MXF_RESOLUTION" ||
            $rp->{"NAME"} =~ /ENCODE(\d*)_RESOLUTION/ ||
            $rp->{"NAME"} eq "QUAD_RESOLUTION")
        {
            $valueField = get_video_resolution_popup("param-$rp->{'ID'}",
                $vrs, $rp->{"VALUE"});
        }
        elsif ($rp->{"NAME"} eq "MXF_WRAPPING" ||
            $rp->{"NAME"} =~ /ENCODE(\d*)_WRAPPING/ ||
            $rp->{"NAME"} eq "QUAD_WRAPPING")
        {
        	$valueField = get_video_wrapping_popup("param-$rp->{'ID'}",
                $fmts, $rp->{"VALUE"});
        }
        elsif ($rp->{"NAME"} eq "MXF_OP" ||
            $rp->{"NAME"} =~ /ENCODE(\d*)_OP/ ||
            $rp->{"NAME"} eq "QUAD_OP")
        {
        	$valueField = get_op_popup("param-$rp->{'ID'}",
                $ops, $rp->{"VALUE"});
        }
        else
        {
            $valueField = textfield(
            	-id => "recorder_$rp->{'NAME'}",
                -name => "param-$rp->{'ID'}", 
                -value => $rp->{"VALUE"},
                -size => 20,
                -maxlength => 250,
            );
        }
        
        push(@paramRows,
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, 
                td([$rp->{"NAME"}, $valueField]),
            )
        );
    }
    
    push(@topRows,  
        Tr({-class=>"simpleTable", -align=>'left', -valign=>'top'}, [
            td([div({-class=>"propHeading1"}, "Parameters:"), 
                table({-class=>"borderTable"}, @paramRows),
            ]),
        ]),
    );

    
    push(@pageContent, table({-class=>"noBorderTable"}, @topRows));

    
    push(@pageContent, 
        p(
            submit({-onclick=>"whichPressed=this.name", -name=>"Done"}), 
            submit({-onclick=>"whichPressed=this.name", -name=>"Reset"}), 
            submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"}),
        )
    );

    push(@pageContent, end_form);

    
    return join('', @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


