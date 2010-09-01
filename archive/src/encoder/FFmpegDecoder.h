/*
 * $Id: FFmpegDecoder.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#ifndef __FFMPEG_DECODER_H__
#define __FFMPEG_DECODER_H__


extern "C" {
#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/swscale.h>
#include <ffmpeg/avcodec.h>
#else
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#endif
}

#include "MaterialResolution.h"
#include "Types.h"



class FFmpegDecoder
{
public:
    FFmpegDecoder(MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster, bool yuv420, int num_threads);
    ~FFmpegDecoder();

    bool DecodeFrame(const unsigned char *compressed_data, unsigned int compressed_size,
                     const unsigned char **decoded_data, unsigned int *decoded_size,
                     bool *have_vitc, rec::Timecode *vitc);

private:
    void CreateMPEGIDecoder();
    bool DecodeMPEGIFrame(const unsigned char *compressed_data, unsigned int data_size,
                          bool *have_vitc, rec::Timecode *vitc);

private:
    int mNumThreads;
    AVCodecContext *mAVContext;
    AVFrame *mAVFrame;
    
    unsigned char *mDecodedData;
    unsigned int mDecodedSize;
    
    bool mYUV420;
    struct SwsContext *mConvertContext;
};



#endif

