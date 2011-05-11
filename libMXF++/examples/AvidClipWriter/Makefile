#
# $Id: Makefile,v 1.3 2011/05/11 10:40:37 philipn Exp $
#
#
#
# Copyright (C) 2008  BBC Research, Philip de Nier <philipn@users.sourceforge.net>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
TOPLEVEL = ../..
include $(TOPLEVEL)/vars.mk

COMMON_DIR = ../Common

INCLUDES += -I$(COMMON_DIR)

SOURCES = AvidClipWriter.cpp
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))

COMMON_SOURCES = DynamicByteArray.cpp
COMMON_OBJECTS = $(patsubst %.cpp,$(COMMON_DIR)/.objs/%.o,$(COMMON_SOURCES))
DEPENDENCIES += $(patsubst %.cpp,.deps/%.d,$(COMMON_SOURCES))


.PHONY: all
all: libAvidClipWriter.a avidclipwriter


libAvidClipWriter.a: $(OBJECTS) $(COMMON_OBJECTS)
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



avidclipwriter: .objs/main.o libAvidClipWriter.a
	$(COMPILE) .objs/main.o -L. -L$(LIBMXFPP_DIR)/libMXF++ -L$(LIBMXF_DIR)/lib -lAvidClipWriter -lMXF++ -lMXF $(UUIDLIB) -o avidclipwriter


.PHONY: install
install: all
	mkdir -p $(INSTALL_PREFIX)/bin
	cp avidclipwriter $(INSTALL_PREFIX)/bin/

.PHONY: uninstall
uninstall:
	rm -f $(INSTALL_PREFIX)/bin/avidclipwriter

.PHONY: clean
clean:
	@rm -f *~ *.a avidclipwriter
	@rm -Rf .objs
	@rm -Rf .deps


ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
-include .deps/main.d
endif
