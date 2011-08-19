/*
 * $Id: YUV_deinterlace.h,v 1.1 2011/08/19 10:53:38 john_f Exp $
 *
 *
 *
 * Copyright (C) 2011 British Broadcasting Corporation, All Rights Reserved
 * Author: Jim Easterbrook
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

#ifndef __YUVLIB_DEINTERLACE__
#define __YUVLIB_DEINTERLACE__

#include "YUV_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Extract one field from two adjacent frames using a 'Weston 3 field' filter.
 * Set 'top' to True to extract the top field, False to extract the bottom
 * field. 'adj_frame' should point to the previous or next frame, depending on
 * the field dominance.
 * 'fil' selects the filter to be used. 0 == 'simple', 1 == 'more complex'.
 * 'work_space' is allocated by the caller and must be large enough to store
 * one line of output as int32_t, i.e. (w * 4) bytes.
 */

int deinterlace_component(const component* in_frame,
                          const component* adj_frame,
                          const component* out_frame,
                          const int top, const int fil, void* work_space);

int deinterlace_pic(const YUV_frame* in_frame,
                    const YUV_frame* adj_frame,
                    const YUV_frame* out_frame,
                    const int top, const int fil, void* work_space);

#ifdef __cplusplus
}
#endif

#endif // __YUVLIB_DEINTERLACE__
