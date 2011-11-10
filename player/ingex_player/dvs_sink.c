/*
 * $Id: dvs_sink.c,v 1.27 2011/11/10 10:53:35 philipn Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Stuart Cunningham
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
#include <inttypes.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>   /* gettimeofday() */

#include "dvs_sink.h"
#include "on_screen_display.h"
#include "video_conversion.h"
#include "video_conversion_10bits.h"
#include "yuvlib/YUV_frame.h"
#include "video_VITC.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


#if !defined(HAVE_DVS)

int dvs_open(int dvsCard, int dvsChannel, SDIVITCSource sdiVITCSource, SDIVITCSource extraSDIVITCSource,
    int numBuffers, int disableOSD, int fitVideo, DVSSink** sink)
{
    return 0;
}

MediaSink* dvs_get_media_sink(DVSSink* sink)
{
    return NULL;
}

SDIRaster dvs_get_raster(DVSSink* sink)
{
    return SDI_SD_625_RASTER;
}

void dvs_close_card(DVSSink* sink)
{
}

int dvs_card_is_available(int card, int channel)
{
    return 0;
}


#else

#include "dvs_clib.h"
#include "dvs_fifo.h"

// Define missing macros for older SDKs
#ifndef SV_FIFO_DMA_ON
#define SV_FIFO_DMA_ON 1
#endif

/* use a hack to try correct for AV sync issues found with the DVS SDK version 2.57 */
#if (DVS_VERSION_MAJOR < 3)
#define AV_SYNC_HACK
#warning DVS SDK version is less than 3; applying av sync hack
#endif

#define MAX_DVS_CARDS               4

#define MAX_DVS_CHANNELS            2

#define MAX_DVS_AUDIO_STREAMS       8

/* we assume that a VITC and LTC are within this maximum of timecode streams */
#define MAX_TIMECODES               32

/* maximum delay before a frame is displayed */
#define MAX_DISPLAY_DELAY           64

/* interval for polling the DVS card fifo for changes */
#define DVS_POLL_INTERVAL           10

/* the minimum number of frames that must be present in the fifo before the previous frame is
repeated to avoid the buffer running empty */
#define MIN_REPEAT_FIFO_BUFFERS     5

#define AUDIO_OVERFLOW_DATA_SIZE    (4 * 4 * 2)


#define SV_CHK_ORET(x) \
    { \
        int res = x; \
        if (res != SV_OK) \
        { \
            ml_log_error("%s failed (%d, %s), in %s, line %d\n", #x, res, sv_geterrortext(res), __FILE__, __LINE__); \
            return 0; \
        } \
    }
#define SV_CHK_OFAIL(x) \
    { \
        int res = x; \
        if (res != SV_OK) \
        { \
            ml_log_error("%s failed (%d, %s), in %s, line %d\n", #x, res, sv_geterrortext(res), __FILE__, __LINE__); \
            goto fail; \
        } \
    }



typedef struct
{
    int streamId;
    TimecodeType timecodeType;
    TimecodeSubType timecodeSubType;
    Timecode timecode;
    int isPresent;
} TimecodeStream;

typedef struct
{
    int index;

    int streamId;
    StreamInfo streamInfo;
    int audio_byte_align;

    int ownData; /* delete here if data is not a reference into the buffer */
    unsigned int dataSize[2];
    unsigned char* data[2];
    unsigned int allocatedDataSize[2];

    int requireFit; /* true if the stream dimensions != dvs frame dimensions */

    int isPresent;
} DVSStream;

typedef struct
{
    unsigned char* buffer;
    unsigned int bufferSize;
    unsigned int audio_data_size;
    int vitcCount;
    int ltcCount;
    Timecode vitcTimecode;
    Timecode extraVITCTimecode;
} DVSFifoBuffer;

struct DVSSink
{
    MediaSink mediaSink;

    /* on screen display */
    OnScreenDisplay* osd;
    OSDListener osdListener;

    /* select source for output VITC and extra VITC */
    SDIVITCSource sdiVITCSource;
    SDIVITCSource extraSDIVITCSource;

    /* accept video that has wrong dimensions and fit it into the dvs frame */
    int fitVideo;

    /* listener for sink events */
    MediaSinkListener* listener;

    int osdInitialised;

    DVSStream videoStream;
    DVSStream audioStream[MAX_DVS_AUDIO_STREAMS];
    int numAudioStreams;

    TimecodeStream timecodes[MAX_TIMECODES];
    int numTimecodes;

    int depth8Bit;
    Rational frameRate;
    unsigned int roundedTimecodeBase;
    unsigned int rasterWidth;
    unsigned int rasterHeight;
    unsigned int videoDataSize;
    unsigned int videoOffset;
    unsigned int maxAudioDataSize;
    unsigned int audioPairOffset[(MAX_DVS_AUDIO_STREAMS + 1) / 2];
    unsigned int audio_overflow_size;
    unsigned char audio_overflow_data[(MAX_DVS_AUDIO_STREAMS + 1) / 2][AUDIO_OVERFLOW_DATA_SIZE];

    /* data sent to DVS card: A/V buffer and timecodes */
    DVSFifoBuffer fifoBuffer[2];
    int currentFifoBuffer;
    pthread_mutex_t dvsFifoMutex;

    /* image characteristics */
    int width;
    int height;
    Rational aspectRatio;


    sv_handle* sv;
    sv_fifo* svfifo;
    int palFFMode;

    int fifoStarted;

    /* info about the last MAX_DISPLAY_OFFSET frames */
    pthread_mutex_t frameInfosMutex;
    FrameInfo frameInfos[MAX_DISPLAY_DELAY];
    int64_t frameInfosCount;


    int64_t numFramesDisplayed;
    int lastDropped;
    int lastFrameTick;
    int numBuffers;
    int numBuffersFilled;

    pthread_t dvsStateInfoThreadId;

    int stopped; /* set when dvs state info thread is about to be cancelled */

    /* reverse play */
    int reversePlay;

    int muteAudio;

    unsigned char* workBuffer1;
    unsigned int workBuffer1Size;
    unsigned char* workBuffer2;
    unsigned int workBuffer2Size;
};



static int int_to_dvs_tc(DVSSink *sink, int tc)
{
    // e.g. turn frame count 1020219 into hex 0x11200819
    int fr = tc % sink->roundedTimecodeBase;
    int hr = (int)(tc / (60*60*sink->roundedTimecodeBase));
    int mi = (int)((tc - (hr * 60*60*sink->roundedTimecodeBase)) / (60 * sink->roundedTimecodeBase));
    int se = (int)((tc - (hr * 60*60*sink->roundedTimecodeBase) - (mi * 60*sink->roundedTimecodeBase)) / sink->roundedTimecodeBase);

    int fr01 = fr % 10;
    int fr10 = fr / 10;
    int se01 = se % 10;
    int se10 = se / 10;
    int mi01 = mi % 10;
    int mi10 = mi / 10;
    int hr01 = hr % 10;
    int hr10 = hr / 10;

    int result = 0;
    result |= fr01;
    result |= (fr10 << 4);
    result |= (se01 << 8);
    result |= (se10 << 12);
    result |= (mi01 << 16);
    result |= (mi10 << 20);
    result |= (hr01 << 24);
    result |= (hr10 << 28);

    return result;
}

static Timecode get_timecode_from_count(DVSSink *sink, int64_t count)
{
    Timecode tc;

    tc.isDropFrame = 0;
    tc.hour = count / (60 * 60 * sink->roundedTimecodeBase);
    tc.min = (count % (60 * 60 * sink->roundedTimecodeBase)) / (60 * sink->roundedTimecodeBase);
    tc.sec = ((count % (60 * 60 * sink->roundedTimecodeBase)) % (60 * sink->roundedTimecodeBase)) / sink->roundedTimecodeBase;
    tc.frame = ((count % (60 * 60 * sink->roundedTimecodeBase)) % (60 * sink->roundedTimecodeBase)) % sink->roundedTimecodeBase;

    return tc;
}

static Timecode get_timecode(DVSSink* sink, TimecodeType type, TimecodeSubType subType, const Timecode* theDefault)
{
    int i;

    for (i = 0; i < sink->numTimecodes; i++)
    {
        if (sink->timecodes[i].isPresent &&
            sink->timecodes[i].timecodeType == type &&
            sink->timecodes[i].timecodeSubType == subType)
        {
            return sink->timecodes[i].timecode;
        }
    }

    return *theDefault;
}

static int get_timecode_count(DVSSink* sink, TimecodeType type, TimecodeSubType subType, int theDefault)
{
    int i;

    for (i = 0; i < sink->numTimecodes; i++)
    {
        if (sink->timecodes[i].isPresent &&
            sink->timecodes[i].timecodeType == type &&
            sink->timecodes[i].timecodeSubType == subType)
        {
            return sink->timecodes[i].timecode.hour * 60 * 60 * sink->roundedTimecodeBase +
                sink->timecodes[i].timecode.min * 60 * sink->roundedTimecodeBase +
                sink->timecodes[i].timecode.sec * sink->roundedTimecodeBase +
                sink->timecodes[i].timecode.frame;
        }
    }

    return theDefault;
}


static int open_dvs_card(int dvsCard, int dvsChannel, int log, sv_handle** svResult, int* selectedCard, int* selectedChannel)
{
    int result;
    sv_handle* sv;
    int card;
    int channel;
    char card_str[64] = {0};
    int oldOpenString = 0;


    if (dvsCard < 0)
    {
        /* card not specified - start check from card 0 */
        card = 0;
    }
    else
    {
        /* card was specified */
        card = dvsCard;
    }
    if (dvsChannel < 0)
    {
        /* channel was not specified - start check with the channel omitted from the card string */
        channel = -1;
    }
    else
    {
        /* channel was specified */
        channel = dvsChannel;
    }
    result = !SV_OK;
    while (1)
    {
        if (channel >= 0)
        {
            if (!oldOpenString) /* don't bother trying again if format is not accepted */
            {
                /* try open the specified card and channel */
                snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d,channel=%d", card, channel);
                result = sv_openex(&sv,
                                   card_str,
                                   SV_OPENPROGRAM_APPLICATION,
                                   SV_OPENTYPE_OUTPUT,
                                   0,
                                   0);
                if (result != SV_OK)
                {
                    if (result == SV_ERROR_SVOPENSTRING)
                    {
                        oldOpenString = 1;
                    }

                    if (log)
                    {
                        ml_log_info("card %d, channel %d: %s\n", card, channel, sv_geterrortext(result));
                        if (oldOpenString)
                        {
                            ml_log_info("DVS channel selection is not supported\n");
                        }
                    }
                }
            }
        }
        else
        {
            /* try open the specified card */
            snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d", card);
            result = sv_openex(&sv,
                               card_str,
                               SV_OPENPROGRAM_APPLICATION,
                               SV_OPENTYPE_OUTPUT,
                               0,
                               0);
            if (result != SV_OK && log)
            {
                ml_log_info("card %d: %s\n", card, sv_geterrortext(result));
            }
        }

        if (result == SV_OK)
        {
            break;
        }

        /* try the next channel if the channel was not specified */
        if (dvsChannel < 0)
        {
            channel++;

            if (channel >= MAX_DVS_CHANNELS)
            {
                channel = dvsChannel;
            }
        }

        /* try the next card if the card was not specified and all channels have been tried */
        if (dvsCard < 0)
        {
            if (channel == dvsChannel)
            {
                card++;

                if (card >= MAX_DVS_CARDS)
                {
                    break;
                }
            }
        }
        else if (channel == dvsChannel)
        {
            /* all channels on specified card have been tried */
            break;
        }
    }

    if (result != SV_OK)
    {
        return 0;
    }

    *svResult = sv;
    *selectedCard = card;
    *selectedChannel = channel;
    return 1;
}

/* TODO: should SV_CHECK exit or just return 0? */
static int display_on_sv_fifo(DVSSink* sink, DVSFifoBuffer* fifoBuffer)
{
    sv_fifo_buffer *pbuffer;
    sv_handle *sv = sink->sv;
    int i;
    unsigned int offset;
    unsigned int num_copy;


    PTHREAD_MUTEX_LOCK(&sink->dvsFifoMutex)

    SV_CHK_OFAIL(sv_fifo_getbuffer(sv, sink->svfifo, &pbuffer, NULL, SV_FIFO_FLAG_NODMAADDR));

    /* set the video data size */
    pbuffer->video[0].size = sink->videoDataSize;

    /* set the video and audio data pointers */
    pbuffer->video[0].addr = (char*)fifoBuffer->buffer;
    for (i = 0; i < MAX_DVS_AUDIO_STREAMS; i += 2)
    {
        pbuffer->audio[0].addr[i / 2] = (char*)fifoBuffer->buffer + sink->audioPairOffset[i / 2];
    }

    /* determine offset required for input audio if size differs from requested by DVS */
    offset = sink->audio_overflow_size;
    if (fifoBuffer->audio_data_size == 0) {
        offset = 0;
    } else if (offset + fifoBuffer->audio_data_size < (unsigned int)pbuffer->audio[0].size) {
        offset += pbuffer->audio[0].size - (offset + fifoBuffer->audio_data_size);
        if (offset > AUDIO_OVERFLOW_DATA_SIZE)
            offset = AUDIO_OVERFLOW_DATA_SIZE;
    } else {
}
    /* offset audio and add overflow data */
    if (offset > 0) {
        for (i = 0; i < MAX_DVS_AUDIO_STREAMS; i += 2) {
            memmove(pbuffer->audio[0].addr[i / 2] + offset,
                    pbuffer->audio[0].addr[i / 2],
                    fifoBuffer->audio_data_size);
            memcpy(pbuffer->audio[0].addr[i / 2],
                   sink->audio_overflow_data[i / 2] + AUDIO_OVERFLOW_DATA_SIZE - offset,
                   offset);
        }
    }

    /* update overflow size for the next frame */
    if (fifoBuffer->audio_data_size == 0) {
        sink->audio_overflow_size = 0;
    } else if (offset + fifoBuffer->audio_data_size > (unsigned int)pbuffer->audio[0].size) {
        sink->audio_overflow_size = (offset + fifoBuffer->audio_data_size) - pbuffer->audio[0].size;
        if (sink->audio_overflow_size > AUDIO_OVERFLOW_DATA_SIZE)
            sink->audio_overflow_size = AUDIO_OVERFLOW_DATA_SIZE;
    } else {
        sink->audio_overflow_size = 0;
    }

    /* copy data for next frame's overflow data */
    num_copy = AUDIO_OVERFLOW_DATA_SIZE;
    if (num_copy > fifoBuffer->audio_data_size)
        num_copy = fifoBuffer->audio_data_size;
    for (i = 0; i < MAX_DVS_AUDIO_STREAMS; i += 2)
    {
        memset(sink->audio_overflow_data[i / 2], 0, AUDIO_OVERFLOW_DATA_SIZE);
        memcpy(sink->audio_overflow_data[i / 2] + AUDIO_OVERFLOW_DATA_SIZE - num_copy,
               pbuffer->audio[0].addr[i / 2] + offset + fifoBuffer->audio_data_size - num_copy,
               num_copy);
    }


    /* set VITC */
    if (sink->palFFMode && sink->depth8Bit) /* writing VITC lines is only supported for 8-bit */
    {
        /* write timecode lines directly */

        /* TODO: writeVITC assumes line width = 720 */

        if (sink->extraSDIVITCSource != 0)
        {
            /* extra VITC timecodes */

            /* line 0 */
            write_vitc(fifoBuffer->extraVITCTimecode.hour, fifoBuffer->extraVITCTimecode.min,
                fifoBuffer->extraVITCTimecode.sec, fifoBuffer->extraVITCTimecode.frame,
                fifoBuffer->buffer, 1);

            /* line 4 - backup copy */
            write_vitc(fifoBuffer->extraVITCTimecode.hour, fifoBuffer->extraVITCTimecode.min,
                fifoBuffer->extraVITCTimecode.sec, fifoBuffer->extraVITCTimecode.frame,
                fifoBuffer->buffer + 4 * 2 * sink->rasterWidth, 1);
        }

        /* line 8 */
        write_vitc(fifoBuffer->vitcTimecode.hour, fifoBuffer->vitcTimecode.min,
            fifoBuffer->vitcTimecode.sec, fifoBuffer->vitcTimecode.frame,
            fifoBuffer->buffer + 8 * 2 * sink->rasterWidth, 1);

        /* line 12 - backup copy */
        write_vitc(fifoBuffer->vitcTimecode.hour, fifoBuffer->vitcTimecode.min,
            fifoBuffer->vitcTimecode.sec, fifoBuffer->vitcTimecode.frame,
            fifoBuffer->buffer + 12 * 2 * sink->rasterWidth, 1);

    }
    else
    {
        // doesn't appear to work i.e. get output over SDI
        pbuffer->timecode.vitc_tc = fifoBuffer->vitcCount;
        pbuffer->timecode.vitc_tc2 = fifoBuffer->vitcCount | 0x80000000;       // set high bit for second field timecode
    }

    /* set LTC */
    pbuffer->timecode.ltc_tc = fifoBuffer->ltcCount;


    // send the filled in buffer to the hardware and increment the current buffer

    SV_CHK_OFAIL(sv_fifo_putbuffer(sv, sink->svfifo, pbuffer, NULL));


    PTHREAD_MUTEX_UNLOCK(&sink->dvsFifoMutex)


    return 1;

fail:
    PTHREAD_MUTEX_UNLOCK(&sink->dvsFifoMutex)
    return 0;
}


static void* get_dvs_state_info_thread(void* arg)
{
    DVSSink* sink = (DVSSink*)arg;
    struct timeval timeNow, lastTimeNow;
    sv_handle* sv = sink->sv;
    sv_fifo_info info;
    memset(&info, 0, sizeof(sv_fifo_info));
    int frameTick;
    int numFramesDropped;
    int64_t numFramesDisplayed;
    int frameInfoIndex;
    int i;
    int64_t prevNumFramesDisplayed = 0;


    while (!sink->stopped)
    {
        gettimeofday(&lastTimeNow, NULL);

        if (sink->fifoStarted)
        {
            SV_CHK_ORET(sv_fifo_status(sv, sink->svfifo, &info));

            sink->numBuffers = info.nbuffers;
            sink->numBuffersFilled = info.nbuffers - info.availbuffers - 1;

            /* each tick is for 1 field, and there are 2 fields per frame */
            frameTick = info.displaytick / 2;

            if (frameTick != sink->lastFrameTick)
            {
                PTHREAD_MUTEX_LOCK(&sink->frameInfosMutex);

                /* calc num frames dropped; round upwards (the + 1) to catch field drops? */
                numFramesDropped = (info.dropped - sink->lastDropped + 1) / 2;


                /* calc number of frames displayed */
                if (numFramesDropped == 0)
                {
                    /* assume a frame has been displayed */
                    numFramesDisplayed = frameTick - sink->lastFrameTick;
                }
                else
                {
                    /* make sure we have sent messages for all frames when we are dropping frames,
                    not including the frames in the fifo buffer */
                    numFramesDisplayed = sink->frameInfosCount - sink->numFramesDisplayed
                     - (info.nbuffers - 1 - info.availbuffers);
                }


                /* legitimise the number of frames displayed */
                if (sink->numFramesDisplayed + numFramesDisplayed > sink->frameInfosCount)
                {
                    numFramesDisplayed = sink->frameInfosCount - sink->numFramesDisplayed;
                }
                if (numFramesDisplayed < 0)
                {
                    numFramesDisplayed = 0;
                }


                /* send messages for displayed frames */
                for (i = 0; i < numFramesDisplayed; i++)
                {
                    frameInfoIndex = (sink->numFramesDisplayed + i) % MAX_DISPLAY_DELAY;
                    msl_frame_displayed(sink->listener, &sink->frameInfos[frameInfoIndex]);
                }
                sink->numFramesDisplayed += numFramesDisplayed;


                /* send messages for dropped frames - only when at least 1 frame has been displayed */
                if (sink->numFramesDisplayed > 0)
                {
                    frameInfoIndex = (sink->numFramesDisplayed - 1 + MAX_DISPLAY_DELAY) % MAX_DISPLAY_DELAY;
                    for (i = 0; i < numFramesDropped; i++)
                    {
                        msl_frame_dropped(sink->listener, &sink->frameInfos[frameInfoIndex]);
                    }
                }

#if 0
                printf("numDisplay=%lld frameInfos=%lld nbuf=%d availbuf=%d tick=%d displaytick=%d dropped=%d waitdrop=%d\n",
                    sink->numFramesDisplayed, sink->frameInfosCount, info.nbuffers, info.availbuffers,
                    info.tick, info.displaytick, info.dropped, info.waitdropped);
#endif


                /* if frames have been dropped or the fifo is below minimum then repeat the last frame until above minimum */
                if (numFramesDropped > 0 || sink->numBuffersFilled < MIN_REPEAT_FIFO_BUFFERS)
                {
#if defined(AV_SYNC_HACK)
                    /* reset the DVS sync if frames have been dropped despite the code below to repeat frames to prevent
                       the buffer emptying */
                    if (numFramesDropped > 0)
                    {
                        sv_info status_info;
                        sv_status(sink->sv, &status_info);
                        int res;
                        if ((res = sv_sync(sink->sv, status_info.sync)) != SV_OK)
                        {
                            ml_log_warn("Failed to reset DVS card after frame drop: %s\n", sv_geterrortext(res));
                        }
                        else
                        {
                            ml_log_warn("DVS card frame drop - resetting sync to avoid av sync problem\n");
                        }
                    }
#endif

                    /* refill the fifo to the minimum level */

                    /* mute the audio */
                    for (i = 0; i < sink->numAudioStreams; i += 2)
                    {
                        memset(&sink->fifoBuffer[(sink->currentFifoBuffer + 1) % 2].buffer[sink->audioPairOffset[i / 2]], 0, sink->maxAudioDataSize);
                    }

                    int i;
                    for (i = 0; i < MIN_REPEAT_FIFO_BUFFERS - sink->numBuffersFilled; i++)
                    {
                        /* send previous frame to the fifo */
                        display_on_sv_fifo(sink, &sink->fifoBuffer[(sink->currentFifoBuffer + 1) % 2]);
                    }

                    /* send message for dropped frame - only when at least 1 frame has been displayed */
                    if (sink->numFramesDisplayed > 0)
                    {
                        frameInfoIndex = (sink->numFramesDisplayed - 1 + MAX_DISPLAY_DELAY) % MAX_DISPLAY_DELAY;
                        msl_frame_dropped(sink->listener, &sink->frameInfos[frameInfoIndex]);
                    }
                }

                if (sink->numBuffersFilled < MIN_REPEAT_FIFO_BUFFERS - 1)
                {
                    ml_log_warn("DVS fifo nearing empty\n");
                }

                PTHREAD_MUTEX_UNLOCK(&sink->frameInfosMutex);


                sink->lastDropped += numFramesDropped * 2;
                prevNumFramesDisplayed = numFramesDisplayed;
            }

            sink->lastFrameTick = frameTick;
        }

        gettimeofday(&timeNow, NULL);
        sleep_diff(DVS_POLL_INTERVAL * 1000, &timeNow, &lastTimeNow);
    }

    pthread_exit((void*)0);
}



static void reset_streams(DVSSink* sink)
{
    int i;

    /* video stream */
    sink->videoStream.isPresent = 0;

    /* audio streams */
    for (i = 0; i < sink->numAudioStreams; i++)
    {
        sink->audioStream[i].isPresent = 0;
    }

    /* timecode streams */
    for (i = 0; i < sink->numTimecodes; i++)
    {
        sink->timecodes[i].isPresent = 0;
    }
}

static void interleave_audio(unsigned char* audio1, unsigned int numSamples1, int inputByteAlign1,
                             unsigned char* audio2, unsigned int numSamples2, int inputByteAlign2,
                             int reversePlay, unsigned char* outBuffer)
{
    unsigned char* audio1Ptr;
    unsigned char* audio2Ptr;
    unsigned char* outAudioPtr = outBuffer;
    unsigned int num_samples;
    unsigned int i;
    int j;

    assert(audio1 == NULL || (inputByteAlign1 >= 1 && inputByteAlign1 <= 4));
    assert(audio2 == NULL || (inputByteAlign2 >= 1 && inputByteAlign2 <= 4));
    assert(audio1 == NULL || audio2 == NULL || numSamples1 == numSamples2);

    num_samples = numSamples1;
    if (num_samples == 0)
        num_samples = numSamples2;

    if (reversePlay)
    {
        /* position at end of data */
        audio1Ptr = audio1 + (num_samples - 1) * inputByteAlign1;
        audio2Ptr = audio2 + (num_samples - 1) * inputByteAlign2;
    }
    else
    {
        audio1Ptr = audio1;
        audio2Ptr = audio2;
    }

    for (i = 0; i < num_samples; i++)
    {
        /* samples are little endian and are filled from the least significant byte */
        /* DVS interleaved audio format has 4 bytes for audio1, 4 bytes audio2 and so on */

        /* audio 1 */
        for (j = 3; j >= 0; j--)
        {
            if (audio1 != NULL && j < inputByteAlign1)
            {
                (*outAudioPtr++) = (*audio1Ptr++);
            }
            else
            {
                (*outAudioPtr++) = 0;
            }
        }
        if (audio1 != NULL && reversePlay)
        {
            /* position at start of sample */
            audio1Ptr -= 2 * inputByteAlign1;
        }

        /* audio 2 */
        for (j = 3; j >= 0; j--)
        {
            if (audio2 != NULL && j < inputByteAlign2)
            {
                (*outAudioPtr++) = (*audio2Ptr++);
            }
            else
            {
                (*outAudioPtr++) = 0;
            }
        }
        if (audio2 != NULL && reversePlay)
        {
            /* position at start of sample */
            audio2Ptr -= 2 * inputByteAlign2;
        }
    }

}

static DVSStream* get_dvs_stream(DVSSink* sink, int streamId)
{
    int i;

    if (sink->videoStream.streamId == streamId)
    {
        return &sink->videoStream;
    }

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        if (sink->audioStream[i].streamId == streamId)
        {
            return &sink->audioStream[i];
        }
    }

    return NULL;
}

static TimecodeStream* get_timecode_stream(DVSSink* sink, int streamId)
{
    int i;

    for (i = 0; i < sink->numTimecodes; i++)
    {
        if (streamId == sink->timecodes[i].streamId)
        {
            return &sink->timecodes[i];
        }
    }
    return NULL;
}


static int dvs_register_listener(void* data, MediaSinkListener* listener)
{
    DVSSink* sink = (DVSSink*)data;

    sink->listener = listener;
    return 1;
}

static void dvs_unregister_listener(void* data, MediaSinkListener* listener)
{
    DVSSink* sink = (DVSSink*)data;

    if (sink->listener == listener)
    {
        sink->listener = NULL;
    }
}

static int dvs_accept_stream(void* data, const StreamInfo* streamInfo)
{
    DVSSink* sink = (DVSSink*)data;

    /* video, 25/29.97 fps, UYVY/YUV422 */
    if (streamInfo->type == PICTURE_STREAM_TYPE &&
        (streamInfo->format == UYVY_FORMAT ||
            streamInfo->format == UYVY_10BIT_FORMAT ||
            streamInfo->format == YUV422_FORMAT) &&
        memcmp(&streamInfo->frameRate, &sink->frameRate, sizeof(streamInfo->frameRate)) == 0)
    {
        if ((unsigned int)streamInfo->width == sink->rasterWidth &&
            ((unsigned int)streamInfo->height == sink->rasterHeight ||
                (streamInfo->height == 576 && sink->rasterHeight == 592 /* PALFF mode */) ||
                (streamInfo->height == 480 && sink->rasterHeight == 486)))
        {
            return 1;
        }
        else if (sink->fitVideo && streamInfo->format == UYVY_FORMAT) /* fitting UYVY_10BIT not supported */
        {
            return 1;
        }
        else if (sink->fitVideo && streamInfo->format == YUV422_FORMAT)
        {
            /* if the format is YUV422_FORMAT then a conversion must take place and the conversion uses a buffer
            with memory sufficient for the output image size. That is why the input image dimensions must be such that
            it doesn't overflow the buffer */
            int requiredSize = streamInfo->width * streamInfo->height * 2;
            int availableSize = sink->rasterWidth * sink->rasterHeight * 2;
            if (requiredSize <= availableSize)
            {
                return 1;
            }
        }
    }
    /* audio, 48kHz, mono, PCM */
    else if (streamInfo->type == SOUND_STREAM_TYPE &&
             memcmp(&streamInfo->samplingRate, &g_profAudioSamplingRate, sizeof(Rational)) == 0 &&
             streamInfo->numChannels == 1 &&
             streamInfo->format == PCM_FORMAT)
    {
        return 1;
    }
    else if (streamInfo->type == TIMECODE_STREAM_TYPE)
    {
        return 1;
    }
    return 0;
}

static int dvs_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    DVSSink* sink = (DVSSink*)data;
    DVSStream* dvsStream = NULL;

    /* this should've been checked, but we do it here anyway */
    if (!dvs_accept_stream(data, streamInfo))
    {
        fprintf(stderr, "Can't register a stream %d that is not accepted\n", streamId);
        return 0;
    }

    if (streamInfo->type == PICTURE_STREAM_TYPE)
    {
        if (sink->videoStream.data[0] != NULL)
        {
            /* already have video stream */
            return 0;
        }

        /* TODO: support 10-bit 720p input where the DVS alignment of 6 pixels doesn't match the
           48 pixel alignment required by v210 */
        if (streamInfo->format == UYVY_10BIT_FORMAT &&
            streamInfo->width % 6 != streamInfo->width % 48)
        {
            fprintf(stderr, "The width for 10-bit UYVY in stream %d is not yet supported in DVS sink\n", streamId);
            return 0;
        }

        sink->width = streamInfo->width;
        sink->height = streamInfo->height;

        if (streamInfo->aspectRatio.num > 0 && streamInfo->aspectRatio.den > 0)
        {
            sink->aspectRatio = streamInfo->aspectRatio;
        }
        else
        {
            sink->aspectRatio.num = 4;
            sink->aspectRatio.den = 3;
        }

        if (sink->osd != NULL && !sink->osdInitialised)
        {
            /* TODO: clean this up */

            StreamFormat format = streamInfo->format;
            if (streamInfo->format == YUV422_FORMAT ||
                (streamInfo->format == UYVY_10BIT_FORMAT && sink->depth8Bit))
            {
                /* input format will be converted to UYVY before applying overlay */
                ((StreamInfo*)streamInfo)->format = UYVY_FORMAT;
            }

            CHK_ORET(osd_initialise(sink->osd, streamInfo, &sink->aspectRatio));
            sink->osdInitialised = 1;

            ((StreamInfo*)streamInfo)->format = format;
        }

        dvsStream = &sink->videoStream;
        dvsStream->index = 0;
        dvsStream->streamId = streamId;
        dvsStream->streamInfo = *streamInfo;
        if (streamInfo->format == UYVY_FORMAT || streamInfo->format == YUV422_FORMAT)
        {
            dvsStream->dataSize[0] = streamInfo->width * streamInfo->height * 2;
        }
        else // UYVY_10BIT_FORMAT
        {
            dvsStream->dataSize[0] = (streamInfo->width + 5) / 6 * 16 * streamInfo->height;
        }
        dvsStream->dataSize[1] = dvsStream->dataSize[0];

        sink->workBuffer1Size = (sink->rasterWidth + 5) / 6 * 16 * sink->rasterHeight;
        CALLOC_ORET(sink->workBuffer1, unsigned char, sink->workBuffer1Size);
        sink->workBuffer2Size = sink->workBuffer1Size;
        CALLOC_ORET(sink->workBuffer2, unsigned char, sink->workBuffer2Size);

        if (((streamInfo->format == UYVY_FORMAT && sink->depth8Bit) ||
                (streamInfo->format == UYVY_10BIT_FORMAT && !sink->depth8Bit)) &&
            (unsigned int)streamInfo->width == sink->rasterWidth &&
            ((unsigned int)streamInfo->height == sink->rasterHeight ||
                    (streamInfo->height == 576 && sink->rasterHeight == 592) ||
                    (streamInfo->height == 480 && sink->rasterHeight == 486)))
        {
            /* the video stream input buffer is set to the output buffer because no
            transformations are required */
            if (sink->rasterHeight == 592 && streamInfo->height == 576)
            {
                // If raster is PALFF mode, skip first 16 lines
                if (sink->depth8Bit)
                {
                    dvsStream->data[0] = &sink->fifoBuffer[0].buffer[sink->rasterWidth * 16 * 2];
                    dvsStream->data[1] = &sink->fifoBuffer[1].buffer[sink->rasterWidth * 16 * 2];
                }
                else
                {
                    dvsStream->data[0] = &sink->fifoBuffer[0].buffer[(sink->rasterWidth + 5) / 6 * 16 * 16];
                    dvsStream->data[1] = &sink->fifoBuffer[1].buffer[(sink->rasterWidth + 5) / 6 * 16 * 16];
                }
            }
            else if (sink->rasterHeight == 486 && streamInfo->height == 480)
            {
                /* skip 4 lines at the top */
                if (sink->depth8Bit)
                {
                    dvsStream->data[0] = &sink->fifoBuffer[0].buffer[sink->rasterWidth * 4 * 2];
                    dvsStream->data[1] = &sink->fifoBuffer[1].buffer[sink->rasterWidth * 4 * 2];
                }
                else
                {
                    dvsStream->data[0] = &sink->fifoBuffer[0].buffer[(sink->rasterWidth + 5) / 6 * 16 * 4];
                    dvsStream->data[1] = &sink->fifoBuffer[1].buffer[(sink->rasterWidth + 5) / 6 * 16 * 4];
                }
            }
            else
            {
                dvsStream->data[0] = &sink->fifoBuffer[0].buffer[0];
                dvsStream->data[1] = &sink->fifoBuffer[1].buffer[0];
            }
            dvsStream->allocatedDataSize[0] = 0;
            dvsStream->allocatedDataSize[1] = 0;
            dvsStream->ownData = 0;
            dvsStream->requireFit = 0;
        }
        else
        {
            /* create a video stream input buffer separate from the output buffer because
            transformations (image format or fitting) are required */

            CALLOC_ORET(dvsStream->data[0], unsigned char, dvsStream->dataSize[0]);
            dvsStream->allocatedDataSize[0] = dvsStream->dataSize[0];
            CALLOC_ORET(dvsStream->data[1], unsigned char, dvsStream->dataSize[1]);
            dvsStream->allocatedDataSize[1] = dvsStream->dataSize[1];
            dvsStream->ownData = 1;

            if ((unsigned int)streamInfo->width != sink->rasterWidth ||
                ((unsigned int)streamInfo->height != sink->rasterHeight &&
                    !(streamInfo->height == 576 && sink->rasterHeight == 592) &&
                    !(streamInfo->height == 480 && sink->rasterHeight == 486)))
            {
                dvsStream->requireFit = 1;
            }
        }
    }
    else if (streamInfo->type == SOUND_STREAM_TYPE)
    {
        if (sink->numAudioStreams >= MAX_DVS_AUDIO_STREAMS)
        {
            return 0;
        }

        dvsStream = &sink->audioStream[sink->numAudioStreams];

        dvsStream->index = sink->numAudioStreams;

        dvsStream->streamId = streamId;
        dvsStream->streamInfo = *streamInfo;
        dvsStream->audio_byte_align = (streamInfo->bitsPerSample + 7) / 8;

        dvsStream->allocatedDataSize[0] = 1920 * dvsStream->audio_byte_align;
        dvsStream->allocatedDataSize[1] = dvsStream->allocatedDataSize[0];
        CALLOC_ORET(dvsStream->data[0], unsigned char, dvsStream->allocatedDataSize[0]);
        CALLOC_ORET(dvsStream->data[1], unsigned char, dvsStream->allocatedDataSize[1]);
        if (memcmp(&sink->frameRate, &g_ntscFrameRate, sizeof(sink->frameRate)) == 0) {
            dvsStream->dataSize[0] = 0; /* variable */
            dvsStream->dataSize[1] = 0; /* variable */
        } else {
            dvsStream->dataSize[0] = dvsStream->allocatedDataSize[0];
            dvsStream->dataSize[1] = dvsStream->allocatedDataSize[1];
        }

        dvsStream->ownData = 1;

        sink->numAudioStreams++;
    }
    else if (streamInfo->type == TIMECODE_STREAM_TYPE)
    {
        if (sink->numTimecodes + 1 >= MAX_TIMECODES)
        {
            return 0;
        }
        sink->timecodes[sink->numTimecodes].streamId = streamId;
        sink->timecodes[sink->numTimecodes].timecodeType = streamInfo->timecodeType;
        sink->timecodes[sink->numTimecodes].timecodeSubType = streamInfo->timecodeSubType;
        sink->numTimecodes++;
    }
    else
    {
        return 0;
    }

    return 1;
}

static int dvs_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    DVSSink* sink = (DVSSink*)data;

    if (get_timecode_stream(sink, streamId) == NULL &&
        get_dvs_stream(sink, streamId) == NULL)
    {
        return 0;
    }

    sink->reversePlay = frameInfo->reversePlay;
    return 1;
}

static int dvs_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    DVSSink* sink = (DVSSink*)data;
    DVSStream* dvsStream;
    TimecodeStream* timecodeStream;

    if ((timecodeStream = get_timecode_stream(sink, streamId)) != NULL)
    {
        if (sizeof(timecodeStream->timecode) != bufferSize)
        {
            fprintf(stderr, "Buffer size (%d) != timecode event data size (%d)\n", bufferSize,
                (int)sizeof(timecodeStream->timecode));
            return 0;
        }

        *buffer = (unsigned char*)&timecodeStream->timecode;
    }
    else if ((dvsStream = get_dvs_stream(sink, streamId)) != NULL)
    {
        if (dvsStream->ownData)
        {
            if (dvsStream->allocatedDataSize[sink->currentFifoBuffer] < bufferSize) {
                dvsStream->allocatedDataSize[sink->currentFifoBuffer] = bufferSize;
                SAFE_FREE(&dvsStream->data[sink->currentFifoBuffer]);
                CALLOC_ORET(dvsStream->data[sink->currentFifoBuffer], unsigned char,
                            dvsStream->allocatedDataSize[sink->currentFifoBuffer]);
            }
        }
        else if (dvsStream->dataSize[sink->currentFifoBuffer] != bufferSize)
        {
            fprintf(stderr, "Buffer size (%d) != data size (%d)\n", bufferSize, dvsStream->dataSize[sink->currentFifoBuffer]);
            return 0;
        }

        *buffer = dvsStream->data[sink->currentFifoBuffer];
    }
    else
    {
        fprintf(stderr, "Can't return DVS sink buffer for unknown stream %d \n", streamId);
        return 0;
    }

    return 1;
}

static int dvs_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    DVSSink* sink = (DVSSink*)data;
    DVSStream* dvsStream;
    TimecodeStream* timecodeStream;

    if ((timecodeStream = get_timecode_stream(sink, streamId)) != NULL)
    {
        if (buffer != (unsigned char*)&timecodeStream->timecode || sizeof(timecodeStream->timecode) != bufferSize)
        {
            fprintf(stderr, "Timecode frame buffer does not correspond to timecode event data\n");
            return 0;
        }

        timecodeStream->isPresent = 1;
    }
    else if ((dvsStream = get_dvs_stream(sink, streamId)) != NULL)
    {
        if (buffer != dvsStream->data[sink->currentFifoBuffer])
        {
            fprintf(stderr, "Frame buffer does not correspond to data\n");
            return 0;
        }

        if (dvsStream->streamInfo.type == SOUND_STREAM_TYPE)
        {
            dvsStream->dataSize[sink->currentFifoBuffer] = bufferSize;
        }
        else if (bufferSize != dvsStream->dataSize[sink->currentFifoBuffer])
        {
            fprintf(stderr, "Buffer size (%d) != data size (%d)\n", bufferSize, dvsStream->dataSize[sink->currentFifoBuffer]);
            return 0;
        }

        dvsStream->isPresent = 1;
    }
    else
    {
        fprintf(stderr, "Can't return DVS sink buffer for unknown stream %d \n", streamId);
        return 0;
    }


    return 1;
}

static int dvs_complete_frame(void* data, const FrameInfo* frameInfo)
{
    DVSSink* sink = (DVSSink*)data;
    int i;
    int vitcCount;
    int ltcCount;
    int maxTCCount = 24 * 60 * 60 * sink->roundedTimecodeBase;
    Timecode defaultTC = {0,0,0,0,0};
    int h;
    int w;
    DVSFifoBuffer* fifoBuffer = &sink->fifoBuffer[sink->currentFifoBuffer];
    unsigned char* activeBuffer = NULL;
    int activeBufferDepth8Bit = 1;


    if (!sink->videoStream.isPresent)
    {
        if (sink->videoStream.data[0] != NULL) /* is NULL if no video stream registered */
        {
            /* if video is not present than set to black */
            if (sink->videoStream.streamInfo.format == UYVY_10BIT_FORMAT)
            {
                fill_black(UYVY_10BIT_FORMAT, sink->width, sink->height, sink->videoStream.data[sink->currentFifoBuffer]);
                activeBufferDepth8Bit = 0;
            }
            else
            {
                fill_black(UYVY_FORMAT, sink->width, sink->height, sink->videoStream.data[sink->currentFifoBuffer]);
                activeBufferDepth8Bit = 1;
            }
            activeBuffer = sink->videoStream.data[sink->currentFifoBuffer];
        }
        else
        {
            activeBuffer = fifoBuffer->buffer;
            activeBufferDepth8Bit = sink->depth8Bit;
        }
    }
    else if (sink->videoStream.streamInfo.format == YUV422_FORMAT)
    {
         /* convert yuv422 to uyvy */
        if (!sink->videoStream.requireFit && sink->depth8Bit)
        {
            /* no more conversion required after this conversion */
            activeBuffer = fifoBuffer->buffer;
        }
        else
        {
            /* need to fit picture or convert to 10-bit */
            activeBuffer = sink->workBuffer1;
        }
        yuv422_to_uyvy_2(sink->width, sink->height, 0, sink->videoStream.data[sink->currentFifoBuffer], activeBuffer);
        activeBufferDepth8Bit = 1;
    }
    else if (sink->depth8Bit && sink->videoStream.streamInfo.format == UYVY_10BIT_FORMAT)
    {
        /* convert 10-bit UYVY to 8-bit */
        if (!sink->videoStream.requireFit)
        {
            /* no more conversion required after this conversion */
            activeBuffer = fifoBuffer->buffer;
        }
        else
        {
            /* need to fit picture */
            activeBuffer = sink->workBuffer1;
        }
        DitherFrameV210(activeBuffer, sink->videoStream.data[sink->currentFifoBuffer],
            sink->width * 2, (sink->width + 5) / 6 * 16,
            sink->width, sink->height);
        activeBufferDepth8Bit = 1;
    }
    else
    {
        activeBuffer = sink->videoStream.data[sink->currentFifoBuffer];
        activeBufferDepth8Bit = !(sink->videoStream.streamInfo.format == UYVY_10BIT_FORMAT);
    }


    /* reverse fields if reverse play */
    if (sink->videoStream.isPresent && frameInfo->reversePlay)
    {
        /* simple reverse field by moving frame down one line */
        if (activeBufferDepth8Bit)
        {
            memmove(activeBuffer + sink->width * 2,  activeBuffer, sink->width * (sink->height - 1) * 2);
        }
        else
        {
            memmove(activeBuffer + (sink->width + 5) / 6 * 16,  activeBuffer, (sink->width + 5) / 6 * 16 * (sink->height - 1));
        }
    }

    /* fit video */
    if (sink->videoStream.isPresent && sink->videoStream.requireFit)
    {
        /* only support 8-bit */
        CHK_ORET(activeBufferDepth8Bit);

        unsigned char* inData;
        unsigned char* outData;
        int imageYStart = 0;
        int imageXStart = 0;
        int rasterYOffset = 0;

        inData = activeBuffer;
        if (sink->depth8Bit)
        {
            outData = fifoBuffer->buffer;
            activeBuffer = fifoBuffer->buffer;
        }
        else
        {
            /* output to workBuffer2 because a 8- to 10-bit conversion is still required */
            outData = sink->workBuffer2;
            activeBuffer = sink->workBuffer2;
        }

        if (((int)sink->rasterHeight == 576 && (sink->height == 592 || sink->height == 608)) ||
             ((int)sink->rasterHeight == 592 && sink->height == 608))
        {
            /* skip the extra VBI lines in the input */
            inData += (sink->height - sink->rasterHeight) * sink->width * 2;
        }
        else if ((int)sink->rasterHeight == 486 && sink->height == 480)
        {
            rasterYOffset = 4;
        }
        else
        {
            /* centre image */
            imageYStart = ((int)sink->rasterHeight < sink->height) ? 0 : (sink->rasterHeight - sink->height) / 2;
            imageYStart -= imageYStart % 2; /* make it even */
        }
        imageXStart = ((int)sink->rasterWidth < sink->width) ? 0 : (sink->rasterWidth - sink->width) / 2;
        imageXStart -= imageXStart % 2; /* make it even */

        for (h = 0; h < (int)sink->rasterHeight; h++)
        {
            if (h < rasterYOffset)
            {
                /* fill with black the area space not filled by input data */
                for (w = 0; w < (int)sink->rasterWidth; w += 2)
                {
                    *outData++ = 0x80;
                    *outData++ = 0x10;
                    *outData++ = 0x80;
                    *outData++ = 0x10;
                }
            }
            else if ((h - rasterYOffset) >= imageYStart && (h - rasterYOffset) < imageYStart + sink->height)
            {
                for (w = 0; w < (int)sink->rasterWidth; w += 2)
                {
                    if (w >= imageXStart && w < imageXStart + sink->width)
                    {
                        /* copy input data to output data */
                        *outData++ = *inData++;
                        *outData++ = *inData++;
                        *outData++ = *inData++;
                        *outData++ = *inData++;
                    }
                    else
                    {
                        /* fill with black the area space not filled by input data */
                        *outData++ = 0x80;
                        *outData++ = 0x10;
                        *outData++ = 0x80;
                        *outData++ = 0x10;
                    }
                }

                if ((int)sink->rasterWidth < sink->width)
                {
                    /* skip input data that exceeds raster width */
                    inData += (sink->width - sink->rasterWidth) * 2;
                }
            }
            else
            {
                /* fill with black the area space not filled by input data */
                for (w = 0; w < (int)sink->rasterWidth; w += 2)
                {
                    *outData++ = 0x80;
                    *outData++ = 0x10;
                    *outData++ = 0x80;
                    *outData++ = 0x10;
                }
            }
        }
    }


    fifoBuffer->audio_data_size = 0;

    if (frameInfo->isRepeat || frameInfo->muteAudio || sink->muteAudio)
    {
        /* mute the audio */
        for (i = 0; i < sink->numAudioStreams; i += 2)
            memset(&fifoBuffer->buffer[sink->audioPairOffset[i / 2]], 0, sink->maxAudioDataSize);
    }
    else
    {
        unsigned int num_audio_samples = 0;

        /* set audio to zero if not present; set num audio samples for first stream that is present */
        for (i = 0; i < sink->numAudioStreams; i++)
        {
            if (!sink->audioStream[i].isPresent) {
                memset(sink->audioStream[i].data[sink->currentFifoBuffer], 0,
                       sink->audioStream[i].allocatedDataSize[sink->currentFifoBuffer]);
            } else {
                num_audio_samples = sink->audioStream[i].dataSize[sink->currentFifoBuffer] / sink->audioStream[i].audio_byte_align;
            }
        }

        fifoBuffer->audio_data_size = num_audio_samples * 4 * 2;

        if (num_audio_samples == 0) {
            /* mute the audio */
            for (i = 0; i < sink->numAudioStreams; i += 2)
                memset(&fifoBuffer->buffer[sink->audioPairOffset[i / 2]], 0, sink->maxAudioDataSize);
        } else {
            /* interleave (and reverse if reverse play) the audio and write to output buffer */
            for (i = 0; i < sink->numAudioStreams; i += 2) {
                if (sink->audioStream[i].isPresent &&
                    num_audio_samples != sink->audioStream[i].dataSize[sink->currentFifoBuffer] / sink->audioStream[i].audio_byte_align)
                {
                    /* different number of samples - don't use this stream and set the data to zero */
                    memset(sink->audioStream[i].data[sink->currentFifoBuffer], 0,
                           sink->audioStream[i].allocatedDataSize[sink->currentFifoBuffer]);
                }

                if (i + 1 < sink->numAudioStreams)
                {
                    if (sink->audioStream[i + 1].isPresent &&
                        num_audio_samples != sink->audioStream[i + 1].dataSize[sink->currentFifoBuffer] / sink->audioStream[i + 1].audio_byte_align)
                    {
                        /* different number of samples - don't use this stream and set the data to zero */
                        memset(sink->audioStream[i + 1].data[sink->currentFifoBuffer], 0,
                               sink->audioStream[i + 1].allocatedDataSize[sink->currentFifoBuffer]);
                    }

                    /* 2 tracks of audio */
                    interleave_audio(sink->audioStream[i].data[sink->currentFifoBuffer],
                                     num_audio_samples,
                                     sink->audioStream[i].audio_byte_align,
                                     sink->audioStream[i + 1].data[sink->currentFifoBuffer],
                                     num_audio_samples,
                                     sink->audioStream[i + 1].audio_byte_align,
                                     frameInfo->reversePlay,
                                     &fifoBuffer->buffer[sink->audioPairOffset[i / 2]]);
                }
                else
                {
                    /* single audio */
                    interleave_audio(sink->audioStream[i].data[sink->currentFifoBuffer],
                                     num_audio_samples,
                                     sink->audioStream[i].audio_byte_align,
                                     NULL,
                                     0,
                                     0,
                                     frameInfo->reversePlay,
                                     &fifoBuffer->buffer[sink->audioPairOffset[i / 2]]);
                }
            }
        }
    }

    /* set the VITC and LTC timecodes associated with the frame buffer */
    if (sink->sdiVITCSource == VITC_AS_SDI_VITC)
    {
        vitcCount = get_timecode_count(sink, SOURCE_TIMECODE_TYPE, VITC_SOURCE_TIMECODE_SUBTYPE, 0);
        fifoBuffer->vitcTimecode = get_timecode(sink, SOURCE_TIMECODE_TYPE, VITC_SOURCE_TIMECODE_SUBTYPE, &defaultTC);
    }
    else if (sink->sdiVITCSource == LTC_AS_SDI_VITC)
    {
        vitcCount = get_timecode_count(sink, SOURCE_TIMECODE_TYPE, LTC_SOURCE_TIMECODE_SUBTYPE, 0);
        fifoBuffer->vitcTimecode = get_timecode(sink, SOURCE_TIMECODE_TYPE, LTC_SOURCE_TIMECODE_SUBTYPE, &defaultTC);
    }
    else
    {
        vitcCount = frameInfo->position % maxTCCount;
        fifoBuffer->vitcTimecode = get_timecode_from_count(sink, frameInfo->position % maxTCCount);
    }
    fifoBuffer->vitcCount = int_to_dvs_tc(sink, vitcCount);
    ltcCount = get_timecode_count(sink, SOURCE_TIMECODE_TYPE, LTC_SOURCE_TIMECODE_SUBTYPE, 0);
    fifoBuffer->ltcCount = int_to_dvs_tc(sink, ltcCount);


    /* set the extra VITC timecodes associated with the frame buffer */
    if (sink->extraSDIVITCSource == VITC_AS_SDI_VITC)
    {
        fifoBuffer->extraVITCTimecode = get_timecode(sink, SOURCE_TIMECODE_TYPE, VITC_SOURCE_TIMECODE_SUBTYPE, &defaultTC);
    }
    else if (sink->extraSDIVITCSource == LTC_AS_SDI_VITC)
    {
        fifoBuffer->extraVITCTimecode = get_timecode(sink, SOURCE_TIMECODE_TYPE, LTC_SOURCE_TIMECODE_SUBTYPE, &defaultTC);
    }
    else if (sink->extraSDIVITCSource == COUNT_AS_SDI_VITC)
    {
        fifoBuffer->extraVITCTimecode = get_timecode_from_count(sink, frameInfo->position % maxTCCount);
    }


    /* add OSD to frame */
    if (sink->osd != NULL && sink->osdInitialised) /* only support OSD for 8-bit video */
    {
        if (!osd_add_to_image(sink->osd, frameInfo, activeBuffer + sink->videoOffset,
            sink->rasterWidth, sink->rasterHeight - sink->videoOffset))
        {
            fprintf(stderr, "Failed to add OSD to frame\n");
            /* continue anyway */
        }
    }

    if (!sink->depth8Bit && activeBufferDepth8Bit)
    {
        /* convert 8-bit UYVY to 10-bit */
        ConvertFrame8toV210(fifoBuffer->buffer, activeBuffer,
            (sink->rasterWidth + 5) / 6 * 16, sink->rasterWidth * 2,
            sink->rasterWidth, sink->rasterHeight);
    }



    PTHREAD_MUTEX_LOCK(&sink->frameInfosMutex);

    /* send frame to dvc fifo buffer for display */
    display_on_sv_fifo(sink, fifoBuffer);

    /* increment the current fifo buffer */
    sink->currentFifoBuffer = (sink->currentFifoBuffer + 1) % 2;
    fifoBuffer = &sink->fifoBuffer[sink->currentFifoBuffer];

    /* record info about frame */
    sink->frameInfos[sink->frameInfosCount % MAX_DISPLAY_DELAY] = *frameInfo;
    sink->frameInfosCount++;

    PTHREAD_MUTEX_UNLOCK(&sink->frameInfosMutex);



    /* start the fifo if not already started */
    if (!sink->fifoStarted)
    {
        sv_handle* sv = sink->sv;
        sv_fifo_info info;

#if defined(AV_SYNC_HACK)
        /* reset the sync mode to ensure we get av sync - without this a 1 field offset can occur */
        sv_info status_info;
        sv_status(sink->sv, &status_info);
        SV_CHK_ORET(sv_sync(sink->sv, status_info.sync));
#endif

        /* initialise the tick and dropped count */
        SV_CHK_ORET(sv_fifo_status(sv, sink->svfifo, &info));
        sink->lastFrameTick = info.displaytick / 2; /* each tick is 1 field */
        sink->lastDropped = info.dropped;

        /* Start the fifo */
        SV_CHK_ORET(sv_fifo_start(sv, sink->svfifo));

        sink->fifoStarted = 1;
    }

    reset_streams(sink);

    return 1;
}

static void dvs_cancel_frame(void* data)
{
    DVSSink* sink = (DVSSink*)data;
    /* TODO */
    /* do nothing? */

    reset_streams(sink);
}

static OnScreenDisplay* dvs_get_osd(void* data)
{
    DVSSink* sink = (DVSSink*)data;

    return sink->osd;
}

static int dvs_get_buffer_state(void* data, int* numBuffers, int* numBuffersFilled)
{
    DVSSink* sink = (DVSSink*)data;

    *numBuffers = sink->numBuffers;
    *numBuffersFilled = sink->numBuffersFilled;

    return 1;
}

static int dvs_mute_audio(void* data, int mute)
{
    DVSSink* sink = (DVSSink*)data;

    if (mute < 0)
    {
        sink->muteAudio = !sink->muteAudio;
    }
    else
    {
        sink->muteAudio = mute;
    }

    return 1;
}

static void dvs_close(void* data)
{
    DVSSink* sink = (DVSSink*)data;
    int i;

    if (data == NULL)
    {
        return;
    }

    if (sink->osd != NULL)
    {
        osd_free(sink->osd);
        sink->osd = NULL;
    }

    sink->stopped = 1;
    join_thread(&sink->dvsStateInfoThreadId, NULL, NULL);


    /* display a blank frame before we leave */
    if (sink->sv != NULL && sink->svfifo != NULL)
    {
        memset(sink->fifoBuffer[0].buffer, 0, sink->fifoBuffer[0].bufferSize);
        if (sink->depth8Bit)
        {
            fill_black(UYVY_FORMAT, sink->rasterWidth, sink->rasterHeight, sink->fifoBuffer[0].buffer);
        }
        else
        {
            fill_black(UYVY_10BIT_FORMAT, sink->rasterWidth, sink->rasterHeight, sink->fifoBuffer[0].buffer);
        }
        sink->fifoBuffer[0].vitcCount = 0;
        sink->fifoBuffer[0].ltcCount = 0;
        memset(&sink->fifoBuffer[0].vitcTimecode, 0, sizeof(sink->fifoBuffer[0].vitcTimecode));
        memset(&sink->fifoBuffer[0].extraVITCTimecode, 0, sizeof(sink->fifoBuffer[0].extraVITCTimecode));

        PTHREAD_MUTEX_LOCK(&sink->frameInfosMutex);
        display_on_sv_fifo(sink, &sink->fifoBuffer[0]);
        PTHREAD_MUTEX_UNLOCK(&sink->frameInfosMutex);
    }

    SAFE_FREE(&sink->workBuffer1);
    SAFE_FREE(&sink->workBuffer2);

    if (sink->videoStream.ownData)
    {
        SAFE_FREE(&sink->videoStream.data[0]);
        SAFE_FREE(&sink->videoStream.data[1]);
    }

    for (i = 0; i < sink->numAudioStreams; i++)
    {
        if (sink->audioStream[i].ownData)
        {
            SAFE_FREE(&sink->audioStream[i].data[0]);
            SAFE_FREE(&sink->audioStream[i].data[1]);
        }
    }

    SAFE_FREE(&sink->fifoBuffer[0].buffer);
    SAFE_FREE(&sink->fifoBuffer[1].buffer);

    if (sink->sv != NULL)
    {
        if (sink->svfifo != NULL)
        {
            sv_fifo_wait(sink->sv, sink->svfifo);
            sv_fifo_free(sink->sv, sink->svfifo);
        }
        sv_close(sink->sv);
        sink->sv = NULL;
    }

    destroy_mutex(&sink->dvsFifoMutex);
    destroy_mutex(&sink->frameInfosMutex);

    SAFE_FREE(&sink);
}

static int dvs_reset_or_close(void* data)
{
    /* reset is not supported/desirable, so we close instead */
    dvs_close(data);
    return 2;
}

static void dvs_refresh_required(void* data)
{
    DVSSink* sink = (DVSSink*)data;

    msl_refresh_required(sink->listener);
}

static void dvs_osd_screen_changed(void* data, OSDScreen screen)
{
    DVSSink* sink = (DVSSink*)data;

    msl_osd_screen_changed(sink->listener, screen);
}



int dvs_card_is_available(int card, int channel)
{
    sv_handle* sv;
    int selectedCard;
    int selectedChannel;

    if (!open_dvs_card(card, channel, 0, &sv, &selectedCard, &selectedChannel))
    {
        return 0;
    }

    sv_close(sv);
    return 1;
}

int dvs_open(int dvsCard, int dvsChannel, SDIVITCSource sdiVITCSource, SDIVITCSource extraSDIVITCSource,
    int numBuffers, int disableOSD, int fitVideo, DVSSink** sink)
{
    DVSSink* newSink;
    int i;
    int selectedCard;
    int selectedChannel;
    sv_info status_info;
    sv_fifo_info fifo_info;


    if (numBuffers > 0 && numBuffers < MIN_NUM_DVS_FIFO_BUFFERS)
    {
        ml_log_error("Number of DVS buffers (%d) is less than the minimum (%d)\n", numBuffers, MIN_NUM_DVS_FIFO_BUFFERS);
        return 0;
    }

    CALLOC_ORET(newSink, DVSSink, 1);

    newSink->sdiVITCSource = sdiVITCSource;
    newSink->extraSDIVITCSource = extraSDIVITCSource;
    newSink->fitVideo = 1;
    newSink->videoStream.streamId = -1;

    newSink->mediaSink.data = newSink;
    newSink->mediaSink.register_listener = dvs_register_listener;
    newSink->mediaSink.unregister_listener = dvs_unregister_listener;
    newSink->mediaSink.accept_stream = dvs_accept_stream;
    newSink->mediaSink.register_stream = dvs_register_stream;
    newSink->mediaSink.accept_stream_frame = dvs_accept_stream_frame;
    newSink->mediaSink.get_stream_buffer = dvs_get_stream_buffer;
    newSink->mediaSink.receive_stream_frame = dvs_receive_stream_frame;
    newSink->mediaSink.complete_frame = dvs_complete_frame;
    newSink->mediaSink.cancel_frame = dvs_cancel_frame;
    newSink->mediaSink.get_osd = dvs_get_osd;
    newSink->mediaSink.get_buffer_state = dvs_get_buffer_state;
    newSink->mediaSink.mute_audio = dvs_mute_audio;
    newSink->mediaSink.reset_or_close = dvs_reset_or_close;
    newSink->mediaSink.close = dvs_close;

    if (!disableOSD)
    {
        /* create a default OSD */
        CHK_ORET(osdd_create(&newSink->osd));

        newSink->osdListener.data = newSink;
        newSink->osdListener.refresh_required = dvs_refresh_required;
        newSink->osdListener.osd_screen_changed = dvs_osd_screen_changed;

        osd_set_listener(newSink->osd, &newSink->osdListener);
    }

    CHK_OFAIL(init_mutex(&newSink->frameInfosMutex));



    /* open the DVS card */

    if (!open_dvs_card(dvsCard, dvsChannel, 1, &newSink->sv, &selectedCard, &selectedChannel))
    {
        ml_log_error("No DVS card is available\n");
        goto fail;
    }

    /* get DVS settings */

    SV_CHK_OFAIL(sv_status(newSink->sv, &status_info));
    ml_log_info("DVS card [card=%d,channel=%d] display raster is %dx%di, mode is 0x%x\n", selectedCard, selectedChannel, status_info.xsize, status_info.ysize, status_info.config);

    CHK_OFAIL(status_info.nbit == 8 || status_info.nbit == 10);
    newSink->depth8Bit = (status_info.nbit == 8);

    if ((status_info.config & SV_MODE_MASK) == SV_MODE_PAL ||
        (status_info.config & SV_MODE_MASK) == SV_MODE_SMPTE274_25I ||
        (status_info.config & SV_MODE_MASK) == SV_MODE_SMPTE274_25sF)
    {
        newSink->frameRate.num = 25;
        newSink->frameRate.den = 1;
        newSink->roundedTimecodeBase = 25;
    }
    else if ((status_info.config & SV_MODE_MASK) == SV_MODE_NTSC ||
             (status_info.config & SV_MODE_MASK) == SV_MODE_SMPTE274_29I ||
             (status_info.config & SV_MODE_MASK) == SV_MODE_SMPTE274_29sF)
    {
        newSink->frameRate.num = 30000;
        newSink->frameRate.den = 1001;
        newSink->roundedTimecodeBase = 30;
    }
    else if ((status_info.config & SV_MODE_MASK) == SV_MODE_SMPTE296_50P)
    {
        newSink->frameRate.num = 50;
        newSink->frameRate.den = 1;
        newSink->roundedTimecodeBase = 50;
    }
    else if ((status_info.config & SV_MODE_MASK) == SV_MODE_SMPTE296_59P)
    {
        newSink->frameRate.num = 60000;
        newSink->frameRate.den = 1001;
        newSink->roundedTimecodeBase = 60;
    }
    else
    {
        ml_log_error("DVS video mode not supported\n");
        goto fail;
    }

    newSink->rasterWidth = status_info.xsize;
    newSink->rasterHeight = status_info.ysize;

    if (newSink->depth8Bit)
    {
        newSink->videoDataSize = newSink->rasterWidth * newSink->rasterHeight * 2;
    }
    else
    {
        newSink->videoDataSize = (newSink->rasterWidth + 5) / 6 * 16 * newSink->rasterHeight;
    }
    newSink->maxAudioDataSize = 1920 * 2 * 4; /* 48k Hz for 25 fps, 2 channels, 32 bit; more than 29.97fps */

    newSink->audioPairOffset[0] = newSink->videoDataSize;
    for (i = 2; i < MAX_DVS_AUDIO_STREAMS; i += 2)
    {
        newSink->audioPairOffset[i / 2] = newSink->audioPairOffset[(i / 2) - 1] + 0x4000;
    }

    newSink->fifoBuffer[0].bufferSize = newSink->videoDataSize + (int)((MAX_DVS_AUDIO_STREAMS + 1) / 2) * 0x4000;
    newSink->fifoBuffer[1].bufferSize = newSink->fifoBuffer[0].bufferSize;
    MEM_ALLOC_OFAIL(newSink->fifoBuffer[0].buffer, valloc, unsigned char, newSink->fifoBuffer[0].bufferSize);
    memset(newSink->fifoBuffer[0].buffer, 0, newSink->fifoBuffer[0].bufferSize);
    MEM_ALLOC_OFAIL(newSink->fifoBuffer[1].buffer, valloc, unsigned char, newSink->fifoBuffer[1].bufferSize);
    memset(newSink->fifoBuffer[1].buffer, 0, newSink->fifoBuffer[1].bufferSize);

    if (newSink->depth8Bit)
    {
        fill_black(UYVY_FORMAT, newSink->rasterWidth, newSink->rasterHeight, newSink->fifoBuffer[0].buffer);
        fill_black(UYVY_FORMAT, newSink->rasterWidth, newSink->rasterHeight, newSink->fifoBuffer[1].buffer);
    }
    else
    {
        fill_black(UYVY_10BIT_FORMAT, newSink->rasterWidth, newSink->rasterHeight, newSink->fifoBuffer[0].buffer);
        fill_black(UYVY_10BIT_FORMAT, newSink->rasterWidth, newSink->rasterHeight, newSink->fifoBuffer[1].buffer);
    }

    // If raster is PALFF mode, then set start offset to after first 16 lines
    if (newSink->rasterHeight == 592)
    {
        newSink->palFFMode = 1;
        if (newSink->depth8Bit)
        {
            newSink->videoOffset = newSink->rasterWidth * 16 * 2;
        }
        else
        {
            newSink->videoOffset = (newSink->rasterWidth + 5) / 6 * 16 * 16;
        }
    }


    SV_CHK_OFAIL( sv_fifo_init( newSink->sv,
                            &newSink->svfifo,   // FIFO handle
                            0,                  // jack (0 for default output fifo)
                            0,                  // bShared (obsolete, must be 0)
                            SV_FIFO_DMA_ON,     // dma
                            0,                  // flagbase
                            numBuffers) );      // nFrames (0 means use maximum)


    SV_CHK_OFAIL(sv_fifo_status(newSink->sv, newSink->svfifo, &fifo_info));
    newSink->numBuffers = fifo_info.nbuffers;
    newSink->numBuffersFilled = fifo_info.availbuffers;


    CHK_OFAIL(init_mutex(&newSink->dvsFifoMutex));


    /* pre-fill the fifo with black frames */

    PTHREAD_MUTEX_LOCK(&newSink->frameInfosMutex);

    /* the - 2 below is because the dvs card complained when sending the last frames that
       the fifo has to be started */
    for (i = 0; i < fifo_info.nbuffers - 2; i++)
    {
#if 1
        display_on_sv_fifo(newSink, &newSink->fifoBuffer[0]);
#else
        // Since SDK v4.0.2.x, some FIFO synchronisation must be achieved before calling sv_fifo_get_buffer,
        // by calling either sv_stop(), sv_black() or sv_videomode().
        // So advantageously use sv_black() instead of display_on_sv_fifo().
        sv_black(newSink->sv);
#endif
    }

    PTHREAD_MUTEX_UNLOCK(&newSink->frameInfosMutex);



    CHK_OFAIL(create_joinable_thread(&newSink->dvsStateInfoThreadId, get_dvs_state_info_thread, newSink));


    *sink = newSink;
    return 1;

fail:
    dvs_close(newSink);
    return 0;
}

MediaSink* dvs_get_media_sink(DVSSink* sink)
{
    return &sink->mediaSink;
}

SDIRaster dvs_get_raster(DVSSink* sink)
{
    if (sink->rasterWidth == 720 && sink->rasterHeight == 576)
        return SDI_SD_625_RASTER;
    else if (sink->rasterWidth == 720 && sink->rasterHeight == 486)
        return SDI_SD_525_RASTER;
    else if (sink->rasterWidth == 1920 && sink->rasterHeight == 1080)
        return SDI_HD_1080_RASTER;

    return SDI_OTHER_RASTER;
}

void dvs_close_card(DVSSink* sink)
{
    if (sink != NULL && sink->sv != NULL)
    {
        // NOTE: not calling sv_fifo_free because it was found to block when frames are still present in the fifo
        // (is this caused by sv_fifo_status calls in the separate dvs state info thread?)

        sv_close(sink->sv);
    }
}



#endif /* #if defined(HAVE_DVS) */


