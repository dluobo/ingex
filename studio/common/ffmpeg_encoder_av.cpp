/*
 * $Id: ffmpeg_encoder_av.cpp,v 1.20 2011/12/30 16:41:37 john_f Exp $
 *
 * Encode AV and write to file.
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

/*
This file is based on ffmpeg/output_example.c from the ffmpeg source tree.
*/

#include <string.h>
#include <stdlib.h>

#define __STDC_CONSTANT_MACROS
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

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

#include "VideoRaster.h"
#include "ffmpeg_encoder_av.h"

const unsigned int MPA_FRAME_SIZE = 1152;
const unsigned int AAC_FRAME_SIZE = 1024;
const unsigned int MAX_VIDEO_FRAME_AUDIO_SIZE = 1920;
const unsigned int MAX_AUDIO_STREAMS = 16;

const bool H264_IMAGE_SCALE = false;

typedef struct
{
    AVStream * audio_st;
    int num_audio_channels;
    int is_pcm;
    int audio_samples_per_output_frame;
    short * audio_inbuf;
    int audio_inbuf_size;
    int audio_inbuf_offset;
    uint8_t * audio_outbuf;
    int audio_outbuf_size;
} audio_encoder_t;

/*
Note that avcodec_encode_audio wants a short * for input audio samples.
Need to check what is really happening regarding input sample size and
whether we rely on sizeof(short) being 2.
*/


typedef struct 
{
    // output format context
    AVFormatContext * oc;

    // video
    AVStream * video_st; ///< video stream
    AVFrame * inputFrame;
    uint8_t * video_outbuf;
    int video_outbuf_size;
    int64_t video_pts;
    int input_width;
    int input_height;
    int output_width;
    int output_height;

    // audio
    unsigned int num_audio_streams;
    audio_encoder_t * audio_encoder[MAX_AUDIO_STREAMS];

    // following needed for video cropping/scaling
    AVPicture * tmpFrame;
    uint8_t * tmpBuffer;
    struct SwsContext * scale_context;
    int scale_image;
    int crop_480_ntsc_dv;
    int crop_480_ntsc_mpeg;
} internal_ffmpeg_encoder_t;


// Local context
namespace
{

int init_video_xdcam(internal_ffmpeg_encoder_t * enc, Ingex::VideoRaster::EnumType raster, int64_t start_tc)
{
    /* allocate input video frame */
    enc->inputFrame = avcodec_alloc_frame();
    if (!enc->inputFrame)
    {
        fprintf(stderr, "Could not allocate input frame\n");
        return 0;
    }

    /* get codec context and set codec */
    AVCodecContext * codec_context = enc->video_st->codec;

    codec_context->codec_id = CODEC_ID_MPEG2VIDEO;
    codec_context->codec_type = CODEC_TYPE_VIDEO;
   
    /* set interlace parameters */
    int width;
    int height;
    int fps_num;
    int fps_den;
    Ingex::Interlace::EnumType interlace;
    Ingex::VideoRaster::GetInfo(raster, width, height, fps_num, fps_den, interlace);

    switch (interlace)
    {
    case Ingex::Interlace::TOP_FIELD_FIRST:
        enc->inputFrame->interlaced_frame = 1;
        enc->inputFrame->top_field_first = 1;
        codec_context->flags |= CODEC_FLAG_INTERLACED_DCT;
        break;
    case Ingex::Interlace::BOTTOM_FIELD_FIRST:
        enc->inputFrame->interlaced_frame = 1;
        enc->inputFrame->top_field_first = 0;
        codec_context->flags |= CODEC_FLAG_INTERLACED_DCT;
        break;
    case Ingex::Interlace::NONE:
    default:
        enc->inputFrame->interlaced_frame = 0;
        enc->inputFrame->top_field_first = 0;
        break;
    }

    codec_context->gop_size = 12;
#if defined(FFMPEG_NONLINEAR_PATCH)
    // non-linear quantization is implemented, except for qscale 1 which could cause an integer overflow
    // in the quantization process for large DCT coefficients
    codec_context->qmin = 2;
    codec_context->qmax = 31;
#else
#warning "Using FFmpeg without MPEG non-linear quantization patch"
    codec_context->qmin = 1;
    codec_context->qmax = 12;
#endif
    codec_context->me_method = 5; // (epzs)
   // codec_context->flags |= CODEC_FLAG_CLOSED_GOP; //No need for closed GOP in non MXF/XDCAM
    codec_context->flags2 |= CODEC_FLAG2_NON_LINEAR_QUANT;
    codec_context->flags2 |= CODEC_FLAG2_INTRA_VLC;
    codec_context->rc_max_available_vbv_use =1;
    codec_context->rc_min_vbv_overflow_use = 1;
    codec_context->lmin = 1 * FF_QP2LAMBDA;
    codec_context->lmax = 3 * FF_QP2LAMBDA;
    codec_context->max_b_frames =2;
    codec_context->mb_decision = FF_MB_DECISION_SIMPLE;
    codec_context->pix_fmt = PIX_FMT_YUV422P;
    codec_context->bit_rate = 50000000;
    codec_context->rc_max_rate = 60000000;
    codec_context->rc_min_rate = 0;
    codec_context->rc_buffer_size = 50000000; 
    codec_context->stream_codec_tag = MKTAG('x', 'd', '5', 'c'); // for mpeg encoder
    codec_context->codec_tag = MKTAG('x', 'd', '5', 'c'); // for quicktime wrapper
    

    /* Setting this non-zero gives us a timecode track in MOV format */
    codec_context->timecode_frame_start = start_tc;

 // some formats want stream headers to be seperate
    if(!strcmp(enc->oc->oformat->name, "mp4")
        || !strcmp(enc->oc->oformat->name, "mov")
        || !strcmp(enc->oc->oformat->name, "3gp"))
    {
        codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    /* find the video encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "video codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* open the codec */
    avcodec_set_dimensions(codec_context, enc->output_width, enc->output_height);
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open video codec\n");
        return -1;
    }


    /* allocate output buffer */
    enc->video_outbuf = NULL;
    if (!(enc->oc->oformat->flags & AVFMT_RAWPICTURE))
    {
        enc->video_outbuf_size = (codec_context->bit_rate / 200)+2000000;
        enc->video_outbuf = (uint8_t *)malloc(enc->video_outbuf_size);
        }

    return 0;
}

/* initialise video stream for DVD encoding */
int init_video_dvd(internal_ffmpeg_encoder_t * enc, Ingex::VideoRaster::EnumType raster)
{
    AVCodecContext * codec_context = enc->video_st->codec;

    /* mpeg2video is the default codec for DVD format */
    codec_context->codec_id = CODEC_ID_MPEG2VIDEO;
    codec_context->codec_type = CODEC_TYPE_VIDEO;
    codec_context->pix_fmt = PIX_FMT_YUV420P;


    /* put dvd parameters */
    codec_context->bit_rate = 5000000;
    codec_context->rc_max_rate = 9000000;
    codec_context->rc_min_rate = 0;
    codec_context->rc_buffer_size = 224*1024*8;
    codec_context->gop_size = 15; /* emit one intra frame every 15 frames at most */
    codec_context->max_b_frames = 2; /* also add B frames */

    /* needed to avoid using macroblocks in which some coeffs overflow 
       this doesnt happen with normal video, it just happens here as the 
       motion of the chroma plane doesnt match the luma plane */
    codec_context->mb_decision = 2;

    /* set coding parameters which depend on video raster */
    int top_field_first;
    switch (raster)
    {
    case Ingex::VideoRaster::PAL_4x3:
    case Ingex::VideoRaster::PAL_16x9:
        top_field_first = 1;
        break;
    case Ingex::VideoRaster::NTSC_4x3:
    case Ingex::VideoRaster::NTSC_16x9:
        top_field_first = 0;
        enc->crop_480_ntsc_mpeg = 1;
        enc->output_width = 720;
        enc->output_height = 480;
        break;
    default:
        top_field_first = 1;
        break;
    }

    /* find the video encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "video codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* open the codec */
    avcodec_set_dimensions(codec_context, enc->output_width, enc->output_height);
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open video codec\n");
        return -1;
    }

    /* allocate input video frame */
    enc->inputFrame = avcodec_alloc_frame();
    if (!enc->inputFrame)
    {
        fprintf(stderr, "Could not allocate input frame\n");
        return 0;
    }
    enc->inputFrame->top_field_first = top_field_first;

    /* allocate output buffer */
    enc->video_outbuf = NULL;
    if (!(enc->oc->oformat->flags & AVFMT_RAWPICTURE))
    {
        enc->video_outbuf_size = 550000;
        enc->video_outbuf = (uint8_t *)malloc(enc->video_outbuf_size);
    }

    return 0;
}

/* initialise video stream for MPEG-4 encoding */
int init_video_mpeg4(internal_ffmpeg_encoder_t * enc, Ingex::VideoRaster::EnumType raster, int64_t start_tc)
{
    AVCodecContext * codec_context = enc->video_st->codec;

    codec_context->codec_id = CODEC_ID_MPEG4;
    codec_context->codec_type = CODEC_TYPE_VIDEO;
    codec_context->pix_fmt = PIX_FMT_YUV420P;

    /* bit rate */
    const int kbit_rate = 800;

    /* set coding parameters */
    codec_context->bit_rate = kbit_rate * 1000;

    /* Setting this non-zero gives us a timecode track in MOV format */
    codec_context->timecode_frame_start = start_tc;


    // some formats want stream headers to be seperate
    if(!strcmp(enc->oc->oformat->name, "mp4")
        || !strcmp(enc->oc->oformat->name, "mov")
        || !strcmp(enc->oc->oformat->name, "3gp"))
    {
        codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    /* set coding parameters which depend on video raster */
    int top_field_first;
    switch (raster)
    {
    case Ingex::VideoRaster::PAL_4x3:
    case Ingex::VideoRaster::PAL_16x9:
        top_field_first = 1;
        break;
    case Ingex::VideoRaster::NTSC_4x3:
    case Ingex::VideoRaster::NTSC_16x9:
        top_field_first = 0;
        enc->crop_480_ntsc_mpeg = 1;
        enc->output_width = 720;
        enc->output_height = 480;
        break;
    default:
        top_field_first = 1;
        break;
    }

    /* find the video encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "video codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* open the codec */
    avcodec_set_dimensions(codec_context, enc->output_width, enc->output_height);
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open video codec\n");
        return -1;
    }

    /* allocate input video frame */
    enc->inputFrame = avcodec_alloc_frame();
    if (!enc->inputFrame)
    {
        fprintf(stderr, "Could not allocate input frame\n");
        return 0;
    }
    enc->inputFrame->top_field_first = top_field_first;

    /* allocate output buffer */
    enc->video_outbuf = NULL;
    if (!(enc->oc->oformat->flags & AVFMT_RAWPICTURE))
    {
        // not sure what size is appropriate for mp4
        enc->video_outbuf_size = 550000;
        enc->video_outbuf = (uint8_t *)malloc(enc->video_outbuf_size);
    }

    return 0;
}

/* initialise video stream for h264 encoding */
int init_video_h264(internal_ffmpeg_encoder_t * enc, MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster, int64_t start_tc)
{
    AVCodecContext * codec_context = enc->video_st->codec;

    codec_context->codec_id = CODEC_ID_H264;
    codec_context->codec_type = CODEC_TYPE_VIDEO;
    codec_context->pix_fmt = PIX_FMT_YUV420P;
    codec_context->codec_tag = MKTAG('a', 'v', 'c', '1');

    /* allocate input video frame */
    enc->inputFrame = avcodec_alloc_frame();
    if (!enc->inputFrame)
    {
        fprintf(stderr, "Could not allocate input frame\n");
        return 0;
    }

    /* set interlace parameters */
    int width;
    int height;
    int fps_num;
    int fps_den;
    Ingex::Interlace::EnumType interlace;
    Ingex::VideoRaster::GetInfo(raster, width, height, fps_num, fps_den, interlace);
    switch (interlace)
    {
    case Ingex::Interlace::TOP_FIELD_FIRST:
        enc->inputFrame->interlaced_frame = 1;
        enc->inputFrame->top_field_first = 1;
        codec_context->flags |= CODEC_FLAG_INTERLACED_DCT;
        break;
    case Ingex::Interlace::BOTTOM_FIELD_FIRST:
        enc->inputFrame->interlaced_frame = 1;
        enc->inputFrame->top_field_first = 0;
        codec_context->flags |= CODEC_FLAG_INTERLACED_DCT;
        break;
    case Ingex::Interlace::NONE:
        enc->inputFrame->interlaced_frame = 0;
        enc->inputFrame->top_field_first = 0;
        break;
    }


    /* Setting this non-zero gives us a timecode track in MOV format */
    codec_context->timecode_frame_start = start_tc;

    // some formats want stream headers to be seperate
    if (!strcmp(enc->oc->oformat->name, "mp4")
        || !strcmp(enc->oc->oformat->name, "mov")
        || !strcmp(enc->oc->oformat->name, "3gp"))
    {
        codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;

    }

    /* Set coding parameters */
    int encoded_frame_size = 0;
    switch (res)
    {
    // Baseline Profile
    case MaterialResolution::MPEG4BP_AAC_MP4:
        codec_context->bit_rate = 512000;
        codec_context->rc_max_rate = 512000;
        codec_context->rc_min_rate = 0;
        codec_context->rc_buffer_size = 512000; 
        codec_context->coder_type = 0;
        codec_context->flags |= CODEC_FLAG_LOOP_FILTER;
        codec_context->me_cmp = FF_CMP_CHROMA;
        codec_context->partitions = X264_PART_I4X4 | X264_PART_P8X8;
        codec_context->me_method = ME_HEX;
        codec_context->me_subpel_quality = 3;
        codec_context->me_range = 16;
        codec_context->gop_size = 250;
        codec_context->keyint_min = 25;
        codec_context->scenechange_threshold = 40;
        codec_context->i_quant_factor = 0.71;
        codec_context->b_frame_strategy = 0;
        codec_context->qcompress=0.6;
        codec_context->qmin = 10;
        codec_context->qmax = 51;
        codec_context->max_qdiff = 4;
        codec_context->max_b_frames = 0;
        codec_context->directpred = 1;
        //codec_context->flags2 |= CODEC_FLAG2_MBTREE | CODEC_FLAG2_WPRED;
        codec_context->flags2 |= CODEC_FLAG2_WPRED;
        //codec_context->weighted_p_pred = 0;
        encoded_frame_size = 512000; // ??
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3:
        case Ingex::VideoRaster::PAL_16x9:
            if (H264_IMAGE_SCALE)
            {
                enc->scale_image = 1;
                enc->output_width = 320;
                enc->output_height = 184;
            }
            break;
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            enc->crop_480_ntsc_mpeg = 1;
            enc->output_width = 720;
            enc->output_height = 480;
            if (H264_IMAGE_SCALE)
            {
                enc->scale_image = 1;
                enc->output_width = 320;
                enc->output_height = 184;
            }
            break;
        default:
            break;
        }
        break;

    // Main Profile
    case MaterialResolution::MPEG4MP_AAC_MP4:
        codec_context->bit_rate = 1024000;
        codec_context->rc_max_rate = 1024000;
        codec_context->rc_min_rate = 0;
        codec_context->rc_buffer_size = 1024000; 
        codec_context->coder_type = 1;
        codec_context->flags |= CODEC_FLAG_LOOP_FILTER;
        codec_context->me_cmp = FF_CMP_CHROMA;
        codec_context->partitions = X264_PART_I4X4 | X264_PART_P8X8;
        codec_context->me_method = ME_HEX;
        // more than 3 in my machine and there will be frame drops
        codec_context->me_subpel_quality = 3;
        codec_context->me_range = 16;
        codec_context->gop_size = 250;
        codec_context->keyint_min = 25;
        codec_context->scenechange_threshold = 40;
        codec_context->i_quant_factor = 0.71;
        codec_context->b_frame_strategy = 1;
        codec_context->qcompress=0.6;
        codec_context->qmin = 10;
        codec_context->qmax = 51;
        codec_context->max_qdiff = 4;
        codec_context->max_b_frames = 0;
        codec_context->directpred = 1;
        // gives vbv buffer under run
        // codec_context->crf = 1;
        //codec_context->flags2 |= CODEC_FLAG2_FASTPSKIP | CODEC_FLAG2_MBTREE | CODEC_FLAG2_MIXED_REFS | CODEC_FLAG2_WPRED;
        codec_context->flags2 |= CODEC_FLAG2_FASTPSKIP | CODEC_FLAG2_MIXED_REFS | CODEC_FLAG2_WPRED;
        //codec_context->weighted_p_pred = 0;
        encoded_frame_size = 1024000; // ??
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3:
        case Ingex::VideoRaster::PAL_16x9:
            if (H264_IMAGE_SCALE)
            {
                enc->scale_image = 1;
                enc->output_width = 640;
                enc->output_height = 480;
            }
            break;
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            enc->crop_480_ntsc_mpeg = 1;
            enc->output_width = 720;
            enc->output_height = 480;
            if (H264_IMAGE_SCALE)
            {
                enc->scale_image = 1;
                enc->output_width = 640;
                enc->output_height = 480;
            }
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    /* find the video encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "video codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* open the codec */
    avcodec_set_dimensions(codec_context, enc->output_width, enc->output_height);
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open video codec\n");
        return -1;
    }

    /* allocate output buffer */
    // not sure what size is appropriate for mp4
    //enc->video_outbuf_size = 550000;
    enc->video_outbuf_size = encoded_frame_size;
    enc->video_outbuf = (uint8_t *)malloc(enc->video_outbuf_size);

    return 0;
}

/* initialise video stream for DV encoding */
int init_video_dv(internal_ffmpeg_encoder_t * enc, MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster, int64_t start_tc)
{
    /* allocate input video frame */
    enc->inputFrame = avcodec_alloc_frame();
    if (!enc->inputFrame)
    {
        fprintf(stderr, "Could not allocate input frame\n");
        return 0;
    }

    /* get codec context and set codec */
    AVCodecContext * codec_context = enc->video_st->codec;

    codec_context->codec_id = CODEC_ID_DVVIDEO;
    codec_context->codec_type = CODEC_TYPE_VIDEO;

    /* set interlace parameters */
    int width;
    int height;
    int fps_num;
    int fps_den;
    Ingex::Interlace::EnumType interlace;
    Ingex::VideoRaster::GetInfo(raster, width, height, fps_num, fps_den, interlace);

    switch (interlace)
    {
    case Ingex::Interlace::TOP_FIELD_FIRST:
        enc->inputFrame->interlaced_frame = 1;
        enc->inputFrame->top_field_first = 1;
        codec_context->flags |= CODEC_FLAG_INTERLACED_DCT;
        break;
    case Ingex::Interlace::BOTTOM_FIELD_FIRST:
        enc->inputFrame->interlaced_frame = 1;
        enc->inputFrame->top_field_first = 0;
        codec_context->flags |= CODEC_FLAG_INTERLACED_DCT;
        break;
    case Ingex::Interlace::NONE:
    default:
        enc->inputFrame->interlaced_frame = 0;
        enc->inputFrame->top_field_first = 0;
        break;
    }


    int encoded_frame_size = 0;

    // Set coding parameters according to SMPTE 370M (note horizontal scaling in HD formats)
    switch (res)
    {
    case MaterialResolution::DV25_MOV:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3_B:
        case Ingex::VideoRaster::PAL_16x9_B:
            codec_context->pix_fmt = PIX_FMT_YUV420P;
            encoded_frame_size = 144000;
            break;
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            codec_context->pix_fmt = PIX_FMT_YUV411P;
            encoded_frame_size = 120000;
            enc->crop_480_ntsc_dv = 1;
            enc->output_width = 720;
            enc->output_height = 480;
            break;
        default:
            break;
        }
        break;
    case MaterialResolution::DV50_MOV:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3_B:
        case Ingex::VideoRaster::PAL_16x9_B:
            codec_context->pix_fmt = PIX_FMT_YUV422P;
            encoded_frame_size = 288000;
            break;
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            codec_context->pix_fmt = PIX_FMT_YUV422P;
            encoded_frame_size = 240000;
            enc->crop_480_ntsc_dv = 1;
            enc->output_width = 720;
            enc->output_height = 480;
            break;
        default:
            break;
        }
    case MaterialResolution::DV100_MOV:
        codec_context->pix_fmt = PIX_FMT_YUV422P;
        switch (raster)
        {
        case Ingex::VideoRaster::SMPTE274_25I:
        case Ingex::VideoRaster::SMPTE274_25PSF:
        case Ingex::VideoRaster::SMPTE274_25P:
            enc->scale_image = 1;
            enc->output_width = 1440;
            enc->output_height = 1080;
            encoded_frame_size = 576000;
            break;
        case Ingex::VideoRaster::SMPTE274_29I:
        case Ingex::VideoRaster::SMPTE274_29PSF:
        case Ingex::VideoRaster::SMPTE274_29P:
            enc->scale_image = 1;
            enc->output_width = 1280;
            enc->output_height = 1080;
            encoded_frame_size = 480000;
            break;
        // 720p formats not tested
        case Ingex::VideoRaster::SMPTE296_50P:
            enc->scale_image = 1;
            enc->output_width = 960;
            enc->output_height = 720;
            encoded_frame_size = 288000;
            break;
        case Ingex::VideoRaster::SMPTE296_59P:
            enc->scale_image = 1;
            enc->output_width = 960;
            enc->output_height = 720;
            encoded_frame_size = 240000;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }


    /* Setting this non-zero gives us a timecode track in MOV format */
    codec_context->timecode_frame_start = start_tc;


    // some formats want stream headers to be seperate
    if (!strcmp(enc->oc->oformat->name, "mp4")
        || !strcmp(enc->oc->oformat->name, "mov")
        || !strcmp(enc->oc->oformat->name, "3gp"))
    {
        codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    /* find the video encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "video codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* open the codec */
    avcodec_set_dimensions(codec_context, enc->output_width, enc->output_height);
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open video codec\n");
        return -1;
    }

    /* allocate output buffer */
    enc->video_outbuf = NULL;
    if (!(enc->oc->oformat->flags & AVFMT_RAWPICTURE))
    {
        enc->video_outbuf_size = encoded_frame_size;
        enc->video_outbuf = (uint8_t *)malloc(enc->video_outbuf_size);
    }

    return 0;
}

/* initialise audio streams for PCM encoding */
int init_audio_pcm(audio_encoder_t * aenc)
{
    AVCodecContext * codec_context = aenc->audio_st->codec;

    /* 16-bit signed big-endian */
    codec_context->codec_id = CODEC_ID_PCM_S16BE;
    codec_context->codec_type = CODEC_TYPE_AUDIO;

    /* find the audio encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "audio codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* coding parameters */
    codec_context->sample_rate = 48000;
    codec_context->channels = aenc->num_audio_channels;

    /* open it */
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open audio codec\n");
        return -1;
    }

    /* set flag for PCM codec */
    aenc->is_pcm = 1;

    /* Set up audio buffer parameters */
    int nch = aenc->num_audio_channels;
    aenc->audio_inbuf_size = MAX_VIDEO_FRAME_AUDIO_SIZE * nch * sizeof(short);

    /* coded frame max size */
    aenc->audio_outbuf_size = MAX_VIDEO_FRAME_AUDIO_SIZE * nch * sizeof(short);

    return 0;
}

/* initialise audio stream for DVD encoding */
int init_audio_dvd(audio_encoder_t * aenc)
{
    AVCodecContext * codec_context = aenc->audio_st->codec;

    /* mp2 is the default audio codec for DVD format */
    codec_context->codec_id = CODEC_ID_MP2;
    codec_context->codec_type = CODEC_TYPE_AUDIO;

    /* find the audio encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "audio codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* coding parameters */
    codec_context->bit_rate = 256000;
    codec_context->sample_rate = 48000;
    codec_context->channels = 2;

    /* open it */
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open audio codec\n");
        return -1;
    }

    /* Set up audio buffer parameters */
    int nch = aenc->num_audio_channels;
    /* We have to do some buffering of input, hence the * 2 on the end */
    aenc->audio_inbuf_size = MAX_VIDEO_FRAME_AUDIO_SIZE * nch * sizeof(short) * 2;

    /* samples per coded frame */
    aenc->audio_samples_per_output_frame = MPA_FRAME_SIZE;
    /* coded frame size */
    aenc->audio_outbuf_size = 960; // enough for 320 kbit/s

    return 0;
}

/* initialise audio stream for MP3 encoding */
int init_audio_mp3(audio_encoder_t * aenc)
{
    AVCodecContext * codec_context = aenc->audio_st->codec;

    codec_context->codec_id = CODEC_ID_MP3;
    codec_context->codec_type = CODEC_TYPE_AUDIO;

    const int kbit_rate = 96;

    /* find the audio encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "audio codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* coding parameters */
    codec_context->bit_rate = kbit_rate * 1000;
    codec_context->sample_rate = 48000;
    codec_context->channels = 2;

    /* open it */
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open audio codec\n");
        return -1;
    }

    /* Set up audio buffer parameters */
    int nch = aenc->num_audio_channels;
    /* We have to do some buffering of input, hence the * 2 on the end */
    aenc->audio_inbuf_size = MAX_VIDEO_FRAME_AUDIO_SIZE * nch * sizeof(short) * 2;

    /* samples per coded frame */
    aenc->audio_samples_per_output_frame = MPA_FRAME_SIZE;
    /* coded frame size */
    aenc->audio_outbuf_size = 960; // enough for 320 kbit/s

    return 0;
}

/* initialise audio stream for AAC encoding */
int init_audio_aac(audio_encoder_t * aenc)
{
    AVCodecContext * codec_context = aenc->audio_st->codec;

    codec_context->codec_id = CODEC_ID_AAC;
    codec_context->codec_type = CODEC_TYPE_AUDIO;
    codec_context->profile = FF_PROFILE_AAC_LOW;
    codec_context->codec_tag = MKTAG('m', 'p', '4', 'a');

    const int kbit_rate = 96;

    /* find the audio encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "audio codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* coding parameters */
    codec_context->bit_rate = kbit_rate * 1000;
    codec_context->sample_rate = 48000;
    codec_context->channels = 2;
    //codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
    //codec_context->cutoff = 0.9;
    //codec_context->flags |= CODEC_FLAG_QSCALE;
    codec_context->sample_fmt = SAMPLE_FMT_S16;

    /* open it */
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open audio codec\n");
        return -1;
    }

    /* Set up audio buffer parameters */
    int nch = aenc->num_audio_channels;
    /* We have to do some buffering of input, hence the * 2 on the end */
    aenc->audio_inbuf_size = MAX_VIDEO_FRAME_AUDIO_SIZE * nch * sizeof(short) * 2;

    /* samples per coded frame */
    aenc->audio_samples_per_output_frame = AAC_FRAME_SIZE;
    /* coded frame size */
    aenc->audio_outbuf_size = 860; // enough for 320 kbit/s

    return 0;
}

void cleanup (internal_ffmpeg_encoder_t * enc)
{
    if (enc)
    {
        if (enc->oc)
        {
            // free the streams
            for (unsigned int i = 0; i < enc->oc->nb_streams; ++i)
            {
                av_freep(&enc->oc->streams[i]);
            }
            av_free(enc->oc);
        }

        av_free(enc->inputFrame);
        av_free(enc->video_outbuf);
        av_free(enc->tmpBuffer);
        av_free(enc->tmpFrame);

        for (unsigned int i = 0; i < enc->num_audio_streams; ++i)
        {
            audio_encoder_t * aenc = enc->audio_encoder[i];
            av_free(aenc->audio_outbuf);
            av_free(aenc->audio_inbuf);
            av_free(aenc);
        }

        av_free(enc);
        enc = NULL;
    }
}

int write_video_frame(internal_ffmpeg_encoder_t * enc, uint8_t * p_video)
{
    AVFormatContext * oc = enc->oc;
    AVStream * st = enc->video_st;
    AVCodecContext * c = enc->video_st->codec;
    
    // Use avpicture_fill to set pointers and linesizes in inputFrame
    AVPicture * picture = (AVPicture *) enc->inputFrame;
    avpicture_fill(picture, (uint8_t*)p_video,
        c->pix_fmt,
        enc->input_width, enc->input_height);
    int width = enc->input_width;
    int height = enc->input_height;

    // Cropping options
    if (enc->crop_480_ntsc_dv)
    {
        // Skip top 4 lines.
        // See SMPTE 314M-2005
        picture->data[0] += 4 * picture->linesize[0];
        picture->data[1] += 4 * picture->linesize[1];
        picture->data[2] += 4 * picture->linesize[2];

        // Bottom 2 lines will also be skipped.
        width = 720;
        height = 480;
    }
    else if (enc->crop_480_ntsc_mpeg)
    {
        // Skip top 4 lines.
        // See SMPTE RP 202-2008
        // Numbers appear a bit wierd because it's 4:2:0
        picture->data[0] += 4 * picture->linesize[0];
        picture->data[1] += 2 * picture->linesize[1];
        picture->data[2] += 2 * picture->linesize[2];

        // Bottom 2 lines will also be skipped.
        width = 720;
        height = 480;
    }

    // Scaling options
    if (enc->scale_image)
    {
        // Set up parameters for scale
        enc->scale_context = sws_getCachedContext(enc->scale_context,
                    width, height, // input WxH
                    c->pix_fmt,
                    enc->output_width, enc->output_height, // output WxH
                    c->pix_fmt,
                    SWS_FAST_BILINEAR,
                    NULL, NULL, NULL);

        // Input image is in picture/inputFrame

        // Output of scale operation goes into tmpFrame
        avpicture_fill(enc->tmpFrame, enc->tmpBuffer,
            c->pix_fmt,
            c->width, c->height);

        // Perform the scale
        sws_scale(enc->scale_context,
            picture->data, picture->linesize,
            0, enc->input_height,
            enc->tmpFrame->data, enc->tmpFrame->linesize);

        width =  enc->output_width;
        height =  enc->output_height;

        // Set picture/inputFrame to the scaled image
        avpicture_fill(picture, enc->tmpBuffer,
            c->pix_fmt,
            width, height);
    }

    /* encode the image */
    int out_size = avcodec_encode_video(c, enc->video_outbuf, enc->video_outbuf_size, enc->inputFrame);

    /* debug */
    //fprintf(stderr, "avcodec_encode_video() returned %d\n", out_size);

    if (out_size < 0)
    {
        fprintf(stderr, "error compressing video!\n");
        return -1;
    }
    else if (out_size == 0)
    {
        // out_size == 0 means the image was buffered
    }
    else
    {
        // out_size > 0
        AVPacket pkt;
        av_init_packet(&pkt);
            
        const int64_t av_nopts_value = AV_NOPTS_VALUE; // avoids compiler warning
        if (c->coded_frame->pts != av_nopts_value)
        {
            pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);
        }
        else
        {
            pkt.pts = av_rescale_q(enc->video_pts++, c->time_base, st->time_base);
        }
        if (c->coded_frame->key_frame)
        {
            pkt.flags |= PKT_FLAG_KEY;
        }
        pkt.stream_index = st->index;
        pkt.data = enc->video_outbuf;
        pkt.size = out_size;
        
        /* write the compressed frame to the media file */
        int ret = av_write_frame(oc, &pkt);

        if (ret != 0)
        {
            fprintf(stderr, "Error while writing video frame\n");
            return -1;
        }
    }

    return 0;
}

int write_audio_frame(internal_ffmpeg_encoder_t * enc, int stream_i, short * p_audio, int num_samples)
{
    AVFormatContext * oc = enc->oc;
    audio_encoder_t * aenc = enc->audio_encoder[stream_i];
    AVStream * st = aenc->audio_st;
    AVCodecContext * c = st->codec;

    AVPacket pkt;
    av_init_packet(&pkt);

    //fprintf(stderr, "Writing audio frame, to audio stream %d, outbuf_size = %d\n", stream_i, aenc->audio_outbuf_size);
    if (aenc->is_pcm)
    {
        int nch = aenc->num_audio_channels;
        pkt.size = avcodec_encode_audio(c, aenc->audio_outbuf, num_samples * nch * 2, p_audio);
    }
    else
    {
        pkt.size = avcodec_encode_audio(c, aenc->audio_outbuf, aenc->audio_outbuf_size, p_audio);
    }

    pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);
    pkt.flags |= PKT_FLAG_KEY;
    pkt.stream_index = st->index;
    pkt.data = aenc->audio_outbuf;

    /* write the compressed frame to the media file */
    if (av_write_frame(oc, &pkt) != 0)
    {
        fprintf(stderr, "Error while writing audio frame\n");
        return -1;
    }

    return 0;
}

} // namespace

extern ffmpeg_encoder_av_t * ffmpeg_encoder_av_init (const char * filename,
                                                        MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster,
                                                        int64_t start_tc, int num_threads,
                                                        unsigned int num_audio_streams, unsigned int num_audio_channels_per_stream)
{
    /* check number of audio streams */
    //fprintf(stderr, "ffmpeg_encoder_av_init(): %d audio streams requested\n", num_audio_streams);

    internal_ffmpeg_encoder_t * enc;
    AVOutputFormat * fmt;
    const char * fmt_name = 0;

    switch (res)
    {
    case MaterialResolution::DVD:
        fmt_name = "dvd";
        break;
    case MaterialResolution::MPEG4_MP3_MOV:
    case MaterialResolution::MPEG4_PCM_MOV:
        fmt_name = "mov";
        break;
    case MaterialResolution::MPEG4BP_AAC_MP4:
    case MaterialResolution::MPEG4MP_AAC_MP4:
        fmt_name = "mp4";
        break;
    case MaterialResolution::DV25_MOV:
    case MaterialResolution::DV50_MOV:
    case MaterialResolution::DV100_MOV:
        fmt_name = "mov";
        break;
    case MaterialResolution::XDCAMHD422_MOV:
        fmt_name = "mov";
    break;
    default:
        break;
    }

    if (!fmt_name)
    {
        fprintf (stderr, "ffmpeg_encoder_av_init: resolution not recognised\n");
        return NULL;
    }

    /* register all ffmpeg codecs and formats */
    av_register_all();

    /* Check if format exists */
    fmt = guess_format (fmt_name, NULL, NULL);
    if (!fmt)
    {
        fprintf (stderr, "ffmpeg_encoder_av_init: format \"%s\" not registered\n", fmt_name);
        return NULL;
    }

    /* Check number of audio tracks */
    if (num_audio_streams > MAX_AUDIO_STREAMS)
    {
        fprintf (stderr, "ffmpeg_encoder_av_init: too many audio tracks (%d)\n", num_audio_streams);
        return NULL;
    }

    /* Allocate encoder object and set all members to zero */
    enc = (internal_ffmpeg_encoder_t *)av_mallocz (sizeof(internal_ffmpeg_encoder_t));
    if (!enc)
    {
        fprintf (stderr, "Could not allocate encoder object\n");
        return NULL;
    }

    /* Allocate audio encoder objects and set all members to zero */
    for (unsigned int i = 0; i < num_audio_streams; ++i)
    {
        enc->audio_encoder[i] = (audio_encoder_t *)av_mallocz (sizeof(audio_encoder_t));
    }


    /* Set audio track info */
    enc->num_audio_streams = num_audio_streams;

    /* Allocate the output media context */
#if LIBAVFORMAT_VERSION_INT > ((52<<16)+(25<<8)+0)
    enc->oc = avformat_alloc_context();
#else
    enc->oc = av_alloc_format_context();
#endif
    if (!enc->oc)
    {
        cleanup(enc);
        fprintf (stderr, "Could not allocate output format context\n");
        return NULL;
    }
    enc->oc->oformat = fmt;

    /* Set parameters for output media context*/
    switch (res)
    {
    case MaterialResolution::DVD:
        enc->oc->packet_size = 2048;
        enc->oc->mux_rate = 10080000;
        enc->oc->preload = (int) (0.5 * AV_TIME_BASE); /* 500 ms */
        enc->oc->max_delay = (int) (0.7 * AV_TIME_BASE); /* 700 ms */
        break;
    default:
        break;
    }

    int width;
    int height;
    int fps_num;
    int fps_den;
    Ingex::Interlace::EnumType interlace;
    Ingex::VideoRaster::GetInfo(raster, width, height, fps_num, fps_den, interlace);


    /* Add the video stream */
    enc->video_st = av_new_stream(enc->oc, 0);
    if (!enc->video_st)
    {
        fprintf(stderr, "Could not alloc video stream\n");
        cleanup(enc);
        return NULL;
    }

    /* Set sample aspect for stream and codec */
    Ingex::Rational sar = Ingex::VideoRaster::SampleAspectRatio(raster);
    enc->video_st->sample_aspect_ratio.num = sar.numerator;
    enc->video_st->sample_aspect_ratio.den = sar.denominator;
    enc->video_st->codec->sample_aspect_ratio.num = sar.numerator;
    enc->video_st->codec->sample_aspect_ratio.den = sar.denominator;

    // Set size
    // output width/height may be changed during codec initialisation
    //avcodec_set_dimensions(enc->video_st->codec, width, height);
    enc->input_width = width;
    enc->input_height = height;
    enc->output_width = width;
    enc->output_height = height;

    /* Set frame rate */
    enc->video_st->r_frame_rate.num = fps_num;
    enc->video_st->r_frame_rate.den = fps_den;

    /* Set time base: This is the fundamental unit of time (in seconds) in terms
       in terms of which frame timestamps are represented.
       For fixed-fps content, timebase should be 1/framerate and timestamp
       increments should be identically 1. */
    enc->video_st->codec->time_base.num = fps_den;
    enc->video_st->codec->time_base.den = fps_num;


    // Setup ffmpeg threads
    if (num_threads != 0)
    {
        int threads = 0;
        if (num_threads == THREADS_USE_BUILTIN_TUNING)
        {
            // select number of threaded based on picture size/codec type
            switch (res)
            {
            case MaterialResolution::DV100_MOV:
            case MaterialResolution::XDCAMHD422_MOV:
                threads = 4;
                break;
            default:
                threads = 0;
                break;
            }
        }
        else
        {
            // use number of threads specified by function arg
            threads = num_threads;
        }

        // Initialise ffmpeg for multiple threads if appropriate
        if (threads > 0)
        {
            avcodec_thread_init(enc->video_st->codec, threads);
        }
    }

    //fprintf(stderr, "vcodec_context->thread_count = %d\n", enc->video_st->codec->thread_count);

    /* Initialise video codec */
    int init_video_result;
    switch (res)
    {
    case MaterialResolution::DVD:
        init_video_result = init_video_dvd(enc, raster);
        break;
    case MaterialResolution::MPEG4_MP3_MOV:
    case MaterialResolution::MPEG4_PCM_MOV:
        init_video_result = init_video_mpeg4(enc, raster, start_tc);
        break;
    case MaterialResolution::MPEG4BP_AAC_MP4:
    case MaterialResolution::MPEG4MP_AAC_MP4:
        init_video_result = init_video_h264(enc, res, raster, start_tc);
        break;

    case MaterialResolution::DV25_MOV:
    case MaterialResolution::DV50_MOV:
    case MaterialResolution::DV100_MOV:
        init_video_result = init_video_dv(enc, res, raster, start_tc);
        break;
    case MaterialResolution::XDCAMHD422_MOV:
        init_video_result = init_video_xdcam(enc, raster, start_tc);
        break;
    default:
        init_video_result = -1;
        break;
    }

    // Allocate tmp buffer if needed
    if (enc->scale_image)
    {
        enc->tmpFrame = (AVPicture *)av_mallocz(sizeof(AVPicture));
        enc->tmpBuffer = (uint8_t *)av_mallocz(enc->output_width * enc->output_height * 2);
    }

    /* Add the audio streams */
    for (unsigned int i = 0; i < enc->num_audio_streams; ++i)
    {
        audio_encoder_t * aenc = enc->audio_encoder[i];

        aenc->audio_st = av_new_stream(enc->oc, 1 + i);
        if (!aenc->audio_st)
        {
            fprintf(stderr, "Could not alloc audio stream[%d]\n", i);
            cleanup(enc);
            return NULL;
        }
    }

    /* Initialise audio codecs */
    for (unsigned int i = 0; i < enc->num_audio_streams; ++i)
    {
        audio_encoder_t * aenc = enc->audio_encoder[i];
        aenc->num_audio_channels = num_audio_channels_per_stream;
        switch (res)
        {
        case MaterialResolution::DVD:
            init_audio_dvd(aenc);
            break;
        case MaterialResolution::MPEG4_MP3_MOV:
            init_audio_mp3(aenc);
            break;
        case MaterialResolution::MPEG4BP_AAC_MP4:
        case MaterialResolution::MPEG4MP_AAC_MP4:
            init_audio_aac(aenc);
            break;
        case MaterialResolution::DV25_MOV:
        case MaterialResolution::DV50_MOV:
        case MaterialResolution::DV100_MOV:
        case MaterialResolution::XDCAMHD422_MOV:
        case MaterialResolution::MPEG4_PCM_MOV:
            init_audio_pcm(aenc);
            break;
        default:
            break;
        }

        /* allocate audio input buffer */
        aenc->audio_inbuf = (short *)av_malloc(aenc->audio_inbuf_size);

        /* allocate audio (coded) output buffer */
        aenc->audio_outbuf = (uint8_t *)av_malloc(aenc->audio_outbuf_size);
    }

    // Set separate stream header if format requires it.
    if (!strcmp(enc->oc->oformat->name, "mp4")
        || !strcmp(enc->oc->oformat->name, "mov")
        || !strcmp(enc->oc->oformat->name, "3gp"))
    {
        enc->video_st->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        for (unsigned int i = 0; i < enc->num_audio_streams; ++i)
        {
            audio_encoder_t * aenc = enc->audio_encoder[i];
            aenc->audio_st->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            //aenc->audio_st->codec->sample_format = SAMPLE_FMT_S16; // possibly needed for ffmpeg 0.6
        }
    }

    /*
    * Set the output parameters - must be done even if no parameters 
    */
    if (av_set_parameters(enc->oc, NULL) < 0)
    {
        fprintf(stderr, "Invalid output format parameters\n");
        cleanup(enc);
        return NULL;
    }

    /* Set output filename */
    snprintf (enc->oc->filename, sizeof(enc->oc->filename), "%s", filename);

    /* Open the output file */
    if (!(fmt->flags & AVFMT_NOFILE))
    {
        if (url_fopen(&enc->oc->pb, enc->oc->filename, URL_WRONLY) < 0)
        {
            cleanup(enc);
            fprintf(stderr, "Could not open '%s'\n", filename);
            return NULL;
        }
    }

    /* check number of audio streams */
    //fprintf(stderr, "ffmpeg_encoder_av_init(): %d streams total\n", enc->oc->nb_streams);

    if (-1 != init_video_result)
    {
        /* Write stream header if any */
        av_write_header(enc->oc);
    }
    else
    {
        ffmpeg_encoder_av_close(enc);
        enc = 0;
    }

    return (ffmpeg_encoder_av_t *)enc;
}

extern int ffmpeg_encoder_av_encode_video (ffmpeg_encoder_av_t * in_enc, uint8_t * p_video)
{
    internal_ffmpeg_encoder_t * enc = (internal_ffmpeg_encoder_t *)in_enc;
    if (!enc)
    {
        return -1;
    }

    /* encode and write video */
    if (p_video && write_video_frame (enc, p_video) == -1)
    {
        return -1;
    }

    return 0;
}

extern int ffmpeg_encoder_av_encode_audio (ffmpeg_encoder_av_t * in_enc, int stream_i, int num_samples, short * p_audio)
{
    internal_ffmpeg_encoder_t * enc = (internal_ffmpeg_encoder_t *)in_enc;
    if (!enc)
    {
        return -1;
    }

    audio_encoder_t * aenc = enc->audio_encoder[stream_i];
    int nch = aenc->num_audio_channels;

    /* encode and write audio */
    if (p_audio && aenc->is_pcm)
    {
        if (write_audio_frame (enc, stream_i, p_audio, num_samples) == -1)
        {
            return -1;
        }
    }
    else if (p_audio)
    {
        /*
        Audio comes in in video frames (num_samples e.g. 1920) but may be encoded
        as MPEG frames (1152 samples) so we need to buffer.
        NB. audio_inbuf_offset is in terms of 16-bit samples, hence the
        rather confusing pointer arithmetic.
        */
        //fprintf (stderr, "audio_inbuf_offset = %d, num_samples = %d, num_channels = %d\n", aenc->audio_inbuf_offset, num_samples, nch);
        memcpy (aenc->audio_inbuf + aenc->audio_inbuf_offset, p_audio, num_samples * 2 * nch);
        aenc->audio_inbuf_offset += (num_samples * nch);
        int diff;
        while ((diff = aenc->audio_inbuf_offset - (aenc->audio_samples_per_output_frame * nch)) >= 0)
        {
            if (write_audio_frame (enc, stream_i, aenc->audio_inbuf, num_samples) == -1)
            {
                return -1;
            }
            memmove (aenc->audio_inbuf, aenc->audio_inbuf + (aenc->audio_samples_per_output_frame * nch), diff * 2);
            aenc->audio_inbuf_offset -= (aenc->audio_samples_per_output_frame * nch);
        }
    }

    return 0;
}

extern int ffmpeg_encoder_av_close (ffmpeg_encoder_av_t * in_enc)
{
    internal_ffmpeg_encoder_t * enc = (internal_ffmpeg_encoder_t *)in_enc;
    if (!enc)
    {
        return -1;
    }

    if (enc->video_st)
    {
        avcodec_close(enc->video_st->codec);
        enc->video_st = NULL;
    }
    for (unsigned int i = 0; i < enc->num_audio_streams; ++i)
    {
        audio_encoder_t * aenc = enc->audio_encoder[i];
        if (aenc->audio_st)
        {
            avcodec_close(aenc->audio_st->codec);
            aenc->audio_st = NULL;
        }
    }

    /* write the trailer, if any */
    av_write_trailer(enc->oc);

    if (!(enc->oc->oformat->flags & AVFMT_NOFILE))
    {
        /* close the output file */
#if (LIBAVFORMAT_VERSION_INT >= ((52<<16)+(0<<8)+0))
        url_fclose(enc->oc->pb);
#else
        url_fclose(&enc->oc->pb);
#endif
    }
    cleanup (enc);
    return 0;
}

