/*
 * $Id: ElapsedTimeReporter.h,v 1.1 2010/10/18 17:46:27 john_f Exp $
 *
 * Class to report elapsed time between construction and destruction
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
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

#ifndef ElapsedTimeReporter_h
#define ElapsedTimeReporter_h

#include <string>
#include <inttypes.h>


class ElapsedTimeReporter
{
public:
    ElapsedTimeReporter(int64_t threshold, const char * format, ...);
    ~ElapsedTimeReporter();
private:
    int64_t mThreshold;
    int64_t mStart;
    std::string mMessage;
};

#endif //ifndef ElapsedTimeReporter_h

