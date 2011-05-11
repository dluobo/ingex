#
# $Id: Makefile,v 1.4 2011/05/11 10:40:37 philipn Exp $
#
# Copyright (C) 2010  British Broadcasting Corporation.
# All Rights Reserved.
#
# Author: Philip de Nier
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA
#/

TOPLEVEL = ../..
include $(TOPLEVEL)/vars.mk

COMMON_DIR = ../Common

INCLUDES += -I$(COMMON_DIR)

SOURCES = D10ContentPackage.cpp \
	D10MXFOP1AWriter.cpp
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))

COMMON_SOURCES = DynamicByteArray.cpp
COMMON_OBJECTS = $(patsubst %.cpp,$(COMMON_DIR)/.objs/%.o,$(COMMON_SOURCES))
DEPENDENCIES += $(patsubst %.cpp,.deps/%.d,$(COMMON_SOURCES))



.PHONY: all
all: libD10MXFOP1AWriter.a test_d10mxfop1awriter



libD10MXFOP1AWriter.a: $(OBJECTS) $(COMMON_OBJECTS)
	$(AR) $@ $(OBJECTS) $(COMMON_OBJECTS)


$(COMMON_OBJECTS):
	@cd $(COMMON_DIR) && $(MAKE) all

.deps/%.d : $(COMMON_DIR)/%.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,$(COMMON_DIR)/\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$



.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDES) $< -o $@



test_d10mxfop1awriter: .objs/test_d10mxfop1awriter.o libD10MXFOP1AWriter.a
	$(COMPILE) .objs/test_d10mxfop1awriter.o -L. -L$(LIBMXFPP_DIR)/libMXF++ -L$(LIBMXF_DIR)/lib -lD10MXFOP1AWriter -lMXF++ -lMXF $(UUIDLIB) -o $@


.PHONY: install
install: all
	mkdir -p $(INSTALL_PREFIX)/lib
	cp libD10MXFOP1AWriter.a $(INSTALL_PREFIX)/lib/
	mkdir -p $(INSTALL_PREFIX)/include/libMXF++/examples/D10MXFOP1AWriter
	cp D10ContentPackage.h $(INSTALL_PREFIX)/include/libMXF++/examples/D10MXFOP1AWriter/
	cp D10MXFOP1AWriter.h $(INSTALL_PREFIX)/include/libMXF++/examples/D10MXFOP1AWriter/

.PHONY: uninstall
uninstall:
	rm -f $(INSTALL_PREFIX)/lib/libD10MXFOP1AWriter.a
	rm -Rf $(INSTALL_PREFIX)/include/libMXF++/examples/D10MXFOP1AWriter

.PHONY: clean
clean:
	@rm -f *~ *.a test_d10mxfop1awriter
	@rm -Rf .objs
	@rm -Rf .deps


ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
-include .deps/test_d10mxfop1awriter.d
endif
