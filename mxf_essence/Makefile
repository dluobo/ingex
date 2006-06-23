# SAMBA_DIR is the path to the Samba sources
ifndef SAMBA_DIR
  $(error Please set the SAMBA_DIR environment variable to the location of your Samba source directory)
endif


CC		= gcc
CFLAGS		= -g -O2 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
CPPFLAGS	= 
LDFLAGS		= 
LDSHFLAGS	= -shared
SAMBA_SOURCE	= $(SAMBA_DIR)/source
SHLIBEXT	= so
OBJEXT		= o 
FLAGS		=  $(CFLAGS) -Iinclude -I$(SAMBA_SOURCE)/include -I$(SAMBA_SOURCE)/ubiqx -I$(SAMBA_SOURCE)/smbwrapper  -I. $(CPPFLAGS) -I$(SAMBA_SOURCE)



.PHONY: default
default: mxf_harmony.so test


test: mxf_essence.o test.o
	$(CC) -L. test.o mxf_essence.o -o test

test.o: test.c mxf_essence.h
	$(CC) -c $(FLAGS) test.c
	
mxf_essence.o: mxf_essence.c mxf_essence.h
	$(CC) -c $(FLAGS) mxf_essence.c
	
mxf_harmony.o: mxf_harmony.c mxf_essence.h
	$(CC) -c $(FLAGS) mxf_harmony.c

mxf_harmony.so: mxf_harmony.o mxf_essence.o
	$(CC) $(LDSHFLAGS) $(LDFLAGS) mxf_harmony.o mxf_essence.o -o mxf_harmony.so 

	
	
clean:
	rm -f *~ *.o *.so test


