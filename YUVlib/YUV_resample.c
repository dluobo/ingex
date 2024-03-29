/*
 * $Id: YUV_resample.c,v 1.1 2010/09/29 09:01:13 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
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

#include <string.h>

#include "YUV_frame.h"
#include "YUV_resample.h"

static inline int max(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int min(int a, int b)
{
    return (a < b) ? a : b;
}

// Copy a line
static void h_copy(BYTE* srcLine, BYTE* dstLine,
                   const int inStride, const int outStride,
                   const int w)
{
    int     i;

    if ((inStride == 1) && (outStride == 1))
    {
        memcpy(dstLine, srcLine, w);
        return;
    }
    if ((inStride == 2) && (outStride == 1))
    {
        // common case - UYVY mux to planar layout
        for (i = w; i != 0; i--)
        {
            *dstLine++ = *srcLine++;
            srcLine++;
        }
        return;
    }
    if (outStride == 1)
    {
        for (i = w; i != 0; i--)
        {
            *dstLine++ = *srcLine;
            srcLine += inStride;
        }
        return;
    }
    if (inStride == 1)
    {
        for (i = w; i != 0; i--)
        {
            *dstLine = *srcLine++;
            dstLine += outStride;
        }
        return;
    }
    for (i = w; i != 0; i--)
    {
        *dstLine = *srcLine;
        dstLine += outStride;
        srcLine += inStride;
    }
}

// Super sample a line with (1,2,1)/4 filtering
static void h_up_2_121(BYTE* srcLine, BYTE* dstLine,
                       const int inStride, const int outStride, const int w)
{
    int     acc;
    int     i;
    BYTE    in_0;
    BYTE    in_1;

    in_0 = *srcLine;
    srcLine += inStride;
    for (i = (w/2) - 1; i != 0; i--)
    {
        *dstLine = in_0;
        dstLine += outStride;
        in_1 = *srcLine;
        srcLine += inStride;
        acc = in_0;
        acc += in_1;
        *dstLine = (acc + 1) / 2;
        dstLine += outStride;
        in_0 = in_1;
    }
    *dstLine = in_0;
    dstLine += outStride;
    // repeat last edge sample
    *dstLine = in_0;
}

int to_444(YUV_frame* in_frame, YUV_frame* out_frame,
           void* workSpace)
{
    int     ssx, ssy;
    int     w, h;
    BYTE*   inBuff0;
    BYTE*   inBuff1;
    BYTE*   outBuff0;
    BYTE*   outBuff1;
    int     j;

    // check out_frame is a 4:4:4 format
    ssx = out_frame->Y.w / out_frame->U.w;
    ssy = out_frame->Y.h / out_frame->U.h;
    if ((ssx != 1) || (ssy != 1))
        return YUV_Fail;
    // check in_frame format
    ssx = in_frame->Y.w / in_frame->U.w;
    ssy = in_frame->Y.h / in_frame->U.h;
    if ((ssx != 2) || (ssy != 1))
        return YUV_Fail;    // can only convert from 4:2:2 so far
    // get dimensions of area to be written
    w = min(in_frame->Y.w, out_frame->Y.w);
    h = min(in_frame->Y.h, out_frame->Y.h);
    // copy Y
    inBuff0 = in_frame->Y.buff;
    outBuff0 = out_frame->Y.buff;
    for (j = h; j != 0; j--)
    {
        h_copy(inBuff0, outBuff0,
               in_frame->Y.pixelStride, out_frame->Y.pixelStride, w);
        inBuff0 += in_frame->Y.lineStride;
        outBuff0 += out_frame->Y.lineStride;
    }
    // upsample U & V
    inBuff0 = in_frame->U.buff;
    inBuff1 = in_frame->V.buff;
    outBuff0 = out_frame->U.buff;
    outBuff1 = out_frame->V.buff;
    for (j = h; j != 0; j--)
    {
        h_up_2_121(inBuff0, outBuff0,
                   in_frame->U.pixelStride, out_frame->U.pixelStride, w);
        h_up_2_121(inBuff1, outBuff1,
                   in_frame->V.pixelStride, out_frame->V.pixelStride, w);
        inBuff0 += in_frame->U.lineStride;
        inBuff1 += in_frame->V.lineStride;
        outBuff0 += out_frame->U.lineStride;
        outBuff1 += out_frame->V.lineStride;
    }
    return YUV_OK;
}
