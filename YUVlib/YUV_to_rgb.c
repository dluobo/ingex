/*
 * $Id: YUV_to_rgb.c,v 1.3 2011/10/14 09:57:48 john_f Exp $
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
#include "YUV_to_rgb.h"

static inline int max(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int min(int a, int b)
{
    return (a < b) ? a : b;
}

// Super sample a line with (1,2,1)/4 filtering
static void h_up_2_121(BYTE* srcLine, BYTE* dstLine,
                       const int inStride, const int w)
{
    int     acc;
    int     i;
    BYTE    in_0;
    BYTE    in_1;

    in_0 = *srcLine;
    srcLine += inStride;
    for (i = (w/2) - 1; i != 0; i--)
    {
        *dstLine++ = in_0;
        in_1 = *srcLine;
        srcLine += inStride;
        acc = in_0;
        acc += in_1;
        *dstLine++ = (acc + 1) / 2;
        in_0 = in_1;
    }
    *dstLine++ = in_0;
    // repeat last edge sample
    *dstLine = in_0;
}

// Super sample a line with high quality filtering
// See http://www.bbc.co.uk/rd/publications/rdreport_1984_04.shtml
static int coefs[] = {-382, 1331, -2930, 5889, -12303, 41163,
                      41163, -12303, 5889, -2930, 1331, -382};
static void h_up_2_HQ(BYTE* srcLine, BYTE* dstLine,
                      const int inStride, const int w)
{
    int     acc;
    int     i, j, k;
    int     x_max;
    BYTE*   copy_ptr;
    int     in_buff[12];

    x_max = (w/2) - 1;
    copy_ptr = srcLine;
    // preload input buffer with edge value
    for (k = 0; k < 6; k++)
        in_buff[k] = *srcLine;
    // load rest of input buffer
    for (k = 6; k < 12; k++)
    {
        srcLine += inStride;
        in_buff[k] = *srcLine;
    }
    k = 0;
    // main interpolation loop
    for (i = 0; i <= x_max; i++)
    {
        // copy even sample
        *dstLine++ = in_buff[(k + 5) % 12];
        // interpolate odd sample
        acc = 0;
        for (j = 0; j < 12; j++)
        {
            acc += in_buff[(j + k) % 12] * coefs[j];
        }
        *dstLine++ = min(max(acc / 65536, 0), 255);
        // read next input sample
        if (i < x_max - 6)
            srcLine += inStride;
        in_buff[k] = *srcLine;
        k = (k + 1) % 12;
    }
}

typedef void up_2_proc(BYTE*, BYTE*, const int, const int);

int to_RGBex(const YUV_frame* in_frame,
             BYTE* out_R, BYTE* out_G, BYTE* out_B,
             const int RGBpixelStride, const int RGBlineStride,
             const matrices matrix, const int fil, void* workSpace)
{
    int         ssx, ssy;
    BYTE*       Y_line;
    BYTE*       U_line;
    BYTE*       V_line;
    BYTE*       Y_p;
    BYTE*       U_p;
    BYTE*       V_p;
    BYTE*       R_line;
    BYTE*       G_line;
    BYTE*       B_line;
    BYTE*       R_p;
    BYTE*       G_p;
    BYTE*       B_p;
    int         i, j;
    int         U_inc, V_inc;
    int         Y, U, V, R, G, B;
    int         VtoR, VtoG, UtoG, UtoB;
    up_2_proc*  up_conv;

    // check in_frame format
    ssx = in_frame->Y.w / in_frame->U.w;
    ssy = in_frame->Y.h / in_frame->U.h;
    if ((ssx > 2) || (ssy != 1))
        return YUV_Fail;    // can only convert from 4:4:4 or 4:2:2 so far
    // set up intermediate arrays
    if (ssx == 2)
    {
        U_inc = 1;
        V_inc = 1;
    }
    else
    {
        U_inc = in_frame->U.pixelStride;
        V_inc = in_frame->V.pixelStride;
    }
    // set matrix coefficients
    if (matrix == Rec601)
    {
        VtoR = 89831;
        VtoG = 45757;
        UtoG = 22050;
        UtoB = 113538;
    }
    else
    {
        VtoR = 100902;
        VtoG = 29994;
        UtoG = 12002;
        UtoB = 118894;
    }
    // select filter
    if (fil > 0)
    {
        up_conv = &h_up_2_HQ;
    }
    else
    {
        up_conv = &h_up_2_121;
    }
    // do it
    Y_line = in_frame->Y.buff;
    U_line = in_frame->U.buff;
    V_line = in_frame->V.buff;
    R_line = out_R;
    G_line = out_G;
    B_line = out_B;
    for (j = in_frame->Y.h; j != 0; j--)
    {
        Y_p = Y_line;
        if (ssx == 2)
        {
            U_p = workSpace;
            V_p = U_p + in_frame->Y.w;
            // up convert chrominance
            up_conv(U_line, U_p, in_frame->U.pixelStride, in_frame->Y.w);
            up_conv(V_line, V_p, in_frame->V.pixelStride, in_frame->Y.w);
        }
        else
        {
            U_p = U_line;
            V_p = V_line;
        }
        R_p = R_line;
        G_p = G_line;
        B_p = B_line;
        for (i = in_frame->Y.w; i != 0; i--)
        {
            Y = *Y_p;   Y_p += in_frame->Y.pixelStride;
            U = *U_p;   U_p += U_inc;
            V = *V_p;   V_p += V_inc;
            U = U - 128;
            V = V - 128;
            R = Y + (V * VtoR / 65536);
            G = Y - (((V * VtoG) + (U * UtoG)) / 65536);
            B = Y + (U * UtoB / 65536);
            *R_p = min(max(R, 0), 255);
            *G_p = min(max(G, 0), 255);
            *B_p = min(max(B, 0), 255);
            R_p += RGBpixelStride;
            G_p += RGBpixelStride;
            B_p += RGBpixelStride;
        }
        Y_line += in_frame->Y.lineStride;
        U_line += in_frame->U.lineStride;
        V_line += in_frame->V.lineStride;
        R_line += RGBlineStride;
        G_line += RGBlineStride;
        B_line += RGBlineStride;
    }
    return YUV_OK;
}

int to_RGB(const YUV_frame* in_frame,
           BYTE* out_R, BYTE* out_G, BYTE* out_B,
           const int RGBpixelStride, const int RGBlineStride,
           void* workSpace)
{
    return to_RGBex(in_frame, out_R, out_G, out_B,
                    RGBpixelStride, RGBlineStride, Rec601, 0, workSpace);
}
