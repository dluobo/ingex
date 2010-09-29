#
# $Id: Makefile,v 1.5 2010/09/29 09:01:14 john_f Exp $
#
# Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
# Author: Philip de Nier
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

include ../../common/libmxf.mk

C_INGEX_PLAYER_DIR = ../ingex_player
INGEX_PLAYER_DIR = ../IngexPlayer
COMMON_DIR = ../../common
YUVLIB_DIR = ../../YUVlib


ifneq ($(MAKECMDGOALS),clean)
	ifneq ($(MAKECMDGOALS),install)
		CHECK_LIB_AVAILABILITIES=1
	endif
endif


ifdef CHECK_LIB_AVAILABILITIES

# dvs_sdk_paths.mk searches for sdk and sets DVS_HARDWARE_INCLUDE and DVS_HARDWARE_LIB
# search is overridden by setting DVSSDK
include ../../common/dvs_sdk_paths.mk

ifndef DVS_HARDWARE_INCLUDE
  $(warning no DVS available - set the DVSSDK environment variable to build with DVS hardware support)
endif

SHTTPD_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/shttpd.h && echo $$f && break; done)
ifeq "$(SHTTPD_PATH)" ""
$(error no shttpd library found)
endif

ifndef DISABLE_FFMPEG
	FFMPEG_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg && echo $$f && break; test -e $$f/libavcodec && echo $$f && break; done)
	ifeq "$(FFMPEG_INCLUDE_PATH)" ""
$(warning no FFMPEG library found - DV stream connector not available)
	endif
endif

PORTAUDIO_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/portaudio.h && echo $$f && break; done)
ifneq "$(PORTAUDIO_INCLUDE_PATH)" ""
HAVE_PORTAUDIO = 1
else
$(warning: no PortAudio library found - No PC audio playback available)
endif


endif


CFLAGS = -Wall -W -Wno-unused-parameter -D_THREAD_SAFE
OPT = -O3 -mmmx -msse2

INCLUDES = -I. -I$(C_INGEX_PLAYER_DIR) -I$(INGEX_PLAYER_DIR) \
	$(LIBMXF_INCLUDE) $(ARCHIVEMXFINFO_INCLUDE) $(MXFREADER_INCLUDE) \

LIBS = -L. -L$(INGEX_PLAYER_DIR) -lIngexPlayer \
	$(ARCHIVEMXFINFO_LIB) $(MXFREADER_LIB) $(LIBMXF_LIB) \
	-L$(COMMON_DIR) -lcommon \
	-L$(YUVLIB_DIR) -lYUVlib \
	-lshttpd -lX11 -lXv -lpthread -lfreetype -lfontconfig


ifdef DVS_HARDWARE_INCLUDE
  LIBS += $(DVS_HARDWARE_LIB)
endif

ifdef FFMPEG_INCLUDE_PATH

	CFLAGS += -DHAVE_FFMPEG

	# Check for old-style FFMPEG header locations and if needed add -DFFMPEG_OLD_INCLUDE_PATHS
	FFMPEG_OLDSTYLE_INCLUDE := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/avcodec.h && echo $$f && break; done)
	ifneq "$(FFMPEG_OLDSTYLE_INCLUDE)" ""
		CFLAGS += -DFFMPEG_OLD_INCLUDE_PATHS
	endif

	# Check for swscale
	FFMPEG_SWSCALE_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/swscale.h && echo YES && break; test -e $$f/libswscale/swscale.h && echo YES && break; done)
	ifeq "$(FFMPEG_SWSCALE_INCLUDE_PATH)" "YES"
		CFLAGS += -DHAVE_FFMPEG_SWSCALE

		FFMPEG_SWSCALE_LIB := $(shell for f in /usr/local/lib /usr/lib; do test -e $$f/libswscale.a && echo YES && break; done)
		ifeq "$(FFMPEG_SWSCALE_LIB)" "YES"
			LIBS += -lswscale
		endif
	endif

	# Use the following alternative link line for ffmpeg rpm installed ffmpeg
	FFMPEG_RPM := $(shell rpm -q ffmpeg-h264-aac > /dev/null && echo YES)
	FFMPEG_DNXHD_RPM := $(shell rpm -q ffmpeg-DNxHD-h264-aac > /dev/null && echo YES)
	ifeq "$(FFMPEG_RPM)" "YES"
		LIBS += -lavformat -lavcodec -lavutil -lswscale -lz -lbz2 -lmp3lame -lx264 -la52 -lfaac -lfaad -lm
	else
		ifeq "$(FFMPEG_DNXHD_RPM)" "YES"
			LIBS += -lavformat -lavcodec -lavutil -lswscale -lz -lbz2 -lmp3lame -lx264 -lfaac -lfaad -lm
			CFLAGS += -DHAVE_DNXHD
		else
			LIBS += -lavformat -lavcodec -lavutil -lswscale -lm
		endif
	endif
else
	CFLAGS += -Werror
endif

ifdef HAVE_PORTAUDIO
	INCLUDES += -I$(PORTAUDIO_INCLUDE_PATH)
	LIBS += -lportaudio
	CFLAGS += -DHAVE_PORTAUDIO
endif


CC = g++ -g $(CFLAGS) $(OPT)
AR = ar cr


SOURCES = IngexException.cpp Utilities.cpp Logging.cpp Threads.cpp JSONObject.cpp \
	HTTPServer.cpp HTTPIngexPlayer.cpp HTTPPlayerState.cpp main.cpp
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))


.PHONY: all
all: http_player


.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(CC) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(CC) -c $(INCLUDES) $< -o $@


http_player: $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o $@



.PHONY: clean
clean:
	@rm -f *~ *.a http_player
	@rm -Rf .objs
	@rm -fR .deps


ifneq ($(MAKECMDGOALS),clean)
-include $(DEPENDENCIES)
endif
