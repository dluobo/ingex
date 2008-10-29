/*
 * $Id: shared_mem_source.c,v 1.6 2008/10/29 17:47:42 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008 BBC Research, Stuart Cunningham, <stuart_hc@users.sourceforge.net>
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

#include "shared_mem_source.h"
#include "video_conversion.h"
#include "source_event.h"
#include "logging.h"
#include "macros.h"


#if defined(DISABLE_SHARED_MEM_SOURCE)

int shared_mem_open(const char *channel_name, MediaSource** source)
{
    return 0;
}


#else



#undef PTHREAD_MUTEX_LOCK
#undef PTHREAD_MUTEX_UNLOCK
#include "../../studio/capture/nexus_control.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

#define MAX_TRACKS      19


typedef struct
{
    int streamIndex;
    StreamInfo streamInfo;
    int frameSize;
    int isDisabled;
} TrackInfo;

typedef struct
{
    MediaSource mediaSource;
    int channel;
    int primary;                    // boolean: primary video buffer or secondary
    CaptureFormat captureFormat;

    TrackInfo tracks[MAX_TRACKS];
    int numTracks;

    int64_t position;
    
    int prevLastFrame;

    char sourceName[64];
    int sourceNameUpdate;
} SharedMemSource;



static uint8_t *ring[4];
static NexusControl    *pctl = NULL;
static int ring_connected = 0;

static int connect_to_shared_memory(int channelnum)
{
    if (ring_connected)
        return 1;

    // Connect to shared memory
    int verbose = 1;
    int                shm_id, control_id;

    if (verbose) {
        printf("Waiting for shared memory... ");
        fflush(stdout);
    }

    int try = 0;
    while (1)
    {
        control_id = shmget(9, sizeof(*pctl), 0666);
        if (control_id != -1)
            break;
        try++;
        if (try > 5)
            return 0;
        usleep(20 * 1000);
    }

    pctl = (NexusControl*)shmat(control_id, NULL, 0 /* read/write */);
    if (verbose)
        printf("connected to pctl\n");

    if (verbose)
        printf("  channels=%d elementsize=%d ringlen=%d\n",
                pctl->channels,
                pctl->elementsize,
                pctl->ringlen);

    if (channelnum+1 > pctl->channels)
    {
        printf("  channelnum not available\n");
        return 0;
    }

    int i;
    for (i = 0; i < pctl->channels; i++)
    {
        while (1)
        {
            shm_id = shmget(10 + i, pctl->elementsize, 0666);
            if (shm_id != -1)
                break;
            usleep(20 * 1000);
        }
        ring[i] = (uint8_t*)shmat(shm_id, NULL, 0 /* read/write */);
        if (verbose)
            printf("  attached to channel[%d] '%s'\n", i, pctl->channel[i].source_name);
    }

    ring_connected = 1;
    return 1;
}

static int shm_get_num_streams(void* data)
{
    SharedMemSource* source = (SharedMemSource*)data;

    return source->numTracks;
}

static int shm_get_stream_info(void* data, int streamIndex, const StreamInfo** streamInfo)
{
    SharedMemSource* source = (SharedMemSource*)data;

    if (streamIndex < 0 || streamIndex >= source->numTracks)
    {
        /* unknown stream */
        return 0;
    }

    *streamInfo = &source->tracks[streamIndex].streamInfo;
    return 1;
}

static int shm_disable_stream(void* data, int streamIndex)
{
    SharedMemSource* source = (SharedMemSource*)data;

    if (streamIndex < 0 || streamIndex >= source->numTracks)
    {
        /* unknown stream */
        return 0;
    }

    source->tracks[streamIndex].isDisabled = 1;
    return 1;
}

static void shm_disable_audio(void* data)
{
    SharedMemSource* source = (SharedMemSource*)data;
    int i;

    for (i = 0; i < source->numTracks; i++)
    {
        if (source->tracks[i].streamInfo.type == SOUND_STREAM_TYPE)
        {
            source->tracks[i].isDisabled = 1;
        }
    }
}

static int shm_stream_is_disabled(void* data, int streamIndex)
{
    SharedMemSource* source = (SharedMemSource*)data;

    if (streamIndex < 0 || streamIndex >= source->numTracks)
    {
        /* unknown stream */
        return 0;
    }

    return source->tracks[streamIndex].isDisabled;
}

static uint8_t *rec_ring_video(SharedMemSource *source, int lastFrame)
{
    int channel = source->channel;

    int video_offset = 0;
    if (! source->primary)
        video_offset = pctl->audio12_offset + pctl->audio_size;

    return ring[channel] + video_offset +
                pctl->elementsize * (lastFrame % pctl->ringlen);
}

static uint8_t *rec_ring_audio(SharedMemSource *source, int track, int lastFrame)
{
    int channel = source->channel;

    // tracks 0 and 1 are mixed together at audio12_offset
    if (track == 0 || track == 1)
        return ring[channel] + pctl->audio12_offset +
                pctl->elementsize * (lastFrame % pctl->ringlen);

    // tracks 2 and 3 are mixed together at audio34_offset
    return ring[channel] + pctl->audio34_offset +
                pctl->elementsize * (lastFrame % pctl->ringlen);
}

static uint8_t *rec_ring_timecode(SharedMemSource *source, int track, int lastFrame)
{
    int channel = source->channel;

    int offset = pctl->vitc_offset;         // VITC
    if (track == 1)
        offset = pctl->ltc_offset;        // LTC

    return ring[channel] + pctl->elementsize * (lastFrame % pctl->ringlen) + offset;
}

static void dvsaudio32_to_16bitmono(int channel, uint8_t *buf32, uint8_t *buf16)
{
    int i;
    // A DVS audio buffer contains a mix of two 32bits-per-sample channels
    // Data for one sample pair is 8 bytes:
    //  a0 a0 a0 a0  a1 a1 a1 a1

    int channel_offset = 0;
    if (channel == 1)
        channel_offset = 4;

    // Skip every other channel, copying 16 most significant bits of 32 bits
    // from little-endian DVS format to little-endian 16bits
    for (i = channel_offset; i < 1920*4*2; i += 8) {
        *buf16++ = buf32[i+2];
        *buf16++ = buf32[i+3];
    }
}

/* returns 0 when successfull, -2 if timed out, otherwise -1 */
static int shm_read_frame(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    SharedMemSource* source = (SharedMemSource*)data;
    int i;
    TrackInfo* track;
    unsigned char* buffer;
    int lastFrame;
    int waitCount = 0;
    const int sleepUSec = 100;
    int nameUpdated;
    
    /* wait until a new frame is available in the ring buffer */
    lastFrame = pctl->channel[source->channel].lastframe - 1;
    waitCount = 0;
    while (lastFrame == source->prevLastFrame && waitCount < 60000 / sleepUSec)
    {
        usleep(sleepUSec);
        waitCount++;
        
        lastFrame = pctl->channel[source->channel].lastframe - 1;
    }
    if (lastFrame == source->prevLastFrame && waitCount >= 60000 / sleepUSec)
    {
        /* waited a 1.5 frames - time out */
        return -2;
    }
    source->prevLastFrame = lastFrame;
    
    
    /* check for updated source name */
    
    PTHREAD_MUTEX_LOCK(&pctl->m_source_name_update);
    
    nameUpdated = 0;
    if (pctl->source_name_update != source->sourceNameUpdate)
    {
        if (strcmp(pctl->channel[source->channel].source_name, source->sourceName) != 0)
        {
            nameUpdated = 1;
            strncpy(source->sourceName, pctl->channel[source->channel].source_name, sizeof(source->sourceName) - 1);
        }
        
        source->sourceNameUpdate = pctl->source_name_update;
    }
    
    PTHREAD_MUTEX_UNLOCK(&pctl->m_source_name_update);

    // Track numbers are hard-coded by setup code in shared_mem_open()
    // 0 - video
    // 1 - audio 1
    // 2 - audio 2
    // 3 - audio 3
    // 4 - audio 4
    // 5 - timecode
    // 6 - timecode
    //
    for (i = 0; i < source->numTracks; i++)
    {
        track = &source->tracks[i];
        
        if (track->isDisabled)
            continue;

        if (! sdl_accept_frame(listener, i, frameInfo))
            continue;

        /* calculate current frame size for events stream */
        if (track->streamInfo.type == EVENT_STREAM_TYPE)
        {
            if (nameUpdated)
            {
                track->frameSize = svt_get_buffer_size(1);
            }
            else
            {
                track->frameSize = svt_get_buffer_size(0);
            }
        }
        
        if (! sdl_allocate_buffer(listener, i, &buffer, track->frameSize))
        {
            /* listener failed to allocate a buffer for us */
            return -1;
        }

        /* copy data into buffer */
        if (track->streamInfo.type == PICTURE_STREAM_TYPE)
        {
            unsigned char* data = rec_ring_video(source, lastFrame);
            
            if (source->captureFormat == Format422PlanarYUVShifted)
            {
                /* shift picture back up one line */
                
                /* Y */
                memcpy(buffer, &data[track->streamInfo.width], 
                    track->streamInfo.width * (track->streamInfo.height - 1));
                memset(&buffer[track->streamInfo.width * (track->streamInfo.height - 1)], 
                    0x80, track->streamInfo.width);
                
                /* U */
                memcpy(&buffer[track->streamInfo.width * track->streamInfo.height], 
                    &data[track->streamInfo.width * track->streamInfo.height + track->streamInfo.width / 2], 
                    track->streamInfo.width * (track->streamInfo.height - 1) / 2);
                memset(&buffer[track->streamInfo.width * track->streamInfo.height + track->streamInfo.width * (track->streamInfo.height - 1) / 2], 
                    0x10, track->streamInfo.width / 2);
                
                /* V */
                memcpy(&buffer[track->streamInfo.width * track->streamInfo.height * 3 / 2], 
                    &data[track->streamInfo.width * track->streamInfo.height * 3 / 2 + track->streamInfo.width / 2], 
                    track->streamInfo.width * (track->streamInfo.height - 1) / 2);
                memset(&buffer[track->streamInfo.width * track->streamInfo.height * 3 / 2 + track->streamInfo.width * (track->streamInfo.height - 1) / 2], 
                    0x10, track->streamInfo.width / 2);
            }
            else if (source->captureFormat == Format420PlanarYUVShifted)
            {
                /* shift picture back up one line */
                
                /* Y */
                memcpy(buffer, &data[track->streamInfo.width], 
                    track->streamInfo.width * (track->streamInfo.height - 1));
                memset(&buffer[track->streamInfo.width * (track->streamInfo.height - 1)], 
                    0x80, track->streamInfo.width);
                
                /* U */
                memcpy(&buffer[track->streamInfo.width * track->streamInfo.height], 
                    &data[track->streamInfo.width * track->streamInfo.height + track->streamInfo.width / 2], 
                    track->streamInfo.width * (track->streamInfo.height - 1) / 4);
                memset(&buffer[track->streamInfo.width * track->streamInfo.height + track->streamInfo.width * (track->streamInfo.height - 1) / 4], 
                    0x10, track->streamInfo.width / 2);
                
                /* V */
                memcpy(&buffer[track->streamInfo.width * track->streamInfo.height * 5 / 4], 
                    &data[track->streamInfo.width * track->streamInfo.height * 5 / 4 + track->streamInfo.width / 2], 
                    track->streamInfo.width * (track->streamInfo.height - 1) / 4);
                memset(&buffer[track->streamInfo.width * track->streamInfo.height * 5 / 4 + track->streamInfo.width * (track->streamInfo.height - 1) / 4], 
                    0x10, track->streamInfo.width / 2);
            }
            else
            {
                memcpy(buffer, data, track->frameSize);
            }
        }

        if (track->streamInfo.type == SOUND_STREAM_TYPE) {
            // Get a pointer to the correct DVS audio buffer
            uint8_t *buf32 = rec_ring_audio(source, i - 1, lastFrame);

            // determine which channel (0 or 1) of the DVS pair to use
            int pair_num = (i - 1) % 2;

            // reformat audio from interleaved DVS audio format to single channel audio
            dvsaudio32_to_16bitmono(pair_num, buf32, buffer);
        }

        if (track->streamInfo.type == TIMECODE_STREAM_TYPE) {
            int tc_as_int;
            memcpy(&tc_as_int, rec_ring_timecode(source, i - 5, lastFrame), sizeof(int));
            // convert ts-as-int to Timecode
            Timecode tc;
            tc.isDropFrame = 0;
            tc.frame = tc_as_int % 25;
            tc.hour = (int)(tc_as_int / (60 * 60 * 25));
            tc.min = (int)((tc_as_int - (tc.hour * 60 * 60 * 25)) / (60 * 25));
            tc.sec = (int)((tc_as_int - (tc.hour * 60 * 60 * 25) - (tc.min * 60 * 25)) / 25);
            memcpy(buffer, &tc, track->frameSize);
        }
        
        if (track->streamInfo.type == EVENT_STREAM_TYPE)
        {
            if (nameUpdated)
            {
                SourceEvent event;
                svt_set_name_update_event(&event, source->sourceName);
                
                svt_write_num_events(buffer, 1);
                svt_write_event(buffer, 0, &event);
            }
            else
            {
                svt_write_num_events(buffer, 0);
            }
        }

        sdl_receive_frame(listener, i, buffer, track->frameSize);
    }
    source->position += 1;

    return 0;
}

static int shm_is_seekable(void* data)
{
    /* cannot seek */
    return 0;
}

/* returns 0 when successfull, -2 if timed out, otherwise -1 */
static int shm_seek(void* data, int64_t position)
{
    /* cannot seek and shouldn't have called this function */
    return -1;
}

static int shm_get_length(void* data, int64_t* length)
{
    /* unknown length */
    return 0;
}

static int shm_get_position(void* data, int64_t* position)
{
    SharedMemSource* source = (SharedMemSource*)data;

    *position = source->position;
    return 1;
}

static int shm_get_available_length(void* data, int64_t* length)
{
    return shm_get_length(data, length);
}

static int shm_eof(void* data)
{
    /* never at "end of file" */
    return 0;
}

static void shm_set_source_name(void* data, const char* name)
{
    SharedMemSource* source = (SharedMemSource*)data;

    int i;
    for (i = 0; i < source->numTracks; i++)
    {
        add_known_source_info(&source->tracks[i].streamInfo, SRC_INFO_NAME, name);    
    }
}

static void shm_set_clip_id(void* data, const char* id)
{
    SharedMemSource* source = (SharedMemSource*)data;

    int i;
    for (i = 0; i < source->numTracks; i++)
    {
        set_stream_clip_id(&source->tracks[i].streamInfo, id);    
    }
}

static void shm_close(void* data)
{
    SharedMemSource* source = (SharedMemSource*)data;
    int i;

    if (data == NULL)
    {
        return;
    }

    for (i = 0; i < source->numTracks; i++)
    {
        clear_stream_info(&source->tracks[i].streamInfo);
    }

    SAFE_FREE(&source);
}

int shared_mem_open(const char* channel_name, MediaSource** source)
{
    SharedMemSource* newSource = NULL;
    int channelNum;
    int primary;
    int sourceId;
    CaptureFormat captureFormat;
    int disable = 0;
    int i;
    
    /* check channel number is within range */
    channelNum = atol(channel_name);
    if (channelNum < 0 || channelNum >= MAX_CHANNELS)
    {
        ml_log_error("Card %s is out of range of maximum %d supported channels\n", channel_name, MAX_CHANNELS);
        return 0;
    }

    /* Support "0p" or "0s" for primary/secondary video */
    if (strchr(channel_name, 'p') == NULL)
    {
        primary = 0;
    }
    else
    {
        primary = 1;
    }

    char title[FILENAME_MAX];
    if (primary)
    {
        sprintf(title, "Shared Memory Video: channel %d primary\n", channelNum);
    }
    else
    {
        sprintf(title, "Shared Memory Video: channel %d secondary\n", channelNum);
    }

    /* try to connect to shared mem */
    if (! connect_to_shared_memory(channelNum))
    {
        ml_log_error("Failed to connect to shared memory for channel %s\n", channel_name);
        return 0;
    }

    
    captureFormat = primary ? pctl->pri_video_format : pctl->sec_video_format;
    
    if (captureFormat != Format422UYVY &&
        captureFormat != Format422PlanarYUV &&
        captureFormat != Format422PlanarYUVShifted &&
        captureFormat != Format420PlanarYUV &&
        captureFormat != Format420PlanarYUVShifted)
    {
        ml_log_error("Shared memory video format %d is not supported\n", pctl->pri_video_format);
        return 0;
    }

    if (channelNum >= pctl->channels)
    {
        ml_log_warn("Shared memory channel %d is not in use - streams will be disabled\n", channelNum);
        disable = 1;
    }
    
    

    CALLOC_ORET(newSource, SharedMemSource, 1);

    newSource->channel = channelNum;
    newSource->prevLastFrame = -1;
    newSource->primary = primary;
    newSource->sourceNameUpdate = -1;
    newSource->captureFormat = captureFormat;
    
    
    /* setup media source */
    newSource->mediaSource.data = newSource;
    newSource->mediaSource.get_num_streams = shm_get_num_streams;
    newSource->mediaSource.get_stream_info = shm_get_stream_info;
    newSource->mediaSource.disable_stream = shm_disable_stream;
    newSource->mediaSource.disable_audio = shm_disable_audio;
    newSource->mediaSource.stream_is_disabled = shm_stream_is_disabled;
    newSource->mediaSource.read_frame = shm_read_frame;
    newSource->mediaSource.is_seekable = shm_is_seekable;
    newSource->mediaSource.seek = shm_seek;
    newSource->mediaSource.get_length = shm_get_length;
    newSource->mediaSource.get_position = shm_get_position;
    newSource->mediaSource.get_available_length = shm_get_available_length;
    newSource->mediaSource.eof = shm_eof;
    newSource->mediaSource.set_source_name = shm_set_source_name;
    newSource->mediaSource.set_clip_id = shm_set_clip_id;
    newSource->mediaSource.close = shm_close;

    
    sourceId = msc_create_id();

    /* video track */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = PICTURE_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.frameRate = g_palFrameRate;
    newSource->tracks[newSource->numTracks].streamInfo.width = pctl->width;
    newSource->tracks[newSource->numTracks].streamInfo.height = pctl->height;
    newSource->tracks[newSource->numTracks].streamInfo.aspectRatio.num = 16;
    newSource->tracks[newSource->numTracks].streamInfo.aspectRatio.den = 9;
    switch (captureFormat)
    {
        case Format422UYVY:
            newSource->tracks[newSource->numTracks].frameSize = pctl->width * pctl->height * 2;
            newSource->tracks[newSource->numTracks].streamInfo.format = UYVY_FORMAT;
            break;
            
        case Format422PlanarYUV:
            newSource->tracks[newSource->numTracks].frameSize = pctl->width * pctl->height * 2;
            newSource->tracks[newSource->numTracks].streamInfo.format = YUV422_FORMAT;
            break;
            
        case Format422PlanarYUVShifted:
            newSource->tracks[newSource->numTracks].frameSize = pctl->width * pctl->height * 2;
            newSource->tracks[newSource->numTracks].streamInfo.format = YUV422_FORMAT;
            break;

        case Format420PlanarYUV:
            newSource->tracks[newSource->numTracks].frameSize = pctl->width * pctl->height * 3/2;
            newSource->tracks[newSource->numTracks].streamInfo.format = YUV420_FORMAT;
            break;
            
        case Format420PlanarYUVShifted:
            newSource->tracks[newSource->numTracks].frameSize = pctl->width * pctl->height * 3/2;
            newSource->tracks[newSource->numTracks].streamInfo.format = YUV420_FORMAT;
            break;
        default:
            goto fail;
            break;
    }
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, title));
    newSource->numTracks++;

    /* audio track 1 */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = SOUND_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = PCM_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.samplingRate = g_profAudioSamplingRate;
    newSource->tracks[newSource->numTracks].streamInfo.numChannels = 1;
    newSource->tracks[newSource->numTracks].streamInfo.bitsPerSample = 16;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "Shared Memory Audio 1"));
    newSource->tracks[newSource->numTracks].frameSize = 2 * 48000 / 25;
    newSource->numTracks++;

    /* audio track 2 */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = SOUND_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = PCM_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.samplingRate = g_profAudioSamplingRate;
    newSource->tracks[newSource->numTracks].streamInfo.numChannels = 1;
    newSource->tracks[newSource->numTracks].streamInfo.bitsPerSample = 16;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "Shared Memory Audio 2"));
    newSource->tracks[newSource->numTracks].frameSize = 2 * 48000 / 25;
    newSource->numTracks++;

    /* audio track 3 */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = SOUND_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = PCM_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.samplingRate = g_profAudioSamplingRate;
    newSource->tracks[newSource->numTracks].streamInfo.numChannels = 1;
    newSource->tracks[newSource->numTracks].streamInfo.bitsPerSample = 16;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "Shared Memory Audio 3"));
    newSource->tracks[newSource->numTracks].frameSize = 2 * 48000 / 25;
    newSource->numTracks++;

    /* audio track 4 */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = SOUND_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = PCM_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.samplingRate = g_profAudioSamplingRate;
    newSource->tracks[newSource->numTracks].streamInfo.numChannels = 1;
    newSource->tracks[newSource->numTracks].streamInfo.bitsPerSample = 16;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "Shared Memory Audio 4"));
    newSource->tracks[newSource->numTracks].frameSize = 2 * 48000 / 25;
    newSource->numTracks++;

    /* timecode track 1 */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = TIMECODE_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = TIMECODE_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeSubType = VITC_SOURCE_TIMECODE_SUBTYPE;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "Shared Memory Timecode 1"));
    newSource->tracks[newSource->numTracks].frameSize = sizeof(Timecode);
    newSource->numTracks++;

    /* timecode track 2 */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = TIMECODE_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = TIMECODE_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeSubType = LTC_SOURCE_TIMECODE_SUBTYPE;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "Shared Memory Timecode 2"));
    newSource->tracks[newSource->numTracks].frameSize = sizeof(Timecode);
    newSource->numTracks++;

    /* event track */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = EVENT_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = SOURCE_EVENT_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "Shared Memory Event"));
    newSource->numTracks++;

    
    /* disabled tracks if the channel is not in use */
    if (disable)
    {
        for (i = 0; i < newSource->numTracks; i++)
        {
            CHK_OFAIL(msc_disable_stream(&newSource->mediaSource, i));
        }
    }
    
    
    *source = &newSource->mediaSource;
    return 1;

fail:
    shm_close(newSource);
    return 0;
}


#endif


