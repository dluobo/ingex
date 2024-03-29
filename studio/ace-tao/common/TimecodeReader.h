/*
 * $Id: TimecodeReader.h,v 1.2 2008/04/18 16:03:31 john_f Exp $
 *
 * Abstract base for a timecode reader.
 *
 * Copyright (C) 2007  British Broadcasting Corporation.
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

#ifndef TimecodeReader_h
#define TimecodeReader_h

#include <string>

class TimecodeReader
{
public:
    virtual std::string Timecode() = 0;

    virtual ~TimecodeReader() {}
};

#endif //#ifndef TimecodeReader_h

