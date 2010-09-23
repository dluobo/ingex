#! /usr/bin/perl -w

#/***************************************************************************
# * $Id: xferserver.pl,v 1.19 2010/09/23 17:32:13 john_f Exp $             *
# *                                                                         *
# *   Copyright (C) 2008-2010 British Broadcasting Corporation              *
# *   - all rights reserved.                                                *
# *   Authors: Philip de Nier, Matthew Marks                                *
# *                                                                         *
# *   This program is free software; you can redistribute it and/or modify  *
# *   it under the terms of the GNU General Public License as published by  *
# *   the Free Software Foundation; either version 2 of the License, or     *
# *   (at your option) any later version.                                   *
# *                                                                         *
# *   This program is distributed in the hope that it will be useful,       *
# *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
# *   GNU General Public License for more details.                          *
# *                                                                         *
# *   You should have received a copy of the GNU General Public License     *
# *   along with this program; if not, write to the                         *
# *   Free Software Foundation, Inc.,                                       *
# *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
# ***************************************************************************/

# This script controls copying of material from the local host to a remote file server mounted as a network drive or via ftp, under the instruction of clients, applying switchable traffic control to prevent saturation of the local disks.

# In the following description, the capitalised words refer to constants defined below.

# The script listens on network port CLIENT_PORT for clients to open connections, send parameters and then close connections.  These clients will typically be Ingex recorders or Ingex players.
# Parameters are as follows:
# <client name> [<identifier> [<priority> <source path> <destination path>]] ... (each terminated with a newline)

# The client name and identifier combination unambiguously identify an event requiring traffic control, to cater for, e.g., recorders which can make more than one recording simultaneously.
# If no other parameters are supplied, this indicates the start of the event.  The name/identifier pair are added to an event list, and traffic control is switched on at rate SLOW_LIMIT for copying to mounted drives, and FTP_SPEED for copying via FTP when option -f is used.
# Additional parameters indicate the end of the event, or a recorder being started.  This will result in the event's identifier being removed from the event list (if present), and if the list is then empty, traffic control being switched off (reverting to copying at FAST_LIMIT for mounted drives, which is unlimited if set to zero, and unlimited for FTP).
# Identifier values are arbitrary (most likely an incrementing number) except for the special case '0' (zero), which is interpreted as covering all events belonging to the associated client.  This value should be sent when a client is started, so that the event list is cleared of any previous events belonging to this client.  This ensures that traffic control does not remain on permanently due to a client crashing and hence not indicating the end of an event.  It should not be used as an event identifier if the client can have more than one event in action simultaneously.
# A further special case is to provide the client name on its own.  This is also interpreted as clearing the list of events belonging to this client (as if '0' followed by more arguments had been supplied), and is useful for clients such as players which do not provide copying path information.  (Players typically only play one file set at once, which means that they can always supply the same identifier value to switch traffic control on.)

# Arguments after the client name and identifier should be supplied in groups of three: priority (a positive number), source path and destination path.  The script will attempt to copy all the files in all the path pairs at the highest priority (the smallest priority argument value) before moving on to the next, and so on.  (Offline files would normally be given higher priority [smaller argument value] than online files, because they are likely to be needed sooner and are smaller so will be copied more quickly.)  The script saves paths in file PATHS_FILE (unless overridden with the -a option) to allow copying to commence without client intervention when the script is (re)started.  When a client supplies paths, the copying cycle starts or restarts from the highest priority, letting any pending file copy finish first.  This minimises the delay in copying high-priority files without wasting any bandwidth by re-copying.

# In each source directory and any subdirectories whose names match SUBDIRS, every file whose name begins with 8 digits and an underscore* and ends in one of the EXTNS (case-insensitive) is examined.  If it does not exist in a subdirectory of the corresponding destination path, named after the first 8 digits of the filename*, it is copied using the COPY executable or curl (FTP mode), into <destination path>/<first 8 digits of filename>*/<source subdirectory if present>/<DEST_INCOMING>, creating directories if necessary.  After a successful copy, the file is moved up one directory level.  This prevents anything looking at the destination directories from seeing partial files.  At the end of each copying cycle, the (empty) DEST_INCOMING subdirectories are removed.  (* If option -d is provided, 8-digit subdirectories are not used and files do not have to start with 8 digits and an underscore.)

# If the file already exists in its final resting place, but is of a different length to the source file, a warning is issued and the destination file is renamed to allow copying without overwriting it.  (Copying is abandoned if there is already a file with the new name.)  If a file fails to copy, any partial file is deleted and no more files are copied from that source directory (because the order of copying must be maintained - see below).  Once a copy cycle has been successfully completed, the script sleeps until it receives further client connections.  If any files could not be copied, it re-awakes after RECHECK_INTERVAL to try again.
# Because the script creates all required destination directories, it is recommended that mount points are made non-writeable so that non-mounted drives are detected rather than destination directories being created at, and data being copied to, the mount point, which may fill up a system drive.

# For each priority, the files to copy are sorted by decreasing age as indicated by ctime, making sure that all files with the latest detected ctime have been accounted for.  Before copying a file, the amount of free space is checked to make sure it is likely to fit (unless in FTP mode), and further copying from this source directory is abandoned (until retry) if it doesn't: this avoids wasting time repeatedly copying large files that won't succeed.  When copying of the last file with a particular ctime in a particular source directory is completed, this ctime is written to the configuration file, and any files older than this are subsequently ignored.  This allows files to be removed from the destination without them being copied again if they remain on the source.  (ctime is used rather than mtime, because it indicates the time the source file was made visible by moving it into the source directory, therefore making it impossible for older files, which would never be copied, to appear subsequently.)
# There must not be more than one destination path for any source path (or the most recent destination path will apply).  When a path pair is supplied, an existing pair with the same source at another priority will be removed.  There is no way to remove path pairs automatically - these must be removed from PATHS_FILE while the script is not running.

# PATHS_FILE is normally automatically handled but for completeness its format is explained here.  It contains fields separated by newlines.  Each group of fields consists of a priority value (indicated by having a leading space), followed by one or more triplets of source directory, destination directory and ctime value.  Any files in the corresponding source directory with a ctime of this value or less will not be copied.  (The -c option causes these values to be set to zero when the file is first read, but updated values will be written to the file even if nothing is copied.)  The exception is the priority 1 section, which contains the extra directory name immediately after the priority value (may be blank) and an extra ctime value for this extra directory, after each normal ctime value.

# If option -e or -g is supplied, the path specified with it forms an additional destination for all files at priority '1'.  This is intended for use with a portable drive onto which to copy offline rushes (and operates in the same way whether using FTP mode or not).  With option -e, each source file will be copied to the extra destination first; with option -g, they are all copied to the extra destination first. 

# As source directories are scanned, source files are deleted if they have been successfully copied (indicated by existing at the destination with the same size, or having a ctime older than the value stored in PATHS_FILE) their mtimes are more than KEEP_PERIOD old, and the source directory is at least DELETE_THRESHOLD percent full.

# The script also runs a very simple HTTP server.  This is configured to return JSON-formatted data at http://<host>:<MON_PORT>/ (status data) or .../availability (a "ping").

# NB This script will only work properly if the source directories are on a filing system that supports ctime.

use strict;
use IO::Socket;
use IO::Select;
use IO::File;
use Getopt::Std;
our $VERSION = '$Revision: 1.19 $'; #used by Getopt in the case of --version or --help
$VERSION =~ s/\s*\$Revision:\s*//;
$VERSION =~ s/\s*\$\s*$//;
$Getopt::Std::STANDARD_HELP_VERSION = 1; #so it stops after version message
use IPC::ShareLite qw( :lock); #don't use Shareable because it doesn't work properly with nested hashes, not cleaning up shared memory or semaphores after itself.  This may need to be installed from CPAN
#the qw( :lock) imports LOCK_EX which shouldn't be needed as an argument to lock() but causes a warning if it isn't supplied
use Storable qw(freeze thaw); #to allow sharing of references
use Filesys::DfPortable; #this may need to be installed from CPAN
use Term::ANSIColor;
#use Data::Dumper;

#things you might want to change
use constant CLIENT_PORT => 2000;
use constant MON_PORT => 7010;
use constant DEST_INCOMING => 'incoming'; #incoming file subdirectory - note that this must match the value in makebrowsed.pl to ensure that directories for incoming files are ignored
use constant KEEP_PERIOD => 24 * 60 * 60 * 7; #a week (in seconds)
use constant DELETE_THRESHOLD => 90; #disk must be at least this full (%) for deletions to start
use constant PERMISSIONS => '0775'; #for directory creation
#use constant COPY => '/home/ingex/bin/cpfs'; #for copying at different speeds
use constant COPY => './cpfs'; #for copying at different speeds
use constant SLOW_LIMIT => 10000;  #bandwidth limit kByte/s as supplied to COPY
use constant FAST_LIMIT => 50000; #bandwidth limit kByte/s as supplied to COPY (0 = unlimited)
use constant RECHECK_INTERVAL => 10; #time (seconds) between rechecks if something failed
use constant PATHS_FILE => 'paths'; #name of file the script generates to save copying details for automatic resumption of copying after restarting
use constant SUBDIRS => qw(xml); #list of subdirectories of each source directory to scan if found (can be empty); equivalent subdirectories are generated at the destination
use constant EXTNS => qw(mxf mov xml mpg); #list of extensions of files to copy; they will be matched case-insensitively and should be in lower case here

use constant FTP_ROOT => '';
use constant FTP_SPEED => '20'; #Mbits/sec

#for colour codes see Term::ANSIColor documentation at http://search.cpan.org/~rra/ANSIColor-3.00/ANSIColor.pm
use constant WARNING_COLOUR => 'red'; #for messages reporting warnings
use constant MAIN_COPY_COLOUR => 'green'; #for messages reporting copying progress to main destination
use constant EXTRA_COPY_COLOUR => 'blue'; #for messages reporting copying progress to extra destination
use constant CLIENT_COLOUR => 'cyan'; #for messages reporting client connections
use constant RETRY_COLOUR => 'red'; #for the retrying message (which sits on the screen for some time)
use constant ABORT_COLOUR => 'on_bright_red'; #for the aborting message (which may sit on the screen for some time)
use constant TC_COLOUR => 'on_yellow'; #for traffic control on/off messages

#things you shouldn't change
use constant UP_TO_DATE => 0;
use constant PROCESSING => 1;
use constant STALE => 2;
use vars qw($childPid %opts @curl $interface @ftpDetails $recSock $monSock $pathsFile $byAge %extns $url);

foreach ((EXTNS)) {
	$extns{$_} = 1;
}

sub HELP_MESSAGE { #given this name, getopts will call this subroutine if given "--help" as a command-line option.
 print STDERR "Usage: xferserver.pl [-a <path>] [-c] [-d] [-e <path>] [-f <server>] [-f '<server> <username> <password>'] [-g <path>] [-m] [-p] [-r] [-s] [-t] [-v]
 -a  optional location of paths file
 -c  clear stored ctime values (re-copy anything not found on server)
 -d  do not make and copy to 8-digit subdirectories
 -e <path>  extra destination for priority 1 files
 -f '<server>[ <username> <password>]'  use ftp rather than copying (apart from to extra destination)
 -g <path>  as -e but prioritises copying to extra destination
 -m  monochrome (do not colour output to terminal)
 -p  preserve files (do not delete anything)
 -r  reset (remove stored information)
 -s  start in slow (traffic-limited) mode - NB will switch to fast as soon as ANY client indicates the end of an event; ignored if option -t is specified
 -t  do not use traffic control
 -v  verbose: print out every source file name with reasons for not copying, and put curl into verbose mode for ftp transfers\n";
  exit 1;
}

HELP_MESSAGE() unless getopts('a:cde:f:g:mprstv', \%opts)
 && !scalar @ARGV #no other args
 && (!defined $opts{e} || (!defined $opts{g} && $opts{e} ne '' && $opts{e} !~ /^-/)) #-e correct if supplied, and -g not supplied as well
 && (!defined $opts{f} || ($opts{f} !~ /^-/ && (@ftpDetails = split/\s+/, $opts{f}) && (1 == scalar @ftpDetails || 3 == scalar @ftpDetails))) #-f correct if supplied
 && (!defined $opts{g} || ($opts{g} ne '' && $opts{g} !~ /^-/)); #-g correct if supplied

$pathsFile = PATHS_FILE;
$pathsFile = $opts{a} if $opts{a};
if ($opts{g}) {
	$byAge = sub {
	  $b->{extra} <=> $a->{extra} || $a->{ctime} <=> $b->{ctime}; #priority given to extra destination (otherwise sort by ctime)
	};
	$opts{e} = $opts{g};
}
else {
	$byAge = sub {
	  $a->{ctime} <=> $b->{ctime} || $b->{extra} <=> $a->{extra}; #priority given to ctime (otherwise put extra destination first)
	};
}

my %transfers;
$transfers{limit} = $opts{t} ? 0 : ($opts{'s'} |= 0); #quotes stop highlighting getting confused in kate!
$transfers{date} = 0; #need this to keep date value in sync when calling Report() from different threads
$transfers{startOfLine} = 1; #to prevent Report() printing a newline before the first message
if ($opts{f}) {
	FtpTrafficControl(0); #in case it's already on, which would result in an abort when we try to switch it on
	if ($transfers{limit}) {
		my $err = FtpTrafficControl(1);
		Abort("Failed to start FTP traffic control: $err") if $err ne '';
		Report("TRAFFIC CONTROL ON\n", 1, \%transfers, TC_COLOUR, 1);
	}
	#generate curl common options
	@curl = ('curl', '--proxy', '');
	push @curl, '--user', "$ftpDetails[1]:$ftpDetails[2]" if 3 == scalar @ftpDetails; #non-anonymous
	if (system "curl --help | grep 'keepalive-time' > /dev/null") { #not found
		Report("WARNING: curl doesn't support '--keepalive-time' option so long transfers may fail, depending on the characteristics of the network\n", 1, \%transfers, WARNING_COLOUR, 1);
	}
	else {
		push @curl, qw(--keepalive-time 600);
	}
	push @curl, '-v' if $opts{v};
	#generate URL stem for curl
	$url = "ftp://$ftpDetails[0]" . (FTP_ROOT ne '' ? '/' . FTP_ROOT : '');
}
$transfers{pid} = 0;

my $share = IPC::ShareLite->new( #use a my variable and pass it as a parameter or the signal is not
	-create => 'yes',
	-destroy => 'yes',
	-size => 10240 #default size of 65k is a bit excessive for this app!
) or die $!;

#trap signals to ensure copying process is killed and shared memory segment is released
my $terminate;
$SIG{INT} = \&Abort;
$SIG{TERM} = \&Abort;
my $parentPID = $$; #so child can kill parent
#start the copying thread - it won't do anything until kicked, so don't need to worry about locking the share yet
if (!defined ($childPid = open CHILD, '|-')) {
	die "failed to fork: $!\n";
}
elsif ($childPid == 0) { # child - copying
	&childLoop($share);
	kill "TERM", $parentPID;
	exit 1;
}
CHILD->autoflush(1);

#load the configuration

my $priority = 1;
$share->lock(LOCK_EX); #calling addPair() may kick the child thread so need to ensure exclusive access
if ($opts{r}) {
	unlink $pathsFile;
}
elsif (-e $pathsFile) {
	if (-s $pathsFile) {
		if (open FILE, $pathsFile) {
			Report("Reading from '$pathsFile'...\n", 1, \%transfers, '', 1);
			my @config = <FILE>;
			close FILE;
			chomp @config;
			#be strict in reading the file because mistakes could lead to data loss (wrong ctimeCleared causing files which haven't been copied to be deleted)
			my $ok = 1;
			while ($ok && @config) {
				my $priority = shift @config;
				unless ($priority =~ s/^ (\d+)$/$1/ && $priority > 0) {
					Report("WARNING: invalid priority value: '$priority'\n", 1, \%transfers, WARNING_COLOUR, 1);
					last;
				}
				my $extraDestMismatch = 0;
				if (1 == $priority) {
					if (@config) {
						if (exists $transfers{extraDest}) {
							Report("WARNING: more than one priority 1 entry\n", 1, \%transfers, WARNING_COLOUR, 1);
							last;
						}
						elsif ($opts{e} && $opts{e} ne $config[0]) {
							Report("WARNING: Extra destination '$config[0]' does not match extra destination '$opts{e}' supplied: ctimeCleared values will be reset\n", 1, \%transfers, WARNING_COLOUR, 1);
							$extraDestMismatch = 1;
						}
						$transfers{extraDest} = $opts{e} |= '';
						shift @config;
					}
					else {
						Report("WARNING: No extra destination supplied\n", 1, \%transfers, WARNING_COLOUR, 1);
						last;
					}
				}
				while ($ok && @config && $config[0] !~ /^ /) {
					my $sourceDir;
					$ok = addPair(\%transfers, \@config, $priority, $extraDestMismatch, $opts{c} |= 0); #get ctimeCleared from @config (unless clearing stored values)
				}
			}
			Report("Ignoring rest of file\n", 1, \%transfers, WARNING_COLOUR, 1) if @config;
			if (!exists $transfers{transfers}) {
				Report("WARNING: No valid transfer pairs found\n\n", 1, \%transfers, WARNING_COLOUR, 1);
			}
		}
		else {
			Report("WARNING: Couldn't read from '$pathsFile': $!\n", 1, \%transfers, WARNING_COLOUR, 1);
		}
	}
	else {
		Report("Empty '$pathsFile' file.\n\n", 1, \%transfers, WARNING_COLOUR, 1);
	}
}
else {
	Report("No stored paths file '$pathsFile' found.  (Assuming we haven't been told to copy anything yet.)\n\n", 1, \%transfers, WARNING_COLOUR, 1);
}

my $ports = new IO::Select;

#open the socket for instructions from recorders via xferclient.pl
$recSock = new IO::Socket::INET(
 	LocalPort => CLIENT_PORT,
# 	Type => SOCK_STREAM,
 	Reuse => 1,
 	Listen => 100,
 	) or Abort('Could not create server socket at port ' . CLIENT_PORT . ": $!");
$ports->add($recSock);
Report("Listening for copying clients on port " . CLIENT_PORT . ".\n", 1, \%transfers);

#open the socket for status requests from web browser-type things
$monSock = new IO::Socket::INET(
 	LocalPort => MON_PORT,
# 	Type => SOCK_STREAM,
 	Reuse => 1,
 	Listen => 100,
 	) or Abort('Could not create server socket at port ' . MON_PORT . ": $!");
$ports->add($monSock);
Report("Listening for status requests at http://localhost:" . MON_PORT . "/.\n\n", 1, \%transfers);

$share->store(freeze \%transfers);
$share->unlock;

my %events;

#main loop
while (1) {
	my @requests = $ports->can_read;
	foreach (@requests) {
 		my $client = $_->accept();
		if ($_ == $recSock) {
			&serveClient($client, $share);
		}
		else {
			&serveStatus($client, $share);
		}
		$client->shutdown(2); # stopped using it - this seems to prevent partial status pages being served (when close() is used instead)
	}
}

# handle commands from a client, modifying the transfers hash and controlling traffic
sub serveClient {
 my ($client, $share) = @_;
 my ($buf, $data);
 #read from client with timeout (otherwise one client connection can block communication from another)
 if (!defined eval {
	local $SIG{ALRM} = sub { die };
	alarm 2; #time in seconds before a SIGALRM is generated; less than 2 may be dangerous as it may be rounded down
	while (sysread($client, $buf, 100)) { # don't use buffered read or can hang if several strings arrive at once
		$data .= $buf;
	}
	alarm 0;
 }) {
	Report("WARNING: Client left socket open too long: ignoring.\n", 0, $share, WARNING_COLOUR);
	return;
 }
 #interpret data
 if (!defined $data) {
	Report("WARNING: Client connection made but no parameters passed: ignoring.\n", 0, $share, WARNING_COLOUR);
	return;
 }
 my @commands = split /\n/, $data;
 $client = shift @commands;
 my $identifier = shift @commands;
 $share->lock(LOCK_EX);
 my $transfers = thaw($share->fetch);
 if (@commands || !defined $identifier) { # event end, from recorder or player
	if (!defined $identifier || $identifier eq '0') { #all events
		Report("Client '$client' is clearing all events.\n", 1, $transfers, CLIENT_COLOUR);
		delete $events{$client};
	}
	else {
		Report("Client '$client' has finished an event (identifier '$identifier').\n", 1, $transfers, CLIENT_COLOUR);
		delete $events{$client}{$identifier};
		if (!keys %{ $events{$client} }) {
			delete $events{$client};
		}
	}
	my $added = 0;
	if (@commands) {
		Report("Client '$client' says copy:\n", 1, $transfers, CLIENT_COLOUR);
		do {
			$added = 1 if 2 == addPair($transfers, \@commands, shift @commands, 0, 2); #printing during this will not be interspersed with printing from the thread as the share is locked
		} while (@commands);
	}
	if (!keys %events && $transfers->{limit}) {
		$transfers->{limit} = 0;
		kill 'USR2', $transfers->{pid} if $transfers->{pid};
		if ($opts{f}) {
			my $err = FtpTrafficControl(0);
			Report("WARNING: failed to remove FTP traffic control: $err", 1, $transfers, WARNING_COLOUR) if $err ne '';
		}
		Report("TRAFFIC CONTROL OFF\n", 1, $transfers, TC_COLOUR, 1);
	}
	if ($added) {
		SaveConfig($transfers, $pathsFile); #so that if the script is re-started it can carry on copying immediately
	}
 }
 else { #event start
	$events{$client}{$identifier} = 1;
	Report("Client '$client' has started an event (identifier '$identifier').\n", 1, $transfers, CLIENT_COLOUR);
	if (!$transfers->{limit} && !$opts{t}) { #not already controlling traffic
		if ($opts{f}) {
			my $err = FtpTrafficControl(1);
			Abort("Failed to start FTP traffic control: $err") if $err ne '';
		}
		$transfers->{limit} = 1;
		kill 'USR1', $transfers->{pid} if $transfers->{pid};
		Report("TRAFFIC CONTROL ON\n", 1, $transfers, TC_COLOUR, 1);
	}
 }
 $share->store(freeze $transfers);
 $share->unlock;
}

# return HTTP status data
sub serveStatus {
 my ($client, $share) = @_;
 my ($buf, $req);
 while (sysread($client, $buf, 1) && $buf ne "\r") { #don't use buffered read or can hang if several strings arrive at once
	$req .= $buf; #build up the first line
 }
 unless ($buf eq "\r") {
 	print "\rWARNING: Status client provided incomplete request: ignoring.\n";
 	return;
 }
 unless ($req =~ m|^GET /(.*) HTTP/1.[01]|) {
 	print "\rWARNING: Status client sent bad HTTP request '$req': ignoring.\n";
	print $client "HTTP/1.0 400 BAD REQUEST\nContent-Type: text/plain\n\n";
	print $client "BAD REQUEST\n";
	return;
 }
 if (uc $1 eq 'AVAILABILITY') {
	print $client "HTTP/1.0 200 OK\nContent-Type: text/plain\n\n";
	print $client "{\"availability\" : true}\n";
 }
 elsif ($1 eq '') { #the data
	print $client "HTTP/1.0 200 OK\nContent-Type: text/plain\n\n";
	print $client "{\n";
	print $client "\t\"state\" : \"info\",\n"; # TODO expand this with "ok", "warn" or "error"
	print $client "\t\"monitorData\" : {\n";

	#don't use free-form strings as names in case they contain incompatible characters
	print $client "\t\t\"recordings\" : [\n";
	if (keys %events) {
		my @output;
		foreach (sort keys %events) { #there will always be at least one pair for each priority
			push @output, "\t\t\t{ \"name\" : " . E($_) . ', "number" : ' . keys(%{ $events{$_} }) . ' }';
		}
		print $client join(",\n", @output), "\n";
	}
	print $client "\t\t],\n";

	print $client "\t\t\"endpoints\" : {\n";
	my $transfers = thaw($share->fetch); #fetch is atomic so no need to lock as not changing $transfers
	if (keys %{ $transfers->{transfers} }) {
		my @priorities;
		foreach my $priority (sort keys %{ $transfers->{transfers} }) {
			my %pairs = %{ $transfers->{transfers}{$priority} };
			my @output;
			foreach (sort keys %pairs) { #there will always be at least one pair for each priority
				push @output, "\t\t\t\t{ \"source\" : " . E($_) . ', "destination" : ' . E($pairs{$_}{dest}) . ' }';
			}
			push @priorities, "\t\t\t\"$priority\" : [\n" . (join ",\n", @output) . "\n\t\t\t]";
		}
		print $client join(",\n", @priorities), "\n";
	}
	print $client "\t\t},\n";

	print $client "\t\t\"current\" : {\n";
	if ($transfers->{current}) { #at least one copy operation has started
		my @output;
		push @output, "\t\t\t\"priority\" : $transfers->{current}{priority}";
		push @output, "\t\t\t\"totalNormalFiles\" : $transfers->{current}{totalNormalFiles}";
		push @output, "\t\t\t\"totalNormalSize\" : $transfers->{current}{totalNormalSize}";
		push @output, "\t\t\t\"totalExtraFiles\" : $transfers->{current}{totalExtraFiles}";
		push @output, "\t\t\t\"totalExtraSize\" : $transfers->{current}{totalExtraSize}";
		if ($transfers->{current}{fileName}) { #at least one copy operation has started
			push @output, "\t\t\t\"fileName\" : " . E($transfers->{current}{fileName});
			push @output, "\t\t\t\"fileSize\" : $transfers->{current}{fileSize}";
		}
		print $client join(",\n", @output), "\n";
	}
	print $client "\t\t},\n";

	print $client "\t\t\"progress\" : {\n";
		if ($transfers->{current}{fileName}) { #at least one copy operation has started
			print $client "\t\t\t\"fileProgress\" : $transfers->{current}{fileProgress}\n";
		}
	print $client "\t\t}\n";

	print $client "\t}\n";
	print $client "}\n";
 }
 else {
 	print "\rWARNING: Status client asked for unknown page '$1': ignoring.\n";
	print $client "HTTP/1.0 404 FILE NOT FOUND\n";
	print $client "Content-Type: text/plain\n\n";
	print $client "Unknown page\n";
 }
}

#escape backslash and quotes
sub E {
	my $string = shift;
	$string =~ s/\\/\\\\/g;
	$string =~ s/"/\\"/g;
	return "\"$string\"";
}

#save priorites, pairs and copy progress in a file for later resumption
#assumes share is locked and should be stored afterwards
sub SaveConfig { #should be locked before doing this to prevent concurrent file writes
	my $transfers = shift;
 	my $config = IO::File->new('>' . $pathsFile);
 	if ($config) {
 		foreach my $priority (sort keys %{ $transfers->{transfers} }) {
			print $config " $priority\n"; #space indicates priority value
			print $config "$transfers->{extraDest}\n" if 1 == $priority;
			my %sources = %{ $transfers->{transfers}{$priority} };
			foreach (sort keys %sources) {
				print $config "$_\n$sources{$_}{dest}\n$sources{$_}{normalCtimeCleared}\n";
				print $config "$sources{$_}{extraCtimeCleared}\n" if 1 == $priority;
			}
 		}
 		$config->close;
 	}
 	else {
 		Report("WARNING: couldn't open '$pathsFile' for writing: $!.\nTransfers will not recommence automatically if script is restarted.\n", 1, $transfers, WARNING_COLOUR);
 	}
}

# add a source/dest directory pair to the transfers
# if $ctimeMode = 0 or 1, read ctimeCleared (and extraCtimeCleared if priority is 1) from @$args and if 1, clear it/them; if 2, read from existing pair or clear if not present
# @$args can be large and only the needed values will be shifted off it
# return 2 if OK, 1 if problem with dirs and 0 if problem with args
# assumes share is locked
sub addPair {
 #check args
 my ($transfers, $args, $priority, $clearExtraCtime, $ctimeMode) = @_;
 my $source = shift @$args;
 my $dest = shift @$args;
 unless ($priority =~ /^\d+$/) {
	Report("\rWARNING: priority argument '$priority' is invalid\n", 1, $transfers, WARNING_COLOUR);
	return 0;
 }
 unless (defined $source) {
	Report("\rWARNING: source directory argument missing\n", 1, $transfers, WARNING_COLOUR);
	return 0;
 }
 $source =~ s|/*$||;
 unless (defined $dest) {
	Report("\rWARNING: destination directory argument missing\n", 1, $transfers, WARNING_COLOUR);
	return 0;
 }
 $dest =~ s|/*$||;
 Report("\rPriority $priority: '$source/*' -> '$dest/*'\n", 1, $transfers, CLIENT_COLOUR);
 my ($ctimeCleared, $extraCtimeCleared);
 unless (2 == $ctimeMode) {
	$ctimeCleared = shift @$args;
	unless (defined $ctimeCleared) {
		Report("\rWARNING: no ctimeCleared value: ignoring this pair\n", 1, $transfers, WARNING_COLOUR);
		return 0;
	}
	unless ($ctimeCleared =~ /^\d+$/) {
		Report("\rWARNING: invalid ctimeCleared value '$ctimeCleared': ignoring this pair\n", 1, $transfers, WARNING_COLOUR);
		return 0;
	}
	if (1 == $priority) {
		$extraCtimeCleared = shift @$args;
		unless (defined $extraCtimeCleared) {
			Report("\rWARNING: no extraCtimeCleared value: ignoring this pair\n", 1, $transfers, WARNING_COLOUR);
			return 0;
		}
		unless ($extraCtimeCleared =~ /^\d+$/) {
			Report("\rWARNING: invalid extraCtimeCleared value '$extraCtimeCleared': ignoring this pair\n", 1, $transfers, WARNING_COLOUR);
			return 0;
		}
	}
 }
 $ctimeCleared = 0 if 1 == $ctimeMode;
 $extraCtimeCleared = 0 if 1 == $ctimeMode || $clearExtraCtime;
 if (!-d $source) {
	Report("\rWARNING: source directory '$source/' doesn't exist: ignoring\n", 1, $transfers, WARNING_COLOUR);
	return 1;
 }
 Report("Also copying to extra destination '$opts{e}/'\n", 1, $transfers, CLIENT_COLOUR) if $opts{e};
 #store details
 foreach (keys %{$transfers->{transfers}}) {
	if (exists $transfers->{transfers}{$_}{$source}) {
		$ctimeCleared = $transfers->{transfers}{$_}{$source}{normalCtimeCleared} unless defined $ctimeCleared; #if this is a different priority then the ctimeCleared value is still valid
		$extraCtimeCleared = $transfers->{transfers}{$_}{$source}{extraCtimeCleared} if exists $transfers->{transfers}{$_}{$source}{extraCtimeCleared} && !defined $extraCtimeCleared; #if this is a different priority then the ctimeCleared value is still valid
		delete $transfers->{transfers}{$_}{$source}; #may be putting this straight back again!
		last; #shouldn't be more than one!
	}
 }
 $transfers->{transfers}{$priority}{$source}{dest} = $dest; #using $source as a hash key prevents duplication of sources
 $transfers->{extraDest} = ($opts{e} ? $opts{e} : '') if 1 == $priority;
 $transfers->{transfers}{$priority}{$source}{stale} = STALE;
 if (defined $ctimeCleared) {
 	$transfers->{transfers}{$priority}{$source}{normalCtimeCleared} = $ctimeCleared;
 	$transfers->{transfers}{$priority}{$source}{extraCtimeCleared} = $extraCtimeCleared if 1 == $priority;
 }
 else {
 	$transfers->{transfers}{$priority}{$source}{normalCtimeCleared} = 0; #copy everything from the epoch onwards
 	$transfers->{transfers}{$priority}{$source}{extraCtimeCleared} = 0 if 1 == $priority;
 }
 #tell the copying thread
 if (!exists $transfers->{staleFrom}) { #not pending rescan
	print CHILD "\n"; #will kick the child into action if it's idle
 }
 if (!exists $transfers->{staleFrom} || $transfers->{staleFrom} > $priority) { #not pending rescan or re-scanning from a lower priority than this pair
	$transfers->{staleFrom} = $priority; #rescan from this priority at the end of the current copy
 }
 return 2;
}

# scan a source/destination directory pair, make a list of files that need to be copied, and delete unneeded source files
# $srcRoot: root of source directory to scan
# $subDir: subdir of source directory to scan, if defined
# $normalDestRoot: path of main destination directory (excluding any subdirectories, if used)
# $ctimeCleared: stage copying has reached in increasing order of ctime value
# $share: shared memory object
# $extraDestRoot, $extraCtimeCleared: optional equivalents for $normalDestRoot and $ctimeCleared if copying to extra destination
# $ftpFileList: optional reference to hash cache of details of files on FTP server

sub ScanDir {
 my ($srcRoot, $subDir, $normalDestRoot, $normalCtimeCleared, $share, $extraDestRoot, $extraCtimeCleared, $ftpFileList) = @_;
 my ($normalOK, $extraOK) = (0, 0);
 my ($totalSrcFiles, $latestCtime, $totalNormalToCopy, $totalExtraToCopy, $totalNormalSize, $totalExtraSize) = (0, 0, 0, 0, 0, 0);
 my (@normalFiles, @extraFiles);
 my ($normalDestPath, $extraDestPath);
 my $srcPath = defined $subDir ? "$srcRoot/$subDir" : $srcRoot;
 if (opendir(SRC, $srcPath)) {
	($normalOK, $extraOK) = (1, defined $extraDestRoot);
	my $ftpOK = 1;
	my $prevLatestCtime = 0;
	my $loop = 0;
	do { #loop to make sure we get all the files with the latest ctime we find
		my $scanTime = time;
		my @fileNames = sort readdir SRC; #sorting useful for verbose option
		while (@fileNames && ($normalOK || $extraOK)) {
			my $srcName = shift @fileNames;
			my $fullSource = "$srcPath/$srcName";
			#check it's not a directory
			if (-d $fullSource) {
				Report("d  $srcName\n", 0, $share) if $opts{v} && $srcName !~ /^\.\.?$/;
				next;
			}
			#check it's a recognised filename and generate temp dest path
			if (
			 $srcName =~ /\.([^.]+)$/ && exists $extns{"\L$1\E"} #recognised extension
			 && ($opts{d} || $srcName =~ /^(\d{8})_/) #date in filename if using date subdirectories
			) {
				$normalDestPath = $normalDestRoot;
				$normalDestPath .= "/$1" unless $opts{d}; #add date subdirectory if being used
				$normalDestPath .= "/$subDir" if defined $subDir;
				if (defined $extraDestRoot) {
					$extraDestPath = $extraDestRoot;
					$extraDestPath .= "/$1" unless $opts{d}; #add date subdirectory if being used
					$extraDestPath .= "/$subDir" if defined $subDir;
				}
			}
			else {
				Report("-  $srcName\n", 0, $share) if $opts{v} && $srcName !~ /^\.\.?$/;
				next;
			}
			$totalSrcFiles++;
			#get info about the source file
			#Use ctime to test for files to be ignored that have been copied already.
			#This is because ctime is updated when the completed files are atomically moved to the source directories,
			#so we can guarantee that files with an earlier ctime won't appear after we've copied everything up to a certain ctime,
			#which could otherwise result in files not being copied.
			#Use mtime to test for deleting old files, because age of file since creation/modification is what is important.
			my ($dev, $size, $mtime, $ctime) = (stat $fullSource)[0, 7, 9, 10];
			unless ($dev) { #if lstat fails then it returns a null list
				Report("WARNING: failed to stat '$fullSource': $!: abandoning this directory\n", 0, $share, WARNING_COLOUR);
				$normalOK = 0;
				$extraOK = 0;
				last;
			}
			unless ($size) {
				Report("WARNING: '$fullSource' has zero length: ignoring\n", 0, $share, WARNING_COLOUR); #if the file later acquires length, its ctime will be updated, so useful content won't be deleted without copying
				next;
			}
			$latestCtime = $ctime if $ctime > $latestCtime;
			#check for existing copies and generate copy details
			(my $altName = $srcName) =~ s/(.*)(\.[^.]+)/$1_dup$2/; #will always succeed due to check above
			my $copied = $ctime <= $normalCtimeCleared; #already copied if ctime indicates it
			my @checkDest;
			if ($ftpOK) {
				@checkDest = CheckDest($srcPath, $normalDestPath, $srcName, $size, $altName, $share, $ftpFileList); #even if this dir has been abandoned, still check for a copy to see if we can delete the source file
				$ftpOK = $checkDest[0]; #will always be 1 if not using FTP
			}
			else {
				#don't make repeated FTP attempts as it can get very slow...
				@checkDest = (0, 0, 0);
			}
			$normalOK &= $checkDest[1];
			Report(($copied ? 'c' : ' ') . ($checkDest[2] ? 's' : ' ') . " $srcName\n", 0, $share) if $opts{v};
			$copied |= $checkDest[2];
			if ($normalOK & !$copied) {
				push @normalFiles, {
					name => $srcName,
					srcRoot => $srcRoot,
					srcDir => $srcPath,
					destRoot => $normalDestRoot,
					destDir => $normalDestPath,
					mtime => $mtime,
					ctime => $ctime,
					size => $size,
					extra => 0,
				};
				$totalNormalToCopy++;
				$totalNormalSize += $size;
			}
			my $extraCopied = 1; #default if no extra destination
			if (defined $extraDestPath) {
				$extraCopied = $ctime <= $extraCtimeCleared; #already copied if ctime indicates it
				@checkDest = CheckDest($srcPath, $extraDestPath, $srcName, $size, $altName, $share); #never use ftp; even if this dir has been abandoned, still check for a copy to see if we can delete the source file
				$extraOK &= $checkDest[1];
				Report(($extraCopied ? 'c' : ' ') . ($checkDest[2] ? 's' : ' ') . " $srcName (extra)\n", 0, $share) if $opts{v};
				$extraCopied |= $checkDest[2];
				if ($extraOK & !$extraCopied) {
					push @extraFiles, {
						name => $srcName,
						srcRoot => $srcRoot,
						srcDir => $srcPath,
						destRoot => $extraDestRoot,
						destDir => $extraDestPath,
						mtime => $mtime,
						ctime => $ctime,
						size => $size,
						extra => 1,
					};
					$totalExtraToCopy++;
					$totalExtraSize += $size;
				}
			}
			#delete source file if redundant
			if ($copied
			&& $extraCopied
			&& time > $mtime + KEEP_PERIOD
			&& dfportable($srcPath)->{per} >= DELETE_THRESHOLD) {
				if ($opts{p}) {
					Report("Would delete old file '$fullSource' if not prevented by command-line option, as aleady copied and disk is " . dfportable($srcPath)->{per} . "% full.\n", 0, $share);
				}
				else {
					Report("Removing old file '$fullSource', as already copied and disk is " . dfportable($srcPath)->{per} . "% full.\n", 0, $share);
					unlink $fullSource or Report("WARNING: could not remove '$fullSource': $!\n", 0, $share, WARNING_COLOUR);
				}
			}
		}
		$loop = ($normalOK || $extraOK) #we've not bailed out so there is a point in scanning again
		 && $latestCtime >= $scanTime #files have been found with the same or later ctime as when (just before) the scan started, so scan again in case more files have appeared with the same ctime as the latest found
		 && $latestCtime != $prevLatestCtime; #trap to prevent continuous looping if system time has been moved back since recordings were made.  The second's delay before scanning again will ensure that all the files up to $latestCtime will have been captured if it's the same as $prevLatestCtime.
 		if (!$normalOK || $loop) {
 			@normalFiles = ();
			$totalNormalToCopy = 0;
			$totalNormalSize = 0;
		}
		if (!$extraOK || $loop) {
 			@extraFiles = ();
			$totalExtraToCopy = 0;
			$totalExtraSize = 0;
		}
		if ($loop) {
			$totalSrcFiles = 0;
			$prevLatestCtime = $latestCtime;
			$latestCtime = 0;
			rewinddir SRC;
			sleep 1; #delay the retry to prevent a possible deluge of scans
		}
		elsif ($normalOK || $extraOK) {
			if ($totalSrcFiles) {
				Report("Directory contains $totalSrcFiles recognised file" . (1 == $totalSrcFiles ? '' : 's') . ".\n", 0, $share);
				Report("$totalNormalToCopy will be copied ($totalNormalSize bytes) to '$normalDestPath/'.\n", 0, $share) if $normalOK;
				Report("$totalExtraToCopy will be copied ($totalExtraSize bytes) to extra destination '$extraDestPath/'.\n", 0, $share) if $extraOK;
			}
			else {
				Report("Directory contains no recognised files.\n", 0, $share);
			}
 		}
	} while $loop;
	closedir(SRC);
 }
 return $normalOK, $extraOK, $latestCtime, $totalNormalToCopy, $totalExtraToCopy, $totalNormalSize, $totalExtraSize, @normalFiles, @extraFiles; #arrays will be concatenated
}

#Checks to see if a destination file already exists; if so and it's the same size, indicate that it's already copied; if not the same size, try to move it out of the way by renaming
#If $ftpFileList and $opts{f} are defined, uses FTP to get a listing of the destination directory if $ftpFileList does not have a key for $destDir; populates $ftpFileList with the complete directory contents for re-use, to reduce the number of FTP calls needed.
#returns:
# directory readable (will always be 1 unless using FTP),
# file OK (will always be 0 unless directory readable),
# file copied (will always be 0 unless file OK)
sub CheckDest
{
 my ($srcDir, $destDir, $name, $size, $altName, $share, $ftpFileList) = @_;
 my $fullDest = "$destDir/$name";
 my $copied = 0;
 if ($opts{f} && $ftpFileList) {
	my $fullUrl = "$url/$destDir/";
	if (!exists $$ftpFileList{$destDir}) { #haven't got a listing of this directory yet
		#use curl to get a directory listing and create directory tree on server if necessary.
		open LISTING, '-|', @curl,
		 qw(-s -S --stderr -), #disable the progress meter as it puts stuff in the middle of listing lines; redirect error messages to stdout to capture them
		 '--ftp-create-dirs',
		 $fullUrl;
		my @listing = <LISTING>;
		close LISTING;
		if ($?) {
			Report("WARNING: couldn't get listing of '$fullUrl':\n@{listing}Abandoning this destination directory\n", 0, $share, WARNING_COLOUR); #$! doesn't work
			return 0, 0, 0;
		}
		foreach (@listing) {
			next unless (/^-(\S+\s+){4}(\d+).*?(\S+)$/); #ignore directories and messages from stderr; this is compatible with a vsftpd listing
			$$ftpFileList{$destDir}{$3} = $2; #filename -> size
		}
	}
	if (exists $$ftpFileList{$destDir}{$name}) {
		if ($$ftpFileList{$destDir}{$name} == $size) {
			$copied = 1;
		}
		else {
			Report("WARNING: '$fullDest' exists and is a different size ($$ftpFileList{$destDir}{$name}) from the same file in '$srcDir' ($size): may be a filename conflict: renaming to '$altName'.\n", 0, $share, WARNING_COLOUR);
			if (exists $$ftpFileList{$destDir}{$altName} && $$ftpFileList{$destDir}{$altName}) {
				Report("WARNING: '$altName' already exists: abandoning this destination directory\n", 0, $share, WARNING_COLOUR);
				return 1, 0, 0;
			}
			else {
				#try to rename the file
				open RENAME, '-|', @curl,
				 qw(-s -S --stderr -), #disable the progress meter; redirect error messages to stdout to capture them
				 '--quote', "RNFR $name", '--quote', "RNTO $altName", #do this before the transfer command so we don't get a listing in the error message
				 $fullUrl;
				my $msgs = join '', <RENAME>;
				close RENAME;
				if ($?) {
					Report("WARNING: Failed to rename file in '$fullUrl':\n${msgs}Abandoning this destination directory\n", 0, $share, WARNING_COLOUR);
					return 1, 0, 0;
				}
				#now the entry in %$ftpFileList for this file is wrong, but doesn't matter as it won't be looked at again
			}
		}
	}
 }
 elsif (-s $fullDest) {
	if (-s $fullDest == $size) {
		$copied = 1;
	}
	else {
		Report("WARNING: '$fullDest' exists and is a different size (" . (-s $fullDest) . ") from the same file in '$srcDir' ($size): may be a filename conflict: renaming to '$altName'.\n", 0, $share, WARNING_COLOUR);
		if (-s "$destDir/$altName") {
			Report("WARNING: '$altName' already exists: abandoning this destination directory\n", 0, $share, WARNING_COLOUR);
			return 1, 0, 0;
		}
		elsif (!rename $fullDest, "$destDir/$altName") {
			Report("WARNING: Failed to rename file: $!: abandoning this destination directory\n", 0, $share, WARNING_COLOUR);
			return 1, 0, 0;
		}
	}
 }
 return 1, 1, $copied;
}

#does nothing until kicked by writing to its STDIN
#only returns if serious error
sub childLoop {
 my $share = shift;
 $SIG{INT} = sub { exit; };
 $SIG{TERM} = sub { exit; };
 $SIG{USR1} = 'IGNORE';
 $SIG{USR2} = 'IGNORE';
 local $| = 1; #autoflush stdout
 my $retry = 0; #if a problem occurs, this is set to 1 to cause a retry after a pause
 while (1) { #share unlocked at this point
	#wait for main thread or retry interval
	my $rin = "";
	vec($rin, fileno(STDIN), 1) = 1;
	if (!select $rin, undef, $rin, $retry ? RECHECK_INTERVAL : undef) { #timed out (only if retrying)
		Report("Retrying...\n", 0, $share);
		$share->lock(LOCK_EX);
	}
	else { #kicked by the main thread
		$share->lock(LOCK_EX) unless $terminate; #do this now to keep STDIN in sync with staleFrom to avoid unnecessary scans; condition prevents a hang on interrupt
		my $dummy;
		sysread STDIN, $dummy, 1; #take the kick! (not supposed to mix buffered read with select or <STDIN> would work here)
	}
	$retry = 0;
	my $interrupting = 0; #this is set to 1 if the normal sequence of copies needs to be interrupted because the script has been told that a higher priority pair needs to be scanned
	my $transfers = thaw($share->fetch);
	delete $transfers->{staleFrom}; #checking started
	my @priorities = sort keys %{ $transfers->{transfers} };
	my $priority;
	#change all stale source dirs to PROCESSING so we can determine where new files arrive while copying (by status being changed to STALE again by main thread)
	foreach $priority (@priorities) {
		foreach (keys %{ $transfers->{transfers}{$priority} }) {
			$transfers->{transfers}{$priority}{$_}{stale} = PROCESSING unless UP_TO_DATE == $transfers->{transfers}{$priority}{$_}{stale};
		}
	}
	my %incomingDirs; #paths of temporary incoming directories, so they can be deleted on completion
	my %ftpFileList; #file names and sizes on server if using FTP, to avoid repeated FTP calls
	Report("Verbose mode. c: ctime cleared; s: on server; d: directory; -: unrecognised\n", 1, $transfers) if $opts{v};
	PRIORITY: foreach my $priority (@priorities) { #NB share locked at this point
		$share->store(freeze $transfers);
		$share->unlock;
		my %pairs = %{ $transfers->{transfers}{$priority} };
		my @priEssenceFiles;
		my ($priNormalFiles, $priNormalSize, $priExtraFiles, $priExtraSize) = (0, 0, 0, 0);
		#make a list of all the files to copy at this priority
		foreach my $srcPath (sort keys %pairs) {
			next unless PROCESSING == $pairs{$srcPath}{stale}; #no point in scanning if not stale
			#generate an array of files that need to be copied, from this destination and any recognised subdirectories (treated as one list of files)
			my ($pairNormalToCopy, $pairExtraToCopy, $pairNormalSize, $pairExtraSize, $pairLatestCtime) = (0, 0, 0, 0, 0);
			my @pairEssenceFiles;
			my $extra = 1 == $priority && '' ne $transfers->{extraDest}; #true if using extra destination directory for this pair
			my $subdirIndex = 0; #steps through predefined list of possible subdirectories
			Report("Priority $priority: scanning '$srcPath/'...\n", 0, $share);
			my ($normalOK, #list of transfers to normal destination directory was created ok
			  $extraOK, #list of transfers to extra destination directory was created ok
			  $latestCtime, #age of youngest recognised source file found
			  $normalToCopy, #number of files to copy to normal destination
			  $extraToCopy, #number of files to copy to extra destination
			  $normalSize, #number of bytes to copy to normal destination
			  $extraSize, #number of bytes to copy to extra destination
			  @essenceFiles) #array of hashes of transfer details
			 = ScanDir($srcPath, undef, $pairs{$srcPath}{dest}, $pairs{$srcPath}{normalCtimeCleared}, $share, $extra ? $transfers->{extraDest} : undef, $pairs{$srcPath}{extraCtimeCleared}, \%ftpFileList); #the main directory
			while (1) { #subdirectory loop
				#check previous scan
				unless ($normalOK || ($extra && $extraOK)) {
					$retry = 1;
					last; #abandon whole pair as common ctime value for main and subdirectories
				}
				#update aggregate values
				$pairLatestCtime = $latestCtime if $latestCtime > $pairLatestCtime;
				$pairNormalToCopy += $normalToCopy;
				$pairExtraToCopy += $extraToCopy;
				$pairNormalSize += $normalSize;
				$pairExtraSize += $extraSize;
				push @pairEssenceFiles, @essenceFiles;
				#find a/next subdirectory to scan
				while (SUBDIRS && defined ((SUBDIRS)[$subdirIndex]) && !-d "$srcPath/" . (SUBDIRS)[$subdirIndex]) {
					$subdirIndex++; #skip subdirectory that doesn't exist
				}
				last unless SUBDIRS && defined ((SUBDIRS)[$subdirIndex]);
				Report("Scanning subdir '" . (SUBDIRS)[$subdirIndex] . "/'...\n", 0, $share);
				($normalOK, $extraOK, $latestCtime, $normalToCopy, $extraToCopy, $normalSize, $extraSize, @essenceFiles)
				 = ScanDir($srcPath, (SUBDIRS)[$subdirIndex], $pairs{$srcPath}{dest}, $pairs{$srcPath}{normalCtimeCleared}, $share, $extra ? $transfers->{extraDest} : undef, $pairs{$srcPath}{extraCtimeCleared}, \%ftpFileList) if -d "$srcPath/" . (SUBDIRS)[$subdirIndex];
				$subdirIndex++;
			}
			#update stale status and saved ctimeCleared values if they won't be updated anyway by copying, to prevent scans of up-to-date directories
			my $locked = 0;
			if ($normalOK && ($extraOK || !$extra) && !@pairEssenceFiles) { #up to date
				$share->lock(LOCK_EX);
				$transfers = thaw($share->fetch);
				$locked = 1;
				if (exists $transfers->{transfers}{$priority}{$srcPath} #make sure config hasn't changed
				 && PROCESSING == $transfers->{transfers}{$priority}{$srcPath}{stale}) { #new files haven't appeared since scanning
					$transfers->{transfers}{$priority}{$srcPath}{stale} = UP_TO_DATE;
				}
			}
			my $needToSave = 0;
			if ($normalOK && $latestCtime != $pairs{$srcPath}{normalCtimeCleared} && !$pairNormalToCopy) { #not copying anything from this source to main dest so ctimeCleared won't get updated, but has changed
				#save the new ctimeCleared value
				unless ($locked) {
					$share->lock(LOCK_EX);
					$transfers = thaw($share->fetch);
					$locked = 1;
				}
				if (exists $transfers->{transfers}{$priority}{$srcPath}) { #make sure config hasn't changed
					$transfers->{transfers}{$priority}{$srcPath}{normalCtimeCleared} = $latestCtime;
				}
				$needToSave = 1;
			}
			if ($extraOK && $latestCtime != $pairs{$srcPath}{extraCtimeCleared} && !$pairExtraToCopy) { #not copying anything from this source to extra dest so extraCtimeCleared won't get updated, but has changed
				#save the new ctimeCleared value
				unless ($locked) {
					$share->lock(LOCK_EX);
					$transfers = thaw($share->fetch);
					$locked = 1;
				}
				if (exists $transfers->{transfers}{$priority}{$srcPath}) { #make sure config hasn't changed
					$transfers->{transfers}{$priority}{$srcPath}{extraCtimeCleared} = $latestCtime;
				}
				$needToSave = 1;
			}
			if ($locked) {
				SaveConfig($transfers) if $needToSave;
				$share->store(freeze $transfers);
				$share->unlock;
			}

			#add information to copying details
			$pairs{$srcPath}{normalNFiles} = $pairNormalToCopy; #so we can determine when all files in this directory have been copied
			$pairs{$srcPath}{extraNFiles} = $pairExtraToCopy; #so we can determine when all files in this directory have been copied (if used)
			$pairs{$srcPath}{latestCtime} = $pairLatestCtime; #for updating saved value correctly even if youngest file doesn't need to be copied
			#update global vbls
			$priNormalFiles += $pairs{$srcPath}{normalNFiles};
			$priExtraFiles += $pairs{$srcPath}{extraNFiles};
			$priNormalSize += $pairNormalSize;
			$priExtraSize += $pairExtraSize;
			push @priEssenceFiles, @pairEssenceFiles;
		}

		#update stats
		$share->lock(LOCK_EX);
		$transfers = thaw($share->fetch);
		$transfers->{current}{priority} = $priority;
		$transfers->{current}{totalNormalFiles} = $priNormalFiles;
		$transfers->{current}{totalExtraFiles} = $priExtraFiles;
		$transfers->{current}{totalNormalSize} = $priNormalSize;
		$transfers->{current}{totalExtraSize} = $priExtraSize;
		my $prevFile;

		#copy the files at this priority, oldest first, optionally with extras taking priority
		foreach my $essenceFile (sort $byAge @priEssenceFiles) { #NB share locked at this point
			if (exists $transfers->{staleFrom} && ($transfers->{staleFrom} < $priority || ($transfers->{staleFrom} == 1 && $opts{g}))) {
				#retry as new files of a higher priority may have appeared,
				#or in the case of $opts{g}, must rescan even if copying priority 1 files, because all new priority 1 files must be copied to extra destination first
				$interrupting = 1;
				last PRIORITY;
			}
			my $latestCtimeCopied = $essenceFile->{extra} ? 'extraLatestCtimeCopied' : 'latestCtimeCopied';
			my $ctimeCleared = $essenceFile->{extra} ? 'extraCtimeCleared' : 'normalCtimeCleared';
			if (exists $pairs{$essenceFile->{srcRoot}}{$latestCtimeCopied}) { #have already tried to copy something from this source root
				if (-1 == $pairs{$essenceFile->{srcRoot}}{$latestCtimeCopied}) { #have had a problem with this source root
					next; #ignore this source dir
				}
				elsif ($essenceFile->{ctime} > $pairs{$essenceFile->{srcRoot}}{$latestCtimeCopied}) { #this file is newer than all files so far copied so all the files up to the age of the last one copied inclusive must have been copied
					$pairs{$essenceFile->{srcRoot}}{$ctimeCleared} = $pairs{$essenceFile->{srcRoot}}{$latestCtimeCopied};
					#update the saved configuration so if the script is stopped now it won't re-copy files which have disappeared from the server when restarted
					if (exists $transfers->{transfers}{$priority}{$essenceFile->{srcRoot}}{$ctimeCleared}) { #in case this directory pair has been moved to another priority
						$transfers->{transfers}{$priority}{$essenceFile->{srcRoot}}{$ctimeCleared} = $pairs{$essenceFile->{srcRoot}}{$latestCtimeCopied};
						SaveConfig($transfers);
					}
				}
			}
			my $incomingPath = "$essenceFile->{destDir}/" . DEST_INCOMING;
			if (!$opts{f} || $essenceFile->{extra}) { #when using ftp, the transfer command creates the directory and there is no way to find out how much free space there is
				# check/create copy subdir
				if (!-d $incomingPath && MakeDir($incomingPath)) {
					Report("WARNING: Couldn't create '$incomingPath': $!\n", 1, $transfers, WARNING_COLOUR);
					$pairs{$essenceFile->{srcRoot}}{$latestCtimeCopied} = -1; #prevents any more transfers from this source root/latestCtimeCopied, which would break the ctime order rule
					$retry = 1;
					next;
				}
				# check free space
				if ((my $free = dfportable($essenceFile->{destDir})->{bavail}) < $essenceFile->{size}) { #a crude test but which will catch most instances of insufficient space.  Assumes subdirs are all on the same partition
					Report("Prio $priority: '$essenceFile->{destDir}' full ($free byte" . ($free == 1 ? '' : 's') . " free; file size $essenceFile->{size} bytes).\nAbandoning copying from '$essenceFile->{srcRoot}/' and subdirectories\n", 1, $transfers, WARNING_COLOUR);
					$pairs{$essenceFile->{srcRoot}}{$latestCtimeCopied} = -1; #prevents any more transfers from this source directory/latestCtimeCopied, which would break the ctime order rule
					$retry = 1;
					next;
				}
				$incomingDirs{dir}{$incomingPath} = 1; #so they can be deleted on completion
			}
			else {
				$incomingDirs{ftp}{$essenceFile->{destDir}} = DEST_INCOMING; #so they can be deleted on completion
			}
			#update stats
			$transfers->{current}{fileName} = "$essenceFile->{srcDir}/$essenceFile->{name}";
			$transfers->{current}{fileSize} = $essenceFile->{size};
			$transfers->{current}{fileProgress} = 0; #in case a status request is made now
			# copy the file
			$share->store(freeze $transfers);
			$share->unlock;
			my $childMsgs;
			my $cpPid;
			$? = 0; #prevent suppression of messages from successful copying process if FTP directory listing failed
			if (!$opts{f} || $essenceFile->{extra}) { # non-FTP copy (block limits scope of signal handlers)
				local $SIG{INT} = sub { kill 'INT', $cpPid; };
				local $SIG{TERM} = sub { kill 'TERM', $cpPid; };
				# (doing the above with the USR signals doesn't seem to work properly when using a forking open rather than a fork)
				# run copy program with a forking open so that we can get its stdout
				my $msg = "Prio $priority ";
				if ($essenceFile->{extra}) {
					$msg .= "($transfers->{current}{totalExtraFiles} to go";
					$msg .= ", plus $transfers->{current}{totalNormalFiles} to normal destination" if $transfers->{current}{totalNormalFiles};
				}
				else {
					$msg .= "($transfers->{current}{totalNormalFiles} to go";
					$msg .= ", plus $transfers->{current}{totalExtraFiles} to extra destination" if $transfers->{current}{totalExtraFiles};
				}
				Report("$msg): '$essenceFile->{srcDir}/$essenceFile->{name}' -> '$incomingPath/':\n", 1, $transfers, $essenceFile->{extra} ? EXTRA_COPY_COLOUR : MAIN_COPY_COLOUR);
				if (!defined ($cpPid = open(COPYCHILD, '-|'))) {
					return;
				}
				elsif ($cpPid == 0) { # child
					STDOUT->autoflush(1);
					# exec to maintain PID so we can send signals to the copy program
					exec COPY,
						1, # always start bandwidth-limited because traffic control might be switched on between now and when the program starts, causing it to miss the signal
						FAST_LIMIT, # the bandwidth limit at high speed
						SLOW_LIMIT, # the bandwidth limit at slow speed
						"$essenceFile->{srcDir}/$essenceFile->{name}", #source file
						"$incomingPath/$essenceFile->{name}" #dest file
					or return;
				}
				STDOUT->autoflush(1);
				# note the PID so that the foreground process can signal the copy program
				$share->lock(LOCK_EX);
				$transfers = thaw($share->fetch);
				$transfers->{pid} = $cpPid;
				$share->store(freeze $transfers);
				$share->unlock;
				# get progress from the copy while waiting for it to finish
				my ($buf, $progress, $line);
				my $signalled = 0;
				while (sysread(COPYCHILD, $buf, 100)) { # can't use <> because there aren't any intermediate newlines
					if (!$signalled) { # first time the copy prog has printed anything so it's just come alive
						$signalled = 1; # only need to do this once
						kill 'USR2', $cpPid unless $transfers->{limit}; # switch to fast copying
					}
					$line .= $buf;
					$line =~ s/^(.*?)([\r\n]?)$/$2/s; #remove everything up to any trailing return/newline as we might want to overwrite this line
					Report($1, 0, $share, $essenceFile->{extra} ? EXTRA_COPY_COLOUR : MAIN_COPY_COLOUR, 1) if !$?; #don't print error messages twice
					$childMsgs .= $1;
					$childMsgs =~ s/.*[\r\n]//s; #remove anything before last line
					$progress .= $buf;
					if ($progress =~ /(\d+)%/) { # % complete figure available
						#update stats
						$share->lock(LOCK_EX);
						$transfers = thaw($share->fetch);
						$transfers->{current}{fileProgress} = $1;
						$share->store(freeze $transfers);
						$share->unlock;
						$progress = ''; #assume not more than one will be present
					}
				}
				# prevent signalling (this doesn't suffer from a race condition)
				$share->lock(LOCK_EX);
				$transfers = thaw($share->fetch);
				$transfers->{pid} = 0;
				$share->store(freeze $transfers);
				$share->unlock;
				close COPYCHILD;
			}
			else { #ftp transfer (never done for extra destination)
				local $SIG{INT} = sub { kill 'INT', $cpPid; };
				local $SIG{TERM} = sub { kill 'TERM', $cpPid; };
				my $fullUrl = "$url/$incomingPath/";
				my $msg = "Prio $priority ($transfers->{current}{totalNormalFiles} to go";
				$msg .= ", plus $transfers->{current}{totalExtraFiles} to extra destination" if $transfers->{current}{totalExtraFiles};
				Report("$msg): '$essenceFile->{srcDir}/$essenceFile->{name}' ->  '$fullUrl':\n", 1, $transfers, MAIN_COPY_COLOUR);

				$cpPid = open COPY, '-|', @curl,
				 qw(--stderr - --ftp-create-dirs), #create the incoming directory if necessary
				 '--upload-file', "$essenceFile->{srcDir}/$essenceFile->{name}",
				 '--quote', "-RNFR $essenceFile->{name}", '--quote', "-RNTO ../$essenceFile->{name}", #move out of incoming dir after copying
				 $fullUrl; #do not remove DEST_INCOMING here or whole operation will fail if it can't remove the dir (eg if it's not empty due to another copying process)
				return unless defined $cpPid;
				STDOUT->autoflush(1);
				# get progress from the copy while waiting for it to finish
				my ($buf, $progress, $line);
				while (sysread COPY, $buf, 100) { # can't use <> because there aren't any intermediate newlines
					$line .= $buf;
					$line =~ s/^(.*?)([\r\n]?)$/$2/s; #remove everything up to any trailing return/newline as we might want to overwrite this line
					Report($1, 0, $share, MAIN_COPY_COLOUR, 1) unless $2; #don't print error messages (which have trailing newlines) twice
					$childMsgs .= $1;
					$childMsgs =~ s/.*[\r\n]//s; #remove anything before last line
					$progress .= $buf;
					if ($progress =~ /\r\s*(\d+)\s/s) { # % complete figure available
						#update stats
						$share->lock(LOCK_EX);
						$transfers = thaw($share->fetch);
						$transfers->{current}{fileProgress} = $1;
						$share->store(freeze $transfers);
						$share->unlock;
						$progress = ''; #assume not more than one will be present
					}
				}
				close COPY;
			}
			# check result
			if ($? & 127) { #child has received terminate signal so terminate
				unlink "$incomingPath/$essenceFile->{name}" if !$opts{f} || $essenceFile->{extra};
				exit 1; # TODO: use more exit code values
			}
			if ($? >> 8) { #copy failed
				unlink "$incomingPath/$essenceFile->{name}" if !$opts{f} || $essenceFile->{extra};
				if (141 == $? >> 8 && $opts{f} && !$essenceFile->{extra}) { #141 is the code returned by curl when killed, which happens when "killall xferserver.pl" is done by the ingex termination script.  Without this check, copying will continue.
					#terminate thread
					exit 1;
				}
				$share->lock(LOCK_EX);
				$transfers = thaw($share->fetch);
				Report("WARNING: copy failed, exiting with value " . ($? >> 8) . ": $childMsgs\n", 1, $transfers, WARNING_COLOUR);
				$pairs{$essenceFile->{srcRoot}}{$latestCtimeCopied} = -1; #prevents any more transfers from this source root/latestCtimeCopied, which would break the ctime order rule
				$retry = 1;
				next;
			}
			# move the file from the incoming directory to the live directory
			if (!$opts{f} || $essenceFile->{extra}) { #non-FTP copy
				$childMsgs =~ s/ +$//; #remove the trailing spaces put in by cpfs to hide the "Flushing buffers..." message
				Report("\r$childMsgs. Moving...", 0, $share, $essenceFile->{extra} ? EXTRA_COPY_COLOUR : MAIN_COPY_COLOUR, 1);
				if (!rename "$incomingPath/$essenceFile->{name}", "$essenceFile->{destDir}/$essenceFile->{name}") {
					$share->lock(LOCK_EX);
					$transfers = thaw($share->fetch);
					Report("\nWARNING: Failed to move file to '$essenceFile->{destDir}/': $!\nAbandoning copying from '$essenceFile->{srcRoot}/' and subdirectories\n", 1, $transfers, WARNING_COLOUR);
					$pairs{$essenceFile->{srcRoot}}{$latestCtimeCopied} = -1; #prevents any more transfers from this source root/latestCtimeCopied, which would break the ctime order rule
					$retry = 1;
					next;
				}
			}
			Report(" Done\n", 0, $share, $essenceFile->{extra} ? EXTRA_COPY_COLOUR : MAIN_COPY_COLOUR, 1);
			$pairs{$essenceFile->{srcRoot}}{$latestCtimeCopied} = $essenceFile->{ctime};
			# update stats
			$share->lock(LOCK_EX);
			$transfers = thaw($share->fetch);
			if ($essenceFile->{extra}) {
				$transfers->{current}{totalExtraFiles}--;
				$transfers->{current}{totalExtraSize} -= $essenceFile->{size};
			}
			else {
				$transfers->{current}{totalNormalFiles}--;
				$transfers->{current}{totalNormalSize} -= $essenceFile->{size};
			}
			if (!--$pairs{$essenceFile->{srcRoot}}{$essenceFile->{extra} ? 'extraNFiles' : 'normalNFiles'}) { #this set all copied
				Report("All files from '$essenceFile->{srcRoot}/' and subdirectories successfully copied to '$essenceFile->{destRoot}/' and subdirectories.\n", 1, $transfers, $essenceFile->{extra} ? EXTRA_COPY_COLOUR : MAIN_COPY_COLOUR);
				if (!exists $pairs{$essenceFile->{srcRoot}}{$essenceFile->{extra} ? 'normalNFiles' : 'extraNFiles'} || !$pairs{$essenceFile->{srcRoot}}{$essenceFile->{extra} ? 'normalNFiles' : 'extraNFiles'}) { #other set not present or all copied
					#mark this source dir as up to date
					if (exists $transfers->{transfers}{$priority}{$essenceFile->{srcRoot}}) { #pair hasn't disappeared
						if (PROCESSING == $transfers->{transfers}{$priority}{$essenceFile->{srcRoot}}{stale}) { #new files haven't appeared since scanning
							$transfers->{transfers}{$priority}{$essenceFile->{srcRoot}}{stale} = UP_TO_DATE;
						}
						$transfers->{transfers}{$priority}{$essenceFile->{srcRoot}}{$ctimeCleared} = $pairs{$essenceFile->{srcRoot}}{latestCtime}; #use {latestCtime} rather than the ctime of the current file to ensure that the saved ctimeCleared reflects the age of the youngest scanned file even if this file has not been copied this time (ctimeCleared values in paths file reset and old but not newer files have been deleted off the server?)
						SaveConfig($transfers);
					}
					else {
						Report("Configuration has changed: rescanning\n", 1, $transfers, WARNING_COLOUR);
						@priorities = ();
						last;
					}
				}
			}
		} #file copy loop
	} # priorities loop
	delete $transfers->{current};
	$share->store(freeze $transfers);
	$share->unlock;
	#remove temp subdirs (in all cases because config might have changed)
	foreach (keys %{ $incomingDirs{dir} }) {
		rmdir;
	}
	foreach (keys %{ $incomingDirs{ftp} }) {
		my $fullUrl = "$url/$_/$incomingDirs{ftp}{$_}/";
		open REMOVE, '-|', @curl,
		 qw(-s -S --stderr -), #disable the progress meter; redirect error messages to stdout to capture them
		 '--quote', '-CWD ..', '--quote', "-RMD $incomingDirs{ftp}{$_}/", #will fail at this point with rc of 21 if directory not empty
		 $fullUrl; #will fail at this point with rc of 9 if directory not present (which doesn't matter - another copying process has removed it in the mean time)
		my $msgs = join '', <REMOVE>;
		close REMOVE;
		Report("WARNING: failed to remove directory '$incomingDirs{ftp}{$_}/' from '$_' in '$fullUrl' (may be another upload to this directory in progress)\n", 0, $share, WARNING_COLOUR) if 21 << 8 == $?;
	}
	if ($interrupting) {
		Report("Interrupting copying to check for new files of a higher priority\n", 0, $share); #rescan will start immediately because the child process will have been kicked
	}
	elsif ($retry) {
		Report('Pausing for ' . RECHECK_INTERVAL . " seconds before retrying.\n\n", 0, $share, RETRY_COLOUR);
	}
	elsif (!exists $transfers->{staleFrom}) {
		Report("Idle.\n\n", 0, $share);
	}
 }
}

sub Abort {
 my $msg = shift;
 if (defined $msg) {
	$msg = ": $msg";
 }
 else {
	$msg = '';
 }
 $msg |= '';
 if ($opts{m}) {
	print "\nABORTING$msg...\n"; #may take some time to abort if copying
 }
 else {
	print "\n" . colored("ABORTING$msg...", ABORT_COLOUR) . "\n"; #may take some time to abort if copying
 }
 $terminate = 1;
 kill "INT", $childPid if defined $childPid;
 waitpid($childPid, 0) if defined $childPid; #so that it's safe to remove the shared object
 FtpTrafficControl(0) if $opts{f};
 $recSock->shutdown(2) if defined $recSock; #leaving it open (possible but unlikely) blocks the port
 $monSock->shutdown(2) if defined $monSock; #leaving it open (possible but unlikely) blocks the port
 die "\n";
}

sub MakeDir { #makes a directory (recursively) without the permissions being modified by the current umask
	return system 'mkdir', '-p', '-m', PERMISSIONS, shift;
}

#msg: the string to print
#mode: 0 if share is unlocked; 1 if it is locked
#object: if share is unlocked, the object; if share is locked, the transfer hash (which should be saved later)
#colour: colour attributes, if defined
#notimestamp: if defined, don't print date or time and don't print newline if not at start of line
sub Report {
 my ($msg, $mode, $object, $colour, $notimestamp) = @_;
 my $transfers;
 if ($mode) { #transfers object supplied
	$transfers = $object;
 }
 else { #unlocked share supplied
	$object->lock(LOCK_EX);
	$transfers = thaw($share->fetch);
 }
 my $update = 0; #only freeze and store if we need to for efficiency
 unless (defined $notimestamp) {
	my @now = localtime();
	my $newdate = sprintf '%d%02d%02d', $now[5] + 1900, $now[4] +1, $now[3];
	if ($newdate ne $transfers->{date}) {
		print "Date is $newdate.\n";
		$transfers->{date} = $newdate;
		$update = 1;
	}
	$msg = sprintf '%s%02d:%02d:%02d %s', $transfers->{startOfLine} eq '1' ? '' : "\n", (@now)[2, 1, 0], $msg;
 }
 if (defined $colour && $colour ne '' && !$opts{m}) {
	my @parts = split /([\r\n]+)/, $msg; #keep but do not colour these characters as it can confuse terminal if background attributes are present
	$msg = join '', map(/[\r\n]/ ? $_ : colored($_, $colour), @parts);
 }
 print $msg;
 if ($transfers->{startOfLine} ne $msg =~ /\n$/s) {
	$transfers->{startOfLine} = $msg =~ /\n$/s;
	$update = 1;
 }
 $object->store(freeze $transfers) if $update && !$mode;
 $object->unlock unless $mode;
}

sub FtpTrafficControl { #Non-zero in first parameter switches traffic control on. Returns an empty string on success

 #determine the interface used to connect to the server
 $_ = gethostbyname($ftpDetails[0]); #doesn't matter if IP address supplied
 return "Failed to resolve IP address of $ftpDetails[0]" unless defined $_;
 $_ = join '.', unpack('C4', $_);
 $_ = `/bin/ip route get $_`;
 my ($interface) = / dev (.*?) /; #not at a fixed position in the result
 return "Failed to ascertain interface for FTP transfers to $ftpDetails[0]" unless defined $interface;

 my $cmd;
 if ($_[0]) { #traffic control on
	$cmd = "sudo /usr/sbin/tc\\
		qdisc add\\
		dev $interface\\
		root\\
		handle 1:0\\
		htb\\
		2>&1 &&\\
		sudo /usr/sbin/tc\\
		class add\\
		dev $interface\\
		parent 1:0\\
		classid 1:1\\
		htb\\
		burst 0mbit\\
		cburst " . FTP_SPEED . "mbit\\
		rate " . FTP_SPEED . "mbit\\
		ceil " . FTP_SPEED . "mbit\\
		2>&1 &&\\
		sudo /usr/sbin/iptables\\
		--table mangle\\
		--append POSTROUTING\\
		--out-interface $interface\\
		--protocol tcp\\
		--destination $ftpDetails[0]\\
		--jump CLASSIFY --set-class 1:1\\
		2>&1";
	return `$cmd`;
 }
 else {
	return `sudo /usr/sbin/tc qdisc del dev $interface root 2>&1`;
 }
}
