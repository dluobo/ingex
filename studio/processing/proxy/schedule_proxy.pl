#!/usr/bin/perl -W

#
# $Id: schedule_proxy.pl,v 1.1 2007/09/11 14:08:45 stuart_hc Exp $
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

use Getopt::Std;

use lib ".";
use prodautodb; # can be found in ingex/Ingex/web/manage/modules


# default settings

my $dbhost = "localhost";
my $dbname = "prodautodb";
my $dbuser = "bamzooki";
my $dbpassword = "bamzooki";
my $maxMaterialDelay = 5 * 60; # number of seconds to wait for material needed for a proxy 


# get options

sub print_usage
{
    printf "Usage: $0 <options>\n\n"
        . "Options:\n"
        . "    -h           Show this usage message\n"
        . "    -i <host>    Database host name (default = $dbhost)\n"
        . "    -d <name>    Database name (default = '$dbname')\n"
        . "    -u <name>    Database user name (default = '$dbuser')\n"
        . "    -p <passwd>  Database user password\n"
        . "    -w <secs>    Number of seconds to wait for material (default = $maxMaterialDelay secs)\n"
        . "\n";

    return 1;        
}

my %opts;
getopt('idupw', \%opts) or (print_usage() && exit(1));

(print_usage() && exit(0)) if ($opts{"h"} or $opts{"-help"});

$dbhost = $opts{"i"} if ($opts{"i"});
$dbname = $opts{"d"} if ($opts{"d"});
$dbuser = $opts{"u"} if ($opts{"u"});
$dbpassword = $opts{"p"} if ($opts{"p"});
$maxMaterialDelay = $opts{"w"} if ($opts{"w"});


# check options

if ($maxMaterialDelay !~ /^\d*$/)
{
    print_usage();
    die "Invalid time (seconds) to wait for material for proxy scheduling";
}


# open connection to database

my $dbh = prodautodb::connect($dbhost, $dbname, $dbuser, $dbpassword) 
    or die "failed to connect to database ($dbhost, $dbname, $dbuser, $dbpassword)";
    


while (1)
{
    print "Scheduling proxy generation...\n";
    schedule_proxies();
    print "Done scheduling proxy generation.\n";
    sleep 30;
}



sub schedule_proxies
{
    # load source configs that are referenced by the proxy definitions
    
    my $scfs = load_source_configs($dbh)
        or die "failed to load source configs: $prodautodb::errstr";
    
       
    # load proxy definitions
    
    my $pxdefs = load_proxy_defs($dbh) 
        or die "failed to load proxy defs: $prodautodb::errstr";
    
    
    # load all the new material
    
    my $material = load_material_for_proxy($dbh)
        or die "failed to load material for proxy: $prodautodb::errstr";
    
    
    # go through the new material and schedule proxy generation
    
    my %processedMaterialPackages;
    foreach my $materialPackage (values % {$material->{"materials"} } )
    {
        next if (defined $processedMaterialPackages{$materialPackage->{"ID"}});
        $processedMaterialPackages{$materialPackage->{"ID"}} = 1;
        
        
        # group material wrt creation date date and start time
        
        my @materialPackageGroup;
        foreach my $materialPackage2 (values % { $material->{"materials"} } )
        {
            next if (defined $processedMaterialPackages{$materialPackage2->{"ID"}});
            
            push(@materialPackageGroup, $materialPackage);
            
            if (get_creation_date_date($materialPackage, -1) 
                    eq get_creation_date_date($materialPackage2, -2) 
                && get_start_time($materialPackage, $material->{"packages"}, -1) 
                    == get_start_time($materialPackage2, $material->{"packages"}, -2))
            {
                push(@materialPackageGroup, $materialPackage2);
                $processedMaterialPackages{$materialPackage2->{"ID"}} = 1;
            }
        }
        
        
        # schedule proxy generation if all source material is present in group
        
        my %proxy;
        foreach my $pxdef (@ { $pxdefs })
        {
            $proxy{"NAME"} = $pxdef->{"def"}->{"NAME"};
            $proxy{"TYPE_ID"} = $pxdef->{"def"}->{"TYPE_ID"};
            $proxy{"START_DATE"} = get_creation_date_date($materialPackage, undef);
            $proxy{"START_TIME"} = get_start_time($materialPackage, $material->{"packages"}, undef);
            $proxy{"STATUS_ID"} = 1; # not started
            $proxy{"tracks"} = [];
        
            my $isComplete = 1;
            PROXYTRACK : foreach my $pxtdef (@ { $pxdef->{"tracks"} })
            {
                my %proxyTrack;
                $proxyTrack{"TRACK_ID"} = $pxtdef->{"def"}->{"TRACK_ID"};
                $proxyTrack{"DATA_DEF_ID"} = $pxtdef->{"def"}->{"DATA_DEF_ID"};
                $proxy{"sources"} = [];
                push(@{$proxy{"tracks"}}, \%proxyTrack);
                
                foreach my $pxsdef (@ { $pxtdef->{"sources"} })
                {
                    if (defined $pxsdef->{"SOURCE_ID"})
                    {
                        my %proxySource;
                        $proxySource{"INDEX"} = $pxsdef->{"INDEX"};
                        push(@{$proxyTrack{"sources"}}, \%proxySource);
                        
                        my ($filename, $materialPackageId, $materialTrackId, 
                            $fileSourcePackageId, $fileSourceTrackId) = 
                            get_filename(
                                $scfs,
                                $pxsdef->{"SOURCE_ID"},
                                $pxsdef->{"SOURCE_TRACK_ID"},
                                \@materialPackageGroup,
                                $material->{"packages"}
                            );
                        
                        if (!defined $filename)
                        {
                            # source material was not found so the material required
                            # for the proxy is incomplete
                            $isComplete = 0;
                            last PROXYTRACK;
                        }
                        
                        $proxySource{"SOURCE_MATERIAL_PACKAGE_UID"} = $materialPackageId;
                        $proxySource{"SOURCE_MATERIAL_TRACK_ID"} = $materialTrackId;
                        $proxySource{"SOURCE_FILE_PACKAGE_UID"} = $fileSourcePackageId;
                        $proxySource{"SOURCE_FILE_TRACK_ID"} = $fileSourceTrackId;
                        $proxySource{"FILENAME"} = $filename;
                    }
                }
            }
            
            
            # save to proxy generation scheduling to database if all material
            # is available and the proxy has not already being scheduled
            
            if ($isComplete)
            {
                if (!have_proxy($dbh, \%proxy))
                {
                    print "scheduled proxy - $proxy{'NAME'}: $proxy{'START_DATE'}, $proxy{'START_TIME'}\n";
                    save_proxy($dbh, \%proxy)
                        or print "failed to save proxy: $prodautodb::errstr";
                }
                else
                {
                    print "proxy already scheduled - $proxy{'NAME'}: $proxy{'START_DATE'}, $proxy{'START_TIME'}\n";
                }
            }
            else
            {
                print "incomplete proxy material - $proxy{'NAME'}: $proxy{'START_DATE'}, $proxy{'START_TIME'}\n";
            }
        }
        
    }
    
    
    
    # if material is older than $maxMaterialDelay seconds then remove the reference
    
    delete_material_for_proxy($dbh, $maxMaterialDelay) 
        or print "failed to delete material for proxy: $prodautodb::errstr";
    
}    


# get the MXF filename of the essence associated with the source track   
sub get_filename
{
    my ($scfs, $scfId, $sourceTrackId, $materialGroup, $packages) = @_;
    
    my ($filename, $materialPackageId, $materialTrackId, $fileSourcePackageId, $fileSourceTrackId);
    
    
    # get the source config name that will identify the tape/live source package 
    
    my $sourceConfigName;
    foreach my $scf (@ { $scfs } )
    {
        if ($scf->{"config"}->{"ID"} == $scfId)
        {
            $sourceConfigName = $scf->{"config"}->{"NAME"};
            last;
        }
    }
    if (!defined $sourceConfigName)
    {
        # we should never be here
        print "error: unknown source config\n";
        return undef;
    }
    
    
    
    # find the tape/live source package for that source config
    # the name of the source package is the name of the source config
    # plus ' - <date>'
    
    my $sourcePackage;
    foreach my $package (values % { $packages } )
    {
        if (defined $package->{"NAME"}
            && defined $package->{"descriptor"}
            && ($package->{"descriptor"}->{"TYPE"} == 2 # tape
                || $package->{"descriptor"}->{"TYPE"} == 3) # live
            && $package->{"NAME"} =~ /^$sourceConfigName\s-\s\d*-\d*-\d*$/) # name + date suffix
        {
            $sourcePackage = $package;
            last;
        }
    }
    if (!defined $sourcePackage)
    {
        # we arrive here if no essence has been ingested from the source package
        # created from the given source config
        # it could be that the material details were written to the database just 
        # after we read the list of new material
        print "unknown tape/live source package\n";
        return undef;
    }

    
    
    # check the track exists in the tape/live source package
    
    my $foundTrack = 0;
    foreach my $track (@ { $sourcePackage->{"tracks"} } )
    {
        if ($track->{"TRACK_ID"} == $sourceTrackId)
        {
            $foundTrack = 1;
            last;
        }
    }
    if (!$foundTrack)
    {
        # we should never be here
        print "error: unknown track in tape/live source package\n";
        return undef;
    }
    
    
    # find the material and file package and track that references the live/tape source */ 
    
    MATERIAL: foreach my $material (@ { $materialGroup } )
    {
        foreach my $materialTrack (@ { $material->{"tracks"} })
        {
            if (defined $materialTrack->{"sourceclip"}->{"SOURCE_PACKAGE_UID"})
            {
                my $fileSourcePackage = $packages->{$materialTrack->{"sourceclip"}->{"SOURCE_PACKAGE_UID"}};
                if (defined $fileSourcePackage
                    && defined $fileSourcePackage->{"descriptor"}
                    && defined $fileSourcePackage->{"descriptor"}->{"FILE_LOCATION"})
                {
                    foreach my $fileSourceTrack (@ { $fileSourcePackage->{"tracks"} })
                    {
                        if (defined $fileSourceTrack->{"sourceclip"}->{"SOURCE_PACKAGE_UID"} 
                            && $fileSourceTrack->{"sourceclip"}->{"SOURCE_PACKAGE_UID"} 
                                eq $sourcePackage->{"UID"}
                            && $fileSourceTrack->{"sourceclip"}->{"SOURCE_TRACK_ID"} == $sourceTrackId)
                        {
                            $materialPackageId = $material->{"UID"};
                            $materialTrackId = $materialTrack->{"TRACK_ID"};
                            $fileSourcePackageId = $fileSourcePackage->{"UID"};
                            $fileSourceTrackId = $fileSourceTrack->{"TRACK_ID"};
                            $filename = $fileSourcePackage->{"descriptor"}->{"FILE_LOCATION"};
                            last MATERIAL;
                        }
                    }
                }
            }
        }
    }
    if (!defined $materialPackageId)
    {
        # the material isn't available or was not yet written to the database
        print "unknown material package for live/tape source\n";
    }
    
    return ($filename, $materialPackageId, $materialTrackId, $fileSourcePackageId, $fileSourceTrackId);
}



# extract the date portion of the creation date (which is actually a timestamp)
sub get_creation_date_date
{
    my ($materialPackage, $errorValue) = @_;
    
    my ($year, $month, $day) = ($materialPackage->{"CREATION_DATE"} =~ /^(\d*)-(\d*)-(\d*).*$/);
    return $year . "-" . $month . "-" . $day if (defined $year);
    return $errorValue;
}



# extract the start time for the material. The start time is present in the SourceClip
# reference in the file SourcePackage
sub get_start_time
{
    my ($materialPackage, $packages, $errorValue) = @_;
    
    foreach my $materialTrack (@ { $materialPackage->{"tracks"} })
    {
        # get the file source package referenced by this track
        my $sourcePackage = $packages->{$materialTrack->{"sourceclip"}->{"SOURCE_PACKAGE_UID"}};
        if (defined $sourcePackage 
            && defined $sourcePackage->{"descriptor"} 
            && $sourcePackage->{"descriptor"}->{"TYPE"} == 1) # File essence descriptor
        {
            # get the start position on the source clip
            foreach my $sourceTrack (@ { $sourcePackage->{"tracks"} })
            {
                if ($sourceTrack->{"DATA_DEF_ID"} == 1 # Picture
                    || $sourceTrack->{"DATA_DEF_ID"} == 2) # Sound
                {
                    if ($sourceTrack->{"EDIT_RATE_NUM"} == $materialTrack->{"EDIT_RATE_NUM"}
                        && $sourceTrack->{"EDIT_RATE_DEN"} == $materialTrack->{"EDIT_RATE_DEN"})
                    {
                        return $sourceTrack->{"sourceclip"}->{"POSITION"};
                    }
                    else
                    {
                        # convert start position to edit units of material package track
                        my $factor = $materialTrack->{"EDIT_RATE_NUM"} * $sourceTrack->{"EDIT_RATE_DEN"}
                            / ($materialTrack->{"EDIT_RATE_DEN"} * $sourceTrack->{"EDIT_RATE_NUM"});
                        return int($sourceTrack->{"sourceclip"}->{"POSITION"} * $factor + 0.5);
                    }
                }
            }
        }
    }
    
    # we shouldn't be here
    print "error: failed to get start time for material package\n";
    return $errorValue;
}




END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


