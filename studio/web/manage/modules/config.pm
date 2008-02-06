#
# $Id: config.pm,v 1.2 2008/02/06 16:59:14 john_f Exp $
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
 
package config;

use strict;



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
        %ingexConfig
        &get_all_avid_aaf_export_dirs
        &get_avid_aaf_export_dir
        &get_all_directors_cut_dbs
        &get_directors_cut_db
    );
}



####################################
#
# Exported package globals
#
####################################

our %ingexConfig;

load_config("ingex.conf");


####################################
#
# Load configuration
#
####################################

sub load_config
{
    my ($configFilename) = @_;
    
    my $configFile;
    open($configFile, "<", "$configFilename") or die "Failed to open config file: $!";
    
    while (my $line = <$configFile>) 
    {
        chomp($line);
        $line =~ s/#.*//;
        $line =~ s/^\s+//;
        $line =~ s/\s+$//;
        if (length $line > 0)
        {
            my ($key,$value) = split(/\s*=\s*/, $line, 2);
            $ingexConfig{$key} = $value;
            
            # untaint values
            # TODO: do a better job with matching
            ($ingexConfig{"db_odbc_dsn"}) = ($ingexConfig{"db_odbc_dsn"} =~ /(.*)/)
                if ($ingexConfig{"db_odbc_dsn"});
            ($ingexConfig{"db_host"}) = ($ingexConfig{"db_host"} =~ /(.*)/)
                if ($ingexConfig{"db_host"});
            ($ingexConfig{"db_name"}) = ($ingexConfig{"db_name"} =~ /(.*)/)
                if ($ingexConfig{"db_name"});
            ($ingexConfig{"db_user"}) = ($ingexConfig{"db_user"} =~ /(.*)/)
                if ($ingexConfig{"db_user"});
            ($ingexConfig{"db_password"}) = ($ingexConfig{"db_password"} =~ /(.*)/)
                if ($ingexConfig{"db_password"});
                
            ($ingexConfig{"create_aaf_dir"}) = ($ingexConfig{"create_aaf_dir"} =~ /(.*)/)
                if ($ingexConfig{"create_aaf_dir"});
            ($ingexConfig{"avid_aaf_export_dir"}) = ($ingexConfig{"avid_aaf_export_dir"} =~ /(.*)/)
                if ($ingexConfig{"avid_aaf_export_dir"});
            ($ingexConfig{"avid_aaf_prefix"}) = ($ingexConfig{"avid_aaf_prefix"} =~ /(.*)/)
                if ($ingexConfig{"avid_aaf_prefix"});
            ($ingexConfig{"directors_cut_db"}) = ($ingexConfig{"directors_cut_db"} =~ /(.*)/)
                if ($ingexConfig{"directors_cut_db"});
            
            ($ingexConfig{"default_material_rows"}) = ($ingexConfig{"default_material_rows"} =~ /(.*)/)
                if ($ingexConfig{"default_material_rows"});
            ($ingexConfig{"max_material_rows"}) = ($ingexConfig{"max_material_rows"} =~ /(.*)/)
                if ($ingexConfig{"max_material_rows"});
        }
    }
} 

    
####################################
#
# Load configuration
#
####################################

sub get_all_avid_aaf_export_dirs
{
    my @pairs = split(/\s*\,\s*/, $ingexConfig{"avid_aaf_export_dir"});

    my @exportDirs;
    foreach my $pair (@pairs)
    {
        my ($name, $dir) = split(/\s*\|\s*/, $pair);
        push(@exportDirs, [$name, $dir]);
    }
    
    return @exportDirs;
}

sub get_avid_aaf_export_dir
{
    my ($targetName) = @_;
    
    my @pairs = split(/\s*\,\s*/, $ingexConfig{"avid_aaf_export_dir"});
    
    foreach my $pair (@pairs)
    {
        my ($name, $dir) = split(/\s*\|\s*/, $pair);
        if ($name eq $targetName)
        {
            return (defined $dir) ? $dir : "";
        }
    }
    
    return undef;
}


sub get_all_directors_cut_dbs
{
    my @pairs = split(/\s*\,\s*/, $ingexConfig{"directors_cut_db"});

    my @dcDBs;
    foreach my $pair (@pairs)
    {
        my ($name, $filename) = split(/\s*\|\s*/, $pair);
        push(@dcDBs, [$name, $filename]);
    }
    
    return @dcDBs;
}

sub get_directors_cut_db
{
    my ($targetName) = @_;
    
    my @pairs = split(/\s*\,\s*/, $ingexConfig{"directors_cut_db"});
    
    foreach my $pair (@pairs)
    {
        my ($name, $filename) = split(/\s*\|\s*/, $pair);
        if ($name eq $targetName)
        {
            return (defined $filename) ? $filename : "";
        }
    }
    
    return undef;
}


1;

