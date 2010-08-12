#
# $Id: htmlutil.pm,v 1.4 2010/08/12 16:39:51 john_f Exp $
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
 
package htmlutil;

use strict;
use warnings;

use CGI::Pretty qw(:standard);
use Text::Template;


####################################
#
# Module exports
#
####################################

BEGIN 
{
    use Exporter ();
    our (@ISA, @EXPORT);

    @ISA = qw(Exporter);
    @EXPORT = qw(
        &trim
        &get_html_param_id
        &redirect_to_page
        &construct_page
        &return_error_page
        &get_html_source_track
        &parse_html_source_track
        &get_sources_popup
        &get_source_config
        &get_video_resolution_popup
        &get_recorder
        &get_recorder_config_popup
        &get_recorder_config
        &get_proxy_def
        &get_proxy_types_popup
        &get_router_config
    );
}


####################################
#
# Load page template
#
####################################

# TODO: put "ingex.tmpl" in a safe place
my $pageTemplate = Text::Template->new(
        UNTAINT => 1,
        SOURCE => "ingex.tmpl") 
    or die "Couldn't construct template: $Text::Template::ERROR";


    
####################################
#
# Remove leading and trailing spaces
#
####################################

sub trim
{
    my ($value) = @_;
    
    $value =~ s/^\s*(\S*(?:\s+\S+)*)\s*$/$1/;
    
    return $value;
}


####################################
#
# Create an id for HTML form element
#
####################################

sub get_html_param_id
{
    my ($names, $ids) = @_;
    
    return join("-", join("-", @{ $names }), join("-", @{ $ids }));
}



####################################
#
# Redirect page
#
####################################

sub redirect_to_page
{
    my ($pageName) = @_;
    
    my $relUrl = url(-relative=>1);
    my $newUrl = url();
    $newUrl =~ s/^(.*)$relUrl$/$1$pageName/;
    
    print redirect($newUrl);

    exit(0);
}

    
####################################
#
# Construct page
#
####################################

sub construct_page
{
    my ($content) = @_;

    return $pageTemplate->fill_in(HASH => [{content => $content}]);
}


####################################
#
# Error page
#
####################################

sub return_error_page
{
    my ($message) = @_;

    my @pageContent = h1("Error");
    push(@pageContent, p({-style=>"color: red;"}, $message));
    
    
    my $page = construct_page(join("", @pageContent)) or
        return_template_error_page($message);

    print header;
    print $page;

    exit(0);    
}

####################################
#
# HTML template error page
#
####################################


sub return_template_error_page
{
    my ($message) = @_;

    print header;
    print start_html,
    
    print h1("HTML Template Error");

    print p({-style=>"color: red;"}, $message) if (defined $message);

    print end_html;
    
    exit(0);
}


####################################
#
# Source config
#
####################################


sub get_html_source_track
{
    my ($sourceId, $trackId) = @_;

    $sourceId = ($sourceId) ? $sourceId : 0;
    $trackId = ($trackId) ? $trackId : 0;
    
    return "$sourceId $trackId";
}

sub parse_html_source_track
{
    my ($htmlValue) = @_;
    
    my @sourceTrack = split(" ", $htmlValue);
    if (!$sourceTrack[0])
    {
        $sourceTrack[0] = undef;
    }
    if (!$sourceTrack[1])
    {
        $sourceTrack[1] = undef;
    }
    
    return @sourceTrack;
}

sub get_sources_popup
{
    my ($paramId, $scfrs, $defaultSourceId, $defaultSourceTrackId) = @_;
    
    my $default;
    my @values;
    my %labels;

    # not connected option
    push(@values, get_html_source_track(undef, undef));
    $labels{get_html_source_track(undef, undef)} = "not connected";

    # sources options
    foreach my $scfr (@{ $scfrs })
    {
        my $value = get_html_source_track($scfr->{"ID"}, $scfr->{"TRACK_ID"});
        my $label = "$scfr->{'NAME'} ($scfr->{'TRACK_NAME'})";
        push(@values, $value);
        $labels{$value} = $label;

        if (!defined $default 
            && defined $defaultSourceId && $scfr->{"ID"} == $defaultSourceId 
            && defined $defaultSourceTrackId && $scfr->{"TRACK_ID"} == $defaultSourceTrackId)
        {
            $default = $value;
        }
    }
    
    # no default then is not connected
    if (!defined $default)
    {
        $default = "0 0";
    }
    
    
    return popup_menu(
        -name => $paramId, 
        -default => $default,
        -values => \@values,
        -labels => \%labels);
}


sub get_source_config
{
    my ($sourceConfig) = @_;
    
    my $scfConfig = $sourceConfig->{"config"};
    
    my @pageContent;

    my @tableRows;
    
    push(@tableRows,
        Tr({-align=>"left", -valign=>"top"}, [
           td([div({-class=>"propHeading1"}, "Name:"), $scfConfig->{"NAME"}]),
        ]));
        
    push(@tableRows,
        Tr({-align=>"left", -valign=>"top"}, [
           td([div({-class=>"propHeading1"}, "Type:"), $scfConfig->{"TYPE"}]),
        ]));
    if ($scfConfig->{"TYPE"} eq "LiveRecording")
    {
        push(@tableRows,
            Tr({-align=>"left", -valign=>"top"}, [
               td([div({-class=>"propHeading1"}, "Location:"), $scfConfig->{"LOCATION"}]),
            ]));
    }
    else
    {
        push(@tableRows,
            Tr({-align=>"left", -valign=>"top"}, [
               td([div({-class=>"propHeading1"}, "Spool number:"), $scfConfig->{"SPOOL"}]),
            ]));
    }

    
    my @trackRows;
    push(@trackRows,
        Tr({-align=>"left", -valign=>"top"}, [
           th(["Id", "Name", "Type"]),
        ]));
        
    
    foreach my $track (@{$sourceConfig->{"tracks"}})
    {
        push(@trackRows, 
            Tr({-align=>"left", -valign=>"top"}, [
                td([$track->{"TRACK_ID"}, 
                    $track->{"NAME"},
                    $track->{"DATA_DEF"},]),
            ]));
        
    }
    
    push(@tableRows,
        Tr({-align=>"left", -valign=>"top"}, [
           td([div({-class=>"propHeading1"}, "Tracks:"), 
            table({-class=>"borderTable"}, @trackRows)]),
        ]));

    
    push(@pageContent, table({-class=>"noBorderTable"},
        @tableRows));

    return join("", @pageContent);
}


####################################
#
# Recorder
#
####################################

sub get_recorder
{
    my ($rec, $noLink) = @_;
    
    # stop the warnings about deep recursion
    push(@CGI::Pretty::AS_IS, qw(div table th tr td));

    
    my @pageContent;

    my @inputRows;
    foreach my $ricf (@{ $rec->{"inputs"} })
    {
        my @trackRows;
        push(@trackRows,
            Tr({-align=>"left", -valign=>"top"}, 
               th(["Id", "Source Name", "Source Track Name", "Clip Track Number"]),
            )
        );
        foreach my $ritcf (@{ $ricf->{"tracks"} })
        {
            push(@trackRows, 
                Tr({-align=>"left", -valign=>"top"}, 
                    td([$ritcf->{"INDEX"}, 
                        (defined $ritcf->{"SOURCE_NAME"}) ? $ritcf->{"SOURCE_NAME"} : "",
                        (defined $ritcf->{"SOURCE_TRACK_NAME"}) ? $ritcf->{"SOURCE_TRACK_NAME"} : "",
                        (defined $ritcf->{"TRACK_NUMBER"}) ? $ritcf->{"TRACK_NUMBER"} : "",
                    ]),
                )
            );
        }

        push(@inputRows,
            Tr({-align=>"left", -valign=>"top"}, [
                td([div({-class=>"propHeading2"}, "Index:"), $ricf->{"config"}->{"INDEX"}]),
                td([div({-class=>"propHeading2"}, "Name:"), $ricf->{"config"}->{"NAME"}]),
                td([div({-class=>"propHeading2"}, "Tracks:"), 
                    table({-class=>"borderTable"}, @trackRows)
                ]),
            ])
        );
    }

    my @recRows;
    push(@recRows,
        Tr({-align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), $rec->{"recorder"}->{"NAME"}]),
            td([div({-class=>"propHeading1"}, "Inputs:"),
                table({-class=>"noBorderTable"}, @inputRows)
            ]),
            td([div({-class=>"propHeading1"}, "Config:"),
                ($noLink ?
                      $rec->{"recorder"}->{"CONF_NAME"}
                    : a({-href=>"#RecorderConfig-$rec->{'recorder'}->{'CONF_ID'}"}, $rec->{"recorder"}->{"CONF_NAME"})),
            ]),
        ])
    );

    push(@pageContent, table({-class=>"noBorderTable"}, @recRows));


    return join("", @pageContent);
}

####################################
#
# Recorder config
#
####################################


sub get_recorder_config_popup
{
    my ($paramId, $rcfs, $defaultId) = @_;
    
    my $default;
    my @values;
    my %labels;

    foreach my $rcf (@{ $rcfs })
    {
        my $value = $rcf->{"config"}->{"ID"};
        my $label = $rcf->{"config"}->{"NAME"};
        push(@values, $value);
        $labels{$value} = $label;

        if (!defined $default && 
            defined $defaultId && $rcf->{"config"}->{"ID"} == $defaultId)
        {
            $default = $value;
        }
    }
    
    if (!defined $default)
    {
        return popup_menu(
            -name => $paramId, 
            -values => \@values,
            -labels => \%labels);
    }
    else
    {
        return popup_menu(
            -name => $paramId, 
            -default => $default,
            -values => \@values,
            -labels => \%labels);
    }
}

sub get_video_resolution_popup
{
    my ($paramId, $vrs, $defaultId) = @_;
    
    my $default;
    my @values;
    my %labels;

    # not set option
    push(@values, 0);
    $labels{0} = "not set";

    foreach my $vrn (@{ $vrs })
    {
        my $value = "$vrn->{'ID'}";
        my $label = "$vrn->{'NAME'}";
        push(@values, $value);
        $labels{$value} = $label;

        if (!defined $default && 
            defined $defaultId && "$vrn->{'ID'}" eq $defaultId)
        {
            $default = $value;
        }
    }
    
    # no default then is not set
    if (!defined $default)
    {
        $default = 0;
    }
    
    
    return popup_menu(
        -name => $paramId, 
        -default => $default,
        -values => \@values,
        -labels => \%labels);
}

sub get_recorder_config
{
    my ($rcf, $vrs) = @_;
    
    # stop the warnings about deep recursion
    push(@CGI::Pretty::AS_IS, qw(div table th tr td));

    
    my @pageContent;

    my @paramRows;
    push(@paramRows,
        Tr({-align=>"left", -valign=>"top"}, 
           th(["Name", "Value"]),
        )
    );
    foreach my $rp (sort { $a->{"NAME"} cmp $b->{"NAME"} } (@{ $rcf->{"parameters"} }))
    {
        my $value = $rp->{"VALUE"};
        if ($rp->{"NAME"} eq "MXF_RESOLUTION" ||
            $rp->{"NAME"} eq "ENCODE1_RESOLUTION" ||
            $rp->{"NAME"} eq "ENCODE2_RESOLUTION" ||
            $rp->{"NAME"} eq "QUAD_RESOLUTION")
        {
            if ($rp->{"VALUE"} == 0)
            {
                $value = "not set";
            }
            else
            {
                foreach my $vrn (@{ $vrs })
                {
                    if ($vrn->{"ID"} == $rp->{"VALUE"})
                    {
                        $value = "$vrn->{'NAME'}";
                        last;
                    }
                }
            }
        }
        push(@paramRows,
            Tr({-align=>"left", -valign=>"top"}, 
                td([$rp->{"NAME"}, $value]),
            )
        );
    }
    
    my @rcfRows;
    push(@rcfRows,
        Tr({-align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), $rcf->{"config"}->{"NAME"}]),
            td([div({-class=>"propHeading1"}, "Parameters:"), 
                table({-class=>"noBorderTable"}, @paramRows)
            ]),
        ])
    );
        
    push(@pageContent, table({-class=>"noBorderTable"}, @rcfRows));
      
        
    return join("", @pageContent);
}


####################################
#
# Multi-camera config
#
####################################


sub get_multicam_config
{
    my ($mccf) = @_;
    
    my @pageContent;

    my @mccfRows;

    push(@mccfRows,
        Tr({-align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), $mccf->{"config"}->{"NAME"}]),
        ]));
        

    my @trackRows;
    push(@trackRows,
        Tr({-align=>"left", -valign=>"top"}, [
           th(["Index", "Clip track num", "Selectors"]),
        ]));
        
    foreach my $mctcf (@{ $mccf->{"tracks"} })
    {
        my @selectorRows;
        push(@selectorRows,
            Tr({-align=>"left", -valign=>"top"}, [
               th(["Index", "Source Name", "Source Track Name"]),
            ]));
            
        foreach my $mcscf (@{$mctcf->{"selectors"}})
        {
            push(@selectorRows, 
                Tr({-align=>"left", -valign=>"top"}, [
                    td([$mcscf->{"INDEX"}, 
                        ($mcscf->{"SOURCE_NAME"}) ? $mcscf->{"SOURCE_NAME"} : "",
                        ($mcscf->{"SOURCE_TRACK_NAME"}) ? $mcscf->{"SOURCE_TRACK_NAME"} : "",
                    ]),
                ]));
        }

        push(@trackRows,
            Tr({-align=>"left", -valign=>"top"}, [
                td([$mctcf->{"config"}->{"INDEX"},
                    $mctcf->{"config"}->{"TRACK_NUMBER"},
                    table({-class=>"borderTable"}, @selectorRows),
                ]),
            ]));
    }

    push(@mccfRows,
        Tr({-align=>"left", -valign=>"top"}, [
           td([div({-class=>"propHeading1"}, "Tracks:"), 
               table({-class=>"borderTable"}, @trackRows)
           ]),
        ]));
        
    push(@pageContent, table({-class=>"noBorderTable"},
        @mccfRows));
        
        
    return join("", @pageContent);
}


####################################
#
# Proxy definition
#
####################################

sub get_proxy_types_popup
{
    my ($popupName, $pxts) = @_;
    
    my $default;
    my @values;
    my %labels;
    foreach my $pxt ( @{ $pxts } )
    {
        push(@values, $pxt->{"ID"});
        $labels{$pxt->{"ID"}} = $pxt->{"NAME"};
        
        if (!defined $default)
        {
            $default = $pxt->{"ID"};
        }
    }
    
    return popup_menu(
        -name=>$popupName,
        -default=>$default,
        -values=>\@values,
        -labels=>\%labels
    );
}

sub get_proxy_def
{
    my ($pxdf) = @_;
    
    my @pageContent;

    my @pxdfRows;

    push(@pxdfRows,
        Tr({-align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), $pxdf->{"def"}->{"NAME"}]),
            td([div({-class=>"propHeading1"}, "Type:"), $pxdf->{"def"}->{"TYPE"}]),
        ]));
        

    my @trackRows;
    push(@trackRows,
        Tr({-align=>"left", -valign=>"top"}, [
           th(["ID", "Data def", "Sources"]),
        ]));
        
    foreach my $pxtdf (@{ $pxdf->{"tracks"} })
    {
        my @sourceRows;
        push(@sourceRows,
            Tr({-align=>"left", -valign=>"top"}, [
               th(["Index", "Source Name", "Source Track Name"]),
            ]));
            
        foreach my $pxsdf (@{$pxtdf->{"sources"}})
        {
            push(@sourceRows, 
                Tr({-align=>"left", -valign=>"top"}, [
                    td([$pxsdf->{"INDEX"}, 
                        ($pxsdf->{"SOURCE_NAME"}) ? $pxsdf->{"SOURCE_NAME"} : "",
                        ($pxsdf->{"SOURCE_TRACK_NAME"}) ? $pxsdf->{"SOURCE_TRACK_NAME"} : "",
                    ]),
                ]));
        }

        push(@trackRows,
            Tr({-align=>"left", -valign=>"top"}, [
                td([$pxtdf->{"def"}->{"TRACK_ID"},
                    $pxtdf->{"def"}->{"DATA_DEF"},
                    table({-class=>"borderTable"}, @sourceRows),
                ]),
            ]));
    }

    push(@pxdfRows,
        Tr({-align=>"left", -valign=>"top"}, [
           td([div({-class=>"propHeading1"}, "Tracks:"), 
               table({-class=>"borderTable"}, @trackRows)
           ]),
        ]));
        
    push(@pageContent, table({-class=>"noBorderTable"},
        @pxdfRows));
        
        
    return join("", @pageContent);
}



####################################
#
# Router config
#
####################################


sub get_router_config
{
    my ($rocf) = @_;
    
    # stop the warnings about deep recursion
    push(@CGI::Pretty::AS_IS, qw(div table th tr td));

    
    my @pageContent;

    
    # inputs
    
    my @inputRows;
    push(@inputRows,
        Tr({-align=>"left", -valign=>"top"}, 
           th(["Index", "Name", "Source Name", "Source Track Name"]),
        )
    );
    foreach my $input (@{ $rocf->{"inputs"} })
    {
        push(@inputRows, 
            Tr({-align=>"left", -valign=>"top"}, 
                td([$input->{"INDEX"}, $input->{"NAME"}, 
                    (defined $input->{"SOURCE_NAME"}) ? $input->{"SOURCE_NAME"} : "",
                    (defined $input->{"SOURCE_TRACK_NAME"}) ? $input->{"SOURCE_TRACK_NAME"} : "",
                ]),
            )
        );
    }

    
    # outputs
    
    my @outputRows;
    push(@outputRows,
        Tr({-align=>"left", -valign=>"top"}, 
           th(["Index", "Name"]),
        )
    );
    foreach my $output (@{ $rocf->{"outputs"} })
    {
        push(@outputRows, 
            Tr({-align=>"left", -valign=>"top"}, 
                td([$output->{"INDEX"}, $output->{"NAME"}]),
            )
        );
    }

    
    my @rocfRows;
    push(@rocfRows,
        Tr({-align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), $rocf->{"config"}->{"NAME"}]),
            td([div({-class=>"propHeading1"}, "Inputs:"), 
                table({-class=>"borderTable"}, @inputRows)
            ]),
            td([div({-class=>"propHeading1"}, "Outputs:"), 
                table({-class=>"borderTable"}, @outputRows)
            ]),
        ])
    );
        
    push(@pageContent, table({-class=>"noBorderTable"}, @rocfRows));
      
        
    return join("", @pageContent);
}




1;
