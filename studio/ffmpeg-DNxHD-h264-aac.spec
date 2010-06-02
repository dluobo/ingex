Summary: FFmpeg library with DNxHD(VC-3) DVCPRO-HD H.264 AAC MP3 A52(AC-3) codecs
Name: ffmpeg-DNxHD-h264-aac
Version: 0.5
Release: 4
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
Url: http://www.ffmpeg.org/download.html
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: autoconf nasm
Provides: ffmpeg libavcodec.a libavformat.a libavutil.a

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

%build
./configure --prefix=/usr --enable-pthreads --disable-demuxer=ogg --enable-swscale --enable-libx264 --enable-libmp3lame --enable-gpl --enable-libfaac --enable-libfaad
make -j 3

%install
make install DESTDIR=$RPM_BUILD_ROOT
make install-man DESTDIR=$RPM_BUILD_ROOT
%ifarch x86_64
mv $RPM_BUILD_ROOT/usr/lib $RPM_BUILD_ROOT/usr/lib64
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
