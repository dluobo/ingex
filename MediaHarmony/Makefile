# SAMBA_DIR is the path to the Samba sources
ifndef SAMBA_DIR
  $(error Please set the SAMBA_DIR environment variable to the location of your Samba source directory)
endif


CC		= gcc
CFLAGS		= -g -O2
CPPFLAGS	= 
LDFLAGS		= 
LDSHFLAGS	= -shared
INSTALLCMD	= /usr/bin/install -c
SAMBA_SOURCE	= $(SAMBA_DIR)source
SHLIBEXT	= so
OBJEXT		= o 
FLAGS		=  $(CFLAGS) -Iinclude -I$(SAMBA_SOURCE)/include -I$(SAMBA_SOURCE)/popt -I$(SAMBA_SOURCE)/ubiqx -I$(SAMBA_SOURCE)/smbwrapper  -I. $(CPPFLAGS) -I$(SAMBA_SOURCE) -fPIC


prefix		= /usr/local/samba
libdir		= ${prefix}/lib

VFS_LIBDIR	= $(libdir)/vfs


.PHONY: default
default: media_harmony.so mh_full_audit.so media_many.so media_link.so mxf_harmony.so test_mxf_essence


# mxf_harmony
test_mxf_essence: mxf_essence.o test_mxf_essence.o
	$(CC) -L. test_mxf_essence.o mxf_essence.o -o test_mxf_essence

test_mxf_essence.o: test_mxf_essence.c mxf_essence.h
	$(CC) -c $(FLAGS) test_mxf_essence.c
	
mxf_essence.o: mxf_essence.c mxf_essence.h
	$(CC) -c $(FLAGS) mxf_essence.c
	
mxf_harmony.o: mxf_harmony.c mxf_essence.h
	$(CC) -c $(FLAGS) mxf_harmony.c

mxf_harmony.so: mxf_harmony.o mxf_essence.o
	$(CC) $(LDSHFLAGS) $(LDFLAGS) mxf_harmony.o mxf_essence.o -o mxf_harmony.so 

	
# media link	

media_link.o: media_link.c
	$(CC) -c $(FLAGS) media_link.c

media_link.so: media_link.o
	$(CC) $(LDSHFLAGS) $(LDFLAGS) media_link.o -o media_link.so 

	
# media many	

media_many.o: media_many.c
	$(CC) -c $(FLAGS) media_many.c

media_many.so: media_many.o
	$(CC) $(LDSHFLAGS) $(LDFLAGS) media_many.o -o media_many.so 


# media harmony	

media_harmony.o: media_harmony.c
	$(CC) -c $(FLAGS) media_harmony.c

media_harmony.so: media_harmony.o
	$(CC) $(LDSHFLAGS) $(LDFLAGS) media_harmony.o -o media_harmony.so 


# media harmony full audit

mh_full_audit.o: mh_full_audit.c
	$(CC) -c $(FLAGS) mh_full_audit.c

mh_full_audit.so: mh_full_audit.o
	$(CC) $(LDSHFLAGS) $(LDFLAGS) mh_full_audit.o -o mh_full_audit.so 



install: default
	$(INSTALLCMD) -d $(VFS_LIBDIR)
	$(INSTALLCMD) -m 755 *.$(SHLIBEXT) $(VFS_LIBDIR)

clean:
	rm -f test_mxf_essence
	rm -rf .libs
	rm -f core *~ *% *.bak *.o *.$(SHLIBEXT)


