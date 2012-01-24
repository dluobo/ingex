#
# $Id: Makefile,v 1.21 2012/01/24 15:17:05 john_f Exp $
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

# Set environment variable BMD_HARDWARE_INCLUDE if building bmd_sdi
# e.g. export BMD_HARDWARE_INCLUDE='/home/ingex/DeckLink/Blackmagic DeckLink SDK 8.6/Linux/include'

SDI_PROGS = dvs_dummy testgen
ifdef DVS_HARDWARE_INCLUDE
  SDI_PROGS += dvs_sdi
else
  $(warning dvs_sdi will not be built)
endif
ifdef BMD_HARDWARE_INCLUDE
  SDI_PROGS += bmd_sdi
endif
#$(warning Building $(SDI_PROGS))

CC = gcc
CPPFLAGS = -DDEBUG -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -DUSE_FFMPEG -I../../common -I../common -I../database/src
CXXFLAGS = -Wall -Wextra -g -O3 -mmmx -msse2

LIBS = -lstudiocommon -lcommon -lYUVlib
LIBPATHS = -L../common -L../../common

FFMPEG_LIBS = -lavformat -lavcodec -lswscale -lavutil -lz -lbz2 -lmp3lame -lx264 -lfaac -lfaad -lm -lpthread

DVS_DUMMY_INCLUDE = -I../../common/tools/dvs_hardware/dummy_include
DVS_DUMMY_LIB = ../../common/tools/dvs_hardware/libdvsoem_dummy.a

PROGS = $(SDI_PROGS) nexus_save nexus_xv nexus_stats nexus_web system_info_web nexus_multicast nexus_set_source 

.PHONY: all
all: $(PROGS)

dvs_sdi: dvs_sdi.cpp
	$(LINK.cpp) $(DVS_HARDWARE_INCLUDE) $< $(DVS_HARDWARE_LIB) $(LIBS) $(LIBPATHS) -lpthread $(FFMPEG_LIBS) -o $@

dvs_dummy.o: dvs_sdi.cpp
	$(COMPILE.cpp) $(DVS_DUMMY_INCLUDE) $< -o $@

dvs_dummy: dvs_dummy.o $(DVS_DUMMY_LIB)
	$(LINK.o) $^ -lstdc++ $(LIBPATHS) $(LIBS) -lpthread $(FFMPEG_LIBS) -o $@

bmd_sdi: bmd_sdi.cpp
	$(LINK.cpp) -I "$(BMD_HARDWARE_INCLUDE)" $< $(LIBS) $(LIBPATHS) -lpthread $(FFMPEG_LIBS) -lDeckLinkAPI -o $@

testgen: testgen.o
	$(LINK.o) $< -lstdc++ $(LIBPATHS) $(LIBS) -lpthread -o $@

nexus_stats: nexus_stats.o
	$(LINK.o) $< -lstdc++ $(LIBPATHS) $(LIBS) -lpthread -o $@

nexus_web: nexus_web.cpp
	@if [ -r /usr/include/shttpd.h -o -r /usr/local/include/shttpd.h ] ; then set -x ; g++ $(CPPFLAGS) nexus_web.cpp $(LIBS) $(LIBPATHS) -lpthread -o nexus_web -lshttpd -ldl ; else echo "shttpd.h not found - nexus_web will not be built"; echo ; fi

system_info_web: system_info_web.cpp
	@if [ -r /usr/include/shttpd.h -o -r /usr/local/include/shttpd.h ] ; then set -x ; $(LINK.cpp) system_info_web.cpp -o system_info_web -lshttpd -ldl -lpthread ; else echo "shttpd.h not found - system_info_web will not be built"; echo ; fi

nexus_set_source: nexus_set_source.o
	$(LINK.o) $< -lstdc++ $(LIBPATHS) $(LIBS) -lpthread -o $@

nexus_save: nexus_save.o
	$(LINK.o) $< -lstdc++ $(LIBPATHS) $(LIBS) $(FFMPEG_LIBS) -lpthread -o $@

nexus_multicast: nexus_multicast.o multicast_compressed.o
	$(LINK.o) $^ -lstdc++ $(LIBPATHS) $(LIBS) $(FFMPEG_LIBS) -o $@

nexus_xv: nexus_xv.o
	$(LINK.o) $< -o nexus_xv -lstdc++ $(LIBPATHS) $(LIBS) -L/usr/X11R6/lib -L/usr/X11R6/lib64 -lXv

clean:
	rm -f core *.o $(PROGS)

