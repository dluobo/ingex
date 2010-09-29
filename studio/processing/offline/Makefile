# $Id: Makefile,v 1.14 2010/09/29 09:01:14 john_f Exp $
#

include ../../../common/libmxf.mk

DATABASELIB_DIR = ../../database/src
MXFWRITER_DIR = ../../mxfwriter
STUDIOCOMMON_DIR = ../../common
COMMON_DIR = ../../../common
YUVLIB_DIR = ../../../YUVlib


CC = gcc
CFLAGS = -g -Wall -Wundef -W -Wno-unused-parameter -O3 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS
INCLUDES = -I. -I$(COMMON_DIR) -I$(STUDIOCOMMON_DIR) -I$(YUVLIB_DIR) -I/usr/local/include \
	$(LIBMXF_INCLUDE) $(WRITEAVIDMXF_INCLUDE) $(MXFREADER_INCLUDE) \
	-I$(DATABASELIB_DIR) -I$(MXFWRITER_DIR) -I/usr/local/include/ffmpeg/

# Check for old-style FFMPEG header locations and if needed add -DFFMPEG_OLD_INCLUDE_PATHS
FFMPEG_OLDSTYLE_INCLUDE := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/avcodec.h && echo $$f && break; done)
ifneq "$(FFMPEG_OLDSTYLE_INCLUDE)" ""
	CFLAGS += -DFFMPEG_OLD_INCLUDE_PATHS
endif


C_COMPILE = gcc $(CFLAGS) $(INCLUDES)
CPP_COMPILE = g++ $(CFLAGS) $(INCLUDES)
AR = ar cr


SOURCES = transcode_avid_mxf.cpp
OBJECTS = $(subst .cpp,.o,$(SOURCES))
DEPENDENCIES = $(subst .cpp,.d,$(SOURCES))


FFMPEG_LIBS = -lavformat -lavcodec -lavutil -lz -lbz2 -lmp3lame -lx264 -lfaac -lfaad -lm

TRANSCODE_LIBS = -ljpeg $(FFMPEG_LIBS) -L$(STUDIOCOMMON_DIR) -lstudiocommon -L$(YUVLIB_DIR) -lYUVlib \
	-L$(DATABASELIB_DIR) -lprodautodb -lpqxx -lxerces-c -L$(COMMON_DIR) -lcommon -L$(STUDIOCOMMON_DIR) -lstudiocommon \
	-L$(MXFWRITER_DIR) -lmxfwriter $(WRITEAVIDMXF_LIB) \
	$(MXFREADER_LIB) $(LIBMXF_LIB) -lpthread


.PHONY: all
all: transcode_avid_mxf


%.d : %.cpp
	@echo Generating dependency file for $<; \
	$(CPP_COMPILE) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.d : %.c
	@echo Generating dependency file for $<; \
	$(C_COMPILE) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.o: %.cpp
	$(CPP_COMPILE) -c $< -o $@	
	
%.o: %.c
	$(C_COMPILE) -c $< -o $@	
	
	
.PHONY: libmxfwriter.a
	@cd $(MXFWRITER_DIR) && $(MAKE) $@
	
	
transcode_avid_mxf: libmxfwriter.a transcode_avid_mxf.o
	$(CPP_COMPILE) transcode_avid_mxf.o $(TRANSCODE_LIBS) -o $@

	

.PHONY: clean
clean:
	@rm -f *.d *.o *.a *~ transcode_avid_mxf
	
	

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
	

