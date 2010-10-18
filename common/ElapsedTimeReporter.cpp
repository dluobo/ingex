/*
 * $Id: ElapsedTimeReporter.cpp,v 1.1 2010/10/18 17:46:27 john_f Exp $
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

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include "ElapsedTimeReporter.h"
#include "time_utils.h"

#include <cstdio>
#include <cstdarg>

ElapsedTimeReporter::ElapsedTimeReporter(int64_t threshold, const char * format, ...)
: mThreshold(threshold)
{
    // store the message
    const size_t MAX_LENGTH = 200;
    char message[MAX_LENGTH];
    va_list ap;
    va_start(ap, format);
    vsnprintf(message, MAX_LENGTH, format, ap);
    va_end(ap);
    mMessage = message;

    // store the start time
    mStart = gettimeofday64();
}

ElapsedTimeReporter::~ElapsedTimeReporter()
{
    int64_t elapsed_time = gettimeofday64() - mStart;

    if (elapsed_time > mThreshold)
    {
        fprintf(stderr, "%s Elapsed time %"PRId64" microseconds\n", mMessage.c_str(), elapsed_time);
    }
}

