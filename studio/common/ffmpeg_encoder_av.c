/*
 * $Id: ffmpeg_encoder_av.c,v 1.1 2007/09/11 14:08:36 stuart_hc Exp $
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

#include <ffmpeg/avformat.h>
#include <ffmpeg/avcodec.h>
#include <string.h>
#include <stdlib.h>

#include "ffmpeg_encoder_av.h"

#define MPA_FRAME_SIZE 1152 // taken from ffmpeg's private mpegaudio.h


typedef struct 
{
    AVFormatContext * oc; ///< output format context
    AVStream * audio_st; ///< audio stream
    int16_t * audio_inbuf;
    int audio_inbuf_size;
    int audio_inbuf_offset;
    uint8_t * audio_outbuf;
    int audio_outbuf_size;
    AVStream * video_st; ///< video stream
    AVFrame * inputFrame;
    uint8_t * video_outbuf;
    int video_outbuf_size;
} internal_ffmpeg_encoder_t;



/* initialise video stream for DVD encoding */
static int init_video_dvd(internal_ffmpeg_encoder_t * enc)
{
    AVCodecContext * codec_context = enc->video_st->codec;

    /* We use the default codec for DVD format (mpeg2video) */
    const int codec_id = enc->oc->oformat->video_codec;

    const int dvd_kbit_rate = 5000;

    enc->video_st->r_frame_rate.num = 25;
    enc->video_st->r_frame_rate.den = 1;

    codec_context->codec_id = codec_id;
    codec_context->codec_type = CODEC_TYPE_VIDEO;

    /* put dvd parameters */
    codec_context->bit_rate = dvd_kbit_rate * 1000;
    codec_context->rc_max_rate = 9000000;
    codec_context->rc_min_rate = 0;
    codec_context->rc_buffer_size = 224*1024*8;

    /* resolution must be a multiple of two */
    codec_context->width = 720;  
    codec_context->height = 576;
    codec_context->sample_aspect_ratio = av_d2q(16.0/9*576/720, 255);

    /* time base: this is the fundamental unit of time (in seconds) in terms
       of which frame timestamps are represented. for fixed-fps content,
       timebase should be 1/framerate and timestamp increments should be
       identically 1. */
    codec_context->time_base.den = 25;  
    codec_context->time_base.num = 1;

    codec_context->gop_size = 15; /* emit one intra frame every 15 frames at most */
    codec_context->pix_fmt = PIX_FMT_YUV420P;

    if (codec_context->codec_id == CODEC_ID_MPEG2VIDEO)
    {
        /* just for testing, we also add B frames */
        codec_context->max_b_frames = 2;
    }
    if (codec_context->codec_id == CODEC_ID_MPEG1VIDEO)
    {
        /* needed to avoid using macroblocks in which some coeffs overflow 
           this doesnt happen with normal video, it just happens here as the 
           motion of the chroma plane doesnt match the luma plane */
        codec_context->mb_decision = 2;
    }

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
    enc->inputFrame->top_field_first = 1;

    /* allocate output buffer */
    enc->video_outbuf = NULL;
    if (!(enc->oc->oformat->flags & AVFMT_RAWPICTURE))
    {
        enc->video_outbuf_size = 550000;
        enc->video_outbuf = malloc(enc->video_outbuf_size);
    }

    return 0;
}

/* initialise video stream for MOV encoding */
static int init_video_mov(internal_ffmpeg_encoder_t * enc)
{
    AVCodecContext * codec_context = enc->video_st->codec;

    /* We use the default codec for MOV format (mpeg4) */
    const int codec_id = enc->oc->oformat->video_codec;

    /* bit rate */
    const int kbit_rate = 800;

    enc->video_st->r_frame_rate.num = 25;
    enc->video_st->r_frame_rate.den = 1;

    codec_context->codec_id = codec_id;
    codec_context->codec_type = CODEC_TYPE_VIDEO;

    /* set coding parameters */
    codec_context->bit_rate = kbit_rate * 1000;

    /* resolution must be a multiple of two */
    codec_context->width = 720;  
    codec_context->height = 576;
    codec_context->sample_aspect_ratio = av_d2q(16.0/9*576/720, 255);

    /* time base: this is the fundamental unit of time (in seconds) in terms
       of which frame timestamps are represented. for fixed-fps content,
       timebase should be 1/framerate and timestamp increments should be
       identically 1. */
    codec_context->time_base.num = 1;
    codec_context->time_base.den = 25;  

    codec_context->pix_fmt = PIX_FMT_YUV420P;

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
    enc->inputFrame->top_field_first = 1;

    /* allocate output buffer */
    enc->video_outbuf = NULL;
    if (!(enc->oc->oformat->flags & AVFMT_RAWPICTURE))
    {
        // not sure what size is appropriate for mp4
        enc->video_outbuf_size = 550000;
        enc->video_outbuf = malloc(enc->video_outbuf_size);
    }

    return 0;
}

static void close_video(internal_ffmpeg_encoder_t * enc)
{
    AVStream * st = enc->video_st;

    avcodec_close(st->codec);

    av_free(enc->inputFrame);
    av_free(enc->video_outbuf);
}

/* initialise audio stream for DVD encoding */
static int init_audio_dvd(internal_ffmpeg_encoder_t * enc)
{
    AVCodecContext * codec_context = enc->audio_st->codec;

    /* We use the default codec for DVD format (mp2) */
    const int codec_id = enc->oc->oformat->audio_codec;

    codec_context->codec_id = codec_id;
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

    /* input and output buffers */
    enc->audio_inbuf_size = 20000;
    enc->audio_inbuf = malloc(enc->audio_inbuf_size);

    enc->audio_outbuf_size = 15000;
    enc->audio_outbuf = malloc(enc->audio_outbuf_size);

    return 0;
}

/* initialise audio stream for MOV encoding */
static int init_audio_mov(internal_ffmpeg_encoder_t * enc)
{
    AVCodecContext * codec_context = enc->audio_st->codec;

    /* We use mp3 codec rather than default */
    const int codec_id = CODEC_ID_MP3;
    const int kbit_rate = 96;

    codec_context->codec_id = codec_id;
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
    codec_context->bit_rate = kbit_rate * 1000;
    codec_context->sample_rate = 48000;
    codec_context->channels = 2;

    /* open it */
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open audio codec\n");
        return -1;
    }

    /* input and output buffers */
    enc->audio_inbuf_size = 20000;
    enc->audio_inbuf = malloc(enc->audio_inbuf_size);

    enc->audio_outbuf_size = 15000;
    enc->audio_outbuf = malloc(enc->audio_outbuf_size);

    return 0;
}

static void close_audio(internal_ffmpeg_encoder_t * enc)
{
    AVStream *st = enc->audio_st;
    avcodec_close(st->codec);
    
    av_free(enc->audio_outbuf);
    av_free(enc->audio_inbuf);
}

static void cleanup (internal_ffmpeg_encoder_t * enc)
{
    if (enc)
    {
        if (enc->video_st)
        {
            close_video(enc);
        }
        if (enc->audio_st)
        {
            close_audio(enc);
        }

        if (enc->oc)
        {
            unsigned int i;
            //free the streams
            for (i = 0; i < enc->oc->nb_streams; i++)
            {
                av_freep(&enc->oc->streams[i]);
            }
            av_free(enc->oc);
        }

        av_free (enc);
        enc = NULL;
    }
}


static int write_video_frame(internal_ffmpeg_encoder_t * enc, uint8_t * p_video)
{
    AVFormatContext * oc = enc->oc;
    AVStream * st = enc->video_st;
    AVCodecContext * c = enc->video_st->codec;
    int out_size;
    int ret;
    
    /* set pointers in inputFrame to point to planes in p_video */
    avpicture_fill((AVPicture*)enc->inputFrame, p_video,
        c->pix_fmt,
        c->width, c->height);

    /* encode the image */
    out_size = avcodec_encode_video(c, enc->video_outbuf, enc->video_outbuf_size, enc->inputFrame);

    if (out_size > 0)
    {
        AVPacket pkt;
        av_init_packet(&pkt);
            
        pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);
        if (c->coded_frame->key_frame)
        {
            pkt.flags |= PKT_FLAG_KEY;
        }
        pkt.stream_index = st->index;
        pkt.data = enc->video_outbuf;
        pkt.size = out_size;
        
        /* write the compressed frame to the media file */
        ret = av_write_frame(oc, &pkt);
    }
    else
    {
        // out_size == 0 means the image was buffered
        ret = 0;
    }

    if (ret != 0)
    {
        fprintf(stderr, "Error while writing video frame\n");
        return -1;
    }
    else
    {
        //enc->frame_count++;
        return 0;
    }
}

static int write_audio_frame(internal_ffmpeg_encoder_t *enc, int16_t *p_audio)
{
    AVFormatContext * oc = enc->oc;
    AVStream * st = enc->audio_st;
    AVCodecContext * c;
    AVPacket pkt;
    av_init_packet(&pkt);
    
    c = st->codec;

    pkt.size = avcodec_encode_audio(c, enc->audio_outbuf, enc->audio_outbuf_size, p_audio);

    pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);
    pkt.flags |= PKT_FLAG_KEY;
    pkt.stream_index = st->index;
    pkt.data = enc->audio_outbuf;

    /* write the compressed frame to the media file */
    if (av_write_frame(oc, &pkt) != 0)
    {
        fprintf(stderr, "Error while writing audio frame\n");
        return -1;
    }
    return 0;
}



extern ffmpeg_encoder_av_t * ffmpeg_encoder_av_init (const char * filename, ffmpeg_encoder_av_resolution_t res)
{
    internal_ffmpeg_encoder_t * enc;
    AVOutputFormat * fmt;
    const char * fmt_name = 0;
    const char * extn = 0;

    switch (res)
    {
    case FF_ENCODER_RESOLUTION_DVD:
        fmt_name = "dvd";
        extn = "mpg";
        break;
    case FF_ENCODER_RESOLUTION_MOV:
        fmt_name = "mov";
        extn = "mov";
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

    /* Allocate encoder object and set all members to zero */
    enc = av_mallocz (sizeof(internal_ffmpeg_encoder_t));
    if (!enc)
    {
        fprintf (stderr, "Could not allocate encoder object\n");
        return NULL;
    }

    /* Allocate the output media context */
    enc->oc = av_alloc_format_context();
    if (!enc->oc)
    {
        cleanup(enc);
        fprintf (stderr, "Could not allocate output format context\n");
        return NULL;
    }
    enc->oc->oformat = fmt;

    /* Set output filename */
    snprintf (enc->oc->filename, sizeof(enc->oc->filename), "%s.%s", filename, extn);

    /* Set parameters */
    switch (res)
    {
    case FF_ENCODER_RESOLUTION_DVD:
        enc->oc->packet_size = 2048;
        enc->oc->mux_rate = 10080000;
        break;
    default:
        break;
    }

    
    /* 
    * add the audio and videostreams and
    * initialise the codecs
    */
    enc->video_st = av_new_stream(enc->oc, 0);
    if (!enc->video_st)
    {
        fprintf(stderr, "Could not alloc video stream\n");
        cleanup(enc);
        return NULL;
    }
    enc->audio_st = av_new_stream(enc->oc, 1);
    if (!enc->audio_st)
    {
        fprintf(stderr, "Could not alloc audio stream\n");
        cleanup(enc);
        return NULL;
    }
    switch (res)
    {
    case FF_ENCODER_RESOLUTION_DVD:
        init_video_dvd(enc);
        init_audio_dvd(enc);
        break;
    case FF_ENCODER_RESOLUTION_MOV:
        init_video_mov(enc);
        init_audio_mov(enc);
        break;
    default:
        break;
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

    /*
    *   open the output file
    */
    if (!(fmt->flags & AVFMT_NOFILE))
    {
        if (url_fopen(&enc->oc->pb, enc->oc->filename, URL_WRONLY) < 0)
        {
            cleanup(enc);
            fprintf(stderr, "Could not open '%s'\n", filename);
            return NULL;
        }
    }

    /*
    * write stream header if any 
    */
    av_write_header(enc->oc);

    return (ffmpeg_encoder_av_t *)enc;
}

extern int ffmpeg_encoder_av_encode (ffmpeg_encoder_av_t * in_enc, uint8_t * p_video, int16_t * p_audio)
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

    /* encode and write audio */
    if (p_audio)
    {
        /*
        Audio comes in in video frames (1920 samples) but is encoded
        as MPEG frames (1152 samples) so we need to buffer.
        NB. audio_inbuf_offset is in terms of 16-bit samples, hence the
        rather confusing pointer arithmetic.
        */
        memcpy (enc->audio_inbuf + enc->audio_inbuf_offset, p_audio, 1920*2*2);
        enc->audio_inbuf_offset += (1920*2);
        while (enc->audio_inbuf_offset >= (MPA_FRAME_SIZE*4))
        {
            if (write_audio_frame (enc, enc->audio_inbuf) == -1)
            {
                return -1;
            }
            enc->audio_inbuf_offset -= (MPA_FRAME_SIZE*2);
            memmove (enc->audio_inbuf, enc->audio_inbuf+(MPA_FRAME_SIZE*2), enc->audio_inbuf_offset*2);
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
        close_video(enc);
        enc->video_st = NULL;
    }
    if (enc->audio_st)
    {
        close_audio(enc);
        enc->audio_st = NULL;
    }

    /* write the trailer, if any */
    av_write_trailer(enc->oc);

    if (!(enc->oc->oformat->flags & AVFMT_NOFILE))
    {
        /* close the output file */
        url_fclose(&enc->oc->pb);
    }
    cleanup (enc);
    return 0;
}
