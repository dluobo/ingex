/*
 * $Id: FFmpegEncoder.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

extern "C" {
#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/swscale.h>
#include <ffmpeg/avcodec.h>
#else
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#endif
}

#include <video_VITC.h>
#include <video_test_signals.h>

#include "FFmpegEncoder.h"
#include "RecorderException.h"
#include "Logging.h"

using namespace rec;



FFmpegEncoder::FFmpegEncoder(MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster, int num_threads,
                             bool enable_vitc)
{
    mEnableVITC = enable_vitc;
    mInputWidth = 0;
    mInputHeight = 0;
    mEncoder = 0;
    mConvertContext = 0;
    mYUV422Buffer = 0;
    mYUV422BufferSize = 0;
    mYUV422ImageHeight = 0;
    mVBIImageHeight = 0;
    mVBIInputHeightSkip = 0;
    mHaveVITCLines = false;

    // TODO: support other formats
    // TODO: VBI data for PAL_592 and PAL_608 is ignored
    REC_ASSERT(res == MaterialResolution::IMX50_MXF_1A);
    REC_ASSERT(raster == Ingex::VideoRaster::PAL ||
               raster == Ingex::VideoRaster::PAL_592 ||
               raster == Ingex::VideoRaster::PAL_608);
    CreateD10Encoder(res, raster, num_threads);
}

FFmpegEncoder::~FFmpegEncoder()
{
    ffmpeg_encoder_close(mEncoder);

    if (mConvertContext)
        sws_freeContext(mConvertContext);

    delete [] mYUV422Buffer;
}

bool FFmpegEncoder::Encode(const unsigned char *input_data, unsigned int input_data_size,
                           unsigned char **output_data, unsigned int *output_data_size)
{
    return EncodeD10(input_data, input_data_size, output_data, output_data_size);
}

bool FFmpegEncoder::Encode(const unsigned char *input_data, unsigned int input_data_size, const Timecode *vitc,
                           unsigned char **output_data, unsigned int *output_data_size)
{
    return EncodeD10(input_data, input_data_size, vitc, output_data, output_data_size);
}

void FFmpegEncoder::CreateD10Encoder(MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster,
                                     int num_threads)
{
    Ingex::Interlace::EnumType interlace;
    int fps_num, fps_den;
    int width, height;
    Ingex::VideoRaster::GetInfo(raster, width, height, fps_num, fps_den, interlace);
    mInputWidth = width;
    mInputHeight = height;

    if (mEnableVITC) {
        // add a 32 line VBI above the input data and write VITC to the VBI

        REC_ASSERT(mInputHeight >= 576 && mInputHeight <= 608);

        mYUV422ImageHeight = 608;
        mYUV422BufferSize = mInputWidth * mYUV422ImageHeight * 2;
        mYUV422Buffer = new unsigned char[mYUV422BufferSize];

        mVBIImageHeight = 32;
        mVBIInputHeightSkip = mInputHeight - 576;
        
        // set VBI to black
        memset(mYUV422Buffer, 0x10, mVBIImageHeight * mInputWidth);
        memset(mYUV422Buffer + mYUV422ImageHeight * mInputWidth, 0x80, mVBIImageHeight * mInputWidth);
        memset(mYUV422Buffer + mYUV422ImageHeight * mInputWidth * 3/2, 0x80, mVBIImageHeight * mInputWidth);

        mEncoder = ffmpeg_encoder_init(res, Ingex::VideoRaster::PAL_608, num_threads);
        REC_CHECK(mEncoder);
    } else {
        mYUV422ImageHeight = mInputHeight;
        mYUV422BufferSize = mInputWidth * mYUV422ImageHeight * 2;
        mYUV422Buffer = new unsigned char[mYUV422BufferSize];

        mEncoder = ffmpeg_encoder_init(res, raster, num_threads);
        REC_CHECK(mEncoder);
    }
}

bool FFmpegEncoder::EncodeD10(const unsigned char *input_data, unsigned int input_data_size,
                              unsigned char **output_data, unsigned int *output_data_size)
{
    REC_ASSERT(!mEnableVITC);
    // TODO: D10MXFFrameWriter sets input_data_size to 0
    //REC_CHECK(input_data_size >= mInputWidth * mInputHeight * 2);

    // convert UYVY to YUV422P
    ConvertToYUV422P(mConvertContext, input_data, mInputHeight, 0, mYUV422Buffer, mYUV422ImageHeight, 0);

    // D10 encode
    int compressed_size = ffmpeg_encoder_encode(mEncoder, mYUV422Buffer, output_data);
    if (compressed_size <= 0)
        return false;

    *output_data_size = compressed_size;
    return true;
}

bool FFmpegEncoder::EncodeD10(const unsigned char *input_data, unsigned int input_data_size, const Timecode *vitc,
                              unsigned char **output_data, unsigned int *output_data_size)
{
    REC_ASSERT(mEnableVITC);
    // TODO: D10MXFFrameWriter sets input_data_size to 0
    //REC_CHECK(input_data_size >= mInputWidth * mInputHeight * 2);

    if (vitc) {
        // add VITC to VBI (VITC on line 19 and 21; active image starts at line 23)
        REC_ASSERT(mVBIImageHeight >= ((23 - 19) * 2));
        write_vitc(vitc->hour, vitc->min, vitc->sec, vitc->frame,
                   mYUV422Buffer + (mVBIImageHeight - ((23 - 19) * 2)) * mInputWidth, 0);
        write_vitc(vitc->hour, vitc->min, vitc->sec, vitc->frame,
                   mYUV422Buffer + (mVBIImageHeight - ((23 - 21) * 2)) * mInputWidth, 0);
        mHaveVITCLines = true;
    } else {
        // VITC not present
        if (mHaveVITCLines) {
            // set Y to black
            memset(mYUV422Buffer + (mVBIImageHeight - ((23 - 19) * 2)) * mInputWidth, 0x10, 2 * mInputWidth);
            memset(mYUV422Buffer + (mVBIImageHeight - ((23 - 21) * 2)) * mInputWidth, 0x10, 2 * mInputWidth);
            mHaveVITCLines = false;
        }
    }


    // convert input UYVY to YUV422P
    ConvertToYUV422P(mConvertContext, input_data, mInputHeight, mVBIInputHeightSkip,
                     mYUV422Buffer, mYUV422ImageHeight, mVBIImageHeight);

    // D10 encode
    int compressed_size = ffmpeg_encoder_encode(mEncoder, mYUV422Buffer, output_data);
    if (compressed_size <= 0)
        return false;

    *output_data_size = compressed_size;
    return true;

}

void FFmpegEncoder::ConvertToYUV422P(struct SwsContext *&convert_context,
                                     const unsigned char *input, unsigned int input_height, unsigned int input_offset,
                                     unsigned char *output, unsigned int output_height, unsigned int output_offset)
{
    unsigned int conversion_height = input_height - input_offset;

    AVPicture from_pict;
    from_pict.data[0] = (uint8_t*)input + input_offset * mInputWidth * 2;
    from_pict.linesize[0] = mInputWidth * 2;

    AVPicture to_pict;
    to_pict.data[0] = output + output_offset * mInputWidth;
    to_pict.data[1] = output + mInputWidth * output_height + output_offset * mInputWidth / 2;
    to_pict.data[2] = output + mInputWidth * output_height * 3 / 2 + output_offset * mInputWidth / 2;
    to_pict.linesize[0] = mInputWidth;
    to_pict.linesize[1] = mInputWidth / 2;
    to_pict.linesize[2] = mInputWidth / 2;

    convert_context = sws_getCachedContext(convert_context,
                                           mInputWidth, conversion_height, PIX_FMT_UYVY422,
                                           mInputWidth, conversion_height, PIX_FMT_YUV422P,
                                           SWS_FAST_BILINEAR, NULL, NULL, NULL);

    sws_scale(convert_context, from_pict.data, from_pict.linesize, 0, conversion_height, to_pict.data, to_pict.linesize);
}

