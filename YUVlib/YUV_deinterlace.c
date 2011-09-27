/*
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

#include <string.h> // for memcpy, bzero
#include <stdint.h> // for int32_t
// #include <stdio.h>

#include "YUV_frame.h"
#include "YUV_deinterlace.h"

static inline int max(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int min(int a, int b)
{
    return (a < b) ? a : b;
}

// Copy a line from frame to frame
static void copy_line(const BYTE* in_line, const int in_stride,
                      BYTE* out_line, const int out_stride, const int w)
{
    int     i;

    if (in_stride == 1 && out_stride == 1)
    {
        memcpy(out_line, in_line, w);
        return;
    }
    for (i = 0; i < w; i++)
    {
        *out_line = *in_line;
        in_line += in_stride;
        out_line += out_stride;
    }
}

// Copy a line from work space to frame, scaling down by 256 * 256
static void scale_copy_line(const int32_t* in_line, BYTE* out_line,
                            const int out_stride, const int w)
{
    int     i;

    for (i = 0; i < w; i++)
    {
        *out_line = min(max(*in_line++ / (256 * 256), 0), 255);
        out_line += out_stride;
    }
}

// Accumulate a line from frame to work_space, with a gain factor
static void acc_line(const BYTE* in_line, const int in_stride,
                     int32_t* out_line, const int gain, const int w)
{
    int i;

    for (i = 0; i < w; i++)
    {
        *out_line++ += *in_line * gain;
        in_line += in_stride;
    }
}

int deinterlace_component(const component* in_frame,
                          const component* adj_frame,
                          const component* out_frame,
                          const int top, const int fil, void* work_space)
{
    BYTE*   in_line;
    BYTE*   out_line;
    int     w, h;
    int     f, j, y_in, y_out;
    // filter coefficients from PH-2071, scaled by 256*256
    int     n_coef_lf[2] = {2, 4};
    int32_t coef_lf[2][4] = {{32768, 32768, 0, 0},
                             {-1704, 34472, 34472, -1704}};
    int     n_coef_hf[2] = {3, 5};
    int32_t coef_hf[2][5] = {{-4096, 8192, -4096, 0, 0},
                             {2032, -7602, 11140, -7602, 2032}};

    w = min(in_frame->w, adj_frame->w);
    h = min(in_frame->h, adj_frame->h);
    w = min(w, out_frame->w);
    h = min(h, out_frame->h);
    f = min(max(fil, 0), 1);
    // copy one field
    if (top)
        y_out = 0;
    else
        y_out = 1;
    in_line = in_frame->buff + (y_out * in_frame->lineStride);
    out_line = out_frame->buff + (y_out * out_frame->lineStride);
    while (y_out < h)
    {
        copy_line(in_line, in_frame->pixelStride,
                  out_line, out_frame->pixelStride, w);
        y_out += 2;
        in_line += in_frame->lineStride * 2;
        out_line += out_frame->lineStride * 2;
    }
    // interpolate other field
    if (top)
        y_out = 1;
    else
        y_out = 0;
    out_line = out_frame->buff + (y_out * out_frame->lineStride);
    while (y_out < h)
    {
        // clear workspace
        bzero(work_space, sizeof(int32_t) * w);
        // get low vertical frequencies from current field
        for (j = 0; j < n_coef_lf[f]; j++)
        {
            y_in = (y_out + 1) + (j * 2) - n_coef_lf[f];
            while (y_in < 0)
                y_in += 2;
            while (y_in >= h)
                y_in -= 2;
            in_line = in_frame->buff + (y_in * in_frame->lineStride);
            acc_line(in_line, in_frame->pixelStride,
                     work_space, coef_lf[f][j], w);
        }
        // get high vertical frequencies from adjacent fields
        for (j = 0; j < n_coef_hf[f]; j++)
        {
            y_in = (y_out + 1) + (j * 2) - n_coef_hf[f];
            while (y_in < 0)
                y_in += 2;
            while (y_in >= h)
                y_in -= 2;
            in_line = in_frame->buff + (y_in * in_frame->lineStride);
            acc_line(in_line, in_frame->pixelStride,
                     work_space, coef_hf[f][j], w);
            in_line = adj_frame->buff + (y_in * adj_frame->lineStride);
            acc_line(in_line, adj_frame->pixelStride,
                     work_space, coef_hf[f][j], w);
        }
        // save scaled result
        scale_copy_line(work_space, out_line, out_frame->pixelStride, w);
        y_out += 2;
        out_line += out_frame->lineStride * 2;
    }
    return YUV_OK;
}

int deinterlace_pic(const YUV_frame* in_frame,
                    const YUV_frame* adj_frame,
                    const YUV_frame* out_frame,
                    const int top, const int fil, void* work_space)
{
    int     result;

    result = deinterlace_component(&in_frame->Y, &adj_frame->Y,
                                   &out_frame->Y, top, fil, work_space);
    if (result != YUV_OK)
        return result;
    result = deinterlace_component(&in_frame->U, &adj_frame->U,
                                   &out_frame->U, top, fil, work_space);
    if (result != YUV_OK)
        return result;
    result = deinterlace_component(&in_frame->V, &adj_frame->V,
                                   &out_frame->V, top, fil, work_space);
    return result;
}
