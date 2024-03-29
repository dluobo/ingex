# $Id: ingexconfig.pm,v 1.4 2010/04/09 10:51:33 john_f Exp $
# Copyright (C) 2008-9 British Broadcasting Corporation
# All rights reserved
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
 
package ingexconfig;

use strict;
use File::Temp qw(mktemp tempfile); 

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
    );
}



####################################
#
# Exported package globals
#
####################################

our %ingexConfig;
$ingexConfig{'WEB_ROOT'} = get_webroot();
load_config($ingexConfig{'WEB_ROOT'}."/cgi-bin/ingex-config/WebIngex.conf");
untaint();


####################################
#
# Get web root for ingex
#
####################################

sub get_webroot
{
	# stored in ingex.conf, which can either be found in ~ or /etc
	my $confFile;
	my $webroot;
	
	my $fname = "~/ingex.conf";
	-s $fname
		or $fname = "/etc/ingex.conf";
	if(-s $fname){
		open($confFile, "<", $fname) or die "Failed to open ingex.conf file: $!";
		
		while (my $line = <$confFile>) 
    	{
	        chomp($line);
	        $line =~ s/#.*//;
	        $line =~ s/^\s+//;
	        $line =~ s/\s+$//;
	        if (length $line > 0)
	        {
	            my ($key,$value) = split(/\s*=\s*/, $line, 2);
	            if(uc $key eq "WEB_ROOT"){
	            	$value =~ s/"//g;
            		$webroot = $value;
	            	last;
	            }
	        }
    	}
	}
	else{
		# no conf file found - use default suse location
		$webroot = "/srv/www";
	}
	
	return $webroot;
}


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
        }
    }
}


####################################
#
# Write key and value to configuration
#
####################################

sub write_config
{
	my ($configFilename, $newKey, $newValue) = @_;
	
	my $next;
	my $match;
	my $newFilename = mktemp("/tmp/configFile_XXXXXX");
    my $oldFilename = "$configFilename.old";
    
    open(IN, "<", "$configFilename") or die "Failed to open config file: $!";
    open(NEW, ">", "$newFilename") or die "Failed to open temp file: $!";
    
  
    # untaint values
    $newKey =~ /(.*)/;
    $newValue =~ /(.*)/;
    
    while(my $line = <IN>)
    {
    	chomp($line);
    	
    	$next = $line;
    	$next =~ s/#.*//;
        $next =~ s/^\s+//;
        $next =~ s/\s+$//;
        
        if (length $next > 0)
        {
            my ($key,$value) = split(/\s*=\s*/, $next, 2);
            
            if($key eq $newKey)
            {
            	$line = "$newKey = $newValue";	# replace line
            	$match = 1;
            }
        }
        
        print NEW "$line\n";		# store next line
    }
    
    # if key was not found, add to end of file
    if(!$match)
	{
		my $line = "$newKey = $newValue";	# new line
        print NEW "$line\n";
	}
    
    close(IN);
    close(NEW);
    
    rename($configFilename, $oldFilename);
    rename($newFilename, $configFilename);
    
}


sub untaint
{
	 # untaint values
        # TODO: do a better job with matching
        ($ingexConfig{"WEB_ROOT"}) = ($ingexConfig{"WEB_ROOT"} =~ /(.*)/)
            if ($ingexConfig{"WEB_ROOT"});
        ($ingexConfig{"db_host"}) = ($ingexConfig{"db_host"} =~ /(.*)/)
            if ($ingexConfig{"db_host"});
        ($ingexConfig{"db_name"}) = ($ingexConfig{"db_name"} =~ /(.*)/)
            if ($ingexConfig{"db_name"});
        ($ingexConfig{"db_user"}) = ($ingexConfig{"db_user"} =~ /(.*)/)
            if ($ingexConfig{"db_user"});
        ($ingexConfig{"db_password"}) = ($ingexConfig{"db_password"} =~ /(.*)/)
            if ($ingexConfig{"db_password"});
        ($ingexConfig{"ingex_log"}) = ($ingexConfig{"ingex_log"} =~ /(.*)/)
            if ($ingexConfig{"ingex_log"});
}

1;

