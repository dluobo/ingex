#!/usr/bin/perl -w
# $Id: check_mxf_umid.pl,v 1.1 2007/09/11 14:08:45 stuart_hc Exp $
use strict;

my $mxffile;
if (@ARGV) {
	$mxffile = shift;
} else {
	die "no mxf file specified";
}

check_header_metadata($mxffile);

sub check_header_metadata
{
	my ($mxffile) = @_;
	open(PIPE, "MXFDump $mxffile |") or die "MXFDump failed: $!";
	my $s = '';
	while (<PIPE>) {
		$s .= $_;
	}
	close(PIPE);

	# Check all MXFMaterialPackage PackageUIDs (should only be one)
	while (1) {
		last unless $s =~ /MXFMaterialPackage.{1,800}PackageUID.{36,42}  ([a-f0-9 ]{48}).{28,32}  ([a-f0-9 ]{48})/smg;
		my $umid = $1 . $2;
		my $is_smpte = smpte_umid($umid);
		printf "Material UMID (%s) = $umid\n", $is_smpte ? "SMPTE" : "AVID ";
	}

	# Check all MXFSourcePackage PackageUIDs (should only be one)
	while (1) {
		last unless $s =~ /MXFSourcePackage.{1,800}PackageUID.{36,42}  ([a-f0-9 ]{48}).{28,32}  ([a-f0-9 ]{48})/smg;
		my $umid = $1 . $2;
		my $is_smpte = smpte_umid($umid);
		printf "Source   UMID (%s) = $umid\n", $is_smpte ? "SMPTE" : "AVID ";
	}
}

sub smpte_umid
{
	my ($umid) = @_;
	return 1 if ($umid =~ /^\s*06 0a 2b 34/);
	return 0;
}
