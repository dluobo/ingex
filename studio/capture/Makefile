#
# $Id: Makefile,v 1.16 2010/08/12 16:27:08 john_f Exp $
#
# Makefile for building ingex server and utilities.
#
# Copyright (C) 2005 - 2010 British Broadcasting Corporation
# All rights reserved
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

# dvs_sdk_paths.mk searches for sdk and sets DVS_HARDWARE_INCLUDE and DVS_HARDWARE_LIB
# search is overridden by setting DVSSDK
include ../../common/dvs_sdk_paths.mk

ifdef DVS_HARDWARE_INCLUDE
  SDI_PROGS=dvs_sdi dvs_dummy testgen
else
  $(warning dvs_sdi will not be built)
  SDI_PROGS=dvs_dummy testgen
endif

CPPFLAGS = -DDEBUG -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -DUSE_FFMPEG -I../../common -I../../common/YUVlib -I../common -I../database/src
CXXFLAGS = -Wall -g -O3
TARGET_ARCH = -mmmx -msse2

LIBS = -lstudiocommon -lcommon -lYUVlib
LIBPATHS = -L../common -L../../common -L../../common/YUVlib

FFMPEG_LIBS = -lavformat -lavcodec -lswscale -lavutil -lz -lbz2 -lmp3lame -lx264 -lfaac -lfaad -lm -lpthread

PROGS = $(SDI_PROGS) nexus_save nexus_xv nexus_stats nexus_web system_info_web nexus_multicast nexus_set_source 

.PHONY: all
all: $(PROGS)

dvs_sdi: dvs_sdi.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) $(DVS_HARDWARE_INCLUDE) $< $(DVS_HARDWARE_LIB) $(LIBS) $(LIBPATHS) -lpthread $(FFMPEG_LIBS) -o dvs_sdi

dvs_dummy: dvs_sdi.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) $(DUMMY_HARDWARE_INCLUDE) $< $(DUMMY_HARDWARE_LIB) $(LIBS) $(LIBPATHS) -lpthread $(FFMPEG_LIBS) -o dvs_dummy

testgen: testgen.o
	$(CXX) $(TARGET_ARCH) $< $(LIBPATHS) $(LIBS) -lpthread -o $@

nexus_stats: nexus_stats.o
	$(CXX) $(TARGET_ARCH) $< $(LIBPATHS) $(LIBS) -lpthread -o $@

nexus_web: nexus_web.cpp
	@if [ -r /usr/include/shttpd.h -o -r /usr/local/include/shttpd.h ] ; then set -x ; g++ $(CPPFLAGS) nexus_web.cpp $(LIBS) $(LIBPATHS) -lpthread -o nexus_web -lshttpd -ldl ; else echo "shttpd.h not found - nexus_web will not be built"; echo ; fi

system_info_web: system_info_web.cpp
	@if [ -r /usr/include/shttpd.h -o -r /usr/local/include/shttpd.h ] ; then set -x ; g++ $(CPPFLAGS) system_info_web.cpp -o system_info_web -lshttpd -ldl -lpthread ; else echo "shttpd.h not found - system_info_web will not be built"; echo ; fi

nexus_set_source: nexus_set_source.o
	$(CXX) $(TARGET_ARCH) $< $(LIBPATHS) $(LIBS) -lpthread -o $@

nexus_save: nexus_save.o
	$(CXX) $(TARGET_ARCH) $< $(LIBPATHS) $(LIBS) $(FFMPEG_LIBS) -lpthread -o $@

nexus_multicast: nexus_multicast.o multicast_compressed.o
	$(CXX) $(TARGET_ARCH) nexus_multicast.o multicast_compressed.o -o $@ $(LIBPATHS) $(LIBS) -L../../common/YUVlib -lYUVlib $(FFMPEG_LIBS)

nexus_xv: nexus_xv.o
	$(CXX) $(TARGET_ARCH) $< -o nexus_xv $(LIBPATHS) $(LIBS) -L/usr/X11R6/lib -L/usr/X11R6/lib64 -lXv

clean:
	rm -f core *.o $(PROGS)

