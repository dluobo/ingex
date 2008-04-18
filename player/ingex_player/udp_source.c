#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "udp_source.h"
#include "video_conversion.h"
#include "source_event.h"
#include "logging.h"
#include "macros.h"

#if defined(DISABLE_UDP_SOURCE)

int udp_open(const char *address, MediaSource** source)
{
    return 0;
}


#else


#include "multicast_video.h"

#define MAX_TRACKS      19


typedef struct
{
    int streamIndex;
    StreamInfo streamInfo;
    int frameSize;
    int isDisabled;
} TrackInfo;

#define SOURCE_NAME_SIZE 64

typedef struct
{
    MediaSource mediaSource;

    int socket_fd;
    uint8_t *video;
    uint8_t *audio;

    TrackInfo tracks[MAX_TRACKS];
    int numTracks;

    int64_t position;
    
    int prevLastFrame;

    char prevSourceName[SOURCE_NAME_SIZE];
    char sourceName[SOURCE_NAME_SIZE];
    int sourceNameUpdate;
} UDPSource;


static int udp_get_num_streams(void* data)
{
    UDPSource* source = (UDPSource*)data;

    return source->numTracks;
}

static int udp_get_stream_info(void* data, int streamIndex, const StreamInfo** streamInfo)
{
    UDPSource* source = (UDPSource*)data;

    if (streamIndex < 0 || streamIndex >= source->numTracks)
    {
        /* unknown stream */
        return 0;
    }

    *streamInfo = &source->tracks[streamIndex].streamInfo;
    return 1;
}

static int udp_disable_stream(void* data, int streamIndex)
{
    UDPSource* source = (UDPSource*)data;

    if (streamIndex < 0 || streamIndex >= source->numTracks)
    {
        /* unknown stream */
        return 0;
    }

    source->tracks[streamIndex].isDisabled = 1;
    return 1;
}

static void udp_disable_audio(void* data)
{
    UDPSource* source = (UDPSource*)data;
    int i;

    for (i = 0; i < source->numTracks; i++)
    {
        if (source->tracks[i].streamInfo.type == SOUND_STREAM_TYPE)
        {
            source->tracks[i].isDisabled = 1;
        }
    }
}

static int udp_stream_is_disabled(void* data, int streamIndex)
{
    UDPSource* source = (UDPSource*)data;

    if (streamIndex < 0 || streamIndex >= source->numTracks)
    {
        /* unknown stream */
        return 0;
    }

    return source->tracks[streamIndex].isDisabled;
}


/* returns 0 when successfull, -2 if timed out, otherwise -1 */
static int udp_read_frame(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    UDPSource* source = (UDPSource*)data;
    int i;
    TrackInfo* track;
    unsigned char* buffer;
    int nameUpdated = 0;

    /* Read a frame from the network */
    IngexNetworkHeader header;
    int total_read = 0;

    int res = udp_read_frame_audio_video(source->socket_fd, &header, source->video, source->audio, &total_read);
    if (res == -1)
        return -2;        /* timeout */

    source->prevLastFrame = header.frame_number;
    
    /* check for updated source name */
    // TODO: user strcmp to check header.source_name against last name
	if (strlen(source->sourceName) == 0 ||
		strncmp(source->sourceName, header.source_name, SOURCE_NAME_SIZE-1) != 0) {
		nameUpdated = 1;
		strncpy(source->sourceName, header.source_name, SOURCE_NAME_SIZE-1);
	}


    // Track numbers are hard-coded by setup code in udp_open()
    // 0 - video
    // 1 - audio 1
    // 2 - timecode
    // 3 - timecode
    // 4 - event
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
            memcpy(buffer, source->video, track->frameSize);
        }

        if (track->streamInfo.type == SOUND_STREAM_TYPE) {
            memcpy(buffer, source->audio, 1920*2);
        }

        if (track->streamInfo.type == TIMECODE_STREAM_TYPE) {
            // choose vitc or ltc based on current track (i)
            int tc_as_int = (i == 2) ? header.vitc : header.ltc;
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

static int udp_is_seekable(void* data)
{
    /* cannot seek */
    return 0;
}

/* returns 0 when successfull, -2 if timed out, otherwise -1 */
static int udp_seek(void* data, int64_t position)
{
    /* cannot seek and shouldn't have called this function */
    return -1;
}

static int udp_get_length(void* data, int64_t* length)
{
    /* unknown length */
    return 0;
}

static int udp_get_position(void* data, int64_t* position)
{
    UDPSource* source = (UDPSource*)data;

    *position = source->position;
    return 1;
}

static int udp_get_available_length(void* data, int64_t* length)
{
    return udp_get_length(data, length);
}

static int udp_eof(void* data)
{
    /* never at "end of file" */
    return 0;
}

static void udp_close(void* data)
{
    UDPSource* source = (UDPSource*)data;
    int i;

    if (data == NULL)
    {
        return;
    }

    for (i = 0; i < source->numTracks; i++)
    {
        clear_stream_info(&source->tracks[i].streamInfo);
    }

    free(source->video);
    free(source->audio);

    SAFE_FREE(&source);
}

#define DEFAULT_PORT 2000

// The multicast address is of the form 239.255.x.y:port
// where port by convention is:
//   2000 for channel 0
//   2001 for channel 1
//   ...
int udp_open(const char *address, MediaSource** source)
{
    UDPSource* newSource = NULL;
    int sourceId;

    // Extract address and port number from string e.g. "239.255.1.1:2000"
    char remote[FILENAME_MAX];
    strncpy(remote, address, sizeof(remote)-1);
    char *p;
    int port;
    if ((p = strchr(remote, ':')) == NULL) {    /* couldn't find ':' */
        port = MULTICAST_DEFAULT_PORT;        /* default port to 2000 (channel 0) */
    }
    else {
        port = atol(p+1);    // extract port
        *p = '\0';          // terminate remote string
    }

    // setup network socket
    int fd;
    if ((fd = connect_to_multicast_address(remote, port)) == -1)
    {
        ml_log_error("Failed to connect to UDP address %s:%d\n", remote, port);
        return 0;
    }

	// Read video parameters from multicast stream
	IngexNetworkHeader header;
	if (udp_read_frame_header(fd, &header) == -1)
	{
        ml_log_error("Failed to read from UDP address %s:%d\n", remote, port);
        return 0;
	}

    CALLOC_ORET(newSource, UDPSource, 1);

    newSource->socket_fd = fd;
    newSource->prevLastFrame = -1;
    newSource->sourceNameUpdate = -1;
    memset(newSource->sourceName, 0, SOURCE_NAME_SIZE);

    char title[FILENAME_MAX];
    sprintf(title, "%s from %s\n", header.source_name, address);


    /* FIXME: can this be done later? */
    /* allocate video and audio buffers for this source */
    newSource->video = malloc(header.width * header.height * 3/2);
    newSource->audio = malloc(1920*2 * 2);

    
    // setup media source
    newSource->mediaSource.data = newSource;
    newSource->mediaSource.get_num_streams = udp_get_num_streams;
    newSource->mediaSource.get_stream_info = udp_get_stream_info;
    newSource->mediaSource.disable_stream = udp_disable_stream;
    newSource->mediaSource.disable_audio = udp_disable_audio;
    newSource->mediaSource.stream_is_disabled = udp_stream_is_disabled;
    newSource->mediaSource.read_frame = udp_read_frame;
    newSource->mediaSource.is_seekable = udp_is_seekable;
    newSource->mediaSource.seek = udp_seek;
    newSource->mediaSource.get_length = udp_get_length;
    newSource->mediaSource.get_position = udp_get_position;
    newSource->mediaSource.get_available_length = udp_get_available_length;
    newSource->mediaSource.eof = udp_eof;
    newSource->mediaSource.close = udp_close;

    
    sourceId = msc_create_id();

    /* video track */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = PICTURE_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.frameRate = g_palFrameRate;
    newSource->tracks[newSource->numTracks].streamInfo.width = header.width;
    newSource->tracks[newSource->numTracks].streamInfo.height = header.height;
    newSource->tracks[newSource->numTracks].streamInfo.aspectRatio.num = 4;
    newSource->tracks[newSource->numTracks].streamInfo.aspectRatio.den = 3;
    newSource->tracks[newSource->numTracks].frameSize = header.width * header.height * 3/2;
    newSource->tracks[newSource->numTracks].streamInfo.format = YUV420_FORMAT;
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
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "UDP Audio 1"));
    newSource->tracks[newSource->numTracks].frameSize = 2 * 48000 / 25;
    newSource->numTracks++;

    /* timecode track 1 */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = TIMECODE_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = TIMECODE_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeSubType = VITC_SOURCE_TIMECODE_SUBTYPE;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "UDP Timecode 1"));
    newSource->tracks[newSource->numTracks].frameSize = sizeof(Timecode);
    newSource->numTracks++;

    /* timecode track 2 */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = TIMECODE_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = TIMECODE_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeType = SOURCE_TIMECODE_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.timecodeSubType = LTC_SOURCE_TIMECODE_SUBTYPE;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "UDP Timecode 2"));
    newSource->tracks[newSource->numTracks].frameSize = sizeof(Timecode);
    newSource->numTracks++;

    /* event track */
    CHK_OFAIL(initialise_stream_info(&newSource->tracks[newSource->numTracks].streamInfo));
    newSource->tracks[newSource->numTracks].streamInfo.type = EVENT_STREAM_TYPE;
    newSource->tracks[newSource->numTracks].streamInfo.format = SOURCE_EVENT_FORMAT;
    newSource->tracks[newSource->numTracks].streamInfo.sourceId = sourceId;
    CHK_OFAIL(add_known_source_info(&newSource->tracks[newSource->numTracks].streamInfo, SRC_INFO_TITLE, "UDP Event"));
    newSource->numTracks++;

    
    *source = &newSource->mediaSource;
    return 1;

fail:
    udp_close(newSource);
    return 0;
}


#endif