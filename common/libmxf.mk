#
# $Id: libmxf.mk,v 1.2 2010/07/21 16:29:33 john_f Exp $
#
# Setup libMXF and libMXF++ includes and libs
#
# Copyright (C) 2010 British Broadcasting Corporation
# All rights reserved
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

ifneq ($(MAKECMDGOALS),clean)

ifdef USE_INSTALLED_LIBMXF
    # use installed libMXF and libMXF++ (e.g. in /usr/local/include and /usr/local/lib)

    LIBMXF_INCLUDE_PATH = $(shell for f in /usr/local/include /usr/include; do test -e $$f/mxf/mxf.h && echo $$f && break; done)
    ifeq "$(LIBMXF_INCLUDE_PATH)" ""
        $(error libMXF not installed. Install libMXF or unset USE_INSTALLED_LIBMXF environment variable)
    endif
    LIBMXF_EXAMPLES_INCLUDE_PATH = $(LIBMXF_INCLUDE_PATH)/mxf/examples

    LIBMXFPP_INCLUDE_PATH = $(shell for f in /usr/local/include /usr/include; do test -e $$f/libMXF++/MXF.h && echo $$f && break; done)
    ifeq "$(LIBMXFPP_INCLUDE_PATH)" ""
        $(error libMXF++ not installed. Install libMXF++ or set USE_SOURCE_LIBMXF environment variable)
    endif
    LIBMXFPP_EXAMPLES_INCLUDE_PATH = $(LIBMXFPP_INCLUDE_PATH)/libMXF++/examples


    LIBMXF_LIB = -lMXF -luuid
    LIBMXFPP_LIB = -lMXF++

    ARCHIVEMXF_INCLUDE = -I$(LIBMXF_EXAMPLES_INCLUDE_PATH)/archive
    WRITEARCHIVEMXF_LIB = -lwritearchivemxf
    WRITEARCHIVEMXF_INCLUDE = $(ARCHIVEMXF_INCLUDE) -I$(LIBMXF_EXAMPLES_INCLUDE_PATH)/archive/write
    ARCHIVEMXFINFO_LIB = -larchivemxfinfo
    ARCHIVEMXFINFO_INCLUDE = $(ARCHIVEMXF_INCLUDE) -I$(LIBMXF_EXAMPLES_INCLUDE_PATH)/archive/info

    MXFREADER_INCLUDE = -I$(LIBMXF_EXAMPLES_INCLUDE_PATH)/reader
    MXFREADER_LIB = -lMXFReader

    WRITEAVIDMXF_INCLUDE = -I$(LIBMXF_EXAMPLES_INCLUDE_PATH)/writeavidmxf
    WRITEAVIDMXF_LIB = -lwriteavidmxf
    
    LIBMXFPPEXAMPLESCOMMON_INCLUDE = -I$(LIBMXFPP_EXAMPLES_INCLUDE_PATH)/Common

    D10MXFOP1AWRITER_INCLUDE = -I$(LIBMXFPP_EXAMPLES_INCLUDE_PATH)/D10MXFOP1AWriter
    D10MXFOP1AWRITER_LIB = -lD10MXFOP1AWriter
    
    OPATOMREADER_INCLUDE = -I$(LIBMXFPP_EXAMPLES_INCLUDE_PATH)/OPAtomReader
    OPATOMREADER_LIB = -lOPAtomReader

else
    # use libMXF and libMXF++ in the source tree

    INGEX_ROOT := $(shell echo $(CURDIR) | sed -e 's,/common/.*,,' -e 's,/player/.*,,' -e 's,/studio/.*,,' -e 's,/archive/.*,,')

    LIBMXF_EXAMPLES_PATH = $(INGEX_ROOT)/libMXF/examples
    LIBMXFPP_EXAMPLES_PATH = $(INGEX_ROOT)/libMXF++/examples


    LIBMXF_INCLUDE = -I$(INGEX_ROOT)/libMXF/lib/include
    LIBMXF_LIB = $(INGEX_ROOT)/libMXF/lib/libMXF.a -luuid
    LIBMXFPP_INCLUDE = -I$(INGEX_ROOT)/libMXF++
    LIBMXFPP_LIB = $(INGEX_ROOT)/libMXF++/libMXF++/libMXF++.a

    ARCHIVEMXF_INCLUDE = -I$(LIBMXF_EXAMPLES_PATH)/archive
    WRITEARCHIVEMXF_LIB = $(LIBMXF_EXAMPLES_PATH)/archive/write/libwritearchivemxf.a
    WRITEARCHIVEMXF_INCLUDE = $(ARCHIVEMXF_INCLUDE) -I$(LIBMXF_EXAMPLES_PATH)/archive/write
    ARCHIVEMXFINFO_LIB = $(LIBMXF_EXAMPLES_PATH)/archive/info/libarchivemxfinfo.a
    ARCHIVEMXFINFO_INCLUDE = $(ARCHIVEMXF_INCLUDE) -I$(LIBMXF_EXAMPLES_PATH)/archive/info

    MXFREADER_INCLUDE = -I$(LIBMXF_EXAMPLES_PATH)/reader
    MXFREADER_LIB = $(LIBMXF_EXAMPLES_PATH)/reader/libMXFReader.a

    WRITEAVIDMXF_INCLUDE = -I$(LIBMXF_EXAMPLES_PATH)/writeavidmxf
    WRITEAVIDMXF_LIB = $(LIBMXF_EXAMPLES_PATH)/writeavidmxf/libwriteavidmxf.a
    
    LIBMXFPPEXAMPLESCOMMON_INCLUDE = -I$(LIBMXFPP_EXAMPLES_PATH)/Common

    D10MXFOP1AWRITER_INCLUDE = -I$(LIBMXFPP_EXAMPLES_PATH)/D10MXFOP1AWriter
    D10MXFOP1AWRITER_LIB = $(LIBMXFPP_EXAMPLES_PATH)/D10MXFOP1AWriter/libD10MXFOP1AWriter.a
    
    OPATOMREADER_INCLUDE = -I$(LIBMXFPP_EXAMPLES_PATH)/OPAtomReader
    OPATOMREADER_LIB = $(LIBMXFPP_EXAMPLES_PATH)/OPAtomReader/libOPAtomReader.a

endif

endif

