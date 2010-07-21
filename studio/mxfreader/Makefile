include ../../common/libmxf.mk

INSTALL_DIR ?= /usr/local

INGEX_DIR = ../..
DATABASE_DIR = $(INGEX_DIR)/studio/database/src
COMMON_DIR = $(INGEX_DIR)/common
STUDIOCOMMON_DIR = $(INGEX_DIR)/studio/common


INCLUDES = -I. \
		   $(LIBMXFPP_INCLUDE) \
		   $(LIBMXF_INCLUDE) \
		   -I$(DATABASE_DIR) \
		   -I$(COMMON_DIR) \
		   -I$(STUDIOCOMMON_DIR)
LIBS = -L. \
	   -L$(DATABASE_DIR) -lprodautodb -lpqxx -lxerces-c \
	   -L$(COMMON_DIR) -lcommon \
	   -L$(STUDIOCOMMON_DIR) -lstudiocommon \
	   $(LIBMXFPP_LIB) $(LIBMXF_LIB)
CFLAGS = -Wall -W -Wno-unused-parameter -Werror -g -O2 -D_FILE_OFFSET_BITS=64 \
		 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE

CC = g++
COMPILE = $(CC) $(CFLAGS)
AR = ar cr


SOURCES = IngexMXFInfo.cpp import_mxf_info.cpp mxf_info_to_xml.cpp
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))

LIBOBJECTS = $(OBJECTS)
LIBOBJECTS -= .objs/import_mxf_info.o


.PHONY: all
all: libingexmxfinfo.a import_mxf_info mxf_info_to_xml


.PHONY: libprodautodb.a
libprodautodb.a:
	@cd $(DATABASE_DIR) && $(MAKE) $@


libingexmxfinfo.a: $(LIBOBJECTS)
	$(AR) $@ $(LIBOBJECTS)
	


.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDES) $< -o $@



import_mxf_info: libprodautodb.a libingexmxfinfo.a .objs/import_mxf_info.o
	$(COMPILE) .objs/import_mxf_info.o -lingexmxfinfo $(LIBS) -o $@

mxf_info_to_xml: libprodautodb.a libingexmxfinfo.a .objs/mxf_info_to_xml.o
	$(COMPILE) .objs/mxf_info_to_xml.o -lingexmxfinfo $(LIBS) -o $@


.PHONY: install
install: import_mxf_info mxf_info_to_xml
	cp import_mxf_info $(INSTALL_DIR)/bin
	cp mxf_info_to_xml $(INSTALL_DIR)/bin

.PHONY: uninstall
uninstall:
	@rm -f $(INSTALL_DIR)/bin/import_mxf_info
	@rm -f $(INSTALL_DIR)/bin/mxf_info_to_xml

	
.PHONY: clean
clean:
	@rm -f *~ *.a import_mxf_info mxf_info_to_xml
	@rm -Rf .objs
	@rm -Rf .deps


ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif
