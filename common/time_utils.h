/*
 * $Id: time_utils.h,v 1.1 2010/03/30 08:07:36 john_f Exp $
 *
 * Time utility functions
 *
 * Copyright (C) 2010 British Broadcasting Corporation
 * All rights reserved
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

#ifndef common_time_utils_h
#define common_time_utils_h

#include <inttypes.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns time-of-day as microseconds */
int64_t gettimeofday64(void);

/* Returns b - a as microseconds */
int64_t tv_diff_microsecs(const struct timeval * a, const struct timeval * b);

#ifdef __cplusplus
}
#endif

#endif //ifndef common_time_utils_h

