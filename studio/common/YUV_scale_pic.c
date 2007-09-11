/*
 * $Id: YUV_scale_pic.c,v 1.1 2007/09/11 14:08:36 stuart_hc Exp $
 *
 * Part of YUVlib.
 *
 * Copyright (C) 2007  British Broadcasting Corporation.
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

#include <string.h>	// for memset
#include <inttypes.h>	// for uint32_t

#include "YUV_frame.h"
#include "YUV_scale_pic.h"

static inline int max(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int min(int a, int b)
{
    return (a < b) ? a : b;
}

// Sub sample a line with no filtering
static void h_sub_alias(BYTE* srcLine, BYTE* dstLine,
                        const int inStride, const int outStride,
                        const int xup, const int xdown, const int w)
{
    int		i;
    int		dx_int;
    int		dx_frac;
    int		err;

    dx_int = xdown / xup; // whole number of input samples per output sample
    dx_frac = xdown - (dx_int * xup); // fractional part of same
    err = xup / 2; // start at centre of first input sample group
    for (i = 0; i < w; i++)
    {
        *dstLine = *srcLine;
        dstLine += outStride;
        srcLine += inStride * dx_int;
        err += dx_frac;
        if (err >= xup)
        {
            err -= xup;
            srcLine += inStride;
        }
    }
}

// same as above, but scales input by 256
static void h_sub_alias2(BYTE* srcLine, uint32_t* dstLine,
                         const int inStride,
                         int xup, int xdown, const int w)
{
    int		i;
    int		dx_int;
    int		dx_frac;
    int		err;

    dx_int = xdown / xup; // whole number of input samples per output sample
    dx_frac = xdown - (dx_int * xup); // fractional part of same
    err = xup / 2; // start at centre of first input sample group
    for (i = 0; i < w; i++)
    {
        *dstLine++ = *srcLine * 256;
        srcLine += inStride * dx_int;
        err += dx_frac;
        if (err >= xup)
        {
            err -= xup;
            srcLine += inStride;
        }
    }
}

// Sub sample a line with averaging
static void h_sub_ave(BYTE* srcLine, uint32_t* dstLine,
                      const int inStride,
                      int xup, int xdown, const int w)
{
    int		i;
    int		err;
    int		gain;
    int		scale;
    uint32_t	part;
    uint32_t	whole;
    uint32_t	acc;

    // start at centre of first input sample group
    err = (xup - xdown) / 2;
    // initialise accumulated output with edge padding
    scale = 256 - ((256 * err) / xup);
    acc = scale * *srcLine;
    srcLine += inStride;
    err -= xup;
    gain = scale;
    for (i = 0; i < w; i++)
    {
        err += xdown;
        while (err >= xup)
        {
            // add next input to accumulated input
            acc += 256 * *srcLine;
            srcLine += inStride;
            err -= xup;
            gain += 256;
        }
        // divide next sample between acumulated input and next output and save
        // this output
        scale = (256 * err) / xup;
        whole = 256 * *srcLine;
        srcLine += inStride;
        err -= xup;
        part = (whole * scale) / 256;
        gain += scale;
        *dstLine++ = ((acc + part) * 256) / gain;
        acc = whole - part; // residue of input sample
        gain = 256 - scale; // residue of scale
    }
}


static void v_sub_alias(component* inFrame, component* outFrame,
                        const int hfil, int xup, int xdown,
                        int yup, int ydown, int yoff, uint32_t** work)
{
    BYTE*	inBuff = inFrame->buff;
    BYTE*	outBuff = outFrame->buff;
    int		i, j;
    int		dy_int;
    int		dy_frac;
    int		err;
    uint32_t*	ptrA;
    BYTE*	outPtr;

    // get whole and scaled fractional part of down:up ratio
    dy_int = ydown / yup;
    dy_frac = ydown - (dy_int * yup);
    // start at centre of first pixel
    err = yup / 2;
    // adjust for offset (percentage of output sample)
    err += ((ydown - yup) * yoff) / 100;
    for (j = 0; j < outFrame->h; j++)
    {
        if (hfil <= 0)
        {
            h_sub_alias(inBuff, outBuff,
                        inFrame->pixelStride, outFrame->pixelStride,
                        xup, xdown, outFrame->w);
        }
        else
        {
            h_sub_ave(inBuff, work[0], inFrame->pixelStride,
                      xup, xdown, outFrame->w);
            ptrA = work[0];
            outPtr = outBuff;
            for (i = 0; i < outFrame->w; i++)
            {
                *outPtr = *ptrA++ / 256;
                outPtr += outFrame->pixelStride;
            }
        }
        outBuff += outFrame->lineStride;
        inBuff += inFrame->lineStride * dy_int;
        err += dy_frac;
        while (err >= yup)
        {
            err -= yup;
            inBuff += inFrame->lineStride;
        }
    }
}

typedef void sub_line_proc(BYTE*, uint32_t*, const int, int, int, const int);

static void v_sub_ave(component* inFrame, component* outFrame,
                      const int hfil, int xup, int xdown,
                      int yup, int ydown, int yoff, uint32_t** work)
{
    BYTE*	inBuff = inFrame->buff;
    BYTE*	outBuff = outFrame->buff;
    int		i, j;
    int		err;
    int		gain;
    int		scale;
    uint32_t*	ptrA;
    uint32_t*	ptrB;
    uint32_t	part;
    BYTE*	outPtr;
    sub_line_proc* h_sub;

    // select horizontal subsampling routine
    if (hfil <= 0)
        h_sub = &h_sub_alias2;
    else
        h_sub = &h_sub_ave;
    // start at centre of first input sample group
    err = (yup - ydown) / 2;
    // adjust for offset (percentage of output sample)
    err += ((ydown - yup) * yoff) / 100;
    // initialise accumulated output, with edge padding
    scale = 256 - ((256 * err) / yup);
    h_sub(inBuff, work[1],
          inFrame->pixelStride, xup, xdown, outFrame->w);
    inBuff += inFrame->lineStride;
    err -= yup;
    ptrA = work[0];
    ptrB = work[1];
    for (i = 0; i < outFrame->w; i++)
        *ptrA++ = (*ptrB++ * scale) / 256;
    gain = scale;
    for (j = 0; j < outFrame->h; j++)
    {
        err += ydown;
        while (err >= yup)
        {
            // get next line
            h_sub(inBuff, work[1],
                  inFrame->pixelStride, xup, xdown, outFrame->w);
            inBuff += inFrame->lineStride;
            err -= yup;
            // add to accumulated input
            ptrA = work[0];
            ptrB = work[1];
            for (i = 0; i < outFrame->w; i++)
                *ptrA++ += *ptrB++;
            gain += 256;
        }
        scale = (256 * err) / yup;
        // get last line of input for this output
        h_sub(inBuff, work[1],
              inFrame->pixelStride, xup, xdown, outFrame->w);
        inBuff += inFrame->lineStride;
        err -= yup;
        // divide between acumulated input and next output and write output
        gain += scale;
        ptrA = work[0];
        ptrB = work[1];
        outPtr = outBuff;
        for (i = 0; i < outFrame->w; i++)
        {
            part = (*ptrB * scale) / 256;
            *outPtr = (*ptrA + part) / gain;
            outPtr += outFrame->pixelStride;
            *ptrA++ = *ptrB++ - part; // residue of input sample
        }
        outBuff += outFrame->lineStride;
        gain = 256 - scale; // residue of scale
    }
}

static void scale_comp(component* inFrame, component* outFrame,
                       const int hfil, const int vfil,
                       int xup, int xdown, int yup, int ydown,
                       int yoff, uint32_t** work)
{
    if (vfil <= 0)
        v_sub_alias(inFrame, outFrame, hfil, xup, xdown, yup, ydown, yoff, work);
    else
        v_sub_ave(inFrame, outFrame, hfil, xup, xdown, yup, ydown, yoff, work);
}

int scale_pic(YUV_frame* in_frame, YUV_frame* out_frame,
              int x, int y, int w, int h,
              int intlc, int hfil, int vfil, void* workSpace)
{
    int		ssx, ssy;
    YUV_frame	sub_frame;
    uint32_t*	work[2];
    int		xup, xdown;
    int		yup, ydown;

    xup = w;
    xdown = in_frame->Y.w;
    yup = h;
    ydown = in_frame->Y.h;
    // make up and down numbers even, for later convenience
    xup = xup * 2;
    xdown = xdown * 2;
    yup = yup * 2;
    ydown = ydown * 2;
    ssx = in_frame->Y.w / in_frame->U.w;
    ssy = in_frame->Y.h / in_frame->U.h;
    // adjust position, if required
    x = max(x, 0);
    y = max(y, 0);
    x -= x % ssx;
    y -= y % ssy;
    if (intlc)
        y -= y % 2;
    // create a sub frame representing the portion of out_frame to be written
    sub_frame = *out_frame;
    sub_frame.Y.buff += (y * sub_frame.Y.lineStride) +
                        (x * sub_frame.Y.pixelStride);
    sub_frame.U.buff += ((y/ssy) * sub_frame.U.lineStride) +
                        ((x/ssx) * sub_frame.U.pixelStride);
    sub_frame.V.buff += ((y/ssy) * sub_frame.V.lineStride) +
                        ((x/ssx) * sub_frame.V.pixelStride);
    sub_frame.Y.w = min(w, out_frame->Y.w - x);
    sub_frame.Y.h = min(h, out_frame->Y.h - y);
    sub_frame.U.w = min(w / ssx, out_frame->U.w - (x/ssx));
    sub_frame.U.h = min(h / ssy, out_frame->U.h - (y/ssy));
    sub_frame.V.w = min(w / ssx, out_frame->V.w - (x/ssx));
    sub_frame.V.h = min(h / ssy, out_frame->V.h - (y/ssy));
    work[0] = workSpace;
    work[1] = work[0] + w;
    if (intlc)
    {
        component	in_field;
        component	out_field;
        // "first" field i.e. "even" lines"
        extract_field(&in_frame->Y, &in_field, 0);
        extract_field(&sub_frame.Y, &out_field, 0);
        scale_comp(&in_field, &out_field, hfil, vfil,
                   xup, xdown, yup, ydown, 0, work);
        extract_field(&in_frame->U, &in_field, 0);
        extract_field(&sub_frame.U, &out_field, 0);
        scale_comp(&in_field, &out_field, hfil, vfil,
                   xup, xdown, yup, ydown, 0, work);
        extract_field(&in_frame->V, &in_field, 0);
        extract_field(&sub_frame.V, &out_field, 0);
        scale_comp(&in_field, &out_field, hfil, vfil,
                   xup, xdown, yup, ydown, 0, work);
        // "second" field i.e. "odd" lines"
        extract_field(&in_frame->Y, &in_field, 1);
        extract_field(&sub_frame.Y, &out_field, 1);
        scale_comp(&in_field, &out_field, hfil, vfil,
                   xup, xdown, yup, ydown, 50, work);
        extract_field(&in_frame->U, &in_field, 1);
        extract_field(&sub_frame.U, &out_field, 1);
        scale_comp(&in_field, &out_field, hfil, vfil,
                   xup, xdown, yup, ydown, 50, work);
        extract_field(&in_frame->V, &in_field, 1);
        extract_field(&sub_frame.V, &out_field, 1);
        scale_comp(&in_field, &out_field, hfil, vfil,
                   xup, xdown, yup, ydown, 50, work);
    }
    else // not interlaced
    {
        scale_comp(&in_frame->Y, &sub_frame.Y, hfil, vfil,
                   xup, xdown, yup, ydown, 0, work);
        scale_comp(&in_frame->U, &sub_frame.U, hfil, vfil,
                   xup, xdown, yup, ydown, 0, work);
        scale_comp(&in_frame->V, &sub_frame.V, hfil, vfil,
                   xup, xdown, yup, ydown, 0, work);
    }
    return 0;
}
