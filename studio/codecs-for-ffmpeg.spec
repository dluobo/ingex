Summary: H.264 AAC MP3 AC-3 codecs
Name: codecs-for-ffmpeg
Version: 20081215
Release: 2
License: GPL
Group: System Environment/Daemons
Source: x264-snapshot-%version-2245.tar.bz2
Source1: faad2-2.7.tar.gz
Source2: faac-1.28.tar.gz
Source3: lame-3.96.1.tar.gz
Url: svn://svn.mplayerhq.hu/ffmpeg/trunk
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: autoconf nasm
Provides: libx264.a

%description
H.264, AAC, MP3 and AC-3 codecs which can be used as-is or when building
ffmpeg.

x264 snapshot from ftp://ftp.videolan.org/pub/videolan/x264/snapshots/

%prep
rm -rf $RPM_BUILD_ROOT

# unpack the ffmpeg source
%setup -q -c -n codecs
%setup -q -T -D -a 1 -n codecs
%setup -q -T -D -a 2 -n codecs
%setup -q -T -D -a 3 -n codecs

%build
cd x264-snapshot-%version-2245
./configure --prefix=/usr --enable-static --enable-pthread --disable-shared
make -j 3

cd ..
cd lame-3.96.1
./configure --prefix=/usr --enable-static --disable-shared
make -j 3
make install DESTDIR=$RPM_BUILD_ROOT

cd ..
cd faad2-2.7
autoreconf -vif
./configure --prefix=/usr --enable-static --disable-shared
make
make install DESTDIR=$RPM_BUILD_ROOT

cd ..
cd faac-1.28
grep -P '\r' -l -r . | xargs dos2unix
./bootstrap
./configure --prefix=/usr --enable-static --disable-shared
make
make install DESTDIR=$RPM_BUILD_ROOT


%install
cd x264-snapshot-%version-2245
make install DESTDIR=$RPM_BUILD_ROOT
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
/usr/man/

%changelog
* Mon May 17 2010 Philip de Nier 20081215-1
- Updated faac to version 1.28
- Updated faad2 to version 2.7
- Removed faad2 patch

* Mon Dec 15 2008 Stuart Cunningham <stuart_hc@users.sourceforge.net> 20081215
- Updated x264 version to 20081215
- No dynamic libs produced (no .so's) to reduce dependencies in other packages

* Fri Oct 10 2008 Stuart Cunningham <stuart_hc@users.sourceforge.net> 20081009
- Updated x264 version to match ffmpeg revision as of 2008-10-10
- Removed liba52 since it is no longer used by ffmpeg

* Tue Feb 13 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> 2245
- Updated x264 version to match ffmpeg revision as of 2008-06-11

* Tue Feb 13 2007 Stuart Cunningham <stuart_hc@users.sourceforge.net> 7964
- Initial version
