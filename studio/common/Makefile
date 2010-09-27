#
# $Id: Makefile,v 1.7 2010/09/27 16:16:18 john_f Exp $
#
# Makefile for building studio common functions
#
# Copyright (C) 2005 - 2010
# British Broadcasting Corporation
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
OPT = -DNDEBUG -O3 -mmmx -msse2
INCLUDES = -I../../common -I../database/src
CFLAGS = -Wall -W -Wno-unused-parameter -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE $(OPT) $(INCLUDES)
CPPFLAGS = -Wall -W -Wno-unused-parameter -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE $(OPT) $(INCLUDES)

FFMPEG_OLDSTYLE_INCLUDE := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/avcodec.h && echo $$f && break; done)
ifneq "$(FFMPEG_OLDSTYLE_INCLUDE)" ""
	CFLAGS+=-DFFMPEG_OLD_INCLUDE_PATHS
	CPPFLAGS+=-DFFMPEG_OLD_INCLUDE_PATHS
endif

FFMPEG_NONLINEAR_PATCH := $(shell ffmpeg -t 0.1 -s 720x576 -pix_fmt uyvy422 -f rawvideo -i /dev/zero -vcodec mpeg2video -flags2 +non_linear_q -qmin 1 -qmax 12 -f mpeg2video -y /dev/null 2>/dev/null && echo nopatch)
ifeq "$(FFMPEG_NONLINEAR_PATCH)" ""
	CFLAGS+=-DFFMPEG_NONLINEAR_PATCH
	CPPFLAGS+=-DFFMPEG_NONLINEAR_PATCH
endif


LIBCSOURCES = \
    mjpeg_compress.c \
    tc_overlay.c

LIBCPPSOURCES = \
    MaterialResolution.cpp \
    ffmpeg_resolutions.cpp \
    ffmpeg_encoder.cpp \
    ffmpeg_encoder_av.cpp \
    XmlTools.cpp

LIBOBJECTS = $(LIBCSOURCES:%.c=%.o) $(LIBCPPSOURCES:%.cpp=%.o)


LIBCOMMON = -L../../common -lcommon
LIBSTUDIOCOMMON = libstudiocommon.a
YUVLIB=-L../../common/YUVlib -lYUVlib

.PHONY: all
all: $(LIBSTUDIOCOMMON) test_ffmpeg_encoder test_ffmpeg_encoder_av yuv422_to_mjpeg yuv422_to_mjpeg_threaded

$(LIBSTUDIOCOMMON): $(LIBOBJECTS)
	ar rcv $(LIBSTUDIOCOMMON) $(LIBOBJECTS)

.PHONY: clean
clean:
	rm -f $(LIBSTUDIOCOMMON) $(LIBOBJECTS) test_ffmpeg_encoder.o test_ffmpeg_encoder_av.o yuv422_to_mjpeg.o

test_ffmpeg_encoder: test_ffmpeg_encoder.o $(LIBSTUDIOCOMMON)
	$(LINK.cpp) -Wall -W -g test_ffmpeg_encoder.o $(LIBSTUDIOCOMMON) $(LIBCOMMON) -lavformat -lavcodec -lswscale -lavutil -lz -lbz2 -lmp3lame -lx264 -lfaac -lfaad -lm -lpthread -o test_ffmpeg_encoder

test_ffmpeg_encoder_av: test_ffmpeg_encoder_av.o $(LIBSTUDIOCOMMON)
	$(LINK.cpp) -Wall -W -g test_ffmpeg_encoder_av.o $(LIBSTUDIOCOMMON) $(LIBCOMMON) -lavformat -lavcodec -lswscale -lavutil -lz -lbz2 -lmp3lame -lx264 -lfaac -lfaad -lm -lpthread -o test_ffmpeg_encoder_av

yuv422_to_mjpeg: yuv422_to_mjpeg.o
	$(LINK.cpp) -Wall -W -g yuv422_to_mjpeg.o $(INCLUDES) $(LIBSTUDIOCOMMON) $(LIBCOMMON) $(YUVLIB) -ljpeg -lpthread -o $@

yuv422_to_mjpeg_threaded: yuv422_to_mjpeg.o
	$(LINK.cpp) -Wall -W -g yuv422_to_mjpeg.o -DBUILD_USING_MJPEG_THREADED_API $(INCLUDES) $(LIBSTUDIOCOMMON) $(LIBCOMMON) $(YUVLIB) -ljpeg -lpthread -o $@

