/*
 * $Id: YUV_to_rgb.h,v 1.4 2012/02/10 15:14:59 john_f Exp $
 *
 *
 * Copyright (C) 2009 British Broadcasting Corporation, All Rights Reserved
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

#ifndef __YUVLIB_TO_RGB__
#define __YUVLIB_TO_RGB__

#include "YUV_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Convert YUV to RGB.
 * Input frame can be any 4:4:4 or 4:2:2 sampled YUVlib format.
 * The output frame is assumed to comprise three BYTE arrays, but these
 * will typically be interleaved. The RGBpixelStride value is the address
 * increment used to get from one R, G or B value to the next R, G or B
 * value. This is most likely to be 3, but may sometimes be 4.
 * The RGBlineStride value is the address increment to get from one R, G
 * or B value to the R, G or B value immediately below. It is probably the
 * image width multiplied by RGBpixelStride, but might be rounded up to get
 * a particular alignment in some systems.
 * This routine uses Rec.601 matrix coefficients and signal ranges of
 * 16..235 for R,G,B,Y and -112..112 for U,V
 */
int to_RGB(const YUV_frame* in_frame,
           BYTE* out_R, BYTE* out_G, BYTE* out_B,
           const int RGBpixelStride, const int RGBlineStride);

// recognised colour matrices
typedef enum
{
    Rec601,
    Rec709
} matrices;

/* Convert YUV to RGB.
 * This routine uses Rec.601 (standard def) or Rec.709 (HD) matrix.
 * It can also use a better quality UV interpolation filter, selected by
 * setting 'fil' to any number greater than zero.
 */
int to_RGBex(const YUV_frame* in_frame,
             BYTE* out_R, BYTE* out_G, BYTE* out_B,
             const int RGBpixelStride, const int RGBlineStride,
             const matrices matrix, const int fil);

#ifdef __cplusplus
}
#endif

#endif // __YUVLIB_TO_RGB__
