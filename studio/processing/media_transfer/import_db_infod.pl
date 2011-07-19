#! /usr/bin/perl -w

#  $Id: import_db_infod.pl,v 1.6 2011/07/19 07:29:57 john_f Exp $
#
# Copyright (C) 2009-2011 British Broadcasting Corporation.
# All Rights Reserved.
# Author: Matthew Marks
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
#

# Scans $ROOT/(keys $topDirs)/<all subdirs>/<all 8-digit subdirs>/ for files ending in .mxf (case-insensitive), invoking $importCuts on them if their mtime is <= the contents of $timestampFile in the same directory and updating $timestampFile with the latest mtime found.
# Does the same for $ROOT/$cutsDir/<all subdirs>/<all 8-digit subdirs>/ except it invokes $importMaterial on them.
# Creates and maintains the timestamp files .../$timestampFile to prevent re-importing (unless overridden by command-line option).
# Watches at every level from $ROOT downwards for the creation of applicable directories/files in order to keep importing new data. 
# If run as root, applies UID and GID of $ROOT to each $timestampFile created.
# Dies if $ROOT does not initially exist.
# Logs to the system log (/var/log/messages) and stderr if not run as a daemon.

use strict;
use Getopt::Std;
use Proc::Daemon;
use Linux::Inotify2;
use File::Path;
use File::Glob qw( :glob); # to ensure whitespace in names returned by glob is escaped
use Sys::Syslog qw(:standard :macros);

my $ROOT = '/store'; #the user/group of this is applied to all files created if the script is run as root
my $cutsDir = 'cuts'; #xml files in this branch of the directory tree (below $ROOT) contain cuts xml files
my %topDirs = qw(mxf_offline 1 mxf_online 1 browse 1 dv 1); #xml files in this branch of the directory tree (below $ROOT) contain material xml files (value 1 has no significance)
my $timestampFile = 'import_db_info_timestamp'; #files with this name will be created in every directory containing imported files
my $importCuts = '/usr/local/bin/import_cuts'; #executable called to import a cuts xml file
my $importMaterial = '/usr/local/bin/import_material'; #executable called to import a material xml file

use constant WATCH_FILE_MASK => IN_CREATE | IN_MOVED_TO;
use constant WATCH_DIR_MASK => IN_CREATE | IN_MOVED_TO | IN_ONLYDIR;

openlog 'import_db_infod', 'perror', 'user'; #'perror' echoes output to stderr (if using -n option)
# check arguments
my %opts;
&usage_exit unless getopts('cnvh:d:u:p:', \%opts);

sub usage_exit() {
 Die(<<END);
Usage: $0 [-c] [-n] [-v] [-h <database host>] [-d <database name>] [-u <database username>] [-p <database password>]
 -c to clear stored times (i.e. import all data)
 -n to prevent daemonising
 -v to be more verbose (reporting individual successful file imports)
END
}
my @exeOpts = ();
push @exeOpts, '--dbhost', $opts{h} if $opts{h};
push @exeOpts, '--dbname', $opts{d} if $opts{d};
push @exeOpts, '--dbuser', $opts{u} if $opts{u};
push @exeOpts, '--dbpw', $opts{p} if $opts{p};

die "$ROOT/ does not exist\n" unless -d $ROOT;

# daemonise and create notifier
Proc::Daemon::Init unless $opts{n};
Report("Started: PID=$$"); #do this after daemonising or it'll report the wrong PID
Report("Not reporting successful file imports (use -v option to change this)") unless $opts{v};
my $notifier = Linux::Inotify2->new() or Die("Unable to create notify object: $!");

my $ugid = join ':', (stat $ROOT)[4,5] if `whoami` =~ /^root$/; #if we can, change owner/group of new files to that of $ROOT

#subroutine called when something is created in the root directory (containing top-level directories)
my $topChange = sub {
 ScanTopDir($_[0]->fullname);
};

#subroutine called when something is created in a top level directory (containing project directories)
my $projectChange = sub {
 ScanProjDir($_[0]->fullname);
};

#subroutine called when something is created in a project directory (containing date directories)
my $dateChange = sub {
 ScanDateDir($_[0]->fullname);
};

#subroutine called when something is created in a material directory (containing xml directories)
my $materialChange = sub {
 ScanMaterialXMLDir($_[0]->fullname);
};

#subroutine called to import an xml file appearing in a material xml/ directory
my $importMaterialFile = sub {
 my $imported = 1;
 my $fail = 0;
 if (!-d $_[0] && $_[0] =~ /\.[xX][mM][lL]$/) {
  $fail = system $importMaterial, @exeOpts, $_[0];
 }
 else {
    $imported = 0;
 }
 if ($fail) {
    $_[0] =~ /(.*)\//;
    Warn("Importing material file $_[0] failed. Will not retry unless daemon is re-run with -c option or after deleting $1/$timestampFile");
    $imported = 0;
 }
 return $imported;
};

#subroutine called to import an xml file appearing in the cuts directory
my $importCutsFile = sub {
 my $imported = 1;
 my $fail = 0;
 if (!-d $_[0] && $_[0] =~ /\.[xX][mM][lL]$/) {
  $fail = system $importCuts, @exeOpts, $_[0];
 }
 else {
    $imported = 0;
 }
 if ($fail) {
    $_[0] =~ /(.*)\//;
    Warn("Importing cuts file $_[0] failed. Will not retry unless daemon is re-run with -c option or after deleting $1/$timestampFile");
    $imported = 0;
 }
 return $imported;
};

#subroutine called when something is created in a cuts date directory
my $cutsFileChange = sub {
 Report("Imported " . $_[0]->fullname) if $importCutsFile->($_[0]->fullname) && $opts{v};
 $_[0]->fullname =~ /(.*)\//;
 WriteTimestampFile($1, (stat $_[0]->fullname)[9]);
};

#subroutine called when something is created in a material date directory
my $materialFileChange = sub {
 Report("Imported " . $_[0]->fullname) if $importMaterialFile->($_[0]->fullname) && $opts{v};
 $_[0]->fullname =~ /(.*)\//;
 WriteTimestampFile($1, (stat $_[0]->fullname)[9]);
};



#initial scan of everything
Report("Watching $ROOT/ for new top-level subdirectories");
my $watch = $notifier->watch($ROOT, WATCH_DIR_MASK, $topChange) or Die("Couldn't watch: $!"); #do this before scanning or we could miss a top-level directory being created in the intervening period
Die("Couldn't open $ROOT/: $!") unless opendir ROOT, $ROOT;
foreach (readdir ROOT) {
   ScanTopDir("$ROOT/$_");
}
closedir ROOT;

#infinite loop waiting for stuff to appear
while (1) {
	$notifier->poll;
}

# subroutines that aren't pre-defined

sub ScanTopDir {
 my $topPath = shift;
 return unless -d $topPath;
 $topPath =~ m|$ROOT/(.*)|;
 return unless exists $topDirs{$1} || $1 eq $cutsDir; #only interested in certain top-level directories
 Report("Watching $topPath/ for new project subdirectories");
 my $watch = $notifier->watch($topPath, WATCH_DIR_MASK, $projectChange) or Warn("Couldn't watch: $!"); #do this before scanning or we could miss a project directory being created in the intervening period
 unless (opendir TOP, $topPath) {
    Warn("Couldn't open $topPath/: $!");
    $watch->cancel;
    return;
 }
 Report("Scanning $topPath/ for project subdirectories");
 foreach (readdir TOP) {
    ScanProjDir("$topPath/$_");
 }
 closedir TOP;
}

sub ScanProjDir {
 my $projPath = shift;
 return unless -d $projPath;
 return if $projPath =~ m|/\.\.?$|;
 Report("Watching $projPath/ for new date subdirectories");
 my $watch = $notifier->watch($projPath, WATCH_DIR_MASK, $dateChange) or Warn("Couldn't watch: $!"); #do this before scanning or we could miss a date directory being created in the intervening period
 unless (opendir PROJ, $projPath) {
    Warn("Couldn't open $projPath/: $!");
    $watch->cancel;
    return;
 }
 Report("Scanning $projPath/ for date subdirectories");
 foreach (readdir PROJ) {
    ScanDateDir("$projPath/$_");
 }
 closedir PROJ;
}

sub ScanDateDir {
 my $datePath = shift;
 return unless -d $datePath;
 return unless $datePath =~ /\/\d{8}$/;

 if ($datePath =~ /^$ROOT\/$cutsDir\//) {
   # scan for cuts xml files
   ScanFileDir($datePath, 1, $cutsFileChange);
 }
 else {
   # scan for material xml directories
   Report("Watching $datePath/ for new xml subdirectories");
   my $watchXML = $notifier->watch($datePath, WATCH_DIR_MASK, $materialChange) or Warn("Couldn't watch: $!"); #do this before scanning or we could miss a date directory being created in the intervening period
   unless (opendir DATE_DIR, $datePath) {
      Warn("Couldn't open $datePath/: $!");
      $watchXML->cancel;
      return;
   }
   Report("Scanning $datePath/ for xml subdirectories");
   foreach (readdir DATE_DIR) {
      ScanMaterialXMLDir("$datePath/$_");
   }
   closedir DATE_DIR;
 }
}

sub ScanMaterialXMLDir {
 my $xmlPath = shift;
 return unless -d $xmlPath;
 return unless $xmlPath =~ /\/xml$/;
 ScanFileDir($xmlPath, 0, $materialFileChange);
}

sub ScanFileDir {
  my ($dirPath, $cuts, $watchCallback) = @_;
  Report("Watching $dirPath/ for new files");
  my $watch = $notifier->watch($dirPath, WATCH_FILE_MASK, $watchCallback) or Warn("Couldn't watch: $!"); #do this before scanning or we could miss a file being created in the intervening period
  unless (opendir FILE_DIR, $dirPath) {
    Warn("Couldn't open $dirPath/: $!");
    $watch->cancel;
    return;
  }
  my $timestamp = 0;
  if (!$opts{c} && -e "$dirPath/$timestampFile") { #when this sub is called by a watcher, there will be no timestamp file anyway so $opts{c} check is irrelevant
    if (open TIMESTAMP, "$dirPath/$timestampFile") {
       $timestamp = <TIMESTAMP>;
       chomp $timestamp;
       if (<TIMESTAMP> || $timestamp !~ /^\d+$/) {
          Warn("Unrecognised contents of $dirPath/$timestampFile: (re-)importing all files in this directory\n");
          $timestamp = 0;
       }
       close TIMESTAMP;
    }
    else {
       Warn("Couldn't open $dirPath/$timestampFile: $!: (re-)importing all files in this directory\n");
    }
  }
  Report("Scanning $dirPath/ for " . ($cuts ? "cuts" : "material") . " files to import");
  my %files;
  my $mtime = 0; #in case no files
  my $maxMTime = 0;
  foreach (readdir FILE_DIR) {
    my $file = "$dirPath/$_";
    next if -d $file;
    $mtime = (stat "$dirPath/$_")[9];
    next if $mtime < $timestamp; #if <=, may miss later files created in the same second; instead we may try to import the earlier files created in the same second twice on failure which isn't a major problem
    if ($mtime > $maxMTime) {
      $maxMTime = $mtime;
    }
    push @{$files{$mtime}}, $_;
  }
  closedir FILE_DIR;
  my $count = 0;
  foreach $mtime (sort keys %files) { #mtime order, oldest first
    foreach (@{$files{$mtime}}) {
       $count++ if ($cuts ? $importCutsFile : $importMaterialFile)->("$dirPath/$_");
    }
  }
  Report('Imported ' . ($count ? $count : 'no') . ' file' . (1 == $count ? '' : 's') . " from $dirPath/");
  WriteTimestampFile($dirPath, $maxMTime);
}

sub WriteTimestampFile {
 my $file = "$_[0]/$timestampFile";
 if (!open TIMESTAMP, ">$file") {
    Warn("Couldn't open $file for writing: $!");
 }
 else {
    print TIMESTAMP "$_[1]\n";
    close TIMESTAMP;
    system 'chown', '-fh', $ugid, $file if $ugid; #if we can, change owner/group of file
 }
}

sub Die {
 syslog LOG_CRIT, (shift) . ": dying";
 closelog;
 exit 1;
}

sub Warn {
 syslog LOG_ERR, shift;
}

sub Report {
 syslog LOG_INFO, shift;
}
