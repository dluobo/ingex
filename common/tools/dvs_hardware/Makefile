#
# $Id: Makefile,v 1.5 2010/07/06 14:15:13 john_f Exp $
#
# Makefile for building DVS hardware capture & playout tools
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

FLAGS=-Wall -g -DDEBUG -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
OPT=-O3 -mmmx -msse2


SDI_PROGS=recplay playout record record_mxf playout_non_mt check_inputs
PROGS=$(SDI_PROGS)

# dvs_sdk_paths.mk searches for sdk and sets DVS_HARDWARE_INCLUDE and DVS_HARDWARE_LIB
# search is overridden by setting DVSSDK
include ../../dvs_sdk_paths.mk

ifndef DVS_HARDWARE_INCLUDE
  $(info SDI tools will not be built)
  PROGS=
endif

INCLUDE=-I../.. -I.. $(DVS_HARDWARE_INCLUDE)

CFLAGS = -std=gnu99 $(FLAGS) $(OPT) $(INCLUDE)
CPPFLAGS = $(FLAGS) $(OPT) $(INCLUDE)

.PHONY: all
all: $(PROGS) libdvsoem_dummy.a

libdvsoem_dummy.a: dvsoem_dummy.o
	ar crv libdvsoem_dummy.a dvsoem_dummy.o
	
playout: playout.o
	$(LINK.c) playout.o $(DVS_HARDWARE_LIB) -lpthread -o $@

record: record.o
	$(LINK.cpp) record.c ../../libcommon.a $(DVS_HARDWARE_LIB) -lpthread -o $@

record_mxf.o: record_mxf.c
	$(COMPILE.c) -c -I../../../libMXF/lib/include -I../../../libMXF/examples/archive -I../../../libMXF/examples/archive/write $< -o $@

record_mxf: record_mxf.o
	$(LINK.cpp) record_mxf.o ../../libcommon.a $(DVS_HARDWARE_LIB) -lpthread -L../../../libMXF/examples/archive/write -L../../../libMXF/lib -lwritearchivemxf -lMXF -luuid -o $@

recplay: recplay.o
	$(LINK.cpp)  recplay.o ../../libcommon.a $(DVS_HARDWARE_LIB) -lpthread -lncurses -o $@

playout_non_mt: playout_non_mt.o
	$(LINK.c) playout_non_mt.o $(DVS_HARDWARE_LIB) -lpthread -o $@

check_inputs: check_inputs.o
	$(LINK.c) $< $(DVS_HARDWARE_LIB) -o $@

clean:
	rm -f $(SDI_PROGS) *.o libdvsoem_dummy.a
