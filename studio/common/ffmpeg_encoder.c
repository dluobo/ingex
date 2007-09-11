/*
 * $Id: ffmpeg_encoder.c,v 1.1 2007/09/11 14:08:36 stuart_hc Exp $
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
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>

#include "ffmpeg_encoder.h"


const uint16_t imx30_intra_matrix[] = {
  32, 16, 16, 17, 19, 22, 26, 34,
  16, 16, 18, 19, 21, 26, 33, 37,
  19, 18, 19, 23, 21, 28, 35, 45,
  18, 19, 20, 22, 28, 35, 32, 49,
  18, 20, 23, 29, 32, 33, 44, 45,
  18, 20, 20, 25, 35, 39, 52, 58,
  20, 22, 23, 28, 31, 44, 51, 57,
  19, 21, 26, 30, 38, 48, 45, 75
};

const uint16_t imx4050_intra_matrix[] = {
  32, 16, 16, 17, 19, 22, 23, 31,
  16, 16, 17, 19, 20, 23, 29, 29,
  19, 17, 19, 22, 19, 25, 28, 35,
  17, 19, 19, 20, 25, 28, 25, 33,
  17, 19, 22, 25, 26, 25, 31, 31,
  17, 19, 17, 20, 26, 28, 35, 36,
  19, 19, 19, 20, 22, 29, 32, 35,
  16, 17, 19, 22, 25, 31, 28, 45 
};



typedef struct 
{
    AVCodec * codec;
    AVCodecContext * codec_context;
    AVFrame * inputFrame;
    uint8_t * outputBuffer;
    unsigned int bufferSize;
    // Following needed from frame padding
    AVPicture * tmpFrame;
    uint8_t * inputBuffer;
    int padtop;
    int padColour[3];
} internal_ffmpeg_encoder_t;


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
            avcodec_close(encoder->codec_context);
        }
        av_free(encoder->codec_context);
        av_free(encoder);
    }
}

extern ffmpeg_encoder_t * ffmpeg_encoder_init (ffmpeg_encoder_resolution_t res)
{
    internal_ffmpeg_encoder_t * encoder;

    /* register all ffmpeg codecs and formats */
    av_register_all();

    /* Allocate encoder object and set all members to zero */
    encoder = av_mallocz(sizeof(internal_ffmpeg_encoder_t));
    if (!encoder)
    {
        fprintf(stderr, "Could not allocate encoder object\n");
        return 0;
    }

    /* Setup encoder */
    int codec_id;
    int imx = 0;
    switch (res)
    {
    case FF_ENCODER_RESOLUTION_DV25:
    case FF_ENCODER_RESOLUTION_DV50:
        codec_id = CODEC_ID_DVVIDEO;
        break;
    case FF_ENCODER_RESOLUTION_IMX30:
    case FF_ENCODER_RESOLUTION_IMX40:
    case FF_ENCODER_RESOLUTION_IMX50:
        codec_id = CODEC_ID_MPEG2VIDEO;
        imx = 1;
        break;
    default:
        codec_id = 0;
    }

    encoder->codec = avcodec_find_encoder(codec_id);
    if (encoder->codec)
    {
        //fprintf(stderr, "Found encoder\n");
    }
    else
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
    switch (res)
    {
    case FF_ENCODER_RESOLUTION_DV25:
        encoder->codec_context->pix_fmt = PIX_FMT_YUV420P;
        break;
    default:
        encoder->codec_context->pix_fmt = PIX_FMT_YUV422P;
        break;
    }
    encoder->codec_context->time_base.num = 1;
    encoder->codec_context->time_base.den = 25;

    int bit_rate = 0;
    int buffer_size = 0;
    int encoded_frame_size = 0;
    const uint16_t * intra_matrix = 0;
    int top_field_first = 1;
    switch (res)
    {
    case FF_ENCODER_RESOLUTION_DV25:
        encoded_frame_size = 144000;
        top_field_first = 0;
        break;
    case FF_ENCODER_RESOLUTION_DV50:
        encoded_frame_size = 288000;
        top_field_first = 0;
        break;
    case FF_ENCODER_RESOLUTION_IMX30:
        bit_rate = 30 * 1000000;
        buffer_size = 30 * 40000;
        encoded_frame_size = bit_rate / 200;
        intra_matrix = imx30_intra_matrix;
        break;
    case FF_ENCODER_RESOLUTION_IMX40:
        bit_rate = 40 * 1000000;
        buffer_size = 40 * 40000;
        encoded_frame_size = bit_rate / 200;
        intra_matrix = imx4050_intra_matrix;
        break;
    case FF_ENCODER_RESOLUTION_IMX50:
        bit_rate = 50 * 1000000;
        buffer_size = 50 * 40000;
        encoded_frame_size = bit_rate / 200;
        intra_matrix = imx4050_intra_matrix;
        break;
    default:
        break;
    }

    if (imx)
    {
        encoder->padtop = 32;

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

        encoder->codec_context->qmin = 1;
        encoder->codec_context->qmax = 3;

        encoder->codec_context->lmin = 1 * FF_QP2LAMBDA;
        encoder->codec_context->lmax = 3 * FF_QP2LAMBDA;

        encoder->codec_context->flags |=
            CODEC_FLAG_INTERLACED_DCT | CODEC_FLAG_LOW_DELAY;

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
        encoder->codec_context->intra_matrix = av_mallocz(sizeof(uint16_t) * 64);
        memcpy(encoder->codec_context->intra_matrix, intra_matrix, sizeof(uint16_t) * 64);
    }


    avcodec_set_dimensions(encoder->codec_context, 720, 576 + encoder->padtop);      // hard-coded for PAL

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

    /* alloc input buffer if we need to pad top of frame */
    if (encoder->padtop)
    {
        encoder->tmpFrame = av_mallocz(sizeof(AVPicture));
        encoder->inputBuffer = av_mallocz(720 * (576 + encoder->padtop) * 2);
        encoder->padColour[0] = 16;
        encoder->padColour[1] = 128;
        encoder->padColour[2] = 128;
    }

    /* allocate output buffer */
    encoder->bufferSize = encoded_frame_size + 50000; // not sure why extra is needed
    encoder->outputBuffer = av_mallocz(encoder->bufferSize);

    return (ffmpeg_encoder_t *) encoder;
}

extern int ffmpeg_encoder_encode(ffmpeg_encoder_t * in_encoder, uint8_t * p_video, uint8_t * * pp_enc_video)
{
    internal_ffmpeg_encoder_t * encoder = (internal_ffmpeg_encoder_t *)in_encoder;
    if (!encoder)
    {
        return -1;
    }

    if (encoder->padtop)
    {
        /* set pointers in tmpFrame to point to planes in p_video */
        avpicture_fill(encoder->tmpFrame, p_video,
            encoder->codec_context->pix_fmt,
            720, 576);

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
    else
    {
        /* set pointers in inputFrame to point to planes in p_video */
        avpicture_fill((AVPicture*)encoder->inputFrame, p_video,
            encoder->codec_context->pix_fmt,
            encoder->codec_context->width, encoder->codec_context->height);
    }


    /* compress video */
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
