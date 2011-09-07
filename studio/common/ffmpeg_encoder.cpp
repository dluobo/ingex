/*
 * $Id: ffmpeg_encoder.cpp,v 1.13 2011/09/07 15:01:05 john_f Exp $
 *
 * Encode uncompressed video to DV using libavcodec
 *
 * Copyright (C) 2005  British Broadcasting Corporation
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

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#include <ffmpeg/swscale.h>
#else
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#endif

#ifdef __cplusplus
}
#endif

#define MPA_FRAME_SIZE              1152 // taken from ffmpeg's private mpegaudio.h
#define PAL_VIDEO_FRAME_AUDIO_SIZE  1920

#include "VideoRaster.h"
#include "ffmpeg_resolutions.h"
#include "ffmpeg_encoder.h"
#include "ffmpeg_defs.h"

// Select IMX intra matrix
const enum { DEFAULT, AVID } IMX_INTRA_MATRIX_SELECTION = AVID;

/*
const uint16_t jpeg2to1_intra_matrix[] = {
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  2,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  3,  2,  2,  3,  1,  1,  1,
  1,  1,  1,  1,  5,  3,  3,  5,
  1,  1,  1,  6,  4,  5,  5,  5
};

const uint16_t jpeg2to1_chroma_intra_matrix[] = {
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  2,  1,  5,
  5,  2,  2,  2,  1,  2,  2,  2,
  5,  5,  5,  5,  5,  5,  5,  5,
  5,  5,  5,  2,  5,  5,  5,  5,
  5,  5,  5,  5,  5,  5,  5,  5,
  5,  5,  5,  5,  5,  5,  5,  5,
  5,  5,  5,  5,  5,  5,  5,  5,
};
*/


typedef struct 
{
    AVCodec * codec;
    AVCodecContext * codec_context;
    // audio
    int num_audio_channels;
    int audio_samples_per_input_frame;
    int audio_samples_per_output_frame;
    short * audio_inbuf;
    int audio_inbuf_size;
    int audio_inbuf_wroffset;
    uint8_t * audio_outbuf;
    int audio_outbuf_size;
    // video
    AVFrame * inputFrame;
    uint8_t * outputBuffer;
    unsigned int bufferSize;
    // following needed for frame padding and input scaling
    AVPicture * tmpFrame;
    uint8_t * inputBuffer;
    int input_width;
    int input_height;
    struct SwsContext* scale_context;
    int scale_image;
    int padtop;
    int padColour[3];
    int crop_480_ntsc_dv;
} internal_ffmpeg_encoder_t;


// Local context
namespace
{

static void cleanup (internal_ffmpeg_encoder_t * encoder)
{
    if (encoder)
    {
        av_free(encoder->outputBuffer);
        av_free(encoder->inputBuffer);
        av_free(encoder->inputFrame);
        av_free(encoder->tmpFrame);
        if (encoder->codec_context)
        {
            if (encoder->codec_context->intra_matrix)
            {
                av_free(encoder->codec_context->intra_matrix);
            }
            avcodec_close(encoder->codec_context);
        }
        sws_freeContext(encoder->scale_context);
        av_free(encoder->codec_context);
        av_free(encoder);
    }
}

} // namespace

extern bool ffmpeg_encoder_check_available(MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster)
{
    enum CodecID codec_id;
    enum CodecType codec_type;
    PixelFormat pix_fmt;
    get_ffmpeg_params(res, raster, codec_id, codec_type, pix_fmt);

    av_register_all();
    if (avcodec_find_encoder(codec_id))
    {
        return true;
    }

    return false;
}

extern ffmpeg_encoder_t * ffmpeg_encoder_init(MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster, int num_threads)
{
    internal_ffmpeg_encoder_t * encoder;

    int width;
    int height;
    int fps_num;
    int fps_den;
    Ingex::Interlace::EnumType interlace;
    Ingex::VideoRaster::GetInfo(raster, width, height, fps_num, fps_den, interlace);
    //fprintf(stderr, "width = %d, height = %d\n", width, height);

    /* register all ffmpeg codecs and formats */
    av_register_all();

    /* Allocate encoder object and set all members to zero */
    encoder = (internal_ffmpeg_encoder_t *) av_mallocz(sizeof(internal_ffmpeg_encoder_t));
    if (!encoder)
    {
        fprintf(stderr, "Could not allocate encoder object\n");
        return 0;
    }

    /* Setup encoder */
    enum CodecID codec_id;
    enum CodecType codec_type;
    PixelFormat pix_fmt;
    get_ffmpeg_params(res, raster, codec_id, codec_type, pix_fmt);
    //fprintf(stderr, "res = %d, codec_id = %d, codec_type = %d, pix_fmt = %d\n", res, codec_id, codec_type, pix_fmt);

    encoder->codec = avcodec_find_encoder(codec_id);
    if (! encoder->codec)
    {
        fprintf(stderr, "Could not find encoder\n");
        cleanup(encoder);
        return 0;
    }

    // Allocate AVCodecContext and set its fields to
    // default values.
    encoder->codec_context = avcodec_alloc_context();
    if (!encoder->codec_context)
    {
        fprintf(stderr, "Could not allocate encoder context\n");
        cleanup(encoder);
        return 0;
    }

    // Set fields in AVCodecContext
    encoder->codec_context->codec_type = codec_type;
    if (CODEC_TYPE_VIDEO == codec_type)
    {
        encoder->codec_context->pix_fmt = pix_fmt;
        encoder->codec_context->time_base.num = fps_den;
        encoder->codec_context->time_base.den = fps_num;
        
        encoder->input_width = width;
        encoder->input_height = height;

        // Set interlace parameters
        int top_field_first;
        int interlaced;
        switch (interlace)
        {
        case Ingex::Interlace::TOP_FIELD_FIRST:
            interlaced = 1;
            top_field_first = 1;
            break;
        case Ingex::Interlace::BOTTOM_FIELD_FIRST:
            interlaced = 1;
            top_field_first = 0;
            break;
        case Ingex::Interlace::NONE:
        default:
            interlaced = 0;
            top_field_first = 0;
        }

        if (interlaced)
        {
            encoder->codec_context->flags |= CODEC_FLAG_INTERLACED_DCT;
        }

        int bit_rate = 0;
        int buffer_size = 0;
        int encoded_frame_size = 0;
        const uint16_t * intra_matrix = 0;

        switch (res)
        {
        case MaterialResolution::DV25_RAW:
        case MaterialResolution::DV25_MXF_ATOM:
        case MaterialResolution::DV25_MOV:
            switch (raster)
            {
            case Ingex::VideoRaster::PAL_B:
            case Ingex::VideoRaster::PAL_B_4x3:
            case Ingex::VideoRaster::PAL_B_16x9:
                encoded_frame_size = 144000;
                break;
            case Ingex::VideoRaster::NTSC:
            case Ingex::VideoRaster::NTSC_4x3:
            case Ingex::VideoRaster::NTSC_16x9:
                height = 480;
                encoder->crop_480_ntsc_dv = 1;
                encoded_frame_size = 120000;
                break;
            default:
                break;
            }
            break;
        case MaterialResolution::DV50_RAW:
        case MaterialResolution::DV50_MXF_ATOM:
        case MaterialResolution::DV50_MOV:
            switch (raster)
            {
            case Ingex::VideoRaster::PAL_B:
            case Ingex::VideoRaster::PAL_B_4x3:
            case Ingex::VideoRaster::PAL_B_16x9:
                encoded_frame_size = 288000;
                break;
            case Ingex::VideoRaster::NTSC:
            case Ingex::VideoRaster::NTSC_4x3:
            case Ingex::VideoRaster::NTSC_16x9:
                height = 480;
                encoder->crop_480_ntsc_dv = 1;
                encoded_frame_size = 240000;
                break;
            default:
                break;
            }
            break;
        case MaterialResolution::IMX30_MXF_ATOM:
        case MaterialResolution::IMX30_MXF_1A:
            bit_rate = 30 * 1000000;
            switch (IMX_INTRA_MATRIX_SELECTION)
            {
            case AVID:
                intra_matrix = imx30_intra_matrix;
                break;
            case DEFAULT:
                intra_matrix = default_intra_matrix;
                break;
            }
            //encoded_frame_size = (bit_rate / 8) * fps_den / fps_num;
            switch (raster)
            {
            case Ingex::VideoRaster::PAL:
            case Ingex::VideoRaster::PAL_4x3:
            case Ingex::VideoRaster::PAL_16x9:
            case Ingex::VideoRaster::PAL_592:
            case Ingex::VideoRaster::PAL_608:
                buffer_size = 1200000;
                break;
            case Ingex::VideoRaster::NTSC:
            case Ingex::VideoRaster::NTSC_4x3:
            case Ingex::VideoRaster::NTSC_16x9:
                buffer_size = 1001000;
                break;
            default:
                break;
            }
            encoded_frame_size = buffer_size / 8;
            break;
        case MaterialResolution::IMX40_MXF_ATOM:
        case MaterialResolution::IMX40_MXF_1A:
            bit_rate = 40 * 1000000;
            switch (IMX_INTRA_MATRIX_SELECTION)
            {
            case AVID:
                intra_matrix = imx4050_intra_matrix;
                break;
            case DEFAULT:
                intra_matrix = default_intra_matrix;
                break;
            }
            //encoded_frame_size = (bit_rate / 8) * fps_den / fps_num;
            switch (raster)
            {
            case Ingex::VideoRaster::PAL:
            case Ingex::VideoRaster::PAL_4x3:
            case Ingex::VideoRaster::PAL_16x9:
            case Ingex::VideoRaster::PAL_592:
            case Ingex::VideoRaster::PAL_608:
                buffer_size = 1600000;
                break;
            case Ingex::VideoRaster::NTSC:
            case Ingex::VideoRaster::NTSC_4x3:
            case Ingex::VideoRaster::NTSC_16x9:
                buffer_size = 1334667;
                break;
            default:
                break;
            }
            encoded_frame_size = buffer_size / 8;
            break;
        case MaterialResolution::IMX50_MXF_ATOM:
        case MaterialResolution::IMX50_MXF_1A:
            bit_rate = 50 * 1000000;
            switch (IMX_INTRA_MATRIX_SELECTION)
            {
            case AVID:
                intra_matrix = imx4050_intra_matrix;
                break;
            case DEFAULT:
                intra_matrix = default_intra_matrix;
                break;
            }
            //encoded_frame_size = (bit_rate / 8) * fps_den / fps_num;
            switch (raster)
            {
            case Ingex::VideoRaster::PAL:
            case Ingex::VideoRaster::PAL_4x3:
            case Ingex::VideoRaster::PAL_16x9:
            case Ingex::VideoRaster::PAL_592:
            case Ingex::VideoRaster::PAL_608:
                buffer_size = 2000000;
                break;
            case Ingex::VideoRaster::NTSC:
            case Ingex::VideoRaster::NTSC_4x3:
            case Ingex::VideoRaster::NTSC_16x9:
                buffer_size = 1668334;
                break;
            default:
                break;
            }
            encoded_frame_size = buffer_size / 8;
            break;
        case MaterialResolution::DNX36P_MXF_ATOM:
            encoder->codec_context->bit_rate = 36 * 1000000;
            encoder->codec_context->qmax = 1024;
            encoded_frame_size = 188416;        // from VC-3 spec
#if defined(CODEC_FLAG2_AVID_COMPAT)
            // to support Avid Nitris decoder
            encoder->codec_context->flags2 |= CODEC_FLAG2_AVID_COMPAT;
#endif
            break;
        case MaterialResolution::DNX120I_MXF_ATOM:
        case MaterialResolution::DNX120P_MXF_ATOM:
            encoder->codec_context->bit_rate = 120 * 1000000;
            encoder->codec_context->qmax = 1024;
            encoded_frame_size = 606208;        // from VC-3 spec
#if defined(CODEC_FLAG2_AVID_COMPAT)
            // to support Avid Nitris decoder
            encoder->codec_context->flags2 |= CODEC_FLAG2_AVID_COMPAT;
#endif
            break;
        case MaterialResolution::DNX185I_MXF_ATOM:
        case MaterialResolution::DNX185P_MXF_ATOM:
            encoder->codec_context->bit_rate = 185 * 1000000;
            encoder->codec_context->qmax = 1024;
            encoded_frame_size = 917504;        // from VC-3 spec
#if defined(CODEC_FLAG2_AVID_COMPAT)
            // to support Avid Nitris decoder
            encoder->codec_context->flags2 |= CODEC_FLAG2_AVID_COMPAT;
#endif
            break;
        case MaterialResolution::DV100_RAW:
        case MaterialResolution::DV100_MXF_ATOM:
        case MaterialResolution::DV100_MOV:
            switch (raster)
            {
            case Ingex::VideoRaster::SMPTE274_25I:
                width = 1440;                       // coded width (input scaled horizontally from 1920)
                height = 1080;
                encoder->scale_image = 1;
                encoded_frame_size = 576000;        // SMPTE 370M spec
                break;
            case Ingex::VideoRaster::SMPTE274_29I:
                width = 1280;                       // coded width (input scaled horizontally from 1920)
                height = 1080;
                encoder->scale_image = 1;
                encoded_frame_size = 480000;        // SMPTE 370M spec
                break;
            // 720p formats not tested
            case Ingex::VideoRaster::SMPTE296_50P:
                width = 960;                       // coded width (input scaled horizontally from 1920)
                height = 720;
                encoder->scale_image = 1;
                encoded_frame_size = 288000;        // SMPTE 370M spec
                break;
            case Ingex::VideoRaster::SMPTE296_59P:
                width = 960;                       // coded width (input scaled horizontally from 1920)
                height = 720;
                encoder->scale_image = 1;
                encoded_frame_size = 240000;        // SMPTE 370M spec
                break;
            default:
                break;
            }
            break;

		case MaterialResolution::XDCAMHD422_RAW:
        case MaterialResolution::XDCAMHD422_MXF_1A:
            codec_id = CODEC_ID_MPEG2VIDEO; 
            codec_type = CODEC_TYPE_VIDEO;
            buffer_size = 364083330; //= 364 MB seems about right.
            bit_rate = 50000000;
            width = 1920;
            height = 1080;
            encoded_frame_size = (bit_rate / 200) + 2000000;
            encoder->codec_context->bit_rate = bit_rate;
            encoder->codec_context->gop_size = 14;
            encoder->codec_context->b_frame_strategy = 0;
#if defined(FFMPEG_NONLINEAR_PATCH)
            // non-linear quantization is implemented, except for qscale 1 which could cause an integer overflow
            // in the quantization process for large DCT coefficients
            encoder->codec_context->qmin = 2;
            encoder->codec_context->qmax = 31;
#else
#warning "Using FFmpeg without MPEG non-linear quantization patch"
            encoder->codec_context->qmin = 1;
            encoder->codec_context->qmax = 12;
#endif
            encoder->codec_context->me_method = 5; // (epzs)
            encoder->codec_context->width = width;
            encoder->codec_context->height = height;
            encoder->codec_context->rc_max_rate = 60000000;
            encoder->codec_context->rc_min_rate = 0;
            encoder->codec_context->rc_buffer_size = 50000000; 
            encoder->codec_context->rc_initial_buffer_occupancy = 50000000;
            encoder->codec_context->flags |= CODEC_FLAG_ALT_SCAN;
            encoder->codec_context->flags |= CODEC_FLAG_CLOSED_GOP;
            encoder->codec_context->flags2 |= CODEC_FLAG2_NON_LINEAR_QUANT;
            encoder->codec_context->flags2 |= CODEC_FLAG2_INTRA_VLC;
            encoder->codec_context->rc_max_available_vbv_use =1;
            encoder->codec_context->rc_min_vbv_overflow_use = 1;
            encoder->codec_context->lmin = 1 * FF_QP2LAMBDA;
            encoder->codec_context->lmax = 3 * FF_QP2LAMBDA;
            encoder->codec_context->max_b_frames =2;
            encoder->codec_context->stream_codec_tag = 'c'<<24 | '5'<<16 | 'd'<<8 | 'x';
            encoder->codec_context->mb_decision = FF_MB_DECISION_SIMPLE;
            encoder->codec_context->scenechange_threshold = 1000000000;
            break;

        default:
            break;
        }

        // setup ffmpeg threads if specified
        if (num_threads != 0)
        {
            int threads = 0;
            if (num_threads == THREADS_USE_BUILTIN_TUNING)
            {
                // select number of threaded based on picture size
                if (width > 720)
                    threads = 4;
            }
            else
            {
                // use number of threads specified by function arg
                threads = num_threads;
            }
            if (threads > 0)
            {
                avcodec_thread_init(encoder->codec_context, threads);
                encoder->codec_context->thread_count= threads;
            }
        }

        // Specific settings for IMX
        switch (res)
        {
        case MaterialResolution::IMX30_MXF_ATOM:
        case MaterialResolution::IMX40_MXF_ATOM:
        case MaterialResolution::IMX50_MXF_ATOM:
        case MaterialResolution::IMX30_MXF_1A:
        case MaterialResolution::IMX40_MXF_1A:
        case MaterialResolution::IMX50_MXF_1A:
            // stored image height is 608 for PAL and 512 for NTSC
            switch (raster)
            {
            case Ingex::VideoRaster::PAL:
            case Ingex::VideoRaster::PAL_4x3:
            case Ingex::VideoRaster::PAL_16x9:
            case Ingex::VideoRaster::PAL_592:
            case Ingex::VideoRaster::PAL_608:
                encoder->padtop = 608 - height;
                break;
            case Ingex::VideoRaster::NTSC:
            case Ingex::VideoRaster::NTSC_4x3:
            case Ingex::VideoRaster::NTSC_16x9:
                encoder->padtop = 512 - height;
                break;
            default:
                break;
            }

            encoder->codec_context->rc_min_rate = bit_rate;
            encoder->codec_context->rc_max_rate = bit_rate;
            encoder->codec_context->bit_rate = bit_rate;
            encoder->codec_context->rc_buffer_size = buffer_size;
            encoder->codec_context->rc_initial_buffer_occupancy = buffer_size;

            // -ps 1
            encoder->codec_context->rtp_payload_size = 1;

            // -qscale 1
            encoder->codec_context->global_quality = 1 * FF_QP2LAMBDA;

            // -intra
            encoder->codec_context->gop_size = 0;

#if defined(FFMPEG_NONLINEAR_PATCH)
            // non-linear quantization is implemented, except for qscale 1 which could cause an integer overflow
            // in the quantization process for large DCT coefficients
            encoder->codec_context->qmin = 2;
            encoder->codec_context->qmax = 31;
#else
#warning "Using FFmpeg without MPEG non-linear quantization patch"
            // dummy non-linear quantization using the linear quantizer scales. It is limited to 12 because
            // there is no non-linear scale factor 26 (linear scale factor is qscale x 2)
            encoder->codec_context->qmin = 1;
            encoder->codec_context->qmax = 12;
            
            // TODO: are these relevant for CBR?
            encoder->codec_context->lmin = 1 * FF_QP2LAMBDA;
            encoder->codec_context->lmax = 12 * FF_QP2LAMBDA;
#endif

#if LIBAVCODEC_VERSION_INT >= ((52<<16)+(4<<8)+0)
            // -rc_max_vbv_use 1
            // -rc_min_vbv_use 1
            // ffmpeg/libavcodec/mpegvideo_enc.c iterates in MPV_encode_picture() until #bits < max_size, where 
            // max_size = rcc->buffer_index * avctx->rc_max_available_vbv_use, where
            // rc_max_available_vbv_use is 0.3 by default
            // i.e. by default the frame size (no padding) is 1/3 x constant target size, i.e. 2/3 padding
            encoder->codec_context->rc_max_available_vbv_use = 1;
            encoder->codec_context->rc_min_vbv_overflow_use = 1;
#endif

            encoder->codec_context->flags |= CODEC_FLAG_LOW_DELAY;

            // See SMPTE 356M Type D-10 Stream Specifications

            // intra_dc_precision = 2
            // -dc 10  (8 gets subtracted)
            encoder->codec_context->intra_dc_precision = 2;

            // frame_pred_frame_dct = 0 (field/frame adaptive)
            // We get this by default

            // q_scale_type = 1 (nonlinear quantiser)
            encoder->codec_context->flags2 |= CODEC_FLAG2_NON_LINEAR_QUANT;

            // intra_vlc_format = 1 (use intra-VLC table)
            encoder->codec_context->flags2 |= CODEC_FLAG2_INTRA_VLC;

            // Avid uses special intra quantiser matrix
            encoder->codec_context->intra_matrix = (uint16_t *)av_mallocz(sizeof(uint16_t) * 64);
            memcpy(encoder->codec_context->intra_matrix, intra_matrix, sizeof(uint16_t) * 64);

            /* alloc input buffer */
            encoder->tmpFrame = (AVPicture *)av_mallocz(sizeof(AVPicture));
            encoder->inputBuffer = (uint8_t *)av_mallocz(width * (height + encoder->padtop) * 2);
            encoder->padColour[0] = 16;
            encoder->padColour[1] = 128;
            encoder->padColour[2] = 128;

            break;
        default:
            break;
        }

        if (encoder->scale_image)
        {
            encoder->scale_image = 1;
            // alloc buffers for scaling input video
            encoder->tmpFrame = (AVPicture *)av_mallocz(sizeof(AVPicture));
            encoder->inputBuffer = (uint8_t *)av_mallocz(width * height * 2);
        }

        avcodec_set_dimensions(encoder->codec_context, width, height + encoder->padtop);


        /* Set aspect ratio */
        Ingex::Rational sar = Ingex::VideoRaster::SampleAspectRatio(raster);
        encoder->codec_context->sample_aspect_ratio.num = sar.numerator;
        encoder->codec_context->sample_aspect_ratio.den = sar.denominator;

        /* prepare codec */
        if (avcodec_open(encoder->codec_context, encoder->codec) < 0)
        {
            fprintf(stderr, "Could not open encoder\n");
            return 0;
        }

        /* allocate input frame */
        encoder->inputFrame = avcodec_alloc_frame();
        if (!encoder->inputFrame)
        {
            fprintf(stderr, "Could not allocate input frame\n");
            return 0;
        }
        encoder->inputFrame->top_field_first = top_field_first;
        encoder->inputFrame->interlaced_frame = interlaced;

        /* allocate output buffer */
        //fprintf(stderr, "encoded_frame_size = %d\n", encoded_frame_size);
        encoder->bufferSize = encoded_frame_size + 50000; // not sure why extra is needed
        encoder->outputBuffer = (uint8_t *)av_mallocz(encoder->bufferSize);
    }
    else
    {
    // audio
        /* set parameters */
        const int kbit_rate = 128;
        encoder->codec_context->bit_rate = kbit_rate * 1000;
        encoder->codec_context->sample_rate = 48000;
        const int nch = 2;
        encoder->num_audio_channels = nch;
        encoder->codec_context->channels = nch;

        /* Set up audio buffer parameters */
        encoder->audio_samples_per_input_frame = PAL_VIDEO_FRAME_AUDIO_SIZE;
        /* We have to do some buffering of input, hence the * 2 on the end */
        encoder->audio_inbuf_size = encoder->audio_samples_per_input_frame * nch * sizeof(short) * 2;

        /* samples per coded frame */
        encoder->audio_samples_per_output_frame = MPA_FRAME_SIZE;
        /* coded frame size */
        encoder->audio_outbuf_size = 2 * 960; // enough for 2 audio frames at 320 kbit/s

        /* allocate audio input buffer */
        encoder->audio_inbuf = (short *)av_malloc(encoder->audio_inbuf_size);
        encoder->audio_inbuf_wroffset = 0;

        /* allocate audio (coded) output buffer */
        encoder->audio_outbuf = (uint8_t *)av_malloc(encoder->audio_outbuf_size);

        /* prepare codec */
        if (avcodec_open(encoder->codec_context, encoder->codec) < 0)
        {
            fprintf(stderr, "Could not open encoder\n");
            return 0;
        }
    }

    return (ffmpeg_encoder_t *) encoder;
}

extern int ffmpeg_encoder_encode(ffmpeg_encoder_t * in_encoder, const uint8_t * p_video, uint8_t * * pp_enc_video)
{
    internal_ffmpeg_encoder_t * encoder = (internal_ffmpeg_encoder_t *)in_encoder;
    if (!encoder)
    {
        return -1;
    }

    if (encoder->padtop)
    {
        /* set pointers in tmpFrame to point to planes in p_video */
        avpicture_fill(encoder->tmpFrame, (uint8_t*)p_video,
            encoder->codec_context->pix_fmt,
            encoder->input_width, encoder->input_height);

        /* set pointers in inputFrame to point to our buffer */
        avpicture_fill((AVPicture*)encoder->inputFrame, encoder->inputBuffer,
            encoder->codec_context->pix_fmt,
            encoder->codec_context->width, encoder->codec_context->height);

        /* copy video data into our buffer with padding */
        av_picture_pad((AVPicture*)encoder->inputFrame, encoder->tmpFrame,
            encoder->codec_context->height,
            encoder->codec_context->width,
            encoder->codec_context->pix_fmt,
            encoder->padtop, 0, 0, 0,
            encoder->padColour);
    }
    else if (encoder->scale_image)
    {
        encoder->scale_context = sws_getCachedContext(encoder->scale_context,
                    encoder->input_width, encoder->input_height,                   // input WxH
                    encoder->codec_context->pix_fmt,
                    encoder->codec_context->width, encoder->codec_context->height, // output WxH
                    encoder->codec_context->pix_fmt,
                    SWS_FAST_BILINEAR,
                    NULL, NULL, NULL);

        // Input image goes into tmpFrame
        avpicture_fill((AVPicture*)encoder->tmpFrame, (uint8_t*)p_video,
            encoder->codec_context->pix_fmt,
            encoder->input_width, encoder->input_height);

        // Output of scale operation goes into inputFrame
        avpicture_fill((AVPicture*)encoder->inputFrame, encoder->inputBuffer,
            encoder->codec_context->pix_fmt,
            encoder->codec_context->width, encoder->codec_context->height);

        // Perform the scale
        sws_scale(encoder->scale_context,
            encoder->tmpFrame->data, encoder->tmpFrame->linesize,
            0, encoder->input_height,
            encoder->inputFrame->data, encoder->inputFrame->linesize);
    }
    else if (encoder->crop_480_ntsc_dv)
    {
        AVPicture * picture = (AVPicture *) encoder->inputFrame;

        // Use avpicture_fill to set pointers and linesizes
        avpicture_fill(picture, (uint8_t*)p_video,
            encoder->codec_context->pix_fmt,
            encoder->input_width, encoder->input_height);

        // Skip top 4 lines
        picture->data[0] += 4 * picture->linesize[0];
        picture->data[1] += 4 * picture->linesize[1];
        picture->data[2] += 4 * picture->linesize[2];
    }
    else
    {
        /* set pointers in inputFrame to point to planes in p_video */
        avpicture_fill((AVPicture*)encoder->inputFrame, (uint8_t*)p_video,
            encoder->codec_context->pix_fmt,
            encoder->codec_context->width, encoder->codec_context->height);
    }

    /* compress video */
    //fprintf(stderr, "outputBuffer = %p, bufferSize = %d\n", encoder->outputBuffer, encoder->bufferSize);
    int size = avcodec_encode_video(encoder->codec_context,
        encoder->outputBuffer, encoder->bufferSize, encoder->inputFrame);
    if (size < 0)
    {
        fprintf(stderr, "error compressing video!\n");
        return -1;
    }
    else
    {
        // debug only
        //fprintf(stderr, "size = %d\n", size);
    }


    /* set pointer to encoded video */
    *pp_enc_video = encoder->outputBuffer;

    return size;
}

extern int ffmpeg_encoder_encode_audio (ffmpeg_encoder_t * in_encoder, int num_samples, short * p_audio, uint8_t * * pp_enc_audio)
{
    internal_ffmpeg_encoder_t * aenc = (internal_ffmpeg_encoder_t *)in_encoder;
    if (!aenc)
    {
        return -1;
    }

    int size = 0;
    *pp_enc_audio = aenc->audio_outbuf;
    int nch = aenc->num_audio_channels;

    /* encode and write audio */
    if (p_audio)
    {
        /*
        Audio comes in in video frames (num_samples e.g. 1920) but may be encoded
        as MPEG frames (1152 samples) so we need to buffer.
        */
        /*
        NB. audio_inbuf_wroffset is in terms of 16-bit samples, hence the
        rather confusing pointer arithmetic.
        */

        // Copy input samples to audio_inbuf.
        memcpy (aenc->audio_inbuf + aenc->audio_inbuf_wroffset, p_audio, num_samples * 2 * nch);
        aenc->audio_inbuf_wroffset += (num_samples * nch);

        // If we have enough samples to code, code them.
        int diff;
        while ((diff = aenc->audio_inbuf_wroffset - (aenc->audio_samples_per_output_frame * nch)) >= 0)
        {
            //fprintf (stderr, "samples available to code = %d, i.e. %d per channel\n", aenc->audio_inbuf_wroffset, aenc->audio_inbuf_wroffset / nch);
            // You don't get any feedback from this function but it appears to consume audio_samples_per_output_frame
            // of input each time you call it.
            int s = avcodec_encode_audio(aenc->codec_context, aenc->audio_outbuf + size, aenc->audio_outbuf_size - size, aenc->audio_inbuf);
            if (s < 0)
            {
                fprintf(stderr, "error compressing audio!\n");
                return -1;
            }
            else
            {
                //fprintf (stderr, "produced %d bytes of encoded audio\n", s);
            }
            size += s;

            /* Shift samples in input buffer.
               The number to shift is diff, i.e. those exceeding what we can code at the moment. */
            memmove (aenc->audio_inbuf, aenc->audio_inbuf + (aenc->audio_samples_per_output_frame * nch), diff * 2);
            aenc->audio_inbuf_wroffset -= (aenc->audio_samples_per_output_frame * nch);
        }
    }

    //fprintf (stderr, "returning size=%d\n", size);
    return size;
}

extern int ffmpeg_encoder_close (ffmpeg_encoder_t * in_encoder)
{
    internal_ffmpeg_encoder_t * encoder = (internal_ffmpeg_encoder_t *)in_encoder;
    if (!encoder)
    {
        return -1;
    }

    cleanup(encoder);

    return 0;
}

