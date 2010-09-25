#!/usr/bin/perl -w
use strict;
# $Id: monitor.pl,v 1.1 2010/09/25 23:42:18 stuart_hc Exp $
#
# Standalone script to monitor a process's CPU time usage
# displayed as total over all cores, per core and per thread.
#
# Usage:
#	monitor.pl pid
# or
#	monitor.pl command-name
#
my $arg = shift || die "Need pid or command name\n";
my $pid;
if ($arg =~ /^\d+$/) {
	$pid = $arg;
}
else {
	my $result = `ps --no-headers -o pid -C $arg`;
	$result =~ s/\n/ /g;
	if ($result =~ /\d/) {
		($pid) = ($result =~ /(\d+)/);
		if ($result =~ /\d\s+\d/) {
			print "Using pid=$pid from possible list: $result\n";
		} else {
			print "Using pid=$pid\n";
		}
	}
	else {
		die "Could not find pid for $arg\n";
	}
}

# setup for recording CPU state
my ($last_user, $last_nice, $last_sys, $last_idle, $last_iowait, $last_irq, $last_sirq, $last_steal) = (0,0,0,0,0,0,0,0);
my @last;
open(PROCSTAT, "/proc/stat") || die "Could not open /proc/stat";

print "all{usr,sys,idle} core{usr,sys,idle} thread{thread-id:pcpu}\n";

my %threads;
my %cpu;
while (1) {
	get_cpu(\%cpu);
	printf "all{%.1f,%.1f,%.1f} core{", $cpu{user}, $cpu{sys}, $cpu{idle};
	for my $i (0 .. $#{ $cpu{core} }) {
		printf "%.1f,%.1f,%.1f%s",
			$cpu{core}[$i]{user}, $cpu{core}[$i]{sys}, $cpu{core}[$i]{idle},
			($i == $#{ $cpu{core} } ? '' : ' ');
	}
	print "} thread{";
	get_thread_time($pid, \%threads);
	show_thread_time(\%threads);
	print "}\n";
	sleep 2;
}


sub get_thread_time
{
	my ($pid, $p) = @_;
	open(PIPE, "ps -p $pid --no-headers -L -o lwp,cputime,pcpu |") || die "Could not run 'ps'\n";
	while(<PIPE>) {
		# e.g. "28696  4-09:32:51  98.2"
		my ($lwp, $hh, $mm, $ss, $pcpu) = /^\s*(\d+)\s+([\d-]+):(\d+):(\d+)\s+(\S+)/;
		if ($hh =~ /(\d+)-(\d+)/) {		# replace days with hours
			$hh = $1 * 24 + $2;
		}
		my $time_secs = $hh * 3600 + $mm * 60 + $ss;
		$p->{$lwp}{cpu} = $time_secs;
		$p->{$lwp}{pcpu} = $pcpu;
	}
}

sub show_thread_time
{
	my ($p) = @_;
	my @threads = sort {$a <=> $b} keys(%$p);
	for my $id (@threads) {
		printf "$id:$p->{$id}{pcpu}%s", ($id == $threads[-1] ? '' : ' ');
	}
}

# collect stats on all CPU cores
sub get_cpu
{
	my ($p) = @_;

	seek(PROCSTAT, 0, 0);		# rewind to start
	read(PROCSTAT, $_, 2000);	# 2000 bytes is more than enugh for 16 cpu cores of info

	# See linux/fs/proc/proc_misc.c and 'top' source code in procps package
	#
	# Read summary line for all cpus
	my ($user, $nice, $sys, $idle, $iowait, $irq, $sirq, $steal) =
		/cpu\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)/;

	# Process each cpu core
	my (@cpu, @diff, @total);
	if (/cpu1/)		# more than one cpu core?
	{
		while (/^cpu(\d+)\s*(.*)/mg) {
			my ($cpu_num, $components) = ($1, $2);
			my @components = split(' ', $components);			# split on whitespace
			$cpu[$cpu_num] = \@components;
		}

		my $num_cpus = scalar @cpu;
		for (my $n = 0; $n < $num_cpus; $n++) {
			$total[$n] = 0;
			for (my $i = 0; $i < 8; $i++) {
				$diff[$n][$i] = $cpu[$n][$i] - (defined $last[$n][$i] ? $last[$n][$i] : 0);
				$total[$n] += $diff[$n][$i];
			}
			$p->{core}[$n]{user} = $diff[$n][0] * 100.0 / $total[$n];
			$p->{core}[$n]{sys} = $diff[$n][2] * 100.0 / $total[$n];
			$p->{core}[$n]{idle} = $diff[$n][3] * 100.0 / $total[$n];
			#printf("cpu%d user=%.1f sys=%.1f idle=%.1f\n", $n, $diff[$n][0] * 100.0 / $total[$n], $diff[$n][2] * 100.0 / $total[$n], $diff[$n][3] * 100.0 / $total[$n]);
		}

		for (my $n = 0; $n < $num_cpus; $n++) {
			for (my $i = 0; $i < 8; $i++) {
				$last[$n][$i] = $cpu[$n][$i];
			}
		}
	}
	
	my $diff_user = $user - $last_user;
	my $diff_nice = $nice - $last_nice;
	my $diff_sys = $sys - $last_sys;
	my $diff_idle = $idle - $last_idle;
	my $diff_iowait = $iowait - $last_iowait;
	my $diff_irq = $irq - $last_irq;
	my $diff_sirq = $sirq - $last_sirq;
	my $diff_steal = $steal - $last_steal;

	my $total_diff = $diff_user + $diff_nice + $diff_sys + $diff_idle + $diff_iowait + $diff_irq + $diff_sirq + $diff_steal;
	$p->{user} = $diff_user * 100.0 / $total_diff;
	$p->{sys} = $diff_sys * 100.0 / $total_diff;
	$p->{idle} = $diff_idle * 100.0 / $total_diff;

	$last_user = $user;
	$last_nice = $nice;
	$last_sys = $sys;
	$last_idle = $idle;
	$last_iowait = $iowait;
	$last_irq = $irq;
	$last_sirq = $sirq;
	$last_steal = $steal;
}
