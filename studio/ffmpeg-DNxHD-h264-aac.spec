# To build the swpat restricted version, set the environment variable
# BUILD_FFMPEG_SWPAT_RESTRICTED=1
# otherwise a normal build is made with all useful codecs
%define build_swpat_restricted %(if test -z "$BUILD_FFMPEG_SWPAT_RESTRICTED" ; then echo 0 ; else echo $BUILD_FFMPEG_SWPAT_RESTRICTED ; fi)

%if %{build_swpat_restricted}
Summary: FFmpeg library with PCM, DV and MJPEG codecs
Name: ffmpeg-swpat-restricted
%else
Summary: FFmpeg library with DNxHD(VC-3) DVCPRO-HD H.264 AAC MP3 A52(AC-3) codecs
Name: ffmpeg-DNxHD-h264-aac
%endif
Version: 0.5
Release: 10
License: GPL
Group: System Environment/Daemons
Source: ffmpeg-%{version}.tar.bz2
Patch0: ffmpeg_fix_imlib2.patch
Patch1: ffmpeg_default_bitrate.patch
Patch2: ffmpeg_tref_quicktime.patch
Patch3: ffmpeg-DVCPROHD-0.5.patch
Patch4: ffmpeg_quicktime_DVCPRO-HD.patch
Patch5: ffmpeg-0.5-nonlinear-quant.patch
Patch6: ffmpeg-0.5-dnxhd-interlaced-parse.patch
Patch7: ffmpeg-0.5-archive-mxf.patch
Patch8: ffmpeg-0.5-dnxhd-avid-nitris.patch
Patch9: ffmpeg-0.5-dnxhd-sst.patch
Patch10: ffmpeg-0.5-quicktime-aspect-ratio.patch
Patch11: ffmpeg-0.5-imx-frame-too-large-error.patch
Patch12: ffmpeg-0.5-mpeg2-closed-gop-b-frames.patch
Patch13: ffmpeg-0.5-x264-require-check.patch
Patch14: ffmpeg-0.5-dnxhd-fix-interlaced-decoding-issue-1753.patch
Url: http://www.ffmpeg.org/download.html
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: autoconf nasm
Provides: ffmpeg libavcodec.a libavformat.a libavutil.a

%if %{build_swpat_restricted}
%description
FFmpeg library with limited codec support to avoid software patent
encumbered codecs.
Tarball taken from http://www.ffmpeg.org/releases/ffmpeg-0.5.tar.bz2
%else
%description
FFmpeg library with DNxHD(VC-3), DVCPRO-HD, H.264, AAC, MP3, A52(AC-3) codecs.
Tarball taken from http://www.ffmpeg.org/releases/ffmpeg-0.5.tar.bz2

Patched to support:
  - DVCPRO-HD (SMPTE SMPTE 370M) support (1080i50, 1080i60, 720p60)
  - Writing of timecode track in Quicktime wrapped files (.mov)
  - MPEG-2 non-linear quantizer for 2 <= qscale <= 31
  - Various DNxHD fixes
  - Avid Nitris decoder compatibility
  - Ingex Archive MXF file support
%endif

%prep
rm -rf $RPM_BUILD_ROOT

# unpack the ffmpeg source
%setup -q -n ffmpeg-%{version}

# Patch code to fix any build time errors
%patch0
%patch1
%patch2
%patch3
%patch4
%patch5
%patch6
%patch7
%patch8
%patch9
%patch10
%patch11
%patch12
%patch13
%patch14

%build
%if %{build_swpat_restricted}

# Restrict encoders and decoders to pcm_s16le, pcm_s16be, dvvideo, mjpeg
# avoiding software patent encumbered codecs
./configure --prefix=/usr --enable-pthreads --disable-demuxer=ogg --enable-swscale --disable-encoders --disable-decoders --disable-parser=h264 --enable-gpl --enable-encoder=pcm_s16le --enable-encoder=pcm_s16be --enable-encoder=dvvideo --enable-encoder=mjpeg --enable-decoder=pcm_s16le --enable-decoder=pcm_s16be --enable-decoder=dvvideo --enable-decoder=mjpeg

%else

# build ffmpeg normally enabling all useful codecs for television post production
./configure --prefix=/usr --enable-pthreads --disable-demuxer=ogg --enable-swscale --enable-libx264 --enable-libmp3lame --enable-gpl --enable-libfaac --enable-libfaad

%endif

make -j 3

%install
make install DESTDIR=$RPM_BUILD_ROOT
make install-man DESTDIR=$RPM_BUILD_ROOT
%ifarch x86_64
mv $RPM_BUILD_ROOT/usr/lib $RPM_BUILD_ROOT/usr/lib64
%endif

%if %{build_swpat_restricted}
# remove unnecessary libx264 presets
rm -rf $RPM_BUILD_ROOT/usr/share/ffmpeg/libx264*
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%ifarch x86_64
/usr/lib64/
%else
/usr/lib/
%endif
/usr/include/
/usr/bin/
/usr/share/

%changelog
* Tue Jul 05 2011 Philip de Nier, 0.5-10
- Add ffmpeg patch (ffmpeg git commit 1307463d5259a80fdbb43f50434eb9ba37c50d30)
  to fix decoding of interlaced dnxhd files produced by Avid. Search for
  DNxHD_Compliance_Issue_To_Licensees-1.doc for more info
- Disable shared libraries build to avoid linking issue with static codec
  libraries (codecs-for-ffmpeg) and to limit changes prior to first Ingex
  release

* Mon Jan 10 2011 Philip de Nier 0.5-9
- Add ffmpeg patch from 2009-09-02 to check for x264_encoder_encode instead of
  x264_encoder_open because a version suffix has been added to the
  x264_encoder_open function

* Thu Dec 30 2010 Philip de Nier 0.5-8
- Add patch to fix decode issue with MPEG-2 GOPs starting with B-frames.
  This issue was fixed in the FFmpeg mainline on 2009-05-15
  https://roundup.ffmpeg.org/issue891

* Wed Sep 12 2010 Stuart Cunningham 0.5-7
- Add build-time switch to build swpat restricted version of ffmpeg
  named ffmpeg-swpat-restricted
- Turn on shared libraries for both builds so ffmpeg libs can be
  changed without recompiling applications that depend on ffmpeg

* Wed Aug 11 2010 Philip de Nier 0.5-6
- Comment out 'encoded frame too large' log message for IMX

* Thu Jun 10 2010 Philip de Nier 0.5-5
- Fix DV aspect ratio in bitstream (DISP)
- Added ffmpeg-0.5-quicktime-aspect-ratio.patch
	- Added 'pasp' atom to quicktime to define the pixel/sample aspect ratio
	- Not adjusting visual/display width using sample aspect ratio
	- Setting visual/display width to 1920/1280 for DVCProHD
* Mon May 10 2010 Philip de Nier 0.5-4
- Added patch implementing MPEG-2 non-linear quantizer (qscale >= 2) to fix
  CBR failure for complex pictures
- Added patch fixing interlaced DNxHD parsing
- Added Ingex Archive MXF patch
- Added patch fixing SST field in DNxHD bitstream
- Added patch to support Avid Nitris decoder

* Mon Mar 16 2009 Stuart Cunningham <stuart_hc@users.sourceforge.net> 0.5
- Update to official 0.5 release (branched from revision 17727)
- Restore DVCPRO-HD support using revised Dan Maas patch

* Tue Dec 16 2008 Stuart Cunningham <stuart_hc@users.sourceforge.net> 16156
- Update to 16156 (includes some more efficient thread usage for DV codec)
- Remove separate DNxHD raw parser since this is now in mainline FFmpeg

* Fri Oct 10 2008 Stuart Cunningham <stuart_hc@users.sourceforge.net> 15593-2
- Turn on swscale support with --enable-swscale
- Add DNxHD raw parser patch for reading from raw DNxHD frames

* Fri Oct 10 2008 Stuart Cunningham <stuart_hc@users.sourceforge.net> 15593
- Updated to revision 15593 of ffmpeg
- This release include in-tree DVCPRO-HD support

* Mon Jun 30 2008 Stuart Cunningham <stuart_hc@users.sourceforge.net> 14032
- Updated to revision 14032 of ffmpeg
- Added timecode track for quicktime wrapping patch

* Thu Oct  4 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> 13743
- Updated to revision 13743 of ffmpeg
- DVCPRO-HD support removed due to out-of-tree patch no longer working

* Thu Oct  4 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> 11056
- Added DVCPRO-HD codec from Dan Maas

* Thu Oct  4 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> 10666
- Initial version
