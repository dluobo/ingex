#!/usr/bin/perl -wT

#
# $Id: editrocf.pl,v 1.2 2011/11/28 16:43:42 john_f Exp $
#
# 
#
# Copyright (C) 2007  Philip de Nier <philipn@users.sourceforge.net>
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
    redirect_to_page("router.pl");
}


my $rocfId = param('id');
my $errorMessage;

my $rocf = load_router_config($dbh, $rocfId) or
    return_error_page("failed to find router config with id=$rocfId from database: $prodautodb::errstr");

 
if (defined param('Reset'))
{
    Delete_all();
}
elsif (defined param("Done"))
{
    if (!($errorMessage = validate_params($rocf)))
    {
        $rocf->{"config"}->{"NAME"} = param("name");
        
        foreach my $input (@{ $rocf->{"inputs"} })
        {
            my $inputNameId = get_html_param_id([ "input", "name"  ], [ $input->{"ID"} ]);
            $input->{"NAME"} = param($inputNameId);
            
            my $sourceId = get_html_param_id([ "input", "source" ], [ $input->{"ID"} ]);
            my @source = parse_html_source_track(param($sourceId));
            $input->{"SOURCE_ID"} = $source[0];
            $input->{"SOURCE_TRACK_ID"} = $source[1];
        }            
 
        foreach my $output (@{ $rocf->{"outputs"} })
        {
            my $outputNameId = get_html_param_id([ "output", "name"  ], [ $output->{"ID"} ]);
            $output->{"NAME"} = param($outputNameId);
        }            

        
        if (!update_router_config($dbh, $rocf))
        {
            # do same as a reset
            Delete_all();
            $rocf = load_router_config($dbh, $rocfId) or
                return_error_page("failed to find router config with id=$rocfId from database: $prodautodb::errstr");
            
            $errorMessage = "failed to update router config to database: $prodautodb::errstr";
        }
        else
        {
            $rocf = load_router_config($dbh, $rocfId) or
                return_error_page("failed to reload saved router config from database: $prodautodb::errstr");
    
            redirect_to_page("router.pl");
        }
        
    }
}


return_edit_page($rocf, $errorMessage);





sub validate_params
{
    my ($rocf) = @_;
    
    return "Error: empty name" if (!defined param("name") || param("name") =~ /^\s*$/);

    foreach my $input (@{ $rocf->{"inputs"} })
    {
        my $inputNameId = get_html_param_id([ "input", "name" ], [ $input->{"ID"} ]);
        return "Error: empty name for input $input->{'INDEX'}" if (!defined param($inputNameId) ||
            param($inputNameId) =~ /^\s*$/);
            
        my $sourceId = get_html_param_id([ "input", "source" ], [ $input->{"ID"} ]);
        return "Error: missing source for input $input->{'INDEX'}" 
            if (!defined param($sourceId));

        my $sourceIdParam = param($sourceId);
        my @source = parse_html_source_track($sourceIdParam);
        return "Error: invalid source reference ('$sourceIdParam') for input $input->{'INDEX'}"
            if (scalar @source != 2 || 
                (defined $source[0] && $source[0] !~ /^\d+$/) || 
                (defined $source[1] && $source[1] !~ /^\d+$/));
    }
    
    foreach my $output (@{ $rocf->{"outputs"} })
    {
        my $outputNameId = get_html_param_id([ "output", "name" ], [ $output->{"ID"} ]);
        return "Error: empty name for output $output->{'INDEX'}" if (!defined param($outputNameId) ||
            param($outputNameId) =~ /^\s*$/);
    }
    
    
    return undef;
}

sub return_edit_page
{
    my ($rocf, $errorMessage) = @_;

    my $page = construct_page(get_edit_content($rocf, $errorMessage)) or
        return_error_page("failed to fill in content for edit router config page");
       
    print header('text/html; charset=utf-8');
    print $page;
    
    exit(0);
}

sub get_edit_content
{
    my ($rocf, $errorMessage) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1('Edit router configuration'));

    if (defined $errorMessage)
    {
        push(@pageContent, p({-class=>"error"}, $errorMessage));
    }
    
    
    push(@pageContent, start_form({-method=>'POST', -action=>'editrocf.pl'}));

    push(@pageContent, hidden('id', $rocf->{'config'}->{'ID'}));

    my @topRows;

    push(@topRows,  
        Tr({-align=>'left', -valign=>'top'}, [
            td([
                div({-class=>"propHeading1"}, 'Name:'), 
                textfield('name', $rocf->{'config'}->{'NAME'})
            ]),
        ])
    );

    my @inputRows;
    push(@inputRows, 
        Tr({-align=>'left', -valign=>'top'}, [
            th(['Index', 'Name', 'Source']),
        ])
    );
    foreach my $input (@{ $rocf->{'inputs'} })
    {
        push(@inputRows, 
            Tr({-align=>'left', -valign=>'top'}, [
                Tr({-align=>'left', -valign=>'top'}, [
                    td([div($input->{'INDEX'}, 
                            hidden(
                                get_html_param_id([ "input", "index" ], [ $input->{'ID'} ]),
                                $input->{'INDEX'}
                            )
                        ),
                        textfield(
                            get_html_param_id([ "input", "name" ], [ $input->{'ID'} ]),
                            $input->{'NAME'}
                        ),
                        get_sources_popup(
                            get_html_param_id([ "input", "source" ], [ $input->{'ID'} ]),
                            load_source_config_refs($dbh),
                            $input->{'SOURCE_ID'}, 
                            $input->{'SOURCE_TRACK_ID'},
                        ),
                    ]),
                ]),
            ]),
        );
    }

    my @outputRows;
    push(@outputRows, 
        Tr({-align=>'left', -valign=>'top'}, [
            th(['Index', 'Name']),
        ])
    );
    foreach my $output (@{ $rocf->{'outputs'} })
    {
        push(@outputRows, 
            Tr({-align=>'left', -valign=>'top'}, [
                Tr({-align=>'left', -valign=>'top'}, [
                    td([div($output->{'INDEX'}, 
                            hidden(
                                get_html_param_id([ "output", "index" ], [ $output->{'ID'} ]),
                                $output->{'INDEX'}
                            )
                        ),
                        textfield(
                            get_html_param_id([ "output", "name" ], [ $output->{'ID'} ]),
                            $output->{'NAME'}
                        ),
                    ]),
                ]),
            ]),
        );
    }

    push(@topRows,  
        Tr({-align=>'left', -valign=>'top'}, [
            td([div({-class=>"propHeading1"}, 'Inputs:'), 
                table({-class=>"borderTable"}, @inputRows),
            ]),
        ]),
        Tr({-align=>'left', -valign=>'top'}, [
            td([div({-class=>"propHeading1"}, 'Outputs:'), 
                table({-class=>"borderTable"}, @outputRows),
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


