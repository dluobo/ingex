#!/usr/bin/perl -w
# $Id: benchmark.pl,v 1.1 2010/09/25 06:33:06 stuart_hc Exp $
use DBI;
use Getopt::Long;
use IO::File;
use Time::HiRes qw(usleep);
use strict;

=pod

=head1 NAME

benchmark.pl - script to test Ingex recordings and provide benchmarks

=head1 SYNOPSIS

perl benchmark.pl

=head1 DESCRIPTION

B<benchmark.pl> is designed to iterate through all supported recording
formats and verify that recordings are made successfully.  In addition,
benchmark data are written to a data file which can be displayed in a
graphical plot using the gnuplot command.

=head1 USAGE

By default a built-in list of encoding formats are tested.
Use -c <config_file> to use a config file instead.  The
format of the config file is given in the CONFIG FILE section.

When any encoding format is specified as HD, the sdi capture runs with primary
capture format of HD.  When Both encode1 and encode2 formats are SD
the capture program runs in SD.

One important determining factor of cputime used by an encoding is the 
benchmark picture.  The configuration file allows the user to specify
different benchmark pictures for each resolution, allowing consistency
of results across benchmark runs.  Without a benchmark picture the
sdi capture will use the incoming signal, or colour bars when using the
dummy sdi capture.

After an encoding run is complete, encoded files are verified to ensure they
are valid MXF files (in the case of an MXF format) which can be decodec by
ingex player.

When a benchmark run is complete SUCCEEDED or FAILED is displayed, along with
the command required to view the cpu usage plot data.
e.g.
  gnuplot plot_CHx1_VC3-185iOP-ATOM_VC3-36pP-ATOM_7289.gnuplot

The system return code is 0 if all benchmarks succeeded, or 1 if any failed.

=head1 CONFIG FILE

  recorder = Ingex2                            # recorder name in database
  ORBRef = corbaloc:iiop:192.168.2.1:8888      # required by Corba clients
  duration = 60                                # in seconds

  # benchmark pictures are optional - if not set, SDI input is used
  # (or colourbars for dummy capture)
  # picture format is one frame of uncompressed UYVY video
  benchmark_picture_1080 = /home/stuartc/cotswolds_1920_1080.uyvy
  benchmark_picture_720 = /home/stuartc/cotswolds_1280_720.uyvy
  benchmark_picture_576 = /home/stuartc/cotswolds_720x576.uyvy

  benchmarks:
  # format is: channels, ENCODE1, ENCODE2, ENCODE3
  # (use None to disable a channel)
    1, MJPEG 2:1 OP-ATOM,          MJPEG 20:1 OP-ATOM,   None
    1, MJPEG 10:1 OP-ATOM,         MJPEG 15:1s OP-ATOM,  None
    1, DV50 OP-ATOM,               DV25 OP-ATOM,         None
    1, Uncompressed UYVY OP_ATOM,  MJPEG 15:1s OP-ATOM,  None
    1, VC3-185i OP-ATOM,           VC3-36p OP-ATOM,      MPEG4 Quicktime
  
    2, MJPEG 2:1 OP-ATOM,          MJPEG 20:1 OP-ATOM,   None
    2, MJPEG 10:1 OP-ATOM,         MJPEG 15:1s OP-ATOM,  None
    2, DV50 OP-ATOM,               DV25 OP-ATOM,         None
    2, Uncompressed UYVY OP_ATOM,  MJPEG 15:1s OP-ATOM,  None
    2, VC3-185i OP-ATOM,           VC3-36p OP-ATOM,      MPEG4 Quicktime
 
=cut

# Assumes database is running and ACE-TAO naming server is running
# e.g. sudo service postgresql start
# e.g. sudo service tao-cosnaming start
#
# Defaults for London
my $base = "$ENV{HOME}/ap-workspace/ingex";
my $benchmark_picture_1080 = "$ENV{HOME}/cotswolds_1920_1080.uyvy";
my $benchmark_picture_720 = "$ENV{HOME}/cotswolds_1280_720.uyvy";
my $benchmark_picture_576 = "$ENV{HOME}/cotswolds_720x576.uyvy";
my $sdi_memory_size = 750;
my $ORBRef = "corbaloc:iiop:172.29.145.172:8888";
my $sdi_program = "dvs_sdi";
if ($ENV{HOSTNAME} =~ /hobbes/) {
	($base = $ENV{PWD}) =~ s,/studio/scripts/benchmark.*,,;		# work out $base from current dir
	$benchmark_picture_1080 = "$ENV{HOME}/work/samples_uyvy/cotswolds_1920_1080.uyvy";
	$benchmark_picture_720 = "$ENV{HOME}/work/samples_uyvy/cotswolds_1280_720.uyvy";
	$benchmark_picture_576 = "$ENV{HOME}/work/samples_uyvy/cotswolds_720x576.uyvy";
	$sdi_memory_size = 250;
	$ORBRef = "corbaloc:iiop:192.168.2.1:8888";
	$sdi_program = "dvs_dummy";
}

my $opt_list_recorders_only = 0;
my $opt_list_formats_only = 0;
my $opt_config_file;
my $opt_recorder;
my $opt_run_sdi_capture = 1;
my $opt_recording_duration = 10;		# in seconds
my $opt_help = 0;
my $verbose = 0;
GetOptions(	'b=s', \$base,
			'l', \$opt_list_recorders_only,
			'f', \$opt_list_formats_only,
			'c=s', \$opt_config_file,
			'd=i', \$opt_recording_duration,
			'r=s', \$opt_recorder,
			'x', \$opt_run_sdi_capture,
			'v', \$verbose,
			'h', \$opt_help,
			);
if ($opt_help) {
	print "Usage:\n\n";
	print "-l                list Recorder configuration, then exit\n";
	print "-f                list possible recording formats, then exit\n";
	print "-c <config_file>  use config file for benchmark settings\n";
	print "-b <base_dir>     base dir of ingex source tree e.g. /home/john/ingex\n";
	print "-r <Recorder>     select Recorder name to use for benchmark run\n";
	print "-d <seconds>      duration of recordings for a benchmark run in seconds\n";
	print "-x                don't run sdi capture (allows user to run capture externally)\n";
	print "-v                verbose messages\n";
	exit 0;
}

# Built-in list of benchmark encodings
# Typically a config file will specify the list of encodings overriding this.
#
# [channels, ENCODE1, ENCODE2, ENCODE3]
# TODO: add more options e.g.
# 		opt_frame_threads,
# 		sdi capture options (e.g. base, ORBRef, threads opts)
my @benchmarks = (	
		[1, 'Uncompressed UYVY OP_ATOM',	'None',					'MPEG4 Quicktime'],
		[1, 'MJPEG 10:1 OP-ATOM',			'MJPEG 15:1s OP-ATOM',	'None'],
		[1, 'MJPEG 3:1 OP-ATOM',			'MJPEG 10:1 OP-ATOM',	'None'],
		[1, 'MJPEG 2:1 OP-ATOM',			'MJPEG 10:1m OP-ATOM',	'None'],
		[1, 'DV50 OP-ATOM',					'DV25 OP-ATOM',			'None'],
		[1, 'VC3-185i OP-ATOM', 			'MJPEG 20:1 OP-ATOM',	'None'],
		);
if (defined $opt_config_file) {
	load_config_file($opt_config_file);
}

# Use Pg database driver to connect to database
my $dbh = DBI->connect("dbi:Pg:dbname=prodautodb", "bamzooki", "") || die "Could not connect to database\n";

# initialise database variables
my (%res_by_id, %id_by_res);
my %config;
my (%recs, %recid_by_name);
my $res;
read_resolution();

get_config();

# list formats if specified
if ($opt_list_formats_only) {
	print "Possible recording formats:\n";
	foreach my $key (sort {$a <=> $b} keys %res_by_id) {
		printf "  $res_by_id{$key}\n";
	}
	exit 0;
}

# display Recorder config
print "Current Recorder config:\n";
for my $recid (sort keys %recs) {
	if (exists $config{$recid}{ENCODE1}) {
		print "  $recs{$recid}($recid) ENCODE1=$config{$recid}{ENCODE1}";
		}
	else {
		print "  $recs{$recid}($recid) not configured\n";
		next;
	}

	if (exists $config{$recid}{ENCODE2}) {
		print " ENCODE2=$config{$recid}{ENCODE2}"
	}
	if (exists $config{$recid}{ENCODE3}) {
		print " ENCODE3=$config{$recid}{ENCODE3}"
	}
	print "\n";
}
exit 0 if ($opt_list_recorders_only);



# check args are correct
if (! defined $opt_recorder) {
	die "-r <recorder> required to run benchmark\n";
}

my $recname = $opt_recorder;
if (! exists $recid_by_name{$recname}) {
	die "No recorder '$recname'\n";
}


# Check encoding config is valid first
for (my $i = 0; $i <= $#benchmarks; $i++) {
	my ($num_channels, $enc1, $enc2, $enc3) = @{ $benchmarks[$i] };
	die "num_channels=$num_channels must be >= 1" if $num_channels < 1;
	die "encoding not valid ($enc1) - use -f to list formats" if !(exists $id_by_res{$enc1});
	die "encoding not valid ($enc2) - use -f to list formats" if !(exists $id_by_res{$enc2});
	die "encoding not valid ($enc3) - use -f to list formats" if !(exists $id_by_res{$enc3});
}

# setup for recording CPU state
my ($last_user, $last_nice, $last_sys, $last_idle, $last_iowait, $last_irq, $last_sirq, $last_steal) = (0,0,0,0,0,0,0,0);
my @last;
open(PROCSTAT, "/proc/stat") || die "Could not open /proc/stat";

######################
# Main benchmark loop
#
# Loop over each encoding to test.
# Set database entries, start & stop Recorder and note times and files
#
my $full_benchmark_result = 1;
my @benchmark_results;
for (my $i = 0; $i <= $#benchmarks; $i++) {
	my $result = 1;
	my $total_cputime = 0;

	system("rm -f /tmp/benchmark_*");

	my ($num_channels, $enc1, $enc2, $enc3) = @{ $benchmarks[$i] };

	print "\n";
	printf "Benchmark run $i: channels = $num_channels\n";

	my $sdi_pid;
	if ($opt_run_sdi_capture) {
		$sdi_pid = run_sdi_capture($enc1, $enc2, $num_channels);
	}
	else {
		chomp($sdi_pid = `ps --no-headers -o pid -C $sdi_program`);
	}

	if (! kill(0, $sdi_pid)) {
		printf("$sdi_program not running\n");
		$result = 0;
		goto update_results;
	}

	# update datebase with new encoder configuration
	update_recorderparameter('ENCODE1_RESOLUTION', $enc1);
	update_recorderparameter('ENCODE2_RESOLUTION', $enc2);
	update_recorderparameter('ENCODE3_RESOLUTION', $enc3);

	my $Rec_pid = run_Recorder();
	if (! $Rec_pid) {
		kill_process($Rec_pid);
		$result = 0;
		goto update_results;
	}

	my @rec_files;

	start_recording();
	my $start_time = time();

	my $plot = start_plot_data($Rec_pid, $num_channels, $enc1, $enc2, $enc3);

	# Monitor Recorder and stop recording once $opt_recording_duration reached
	my %rec_threads;
	my %cpu;
	while (1) {
		# check Recorder is still running
		if (! kill(0, $Rec_pid)) {
			printf("Recorder no longer running (presumably it crashed)\n");
			$result = 0;
			last;
		}

		# check sdi capture is still running
		if (! kill(0, $sdi_pid)) {
			printf("capture no longer running (presumably it crashed)\n");
			$result = 0;
			last;
		}

		# display process and thread stats
		get_cpu(\%cpu);
		get_thread_time($Rec_pid, \%rec_threads);
		my $now = time();
		print_cputime_and_thread_stats($now - $start_time, \%rec_threads, \%cpu);

		# write plot data to file
		add_plot_data($plot, $now - $start_time, \%cpu);

		# pause and loop until recording duration reached
		sleep 2;
		if ($now - $start_time >= $opt_recording_duration) {
			@rec_files = stop_recording($Rec_pid);
			last;
		}
	}

	print "Waiting for Recorder to complete writing\n";
	my @complete_files = get_Recorder_completed_files($plot, $start_time, \%cpu, $Rec_pid, @rec_files);
	printf "%d files out of %d were completed\n", scalar @complete_files, scalar @rec_files;

	$result = 0 if scalar(@complete_files) != scalar(@rec_files);

	$total_cputime = total_cputime(\%rec_threads);

	# kill Recorder and sdi processes
	kill_process($Rec_pid);
	if ($opt_run_sdi_capture) {
		kill_process($sdi_pid, 3);
	}

	add_plot_data($plot, time() - $start_time, \%cpu);
	close_plot_data($plot, \%cpu);
	print "  gnuplot $plot->{cmdname}\n";

	print "Verifying files\n";
	my $valid_files = verify_recorded_files(@complete_files);
	$result = 0 if ! $valid_files;

update_results:
	printf("Benchmark run %s for \"channels=$num_channels enc1=$enc1 enc2=$enc2 enc3=$enc3\"\n", $result ? "SUCCEEDED" : " FAILED!!");
	push @benchmark_results, [$result, $total_cputime, "channels=$num_channels enc1=$enc1 enc2=$enc2 enc3=$enc3"];
	$full_benchmark_result = 0 if ! $result;		# final result will be error if any test fails
	
	print "\n";
}

printf "Overall benchmark %s\n", $full_benchmark_result ? "SUCCEEDED" : "FAILED!!";
map { printf "  %s: cputime=%3d $_->[2]\n", $_->[0] ? "SUCCEEDED" : "   FAILED", $_->[1] } @benchmark_results;

exit($full_benchmark_result ? 0 : 1);

# verify recorded files are correct
#
# Implemented checks (only MXF files so far):
#	avidmxfinfo reads non-zero clip duration
#	player can play full file
#	player's count of decoded/played frames equals avidmxfinfo's clip duration
#	all related files have the same duration
#
# Further checks could include:
# 	MXFDump, InfoDumper
# 	aafextract/simple_mxf_demux counts of decoded frames
# 	run player --raw-out then calcpsnr for each frame compared to golden frame / file
# 	extract audio and compare
# 	A/V sync check (provided clapper-board input was used)
sub verify_recorded_files
{
	my @files = @_;

	return 0 if (scalar @files == 0);

	my $result = 1;
	my $first_clip_duration = -1;
	my $first_clip_basename;
	for my $file (@files) {
		(my $basename = $file) =~ s,.*/,,;		# get basename
		if ($file !~ /\.mxf$/) {
			next;		# skip non-MXF files (TODO: check .mov and raw files)
		}

		my $info = `$base/libMXF/examples/avidmxfinfo/avidmxfinfo $file`;

		# get clip duration from avidmxfinfo as frames and timecode length
		my ($clip_duration_fr, $clip_duration_tc) = ($info =~ /Clip duration = (\d+) samples \(([0-9:]+)/);
		if ($clip_duration_fr == 0) {
			print "clip duration 0 for $file\n";
			$result = 0;
			next;
		}
		print "$basename: duration $clip_duration_fr ($clip_duration_tc)\n" if $verbose;
		if ($first_clip_duration == -1) {
			$first_clip_duration = $clip_duration_fr;
			$first_clip_basename = $basename;
		}

		if ($clip_duration_fr != $first_clip_duration) {
			print "  different clip durations $clip_duration_fr != $first_clip_duration\n";
			print "    $basename != $first_clip_basename\n";
			$result = 0;
		}

		my $logbuf_file = "tmp_logbuf_$$.txt";
		my $player_info = `$base/player/ingex_player/player --log-buf $logbuf_file --src-info --null-out --exit-at-end --disable-shuttle -m $file 2>&1`;
		if ($? != 0) {
			print "player failed to read $file\n";
			$result = 0;
			system("rm -f $logbuf_file");
			next;
		}

		# Count lines in logbuf file to get duration
		if (!open(LOGBUF, $logbuf_file)) {
			$result = 0;
			warn "Could not open $logbuf_file: $!";
			next;
		}
		my $buf_lines = 0;
		while(<LOGBUF>) {
			next if /^#/;
			$buf_lines++;
		}
		system("rm -f $logbuf_file");
		if ($buf_lines != $clip_duration_fr) {
			printf "$basename: player decoded frames %d != avidmxfinfo frames %d\n", $buf_lines, $clip_duration_fr;
			$result = 0;
			next;
		}

		print "$basename: player decoded frames == avidmxfinfo frames == $clip_duration_fr\n" if $verbose;
	}
	return $result;
}


######################
# Process management
######################
sub run_Recorder
{
	# Execute Recorder, using awkward shell command to guarantee $Rec_pid is corrent
	my $Recorder_command = "Recorder --name $recname --dbuser bamzooki --dbpass bamzooki -ORBDefaultInitRef $ORBRef -ORBDottedDecimalAddresses 1";
	print "Starting Recorder\n$base/studio/ace-tao/Recorder/$Recorder_command\n";
	system("sh -c \"echo \\\$\\\$ > /tmp/benchmark_Recorder_$$; exec $base/studio/ace-tao/Recorder/$Recorder_command > Recorder_log_\\\$\\\$.txt 2>&1\" &");
	usleep(500000);		# give it half a second to start up
	chomp(my $Rec_pid = `cat /tmp/benchmark_Recorder_$$`);

	# Use lsof to check when Recorder is 'up' and LISTENing for CORBA commands
	my $try = 0;
	while (1) {
		if (kill(0, $Rec_pid)) {						# Recorder running?
			my $lsof = `lsof -n -p $Rec_pid`;
			last if ($lsof =~ /\sTCP\s.*LISTEN/);		# Recorder is now listening
		}
		sleep 1;
		$try++;
		if ($try >= 10) {
			my $running = kill(0, $Rec_pid);
			printf "Recorder pid $Rec_pid %s running, not LISTENing%s\n",
						$running ? "is" : "not",
						! $running ? " (perhaps it crashed)" : "";
			return 0;
		}
	}

	printf "Recorder pid $Rec_pid, running = %d\n", kill(0, $Rec_pid);
	return $Rec_pid;
}

sub get_Recorder_completed_files
{
	my ($plot, $start_time, $cpu, $pid, @files) = @_;

	my $timeout = 60;		# seconds
	my $interval = 2;		# seconds

	my @completed_files;
	my $try = 0;
	while (1) {
		my $num_creating = 0;
		@completed_files = ();

		# in-progress files contain "/Creating/" while completed files do not
		for my $inprogress (@files) {
			my $complete = $inprogress;
			$complete =~ s,/Creating/,/,;
			if (-e $inprogress) {
				print "  in-progress exists: $inprogress\n" if $verbose;
				$num_creating++;
			}
			if (-e $complete) {
				print "  completed   exists: $complete\n" if $verbose;
				push @completed_files, $complete;
			}
		}
		print "\n" if $verbose;
		printf("  %d out of %d files complete\n", scalar @completed_files, scalar @files) if ! $verbose;

		# update plot data
		my $now = time();
		add_plot_data($plot, $now - $start_time, $cpu);
			
		last if $num_creating == 0;
		sleep($interval);

		$try++;
		if ($try >= ($timeout / $interval)) {
			printf "Recorder pid $pid %s running, has incomplete files after $timeout sec timeout\n", kill(0, $pid) ? "is" : "not";
			return @completed_files;
		}
	}
	return @completed_files;
}

sub start_recording
{
	my $RecorderClient_command = "RecorderClient -ORBDefaultInitRef $ORBRef -ORBDottedDecimalAddresses 1 $recname";
	print "Start recording\n";
	system("$base/studio/ace-tao/RecorderClient/$RecorderClient_command -start");
}

sub stop_recording
{
	my ($pid) = @_;

	my $lsof = `lsof -n -p $pid`;
	my @files = ($lsof =~ m,\s(/\S+/Creating/\S+),gsm);			# pull out all files with a /Creating/ in the path
	printf "Stop recording.  Currently writing to %d files:\n", scalar @files;
	for my $f (@files) { print "  $f\n" };

	my $RecorderClient_command = "RecorderClient -ORBDefaultInitRef $ORBRef -ORBDottedDecimalAddresses 1 $recname";
	system("$base/studio/ace-tao/RecorderClient/$RecorderClient_command -stop");

	return @files;
}

sub run_sdi_capture
{
	my ($enc1, $enc2, $num_channels) = @_;

	my $mode = 'PAL';			# e.g. PAL, 1920x1080i25, 1280x720p50, NTSC, 1920x1080i29, 1280x720p59
	my $benchmark_picture = $benchmark_picture_576;
	my $pri_fmt = 'YUV422';		# e.g. YUV422, DV50, UYVY
	my $sec_fmt = 'None';		# e.g. None, YUV422, DV50, MPEG, DV25

	# Use the encoding formats to determine which primary and secondary formats to use
	# and also which benchmark image (if any)
	if ($enc1 =~ /DVCPRO-HD|VC3/) {		# DVCProHD and DNxHD(VC3)
		$mode = '1920x1080i25';
		$pri_fmt = 'YUV422';
		$benchmark_picture = $benchmark_picture_1080;
	}
	elsif ($enc1 =~ /MJPEG|IMX/) {
		$pri_fmt = 'YUV422';
	}
	elsif ($enc1 =~ /DV50/) {
		$pri_fmt = 'DV50';
	}
	elsif ($enc1 =~ /UYVY/) {
		$pri_fmt = 'UYVY';
	}

	if ($enc2 =~ /MJPEG/) {
		$sec_fmt = 'YUV422';
	}
	elsif ($enc2 =~ /DV50/) {
		$sec_fmt = 'DV50';
	}
	elsif ($enc2 =~ /DV25/) {
		$sec_fmt = 'DV25';
	}
	elsif ($enc2 =~ /MPEG/) {
		$sec_fmt = 'MPEG';
	}

	my $sdi_args = " -m $sdi_memory_size -mode $mode -f $pri_fmt -s $sec_fmt -c $num_channels -b $benchmark_picture";

	print "Executing: $base/studio/capture/$sdi_program $sdi_args\n";

	# Unwieldy shell command needed to reliably get PID by reading tmp file
	system("sh -c \"export DVSDUMMY_PARAM=video=$mode; echo \\\$\\\$ > /tmp/benchmark_sdi_$$; exec $base/studio/capture/$sdi_program $sdi_args > sdi_log_$$.txt 2>&1\" &");
	sleep(3);

	chomp(my $pid = `cat /tmp/benchmark_sdi_$$`);
	printf "SDI capture pid $pid, running = %d\n", kill(0, $pid);
	return $pid;
}

sub kill_process
{
	my ($process, $delay) = @_;
	$delay = 1 if ! defined $delay;
	my $pid;
	if ($process =~ /^\d+$/) {		# process can be pid number (all digits)
		$pid = $process;
	}
	else {
		chomp($pid = `cat /tmp/benchmark_${process}_$$`);
	}

	if (! kill(0, $pid)) {
		print "Process $pid was not running, nothing to kill\n";
		return;
	}

	print "Killing $process, pid=$pid\n" if $verbose;
	kill(2, $pid);
	sleep($delay);

	my $try = 0;
	while (kill(0, $pid)) {
		if ($try > 5) {
			print "Could not kill $process after $try tries, giving up\n";
			last;
		}
		sleep(2);
		print "Killing $pid again, try=$try\n" if $verbose;
		kill(2, $pid);
		$try++;
	}
	print "$process, pid=$pid, is dead\n" if $verbose;
}

#######################################
# Plotting and recording process stats
#######################################
sub start_plot_data
{
	my ($recpid, $num_channels, $enc1, $enc2, $enc3) = @_;

	my $title = "channels=$num_channels enc1=$enc1 enc2=$enc2 enc3=$enc3";
	my $fileprefix = "plot_CHx${num_channels}_${enc1}_${enc2}_$recpid";
	$fileprefix =~ s/ +//g;			# remove spaces from filename
	$fileprefix =~ s/://g;			# remove colons from filename

	my %plot;
	$plot{cmdname} = "$fileprefix.gnuplot";
	$plot{datname} = "$fileprefix.dat";
	$plot{cmdfh} = new IO::File "> $plot{cmdname}";
	$plot{datfh} = new IO::File "> $plot{datname}";
	print {$plot{cmdfh}} "set title \"$title\"\n";
	print {$plot{cmdfh}} "set xlabel \"elapsed time (seconds)\"\n";
	print {$plot{cmdfh}} "set ylabel \"CPU time %\"\n";
	print {$plot{cmdfh}} "set yrange [0:100]\n";

	return \%plot;
}

sub add_plot_data
{
	my ($p, $time, $pcpu) = @_;

	print {$p->{datfh}} "$time ";			# x axis is in seconds
	for my $i (0 .. $#{ $pcpu->{core} }) {
		printf {$p->{datfh}} "%.1f ", $pcpu->{core}[$i]{user};
	}
	printf {$p->{datfh}} "\n";
}

sub close_plot_data
{
	my ($p, $pcpu) = @_;

	# Each core has a separate column in the data file and requires
	# a separate "using" plot argument
	for my $i (0 .. $#{ $pcpu->{core} }) {
		printf {$p->{cmdfh}} "%s '$p->{datname}' using 1:%d title \"core %d\" with lines%s",
				$i == 0 ? "plot" : "",
				$i + 2,
				$i,
				$i == $#{ $pcpu->{core} } ? "\n" : ", ";
	}
	printf {$p->{cmdfh}} "pause -1 \"Hit return to close window\"\n";
	printf {$p->{cmdfh}} "\n";
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
sub total_cputime
{
	my ($p) = @_;
	my @threads = sort {$a <=> $b} keys(%$p);
	my $total_cputime = 0;
	for my $id (@threads) {
		$total_cputime += $p->{$id}{cpu};
	}
	return $total_cputime;
}
sub show_thread_time
{
	my ($p) = @_;
	my @threads = sort {$a <=> $b} keys(%$p);
	for my $id (@threads) {
		printf "$id:$p->{$id}{pcpu}%%%s", ($id == $threads[-1] ? '' : ' ');
	}
}
sub print_cputime_and_thread_stats
{
	my ($elapsed_time, $rec_threads, $cpu) = @_;

	my $total_cputime = total_cputime($rec_threads);
	printf "%3d cputime=%3ds all-cores=%4.1f%%[", $elapsed_time, $total_cputime, $cpu->{user}, $cpu->{sys}, $cpu->{idle};

	for my $i (0 .. $#{ $cpu->{core} }) {
		printf "%4.1f%%%s", $cpu->{core}[$i]{user}, ($i == $#{ $cpu->{core} } ? ']' : ',');
	}
	print " threads[";
	show_thread_time($rec_threads);
	print "]\n";
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

######################
# config file loading
######################
sub load_config_file
{
	my ($file) = @_;

	open(CONFIG, $file) || die "Could not read config file $file: $!\n";

	my $in_benchmarks_section = 0;
	while (<CONFIG>) {
		next if (/^#/ || /^\s*$/);		# skip comments and blank lines
		s/#.*//;						# strip trailing comments

		if (/(\w+)\s*=\s*(\S+)/) {		# key = value
			my ($key, $value) = ($1, $2);
			if ($key eq 'base')						{ $base						= $value; next }
			if ($key eq 'benchmark_picture_1080')	{ $benchmark_picture_1080	= $value; next }
			if ($key eq 'benchmark_picture_720')	{ $benchmark_picture_720	= $value; next }
			if ($key eq 'benchmark_picture_576')	{ $benchmark_picture_576	= $value; next }
			if ($key eq 'recorder')					{ $opt_recorder				= $value; next }
			if ($key eq 'duration')					{ $opt_recording_duration	= $value; next }
			if ($key eq 'ORBRef')					{ $ORBRef					= $value; next }
			if ($key eq 'sdi_program')				{ $sdi_program				= $value; next }
			if ($key eq 'sdi_memory_size')			{ $sdi_memory_size			= $value; next }

			warn "\nUnknown parameter in config file: $_\n";
		}

		if (/^\s*benchmarks\s*:/) {
			$in_benchmarks_section = 1;
			@benchmarks = ();					# clear and overwrite benchmarks array
			next;
		}

		if ($in_benchmarks_section) {
			my @line = split(/,/);
			map { s/^\s+//; s/\s+$// } @line;	# strip leading, trailing whitespace
			push @benchmarks, [@line];
			next;
		}
	}
}

######################
# Database functions
######################
sub update_recorderparameter
{
	my ($field, $encoding) = @_;

	printf "  setting $field=%s(%s)\n", $encoding, $id_by_res{$encoding};
	$dbh->do("UPDATE recorderparameter set rep_value=$id_by_res{$encoding} where rep_name='$field' and rep_recorder_conf_id=$recid_by_name{$recname}") || die "update failed: $field=$id_by_res{$encoding}";
}

# update %config with current recorder config
sub get_config
{
	# fill %recs
	$res = $dbh->selectall_arrayref("SELECT rer_name,rer_conf_id FROM recorder ORDER BY rer_conf_id");
	for my $p (@$res) {
		$recs{$p->[1]} = $p->[0];			# %recs has 17 => 'Ingex-HD'
		$recid_by_name{$p->[0]} = $p->[1];
	}

	# get ENCODE1 settings
	$res  = $dbh->selectall_arrayref("SELECT rep_recorder_conf_id,rep_value from recorderparameter where rep_name like 'ENCODE1_RESOLUTION' order by rep_recorder_conf_id,rep_name");
	for my $p (@$res) { $config{$p->[0]}{ENCODE1} = $res_by_id{$p->[1]} }

	# get ENCODE2 settings
	$res  = $dbh->selectall_arrayref("SELECT rep_recorder_conf_id,rep_value from recorderparameter where rep_name like 'ENCODE2_RESOLUTION' order by rep_recorder_conf_id,rep_name");
	for my $p (@$res) { $config{$p->[0]}{ENCODE2} = $res_by_id{$p->[1]} }

	# get ENCODE3 settings
	$res  = $dbh->selectall_arrayref("SELECT rep_recorder_conf_id,rep_value from recorderparameter where rep_name like 'ENCODE3_RESOLUTION' order by rep_recorder_conf_id,rep_name");
	for my $p (@$res) { $config{$p->[0]}{ENCODE3} = $res_by_id{$p->[1]} }
}

# Setup %res_by_id and %id_by_res by reading VideoResolution table
sub read_resolution
{
	$res_by_id{'0'} = 'None';		# special case not in database
	$id_by_res{'None'} = '0';

	$res = $dbh->selectall_arrayref("SELECT vrn_identifier,vrn_name FROM VideoResolution");
	for my $p (@$res) {
        # cleanup names for clarity
		$p->[1] =~ s/\bMXF //;
		$p->[1] =~ s/Avid //;

		# setup resolution id and name hashes
		$res_by_id{$p->[0]} = $p->[1];
		$id_by_res{$p->[1]} = $p->[0];
	}
}

# To help debugging, a snapshot of the VideoResolution table is below:
# 0 is not listed but indicates not used
__DATA__
 vrn_identifier |           vrn_name            
----------------+-------------------------------
             10 | Uncompressed UYVY
             12 | Uncompressed UYVY MXF OP_ATOM
             20 | DV25 Raw
             22 | DV25 MXF OP-ATOM
             24 | DV25 Quicktime
             30 | DV50 Raw
             32 | DV50 MXF OP-ATOM
             34 | DV50 Quicktime
             40 | DVCPRO-HD Raw
             42 | DVCPRO-HD MXF OP-ATOM
             44 | DVCPRO-HD Quicktime
             50 | Avid MJPEG 2:1 MXF OP-ATOM
             52 | Avid MJPEG 3:1 MXF OP-ATOM
             54 | Avid MJPEG 10:1 MXF OP-ATOM
             56 | Avid MJPEG 10:1m MXF OP-ATOM
             58 | Avid MJPEG 15:1s MXF OP-ATOM
             60 | Avid MJPEG 20:1 MXF OP-ATOM
             70 | IMX30 MXF OP-ATOM
             72 | IMX40 MXF OP-ATOM
             74 | IMX50 MXF OP-ATOM
             80 | IMX30 MXF OP-1A
             82 | IMX40 MXF OP-1A
             84 | IMX50 MXF OP-1A
            100 | VC3-36p MXF OP-ATOM
            102 | VC3-120i MXF OP-ATOM
            104 | VC3-185i MXF OP-ATOM
            106 | VC3-120p MXF OP-ATOM
            108 | VC3-185p MXF OP-ATOM
            120 | XDCAMHD422 Raw
            124 | XDCAMHD422 Quicktime
            200 | MPEG2 for DVD
            210 | MPEG4 Quicktime
            300 | MP3 Audio only
            400 | Vision Cuts
