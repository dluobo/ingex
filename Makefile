#
# $Id: Makefile,v 1.5 2010/07/21 18:51:26 john_f Exp $
#
# Makefile for building the Ingex suite of applications
#
# Copyright (C) 2008 British Broadcasting Corporation
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

ifndef USE_INSTALLED_LIBMXF
# Uncomment for the ingex-studio file release
#$(error You need to set the environment variable USE_INSTALLED_LIBMXF)
endif

.PHONY: all
all: studio

.PHONY: studio
studio:
ifndef USE_INSTALLED_LIBMXF
	$(MAKE) -C libMXF
	$(MAKE) -C libMXF++
endif
	$(MAKE) -C common
	$(MAKE) -C player
	$(MAKE) -C studio

.PHONY: archive
archive:
ifndef USE_INSTALLED_LIBMXF
	$(MAKE) -C libMXF
	$(MAKE) -C libMXF++
endif
	$(MAKE) -C common
	$(MAKE) -C player
	$(MAKE) -C archive/src

# do 'realclean' for studio/ace-tao to delete files generated from IDL
.PHONY: clean
clean:
ifndef USE_INSTALLED_LIBMXF
	$(MAKE) -C libMXF $@
	$(MAKE) -C libMXF++ $@
endif
	$(MAKE) -C common $@
	$(MAKE) -C player $@
	$(MAKE) -C studio realclean
	$(MAKE) -C archive/src $@

# So far, only libMXF and common have make check targets
.PHONY: check
check: all
ifndef USE_INSTALLED_LIBMXF
	$(MAKE) -C libMXF $@
endif
	$(MAKE) -C common $@

.PHONY: valgrind-check
valgrind-check: all
ifndef USE_INSTALLED_LIBMXF
	$(MAKE) -C libMXF $@
endif
	$(MAKE) -C common $@

