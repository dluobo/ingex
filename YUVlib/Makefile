#
# $Id: Makefile,v 1.2 2011/02/18 16:08:53 john_f Exp $
#
# Makefile for building YUVlib
#
# Copyright (C) 2005-2008 British Broadcasting Corporation.  All Rights Reserved.
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

INSTALL_PREFIX ?= /usr/local

CC=gcc
OPT=-O3 -mmmx -msse2
INCLUDES=-I/usr/local/include/ffmpeg -I/usr/include/freetype2 -I/usr/include/fontconfig
CFLAGS=-Wall -W -Wno-unused-parameter -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE $(OPT) $(INCLUDES)

ifndef DISABLE_FFMPEG

	FFMPEG_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg && echo $$f && break; test -e $$f/libavcodec && echo $$f && break; done)
	ifneq "$(FFMPEG_INCLUDE_PATH)" ""
		CFLAGS += -DHAVE_FFMPEG
	endif
	
	# Check for old-style FFMPEG header locations and if needed add -DFFMPEG_OLD_INCLUDE_PATHS
	FFMPEG_OLDSTYLE_INCLUDE := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/avcodec.h && echo $$f && break; done)
	ifneq "$(FFMPEG_OLDSTYLE_INCLUDE)" ""
		CFLAGS += -DFFMPEG_OLD_INCLUDE_PATHS
	endif

endif	

HEADERS=$(filter-out YUV_DirectShow.h,$(wildcard *.h))
CFILES=$(wildcard *.c)
OBJECTS=$(CFILES:%.c=%.o)

LIBYUVLIB=libYUVlib.a

.PHONY: all
all: $(LIBYUVLIB)($(OBJECTS))

$(LIBYUVLIB)(%.o): %.o
	ar rcv $(LIBYUVLIB) $<

.PHONY: install
install: $(LIBYUVLIB)
	mkdir -p $(INSTALL_PREFIX)/lib
	cp $(LIBYUVLIB) $(INSTALL_PREFIX)/lib
	mkdir -p $(INSTALL_PREFIX)/include/yuvlib
	cp $(HEADERS) $(INSTALL_PREFIX)/include/yuvlib/

.PHONY: uninstall
uninstall:
	rm -f $(INSTALL_PREFIX)/lib/$(LIBYUVLIB)
	rm -fR $(INSTALL_PREFIX)/include/yuvlib

.PHONY: clean
clean:
	rm -f *.o *.a



