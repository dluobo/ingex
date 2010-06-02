#
# $Id: Makefile,v 1.13 2010/06/02 13:06:43 john_f Exp $
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

CC = g++
FLAGS = -Wall -g -O3 -DDEBUG -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
INCLUDES = -I../../common -I../common -I../database/src
OPT = -mmmx -msse2
CFLAGS = -DUSE_FFMPEG $(FLAGS) $(INCLUDES) $(OPT)

LIBS = -lstudiocommon -lcommon
LIBPATHS = -L../common -L../../common

FFMPEG_LIBS = -lavformat -lavcodec -lswscale -lavutil -lz -lbz2 -lmp3lame -lx264 -lfaac -lfaad -lm -lpthread

PROGS = $(SDI_PROGS) nexus_save nexus_xv nexus_stats nexus_web system_info_web nexus_multicast nexus_set_source 

.PHONY: all
all: $(PROGS)

dvs_sdi: dvs_sdi.c
	g++ $(CFLAGS) $(DVS_HARDWARE_INCLUDE) dvs_sdi.c $(DVS_HARDWARE_LIB) $(LIBS) $(LIBPATHS) -lpthread -o dvs_sdi $(FFMPEG_LIBS)

dvs_dummy: dvs_sdi.c
	g++ $(CFLAGS) $(DUMMY_HARDWARE_INCLUDE) dvs_sdi.c $(DUMMY_HARDWARE_LIB) $(LIBS) $(LIBPATHS) -lpthread -o dvs_dummy $(FFMPEG_LIBS)

testgen: testgen.c
	g++ $(CFLAGS) testgen.c $(LIBS) $(LIBPATHS) -lpthread -o testgen

nexus_stats: nexus_stats.c
	g++ $(CFLAGS) nexus_stats.c $(LIBS) $(LIBPATHS) -lpthread -o nexus_stats

nexus_web: nexus_web.c
	@if [ -r /usr/include/shttpd.h -o -r /usr/local/include/shttpd.h ] ; then set -x ; g++ $(CFLAGS) nexus_web.c $(LIBS) $(LIBPATHS) -lpthread -o nexus_web -lshttpd -ldl ; else echo "shttpd.h not found - nexus_web will not be built"; echo ; fi

system_info_web: system_info_web.c
	@if [ -r /usr/include/shttpd.h -o -r /usr/local/include/shttpd.h ] ; then set -x ; g++ $(CFLAGS) system_info_web.c -o system_info_web -lshttpd -ldl -lpthread ; else echo "shttpd.h not found - system_info_web_web will not be built"; echo ; fi

nexus_set_source: nexus_set_source.c
	g++ $(CFLAGS) nexus_set_source.c $(LIBS) $(LIBPATHS) -lpthread -o nexus_set_source

nexus_save: nexus_save.o
	$(LINK.cpp) nexus_save.o -o nexus_save $(LIBS) $(LIBPATHS) $(FFMPEG_LIBS)

nexus_multicast: nexus_multicast.c
	g++ $(CFLAGS) nexus_multicast.c -o nexus_multicast $(LIBS) $(LIBPATHS) -L../../common/YUVlib -lYUVlib $(FFMPEG_LIBS)

nexus_xv: nexus_xv.c
	g++ $(CFLAGS) nexus_xv.c -o nexus_xv $(LIBS) $(LIBPATHS) -L/usr/X11R6/lib -L/usr/X11R6/lib64 -lXv

clean:
	rm -f core *.o dvs_sdi $(PROGS)

