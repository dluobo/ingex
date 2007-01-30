/*
 * $Id: dv_encoder.c,v 1.1 2007/01/30 12:12:04 john_f Exp $
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

#include <string.h>
#include <avcodec.h>
#include <avformat.h>

#include "dv_encoder.h"


typedef struct 
{
    AVCodec * encoder;
    AVCodecContext * enc;
    AVFrame * rawFrame;
    uint8_t * compressedBuffer;
    unsigned int compressed_size;
} internal_dv_encoder_t;

// Hardcode to 16:9 by using 0xCA instead of 0xC8 at offset 0x1C7
static const uint8_t header_metadata[] = {
0x1F,0x07,0x00,0xBF,0xF9,0x79,0x79,0x79,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0x3F,0x07,0x00,0x8F,0xF0,0xFF,0x13,0x40,0x80,0x80,0xC0,0x8F,0xF1,0xFF,0x13,0x40,
0x80,0x80,0xC0,0x8F,0xF2,0xFF,0x13,0x40,0x80,0x80,0xC0,0x8F,0xF3,0xFF,0x13,0x40,
0x80,0x80,0xC0,0x8F,0xF4,0xFF,0x13,0x40,0x80,0x80,0xC0,0x8F,0xF5,0xFF,0x13,0x40,
0x80,0x80,0xC0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0x3F,0x07,0x01,0x8F,0xF0,0xFF,0x13,0x40,0x80,0x80,0xC0,0x8F,0xF1,0xFF,0x13,0x40,
0x80,0x80,0xC0,0x8F,0xF2,0xFF,0x13,0x40,0x80,0x80,0xC0,0x8F,0xF3,0xFF,0x13,0x40,
0x80,0x80,0xC0,0x8F,0xF4,0xFF,0x13,0x40,0x80,0x80,0xC0,0x8F,0xF5,0xFF,0x13,0x40,
0x80,0x80,0xC0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0x56,0x07,0x00,0x60,0xFF,0xFF,0xE4,0xFF,0x61,0x3F,0xCA,0xFC,0xFF,0x62,0xFF,0xC1,
0x01,0x70,0x63,0xFF,0x80,0x80,0xC0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0x60,0xFF,0xFF,0xE4,0xFF,0x61,0x3F,0xCA,0xFC,0xFF,0x62,0xFF,0xC1,0x01,0x70,0x63,
0xFF,0x80,0x80,0xC0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0x56,0x07,0x01,0x60,0xFF,0xFF,0xE4,0xFF,0x61,0x3F,0xCA,0xFC,0xFF,0x62,0xFF,0xC1,
0x01,0x70,0x63,0xFF,0x80,0x80,0xC0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0x60,0xFF,0xFF,0xE4,0xFF,0x61,0x3F,0xCA,0xFC,0xFF,0x62,0xFF,0xC1,0x01,0x70,0x63,
0xFF,0x80,0x80,0xC0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0x56,0x07,0x02,0x60,0xFF,0xFF,0xE4,0xFF,0x61,0x3F,0xCA,0xFC,0xFF,0x62,0xFF,0xC1,
0x01,0x70,0x63,0xFF,0x80,0x80,0xC0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0x60,0xFF,0xFF,0xE4,0xFF,0x61,0x3F,0xCA,0xFC,0xFF,0x62,0xFF,0xC1,0x01,0x70,0x63,
0xFF,0x80,0x80,0xC0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0x76,0x07,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0x02,0xE0,0x00,0x39,0xF5,0xF3,0x05,0xD4,
0xFC,0x61,0x06,0x0A,0xFE,0x2F,0xFE,0x86,0x05,0x4A,0xFB,0x1C,0x06,0x78,0xFB,0x83,
0x01,0xE2,0xFF,0x47,0xF8,0x37,0x07,0xA0,0xFE,0x19,0x04,0x07,0xFB,0x46,0x03,0x07,
0x06,0xBD,0xFC,0x0E,0x05,0xD0,0xFE,0x4B,0x04,0xF2,0x01,0xE2,0xFB,0x5C,0xFF,0x76,
0xF7,0x55,0x07,0x0B,0x01,0x48,0xFE,0x25,0x00,0xD9,0xFE,0x61,0x04,0xD2,0xF8,0x96
};

static void cleanup (internal_dv_encoder_t * dv)
{
    if (dv)
    {
        av_free(dv->compressedBuffer);
        av_free(dv->rawFrame);
        if (dv->enc)
        {
            avcodec_close(dv->enc);
        }
        av_free(dv->enc);
        av_free(dv);
    }
}

extern dv_encoder_t * dv_encoder_init (enum dv_encoder_resolution res)
{
    internal_dv_encoder_t * dv;

    /* register all ffmpeg codecs and formats */
    av_register_all();

    dv = av_mallocz(sizeof(internal_dv_encoder_t));
    if (!dv)
    {
        fprintf(stderr, "Could not allocate encoder object\n");
        return 0;
    }
    /* set all members to 0 */
    memset(dv, 0, sizeof(internal_dv_encoder_t));

    /* Setup encoder */
    dv->encoder = avcodec_find_encoder(CODEC_ID_DVVIDEO);
    if (!dv->encoder)
    {
        fprintf(stderr, "Could not find encoder\n");
        cleanup(dv);
        return 0;
    }

    dv->enc = avcodec_alloc_context();
    if (!dv->enc)
    {
        fprintf(stderr, "Could not allocate encoder context\n");
        cleanup(dv);
        return 0;
    }

    dv->compressedBuffer = av_mallocz(720 * 576 * 2);

    /* Set parameters */
    switch (res)
    {
    case DV_ENCODER_RESOLUTION_DV25:
        dv->enc->pix_fmt = PIX_FMT_YUV420P;
        dv->compressed_size = 144000;
        break;
    case DV_ENCODER_RESOLUTION_DV50:
        dv->enc->pix_fmt = PIX_FMT_YUV422P;
        dv->compressed_size = 288000;
        break;
    default:
        fprintf(stderr, "Unknown DV resoution\n");
        cleanup(dv);
        return 0;
        break;
    }

    avcodec_set_dimensions(dv->enc, 720, 576);      // hard-coded for PAL

    /* prepare codec */
    if (avcodec_open(dv->enc, dv->encoder) < 0)
    {
        fprintf(stderr, "Could not open encoder\n");
        return 0;
    }

    /* allocate frame buffers */
    dv->rawFrame = avcodec_alloc_frame();
    if (!dv->rawFrame)
    {
        fprintf(stderr, "Could not allocate raw frame\n");
        return 0;
    }

    return (dv_encoder_t *)dv;
}

extern int dv_encoder_encode (dv_encoder_t *in_dv, uint8_t *p_video, uint8_t * * pp_enc_video, int32_t frame_number)
{
    internal_dv_encoder_t * dv = (internal_dv_encoder_t *)in_dv;
    if (!dv)
    {
        return -1;
    }

    /* set internal video pointers */
    avpicture_fill((AVPicture*)dv->rawFrame, p_video, dv->enc->pix_fmt,
                        dv->enc->width, dv->enc->height);

    /* compress video */
    int size = avcodec_encode_video(dv->enc, dv->compressedBuffer, dv->compressed_size, dv->rawFrame);
    if (size != dv->compressed_size)
    {
        fprintf(stderr, "error compressing video (got %d bytes, expected %d bytes)\n", size, dv->compressed_size);
        return -1;
    }

    /* write header metadata (necessary to get Apple FCP to use as DV */
    memcpy(dv->compressedBuffer, header_metadata, sizeof(header_metadata));


    /* set pointer to encoded video */
    *pp_enc_video = dv->compressedBuffer;

    return size;
}

extern int dv_encoder_close (dv_encoder_t *in_dv)
{
    internal_dv_encoder_t * dv = (internal_dv_encoder_t *)in_dv;
    if (!dv)
    {
        return -1;
    }

    cleanup(dv);

    return 0;
}