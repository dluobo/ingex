/*
 * $Id: YUV_to_rgb.c,v 1.4 2012/02/10 15:14:59 john_f Exp $
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
                       const int inStride, const int outStride, const int w)
{
    int     acc;
    int     i;
    int     x_max;

    x_max = (w/2) - 1;
    for (i = 0; i < x_max; i++)
    {
        // copy even sample
        *dstLine = *srcLine;
        dstLine += outStride;
        // interpolate odd sample
        acc = *srcLine;
        srcLine += inStride;
        acc += *srcLine;
        *dstLine = (acc + 1) / 2;
        dstLine += outStride;
    }
    // right hand edge padding
    *dstLine = *srcLine;
    dstLine += outStride;
    *dstLine = *srcLine;
    dstLine += outStride;
}

// Super sample a line with high quality filtering
// See http://www.bbc.co.uk/rd/publications/rdreport_1984_04.shtml
static int coefs[] = {-382, 1331, -2930, 5889, -12303, 41163,
                      41163, -12303, 5889, -2930, 1331, -382};
static void h_up_2_HQ(BYTE* srcLine, BYTE* dstLine,
                      const int inStride, const int outStride, const int w)
{
    int     acc;
    int     i, j, k;
    int     x_max;

    x_max = (w/2) - 1;
    // left hand edge padding
    for (i = 0; i <= min(4, x_max); i++)
    {
        // copy even sample
        *dstLine = srcLine[i*inStride];
        dstLine += outStride;
        // interpolate odd sample
        acc = 0;
        for (j = 0; j < 12; j++)
        {
            k = min(max(i + j - 5, 0), x_max);
            acc += srcLine[k*inStride] * coefs[j];
        }
        *dstLine = min(max(acc / 65536, 0), 255);
        dstLine += outStride;
    }
    // main interpolation loop
    for (i = 5; i < x_max - 5; i++)
    {
        // copy even sample
        *dstLine = srcLine[i*inStride];
        dstLine += outStride;
        // interpolate odd sample
        acc = 0;
        for (j = 0; j < 12; j++)
        {
            k = i + j - 5;
            acc += srcLine[k*inStride] * coefs[j];
        }
        *dstLine = min(max(acc / 65536, 0), 255);
        dstLine += outStride;
    }
    // right hand edge padding
    for (i = max(x_max - 5, 5); i <= x_max; i++)
    {
        // copy even sample
        *dstLine = srcLine[i*inStride];
        dstLine += outStride;
        // interpolate odd sample
        acc = 0;
        for (j = 0; j < 12; j++)
        {
            k = min(i + j - 5, x_max);
            acc += srcLine[k*inStride] * coefs[j];
        }
        *dstLine = min(max(acc / 65536, 0), 255);
        dstLine += outStride;
    }
}

typedef void up_2_proc(BYTE*, BYTE*, const int, const int, const int);

int to_RGBex(const YUV_frame* in_frame,
             BYTE* out_R, BYTE* out_G, BYTE* out_B,
             const int RGBpixelStride, const int RGBlineStride,
             const matrices matrix, const int fil)
{
    int         ssx, ssy;
    BYTE*       Y_line;
    BYTE*       U_line;
    BYTE*       V_line;
    BYTE*       U_p;
    BYTE*       V_p;
    BYTE*       R_line;
    BYTE*       G_line;
    BYTE*       B_line;
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
        U_inc = RGBpixelStride;
        V_inc = RGBpixelStride;
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
    #pragma omp parallel for \
        default (shared) \
	private (i,j,Y_line,U_line,V_line,R_line,G_line,B_line, \
	    U_p,V_p,Y,U,V,R,G,B)
    for (j = 0; j < in_frame->Y.h; j++)
    {
        Y_line = in_frame->Y.buff + (j * in_frame->Y.lineStride);
        U_line = in_frame->U.buff + (j * in_frame->U.lineStride);
        V_line = in_frame->V.buff + (j * in_frame->V.lineStride);
        R_line = out_R + (j * RGBlineStride);
        G_line = out_G + (j * RGBlineStride);
        B_line = out_B + (j * RGBlineStride);
        if (ssx == 2)
        {
            U_p = R_line;
            V_p = B_line;
            // up convert chrominance
            up_conv(U_line, U_p, in_frame->U.pixelStride, U_inc, in_frame->Y.w);
            up_conv(V_line, V_p, in_frame->V.pixelStride, V_inc, in_frame->Y.w);
        }
        else
        {
            U_p = U_line;
            V_p = V_line;
        }
        for (i = 0; i < in_frame->Y.w; i++)
        {
            Y = *Y_line;
            U = *U_p;
            V = *V_p;
            Y_line += in_frame->Y.pixelStride;
            U_p += U_inc;
            V_p += V_inc;
            U = U - 128;
            V = V - 128;
            R = Y + (V * VtoR / 65536);
            G = Y - (((V * VtoG) + (U * UtoG)) / 65536);
            B = Y + (U * UtoB / 65536);
            *R_line = min(max(R, 0), 255);
            *G_line = min(max(G, 0), 255);
            *B_line = min(max(B, 0), 255);
            R_line += RGBpixelStride;
            G_line += RGBpixelStride;
            B_line += RGBpixelStride;
        }
    }
    return YUV_OK;
}

int to_RGB(const YUV_frame* in_frame,
           BYTE* out_R, BYTE* out_G, BYTE* out_B,
           const int RGBpixelStride, const int RGBlineStride)
{
    return to_RGBex(in_frame, out_R, out_G, out_B,
                    RGBpixelStride, RGBlineStride, Rec601, 0);
}
