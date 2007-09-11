#
# $Id: Makefile,v 1.1 2007/09/11 14:08:00 stuart_hc Exp $
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
INCLUDES=
CFLAGS=-Wall -W -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE $(OPT) $(INCLUDES)

HEADERS=$(wildcard *.h)
CFILES=$(wildcard *.c)
# filter out test programs with a main()
CFILES:=$(filter-out %unittests.c,$(CFILES))

OBJECTS=$(CFILES:%.c=%.o)

LIBCOMMON=libcommon.a

PROGS=unittests

.PHONY: all
all: YUVlib $(LIBCOMMON) $(PROGS)

$(LIBCOMMON): $(OBJECTS)
	ar rcv $(LIBCOMMON) $(OBJECTS)

unittests: unittests.c $(LIBCOMMON)
	gcc -Wall -W -g unittests.c -o unittests $(LIBCOMMON)

.PHONY: check
check:
	./unittests

.PHONY: valgrind-check
valgrind-check:
	valgrind ./unittests

.PHONY: clean
clean:
	rm -f $(LIBCOMMON) $(OBJECTS) $(PROGS)
	
.PHONY: YUVlib
YUVlib:
	@cd YUVlib && $(MAKE)
