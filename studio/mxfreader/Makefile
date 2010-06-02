INSTALL_DIR ?= /usr/local

INGEX_DIR = ../..
LIBMXF_DIR = $(INGEX_DIR)/libMXF
LIBMXFPP_DIR = $(INGEX_DIR)/libMXF++
DATABASE_DIR = $(INGEX_DIR)/studio/database/src
COMMON_DIR = $(INGEX_DIR)/common
STUDIOCOMMON_DIR = $(INGEX_DIR)/studio/common


INCLUDES = -I. \
		   -I$(LIBMXFPP_DIR) \
		   -I$(LIBMXF_DIR)/lib/include \
		   -I$(DATABASE_DIR) \
		   -I$(COMMON_DIR) \
		   -I$(STUDIOCOMMON_DIR)
LIB_DIRS = -L. \
		   -L$(LIBMXFPP_DIR)/libMXF++ \
		   -L$(LIBMXF_DIR)/lib \
		   -L$(DATABASE_DIR) \
		   -L$(COMMON_DIR) \
		   -L$(STUDIOCOMMON_DIR)
LIBS = -lprodautodb -lcommon -lstudiocommon -lpq -lpqxx \
	   -lMXF++ -lMXF -luuid
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


.PHONY: libMXF.a
libMXF.a:
	@cd $(LIBMXF_DIR)/lib && $(MAKE) $@

.PHONY: libMXF++.a
libMXF++.a:
	@cd $(LIBMXFPP_DIR)/libMXF++ && $(MAKE) $@

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



import_mxf_info: libMXF.a libMXF++.a libprodautodb.a libingexmxfinfo.a .objs/import_mxf_info.o
	$(COMPILE) .objs/import_mxf_info.o $(LIB_DIRS) -lingexmxfinfo $(LIBS) -o $@

mxf_info_to_xml: libMXF.a libMXF++.a libprodautodb.a libingexmxfinfo.a .objs/mxf_info_to_xml.o
	$(COMPILE) .objs/mxf_info_to_xml.o $(LIB_DIRS) -lingexmxfinfo $(LIBS) -o $@


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
