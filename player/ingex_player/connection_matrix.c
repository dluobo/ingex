/*
 * $Id: connection_matrix.c,v 1.6 2011/09/27 10:14:29 philipn Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "connection_matrix.h"
#include "stream_connect.h"
#include "dv_stream_connect.h"
#include "mpegi_stream_connect.h"
#include "mjpeg_stream_connect.h"
#include "dnxhd_stream_connect.h"
#include "avci_stream_connect.h"
#include "logging.h"
#include "macros.h"


typedef struct
{
    int sourceStreamId;
    int sinkStreamId;
    StreamConnect* connect;
} ConnectionMatrixEntry;

struct ConnectionMatrix
{
    ConnectionMatrixEntry* entries;
    int numStreams;
    MediaSourceListener sourceListener;
};


static int stm_accept_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    ConnectionMatrix* matrix = (ConnectionMatrix*)data;
    int i;

    for (i = 0; i < matrix->numStreams; i++)
    {
        if (matrix->entries[i].connect != NULL && matrix->entries[i].sourceStreamId == streamId)
        {
            return sdl_accept_frame(stc_get_source_listener(matrix->entries[i].connect), streamId, frameInfo);
        }
    }
    return 0;
}

static int stm_allocate_buffer(void* data, int streamId, unsigned char** buffer, unsigned int bufferSize)
{
    ConnectionMatrix* matrix = (ConnectionMatrix*)data;
    int i;

    for (i = 0; i < matrix->numStreams; i++)
    {
        if (matrix->entries[i].connect != NULL && matrix->entries[i].sourceStreamId == streamId)
        {
            return sdl_allocate_buffer(stc_get_source_listener(matrix->entries[i].connect), streamId,
                buffer, bufferSize);
        }
    }
    return 0;
}

static void stm_deallocate_buffer(void* data, int streamId, unsigned char** buffer)
{
    ConnectionMatrix* matrix = (ConnectionMatrix*)data;
    int i;

    for (i = 0; i < matrix->numStreams; i++)
    {
        if (matrix->entries[i].connect != NULL && matrix->entries[i].sourceStreamId == streamId)
        {
            sdl_deallocate_buffer(stc_get_source_listener(matrix->entries[i].connect), streamId, buffer);
            return;
        }
    }
}

static int stm_receive_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    ConnectionMatrix* matrix = (ConnectionMatrix*)data;
    int i;

    for (i = 0; i < matrix->numStreams; i++)
    {
        if (matrix->entries[i].connect != NULL && matrix->entries[i].sourceStreamId == streamId)
        {
            return sdl_receive_frame(stc_get_source_listener(matrix->entries[i].connect), streamId,
                buffer, bufferSize);
        }
    }
    return 0;
}

static int stm_receive_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    ConnectionMatrix* matrix = (ConnectionMatrix*)data;
    int i;

    for (i = 0; i < matrix->numStreams; i++)
    {
        if (matrix->entries[i].connect != NULL && matrix->entries[i].sourceStreamId == streamId)
        {
            return sdl_receive_frame_const(stc_get_source_listener(matrix->entries[i].connect), streamId,
                buffer, bufferSize);
        }
    }
    return 0;
}

int stm_create_connection_matrix(MediaSource* source, MediaSink* sink, int numFFMPEGThreads,
    int useWorkerThreads, ConnectionMatrix** matrix)
{
    ConnectionMatrix* newMatrix;
    int numSourceStreams;
    int i;
    const StreamInfo* streamInfo;
    StreamInfo decodedStreamInfo;
    int streamIndex;

    CALLOC_ORET(newMatrix, ConnectionMatrix, 1);

    numSourceStreams = msc_get_num_streams(source);


    /* complete blank video sources using the first or default video stream info */
    for (i = 0; i < numSourceStreams; i++)
    {
        if (msc_stream_is_disabled(source, i))
        {
            continue;
        }

        CHK_OFAIL(msc_get_stream_info(source, i, &streamInfo));
        if (streamInfo->type == PICTURE_STREAM_TYPE &&
            streamInfo->format != UNKNOWN_FORMAT)
        {
            if (pass_through_accept(sink, streamInfo, &decodedStreamInfo) ||
                dv_connect_accept(sink, streamInfo, &decodedStreamInfo) ||
                mpegi_connect_accept(sink, streamInfo, &decodedStreamInfo) ||
                mjpeg_connect_accept(sink, streamInfo, &decodedStreamInfo) ||
                dnxhd_connect_accept(sink, streamInfo, &decodedStreamInfo))
            {
                break;
            }
        }
    }
    if (i >= numSourceStreams)
    {
        /* use default settings if no video source was accepted */
        decodedStreamInfo.type = PICTURE_STREAM_TYPE;
        decodedStreamInfo.format = UYVY_FORMAT;
        decodedStreamInfo.width = 720;
        decodedStreamInfo.height = 576;
        decodedStreamInfo.frameRate = g_palFrameRate;
        decodedStreamInfo.aspectRatio.num = 4;
        decodedStreamInfo.aspectRatio.den = 3;
    }
    CHK_OFAIL(msc_finalise_blank_source(source, &decodedStreamInfo));


    /* count the number of media source streams that can be connected */
    for (i = 0; i < numSourceStreams; i++)
    {
        if (msc_stream_is_disabled(source, i))
        {
            /* streams that are disabled before connection are not connected */
            continue;
        }

        CHK_OFAIL(msc_get_stream_info(source, i, &streamInfo));
        if (pass_through_accept(sink, streamInfo, &decodedStreamInfo))
        {
            newMatrix->numStreams++;
        }
        else if (dv_connect_accept(sink, streamInfo, &decodedStreamInfo))
        {
            newMatrix->numStreams++;
        }
        else if (mpegi_connect_accept(sink, streamInfo, &decodedStreamInfo))
        {
            newMatrix->numStreams++;
        }
        else if (mjpeg_connect_accept(sink, streamInfo, &decodedStreamInfo))
        {
            newMatrix->numStreams++;
        }
        else if (dnxhd_connect_accept(sink, streamInfo, &decodedStreamInfo))
        {
            newMatrix->numStreams++;
        }
        else if (avci_connect_accept(sink, streamInfo, &decodedStreamInfo))
        {
            newMatrix->numStreams++;
        }
        else
        {
            ml_log_info("Failed to find stream connector for stream %d (%s, %s)\n",
                i, get_stream_type_string(streamInfo->type), get_stream_format_string(streamInfo->format));
        }
    }


    newMatrix->sourceListener.data = newMatrix;
    newMatrix->sourceListener.accept_frame = stm_accept_frame;
    newMatrix->sourceListener.allocate_buffer = stm_allocate_buffer;
    newMatrix->sourceListener.deallocate_buffer = stm_deallocate_buffer;
    newMatrix->sourceListener.receive_frame = stm_receive_frame;
    newMatrix->sourceListener.receive_frame_const = stm_receive_frame_const;

    if (newMatrix->numStreams > 0)
    {
        CALLOC_OFAIL(newMatrix->entries, ConnectionMatrixEntry, newMatrix->numStreams);

        /* create stream connects */
        streamIndex = 0;
        for (i = 0; i < numSourceStreams; i++)
        {
            if (msc_stream_is_disabled(source, i))
            {
                /* streams that are disabled before connection are not connected */
                continue;
            }

            CHK_OFAIL(msc_get_stream_info(source, i, &streamInfo));
            if (pass_through_accept(sink, streamInfo, &decodedStreamInfo))
            {
                if (create_pass_through_connect(sink, i, i, streamInfo, &newMatrix->entries[streamIndex].connect))
                {
                    newMatrix->entries[streamIndex].sourceStreamId = i;
                    newMatrix->entries[streamIndex].sinkStreamId = i;
                }
                /* failure is possible (eg. max streams exceeded) and so we ignore the stream */
                else
                {
                    ml_log_info("Failed to pass through connect stream %d (%s, %s)\n",
                        i, get_stream_type_string(streamInfo->type), get_stream_format_string(streamInfo->format));
                    CHK_OFAIL(msc_disable_stream(source, i));
                }

                streamIndex++;
            }
            else if (dv_connect_accept(sink, streamInfo, &decodedStreamInfo))
            {
                if (create_dv_connect(sink, i, i, streamInfo, numFFMPEGThreads, useWorkerThreads,
                        &newMatrix->entries[streamIndex].connect))
                {
                    newMatrix->entries[streamIndex].sourceStreamId = i;
                    newMatrix->entries[streamIndex].sinkStreamId = i;
                }
                /* failure is possible (eg. max streams exceeded) and so we ignore the stream */
                else
                {
                    ml_log_info("Failed to dv connect stream %d (%s, %s)\n",
                        i, get_stream_type_string(streamInfo->type), get_stream_format_string(streamInfo->format));
                    CHK_OFAIL(msc_disable_stream(source, i));
                }

                streamIndex++;
            }
            else if (mpegi_connect_accept(sink, streamInfo, &decodedStreamInfo))
            {
                if (create_mpegi_connect(sink, i, i, streamInfo, numFFMPEGThreads, useWorkerThreads,
                        &newMatrix->entries[streamIndex].connect))
                {
                    newMatrix->entries[streamIndex].sourceStreamId = i;
                    newMatrix->entries[streamIndex].sinkStreamId = i;
                }
                /* failure is possible (eg. max streams exceeded) and so we ignore the stream */
                else
                {
                    ml_log_info("Failed to MPEG I-frame only connect stream %d (%s, %s)\n",
                        i, get_stream_type_string(streamInfo->type), get_stream_format_string(streamInfo->format));
                    CHK_OFAIL(msc_disable_stream(source, i));
                }

                streamIndex++;
            }
            else if (mjpeg_connect_accept(sink, streamInfo, &decodedStreamInfo))
            {
                if (create_mjpeg_connect(sink, i, i, streamInfo, numFFMPEGThreads, useWorkerThreads,
                        &newMatrix->entries[streamIndex].connect))
                {
                    newMatrix->entries[streamIndex].sourceStreamId = i;
                    newMatrix->entries[streamIndex].sinkStreamId = i;
                }
                /* failure is possible (eg. max streams exceeded) and so we ignore the stream */
                else
                {
                    ml_log_info("Failed to MJPEG connect stream %d (%s, %s)\n",
                        i, get_stream_type_string(streamInfo->type), get_stream_format_string(streamInfo->format));
                    CHK_OFAIL(msc_disable_stream(source, i));
                }

                streamIndex++;
            }
            else if (dnxhd_connect_accept(sink, streamInfo, &decodedStreamInfo))
            {
                if (create_dnxhd_connect(sink, i, i, streamInfo, numFFMPEGThreads, useWorkerThreads,
                        &newMatrix->entries[streamIndex].connect))
                {
                    newMatrix->entries[streamIndex].sourceStreamId = i;
                    newMatrix->entries[streamIndex].sinkStreamId = i;
                }
                /* failure is possible (eg. max streams exceeded) and so we ignore the stream */
                else
                {
                    ml_log_info("Failed to DNxHD connect stream %d (%s, %s)\n",
                        i, get_stream_type_string(streamInfo->type), get_stream_format_string(streamInfo->format));
                    CHK_OFAIL(msc_disable_stream(source, i));
                }

                streamIndex++;
            }
            else if (avci_connect_accept(sink, streamInfo, &decodedStreamInfo))
            {
                if (create_avci_connect(sink, i, i, streamInfo, numFFMPEGThreads, useWorkerThreads,
                        &newMatrix->entries[streamIndex].connect))
                {
                    newMatrix->entries[streamIndex].sourceStreamId = i;
                    newMatrix->entries[streamIndex].sinkStreamId = i;
                }
                /* failure is possible (eg. max streams exceeded) and so we ignore the stream */
                else
                {
                    ml_log_info("Failed to AVC-Intra connect stream %d (%s, %s)\n",
                        i, get_stream_type_string(streamInfo->type), get_stream_format_string(streamInfo->format));
                    CHK_OFAIL(msc_disable_stream(source, i));
                }

                streamIndex++;
            }
            else
            {
                CHK_OFAIL(msc_disable_stream(source, i));
            }
        }
    }

    *matrix = newMatrix;
    return 1;

fail:
    stm_close(&newMatrix);
    return 0;
}

MediaSourceListener* stm_get_stream_listener(ConnectionMatrix* matrix)
{
    return &matrix->sourceListener;
}

int stm_sync(ConnectionMatrix* matrix)
{
    int i;
    int result = 1;

    for (i = 0; i < matrix->numStreams; i++)
    {
        result = stc_sync(matrix->entries[i].connect) && result;
    }

    return result;
}

void stm_close(ConnectionMatrix** matrix)
{
    int i;

    if (*matrix == NULL)
    {
        return;
    }

    for (i = 0; i < (*matrix)->numStreams; i++)
    {
        stc_close((*matrix)->entries[i].connect);
    }

    SAFE_FREE(&(*matrix)->entries);

    SAFE_FREE(matrix);
}



