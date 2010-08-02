/*
 * $Id: vitc_reader_sink_source.c,v 1.3 2010/08/02 16:45:03 john_f Exp $
 *
 * Copyright (C) 2010 British Broadcasting Corporation, All Rights Reserved
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
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <video_VITC.h>

#include "vitc_reader_sink_source.h"
#include "logging.h"
#include "utils.h"
#include "macros.h"



struct VITCReaderSinkSource
{
    unsigned int *vitcLines;
    int numVITCLines;
    
    MediaSource source;
    StreamInfo sourceStreamInfo;
    int isDisabled;
    int64_t position;
    int64_t length;
    int stopped;

    MediaSink *targetSink;
    MediaSink sink;
    int videoStreamId;
    StreamInfo videoStreamInfo;
    
    pthread_mutex_t timecodeMutex;
    pthread_cond_t timecodeReadyCond;
    Timecode timecode;
    int timecodeReady;
    int haveDecodedTimecode;
};



static void decode_vitc(VITCReaderSinkSource *sink, const unsigned char *buffer, unsigned int bufferSize)
{
    int is_uyvy = (sink->videoStreamInfo.format == UYVY_FORMAT);
    int vbi_height = sink->videoStreamInfo.height - 576;
    assert(vbi_height > 0);
    const unsigned char *line;
    Timecode timecode = {0, 0, 0, 0, 0};
    int have_decoded_timecode = 0;
    int i;
    for (i = 0; i < sink->numVITCLines; i++) {
        if ((vbi_height - (int)((23 - sink->vitcLines[i]) * 2)) < 0)
            continue;

        if (is_uyvy)
            line = buffer + (vbi_height - ((23 - sink->vitcLines[i]) * 2)) * sink->videoStreamInfo.width * 2;
        else
            line = buffer + (vbi_height - ((23 - sink->vitcLines[i]) * 2)) * sink->videoStreamInfo.width;

        if (readVITC(line, is_uyvy, &timecode.hour, &timecode.min, &timecode.sec, &timecode.frame))
            break;
    }
    have_decoded_timecode = (i < sink->numVITCLines);

    /* signal to source that timecode is ready */
    PTHREAD_MUTEX_LOCK(&sink->timecodeMutex);
    sink->timecode = timecode;
    sink->timecodeReady = 1;
    sink->haveDecodedTimecode = have_decoded_timecode;
    pthread_cond_broadcast(&sink->timecodeReadyCond);
    PTHREAD_MUTEX_UNLOCK(&sink->timecodeMutex);
}



static int vss_get_num_streams(void *data)
{
    return 1;
}

static int vss_get_stream_info(void *data, int streamIndex, const StreamInfo **streamInfo)
{
    VITCReaderSinkSource *source = (VITCReaderSinkSource*)data;

    if (streamIndex < 0 || streamIndex >= 1)
        return 0;

    *streamInfo = &source->sourceStreamInfo;
    return 1;
}

static void vss_set_frame_rate_or_disable(void *data, const Rational *frameRate)
{
    VITCReaderSinkSource *source = (VITCReaderSinkSource*)data;

    source->sourceStreamInfo.frameRate = *frameRate;

    /* we use this function to reset the stopped flag */
    PTHREAD_MUTEX_LOCK(&source->timecodeMutex);
    source->stopped = 0;
    PTHREAD_MUTEX_UNLOCK(&source->timecodeMutex);
}

static int vss_disable_stream(void *data, int streamIndex)
{
    VITCReaderSinkSource *source = (VITCReaderSinkSource*)data;

    if (streamIndex != 0)
        return 0;

    source->isDisabled = 1;
    return 1;
}

static int vss_stream_is_disabled(void *data, int streamIndex)
{
    VITCReaderSinkSource *source = (VITCReaderSinkSource*)data;

    if (streamIndex != 0)
        return 0;

    return source->isDisabled;
}

static int vss_read_frame(void *data, const FrameInfo *frameInfo, MediaSourceListener *listener)
{
    VITCReaderSinkSource *source = (VITCReaderSinkSource*)data;
    int result = 0;
    unsigned char *buffer;

    if (source->isDisabled)
        return 0;

    if (source->videoStreamId >= 0) {
        PTHREAD_MUTEX_LOCK(&source->timecodeMutex);
        while (!source->timecodeReady && !source->stopped) {
            if (pthread_cond_wait(&source->timecodeReadyCond, &source->timecodeMutex) != 0) {
                ml_log_error("VITC reader read_frame failed to wait for condition: %s\n", strerror(errno));
                /* TODO: break here? */
            }
        }
        if (!source->stopped) {
            if (sdl_accept_frame(listener, 0, frameInfo)) {
                if (!sdl_allocate_buffer(listener, 0, &buffer, sizeof(Timecode))) {
                    result = -1;
                } else {
                    if (source->haveDecodedTimecode)
                        memcpy(buffer, &source->timecode, sizeof(Timecode));
                    else
                        memset(buffer, 0, sizeof(Timecode));
                    sdl_receive_frame(listener, 0, buffer, sizeof(Timecode));
                }
            }
        }
        PTHREAD_MUTEX_UNLOCK(&source->timecodeMutex);
    }

    if (result == 0)
        source->position++;

    return result;
}

static int vss_is_seekable(void *data)
{
    return 1;
}

static int vss_seek(void *data, int64_t position)
{
    VITCReaderSinkSource *source = (VITCReaderSinkSource*)data;

    if (source->isDisabled)
        return 0;

    source->position = position;
    return 0;
}

static int vss_get_length(void *data, int64_t* length)
{
    VITCReaderSinkSource *source = (VITCReaderSinkSource*)data;

    *length = source->length;
    return 1;
}

static int vss_get_position(void *data, int64_t* position)
{
    VITCReaderSinkSource *source = (VITCReaderSinkSource*)data;

    if (source->isDisabled)
        return 0;

    *position = source->position;
    return 1;
}

static int vss_get_available_length(void *data, int64_t* length)
{
    VITCReaderSinkSource *source = (VITCReaderSinkSource*)data;

    *length = source->length;
    return 1;
}

static int vss_eof(void *data)
{
    VITCReaderSinkSource *source = (VITCReaderSinkSource*)data;

    if (source->isDisabled)
        return 0;

    return source->position >= source->length;
}

static void vss_set_source_name(void *data, const char* name)
{
    VITCReaderSinkSource *source = (VITCReaderSinkSource*)data;

    add_known_source_info(&source->sourceStreamInfo, SRC_INFO_NAME, name);
}

static void vss_set_clip_id(void *data, const char* id)
{
    VITCReaderSinkSource *source = (VITCReaderSinkSource*)data;

    set_stream_clip_id(&source->sourceStreamInfo, id);
}

static void vss_source_close(void *data)
{
    VITCReaderSinkSource *source = (VITCReaderSinkSource*)data;

    if (data == NULL)
        return;
    
    PTHREAD_MUTEX_LOCK(&source->timecodeMutex);
    source->stopped = 1;
    pthread_cond_broadcast(&source->timecodeReadyCond);
    PTHREAD_MUTEX_UNLOCK(&source->timecodeMutex);

    clear_stream_info(&source->sourceStreamInfo);
    
    source->isDisabled = 0;
    source->position = 0;

    // free in sink only
}



static int vss_register_listener(void *data, MediaSinkListener *listener)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;

    return msk_register_listener(sink->targetSink, listener);
}

static void vss_unregister_listener(void *data, MediaSinkListener *listener)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;

    msk_unregister_listener(sink->targetSink, listener);
}

static int vss_accept_stream(void *data, const StreamInfo *streamInfo)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;

    return msk_accept_stream(sink->targetSink, streamInfo);
}

static int vss_register_stream(void *data, int streamId, const StreamInfo *streamInfo)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;
    
    if (msk_register_stream(sink->targetSink, streamId, streamInfo)) {
        if (sink->videoStreamId < 0 &&
            streamInfo->type == PICTURE_STREAM_TYPE &&
            (streamInfo->format == UYVY_FORMAT ||
                streamInfo->format == YUV422_FORMAT ||
                streamInfo->format == YUV420_FORMAT ||
                streamInfo->format == YUV411_FORMAT ||
                streamInfo->format == YUV444_FORMAT) &&
            streamInfo->width == 720 && (streamInfo->height > 576 && streamInfo->height <= 608))
        {
            sink->videoStreamId = streamId;
            sink->videoStreamInfo = *streamInfo;
        }
        
        return 1;
    }
    
    return 0;
}

static int vss_accept_stream_frame(void *data, int streamId, const FrameInfo* frameInfo)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;
    int result;
    
    result = msk_accept_stream_frame(sink->targetSink, streamId, frameInfo);

    if (!result && sink->videoStreamId == streamId) {
        /* video stream not available for vitc decode - signal to source that timecode is ready, but not decoded */
        PTHREAD_MUTEX_LOCK(&sink->timecodeMutex);
        sink->timecodeReady = 1;
        sink->haveDecodedTimecode = 0;
        pthread_cond_broadcast(&sink->timecodeReadyCond);
        PTHREAD_MUTEX_UNLOCK(&sink->timecodeMutex);
    }
    
    return result;
}

static int vss_get_stream_buffer(void *data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;
    int result;

    result = msk_get_stream_buffer(sink->targetSink, streamId, bufferSize, buffer);

    if (!result && sink->videoStreamId == streamId) {
        /* video stream not available for vitc decode - signal to source that timecode is ready, but not decoded */
        PTHREAD_MUTEX_LOCK(&sink->timecodeMutex);
        sink->timecodeReady = 1;
        sink->haveDecodedTimecode = 0;
        pthread_cond_broadcast(&sink->timecodeReadyCond);
        PTHREAD_MUTEX_UNLOCK(&sink->timecodeMutex);
    }
    
    return result;
}

static int vss_receive_stream_frame(void *data, int streamId, unsigned char *buffer, unsigned int bufferSize)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;
    
    if (streamId == sink->videoStreamId)
        decode_vitc(sink, buffer, bufferSize);

    return msk_receive_stream_frame(sink->targetSink, streamId, buffer, bufferSize);
}

static int vss_receive_stream_frame_const(void *data, int streamId, const unsigned char *buffer, unsigned int bufferSize)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;

    if (streamId == sink->videoStreamId)
        decode_vitc(sink, buffer, bufferSize);

    return msk_receive_stream_frame_const(sink->targetSink, streamId, buffer, bufferSize);
}

static int vss_complete_frame(void *data, const FrameInfo* frameInfo)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;

    PTHREAD_MUTEX_LOCK(&sink->timecodeMutex);
    sink->timecodeReady = 0;
    sink->haveDecodedTimecode = 0;
    PTHREAD_MUTEX_UNLOCK(&sink->timecodeMutex);

    return msk_complete_frame(sink->targetSink, frameInfo);
}

static void vss_cancel_frame(void *data)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;

    PTHREAD_MUTEX_LOCK(&sink->timecodeMutex);
    sink->timecodeReady = 0;
    sink->haveDecodedTimecode = 0;
    PTHREAD_MUTEX_UNLOCK(&sink->timecodeMutex);

    msk_cancel_frame(sink->targetSink);
}

static OnScreenDisplay* vss_get_osd(void *data)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;

    return msk_get_osd(sink->targetSink);
}

static VideoSwitchSink* vss_get_video_switch(void *data)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;

    return msk_get_video_switch(sink->targetSink);
}

static AudioSwitchSink* vss_get_audio_switch(void *data)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;

    return msk_get_audio_switch(sink->targetSink);
}

static HalfSplitSink* vss_get_half_split(void *data)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;

    return msk_get_half_split(sink->targetSink);
}

static FrameSequenceSink* vss_get_frame_sequence(void *data)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;

    return msk_get_frame_sequence(sink->targetSink);
}

static int vss_get_buffer_state(void *data, int* numBuffers, int* numBuffersFilled)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;

    return msk_get_buffer_state(sink->targetSink, numBuffers, numBuffersFilled);
}

static int vss_mute_audio(void *data, int mute)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;

    return msk_mute_audio(sink->targetSink, mute);
}

static void vss_sink_close(void *data)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;

    vss_source_close(data);
    
    msk_close(sink->targetSink);
    
    SAFE_FREE(&sink->vitcLines);

    destroy_cond_var(&sink->timecodeReadyCond);
    destroy_mutex(&sink->timecodeMutex);

    SAFE_FREE(&sink);
}

static int vss_reset_or_close(void *data)
{
    VITCReaderSinkSource *sink = (VITCReaderSinkSource*)data;
    int result;

    sink->videoStreamId = -1;

    result = msk_reset_or_close(sink->targetSink);
    if (result != 1) {
        if (result == 2) {
            /* target sink was closed */
            sink->targetSink = NULL;
        }
        
        vss_sink_close(data);
        return 2;
    }

    return 1;
}



int vss_create_vitc_reader(unsigned int *vitc_lines, int num_vitc_lines, VITCReaderSinkSource **sonk)
{
    VITCReaderSinkSource *newSonk;
    
    CHK_ORET(vitc_lines && num_vitc_lines > 0);

    CALLOC_ORET(newSonk, VITCReaderSinkSource, 1);
    newSonk->videoStreamId = -1;
    newSonk->length = 0x7fffffffffffffffLL;
    
    CALLOC_ORET(newSonk->vitcLines, unsigned int, num_vitc_lines);
    memcpy(newSonk->vitcLines, vitc_lines, num_vitc_lines * sizeof(unsigned int));
    newSonk->numVITCLines = num_vitc_lines;

    newSonk->source.data = newSonk;
    newSonk->source.get_num_streams = vss_get_num_streams;
    newSonk->source.get_stream_info = vss_get_stream_info;
    newSonk->source.set_frame_rate_or_disable = vss_set_frame_rate_or_disable;
    newSonk->source.disable_stream = vss_disable_stream;
    newSonk->source.stream_is_disabled = vss_stream_is_disabled;
    newSonk->source.read_frame = vss_read_frame;
    newSonk->source.is_seekable = vss_is_seekable;
    newSonk->source.seek = vss_seek;
    newSonk->source.get_length = vss_get_length;
    newSonk->source.get_position = vss_get_position;
    newSonk->source.get_available_length = vss_get_available_length;
    newSonk->source.eof = vss_eof;
    newSonk->source.set_source_name = vss_set_source_name;
    newSonk->source.set_clip_id = vss_set_clip_id;
    newSonk->source.close = vss_source_close;

    CHK_OFAIL(initialise_stream_info(&newSonk->sourceStreamInfo));
    newSonk->sourceStreamInfo.type = TIMECODE_STREAM_TYPE;
    newSonk->sourceStreamInfo.format = TIMECODE_FORMAT;
    newSonk->sourceStreamInfo.frameRate.num = 25; /* frame rate could be updated in set_frame_rate_or_disable */
    newSonk->sourceStreamInfo.frameRate.den = 1;
    newSonk->sourceStreamInfo.isHardFrameRate = 0;
    newSonk->sourceStreamInfo.format = TIMECODE_FORMAT;
    newSonk->sourceStreamInfo.sourceId = msc_create_id();
    newSonk->sourceStreamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
    newSonk->sourceStreamInfo.timecodeSubType = VITC_SOURCE_TIMECODE_SUBTYPE;


    newSonk->sink.data = newSonk;
    newSonk->sink.register_listener = vss_register_listener;
    newSonk->sink.unregister_listener = vss_unregister_listener;
    newSonk->sink.accept_stream = vss_accept_stream;
    newSonk->sink.register_stream = vss_register_stream;
    newSonk->sink.accept_stream_frame = vss_accept_stream_frame;
    newSonk->sink.get_stream_buffer = vss_get_stream_buffer;
    newSonk->sink.receive_stream_frame = vss_receive_stream_frame;
    newSonk->sink.receive_stream_frame_const = vss_receive_stream_frame_const;
    newSonk->sink.complete_frame = vss_complete_frame;
    newSonk->sink.cancel_frame = vss_cancel_frame;
    newSonk->sink.get_osd = vss_get_osd;
    newSonk->sink.get_video_switch = vss_get_video_switch;
    newSonk->sink.get_audio_switch = vss_get_audio_switch;
    newSonk->sink.get_half_split = vss_get_half_split;
    newSonk->sink.get_frame_sequence = vss_get_frame_sequence;
    newSonk->sink.get_buffer_state = vss_get_buffer_state;
    newSonk->sink.mute_audio = vss_mute_audio;
    newSonk->sink.reset_or_close = vss_reset_or_close;
    newSonk->sink.close = vss_sink_close;


    CHK_OFAIL(init_mutex(&newSonk->timecodeMutex));
    CHK_OFAIL(init_cond_var(&newSonk->timecodeReadyCond));


    *sonk = newSonk;
    return 1;

fail:
    vss_sink_close(newSonk);
    return 0;
}

void vss_set_target_sink(VITCReaderSinkSource *sonk, MediaSink *target_sink)
{
    sonk->targetSink = target_sink;
}

MediaSink* vss_get_media_sink(VITCReaderSinkSource *sonk)
{
    return &sonk->sink;
}

MediaSource* vss_get_media_source(VITCReaderSinkSource *sonk)
{
    return &sonk->source;
}

