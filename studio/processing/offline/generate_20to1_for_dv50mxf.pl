#!/usr/bin/perl -w
use File::Basename;
use strict;

# given an input DV50 video MXF file
# - determine the corresponding audio MXF files
# - extract tape anme, clip name etc
# - check all required files are present (using locators?)
# - transcode DV50 to MJPEG bitstream
# - extract raw PCM audio
# - create set of new MXF files representing new package with 20:1 video


# e.g. test_write_avid_mxf --prefix output --clip clipname --tape spool1 --dv50 ../writeaviddv50/outdv50.dv --pcm a.pcm --pcm b.pcm

# test_write_avid_mxf --prefix output --clip clipname --tape spool1 --mjpeg t.mjpeg --res 20:1 --pcm /dev/zero --pcm /dev/zero

my $vfile;
if (@ARGV) {
	$vfile = shift;
} else {
	#$vfile = '/video1/mxf/20061213_15330909_card0_1_v1.mxf';
	die "no arg";
}
my $pattern = $vfile;
$pattern =~ s/._v1\./?_a?./;
my @audio_mxf = glob($pattern);

#testing
#my $vfile = 'outdv50.mxf';
#my @audio_mxf = qw( output_a1.mxf output_a2.mxf );

# extract metadata
my $start_pos = extract_header_metadata($vfile);

# extract and transcode audio and video
my $tmp_mjpeg = tmpname($vfile, 'mjpeg');
print "extracting and transcoding $vfile -> $tmp_mjpeg\n";
#system("dv50mxf_to_mjpeg $vfile $tmp_mjpeg");
if ($? != 0) { die "dv50mxf_to_mjpeg failed: $?\n" }

my @audio_pcm;
for my $afile (@audio_mxf) {
	my $tmp_pcm = tmpname($afile, 'pcm');
	push @audio_pcm, $tmp_pcm;
	print "extracting audio $afile -> $tmp_pcm\n";
	system("extract_pcm_audio $afile $tmp_pcm");
	if ($? != 0) { die "extract_pcm_audio failed: $?\n" }
}

# prepare final MXF creation
my $pcm_file_args = '';
for my $afile (@audio_pcm) {
	$pcm_file_args .= "--pcm $afile ";
}
#my $cmd = "test_write_avid_mxf --prefix output --clip clipname --tape spool1 --start-pos $start_pos --mjpeg $tmp_mjpeg --res 20:1 $pcm_file_args";
my $cmd = "test_write_avid_mxf --prefix output --clip clipname --tape spool1 --mjpeg $tmp_mjpeg --res 20:1 $pcm_file_args";
print "running $cmd\n";
system($cmd);
if ($? != 0) { die "$cmd failed: $?\n" }

# clean up temp files
unlink($tmp_mjpeg);
unlink(@audio_pcm);

sub extract_header_metadata
{
	my ($mxffile) = @_;
	open(PIPE, "MXFDump $mxffile |") or die "MXFDump failed: $!";
	my $s = '';
	while (<PIPE>) {
		$s .= $_;
	}
	close(PIPE);

	my ($start_pos) = ($s =~ /StartPosition.{1,100}\]\s+0\s+(.{24}).{1,500}MXFCDCIEssence/sm);
	$start_pos =~ s/\s+//g;
	my $decimal = hex($start_pos);
	print "start_pos = $start_pos (in dec $decimal)\n";

	#my ($tape_name) = ($s =~ /(k = Name.{300,780}k = Descriptor)/sm);
	#$tape_name =~ s/k = Tracks.*//;
	#print "Name = $tape_name\n";
	return $decimal;
}

# return a /video1/tmp/ file
sub tmpname
{
	my ($file, $ext) = @_;
	return '/video1/tmp/' . basename($file) . '.' . $ext;
}
