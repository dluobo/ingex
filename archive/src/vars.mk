# NOTES:
#
# Set the (optional) BBC_ARCHIVE_DIR environment variable to the directory containing the BBC stuff, including the PSE module
# Set the DVSSDK environment variable pointing to the DVS SDK directory
# Set the (optional) USE_DVS_DUMMY environment variable to use the dummy capture when testing without a DVS card
# Set the (optional) LEGACY_FFMPEG environment variable to use the legacy library dependencier for FFmpeg
# Edit the FFMPEG_LIB variable to match the library dependencies of the installed FFmpeg library
# Set the (optional) ENABLE_DEBUG environment variable to enable debugging using gdb or valgrind



# TOPLEVEL must be set by any Makefile which includes this file
ifndef TOPLEVEL
  $(error TOPLEVEL variable not defined)
endif



# check environment variables

ifndef DVSSDK
  $(error DVSSDK environment variable is not set)
endif


# determine OS

OS_64_BIT =
ifeq "$(shell uname -m)" "x86_64"
	OS_64_BIT = 1
endif



# directories. 

INGEX_DIR = $(TOPLEVEL)/../..

INGEX_COMMON_DIR = $(INGEX_DIR)/common
INGEX_YUVLIB_DIR = $(INGEX_DIR)/YUVlib

INGEX_STUDIO_COMMON_DIR = $(INGEX_DIR)/studio/common

INGEX_PLAYER_DIR = $(INGEX_DIR)/player/ingex_player

LIBMXF_DIR = $(INGEX_DIR)/libMXF
LIBMXF_LIB_DIR = $(LIBMXF_DIR)/lib
LIBMXF_ARCHIVE_DIR = $(LIBMXF_DIR)/examples/archive
LIBMXF_READER_DIR = $(LIBMXF_DIR)/examples/reader

LIBMXFPP_DIR = $(INGEX_DIR)/libMXF++
LIBMXFPP_D10MXFOP1AWRITER_DIR = $(LIBMXFPP_DIR)/examples/D10MXFOP1AWriter
LIBMXFPP_COMMON_DIR = $(LIBMXFPP_DIR)/examples/Common

JOGSHUTTLE_DIR = $(INGEX_DIR)/player/jogshuttle

ARCHIVE_DIR = $(TOPLEVEL)
GENERAL_DIR = $(ARCHIVE_DIR)/general
INFAX_ACCESS_DIR = $(ARCHIVE_DIR)/infax-access
DATABASE_DIR = $(ARCHIVE_DIR)/database
RECMXF_DIR = $(ARCHIVE_DIR)/mxf
ENCODER_DIR = $(ARCHIVE_DIR)/encoder
CACHE_DIR = $(ARCHIVE_DIR)/cache
HTTP_DIR = $(ARCHIVE_DIR)/http
BARCODE_SCANNER_DIR = $(ARCHIVE_DIR)/barcode-scanner
BROWSE_ENCODER_DIR = $(ARCHIVE_DIR)/browse-encoder
CAPTURE_DIR = $(ARCHIVE_DIR)/capture
TAPE_IO_DIR = $(ARCHIVE_DIR)/tape-io
VTR_DIR = $(ARCHIVE_DIR)/vtr
PSE_DIR = $(ARCHIVE_DIR)/pse
LTCSMPTE_DIR = $(ARCHIVE_DIR)/libltcsmpte

BBC_ARCHIVE_DIR ?=



# Variables for compilation

CC = g++
C_CC = gcc
INCLUDES = -I.
CFLAGS = -Wall -W -Wno-unused-parameter -Werror -g -O2 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE $(INCLUDES)
AR = ar cr

COMPILE = $(CC) $(CFLAGS)
C_COMPILE = $(C_CC) $(CFLAGS)


# Debugging
ifdef ENABLE_DEBUG
	CFLAGS += -DENABLE_DEBUG
endif


# Check for old-style FFMPEG header locations and if needed add -DFFMPEG_OLD_INCLUDE_PATHS
FFMPEG_OLDSTYLE_INCLUDE := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/avcodec.h && echo $$f && break; done)
ifneq "$(FFMPEG_OLDSTYLE_INCLUDE)" ""
	CFLAGS += -DFFMPEG_OLD_INCLUDE_PATHS
endif


# PSE module

ifdef BBC_ARCHIVE_DIR
	CFLAGS += -DHAVE_PSE_FPA
endif


# DVS dummy source file

ifdef DVS_DUMMY_SOURCE_FILE
	CFLAGS += -DDVS_DUMMY_SOURCE_FILE
endif


# Objects and dependency files associated with $SOURCES and $C_SOURCES

OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))

C_OBJECTS = $(patsubst %.c,.c_objs/%.o,$(C_SOURCES))
C_DEPENDENCIES = $(patsubst %.c,.c_deps/%.d,$(C_SOURCES))



# includes

INGEX_COMMON_INC = -I$(INGEX_COMMON_DIR)
INGEX_YUVLIB_INC = -I$(INGEX_YUVLIB_DIR)

INGEX_STUDIO_FFMPEG_INC = -I$(INGEX_STUDIO_COMMON_DIR)

LIBMXF_INC = -I$(LIBMXF_LIB_DIR)/include
LIBMXF_ARCHIVE_INC = -I$(LIBMXF_ARCHIVE_DIR)
LIBMXF_ARCHIVE_WRITE_INC = -I$(LIBMXF_ARCHIVE_DIR)/write
LIBMXF_ARCHIVE_INFO_INC = -I$(LIBMXF_ARCHIVE_DIR)/info
LIBMXF_READER_INC = -I$(LIBMXF_READER_DIR)

LIBMXFPP_INC = -I$(LIBMXFPP_DIR)
LIBMXFPP_D10MXFOP1AWRITER_INC = -I$(LIBMXFPP_D10MXFOP1AWRITER_DIR)
LIBMXFPP_COMMON_INC = -I$(LIBMXFPP_COMMON_DIR)

JOGSHUTTLE_INC = -I$(JOGSHUTTLE_DIR)

GENERAL_INC=-I$(GENERAL_DIR)
RECMXF_INC=-I$(RECMXF_DIR)
ENCODER_INC=-I$(ENCODER_DIR)
CACHE_INC=-I$(CACHE_DIR)
INFAX_ACCESS_INC=-I$(INFAX_ACCESS_DIR)
DATABASE_INC=-I$(DATABASE_DIR)
HTTP_INC=-I$(HTTP_DIR)
BARCODE_SCANNER_INC=-I$(BARCODE_SCANNER_DIR)
BROWSE_ENCODER_INC=-I$(BROWSE_ENCODER_DIR)
TAPE_IO_INC=-I$(TAPE_IO_DIR)
CAPTURE_INC=-I$(CAPTURE_DIR)
VTR_INC=-I$(VTR_DIR)
PSE_INC=-I$(PSE_DIR)
LTCSMPTE_INC=-I$(LTCSMPTE_DIR)

DVS_INC = -I$(DVSSDK)/development/header

ifdef BBC_ARCHIVE_DIR
	FPA_INC = -I$(BBC_ARCHIVE_DIR)/pse
else
	FPA_INC =
endif



# libraries 

INGEX_COMMON_LIB = -L$(INGEX_COMMON_DIR) -lcommon
INGEX_YUVLIB_LIB = -L$(INGEX_YUVLIB_DIR) -lYUVlib

INGEX_STUDIO_FFMPEG_OBJ = $(INGEX_STUDIO_COMMON_DIR)/ffmpeg_encoder.o \
	$(INGEX_STUDIO_COMMON_DIR)/ffmpeg_resolutions.o \
	$(INGEX_STUDIO_COMMON_DIR)/MaterialResolution.o

LIBMXF_LIB = -L$(LIBMXF_LIB_DIR) -lMXF -luuid
LIBMXF_ARCHIVE_WRITE_LIB = -L$(LIBMXF_ARCHIVE_DIR)/write -lwritearchivemxf
LIBMXF_ARCHIVE_INFO_LIB = -L$(LIBMXF_ARCHIVE_DIR)/info -larchivemxfinfo
LIBMXF_READER_LIB = -L$(LIBMXF_READER_DIR) -lMXFReader

LIBMXFPP_LIB = -L$(LIBMXFPP_DIR)/libMXF++ -lMXF++
LIBMXFPP_D10MXFOP1AWRITER_LIB = -L$(LIBMXFPP_D10MXFOP1AWRITER_DIR) -lD10MXFOP1AWriter

JOGSHUTTLE_LIB = -L$(JOGSHUTTLE_DIR) -ljogshuttle

ifdef USE_DVS_DUMMY
	DVS_LIB = $(CAPTURE_DIR)/.c_objs/sv_dummy.o
else
	ifdef OS_64_BIT
		DVS_LIB = -L$(DVSSDK)/linux-x86_64/lib -ldvsoem
	else
		DVS_LIB = -L$(DVSSDK)/linux-x86/lib -ldvsoem
	endif
endif

ifdef BBC_ARCHIVE_DIR
	ifdef OS_64_BIT
		FPA_LIB = -L$(BBC_ARCHIVE_DIR)/pse/lib64
	else
		FPA_LIB = -L$(BBC_ARCHIVE_DIR)/pse/lib32
	endif
	FPA_LIB += -lfpacore2 $(BBC_ARCHIVE_DIR)/pse/pse_fpa.o
else
	FPA_LIB =
endif

LIBPQXX_LIB = -lpqxx -lpq

SHTTPD_LIB = -lshttpd

ifdef LEGACY_FFMPEG
	FFMPEG_LIB = -L/usr/local/lib -lavformat -lavcodec -lavutil -lmp3lame -lx264 -lfaac -lfaad -la52 -lpthread -lz -lbz2 -lm
else
	# modify this line to match the FFMPEG installation
	FFMPEG_LIB = -L/usr/local/lib -lavformat -lavcodec -lavutil -lswscale -lmp3lame -lx264 -lfaac -lfaad -lpthread -lz -lbz2 -lm
endif

LTCSMPTE_LIB = $(LTCSMPTE_DIR)/libltcsmpte.a

