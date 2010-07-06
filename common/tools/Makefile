#
# $Id: Makefile,v 1.7 2010/07/06 14:15:13 john_f Exp $
#
# Makefile for building useful command-line tools
#
# Copyright (C) 2005 BBC, author Stuart Cunningham <stuart_hc@users.sourceforge.net>
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

CC = gcc

MXF_LIB = -L../../libMXF/examples/archive/write -L../../libMXF/lib -lwritearchivemxf -lMXF -luuid
YUV_LIB = -L../YUVlib -lYUVlib

INCLUDE = -I.. -I../YUVlib -I../../../libMXF/lib/include

CPPFLAGS = -Wall -W -Wno-unused-parameter -g -DDEBUG -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE $(INCLUDE)
CFLAGS = -Wall -W -Wno-unused-parameter -g -O3
CXXFLAGS = -Wall -W -Wno-unused-parameter -g -O3
TARGET_ARCH = -mmmx -msse2

LIB_COMMON = ../libcommon.a

PROGS = convert_audio convert_10bit_video create_video_test_signal detect_digibeta_dropouts \
	compare_archive_mxf clapperboard_avsync disk_rw_benchmark send_video receive_video \
	create_audio_test_signal dump_vc3

.PHONY: all
all: $(PROGS) dvs_hardware

.PHONY: dvs_hardware
dvs_hardware:
	cd dvs_hardware && $(MAKE)

convert_audio: convert_audio.o $(LIB_COMMON)
	$(CXX) $(CXXFLAGS) $(TARGET_ARCH) -o $@ $< $(LIB_COMMON) -lm

convert_10bit_video: convert_10bit_video.o $(LIB_COMMON)
	$(CXX) $(CXXFLAGS) $(TARGET_ARCH) -o $@ $< $(LIB_COMMON)

create_video_test_signal: create_video_test_signal.o $(LIB_COMMON)
	$(CXX) $(CXXFLAGS) $(TARGET_ARCH) -o $@ $< $(LIB_COMMON)

create_audio_test_signal: create_audio_test_signal.o
	$(CC) $(CXXFLAGS) $(TARGET_ARCH) -o $@ $< -lm

detect_digibeta_dropouts: detect_digibeta_dropouts.o $(LIB_COMMON)
	$(CXX) $(CXXFLAGS) $(TARGET_ARCH) -o $@ $< $(LIB_COMMON) $(YUV_LIB)

compare_archive_mxf: compare_archive_mxf.o $(LIB_COMMON)
	$(CXX) $(CXXFLAGS) $(TARGET_ARCH) -o $@ $< $(LIB_COMMON)

clapperboard_avsync: clapperboard_avsync.o $(LIB_COMMON)
	$(CXX) $(CXXFLAGS) $(TARGET_ARCH) -o $@ $< $(LIB_COMMON) $(MXF_LIB) -lm

send_video: send_video.o $(LIB_COMMON)
	$(CXX) $(CXXFLAGS) $(TARGET_ARCH) -o $@ $< $(LIB_COMMON) $(YUV_LIB) -lpthread

receive_video: receive_video.o $(LIB_COMMON)
	$(CXX) $(CXXFLAGS) $(TARGET_ARCH) -o $@ $< $(LIB_COMMON) $(YUV_LIB) -lpthread -lm

dump_vc3: dump_vc3.o
	$(CC) $(CXXFLAGS) $(TARGET_ARCH) -o $@ $<

clean:
	cd dvs_hardware && $(MAKE) $@
	rm -f $(PROGS)

