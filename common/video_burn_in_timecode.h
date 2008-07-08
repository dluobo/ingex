/*
 * $Id: video_burn_in_timecode.h,v 1.1 2008/07/08 14:59:17 philipn Exp $
 *
 * Quick and dirty timecode burning for testing purposes
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
 
#ifndef __VIDEO_BURN_IN_TIMECODE_H__
#define __VIDEO_BURN_IN_TIMECODE_H__

#ifdef __cplusplus
extern "C" 
{
#endif

void burn_mask_yuv420(int frame_number, int x_offset, int y_offset, int frame_width, int frame_height, unsigned char *frame);
void burn_mask_yuv422(int frame_number, int x_offset, int y_offset, int frame_width, int frame_height, unsigned char *frame);
void burn_mask_uyvy(int frame_number, int x_offset, int y_offset, int frame_width, int frame_height, unsigned char *frame);

#ifdef __cplusplus
}
#endif

#endif
