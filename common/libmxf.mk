#
# $Id: libmxf.mk,v 1.3 2011/02/18 16:26:54 john_f Exp $
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

# use installed libMXF and libMXF++ (e.g. in /usr/local/include and /usr/local/lib)

LIBMXF_INCLUDE_PATH = $(shell for f in /usr/local/include /usr/include; do test -e $$f/mxf/mxf.h && echo $$f && break; done)
ifeq "$(LIBMXF_INCLUDE_PATH)" ""
$(error libMXF not installed)
endif
LIBMXF_EXAMPLES_INCLUDE_PATH = $(LIBMXF_INCLUDE_PATH)/mxf/examples

LIBMXFPP_INCLUDE_PATH = $(shell for f in /usr/local/include /usr/include; do test -e $$f/libMXF++/MXF.h && echo $$f && break; done)
ifeq "$(LIBMXFPP_INCLUDE_PATH)" ""
$(error libMXF++ not installed)
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

endif

