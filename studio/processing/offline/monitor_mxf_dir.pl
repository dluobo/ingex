#!/usr/bin/perl -w
# $Id: monitor_mxf_dir.pl,v 1.1 2007/09/11 14:08:45 stuart_hc Exp $
#
use strict;
use File::Basename;
use threads;
use threads::shared;

my $limit_files;
if (scalar @ARGV) {
	$limit_files = shift;
}

my $mxf_dir = '/video1/online';
chdir($mxf_dir);

my $out_dir = '/video1/offline';

# used to track running threads
my %running_threads;
my %to_join : shared;

# wait until modification time of directory changes
my $last_mtime = 0;
while (1) {
	my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
		$atime,$mtime,$ctime,$blksize,$blocks) = stat($mxf_dir) or die "stat";

	if ($mtime > $last_mtime) {
		# check to see if any DV50 MXF files do not have matching 20:1 files
		check_missing_transcode($mxf_dir, $out_dir);
	}
	else {
		print "no change\n";
	}

	sleep(5);
	$last_mtime = $mtime;
}

sub check_missing_transcode
{
	my ($dir, $targ_dir) = @_;

	my @required;
	print "checking $dir\n";
	opendir(DIR, $dir) || die "opendir $dir: $!";
	while (my $d = readdir(DIR)) {
		next if $d !~ /_v\d+\.mxf$/i;				# skip non-video-mxf files

		# skip for testing if specified
		next if defined $limit_files && $d !~ /$limit_files/;

		my $target = basename($d);
		$target =~ s/^/20to1_/;						# add 20to1_ to start of name
		next if (-f "$targ_dir/$target");				# target present
		next if (-f "$targ_dir/Creating/$target");		# target under construction

		push @required, $d;
		print "Required $d -> $target\n";
	}
	closedir(DIR);

	# run no more than 4 jobs at once
	my $max_jobs = 4;
	while (1) {
		printf "checking %d >= $max_jobs\n", scalar keys(%running_threads);
		if (scalar keys(%running_threads) >= $max_jobs) {
			# join finished jobs if any
			print "max jobs running, about to checking running jobs\n";
			{
				lock(%to_join);
				printf "  keys running_threads: [%s]\n", join(',', keys %running_threads);
				printf "  keys to_join: [%s]\n", join(',', keys %to_join);
				for my $id (keys %to_join) {
					print "next id from to_join= $id\n";
					my $obj = $running_threads{$id};
					die "bad obj $obj" if (! defined $obj);
					print "about to $obj->join()\n";
					$obj->join();
					print "finished join\n";
					delete $running_threads{$id};
					delete $to_join{$id};
				}
			}
			sleep(1);
			next;
		}

		# start a new job
		if (scalar @required) {
			my $file = shift @required;
			my $thr = threads->new(\&run_transcode, $file);
			my $id = $thr->tid();
			$running_threads{$id} = $thr;
			print "Started new job: $id for $file ($thr)\n";
			next;
		}

		# @required is empty
		last;
	}

	return;
}

sub run_transcode
{
	my ($src_dv50) = @_;

	my $target = basename($src_dv50);
	$target =~ s/^/20to1_/;						# add 20to1_ to start of name
	$target =~ s/_\d+_\w\d+\.mxf.*//;			# strip track, essence type suffix, .mxf

	my $src_pattern = $src_dv50;
	$src_pattern =~ s/_\d+_\w\d+.*/*.mxf/;

	my @inputs = glob($src_pattern);
	my $inputs = join(' ', @inputs);

	system("env LD_LIBRARY_PATH=/home/bamzooki/elstreeFeb12 /home/bamzooki/elstreeFeb12/TranscodeAvidMXF --dns prodautodb --mjpeg201 --createp $out_dir/Creating --destp $out_dir --failp $out_dir --prefix $target $inputs");

	printf "finished writing $src_dv50 -> $target (tid=%s)\n", threads->self()->tid();
	lock(%to_join);
	$to_join{ threads->self()->tid() } = 1;
}
