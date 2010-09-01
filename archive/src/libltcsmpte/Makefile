#TOPLEVEL = ..
#include $(TOPLEVEL)/vars.mk


#C_SOURCES = libltcsmpte.c \
#	decoder.c \
#	encoder.c \
#	smpte.c \
#	framerate.c

#LIBLTCSMPTE = libltcsmpte.a

#.PHONY: all
#all: $(LIBLTCSMPTE) $(C_OBJECTS)

#.PHONY: $(LIBLTCSMPTE)
#$(LIBLTCSMPTE): (.c_objs/%.o): %.c
#	ar rcv $(LIBLTCSMPTE) $<

#.PHONY: clean
#clean: cmn-clean
	

#include $(TOPLEVEL)/rules.mk

CC=gcc
OPT=-O3 -mmmx -msse2
INCLUDES=-I.
CFLAGS=-Wall -W -Wno-unused-parameter -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE $(OPT) $(INCLUDES)

HEADERS=$(wildcard, *.h)
CFILES=$(wildcard *.c)
OBJECTS=$(CFILES:%.c=%.o)

LIBLTCSMPTE = libltcsmpte.a

.PHONY: all clean
all: $(LIBLTCSMPTE)($(OBJECTS))

clean:
	rm -f *.o *.a

$(LIBLTCSMPTE)(%.o): %.o
	ar rcv $(LIBLTCSMPTE) $<
