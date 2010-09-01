/*
 * $Id: FFmpegDecoder.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#include <video_VITC.h>
 
#include "FFmpegDecoder.h"
#include "RecorderException.h"
#include "Logging.h"

using namespace std;
using namespace rec;



FFmpegDecoder::FFmpegDecoder(MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster, bool yuv420,
                             int num_threads)
{
    mYUV420 = yuv420;
    mNumThreads = num_threads;
    mAVContext = 0;
    mAVFrame = 0;
    mDecodedData = 0;
    mDecodedSize = 0;
    mConvertContext = 0;
    
    // TODO: support other formats
    REC_ASSERT(res == MaterialResolution::IMX50_MXF_1A);
    REC_ASSERT(raster == Ingex::VideoRaster::PAL);
    
    CreateMPEGIDecoder();
}

FFmpegDecoder::~FFmpegDecoder()
{
    if (mNumThreads > 1)
        avcodec_thread_free(mAVContext);
    avcodec_close(mAVContext);

    free(mAVContext);
    free(mAVFrame);
    
    delete [] mDecodedData;
    
    if (mConvertContext)
        sws_freeContext(mConvertContext);
}

bool FFmpegDecoder::DecodeFrame(const unsigned char *compressed_data, unsigned int compressed_size,
                                const unsigned char **decoded_data, unsigned int *decoded_size,
                                bool *have_vitc, rec::Timecode *vitc)
{
    bool result = DecodeMPEGIFrame(compressed_data, compressed_size, have_vitc, vitc);
    if (!result)
        return false;
    
    *decoded_data = mDecodedData;
    *decoded_size = mDecodedSize;
    return true;
}

void FFmpegDecoder::CreateMPEGIDecoder()
{
    AVCodec *av_decoder;
    
    av_decoder = avcodec_find_decoder(CODEC_ID_MPEG2VIDEO);
    if (!av_decoder)
        REC_LOGTHROW(("Could not find the MPEG-2 video decoder"));


    mAVContext = avcodec_alloc_context();
    if (!mAVContext)
        REC_LOGTHROW(("Could not allocate decoder context"));

    if (mNumThreads > 1)
        avcodec_thread_init(mAVContext, mNumThreads);

    avcodec_set_dimensions(mAVContext, 720, 608);

    
    if (avcodec_open(mAVContext, av_decoder) < 0)
        REC_LOGTHROW(("Could not open decoder"));


    mAVFrame = avcodec_alloc_frame();
    if (!mAVFrame)
        REC_LOGTHROW(("Could not allocate decoder frame"));
    
    
    mDecodedSize = mYUV420 ? 720 * 576 * 3 / 2 : 720 * 576 * 2;
    mDecodedData = new unsigned char[mDecodedSize];
}

bool FFmpegDecoder::DecodeMPEGIFrame(const unsigned char *compressed_data, unsigned int compressed_size,
                                     bool *have_vitc, rec::Timecode *vitc)
{
    int finished;
    int result = avcodec_decode_video(mAVContext, mAVFrame, &finished, compressed_data, compressed_size);
    if (result <= 0 || !finished) {
        Logging::error("Failed to decode MPEG-I video\n");
        return false;
    }


    // decode VITC from VBI (height 32), lines 19 and 21

    if (vitc) {
        unsigned int hour, min, sec, frame;
        if (readVITC(mAVFrame->data[0] + (32 - ((23 - 19) * 2)) * mAVFrame->linesize[0],
                     false, &hour, &min, &sec, &frame) ||
            readVITC(mAVFrame->data[0] + (32 - ((23 - 21) * 2)) * mAVFrame->linesize[0],
                     false, &hour, &min, &sec, &frame))
        {
            *vitc = Timecode(hour, min, sec, frame);
            *have_vitc = true;
        }
        else
        {
            *have_vitc = false;
        }
    }


    // strip VBI and convert to YUV420 if required
    
    AVPicture from_pict, to_pict;
    
    from_pict.data[0] = mAVFrame->data[0];
    from_pict.data[1] = mAVFrame->data[1];
    from_pict.data[2] = mAVFrame->data[2];
    from_pict.linesize[0] = mAVFrame->linesize[0];
    from_pict.linesize[1] = mAVFrame->linesize[1];
    from_pict.linesize[2] = mAVFrame->linesize[2];
    // skip VBI (32 lines)
    from_pict.data[0] += (608 - 576) * from_pict.linesize[0];
    from_pict.data[1] += (608 - 576) * from_pict.linesize[1];
    from_pict.data[2] += (608 - 576) * from_pict.linesize[2];
    
    if (mYUV420) {
        to_pict.data[0] = mDecodedData;
        to_pict.data[1] = to_pict.data[0] + 720 * 576;
        to_pict.data[2] = to_pict.data[1] + 720 * 576 / 4;
        to_pict.linesize[0] = 720;
        to_pict.linesize[1] = 720 / 2;
        to_pict.linesize[2] = 720 / 2;
        
        mConvertContext = sws_getCachedContext(mConvertContext,
                                               720, 576, PIX_FMT_YUV422P,
                                               720, 576, PIX_FMT_YUV420P,
                                               SWS_FAST_BILINEAR, NULL, NULL, NULL);
    } else {
        to_pict.data[0] = mDecodedData;
        to_pict.linesize[0] = 720 * 2;
        
        mConvertContext = sws_getCachedContext(mConvertContext,
                                               720, 576, PIX_FMT_YUV422P,
                                               720, 576, PIX_FMT_UYVY422,
                                               SWS_FAST_BILINEAR, NULL, NULL, NULL);
    }
    
    sws_scale(mConvertContext, from_pict.data, from_pict.linesize, 0, 576, to_pict.data, to_pict.linesize);
    
    
    return true;
}

