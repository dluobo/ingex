#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "shared_mem_source.h"
#include "video_conversion.h"
#include "logging.h"
#include "macros.h"


#if defined(DISABLE_SHARED_MEM_SOURCE)

int shared_mem_open(const char *card_name, MediaSource** source)
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
    int card;
    int primary;                    // boolean: primary video buffer or secondary

    TrackInfo tracks[MAX_TRACKS];
    int numTracks;

    int64_t position;
    
    int prevLastFrame;
} SharedMemSource;



static uint8_t *ring[4];
static NexusControl    *pctl = NULL;
static int ring_connected = 0;

static int connect_to_shared_memory(int cardnum)
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
        control_id = shmget(9, sizeof(*pctl), 0444);
        if (control_id != -1)
            break;
        try++;
        if (try > 5)
            return 0;
        usleep(20 * 1000);
    }

    pctl = (NexusControl*)shmat(control_id, NULL, SHM_RDONLY);
    if (verbose)
        printf("connected to pctl\n");

    if (verbose)
        printf("  cards=%d elementsize=%d ringlen=%d\n",
                pctl->cards,
                pctl->elementsize,
                pctl->ringlen);

    if (cardnum+1 > pctl->cards)
    {
        printf("  cardnum not available\n");
        return 0;
    }

    int i;
    for (i = 0; i < pctl->cards; i++)
    {
        while (1)
        {
            shm_id = shmget(10 + i, pctl->elementsize, 0444);
            if (shm_id != -1)
                break;
            usleep(20 * 1000);
        }
        ring[i] = (uint8_t*)shmat(shm_id, NULL, SHM_RDONLY);
        if (verbose)
            printf("  attached to card[%d]\n", i);
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
    int card = source->card;

    int video_offset = 0;
    if (! source->primary)
        video_offset = pctl->audio12_offset + pctl->audio_size;

    return ring[card] + video_offset +
                pctl->elementsize * (lastFrame % pctl->ringlen);
}

static uint8_t *rec_ring_audio(SharedMemSource *source, int track, int lastFrame)
{
    int card = source->card;

    // tracks 0 and 1 are mixed together at audio12_offset
    if (track == 0 || track == 1)
        return ring[card] + pctl->audio12_offset +
                pctl->elementsize * (lastFrame % pctl->ringlen);

    // tracks 2 and 3 are mixed together at audio34_offset
    return ring[card] + pctl->audio34_offset +
                pctl->elementsize * (lastFrame % pctl->ringlen);
}

static uint8_t *rec_ring_timecode(SharedMemSource *source, int track, int lastFrame)
{
    int card = source->card;

    int offset = pctl->tc_offset;         // VITC
    if (track == 1)
        offset = pctl->ltc_offset;        // LTC

    return ring[card] + pctl->elementsize * (lastFrame % pctl->ringlen) + offset;
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

    /* wait until a new frame is available in the ring buffer */
    lastFrame = pctl->card[source->card].lastframe - 1;
    waitCount = 0;
    while (lastFrame == source->prevLastFrame && waitCount < 60000 / sleepUSec)
    {
        usleep(sleepUSec);
        waitCount++;
        
        lastFrame = pctl->card[source->card].lastframe - 1;
    }
    if (lastFrame == source->prevLastFrame && waitCount >= 60000 / sleepUSec)
    {
        /* waited a 1.5 frames - time out */
        return -2;
    }
    source->prevLastFrame = lastFrame;
    
    
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

        if (! sdl_allocate_buffer(listener, i, &buffer, track->frameSize))
        {
            /* listener failed to allocate a buffer for us */
            return -1;
        }

        /* copy data into buffer */
        if (track->streamInfo.type == PICTURE_STREAM_TYPE)
        {
            // FIXME: Once the shared mem control structure records the
            // video format of the primary, decide whether to copy as is:
            //   memcpy(buffer, rec_ring_video(source), track->frameSize);
            // or do the following YUV422 -> UYVY conversion:

            unsigned char* data = rec_ring_video(source, lastFrame);
            AVFrame avFrame;
            avFrame.data[0] = &data[0];
            avFrame.data[1] = &data[720 * 576];
            avFrame.data[2] = &data[720 * 576 * 3 / 2];

            avFrame.linesize[0] = 720;
            avFrame.linesize[1] = 720 / 2;
            avFrame.linesize[2] = 720 / 2;

            yuv422_to_uyvy(720, 576, 0, &avFrame, buffer);
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
            tc.frame = tc_as_int % 25;
            tc.hour = (int)(tc_as_int / (60 * 60 * 25));
            tc.min = (int)((tc_as_int - (tc.hour * 60 * 60 * 25)) / (60 * 25));
            tc.sec = (int)((tc_as_int - (tc.hour * 60 * 60 * 25) - (tc.min * 60 * 25)) / 25);
            memcpy(buffer, &tc, track->frameSize);
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

int shared_mem_open(const char *card_name, MediaSource** source)
{
    SharedMemSource* newSource = NULL;
    int cardNum;
    int primary;
    
    /* check card number is within range */
    cardNum = atol(card_name);
    if (cardNum < 0 || cardNum >= MAX_CHANNELS)
    {
        ml_log_error("Card %s is out of range of maximum %d supported cards\n", card_name, MAX_CHANNELS);
        return 0;
    }

    // Support "0p" or "0s" for primary/secondary video
    if (strchr(card_name, 'p') == NULL)
        primary = 0;
    else
        primary = 1;

    char title[FILENAME_MAX];
    if (primary)
        sprintf(title, "Shared Memory Video: card %d primary\n", cardNum);
    else
        sprintf(title, "Shared Memory Video: card %d secondary\n", cardNum);

    // try to connect to shared mem
    if (! connect_to_shared_memory(cardNum))
    {
        ml_log_error("Failed to connect to shared memory for card %s\n", card_name);
        return 0;
    }
    

    CALLOC_ORET(newSource, SharedMemSource, 1);

    newSource->card = cardNum;
    newSource->prevLastFrame = -1;
    newSource->primary = primary;


    // setup media source
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
    newSource->mediaSource.close = shm_close;


    /* video track */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = PICTURE_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = (newSource->primary) ? UYVY_FORMAT : YUV420_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.frameRate = g_palFrameRate;
    newSource->tracks[newSource->numTracks].streamInfo.width = 720;
    newSource->tracks[newSource->numTracks].streamInfo.height = 576;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, title));
    newSource->tracks[newSource->numTracks].frameSize = (newSource->primary) ? 720*576*2 : 720*576*3/2;
    newSource->numTracks++;

    /* audio track 1 */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = SOUND_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = PCM_FORMAT;
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
    newSource->tracks[newSource->numTracks].streamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeSubType = VITC_SOURCE_TIMECODE_SUBTYPE;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "Shared Memory Timecode 1"));
    newSource->tracks[newSource->numTracks].frameSize = sizeof(Timecode);
    newSource->numTracks++;

    /* timecode track 2 */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = TIMECODE_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = TIMECODE_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeSubType = LTC_SOURCE_TIMECODE_SUBTYPE;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "Shared Memory Timecode 2"));
    newSource->tracks[newSource->numTracks].frameSize = sizeof(Timecode);
    newSource->numTracks++;

    *source = &newSource->mediaSource;
    return 1;

fail:
    shm_close(newSource);
    return 0;
}


#endif


