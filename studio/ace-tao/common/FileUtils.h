/*
 * $Id: FileUtils.h,v 1.1 2007/09/11 14:08:33 stuart_hc Exp $
 *
 * File utilities.
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef FileUtils_h
#define FileUtils_h

#include <string>

class FileUtils
{
public:
    static bool CreatePath(const std::string & path);
    static bool CreateUniquePath(std::string & path);
};

#endif //#ifndef FileUtils_h
