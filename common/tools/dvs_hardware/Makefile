#
# $Id: Makefile,v 1.1 2008/05/07 17:04:20 philipn Exp $
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

CFLAGS=-Wall -g -DDEBUG -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
OPT=-O3 -mmmx -msse2
FLAGS=$(CFLAGS) $(OPT)

ifeq "$(CPU_ARCH)" "x86_64"
  ARCH1 := "_x86_64"
  ARCH2 := "-x86_64"
else
  ARCH1 := ""
  ARCH2 := "-x86"
endif

SDI_PROGS=recplay playout record record_mxf playout_non_mt 
PROGS=$(SDI_PROGS)

# Use DVSSDK is set or detect DVS sdk directory by scanning for versions
# Currently 2.7p57 is preferred over other versions since it the
# the first to support the Centaurus II board (3.0.0.1 doesn't)
ifndef DVSSDK
  CPU_ARCH := $(shell uname -m)
  TMP_DVSSDK := $(shell for f in $$HOME/sdk2.7p57$(ARCH1)/linux $$HOME/sdk2.7p53$(ARCH1)/linux $$HOME/sdk3.0.0.1/linux$(ARCH2) $$HOME/sdk2.7p48$(ARCH1)/linux $$HOME/sdk2.7p39g$(ARCH1)/linux; do test -d $$f && dirname $$f && break; done)
  ifneq "$(TMP_DVSSDK)" ""
    DVSSDK = $(TMP_DVSSDK)
  else
    $(warning DVSSDK not set and could not be detected)
	PROGS=
  endif
endif

HARDWARE_INCLUDE=-I$(DVSSDK)/development/header
# lib dir may be under linux or linux-x86 or linux-x86_64
HARDWARE_LIB=-L$(DVSSDK)/linux$(ARCH2)/lib -L$(DVSSDK)/linux/lib -ldvsoem

INCLUDE=-I../.. -I.. $(HARDWARE_INCLUDE)


.PHONY: all
all: $(PROGS)

playout: playout.c
	g++ $(FLAGS) $(INCLUDE) playout.c $(HARDWARE_LIB) -lpthread -o playout

record: record.c ../../libcommon.a
	g++ $(FLAGS) -I. $(INCLUDE) record.c ../../libcommon.a $(HARDWARE_LIB) -lpthread -o record

record_mxf: record_mxf.c ../../libcommon.a
	g++ $(FLAGS) -I. $(INCLUDE) -I../../../libMXF/lib/include -I../../../libMXF/examples/archive -I../../../libMXF/examples/archive/write record_mxf.c ../../libcommon.a $(HARDWARE_LIB) -lpthread -L../../../libMXF/examples/archive/write -L../../../libMXF/lib -lwritearchivemxf -lMXF -luuid -o record_mxf

recplay: recplay.c ../../libcommon.a
	g++ $(FLAGS) -I. $(INCLUDE) recplay.c ../../libcommon.a $(HARDWARE_LIB) -lpthread -lncurses -o recplay

playout_non_mt: playout_non_mt.c
	g++ $(FLAGS) $(INCLUDE) playout_non_mt.c $(HARDWARE_LIB) -lpthread -o playout_non_mt

clean:
	rm -f $(PROGS)
