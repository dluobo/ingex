include ../../../common/libmxf.mk

INSTALL_DIR ?= /usr/local

INGEX_DIR = ../../..

COMMON_DIR = $(INGEX_DIR)/common
STUDIOCOMMON_DIR = $(INGEX_DIR)/studio/common
DATABASE_DIR = $(INGEX_DIR)/studio/database/src
INGEXMXFINFO_DIR = $(INGEX_DIR)/studio/mxfreader
MXFWRITER_DIR = $(INGEX_DIR)/studio/mxfwriter


CC = g++
CFLAGS = -g -Wall -Wundef -W -Werror  -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
COMPILE = $(CC) $(CFLAGS)

INCLUDES = -I. \
		   -I$(COMMON_DIR) \
		   -I$(STUDIOCOMMON_DIR) \
		   $(LIBMXFPP_INCLUDE) \
		   $(LIBMXF_INCLUDE) \
		   -I$(DATABASE_DIR) \
		   -I$(INGEXMXFINFO_DIR) \
		   -I$(MXFWRITER_DIR) \
		   $(OPATOMREADER_INCLUDE) \
		   $(LIBMXFPPEXAMPLESCOMMON_INCLUDE)
LIBS = -L$(INGEXMXFINFO_DIR) -lingexmxfinfo \
	   -L$(MXFWRITER_DIR) -lmxfwriter \
	   -L$(DATABASE_DIR) -lprodautodb -lpqxx -lxerces-c \
	   -L$(COMMON_DIR) -lcommon \
	   -L$(STUDIOCOMMON_DIR) -lstudiocommon \
	   $(OPATOMREADER_LIB) \
	   $(LIBMXFPP_LIB) \
	   $(WRITEAVIDMXF_LIB) \
	   $(LIBMXF_LIB) \
	   -lpthread


SOURCES = $(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))


.PHONY: all
all: recover_mxf


.PHONY: libprodautodb.a
libprodautodb.a:
	@cd $(DATABASE_DIR) && $(MAKE) $@

.PHONY: libingexmxfinfo.a
libingexmxfinfo.a:
	@cd $(INGEXMXFINFO_DIR) && $(MAKE) $@

.PHONY: libmxfwriter.a
libmxfwriter.a:
	@cd $(MXFWRITER_DIR) && $(MAKE) $@


.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDES) $< -o $@


recover_mxf: libprodautodb.a libingexmxfinfo.a libmxfwriter.a $(OBJECTS)
	$(COMPILE) $(OBJECTS) $(LIBS) -o $@



.PHONY: install
install: recover_mxf
	cp recover_mxf $(INSTALL_DIR)/bin

.PHONY: uninstall
uninstall:
	@rm -f $(INSTALL_DIR)/bin/recover_mxf


.PHONY: clean
clean:
	@rm -f *.a *~ recover_mxf
	@rm -Rf .objs
	@rm -Rf .deps


ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
	

