#
# $Id: Makefile,v 1.12 2011/02/18 16:26:54 john_f Exp $
#
# Makefile for building common functions
#
# Copyright (C) 2005 - 2010 British Broadcasting Corporation
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

CC=gcc
OPT=-DNDEBUG -O3 -mmmx -msse2
INCLUDES=
CFLAGS=-Wall -W -Wno-unused-parameter -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE $(OPT) $(INCLUDES)
CXXFLAGS=-Wall -W -Wno-unused-parameter -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE $(OPT) $(INCLUDES)

# Check for old-style FFMPEG header locations and if needed add -DFFMPEG_OLD_INCLUDE_PATHS
FFMPEG_OLDSTYLE_INCLUDE := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/avcodec.h && echo $$f && break; done)
ifneq "$(FFMPEG_OLDSTYLE_INCLUDE)" ""
	CFLAGS += -DFFMPEG_OLD_INCLUDE_PATHS
	CPPFLAGS += -DFFMPEG_OLD_INCLUDE_PATHS
endif

HEADERS=$(wildcard *.h)
CFILES=$(wildcard *.c)
# filter out test programs with a main()
CFILES:=$(filter-out %unittests.c,$(CFILES))
CPPFILES=$(wildcard *.cpp)

OBJECTS=$(CFILES:%.c=%.o) $(CPPFILES:%.cpp=%.o)

LIBCOMMON=libcommon.a

PROGS=unittests

.PHONY: all
all: $(LIBCOMMON) $(PROGS) tools

$(LIBCOMMON): $(OBJECTS)
	ar rcv $(LIBCOMMON) $(OBJECTS)

unittests: unittests.cpp $(LIBCOMMON)
	g++ -Wall -W -g unittests.cpp -o unittests $(LIBCOMMON) -lm

.PHONY: check
check: all
	./unittests

.PHONY: valgrind-check
valgrind-check: all
	valgrind ./unittests

.PHONY: clean
clean:
	rm -f $(LIBCOMMON) $(OBJECTS) $(PROGS)
	cd tools && $(MAKE) $@
	
.PHONY: tools
tools:
	cd tools && $(MAKE)

