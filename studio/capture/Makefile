#
# $Id: Makefile,v 1.4 2008/04/18 16:41:11 john_f Exp $
#
# Makefile for building ingex server and utilities.
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

CFLAGS=-Wall -g -DDEBUG -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
INCLUDES=-I../../common
LIBS=-L../../common -lcommon
OPT=-O3 -mmmx -msse2
FLAGS=$(CFLAGS) $(INCLUDES) $(OPT)

# Detect DVS sdk directory in descending order of preference
ifeq "$(CPU_ARCH)" "x86_64"
  ARCH1 := "_x86_64"
  ARCH2 := "-x86_64"
else
  ARCH1 := ""
  ARCH2 := "-x86"
endif

ifndef DVSSDK
  CPU_ARCH := $(shell uname -m)
  TMP_DVSSDK := $(shell for f in $$HOME/sdk2.7p57$(ARCH1)/linux $$HOME/sdk2.7p53$(ARCH1)/linux $$HOME/sdk3.0.0.1/linux$(ARCH2) $$HOME/sdk2.7p48$(ARCH1)/linux $$HOME/sdk2.7p39g$(ARCH1)/linux; do test -d $$f && dirname $$f && break; done)
  ifneq "$(TMP_DVSSDK)" ""
    DVSSDK = $(TMP_DVSSDK)
  else
    $(warning DVSSDK not set and could not be detected)
  endif
endif


HARDWARE_INCLUDE=-I$(DVSSDK)/development/header
#HARDWARE_LIB=-L$(DVSSDK)/linux/lib -ldvsoem
HARDWARE_LIB=-L$(DVSSDK)/linux-x86/lib -L$(DVSSDK)/linux-x86_64/lib -ldvsoem
#HARDWARE_LIB=-L$(DVSSDK)/linux-x86_64/lib -ldvsoem

SDI_PROGS=dvs_sdi testgen
PROGS=$(SDI_PROGS) nexus_save nexus_xv nexus_stats nexus_multicast nexus_set_source

.PHONY: all
all: $(PROGS)

dvs_sdi: dvs_sdi.c nexus_control.h
	g++ $(FLAGS) $(HARDWARE_INCLUDE) dvs_sdi.c $(HARDWARE_LIB) $(LIBS) -lpthread -o dvs_sdi

testgen: testgen.c nexus_control.h
	g++ $(FLAGS) testgen.c $(LIBS) -lpthread -o testgen

nexus_stats: nexus_stats.c nexus_control.h
	g++ $(FLAGS) nexus_stats.c $(LIBS) -lpthread -o nexus_stats

nexus_set_source: nexus_set_source.c nexus_control.h
	g++ $(FLAGS) nexus_set_source.c $(LIBS) -lpthread -o nexus_set_source

ifeq ($(USE_FFMPEG), 1)
.PHONY: ../common/ffmpeg_encoder.o
nexus_save: nexus_save.c nexus_control.h ../common/ffmpeg_encoder.o
	g++ $(FLAGS) -DUSE_FFMPEG nexus_save.c -o nexus_save ../common/ffmpeg_encoder.o -lavformat -lavcodec -lavutil -lz -lmp3lame -lx264 -la52 -lfaac -lfaad -lm -lpthread
else
nexus_save: nexus_save.c nexus_control.h
	g++ $(FLAGS) nexus_save.c -o nexus_save
endif

nexus_multicast: nexus_multicast.c nexus_control.h
	gcc -I../../common $(FLAGS) nexus_multicast.c -o nexus_multicast -L../../common -lcommon -L../../common/YUVlib -lYUVlib

nexus_xv: nexus_xv.c nexus_control.h
	g++ $(FLAGS) nexus_xv.c -o nexus_xv -L/usr/X11R6/lib -lXv

clean:
	rm -f core $(PROGS)
