Required standard packages
  autoconf
  automake
  fontconfig-devel
  freetype2-devel
  kernel sources
  libbz2-devel
  libjpeg-devel
  libpqxx-devel
  libtool
  libuuid-devel
  libXerces-c-devel
  portaudio-devel
  postgresql-devel
  postgresql-server
  SDL-devel
  wxGTK-devel

Extra packages produced by the Ingex team for convenience
  codecs-for-ffmpeg
  ffmpeg-DNxHD-h264-aac
  shttpd

To build ingex-studio using installed libMXF...

> export USE_INSTALLED_LIBMXF=yes
> make

To make the ffmpeg RPM...

Put patches and ffmpeg-0.5.tar.bz2 in /usr/src/packages/SOURCES

> rpmbuild -bb ffmpeg-DNxHD-h264-aac.spec


