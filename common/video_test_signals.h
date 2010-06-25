/*
 * $Id: video_test_signals.h,v 1.3 2010/06/25 13:54:06 philipn Exp $
 *
 * Video test frames
 *
 * Copyright (C) 2005  Stuart Cunningham <stuart_hc@users.sourceforge.net>
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
 
#ifndef __VIDEO_TEST_SIGNALS_H__
#define __VIDEO_TEST_SIGNALS_H__

#include <inttypes.h>

#ifdef __cplusplus
extern "C" 
{
#endif


void uyvy_black_frame(int width, int height, uint8_t *output);
void uyvy_color_bars(int width, int height, int rec601, uint8_t *output);
void uyvy_random_frame(int width, int height, uint8_t *output);

void uyvy_no_video_frame(int width, int height, uint8_t *output);


void yuv420p_black_frame(int width, int height, uint8_t *output);


#ifdef __cplusplus
}
#endif

#endif
