/*
 * Copyright (C) 2011 British Broadcasting Corporation.
 * All Rights Reserved
 *
 * Author: Philip de Nier
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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


#include "avci_stream_connect.h"
#include "video_conversion_10bits.h"
#include "video_conversion.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


#if !defined(HAVE_FFMPEG)

int avci_connect_accept(MediaSink* sink, const StreamInfo* streamInfo, StreamInfo* decodedStreamInfo)
{
    return 0;
}

int create_avci_connect(MediaSink* sink, int sinkStreamId, int sourceStreamId,
                        const StreamInfo* streamInfo, int numFFMPEGThreads, int useWorkerThread,
                        StreamConnect** connect)
{
    return 0;
}

int init_avci_decoder_resources()
{
    return 0;
}

void free_avci_decoder_resources()
{
}


#else

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#ifdef __cplusplus
}
#endif


/* some large number that we would not exceed */
#define MAX_STATIC_AVCI_DECODERS          32


typedef struct
{
    StreamFormat format;
    int width;
    int height;

    int inUse;

    AVCodecContext* dec;
    int isThreaded;
    int openedDecoder; /* only close if decoder opened */
    AVFrame *decFrame;
} AVCIDecoder;

typedef struct
{
    pthread_mutex_t resourceMutex;
    AVCIDecoder* decoder[MAX_STATIC_AVCI_DECODERS];
    int numDecoders;
    int numDecodersInUse;
} AVCIDecoderResource;

typedef struct
{
    int useWorkerThread;

    int sourceStreamId;
    int sinkStreamId;

    StreamConnect streamConnect;
    MediaSourceListener sourceListener;

    MediaSink* sink;

    StreamInfo streamInfo;
    StreamInfo decodedStreamInfo;

    AVCIDecoder* decoder;

    unsigned char* avciData;
    unsigned int avciDataSize;
    unsigned int allocAVCIDataSize;

    unsigned char* sinkBuffer;
    unsigned int sinkBufferSize;

    int frameWasReceived;

    pthread_t workerThreadId;
    pthread_mutex_t workerMutex;
    pthread_cond_t frameIsReadyCond;
    pthread_cond_t workerIsBusyCond;
    int frameIsReady;
    int workerIsBusy;
    int workerResult;

    int stopped; /* set when the worker thread is stopped */
} AVCIDecodeStreamConnect;


static AVCIDecoderResource g_decoderResource;
static int g_decoderResourceRefCount = 0;



static void free_avci_decoder(AVCIDecoder** decoder)
{
    int decoderResourceRefCount = g_decoderResourceRefCount;
    int i;
    int numDecoders = g_decoderResource.numDecoders;

    if (*decoder == NULL)
    {
        return;
    }

    /* check if decoder is a static resource - return to pile if it is */

    if (decoderResourceRefCount > 0)
    {
        for (i = 0; i < numDecoders; i++)
        {
            if (g_decoderResource.decoder[i] == (*decoder))
            {
                (*decoder)->inUse = 0;
                *decoder = NULL;
                return;
            }
        }
    }


    /* free the decoder */

    if ((*decoder)->isThreaded)
    {
        /* TODO: disabled this because it sometimes blocks */
        /* NOTE: this is the reason for the static resources stuff */
        /* avcodec_thread_free((*decoder)->dec); */
    }
    if ((*decoder)->openedDecoder)
    {
        avcodec_close((*decoder)->dec);
    }
    SAFE_FREE(&(*decoder)->dec);
    SAFE_FREE(&(*decoder)->decFrame);

    SAFE_FREE(decoder);
}

static int create_avci_decoder(StreamFormat format, int width, int height, int numFFMPEGThreads, AVCIDecoder** decoder)
{
    int decoderResourceRefCount = g_decoderResourceRefCount;
    int i;
    AVCIDecoder* newDecoder = NULL;
    int numDecoders = g_decoderResource.numDecoders;
    AVCodec* avDecoder = NULL;

    /* see if there is matching decoder not in use */
    if (decoderResourceRefCount > 0)
    {
        for (i = 0; i < numDecoders; i++)
        {
            if (!g_decoderResource.decoder[i]->inUse &&
                g_decoderResource.decoder[i]->format == format &&
                g_decoderResource.decoder[i]->width == width &&
                g_decoderResource.decoder[i]->height == height)
            {
                /* found one not in use */
                *decoder = g_decoderResource.decoder[i];
                g_decoderResource.decoder[i]->inUse = 1;
                return 1;
            }
        }
    }


    /* create a new one */

    CALLOC_ORET(newDecoder, AVCIDecoder, 1);

    newDecoder->inUse = 1;
    newDecoder->format = format;
    newDecoder->width = width;
    newDecoder->height = height;


    avDecoder = avcodec_find_decoder(CODEC_ID_H264);
    if (!avDecoder)
    {
        ml_log_error("Could not find the AVC-Intra decoder\n");
        goto fail;
    }

    newDecoder->dec = avcodec_alloc_context();
    if (!newDecoder->dec)
    {
        ml_log_error("Could not allocate AVC-Intra decoder context\n");
        goto fail;
    }

    if (numFFMPEGThreads > 1)
    {
        avcodec_thread_init(newDecoder->dec, numFFMPEGThreads);
        newDecoder->isThreaded = 1;
    }


    avcodec_set_dimensions(newDecoder->dec, width, height);

    newDecoder->dec->flags |= CODEC_FLAG_LOW_DELAY; /* no decode delay for intra-frame encoding */

    if (avcodec_open(newDecoder->dec, avDecoder) < 0)
    {
        ml_log_error("Could not open decoder\n");
        goto fail;
    }
    newDecoder->openedDecoder = 1;

    newDecoder->decFrame = avcodec_alloc_frame();
    if (!newDecoder->decFrame)
    {
        ml_log_error("Could not allocate decoded frame\n");
        goto fail;
    }


    /* add to static resources if they have been initialised */

    if (decoderResourceRefCount > 0)
    {
        if ((size_t)g_decoderResource.numDecoders >= sizeof(g_decoderResource.decoder) / sizeof(AVCIDecoder*))
        {
            /* more than x decoders? what are you doing? */
            ml_log_error("Number of AVC-Intra decoders exceeded hard coded limit %d\n",
                sizeof(g_decoderResource.decoder) / sizeof(AVCIDecoder));
            goto fail;
        }

        PTHREAD_MUTEX_LOCK(&g_decoderResource.resourceMutex);
        g_decoderResource.decoder[g_decoderResource.numDecoders] = newDecoder;
        g_decoderResource.numDecoders++;
        PTHREAD_MUTEX_UNLOCK(&g_decoderResource.resourceMutex);
    }

    *decoder = newDecoder;
    return 1;

fail:
    free_avci_decoder(&newDecoder);
    return 0;
}


static int decode_and_send_const(AVCIDecodeStreamConnect* connect, const unsigned char* buffer,
    unsigned int bufferSize)
{
    int got_picture;
    int result;

#if LIBAVCODEC_VERSION_MAJOR < 53
    result = avcodec_decode_video(connect->decoder->dec, connect->decoder->decFrame, &got_picture, (uint8_t*)buffer, bufferSize);
#else
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = (uint8_t*)buffer;
    packet.size = bufferSize;
    result = avcodec_decode_video2(connect->decoder->dec, connect->decoder->decFrame, &got_picture, &packet);
#endif
    if (result < 0)
    {
        ml_log_error("error decoding AVC-Intra video\n");
        return 0;
    }
    else if (!got_picture)
    {
        ml_log_error("no output from AVC-Intra decoder\n");
        return 0;
    }


    /* convert to output format */

    if (connect->decodedStreamInfo.format == YUV422_FORMAT)
    {
        ConvertFrameYUV10to8(connect->sinkBuffer,
                             (const uint16_t*)connect->decoder->decFrame->data[0],
                             (const uint16_t*)connect->decoder->decFrame->data[1],
                             (const uint16_t*)connect->decoder->decFrame->data[2],
                             connect->decoder->decFrame->linesize[0],
                             connect->decoder->decFrame->linesize[1],
                             connect->decoder->decFrame->linesize[2],
                             connect->decodedStreamInfo.width, connect->decodedStreamInfo.height,
                             2, 1);
    }
    else if (connect->decodedStreamInfo.format == YUV420_FORMAT)
    {
        ConvertFrameYUV10to8(connect->sinkBuffer,
                             (const uint16_t*)connect->decoder->decFrame->data[0],
                             (const uint16_t*)connect->decoder->decFrame->data[1],
                             (const uint16_t*)connect->decoder->decFrame->data[2],
                             connect->decoder->decFrame->linesize[0],
                             connect->decoder->decFrame->linesize[1],
                             connect->decoder->decFrame->linesize[2],
                             connect->decodedStreamInfo.width, connect->decodedStreamInfo.height,
                             2, 2);
    }
    else if (connect->decodedStreamInfo.format == YUV422_10BIT_FORMAT)
    {
        /* get rid of ffmpeg padding */
        uint16_t *outY = (uint16_t*)connect->sinkBuffer;
        uint16_t *outU = outY + connect->decodedStreamInfo.width * connect->decodedStreamInfo.height;
        uint16_t *outV = outU + connect->decodedStreamInfo.width * connect->decodedStreamInfo.height / 2;
        const uint8_t *inY = connect->decoder->decFrame->data[0];
        const uint8_t *inU = connect->decoder->decFrame->data[1];
        const uint8_t *inV = connect->decoder->decFrame->data[2];
        int y;
        for (y = 0; y < connect->decodedStreamInfo.height; y++)
        {
            memcpy((uint8_t*)outY, inY, connect->decodedStreamInfo.width * 2);
            outY += connect->decodedStreamInfo.width;
            inY += connect->decoder->decFrame->linesize[0];

            memcpy((uint8_t*)outU, inU, connect->decodedStreamInfo.width);
            outU += connect->decodedStreamInfo.width / 2;
            inU += connect->decoder->decFrame->linesize[1];

            memcpy((uint8_t*)outV, inV, connect->decodedStreamInfo.width);
            outV += connect->decodedStreamInfo.width / 2;
            inV += connect->decoder->decFrame->linesize[2];
        }
    }
    else /* connect->decodedStreamInfo.format == YUV420_10BIT_FORMAT */
    {
        /* get rid of ffmpeg padding */
        uint16_t *outY = (uint16_t*)connect->sinkBuffer;
        uint16_t *outU = outY + connect->decodedStreamInfo.width * connect->decodedStreamInfo.height;
        uint16_t *outV = outU + connect->decodedStreamInfo.width * connect->decodedStreamInfo.height / 4;
        const uint8_t *inY = connect->decoder->decFrame->data[0];
        const uint8_t *inU = connect->decoder->decFrame->data[1];
        const uint8_t *inV = connect->decoder->decFrame->data[2];
        int y;
        for (y = 0; y < connect->decodedStreamInfo.height; y++)
        {
            memcpy((uint8_t*)outY, inY, connect->decodedStreamInfo.width * 2);
            outY += connect->decodedStreamInfo.width;
            inY += connect->decoder->decFrame->linesize[0];

            if (y % 2 == 0)
            {
                memcpy((uint8_t*)outU, inU, connect->decodedStreamInfo.width);
                outU += connect->decodedStreamInfo.width / 2;
                inU += connect->decoder->decFrame->linesize[1];

                memcpy((uint8_t*)outV, inV, connect->decodedStreamInfo.width);
                outV += connect->decodedStreamInfo.width / 2;
                inV += connect->decoder->decFrame->linesize[2];
            }
        }
    }


    /* send decoded frame to sink */
    if (!msk_receive_stream_frame(connect->sink, connect->sinkStreamId, connect->sinkBuffer,
        connect->sinkBufferSize))
    {
        ml_log_error("failed to write frame to media sink\n");
        return 0;
    }

    return 1;
}

static int decode_and_send(AVCIDecodeStreamConnect* connect)
{
    return decode_and_send_const(connect, connect->avciData, connect->avciDataSize);
}

static void* worker_thread(void* arg)
{
    AVCIDecodeStreamConnect* connect = (AVCIDecodeStreamConnect*)arg;
    int status;
    int workerResult;
    int doneWaiting;

    while (!connect->stopped)
    {
        /* wait until a frame is ready */
        doneWaiting = 0;
        while (!doneWaiting && !connect->stopped)
        {
            /* wait for a frame */
            PTHREAD_MUTEX_LOCK(&connect->workerMutex);
            if (!connect->frameIsReady)
            {
                status = pthread_cond_wait(&connect->frameIsReadyCond, &connect->workerMutex);
                if (status != 0)
                {
                    ml_log_error("AVC-Intra connect worker thread failed to wait for condition\n");
                    /* TODO: don't try again? */
                }
            }

            /* done waiting if there is a frame ready for processing */
            if (connect->frameIsReady)
            {
                /* worker will now be busy */
                connect->workerIsBusy = 1;
                connect->workerResult = 0;
                connect->frameIsReady = 0;
                doneWaiting = 1;
            }
            PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
        }

        /* no more work */
        if (connect->stopped)
        {
            break;
        }

        /* decode and send frame to sink */

        workerResult = decode_and_send(connect);


        /* signal that we are done with the frame */

        PTHREAD_MUTEX_LOCK(&connect->workerMutex);
        connect->workerResult = workerResult;
        connect->workerIsBusy = 0;
        status = pthread_cond_signal(&connect->workerIsBusyCond);
        if (status != 0)
        {
            ml_log_error("AVC-Intra connect worker thread failed to send worker busy condition signal\n");
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
    }

    pthread_exit((void*) 0);
}





static MediaSourceListener* ddc_get_source_listener(void* data)
{
    AVCIDecodeStreamConnect* connect = (AVCIDecodeStreamConnect*)data;

    return &connect->sourceListener;
}

static int ddc_sync(void* data)
{
    AVCIDecodeStreamConnect* connect = (AVCIDecodeStreamConnect*)data;
    int status;
    int workerResult = 0;
    int doneWaiting;

    if (!connect->frameWasReceived)
    {
        /* no work was required */
        return 1;
    }

    /* reset for next time */
    connect->frameWasReceived = 0;

    if (!connect->useWorkerThread)
    {
        /* work is already complete */
        return 1;
    }

    /* TODO: timed wait to prevent eternal loop? */

    /* wait until worker has processed the frame and is no longer busy */
    doneWaiting = 0;
    while (!doneWaiting && !connect->stopped)
    {
        /* wait for worker */
        PTHREAD_MUTEX_LOCK(&connect->workerMutex);
        if (connect->workerIsBusy)
        {
            status = pthread_cond_wait(&connect->workerIsBusyCond, &connect->workerMutex);
            if (status != 0)
            {
                ml_log_error("AVC-Intra connect worker thread failed to wait for condition\n");
                /* TODO: don't try again? */
            }
        }

        /* worker is not busy and no frame is waiting to be processed */
        if (!connect->workerIsBusy && !connect->frameIsReady)
        {
            workerResult = connect->workerResult;
            doneWaiting = 1;
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
    }

    return workerResult;
}

static void ddc_close(void* data)
{
    AVCIDecodeStreamConnect* connect = (AVCIDecodeStreamConnect*)data;

    if (data == NULL)
    {
        return;
    }

    if (connect->useWorkerThread)
    {
        connect->stopped = 1;

        /* wake up threads - this is to avoid valgrind saying the mutx is
        still in use when pthread_mutex_destroy is called below */
        PTHREAD_MUTEX_LOCK(&connect->workerMutex);
        pthread_cond_broadcast(&connect->frameIsReadyCond);
        pthread_cond_broadcast(&connect->workerIsBusyCond);
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);

        join_thread(&connect->workerThreadId, NULL, NULL);
    }

    free_avci_decoder(&connect->decoder);
    free_avci_decoder_resources();

    SAFE_FREE(&connect->avciData);

    if (connect->useWorkerThread)
    {
        destroy_cond_var(&connect->workerIsBusyCond);
        destroy_cond_var(&connect->frameIsReadyCond);
        destroy_mutex(&connect->workerMutex);
    }


    SAFE_FREE(&connect);
}



static int ddc_accept_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    AVCIDecodeStreamConnect* connect = (AVCIDecodeStreamConnect*)data;

    connect->frameWasReceived = 0;

    return connect->sourceStreamId == streamId && msk_accept_stream_frame(connect->sink, streamId, frameInfo);
}

static int ddc_allocate_buffer(void* data, int streamId, unsigned char** buffer, unsigned int bufferSize)
{
    AVCIDecodeStreamConnect* connect = (AVCIDecodeStreamConnect*)data;
    int result;

    if (connect->sourceStreamId != streamId)
    {
        ml_log_error("Buffer allocation request for unknown source stream %d in copy connect\n", streamId);
        return 0;
    }

    if (connect->avciDataSize < bufferSize)
    {
        /* allocate buffer if neccessary and set size */
        if (connect->allocAVCIDataSize < bufferSize)
        {
            SAFE_FREE(&connect->avciData);
            connect->avciDataSize = 0;
            connect->allocAVCIDataSize = 0;

            CALLOC_ORET(connect->avciData, unsigned char,
                bufferSize + FF_INPUT_BUFFER_PADDING_SIZE /* FFMPEG for some reason needs the extra space */);
            connect->allocAVCIDataSize = bufferSize; /* we lie and don't include the FFMPEG extra space */
        }
        connect->avciDataSize = bufferSize;
    }

    /* ask sink to allocate buffer for decoded frame */
    result = msk_get_stream_buffer(connect->sink, connect->sinkStreamId, connect->sinkBufferSize,
        &connect->sinkBuffer);
    if (!result)
    {
        ml_log_error("Sink failed to allocate buffer for stream %d for AVC-Intra decoder connector\n", streamId);
        return 0;
    }

    *buffer = connect->avciData;
    return 1;
}

static void ddc_deallocate_buffer(void* data, int streamId, unsigned char** buffer)
{
    /* do nothing because buffer is reused */
}

static int ddc_receive_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    AVCIDecodeStreamConnect* connect = (AVCIDecodeStreamConnect*)data;
    int status;
    int result = 1;

    if (connect->sourceStreamId != streamId)
    {
        ml_log_error("Received frame for unknown source stream %d in copy connect\n", streamId);
        return 0;
    }

    /* signal to ddc_sync at later time that we have received a frame to decode and send */
    connect->frameWasReceived = 1;


    if (!connect->useWorkerThread)
    {
        result = decode_and_send(connect);
    }
    else
    {

        /* check that the worker isn't busy */

        PTHREAD_MUTEX_LOCK(&connect->workerMutex);
        if (connect->workerIsBusy)
        {
            ml_log_error("AVC-Intra connect worker thread is still busy, and therefore cannot receive a new frame\n");
            result = 0;
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);

        if (result != 1)
        {
            return result;
        }


        /* signal worker that a new frame is ready */

        PTHREAD_MUTEX_LOCK(&connect->workerMutex);
        connect->frameIsReady = 1;
        status = pthread_cond_signal(&connect->frameIsReadyCond);
        if (status != 0)
        {
            ml_log_error("AVC-Intra connect worker thread failed to send frame is ready condition signal\n");
            result = 0;
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
    }

    return result;
}

static int ddc_receive_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    AVCIDecodeStreamConnect* connect = (AVCIDecodeStreamConnect*)data;
    int status;
    int result;
    unsigned char* nonconstBuffer;

    if (connect->useWorkerThread)
    {
        /* the worker thread requires the data to be copied into connect->avciData */
        result = ddc_allocate_buffer(data, streamId, &nonconstBuffer, bufferSize);
        if (result)
        {
            memcpy(nonconstBuffer, buffer, bufferSize);
            result = ddc_receive_frame(data, streamId, nonconstBuffer, bufferSize);
        }
        return result;
    }


    if (connect->sourceStreamId != streamId)
    {
        ml_log_error("Received frame for unknown source stream %d in copy connect\n", streamId);
        return 0;
    }

    /* ask sink to allocate buffer for decoded frame */
    result = msk_get_stream_buffer(connect->sink, connect->sinkStreamId, connect->sinkBufferSize,
        &connect->sinkBuffer);
    if (!result)
    {
        ml_log_error("Sink failed to allocate buffer for stream %d for AVC-Intra decoder connector\n", streamId);
        return 0;
    }


    /* signal to ddc_sync at later time that we have received a frame to decode and send */
    connect->frameWasReceived = 1;


    if (!connect->useWorkerThread)
    {
        result = decode_and_send_const(connect, buffer, bufferSize);
    }
    else
    {

        /* check that the worker isn't busy */

        PTHREAD_MUTEX_LOCK(&connect->workerMutex);
        if (connect->workerIsBusy)
        {
            ml_log_error("AVC-Intra connect worker thread is still busy, and therefore cannot receive a new frame\n");
            result = 0;
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);

        if (result != 1)
        {
            return result;
        }


        /* signal worker that a new frame is ready */

        PTHREAD_MUTEX_LOCK(&connect->workerMutex);
        connect->frameIsReady = 1;
        status = pthread_cond_signal(&connect->frameIsReadyCond);
        if (status != 0)
        {
            ml_log_error("AVC-Intra connect worker thread failed to send frame is ready condition signal\n");
            result = 0;
        }
        PTHREAD_MUTEX_UNLOCK(&connect->workerMutex);
    }

    return result;
}



int avci_connect_accept(MediaSink* sink, const StreamInfo* streamInfo, StreamInfo* decodedStreamInfoOut)
{
    StreamInfo decodedStreamInfo;
    int result;

    if (streamInfo->type != PICTURE_STREAM_TYPE ||
        !(streamInfo->format == AVCI_100_FORMAT || streamInfo->format == AVCI_50_FORMAT))
    {
        return 0;
    }


    decodedStreamInfo = *streamInfo;

    /* the extra lines used for macro block alignment are skipped */
    if (decodedStreamInfo.height == 1088)
        decodedStreamInfo.height = 1080;

    if (streamInfo->format == AVCI_100_FORMAT)
    {
        decodedStreamInfo.format = YUV422_10BIT_FORMAT;
    }
    else
    {
        decodedStreamInfo.format = YUV420_10BIT_FORMAT;
        /* the FFmpeg decoder outputs 1440x1080 or 960x720, i.e. it doesn't undo the 1920->1440 or 1280->960 scaling */
        if (decodedStreamInfo.width == 1920)
            decodedStreamInfo.width = 1440;
        else if (decodedStreamInfo.width == 1280)
            decodedStreamInfo.width = 960;
        /* set aspect ratio to 4/3 to reverse the scaling from 1920->1440 and 1280->960 */
        decodedStreamInfo.aspectRatio.num = 4;
        decodedStreamInfo.aspectRatio.den = 3;
    }

    result = msk_accept_stream(sink, &decodedStreamInfo);

    if (!result)
    {
        if (streamInfo->format == AVCI_100_FORMAT)
        {
            decodedStreamInfo.format = YUV422_FORMAT;
            if (decodedStreamInfo.componentDepth > 8)
                decodedStreamInfo.componentDepth = 8;
        }
        else
        {
            decodedStreamInfo.format = YUV420_FORMAT;
            if (decodedStreamInfo.componentDepth > 8)
                decodedStreamInfo.componentDepth = 8;
        }

        result = msk_accept_stream(sink, &decodedStreamInfo);
    }

    if (result)
    {
        *decodedStreamInfoOut = decodedStreamInfo;
    }

    return result;
}

int create_avci_connect(MediaSink* sink, int sinkStreamId, int sourceStreamId,
    const StreamInfo* streamInfo, int numFFMPEGThreads, int useWorkerThread, StreamConnect** connect)
{
    AVCIDecodeStreamConnect* newConnect;
    StreamInfo decodedStreamInfo;
    int result;

    decodedStreamInfo = *streamInfo;

    /* the extra lines used for macro block alignment are skipped */
    if (decodedStreamInfo.height == 1088)
        decodedStreamInfo.height = 1080;

    if (streamInfo->format == AVCI_100_FORMAT)
    {
        decodedStreamInfo.format = YUV422_10BIT_FORMAT;
    }
    else
    {
        decodedStreamInfo.format = YUV420_10BIT_FORMAT;
        /* the FFmpeg decoder outputs 1440x1080 or 960x720, i.e. it doesn't undo the 1920->1440 or 1280->960 scaling */
        if (decodedStreamInfo.width == 1920)
            decodedStreamInfo.width = 1440;
        else if (decodedStreamInfo.width == 1280)
            decodedStreamInfo.width = 960;
        /* set aspect ratio to 4/3 to reverse the scaling from 1920->1440 and 1280->960 */
        decodedStreamInfo.aspectRatio.num = 4;
        decodedStreamInfo.aspectRatio.den = 3;
    }

    result = msk_accept_stream(sink, &decodedStreamInfo);
    if (!result)
    {
        if (streamInfo->format == AVCI_100_FORMAT)
        {
            decodedStreamInfo.format = YUV422_FORMAT;
            if (decodedStreamInfo.componentDepth > 8)
                decodedStreamInfo.componentDepth = 8;
        }
        else
        {
            decodedStreamInfo.format = YUV420_FORMAT;
            if (decodedStreamInfo.componentDepth > 8)
                decodedStreamInfo.componentDepth = 8;
        }

        result = msk_accept_stream(sink, &decodedStreamInfo);
    }

    if (!msk_register_stream(sink, sinkStreamId, &decodedStreamInfo))
    {
        /* could have failed if max streams exceeded for example */
        return 0;
    }


    CALLOC_ORET(newConnect, AVCIDecodeStreamConnect, 1);

    newConnect->useWorkerThread = useWorkerThread;
    newConnect->sink = sink;
    newConnect->sourceStreamId = sourceStreamId;
    newConnect->sinkStreamId = sinkStreamId;
    newConnect->streamInfo = *streamInfo;
    newConnect->decodedStreamInfo = decodedStreamInfo;
    if (streamInfo->format == AVCI_100_FORMAT)
    {
        newConnect->sinkBufferSize = decodedStreamInfo.width * decodedStreamInfo.height * 2;
    }
    else
    {
        newConnect->sinkBufferSize = decodedStreamInfo.width * decodedStreamInfo.height * 3 / 2;
    }
    if (decodedStreamInfo.format == YUV422_10BIT_FORMAT || decodedStreamInfo.format == YUV420_10BIT_FORMAT)
    {
        /* 16-bit coded bit depth */
        newConnect->sinkBufferSize *= 2;
    }

    newConnect->streamConnect.data = newConnect;
    newConnect->streamConnect.get_source_listener = ddc_get_source_listener;
    newConnect->streamConnect.sync = ddc_sync;
    newConnect->streamConnect.close = ddc_close;

    newConnect->sourceListener.data = newConnect;
    newConnect->sourceListener.accept_frame = ddc_accept_frame;
    newConnect->sourceListener.allocate_buffer = ddc_allocate_buffer;
    newConnect->sourceListener.deallocate_buffer = ddc_deallocate_buffer;
    newConnect->sourceListener.receive_frame = ddc_receive_frame;
    newConnect->sourceListener.receive_frame_const = ddc_receive_frame_const;



    /* create AVC-Intra decoder */

    CHK_OFAIL(init_avci_decoder_resources());

    CHK_OFAIL(create_avci_decoder(decodedStreamInfo.format, decodedStreamInfo.width, decodedStreamInfo.height,
        numFFMPEGThreads, &newConnect->decoder));


    /* create worker thread */

    if (useWorkerThread)
    {
        CHK_OFAIL(init_mutex(&newConnect->workerMutex));
        CHK_OFAIL(init_cond_var(&newConnect->frameIsReadyCond));
        CHK_OFAIL(init_cond_var(&newConnect->workerIsBusyCond));

        CHK_OFAIL(create_joinable_thread(&newConnect->workerThreadId, worker_thread, newConnect));
    }


    *connect = &newConnect->streamConnect;
    return 1;

fail:
    ddc_close(newConnect);
    return 0;
}



int init_avci_decoder_resources()
{
    if (g_decoderResourceRefCount == 0)
    {
        av_register_all();

        memset(&g_decoderResource, 0, sizeof(AVCIDecoderResource));
        CHK_ORET(init_mutex(&g_decoderResource.resourceMutex));
        g_decoderResourceRefCount = 1;
    }
    else
    {
        g_decoderResourceRefCount++;
    }

    return 1;
}

void free_avci_decoder_resources()
{
    int i;
    AVCIDecoder* decoder = NULL;
    int refCount;

    if (g_decoderResourceRefCount <= 0)
    {
        return;
    }

    PTHREAD_MUTEX_LOCK(&g_decoderResource.resourceMutex);
    g_decoderResourceRefCount--;
    refCount = g_decoderResourceRefCount;
    PTHREAD_MUTEX_UNLOCK(&g_decoderResource.resourceMutex);

    if (refCount == 0)
    {
        if (g_decoderResource.numDecodersInUse > 0)
        {
            ml_log_warn("There are %d AVC-Intra decoder resources still in use - please fix the source code\n",
                g_decoderResource.numDecodersInUse);
        }

        for (i = 0; i < g_decoderResource.numDecoders; i++)
        {
            /* set decoder NULL in list so that free doesn't just change it to !inUse */
            decoder = g_decoderResource.decoder[i];
            g_decoderResource.decoder[i] = NULL;

            free_avci_decoder(&decoder);
        }

        destroy_mutex(&g_decoderResource.resourceMutex);

        memset(&g_decoderResource, 0, sizeof(AVCIDecoderResource));
    }
}


#endif /* HAVE_FFMPEG */

