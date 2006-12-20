/*
 * $Id: DateTime.cpp,v 1.1 2006/12/20 12:28:28 john_f Exp $
 *
 * Date/time to text functions.
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

#include "DateTime.h"

#include <ace/OS_NS_time.h>
#include <ace/OS_NS_stdio.h>

/*
Returns a pointer to a null-terminated string in format YYYYMMDD.
The caller has responsibility for deallocating the memory
by invoking delete [] on the returned pointer.
*/
const char * DateTime::DateNoSeparators()
{
    time_t my_time = ACE_OS::time();
    tm my_tm;
    ACE_OS::localtime_r(&my_time, &my_tm);

    char * p = new char[9];
    ACE_OS::sprintf(p, "%04d%02d%02d",
        my_tm.tm_year + 1900,
        my_tm.tm_mon + 1,
        my_tm.tm_mday
        );

    return p;
}

/*
Returns a pointer to a null-terminated string in format YYYYMMDDHHMM.
The caller has responsibility for deallocating the memory
by invoking delete [] on the returned pointer.
*/
const char * DateTime::DateTimeNoSeparators()
{
    time_t my_time = ACE_OS::time();
    tm my_tm;
    ACE_OS::localtime_r(&my_time, &my_tm);

    char * p = new char[13];
    ACE_OS::sprintf(p, "%04d%02d%02d%02d%02d",
        my_tm.tm_year + 1900,
        my_tm.tm_mon + 1,
        my_tm.tm_mday,
        my_tm.tm_hour,
        my_tm.tm_min
        );

    return p;
}

void DateTime::GetDate(int & year, int & month, int & day)
{
    time_t my_time = ACE_OS::time();
    tm my_tm;
    ACE_OS::localtime_r(&my_time, &my_tm);

    year = my_tm.tm_year + 1900;
    month = my_tm.tm_mon + 1;
    day = my_tm.tm_mday;
}


