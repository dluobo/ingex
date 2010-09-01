/*
 * $Id: FFmpegEncoder.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * FFmpeg encoder
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
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

#ifndef __RECORDER_FFMPEG_ENCODER_H__
#define __RECORDER_FFMPEG_ENCODER_H__


extern "C" {
#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/swscale.h>
#else
#include <libswscale/swscale.h>
#endif
}

#include <ffmpeg_encoder.h>

#include "Types.h"



namespace rec
{


class FFmpegEncoder
{
public:
    FFmpegEncoder(MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster, int num_threads,
                  bool enable_vitc);
    ~FFmpegEncoder();

    bool Encode(const unsigned char *input_data, unsigned int input_data_size,
                unsigned char **output_data, unsigned int *output_data_size);

    bool Encode(const unsigned char *input_data, unsigned int input_data_size, const Timecode *vitc,
                unsigned char **output_data, unsigned int *output_data_size);

private:
    void CreateD10Encoder(MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster, int num_threads);
    bool EncodeD10(const unsigned char *input_data, unsigned int input_data_size,
                   unsigned char **output_data, unsigned int *output_data_size);
    bool EncodeD10(const unsigned char *input_data, unsigned int input_data_size, const Timecode *vitc,
                   unsigned char **output_data, unsigned int *output_data_size);

    void ConvertToYUV422P(struct SwsContext *&convert_context,
                          const unsigned char *input, unsigned int input_height, unsigned int input_offset,
                          unsigned char *output, unsigned int output_height, unsigned int output_offset);

private:
    bool mEnableVITC;

    unsigned int mInputWidth;
    unsigned int mInputHeight;

    ffmpeg_encoder_t *mEncoder;

    struct SwsContext *mConvertContext;
    unsigned char *mYUV422Buffer;
    unsigned int mYUV422BufferSize;
    unsigned int mYUV422ImageHeight;

    unsigned int mVBIImageHeight;
    unsigned int mVBIInputHeightSkip;
    bool mHaveVITCLines;
};


};


#endif
