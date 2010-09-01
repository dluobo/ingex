/*
 * $Id: indexfile.h,v 1.2 2010/09/01 16:05:23 philipn Exp $
 *
 * Boilerplate text added to the LTO index file
 *
 * Copyright (C) 2008 BBC Research, Stuart Cunningham <stuart_hc@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef INDEXFILE_H
#define INDEXFILE_H

#include <string>

using std::string;

extern string boiler_plate;
extern void lto_rebadge_name(const char *lto_id, unsigned file_num, string *p);

#endif // INDEXFILE_H
