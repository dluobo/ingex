# $Id: Makefile,v 1.1 2007/09/11 14:08:45 stuart_hc Exp $
#
LIBMXF_DIR ?= ../../lib

CC = gcc -Wall -W -Werror -Wno-unused-parameter -g -O3 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
INCLUDES = -I. -I.. -I$(LIBMXF_DIR)/include -I../../common
LIBS = -L../../../common/YUVlib -lYUVlib -ljpeg
AR = ar cr

.PHONY: all
all: yuv422_to_mjpeg

mjpeg_compress.o: mjpeg_compress.c mjpeg_compress.h
	$(CC) -Wno-error -c $(INCLUDES) mjpeg_compress.c

yuv422_to_mjpeg: mjpeg_compress.o yuv422_to_mjpeg.c
	$(CC) yuv422_to_mjpeg.c mjpeg_compress.o $(LIBS) -o $@

clean:
	rm -f mjpeg_compress.o yuv422_to_mjpeg
