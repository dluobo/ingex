#
# $Id: Makefile,v 1.5 2008/04/18 15:54:16 john_f Exp $
#
# Makefile for building common functions
#
# Copyright (C) 2005  Stuart Cunningham <stuart_hc@users.sourceforge.net>
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
INCLUDES=-IYUVlib
CFLAGS=-Wall -W -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE $(OPT) $(INCLUDES)

HEADERS=$(wildcard *.h)
CFILES=$(wildcard *.c)
# filter out test programs with a main()
CFILES:=$(filter-out %unittests.c,$(CFILES))

OBJECTS=$(CFILES:%.c=%.o)

LIBCOMMON=libcommon.a

PROGS=unittests

.PHONY: all
all: YUVlib $(LIBCOMMON) $(PROGS) tools

$(LIBCOMMON): $(OBJECTS)
	ar rcv $(LIBCOMMON) $(OBJECTS)

unittests: unittests.c $(LIBCOMMON)
	gcc -Wall -W -g unittests.c -o unittests $(LIBCOMMON) -lm

.PHONY: check
check: all
	./unittests

.PHONY: valgrind-check
valgrind-check: all
	valgrind ./unittests

.PHONY: clean
clean:
	rm -f $(LIBCOMMON) $(OBJECTS) $(PROGS)
	cd YUVlib && $(MAKE) $@
	cd tools && $(MAKE) $@
	
.PHONY: YUVlib
YUVlib:
	cd YUVlib && $(MAKE)

.PHONY: tools
tools:
	cd tools && $(MAKE)
