#
# $Id: Makefile,v 1.1 2007/09/11 14:08:35 stuart_hc Exp $
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
ifndef DVSSDK
  CPU_ARCH := $(shell uname -m)
  ifeq "$(CPU_ARCH)" "x86_64"
    TMP_DVSSDK := $(shell for f in $$HOME/sdk2.7p48_x86_64 $$HOME/sdk2.7p39g_x86_64; do test -d $$f/linux && echo $$f && break; done)
  else
    TMP_DVSSDK := $(shell for f in $$HOME/sdk2.7p48 $$HOME/sdk2.7p39g; do test -d $$f/linux && echo $$f && break; done)
  endif
  ifneq "$(TMP_DVSSDK)" ""
    DVSSDK = $(TMP_DVSSDK)
  else
    $(error DVSSDK not set and could not be detected)
  endif
endif

HARDWARE_INCLUDE=-I$(DVSSDK)/development/header
HARDWARE_LIB=-L$(DVSSDK)/linux/lib -ldvsoem

SDI_PROGS=dvs_sdi testgen
PROGS=$(SDI_PROGS) nexus_save nexus_xv

.PHONY: sdi all
sdi: $(SDI_PROGS)
all: $(PROGS)

dvs_sdi: dvs_sdi.c nexus_control.h utils.o
	g++ $(FLAGS) $(HARDWARE_INCLUDE) dvs_sdi.c utils.o $(HARDWARE_LIB) $(LIBS) -lpthread -o dvs_sdi

testgen: testgen.c nexus_control.h
	g++ $(FLAGS) testgen.c $(LIBS) -lpthread -o testgen

nexus_stats: nexus_stats.c nexus_control.h
	g++ $(FLAGS) nexus_stats.c $(LIBS) -lpthread -o nexus_stats

nexus_save: nexus_save.c nexus_control.h dvd_encoder.h AudioMixer.h audio_utils.h librecord.a
	g++ $(FLAGS) nexus_save.c -o nexus_save -L. -lrecord -lavformat -lavutil -lavcodec -lmp3lame -lz -lm -lpthread

nexus_xv: nexus_xv.c nexus_control.h
	g++ $(FLAGS) nexus_xv.c -o nexus_xv -L/usr/X11R6/lib -lXv

clean:
	rm -f $(PROGS)
