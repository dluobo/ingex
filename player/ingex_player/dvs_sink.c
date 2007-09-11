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
#include "utils.h"
#include "logging.h"
#include "macros.h"


#if !defined(HAVE_DVS)

int dvs_open(SDIVITCSource sdiVITCSource, int extraSDIVITCSource, int numBuffers, int disableOSD, int fitVideo, DVSSink** sink)
{
    return 0;
}

MediaSink* dvs_get_media_sink(DVSSink* sink)
{
    return NULL;
}

void dvs_close_card(DVSSink* sink)
{
}

int dvs_card_is_available()
{
    return 0;
}


#else

#include "dvs_clib.h"
#include "dvs_fifo.h"


#define MAX_DVS_CARDS               4

#define MAX_DVS_AUDIO_STREAMS       4

/* we assume that a VITC and LTC are within this maximum of timecode streams */
#define MAX_TIMECODES               32

/* maximum delay before a frame is displayed */
#define MAX_DISPLAY_DELAY           64  

/* interval for polling the DVS card fifo for changes */
#define DVS_POLL_INTERVAL           10

/* 23:59:59:24 + 1 */
#define MAX_TIMECODE_COUNT          2160000


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

    int ownData; /* delete here if data is not a reference into the buffer */
    unsigned char* data;
    unsigned int dataSize;
    
    int requireFit; /* true if the stream dimensions != dvs frame dimensions */
    
    int isPresent;
} DVSStream;

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
    
    unsigned int rasterWidth;
    unsigned int rasterHeight;
    unsigned int videoDataSize;
    unsigned int videoOffset;
    unsigned int audioDataSize;
    unsigned int audio12Offset;
    unsigned int audio34Offset;
    
    /* data sent to DVS card: A/V buffer and timecodes */
    unsigned char* buffer;
    unsigned int bufferSize;
    int vitcCount;
    int ltcCount;
    Timecode vitcTimecode;
    Timecode extraVITCTimecode;
    
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
};




static inline void write_bcd(unsigned char* target, int val)
{
    *target = ((val / 10) << 4) + (val % 10);
}

static int int_to_dvs_tc(int tc)
{
    // e.g. turn 1020219 into hex 0x11200819
    int fr = tc % 25;
    int hr = (int)(tc / (60*60*25));
    int mi = (int)((tc - (hr * 60*60*25)) / (60 * 25));
    int se = (int)((tc - (hr * 60*60*25) - (mi * 60*25)) / 25);

    unsigned char raw_tc[4];
    write_bcd(raw_tc + 0, fr);
    write_bcd(raw_tc + 1, se);
    write_bcd(raw_tc + 2, mi);
    write_bcd(raw_tc + 3, hr);
    int result = *(int *)(raw_tc);
    return result;
}

static Timecode get_timecode_from_count(int64_t count)
{
    Timecode tc;
    
    tc.isDropFrame = 0;
    tc.hour = count / (60 * 60 * 25);
    tc.min = (count % (60 * 60 * 25)) / (60 * 25);
    tc.sec = ((count % (60 * 60 * 25)) % (60 * 25)) / 25;
    tc.frame = ((count % (60 * 60 * 25)) % (60 * 25)) % 25;
    
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
            return sink->timecodes[i].timecode.hour * 60 * 60 * 25 +
                sink->timecodes[i].timecode.min * 60 * 25 +
                sink->timecodes[i].timecode.sec * 25 +
                sink->timecodes[i].timecode.frame;
        }
    }
    
    return theDefault;
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



/* TODO: should SV_CHECK exit or just return 0? */
static int display_on_sv_fifo(DVSSink* sink)
{
    // write frame to sv fifo
    sv_fifo_buffer      *pbuffer;
    sv_handle           *sv = sink->sv;

    // Get sv memmory buffer
    SV_CHK_ORET(sv_fifo_getbuffer(sv, sink->svfifo, &pbuffer, NULL, 0));

    pbuffer->dma.addr = (char*)sink->buffer;
    pbuffer->dma.size = sink->bufferSize;

    /* set VITC */
    if (sink->palFFMode)
    {
        /* write timecode lines directly */
        
        /* TODO: writeVITC assumes line width = 720 */

	if (sink->extraSDIVITCSource != 0)
        {
            /* extra VITC timecodes */

            /* line 0 */
            writeVITC(sink->extraVITCTimecode.hour, sink->extraVITCTimecode.min,
                sink->extraVITCTimecode.sec, sink->extraVITCTimecode.frame,
                sink->buffer);

            /* line 4 - backup copy */
            writeVITC(sink->extraVITCTimecode.hour, sink->extraVITCTimecode.min,
                sink->extraVITCTimecode.sec, sink->extraVITCTimecode.frame,
                sink->buffer + 4 * 2 * sink->rasterWidth);
        }
        
        /* line 8 */
        writeVITC(sink->vitcTimecode.hour, sink->vitcTimecode.min, 
            sink->vitcTimecode.sec, sink->vitcTimecode.frame, 
            sink->buffer + 8 * 2 * sink->rasterWidth);
        
        /* line 12 - backup copy */
        writeVITC(sink->vitcTimecode.hour, sink->vitcTimecode.min, 
            sink->vitcTimecode.sec, sink->vitcTimecode.frame, 
            sink->buffer + 12 * 2 * sink->rasterWidth);
        
    }
    else
    {
        // doesn't appear to work i.e. get output over SDI
        pbuffer->timecode.vitc_tc = sink->vitcCount;
        pbuffer->timecode.vitc_tc2 = sink->vitcCount | 0x80000000;       // set high bit for second field timecode
    }
    
    /* set LTC */
    pbuffer->timecode.ltc_tc = sink->ltcCount;

    // send the filled in buffer to the hardware
    SV_CHK_ORET(sv_fifo_putbuffer(sv, sink->svfifo, pbuffer, NULL));

    return 1;
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

static void interleave_audio(unsigned char* audio1, 
    int inputByteAlign1, 
    unsigned char* audio2, 
    int inputByteAlign2,
    int reversePlay,
    unsigned char* outBuffer)
{
    unsigned char* audio1Ptr;
    unsigned char* audio2Ptr;
    unsigned char* outAudioPtr = outBuffer;
    int i;
    int j;

    assert(audio1 == NULL || (inputByteAlign1 >= 1 && inputByteAlign1 <= 4));
    assert(audio2 == NULL || (inputByteAlign2 >= 1 && inputByteAlign2 <= 4));

    if (reversePlay)
    {
        /* position at end of data */
        audio1Ptr = audio1 + (1920 - 1) * inputByteAlign1;
        audio2Ptr = audio2 + (1920 - 1) * inputByteAlign2;
    }
    else
    {
        audio1Ptr = audio1;
        audio2Ptr = audio2;
    }

    for (i = 0; i < 1920; i++)
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
    
    /* video, 25 fps, UYVY */
    if (streamInfo->type == PICTURE_STREAM_TYPE &&
        streamInfo->format == UYVY_FORMAT &&
        memcmp(&streamInfo->frameRate, &g_palFrameRate, sizeof(Rational)) == 0 &&
        (sink->fitVideo ||
            ((unsigned int)streamInfo->width == sink->rasterWidth &&
                ((unsigned int)streamInfo->height == sink->rasterHeight || 
                    (streamInfo->height == 576 && sink->rasterHeight == 592)))))
    {
        return 1;
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
        if (sink->videoStream.data != NULL)
        {
            /* already have video stream */
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
            CHK_ORET(osd_initialise(sink->osd, streamInfo, &sink->aspectRatio));
            sink->osdInitialised = 1;
        }

        dvsStream = &sink->videoStream;
        dvsStream->index = 0;
        dvsStream->streamId = streamId;
        dvsStream->streamInfo = *streamInfo;
        dvsStream->dataSize = streamInfo->width * streamInfo->height * 2;
        if ((unsigned int)streamInfo->width == sink->rasterWidth &&
            ((unsigned int)streamInfo->height == sink->rasterHeight || 
                    (streamInfo->height == 576 && sink->rasterHeight == 592)))
        {
            if (sink->rasterHeight == 592 && streamInfo->height == 576)
            {
                // If raster is PALFF mode, skip first 16 lines
                dvsStream->data = &sink->buffer[sink->rasterWidth * 16 * 2];
            }
            else
            {
                dvsStream->data = &sink->buffer[0];
            }
            dvsStream->ownData = 0;
            dvsStream->requireFit = 0;
        }
        else
        {
            if ((dvsStream->data = (unsigned char*)calloc(dvsStream->dataSize, sizeof(unsigned char))) == NULL)
            {
                fprintf(stderr, "Failed to allocate memory\n");
                return 0;
            }
            dvsStream->ownData = 1;
            dvsStream->requireFit = 1;
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

        dvsStream->dataSize = 1920 * ((streamInfo->bitsPerSample + 7) / 8);
        if ((dvsStream->data = (unsigned char*)calloc(dvsStream->dataSize, sizeof(unsigned char))) == NULL)
        {
            fprintf(stderr, "Failed to allocate memory\n");
            return 0;
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
        if (dvsStream->dataSize != bufferSize)
        {
            fprintf(stderr, "Buffer size (%d) != data size (%d)\n", bufferSize, dvsStream->dataSize);
            return 0;
        }
        
        *buffer = dvsStream->data;
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
        if (buffer != dvsStream->data || bufferSize != dvsStream->dataSize)
        {
            fprintf(stderr, "Frame buffer does not correspond to data\n");
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
    unsigned int offset = sink->audio12Offset;
    int i;
    int vitcCount;
    int ltcCount; 
    Timecode defaultTC = {0,0,0,0,0};
    int h;
    int w;


    /* if video is not present than set to black */
    if (!sink->videoStream.isPresent)
    {
        for (i = 0; i < (int)(sink->height * sink->width * 2); i += 4) 
        {
            sink->videoStream.data[i + 0] = 0x80;
            sink->videoStream.data[i + 1] = 0x10;
            sink->videoStream.data[i + 2] = 0x80;
            sink->videoStream.data[i + 3] = 0x10;
        }
    }
    
    /* reverse fields if reverse play */    
    if (sink->videoStream.isPresent && frameInfo->reversePlay)
    {
        /* simple reverse field by moving frame down one line */
        memmove(sink->videoStream.data + sink->width * 2,
            sink->videoStream.data,
            sink->width * (sink->height - 1) * 2);
    }
    
    /* fit video */
    if (sink->videoStream.requireFit)
    {
        int height;
        unsigned char* inData = sink->videoStream.data;
        unsigned char* outData = sink->buffer;
        
        if (sink->rasterHeight == 592) /* PALFF mode */
        {
            /* skip the first 16 lines */
            outData += 16 * sink->rasterWidth * 2;
            height = 576;
        }
        else
        {
            height = sink->rasterHeight;
        }

        for (h = 0; h < height; h++)
        {
            if (h < sink->height)
            {
                for (w = 0; w < (int)(sink->rasterWidth * 2); w += 4) 
                {
                    if (w < sink->width * 2)
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
                if (w < sink->width * 2)
                {
                    /* skip input data that exceeds raster width */ 
                    inData += sink->width * 2 - w;
                }
            }
            else
            {
                /* fill with black the area space not filled by input data */
                for (w = 0; w < (int)(sink->rasterWidth * 2); w += 4) 
                {
                    *outData++ = 0x80;
                    *outData++ = 0x10;
                    *outData++ = 0x80;
                    *outData++ = 0x10;
                }
            }
        }
    }
    
    /* if the frame is a repeat then mute the audio */
    if (frameInfo->isRepeat)
    {
        memset(&sink->buffer[sink->audio12Offset], 0, sink->audioDataSize);
        memset(&sink->buffer[sink->audio34Offset], 0, sink->audioDataSize);
    }
    else
    {
        /* set audio to zero if not present */
        for (i = 0; i < sink->numAudioStreams; i++)
        {
            if (!sink->audioStream[i].isPresent)
            {
                memset(sink->audioStream[i].data, 0, sink->audioStream[i].dataSize);
            }
        }

    
        /* interleave (and reverse if reverse play) the audio and write to output buffer */
        for (i = 0; i < sink->numAudioStreams; i += 2)
        {
            if (i + 1 < sink->numAudioStreams)
            {
                /* 2 tracks of audio */
                interleave_audio(
                    sink->audioStream[i].data, 
                    (sink->audioStream[i].streamInfo.bitsPerSample + 7) / 8,
                    sink->audioStream[i + 1].data, 
                    (sink->audioStream[i + 1].streamInfo.bitsPerSample + 7) / 8,
                    frameInfo->reversePlay,
                    &sink->buffer[offset]);
            }
            else
            {
                /* single audio */
                interleave_audio(
                    sink->audioStream[i].data, 
                    (sink->audioStream[i].streamInfo.bitsPerSample + 7) / 8,
                    NULL,
                    0,
                    frameInfo->reversePlay,
                    &sink->buffer[offset]);
            }
            offset += 0x4000;
        }
    }

    /* set the VITC and LTC timecodes associated with the frame buffer */
    if (sink->sdiVITCSource == VITC_AS_SDI_VITC)
    {
        vitcCount = get_timecode_count(sink, SOURCE_TIMECODE_TYPE, VITC_SOURCE_TIMECODE_SUBTYPE, 0);
        sink->vitcTimecode = get_timecode(sink, SOURCE_TIMECODE_TYPE, VITC_SOURCE_TIMECODE_SUBTYPE, &defaultTC);
    }
    else if (sink->sdiVITCSource == LTC_AS_SDI_VITC)
    {
        vitcCount = get_timecode_count(sink, SOURCE_TIMECODE_TYPE, LTC_SOURCE_TIMECODE_SUBTYPE, 0);
        sink->vitcTimecode = get_timecode(sink, SOURCE_TIMECODE_TYPE, LTC_SOURCE_TIMECODE_SUBTYPE, &defaultTC);
    }
    else
    {
        vitcCount = frameInfo->position % MAX_TIMECODE_COUNT;
        sink->vitcTimecode = get_timecode_from_count(frameInfo->position % MAX_TIMECODE_COUNT);
    }
    sink->vitcCount = int_to_dvs_tc(vitcCount);
    ltcCount = get_timecode_count(sink, SOURCE_TIMECODE_TYPE, LTC_SOURCE_TIMECODE_SUBTYPE, 0); 
    sink->ltcCount = int_to_dvs_tc(ltcCount);
  
 
    /* set the extra VITC timecodes associated with the frame buffer */
    if (sink->extraSDIVITCSource == VITC_AS_SDI_VITC) 
    {
        sink->extraVITCTimecode = get_timecode(sink, SOURCE_TIMECODE_TYPE, VITC_SOURCE_TIMECODE_SUBTYPE, &defaultTC);
    }
    else if (sink->extraSDIVITCSource == LTC_AS_SDI_VITC)
    {
        sink->extraVITCTimecode = get_timecode(sink, SOURCE_TIMECODE_TYPE, LTC_SOURCE_TIMECODE_SUBTYPE, &defaultTC);
    }
    else if (sink->extraSDIVITCSource == COUNT_AS_SDI_VITC)
    {
        sink->extraVITCTimecode = get_timecode_from_count(frameInfo->position % MAX_TIMECODE_COUNT);
    }

    
    /* add OSD to frame */
    if (sink->videoStream.isPresent && sink->osd != NULL)
    {
        if (!osd_add_to_image(sink->osd, frameInfo, sink->buffer + sink->videoOffset, sink->rasterWidth, sink->rasterHeight))
        {
            fprintf(stderr, "Failed to add OSD to frame\n");
            /* continue anyway */
        }
    }
    
    
    PTHREAD_MUTEX_LOCK(&sink->frameInfosMutex);

    /* send frame to dvc fifo buffer for display */
    display_on_sv_fifo(sink);
    
    /* record info about frame */
    sink->frameInfos[sink->frameInfosCount % MAX_DISPLAY_DELAY] = *frameInfo;
    sink->frameInfosCount++;

    PTHREAD_MUTEX_UNLOCK(&sink->frameInfosMutex);


    /* start the fifo if not already started */    
    if (!sink->fifoStarted)
    {
        sv_handle* sv = sink->sv;
        sv_fifo_info info;
        
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
    
    
    if (sink->videoStream.ownData)
    {
        SAFE_FREE(&sink->videoStream.data);
    }
    
    for (i = 0; i < sink->numAudioStreams; i++)
    {
        if (sink->audioStream[i].ownData)
        {
            SAFE_FREE(&sink->audioStream[i].data);
        }
    }
    
    SAFE_FREE(&sink->buffer);
    
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



int dvs_card_is_available()
{
    int card;
    sv_handle* sv;
    char card_str[20] = {0};
    int res;
    int foundCard = 0;
    
    for (card = 0; card < MAX_DVS_CARDS; card++)
    {
        snprintf(card_str, sizeof(card_str) - 1, "PCI,card=%d", card);

        res = sv_openex(&sv, 
                        card_str,
                        SV_OPENPROGRAM_DEMOPROGRAM,
                        SV_OPENTYPE_OUTPUT,
                        0,
                        0);
        if (res == SV_OK)
        {
            sv_close(sv);
            foundCard = 1;
            break;
        }
    }
    
    return foundCard;
}

int dvs_open(SDIVITCSource sdiVITCSource, int extraSDIVITCSource, int numBuffers, int disableOSD, 
    int fitVideo, DVSSink** sink)
{
    DVSSink* newSink;
    
    CALLOC_ORET(newSink, DVSSink, 1);

    newSink->sdiVITCSource = sdiVITCSource;
    newSink->extraSDIVITCSource = extraSDIVITCSource;
    newSink->fitVideo = 1;
    
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
    

    //////////////////////////////////////////////////////
    // Attempt to open all sv cards
    //
    // card specified by string of form "PCI,card=n" where n = 0,1,2,3
    //
    int card;
    for (card = 0; card < MAX_DVS_CARDS; card++)
    {
        sv_info             status_info;
        char card_str[20] = {0};

        snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d", card);

        int res = sv_openex(&newSink->sv,
                            card_str,
                            SV_OPENPROGRAM_DEMOPROGRAM,
                            SV_OPENTYPE_OUTPUT,
                            0,
                            0);
        if (res != SV_OK)
        {
            // typical reasons include:
            //  SV_ERROR_DEVICENOTFOUND
            //  SV_ERROR_DEVICEINUSE
            ml_log_warn("card %d: %s\n", card, sv_geterrortext(res));
            continue;
        }
        sv_status( newSink->sv, &status_info);
        ml_log_info("DVS card[%d] display raster is %dx%d\n", card, status_info.xsize, status_info.ysize);
        newSink->rasterWidth = status_info.xsize;
        newSink->rasterHeight = status_info.ysize;
        break;
    }
    if (card == MAX_DVS_CARDS)
    {
        ml_log_error("No DVS card is available\n");
        goto fail;
    }


    newSink->videoDataSize = newSink->rasterWidth * newSink->rasterHeight * 2;
    newSink->audioDataSize = 1920 * 2 * 4; /* 48k Hz for 25 fps, 2 channels, 32 bit */

    newSink->audio12Offset = newSink->rasterWidth * newSink->rasterHeight * 2;
    newSink->audio34Offset = newSink->audio12Offset + 0x4000;
    
    newSink->bufferSize = newSink->videoDataSize + 0x8000;
    MEM_ALLOC_ORET(newSink->buffer, valloc, unsigned char, newSink->bufferSize);
    memset(newSink->buffer, 0, newSink->bufferSize);

    // If raster is PALFF mode, set first 16 lines to UYVY black
    if (newSink->rasterHeight == 592) 
    {
        newSink->palFFMode = 1;
	newSink->videoOffset = newSink->rasterWidth * 16 * 2;
        
        unsigned i;
        for (i = 0; i < newSink->rasterWidth * 16 * 2; i+= 4) 
        {
            newSink->buffer[i+0] = 0x80;
            newSink->buffer[i+1] = 0x10;
            newSink->buffer[i+2] = 0x80;
            newSink->buffer[i+3] = 0x10;
        }
    }
    

    sv_handle           *sv = newSink->sv;
    sv_info             status_info;
    sv_storageinfo      storage_info;
    
    SV_CHK_OFAIL( sv_status( sv, &status_info) );

    SV_CHK_OFAIL( sv_storage_status(sv,
                            0,
                            NULL,
                            &storage_info,
                            sizeof(storage_info),
                            0) );

    SV_CHK_OFAIL( sv_fifo_init( sv,
                            &newSink->svfifo,       // FIFO handle
                            FALSE,          // bInput (FALSE for playback)
                            FALSE,          // bShared (TRUE for input/output share memory)
                            TRUE,           // bDMA
                            FALSE,          // reserved
                            numBuffers) );           // nFrames (0 means use maximum)



    sv_fifo_info fifo_info;
    SV_CHK_OFAIL(sv_fifo_status(sv, newSink->svfifo, &fifo_info));
    newSink->numBuffers = fifo_info.nbuffers;
    newSink->numBuffersFilled = fifo_info.availbuffers;

    
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


