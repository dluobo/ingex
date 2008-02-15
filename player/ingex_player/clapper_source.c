#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "clapper_source.h"
#include "YUV_frame.h"
#include "YUV_text_overlay.h"
#include "video_conversion.h"
#include "types.h"
#include "logging.h"
#include "macros.h"


#define VIDEO_STREAM_INDEX          0
#define AUDIO_L_STREAM_INDEX        1
#define AUDIO_R_STREAM_INDEX        2

#define REL_X_MARGIN                15
#define TICK_WIDTH                  4
#define ZERO_TICK_WIDTH             8
#define TICK_Y_MARGIN               5
#define REL_PROGRESS_BAR_HEIGHT     10
#define REL_TICK_HEIGHT             50
#define REL_FLASH_BAR_HEIGHT        10
    

typedef struct
{
    MediaSource mediaSource;
    StreamInfo videoStreamInfo;
    StreamInfo audioStreamInfo;
    formats yuvFormat;

    int videoIsDisabled;
    int audioLIsDisabled;
    int audioRIsDisabled;

    int64_t length;
    int64_t position;
    
    unsigned char* image;
    unsigned int imageSize;
    
    const unsigned char* audioSec;
    unsigned int audioSecSize;
    unsigned int audioFrameSize;
} ClapperSource;


static int16_t g_audioSec[];
static overlay g_bbcLogo; 


static int add_static_image_uyvy(ClapperSource* source)
{
    int i;
    int j;
    int xPos;
    int yPos;
    int tickDelta;
    int tickHeight;
    int zeroTickHeight;
    int barHeight;
    unsigned char* imagePtr;
    unsigned char* imagePtr2;
    YUV_frame yuvFrame;
    char txtY, txtU, txtV, box;
    overlay textOverlay;
    p_info_rec p_info;
    char buf[3];

    memset(&p_info, 0, sizeof(p_info));
    CHK_ORET(YUV_frame_from_buffer(&yuvFrame, source->image, source->videoStreamInfo.width, source->videoStreamInfo.height, source->yuvFormat) == 1);

    
    barHeight = source->videoStreamInfo.height / REL_PROGRESS_BAR_HEIGHT;
    barHeight = (barHeight < 4) ? 4 : barHeight;
    barHeight += barHeight % 2;
    tickHeight = source->videoStreamInfo.height / REL_TICK_HEIGHT;
    tickHeight = (tickHeight < 4) ? 4 : tickHeight;
    tickHeight += tickHeight % 2;
    zeroTickHeight = tickHeight * 2;
    tickDelta = (source->videoStreamInfo.width - 2 * source->videoStreamInfo.width / REL_X_MARGIN) / 25;
    
    /* black background */
    
    fill_black(source->videoStreamInfo.format, source->videoStreamInfo.width, source->videoStreamInfo.height, source->image);
    
    
    /* ticks */
    
    xPos = source->videoStreamInfo.width / REL_X_MARGIN + tickDelta - TICK_WIDTH / 2;
    yPos = source->videoStreamInfo.height / 2 + barHeight / 2 + TICK_Y_MARGIN;
    for (i = 0; i < 25; i++)
    {
        imagePtr = source->image + yPos * (source->videoStreamInfo.width * 2) + xPos * 2;
        for (j = 0; j < TICK_WIDTH / 2; j++)
        {
            *imagePtr++ = g_rec601YUVColours[GREEN_COLOUR].U;
            *imagePtr++ = g_rec601YUVColours[GREEN_COLOUR].Y;
            *imagePtr++ = g_rec601YUVColours[GREEN_COLOUR].V;
            *imagePtr++ = g_rec601YUVColours[GREEN_COLOUR].Y;
        }
        
        xPos += tickDelta;
    }
    imagePtr = source->image + yPos * (source->videoStreamInfo.width * 2);
    for (i = 0; i < tickHeight - 1; i++)
    {
        yPos++;
        imagePtr2 = source->image + yPos * (source->videoStreamInfo.width * 2);
        
        memcpy(imagePtr2, imagePtr, source->videoStreamInfo.width * 2);
    }
    
    /* zero tick */
    xPos = source->videoStreamInfo.width / REL_X_MARGIN + tickDelta * 13 - ZERO_TICK_WIDTH / 2;
    yPos = source->videoStreamInfo.height / 2 + barHeight / 2 + TICK_Y_MARGIN;
    imagePtr = source->image + yPos * (source->videoStreamInfo.width * 2) + xPos * 2;
    for (j = 0; j < ZERO_TICK_WIDTH / 2; j++)
    {
        *imagePtr++ = g_rec601YUVColours[GREEN_COLOUR].U;
        *imagePtr++ = g_rec601YUVColours[GREEN_COLOUR].Y;
        *imagePtr++ = g_rec601YUVColours[GREEN_COLOUR].V;
        *imagePtr++ = g_rec601YUVColours[GREEN_COLOUR].Y;
    }
    imagePtr = source->image + yPos * (source->videoStreamInfo.width * 2) + xPos * 2;
    for (i = 0; i < zeroTickHeight - 1; i++)
    {
        yPos++;
        imagePtr2 = source->image + yPos * (source->videoStreamInfo.width * 2) + xPos * 2;
        
        memcpy(imagePtr2, imagePtr, ZERO_TICK_WIDTH * 2);
    }
    
    
    /* tick labels */

    txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
    txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
    txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
    box = 100;
    
    yPos = source->videoStreamInfo.height / 2 + barHeight / 2 + TICK_Y_MARGIN + tickHeight + 5;
    xPos = source->videoStreamInfo.width / REL_X_MARGIN + tickDelta - TICK_WIDTH;
    for (i = -12; i < 13; i++)
    {
        if (i == 0)
        {
            /* no label at zero tick */
            xPos += tickDelta;
            continue;
        }
        
        sprintf(buf, "%d", i < 0 ? -i : i);
        if (text_to_overlay(&p_info, &textOverlay, 
            buf, 
            source->videoStreamInfo.width, 0,
            0, 0,
            0,
            0,
            0,
            "Ariel", 12, source->videoStreamInfo.aspectRatio.num, source->videoStreamInfo.aspectRatio.den) < 0)
        {
            ml_log_error("Failed to create text overlay\n");
            return 1;
        }
        
        CHK_ORET(add_overlay(&textOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == YUV_OK);
        free_overlay(&textOverlay);

        xPos += tickDelta;
    }
    
    
    
    
    /* video/audio late */

    if (text_to_overlay(&p_info, &textOverlay, 
        "VIDEO LATE", 
        source->videoStreamInfo.width, 0,
        0, 0,
        0,
        0,
        0,
        "Ariel", 24, source->videoStreamInfo.aspectRatio.num, source->videoStreamInfo.aspectRatio.den) < 0)
    {
        ml_log_error("Failed to create text overlay\n");
        return 1;
    }
    
    xPos = source->videoStreamInfo.width / REL_X_MARGIN + 3 * tickDelta / 2;
    yPos = source->videoStreamInfo.height / 2 + barHeight / 2 + TICK_Y_MARGIN + zeroTickHeight * 3;
    CHK_ORET(add_overlay(&textOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == YUV_OK);
    free_overlay(&textOverlay);
    
    if (text_to_overlay(&p_info, &textOverlay, 
        "AUDIO LATE", 
        source->videoStreamInfo.width, 0,
        0, 0,
        0,
        0,
        0,
        "Ariel", 24, source->videoStreamInfo.aspectRatio.num, source->videoStreamInfo.aspectRatio.den) < 0)
    {
        ml_log_error("Failed to create text overlay\n");
        return 1;
    }
    
    xPos = source->videoStreamInfo.width - source->videoStreamInfo.width / REL_X_MARGIN - tickDelta - textOverlay.w;
    yPos = source->videoStreamInfo.height / 2 + barHeight / 2 + TICK_Y_MARGIN + zeroTickHeight * 3;
    CHK_ORET(add_overlay(&textOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == YUV_OK);
    free_overlay(&textOverlay);
    
    
    /* BBC logo */
    
    xPos = source->videoStreamInfo.width / 2 - g_bbcLogo.w / 2;
    yPos = source->videoStreamInfo.height / 2 + barHeight / 2 + TICK_Y_MARGIN + zeroTickHeight * 4;
    CHK_ORET(add_overlay(&g_bbcLogo, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == YUV_OK);
    
    
    free_info_rec(&p_info);
    
    return 1;
}

static void add_green_progress_bar(ClapperSource* source)
{
    int i;
    int yPos;
    int barHeight;
    int tickDelta;
    unsigned char* imagePtr;
    unsigned char* imagePtr2;
    int barPosition;

    barHeight = source->videoStreamInfo.height / REL_PROGRESS_BAR_HEIGHT;
    barHeight = (barHeight < 4) ? 4 : barHeight;
    barHeight += barHeight % 2;
    tickDelta = (source->videoStreamInfo.width - 2 * source->videoStreamInfo.width / REL_X_MARGIN) / 25;
    
    barPosition = source->position % 25;

    yPos = source->videoStreamInfo.height / 2 - barHeight / 2;
    yPos += (yPos % 2); /* field 1 */
    imagePtr = source->image + yPos * (source->videoStreamInfo.width * 2) + 
        2 * source->videoStreamInfo.width / REL_X_MARGIN;
    imagePtr2 = source->image + (yPos + 1) * (source->videoStreamInfo.width * 2) + 
        2 * source->videoStreamInfo.width / REL_X_MARGIN;
    for (i = source->videoStreamInfo.width / REL_X_MARGIN; i < source->videoStreamInfo.width - source->videoStreamInfo.width / REL_X_MARGIN; i += 2)
    {
        /* field 1 */
        if (((i - source->videoStreamInfo.width / REL_X_MARGIN) / tickDelta) <= barPosition)
        {
            /* green */
            *imagePtr++ = g_rec601YUVColours[GREEN_COLOUR].U;
            *imagePtr++ = g_rec601YUVColours[GREEN_COLOUR].Y;
            *imagePtr++ = g_rec601YUVColours[GREEN_COLOUR].V;
            *imagePtr++ = g_rec601YUVColours[GREEN_COLOUR].Y;
        }
        else
        {
            /* black */
            *imagePtr++ = g_rec601YUVColours[BLACK_COLOUR].U;
            *imagePtr++ = g_rec601YUVColours[BLACK_COLOUR].Y;
            *imagePtr++ = g_rec601YUVColours[BLACK_COLOUR].V;
            *imagePtr++ = g_rec601YUVColours[BLACK_COLOUR].Y;
        }
        
        /* field 2 */
        if (i - source->videoStreamInfo.width / REL_X_MARGIN < tickDelta / 2 ||
            ((i - source->videoStreamInfo.width / REL_X_MARGIN - tickDelta / 2) / tickDelta) <= barPosition)
        {
            /* green */
            *imagePtr2++ = g_rec601YUVColours[GREEN_COLOUR].U;
            *imagePtr2++ = g_rec601YUVColours[GREEN_COLOUR].Y;
            *imagePtr2++ = g_rec601YUVColours[GREEN_COLOUR].V;
            *imagePtr2++ = g_rec601YUVColours[GREEN_COLOUR].Y;
        }
        else
        {
            /* black */
            *imagePtr2++ = g_rec601YUVColours[BLACK_COLOUR].U;
            *imagePtr2++ = g_rec601YUVColours[BLACK_COLOUR].Y;
            *imagePtr2++ = g_rec601YUVColours[BLACK_COLOUR].V;
            *imagePtr2++ = g_rec601YUVColours[BLACK_COLOUR].Y;
        }
    }
    imagePtr = source->image + yPos * (source->videoStreamInfo.width * 2);
    for (i = 0; i < barHeight - 2; i += 2)
    {
        yPos += 2;
        memcpy(source->image + yPos * (source->videoStreamInfo.width * 2), imagePtr, source->videoStreamInfo.width * 2 * 2);
    }

}

static void add_red_flash(ClapperSource* source)
{
    int i;
    int yPos;
    int barHeight;
    unsigned char* imagePtr;
    int flash;

    barHeight = source->videoStreamInfo.height / REL_FLASH_BAR_HEIGHT;
    barHeight = (barHeight < 4) ? 4 : barHeight;
    barHeight += barHeight % 2;
    
    flash = (source->position % 25) == 12;

    yPos = source->videoStreamInfo.height / 4 - barHeight / 2;
    yPos += (yPos % 2); /* field 1 */
    imagePtr = source->image + yPos * (source->videoStreamInfo.width * 2) + 
        2 * source->videoStreamInfo.width / REL_X_MARGIN;
    for (i = source->videoStreamInfo.width / REL_X_MARGIN; i < source->videoStreamInfo.width - source->videoStreamInfo.width / REL_X_MARGIN; i += 2)
    {
        if (flash)
        {
            /* red */
            *imagePtr++ = g_rec601YUVColours[RED_COLOUR].U;
            *imagePtr++ = g_rec601YUVColours[RED_COLOUR].Y;
            *imagePtr++ = g_rec601YUVColours[RED_COLOUR].V;
            *imagePtr++ = g_rec601YUVColours[RED_COLOUR].Y;
        }
        else
        {
            /* black */
            *imagePtr++ = g_rec601YUVColours[BLACK_COLOUR].U;
            *imagePtr++ = g_rec601YUVColours[BLACK_COLOUR].Y;
            *imagePtr++ = g_rec601YUVColours[BLACK_COLOUR].V;
            *imagePtr++ = g_rec601YUVColours[BLACK_COLOUR].Y;
        }
    }
    imagePtr = source->image + yPos * (source->videoStreamInfo.width * 2);
    for (i = 0; i < barHeight - 1; i++)
    {
        yPos++;
        memcpy(source->image + yPos * (source->videoStreamInfo.width * 2), imagePtr, source->videoStreamInfo.width * 2 * 2);
    }
}


static int clp_get_num_streams(void* data)
{
    return 3;
}

static int clp_get_stream_info(void* data, int streamIndex, const StreamInfo** streamInfo)
{
    ClapperSource* source = (ClapperSource*)data;
    
    if (streamIndex == VIDEO_STREAM_INDEX)
    {
        *streamInfo = &source->videoStreamInfo;
        return 1;
    }
    else if (streamIndex == AUDIO_L_STREAM_INDEX)
    {
        *streamInfo = &source->audioStreamInfo;
        return 1;
    }
    else if (streamIndex == AUDIO_R_STREAM_INDEX)
    {
        *streamInfo = &source->audioStreamInfo;
        return 1;
    }
    
    return 0;
}

static int clp_disable_stream(void* data, int streamIndex)
{
    ClapperSource* source = (ClapperSource*)data;
    
    if (streamIndex == VIDEO_STREAM_INDEX)
    {
        source->videoIsDisabled = 1;
        return 1;
    }
    else if (streamIndex == AUDIO_L_STREAM_INDEX)
    {
        source->audioLIsDisabled = 1;
        return 1;
    }
    else if (streamIndex == AUDIO_R_STREAM_INDEX)
    {
        source->audioRIsDisabled = 1;
        return 1;
    }
    
    return 0;
}

static int clp_stream_is_disabled(void* data, int streamIndex)
{
    ClapperSource* source = (ClapperSource*)data;
    
    if (streamIndex == VIDEO_STREAM_INDEX)
    {
        return source->videoIsDisabled;
    }
    else if (streamIndex == AUDIO_L_STREAM_INDEX)
    {
        return source->audioLIsDisabled;
    }
    else if (streamIndex == AUDIO_R_STREAM_INDEX)
    {
        return source->audioRIsDisabled;
    }
    
    return 0;
}

static int clp_read_frame(void* data, const FrameInfo* frameInfo, MediaSourceListener* listener)
{
    ClapperSource* source = (ClapperSource*)data;
    int audioSampleOffset;
    
    if (!source->videoIsDisabled)
    {
        if (sdl_accept_frame(listener, VIDEO_STREAM_INDEX, frameInfo))
        {
            add_green_progress_bar(source);
            add_red_flash(source);
            
            if (!sdl_receive_frame_const(listener, VIDEO_STREAM_INDEX, source->image, source->imageSize))
            {
                return -1;
            }
        }
    }
    
    if (!source->audioLIsDisabled)
    {
        if (sdl_accept_frame(listener, AUDIO_L_STREAM_INDEX, frameInfo))
        {
            audioSampleOffset = (source->position % 25) * source->audioFrameSize;
            if (!sdl_receive_frame_const(listener, AUDIO_L_STREAM_INDEX, &source->audioSec[audioSampleOffset], source->audioFrameSize))
            {
                return -1;
            }
        }
    }
    
    if (!source->audioRIsDisabled)
    {
        if (sdl_accept_frame(listener, AUDIO_R_STREAM_INDEX, frameInfo))
        {
            audioSampleOffset = (source->position % 25) * source->audioFrameSize;
            if (!sdl_receive_frame_const(listener, AUDIO_R_STREAM_INDEX, &source->audioSec[audioSampleOffset], source->audioFrameSize))
            {
                return -1;
            }
        }
    }
    
    source->position++;
    
    return 0;
}

static int clp_is_seekable(void* data)
{
    return 1;
}

static int clp_seek(void* data, int64_t position)
{
    ClapperSource* source = (ClapperSource*)data;
    
    source->position = position;
    return 0;
}

static int clp_get_length(void* data, int64_t* length)
{
    ClapperSource* source = (ClapperSource*)data;

    *length = source->length;
    return 1;
}

static int clp_get_position(void* data, int64_t* position)
{
    ClapperSource* source = (ClapperSource*)data;

    if (source->videoIsDisabled && source->audioLIsDisabled && source->audioRIsDisabled)
    {
        return 0;
    }
    
    *position = source->position;
    return 1;
}

static int clp_get_available_length(void* data, int64_t* length)
{
    ClapperSource* source = (ClapperSource*)data;

    *length = source->length;
    return 1;
}

static int clp_eof(void* data)
{
    ClapperSource* source = (ClapperSource*)data;
    
    if (source->videoIsDisabled && source->audioLIsDisabled && source->audioRIsDisabled)
    {
        return 0;
    }
    
    if (source->position >= source->length)
    {
        return 1;
    }
    return 0;
}

static void clp_close(void* data)
{
    ClapperSource* source = (ClapperSource*)data;
    
    if (data == NULL)
    {
        return;
    }
    
    clear_stream_info(&source->videoStreamInfo);
    clear_stream_info(&source->audioStreamInfo);
    
    SAFE_FREE(&source->image);
    
    SAFE_FREE(&source);
}


int clp_create(const StreamInfo* videoStreamInfo, const StreamInfo* audioStreamInfo, 
    int64_t length, MediaSource** source)
{
    ClapperSource* newSource = NULL;

    
    /* TODO: support YUV422 and YUV420 */
    if (videoStreamInfo->type != PICTURE_STREAM_TYPE ||
        (videoStreamInfo->format != UYVY_FORMAT))/* &&
            videoStreamInfo->format != YUV422_FORMAT &&
            videoStreamInfo->format != YUV420_FORMAT))*/
    {
        ml_log_error("Invalid video stream for clapper source\n");
        return 0;
    }
    if (audioStreamInfo->type != SOUND_STREAM_TYPE ||
        audioStreamInfo->format != PCM_FORMAT ||
        audioStreamInfo->samplingRate.num != 48000 ||
        audioStreamInfo->bitsPerSample != 16 ||
        audioStreamInfo->numChannels != 1)
    {
        ml_log_error("Invalid audio stream for clapper source\n");
        return 0;
    }
    
    CALLOC_ORET(newSource, ClapperSource, 1);
    
    newSource->length = length;
    
    if (videoStreamInfo->format == UYVY_FORMAT)
    {
        newSource->imageSize = videoStreamInfo->width * videoStreamInfo->height * 2;
        newSource->yuvFormat = UYVY;
    }
    else if (videoStreamInfo->format == YUV422_FORMAT)
    {
        newSource->imageSize = videoStreamInfo->width * videoStreamInfo->height * 2;
        newSource->yuvFormat = YUV422;
    }
    else /* videoStreamInfo->format == YUV420_FORMAT */
    {
        newSource->imageSize = videoStreamInfo->width * videoStreamInfo->height * 3 / 2;
        newSource->yuvFormat = I420;
    }
    MALLOC_OFAIL(newSource->image, unsigned char, newSource->imageSize);
    
    newSource->audioSecSize = 1920 * 2 * 25;
    newSource->audioFrameSize = 1920 * 2;
    newSource->audioSec = (unsigned char*)g_audioSec;
    
    newSource->mediaSource.data = newSource;
    newSource->mediaSource.get_num_streams = clp_get_num_streams;
    newSource->mediaSource.get_stream_info = clp_get_stream_info;
    newSource->mediaSource.disable_stream = clp_disable_stream;
    newSource->mediaSource.stream_is_disabled = clp_stream_is_disabled;
    newSource->mediaSource.read_frame = clp_read_frame;
    newSource->mediaSource.is_seekable = clp_is_seekable;
    newSource->mediaSource.seek = clp_seek;
    newSource->mediaSource.get_length = clp_get_length;
    newSource->mediaSource.get_position = clp_get_position;
    newSource->mediaSource.get_available_length = clp_get_available_length;
    newSource->mediaSource.eof = clp_eof;
    newSource->mediaSource.close = clp_close;
    
    newSource->videoStreamInfo = *videoStreamInfo;
    newSource->videoStreamInfo.sourceId = msc_create_id();
    newSource->audioStreamInfo = *audioStreamInfo;
    newSource->audioStreamInfo.sourceId = newSource->videoStreamInfo.sourceId;
    
    CHK_OFAIL(add_known_source_info(&newSource->videoStreamInfo, SRC_INFO_TITLE, "Clapper test sequence"));
    CHK_OFAIL(add_known_source_info(&newSource->audioStreamInfo, SRC_INFO_TITLE, "Clapper test sequence"));

    CHK_OFAIL(add_static_image_uyvy(newSource));
    
    
    *source = &newSource->mediaSource;
    return 1;
    
fail:
    clp_close(newSource);
    return 0;
}





/* BBC Logo */

static BYTE g_bbcLogoBitMap[] = 
{
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xe5,0xc8,0xc8,0xc8,0xc8,0xc8,0xc8,0xc8,0xcb,0xe7,0xfe,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xdd,0xc8,0xc8,0xc8,0xc8,0xc8,0xc8,0xc8,0xce,0xec,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xe8,0xb4,0x90,0x7e,0x7a,0x81,0x95,0xb8,0xec,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09,0x39,0x98,0xf3,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0d,0x46,0xa8,0xfb,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xe8,0x86,0x37,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x29,0x73,0xc4,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x25,0xce,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3a,0xe1,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xf5,0x78,0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x23,0xb1,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0d,0xd4,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1f,0xea,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xd2,0x26,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x79,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x07,0x69,0x69,0x68,0x5b,0x28,0x02,0x00,0x00,0x00,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x17,0x69,0x69,0x68,0x55,0x1f,0x00,0x00,0x00,0x00,0x00,0x61,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xc0,0x0e,0x00,0x00,0x00,0x00,0x00,0x0a,0x4c,0x81,0x9f,0xa6,0x9d,0x85,0x59,0x1c,0x00,0x00,0x00,0x00,0x79,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x11,0xff,0xff,0xff,0xff,0xff,0xd7,0x2a,0x00,0x00,0x00,0x00,0xd0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xc3,0x16,0x00,0x00,0x00,0x06,0xf3,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xd6,0x10,0x00,0x00,0x00,0x00,0x04,0x7b,0xf0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xca,0x68,0x0b,0x00,0x79,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x11,0xff,0xff,0xff,0xff,0xff,0xff,0xb6,0x00,0x00,0x00,0x00,0xa0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xff,0x8e,0x00,0x00,0x00,0x00,0xce,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xfc,0x31,0x00,0x00,0x00,0x00,0x26,0xc7,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xed,0x7b,0x82,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x11,0xff,0xff,0xff,0xff,0xff,0xff,0xea,0x00,0x00,0x00,0x00,0xa0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xff,0xc2,0x00,0x00,0x00,0x00,0xcf,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xa0,0x00,0x00,0x00,0x00,0x40,0xf5,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x11,0xff,0xff,0xff,0xff,0xff,0xff,0xc6,0x00,0x00,0x00,0x00,0xdb,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xff,0x9e,0x00,0x00,0x00,0x0c,0xfc,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xfd,0x25,0x00,0x00,0x00,0x13,0xe6,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x11,0xff,0xff,0xff,0xff,0xff,0xfa,0x49,0x00,0x00,0x00,0x17,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xf1,0x2a,0x00,0x00,0x00,0x40,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xce,0x00,0x00,0x00,0x00,0x89,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x10,0xf0,0xf0,0xe9,0xca,0x9a,0x29,0x00,0x00,0x00,0x00,0x9f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x35,0xf0,0xef,0xe6,0xc3,0x8e,0x1a,0x00,0x00,0x00,0x05,0xc3,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0x8d,0x00,0x00,0x00,0x02,0xe9,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0e,0xa0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1b,0xbb,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0x50,0x00,0x00,0x00,0x2a,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x33,0xa6,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x46,0xba,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0x34,0x00,0x00,0x00,0x4a,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x44,0xde,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x5f,0xee,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0x31,0x00,0x00,0x00,0x4e,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x05,0x41,0x41,0x41,0x41,0x3b,0x24,0x02,0x00,0x00,0x00,0x00,0x0c,0xcd,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x0f,0x41,0x41,0x41,0x41,0x38,0x1e,0x00,0x00,0x00,0x00,0x00,0x1e,0xe9,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0x41,0x00,0x00,0x00,0x2f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x11,0xff,0xff,0xff,0xff,0xff,0xff,0xe5,0x83,0x0a,0x00,0x00,0x00,0x27,0xf8,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xff,0xda,0x6f,0x03,0x00,0x00,0x00,0x49,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0x6b,0x00,0x00,0x00,0x07,0xf1,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x11,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xa7,0x00,0x00,0x00,0x00,0xae,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f,0x00,0x00,0x00,0x00,0xd5,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xad,0x00,0x00,0x00,0x00,0x9b,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x11,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0x16,0x00,0x00,0x00,0x6d,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xea,0x00,0x00,0x00,0x00,0x94,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xf7,0x14,0x00,0x00,0x00,0x1f,0xf5,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x11,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0x17,0x00,0x00,0x00,0x58,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xed,0x00,0x00,0x00,0x00,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0x82,0x00,0x00,0x00,0x00,0x55,0xfd,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xeb,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x11,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xc3,0x00,0x00,0x00,0x00,0x71,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x9b,0x00,0x00,0x00,0x00,0x98,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xf3,0x1f,0x00,0x00,0x00,0x00,0x52,0xf3,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xeb,0x74,0x3e,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x11,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xab,0x1b,0x00,0x00,0x00,0x00,0xb3,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xff,0xfc,0x94,0x0e,0x00,0x00,0x00,0x02,0xdc,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xc3,0x06,0x00,0x00,0x00,0x00,0x1e,0x9b,0xf6,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfb,0xc4,0x65,0x0b,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x08,0x71,0x71,0x71,0x71,0x6e,0x4f,0x1f,0x00,0x00,0x00,0x00,0x00,0x2c,0xfb,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x19,0x71,0x71,0x71,0x71,0x6b,0x48,0x17,0x00,0x00,0x00,0x00,0x00,0x56,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xaa,0x04,0x00,0x00,0x00,0x00,0x00,0x0a,0x43,0x7c,0x9b,0xa6,0x9d,0x81,0x4b,0x0e,0x00,0x00,0x00,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0d,0xd6,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1f,0xed,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xb4,0x11,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x39,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3f,0xda,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x57,0xea,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xe3,0x58,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1d,0x9b,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x57,0xa9,0xfe,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x63,0xbb,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xd4,0x6a,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x13,0x56,0xb1,0xfb,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xe2,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xd1,0xea,0xfe,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xd8,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc1,0xd5,0xee,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xd5,0xa9,0x8a,0x79,0x7b,0x82,0x90,0xad,0xd6,0xfe,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x51,0x00,0x00,0x00,0x00,0x29,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x29,0x00,0x00,0x00,0x00,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static overlay g_bbcLogo = 
{
    120,
    34,
    -1,
    -1,
    g_bbcLogoBitMap,
    0
};



/* audio clapper provided by Andrew Mason, BBC Research */

static int16_t g_audioSec[] = 
{
6,6,5,4,4,4,6,8,5,9,6,7,5,8,6,6,6,7,7,8,
6,7,7,8,8,6,8,5,6,5,7,6,6,7,7,9,11,9,10,10,
9,11,10,12,12,13,9,11,11,8,9,11,8,7,8,9,8,7,9,10,
12,10,10,11,10,11,11,10,10,11,11,13,11,11,10,11,11,11,13,12,
13,12,14,11,11,14,10,11,12,13,12,11,11,10,13,9,13,11,13,10,
12,11,8,11,8,10,11,10,11,9,11,10,12,9,10,10,10,9,9,9,
9,8,9,10,7,8,10,8,10,9,9,10,8,10,10,9,10,8,9,10,
10,9,11,9,12,9,9,8,8,10,7,8,11,7,9,9,10,8,7,8,
7,8,8,9,7,8,6,6,7,5,8,8,6,6,7,7,6,6,7,5,
5,6,5,7,7,8,6,7,9,7,6,8,6,7,5,7,5,4,6,3,
5,5,5,2,5,2,4,4,4,5,4,1,3,3,3,1,2,2,1,3,
1,0,2,1,2,2,-1,2,-1,2,2,2,-1,1,1,1,1,-1,2,0,
2,2,0,1,-1,0,2,-1,-1,1,0,-1,0,1,2,0,0,-1,1,-2,
0,1,-2,1,1,-1,0,0,-2,-2,0,0,-1,0,0,3,2,1,2,2,
2,2,0,0,-2,2,1,0,-2,2,0,-2,-2,-2,-1,-1,0,2,0,-1,
2,2,0,2,-1,1,-1,0,0,-1,0,-1,2,2,3,3,3,2,4,2,
3,2,3,6,6,3,7,3,4,7,6,5,7,7,5,5,8,6,6,8,
6,7,7,7,9,11,10,12,12,9,10,11,11,9,10,10,10,8,11,10,
10,9,10,11,10,12,12,10,12,13,12,12,13,12,12,14,12,11,12,12,
12,14,14,13,15,14,14,13,14,11,11,13,13,10,10,13,14,13,13,13,
13,11,12,9,10,9,9,10,11,8,10,8,8,9,11,9,9,10,7,8,
8,9,9,7,6,7,5,7,6,7,3,5,6,4,6,3,4,3,3,5,
1,3,3,4,2,2,2,4,1,1,0,3,-2,1,2,-1,-2,-3,-1,-4,
-3,-2,-5,-4,-5,-3,-3,-4,-3,-5,-2,-4,-2,-2,-4,-5,-4,-5,-6,-4,
-5,-5,-2,-5,-7,-4,-6,-7,-6,-3,-5,-3,-5,-6,-4,-3,-6,-4,-3,-7,
-7,-7,-6,-6,-5,-9,-6,-7,-9,-6,-5,-5,-6,-7,-7,-5,-7,-6,-5,-4,
-7,-4,-7,-5,-8,-5,-3,-4,-5,-5,-4,-3,-4,-5,-5,-4,-4,-2,0,-1,
-2,-2,-2,-2,-2,-2,-4,-4,-6,-4,-3,-3,-3,-4,-3,-2,-3,-3,-3,-2,
-3,-4,-3,-3,-3,-2,-4,-2,-4,-3,-2,-2,-1,1,-1,0,1,-2,-1,-2,
0,-1,-4,0,-1,0,-2,-1,0,-1,0,-1,0,-4,-2,-1,-2,0,-2,0,
3,-2,-2,0,-1,0,-1,-3,-3,-3,-2,-3,-3,-5,-2,-3,-4,-6,-6,-8,
-6,-4,-5,-6,-5,-6,-8,-5,-7,-5,-5,-6,-5,-5,-5,-5,-6,-5,-6,-3,
-5,-4,-6,-5,-6,-6,-8,-6,-6,-7,-4,-5,-8,-7,-6,-8,-7,-6,-7,-7,
-7,-9,-6,-7,-6,-4,-5,-4,-4,-5,-6,-4,-4,-5,-6,-3,-5,-5,-5,-3,
-4,-3,-2,-1,-2,-4,-1,0,-3,-1,-1,-1,-3,-2,1,0,-1,-3,0,-1,
0,-3,-1,-1,1,0,-1,2,1,-1,-1,1,-1,1,0,-1,1,3,0,3,
1,2,5,3,1,3,3,2,3,4,3,5,5,6,4,6,3,5,6,3,
7,6,6,3,4,5,3,5,4,2,5,5,2,4,3,1,2,5,3,5,
3,6,5,4,4,6,2,6,5,3,4,5,5,3,3,2,3,5,3,3,
3,2,2,3,4,3,2,2,3,4,0,2,0,2,0,3,3,0,0,2,
2,1,1,1,3,3,4,3,4,1,2,2,2,3,2,3,2,2,3,4,
0,3,4,2,1,3,1,2,3,2,3,3,2,4,4,4,2,3,1,3,
2,3,3,4,-1,3,1,0,0,3,-2,0,1,2,0,0,0,-1,-3,-1,
-2,-2,-1,-2,-2,-1,-2,0,-3,-2,-1,-3,-2,-2,-2,-2,-3,-3,-3,-1,
-2,-3,-3,-3,-2,-2,-3,-2,-2,-1,-1,-3,-2,-4,-2,-2,-2,-2,-2,-1,
1,-2,-2,1,-1,-1,-1,-2,-2,-1,0,-1,-1,-1,0,0,-3,1,0,-3,
-1,0,-3,-1,-2,-4,1,-1,-3,-3,0,-1,-1,-1,1,-1,2,-1,1,0,
1,1,-1,1,2,3,0,0,-1,-1,-1,-1,2,-2,0,0,-1,0,-1,1,
1,1,1,3,0,1,2,0,1,1,1,2,0,2,1,2,-1,2,4,3,
0,3,2,-1,5,3,1,4,3,1,3,2,4,2,5,3,1,1,2,4,
3,4,4,3,3,5,2,1,2,1,2,1,3,3,2,2,3,1,0,1,
3,1,0,0,2,1,1,1,1,0,2,-1,1,0,1,1,-1,-2,-1,-2,
-2,2,0,-1,1,-2,-1,0,-1,-2,-1,0,-1,0,-1,-3,-1,-1,-2,-1,
-2,-1,-2,-4,-1,-3,-2,-4,-1,-3,-3,-1,-4,-5,-5,-2,-4,-3,-2,-3,
-2,-4,-2,-4,-3,-2,-3,-5,-2,-4,-4,-5,-3,-4,-2,-3,-2,-2,-1,-4,
-3,-2,-4,-3,-2,-2,-3,-2,-2,-1,0,-2,0,0,0,-2,-3,1,-2,1,
-1,-1,-1,0,-1,-1,0,-2,1,2,2,0,1,-1,1,2,3,2,3,4,
3,3,0,3,-1,4,2,0,0,3,0,1,2,2,0,5,2,0,1,-1,
0,3,3,2,4,3,3,2,1,1,3,1,3,2,1,2,1,0,4,0,
-1,-1,-1,-1,-1,1,-2,-1,1,0,1,-1,-2,0,-3,-1,-3,1,1,0,
2,0,-1,0,-1,1,0,1,-1,0,2,-1,2,0,0,-1,2,2,1,-1,
-1,-1,-1,-1,0,1,0,-1,-1,-1,1,-2,1,0,-1,1,2,-1,-1,-1,
0,-1,1,-1,-3,-1,-1,-1,-3,-3,-2,-2,-3,-5,-3,-6,-5,-2,-2,-5,
-5,-2,-3,-5,-3,-5,-3,-4,-1,-3,-2,-4,-3,-4,-4,-6,-1,-3,-2,-2,
-4,-2,-2,0,-2,-1,-1,1,0,-1,-1,-1,-1,0,0,-1,1,-1,-2,-2,
0,0,1,0,0,2,1,0,0,1,1,1,0,0,-1,1,0,0,-1,0,
0,-1,1,1,0,1,1,3,2,2,3,3,3,1,4,5,2,2,2,2,
4,1,3,2,3,3,1,4,3,3,4,3,4,3,6,8,2,5,6,3,
5,3,5,5,5,5,5,3,4,1,3,3,1,2,2,-1,1,2,2,0,
4,0,3,3,3,5,3,2,2,4,1,-1,2,0,0,-2,-1,0,0,-2,
-2,0,0,-2,-2,1,-1,-1,-1,1,2,-1,-2,0,0,-1,-1,0,-2,0,
0,0,-1,0,-1,-1,-2,-1,-2,-1,-2,1,-1,-3,0,-3,-3,-1,1,-3,
0,-2,-1,-3,-3,-2,-2,-3,1,-1,-2,-2,0,-1,-1,1,-1,1,1,2,
3,2,2,2,4,1,3,1,-1,1,3,1,2,2,1,3,1,2,2,0,
3,4,4,4,5,1,1,3,4,2,3,2,3,2,4,2,3,4,2,3,
1,0,3,0,2,2,6,3,2,5,4,3,2,3,2,2,3,0,3,3,
4,4,3,1,3,2,1,2,1,-1,1,3,0,3,2,-1,3,0,0,-2,
0,-1,-1,-3,-1,2,-1,1,-1,1,1,1,0,-2,0,0,-1,-4,-3,-1,
-2,-4,-3,-2,-2,-3,-4,-5,-4,-4,-5,-4,-4,-3,-3,-4,-3,-5,-5,-6,
-7,-5,-5,-6,-5,-4,-6,-4,-3,-6,-4,-3,-4,-6,-4,-5,-1,-3,-2,-2,
-4,-1,-4,-6,-2,-2,-4,-3,-3,-3,-3,-4,-6,-4,-4,-4,-6,-2,-4,-2,
-4,-3,-3,-1,2,-2,-2,0,-1,-1,-1,0,0,-1,-1,-4,1,-1,-2,0,
-1,1,2,-1,2,-1,-1,-3,-1,0,-3,-2,-3,0,-1,-1,0,-2,-1,-1,
-1,0,1,-2,0,0,1,1,-3,0,1,-2,1,-1,-1,0,2,1,2,3,
2,3,1,0,0,0,4,2,1,3,0,3,1,2,2,2,3,2,4,1,
2,2,2,2,3,6,3,3,3,2,5,3,6,3,5,3,4,4,3,3,
3,3,3,5,7,4,6,8,5,5,6,7,2,5,4,4,4,3,2,6,
4,3,5,6,2,6,6,3,3,6,2,3,3,1,3,1,1,0,3,2,
5,4,0,4,2,1,2,3,3,3,4,5,4,4,4,6,3,5,4,5,
5,5,4,4,5,3,4,6,4,5,5,5,7,4,6,5,6,8,8,6,
9,7,7,9,6,8,7,9,10,9,8,10,10,7,8,11,9,10,10,9,
11,11,11,9,11,8,11,11,9,10,9,10,12,9,13,11,12,11,10,13,
11,13,11,12,10,9,11,11,12,10,13,10,13,10,10,11,11,10,9,8,
9,8,11,9,9,8,9,7,9,9,9,7,7,6,7,8,5,7,6,4,
6,5,5,5,6,4,4,4,3,5,4,4,5,3,2,5,2,1,-2,1,
1,0,3,3,3,4,2,0,3,0,2,2,0,1,1,-3,-1,1,0,-1,
0,0,-2,3,0,0,0,-1,-3,1,-3,1,0,-1,1,0,-3,-2,-1,-1,
0,2,0,-1,0,1,0,0,-1,0,1,0,-2,1,2,-1,1,0,-3,0,
0,0,1,2,1,2,2,1,1,2,3,0,0,3,2,0,1,3,1,0,
0,0,0,3,1,0,2,1,1,1,2,1,3,3,1,4,5,4,2,6,
3,3,4,4,1,3,3,5,3,4,2,0,3,5,3,4,2,3,5,4,
0,1,1,2,5,3,0,3,3,1,3,1,2,1,3,1,0,1,1,0,
2,1,3,1,3,0,1,1,3,1,3,1,2,0,2,4,2,0,2,2,
2,1,4,0,1,1,0,3,1,2,-1,1,1,0,-1,1,2,1,2,0,
2,2,-1,2,2,2,0,2,3,0,1,1,0,2,0,3,1,3,0,0,
0,2,2,1,4,1,0,5,1,2,3,3,1,3,2,2,4,2,3,3,
2,1,2,2,2,1,1,-1,-1,1,1,1,0,-3,0,-1,-3,2,0,0,
2,1,-2,3,0,2,1,3,2,-1,1,0,3,3,2,2,2,1,2,-1,
3,2,3,2,1,4,3,3,3,2,3,4,3,2,3,3,4,2,3,2,
5,3,3,2,0,2,4,0,1,0,1,0,2,1,-2,1,3,2,1,2,
0,0,0,1,-1,0,1,2,5,0,2,2,0,-1,1,-1,-2,-1,-2,1,
1,1,1,1,2,4,2,4,3,2,2,2,4,5,3,4,1,3,4,2,
5,3,7,3,4,5,4,6,6,4,5,2,4,4,3,2,2,5,3,4,
4,7,5,5,5,7,5,6,3,5,6,4,6,6,7,7,4,5,4,6,
6,4,6,5,4,5,5,5,4,4,5,7,3,8,8,8,6,6,5,2,
5,8,6,6,7,8,8,5,4,6,4,6,5,5,7,6,6,5,6,7,
5,9,10,6,6,9,7,7,8,6,7,6,7,4,3,4,7,5,5,7,
6,6,6,4,7,6,6,4,5,2,3,4,6,2,3,4,3,0,3,4,
4,5,5,5,7,3,7,5,3,6,7,3,7,6,5,4,3,1,4,4,
3,3,4,1,5,4,1,3,5,5,4,3,4,2,2,5,2,2,2,1,
4,1,-1,3,-2,-1,0,0,-1,2,0,3,1,2,1,3,1,0,2,4,
0,2,-2,2,4,2,3,2,0,4,2,-1,3,1,1,3,2,2,2,3,
3,1,3,4,5,2,3,1,2,4,1,2,2,1,3,1,3,2,3,3,
2,4,2,4,4,2,3,5,4,4,3,2,0,2,3,3,2,3,1,1,
4,3,2,5,3,3,5,4,2,4,3,2,4,2,4,4,4,3,3,5,
4,6,3,4,3,4,3,4,5,8,6,5,3,4,5,6,7,5,7,6,
5,5,7,5,5,6,6,6,4,6,3,4,4,1,4,1,4,3,5,4,
7,5,4,5,5,4,5,5,6,3,5,2,2,3,0,1,4,2,3,3,
2,1,3,4,2,5,5,4,3,3,4,5,1,2,3,2,0,2,3,0,
5,4,2,2,4,3,3,3,6,1,4,5,2,3,4,2,2,5,5,2,
4,5,1,4,4,4,2,7,1,2,2,4,4,3,5,3,6,5,2,5,
5,3,4,3,2,5,3,4,2,4,6,5,3,4,3,1,2,3,1,2,
2,3,2,6,2,1,3,0,2,2,1,2,2,0,2,0,0,0,2,0,
2,1,2,0,0,1,1,1,1,-1,-1,-1,-2,-1,-3,-2,-1,-3,-1,-2,
-2,-2,-3,-3,-3,-2,-3,-5,-2,-4,-3,-3,-4,-5,-1,-3,-2,-6,-2,-4,
-4,-3,-4,-4,-4,-4,-6,-5,-4,-2,-6,-4,-3,-4,-2,-3,-6,-2,-2,1,
-4,-3,-3,-2,-1,-3,0,-2,-2,0,-4,-2,-4,-2,-6,-2,-3,-3,-1,-4,
-4,-1,-1,-3,-1,-3,-4,-3,-3,-3,-4,-2,-3,-3,-1,-4,-2,-2,-5,-4,
-3,-3,-5,-2,-3,-2,-2,-2,0,-2,-2,-1,-1,-3,0,-3,-3,-3,-1,-1,
1,-1,1,0,0,-1,3,1,-1,3,-2,1,0,0,-2,0,2,0,0,1,
-1,1,-1,0,0,1,1,1,-2,-2,2,0,-2,1,1,-1,0,-2,-3,0,
-1,-2,-1,-1,-4,-1,-3,-2,-3,-1,-1,-3,-3,-1,1,-1,1,0,3,0,
1,1,1,2,-1,1,-1,0,0,-1,-1,-2,0,0,1,0,2,0,0,1,
2,2,0,0,-1,0,0,-2,0,0,0,2,2,3,1,3,5,3,3,4,
3,4,5,4,3,3,4,1,4,2,1,2,4,2,5,3,2,5,2,5,
2,3,3,3,5,2,4,6,4,5,6,6,5,4,6,5,6,4,7,5,
5,2,3,3,3,5,3,4,3,3,3,5,3,5,4,1,3,3,4,2,
4,2,4,3,4,3,2,3,3,3,4,2,4,3,3,5,2,3,4,5,
1,2,4,3,1,2,1,2,2,0,2,2,1,2,2,0,1,0,-1,0,
1,-2,4,1,2,3,5,0,2,4,1,2,1,2,1,1,3,1,1,2,
1,1,5,1,3,2,2,0,3,3,3,2,0,-4,-1,-2,-2,2,-1,1,
-1,-3,-1,-2,0,0,2,0,2,0,3,0,2,2,1,-2,0,0,0,-1,
-2,-1,-2,-2,-3,-2,-3,-1,-2,-3,-2,-3,-2,-1,-3,-3,-2,-4,-1,-3,
-2,0,-3,0,-1,-1,-1,-2,-3,-1,-1,-2,-1,-1,-1,1,-2,0,-2,-2,
-4,-2,-5,0,-3,-1,1,-2,-1,-1,-2,1,-1,-1,-1,-1,0,-2,0,-2,
-2,-1,-1,0,0,0,0,-1,-3,-1,-2,-2,-2,-3,-1,-1,-3,1,-1,2,
-1,1,2,0,-1,0,2,-1,3,1,1,2,0,1,-1,0,-1,2,1,-1,
1,0,1,-1,1,0,1,2,-1,0,2,4,2,3,1,4,2,5,0,4,
1,4,3,1,-1,2,1,-1,1,0,1,-2,2,2,0,1,3,4,4,2,
3,2,2,5,2,4,5,1,1,2,1,2,3,2,4,4,5,4,6,4,
4,4,5,3,5,3,4,1,4,3,3,2,0,4,2,1,2,2,3,2,
1,2,2,1,4,3,4,3,1,2,3,5,-1,1,0,1,4,4,3,4,
4,6,2,1,3,1,3,4,2,4,3,3,4,5,2,3,1,3,3,2,
3,2,2,2,1,2,2,2,-1,2,2,-1,0,0,1,3,0,1,1,2,
1,1,2,0,0,2,0,-1,1,1,0,1,1,1,1,1,-1,1,3,2,
2,3,2,1,-1,3,0,-1,1,1,-3,0,0,0,0,-1,-2,-1,0,-2,
1,-3,-2,-1,-1,-2,-2,-2,-1,-1,-1,-2,-2,-3,-2,-4,-2,-3,-3,-4,
-5,-1,-5,-3,-2,-4,-6,-6,-7,-5,-5,-5,-3,-4,-3,-5,-4,-3,-5,-6,
-4,-4,-5,-5,-3,-7,-4,-5,-2,-4,-4,-2,-3,-5,-2,-2,-3,-4,0,-2,
-2,-3,-1,-2,1,-1,-1,-2,0,0,-2,-1,0,0,0,0,0,-2,1,0,
2,0,2,0,1,0,0,0,1,4,3,3,2,4,2,2,1,3,1,4,
2,3,3,4,4,4,4,2,5,2,3,3,5,2,4,4,3,3,4,4,
2,6,4,2,5,6,4,6,4,5,6,6,5,6,5,5,7,7,7,7,
6,8,9,6,7,7,7,9,7,7,8,7,6,6,8,6,9,7,11,9,
8,8,8,9,7,7,7,8,7,7,7,5,7,7,6,5,3,5,4,3,
4,2,2,3,4,2,2,5,4,3,2,1,2,2,0,1,3,3,3,4,
1,1,2,-1,-2,0,0,-1,1,-2,-1,-1,-1,-1,-1,-1,-1,2,-1,0,
2,-1,-2,-1,1,-1,-1,0,-1,-1,-1,-2,-2,-2,-1,-2,0,0,-2,-1,
-1,-2,-2,0,2,-2,-2,-1,-2,-2,-1,-2,-2,-1,0,-1,-1,-2,-1,-3,
-1,-2,-3,-1,-2,1,-2,-1,1,0,-1,-2,-1,2,1,0,0,-1,-3,0,
0,-1,-1,1,2,1,1,2,1,1,1,0,0,0,1,2,2,0,2,1,
0,1,3,1,1,2,2,2,1,2,2,2,3,0,2,3,1,1,2,3,
1,1,4,1,0,2,1,0,3,1,4,4,2,4,4,1,1,3,4,2,
3,4,2,4,2,3,2,4,2,4,1,2,3,3,3,2,0,2,2,1,
2,1,2,4,4,2,3,1,2,2,4,3,2,3,2,3,2,1,2,2,
3,1,2,3,3,3,2,5,3,6,4,2,4,6,3,2,5,6,4,4,
4,2,4,4,5,6,5,2,6,3,3,5,5,5,4,6,5,6,6,5,
3,4,1,2,2,2,2,2,3,3,0,3,4,3,4,5,4,3,3,5,
5,6,4,6,6,4,5,4,3,2,4,2,3,3,6,5,6,6,6,3,
3,4,2,2,3,3,3,3,5,2,3,2,2,3,1,4,1,4,5,5,
4,6,6,4,4,2,5,4,5,3,5,2,5,7,4,5,5,4,3,6,
2,3,4,6,5,6,6,4,5,5,4,5,4,3,6,1,4,6,4,5,
5,3,8,5,4,6,4,4,7,5,6,4,7,5,7,5,6,7,6,7,
6,9,4,7,4,8,4,6,7,5,2,4,5,3,4,4,0,3,3,3,
1,3,3,3,1,3,1,4,3,3,4,3,1,4,3,3,3,2,2,4,
1,2,2,4,3,4,5,4,5,7,5,5,5,7,2,5,3,3,6,7,
3,5,4,5,6,4,5,5,7,6,6,7,5,7,8,8,8,6,6,7,
6,7,9,6,5,7,7,8,6,7,8,8,7,5,7,8,8,10,8,7,
8,9,7,7,7,8,7,8,6,7,7,9,7,6,10,9,5,9,8,6,
5,8,7,7,9,7,5,5,7,6,6,6,7,8,4,7,5,6,5,5,
7,4,4,5,3,3,4,6,5,3,5,4,4,4,5,5,6,4,5,6,
6,5,4,6,3,5,7,5,3,7,8,6,7,6,3,6,6,5,4,6,
6,4,5,6,5,4,5,4,5,4,3,4,3,3,1,4,3,1,1,3,
2,1,3,1,0,0,0,-2,1,1,0,1,-1,0,-1,-1,0,0,-3,-1,
1,-1,2,1,2,1,1,2,3,0,1,1,0,-1,0,0,3,2,1,2,
0,3,2,3,2,0,2,3,1,3,1,3,1,2,2,0,0,3,-1,1,
-1,2,0,1,1,2,4,-2,2,0,5,2,2,3,0,2,2,2,2,3,
4,2,5,3,2,2,2,2,3,4,3,3,5,3,3,3,4,3,2,2,
3,3,4,3,4,2,2,5,4,5,2,2,3,3,3,3,4,3,3,2,
4,3,4,4,5,1,3,2,0,0,1,1,0,1,4,1,3,1,3,4,
4,0,-1,2,2,4,5,4,1,3,-1,2,1,-1,1,-1,-1,0,0,-3,
0,1,1,1,1,1,0,1,3,1,2,1,-1,1,-1,0,1,-3,1,0,
2,0,0,-3,2,0,-1,2,1,-1,-3,1,0,-1,2,1,1,2,2,2,
-1,5,4,3,4,6,6,3,5,3,5,4,4,1,3,3,3,1,2,3,
3,3,4,4,4,3,2,3,2,3,1,2,2,0,1,2,0,0,0,-1,
0,1,0,-2,-1,0,0,0,2,1,1,3,0,2,2,0,2,0,1,0,
2,3,0,0,-1,0,3,3,1,3,3,3,1,3,1,1,-2,2,-3,0,
0,0,-1,0,0,-1,1,0,-1,-1,1,-2,-1,1,0,0,0,-1,-2,0,
0,-1,1,-1,1,0,0,0,0,2,-1,3,2,-1,3,2,1,2,1,2,
1,-1,0,3,-1,2,2,3,0,3,2,1,4,2,4,5,2,4,5,6,
3,4,5,4,6,2,3,5,2,3,3,2,3,1,0,3,1,4,3,3,
2,3,4,4,3,5,3,2,5,3,3,6,3,7,5,6,3,5,6,6,
5,8,6,7,5,6,6,5,6,5,6,9,2,5,4,4,5,8,5,2,
3,4,2,3,6,3,2,3,5,2,6,3,3,3,6,4,3,2,5,6,
2,3,4,3,2,6,4,3,3,3,1,0,0,-1,-1,-1,0,-1,-1,1,
-1,-1,-1,-1,-1,-2,-1,-3,0,-2,-3,-4,-2,-3,-4,-5,-4,-1,-4,-3,
-2,-1,-4,-3,-3,-3,-5,-5,-6,-4,-7,-6,-6,-3,-3,-5,-7,-6,-5,-5,
-6,-2,-3,-3,-3,-5,-2,-2,-3,-4,-5,-5,-3,-4,-3,-6,-3,-3,-4,-4,
-3,-6,-3,-4,-2,-3,-2,-2,0,-2,-1,-2,-1,-1,0,-1,-2,0,-1,-3,
-1,0,-2,-1,-3,-4,-3,-6,-5,-4,-3,-3,-2,-3,-2,-4,-1,-2,-1,-1,
-3,0,-3,-2,-3,-2,-2,-2,-1,-1,-3,-2,-1,0,-3,-1,0,-2,0,-2,
1,-1,0,1,0,1,0,-2,-1,0,1,4,2,2,3,5,-1,2,5,2,
3,3,1,3,2,2,2,1,1,1,3,3,2,3,0,3,3,3,3,2,
3,4,4,2,3,4,3,3,3,0,3,5,2,5,4,4,4,4,4,4,
7,5,6,4,5,5,4,6,6,5,5,7,4,4,4,5,3,5,4,6,
4,3,5,5,4,4,3,2,2,3,4,4,1,2,4,3,2,3,3,3,
2,2,3,3,2,2,1,3,0,0,2,0,2,0,3,-1,0,1,1,0,
0,0,1,3,0,3,1,3,3,4,0,3,0,2,4,1,3,2,3,2,
2,2,2,2,4,2,4,4,4,6,4,4,2,4,3,3,4,4,4,4,
3,3,4,4,4,2,3,3,4,5,3,6,3,6,2,4,3,5,2,2,
4,4,1,2,2,3,3,3,2,2,3,4,4,2,3,2,0,1,1,2,
4,2,5,4,3,2,4,2,0,4,2,4,3,2,2,4,1,2,2,3,
2,6,4,3,3,4,2,2,2,2,4,2,3,2,2,3,0,0,1,1,
2,0,1,0,0,0,1,2,1,0,-1,2,0,1,0,-1,-2,-1,0,-3,
0,-3,-3,-2,-4,0,-2,-1,-1,-2,-1,-2,-2,-2,-2,-2,-2,-2,-1,-3,
-6,-3,-2,-4,-6,-4,-6,-5,-5,-3,-2,-5,-4,-5,-3,-4,-5,-5,-4,-4,
-4,-4,-5,-4,-2,-4,-3,-3,-4,-3,-2,-1,-2,-3,-2,0,-3,0,0,0,
-1,1,-1,-1,0,-2,0,0,0,-1,0,0,1,-2,1,-1,0,2,0,1,
0,2,5,2,3,1,4,3,1,5,4,2,3,4,5,1,4,4,2,3,
5,4,5,5,6,4,6,3,6,5,4,6,6,4,4,5,4,4,3,5,
5,5,3,5,5,3,4,4,5,3,6,3,5,5,6,5,5,7,6,5,
5,5,7,3,6,7,5,5,6,6,3,3,3,2,3,4,5,4,4,5,
5,5,3,4,2,2,2,1,2,2,2,3,2,2,2,1,2,1,2,1,
3,3,-2,2,3,1,1,1,1,3,2,0,0,1,0,0,2,1,1,2,
2,3,1,1,5,3,2,4,3,2,5,0,1,0,1,1,0,2,1,0,
1,4,1,2,2,1,3,-1,2,1,2,1,0,2,1,3,-2,2,2,2,
2,0,1,0,0,0,-1,2,2,3,2,2,3,2,1,1,1,3,2,3,
4,3,2,5,4,5,6,4,4,3,5,3,4,2,2,5,5,5,7,5,
6,5,7,5,4,4,6,7,6,6,6,5,5,4,4,5,3,4,3,5,
5,5,6,5,9,7,8,6,8,6,7,8,5,5,6,5,7,4,5,6,
6,4,7,5,5,5,6,5,6,5,3,3,6,3,5,6,4,2,3,3,
2,3,3,5,3,3,4,3,5,5,5,3,5,4,5,4,5,5,4,4,
5,5,4,5,4,5,3,5,5,3,6,4,5,5,5,4,4,3,3,5,
3,4,2,4,2,6,5,2,3,5,4,7,5,4,5,2,4,5,3,2,
2,2,3,1,2,0,3,0,2,0,2,4,0,2,3,3,2,4,2,3,
3,0,2,3,1,1,2,2,0,1,-1,1,0,0,0,3,0,2,1,5,
3,0,5,2,1,2,0,1,3,2,1,2,1,2,1,3,4,3,3,1,
1,0,3,2,3,2,3,2,1,3,4,1,4,5,5,4,4,7,4,3,
3,4,3,5,3,6,4,6,3,5,4,3,4,5,4,4,3,3,4,6,
3,4,5,6,4,5,4,6,8,4,7,6,5,6,6,6,6,8,7,5,
8,5,6,7,7,5,6,7,8,6,5,5,5,3,3,5,3,4,3,4,
4,3,4,3,4,3,4,3,3,4,3,2,3,2,4,3,1,1,0,1,
2,0,1,-1,-2,-1,0,-1,1,-2,-2,-3,0,-1,-1,-2,-2,-3,-1,-2,
-3,-4,-2,-3,-4,-4,-3,-2,-2,-3,-6,-5,-5,-3,-4,-3,-2,-2,-2,-2,
0,-4,-6,-4,-6,-4,-6,-3,-5,-4,-2,-3,-3,-4,-3,-4,-1,-4,-3,-3,
-3,-1,-4,-6,-3,-3,-6,-3,-1,-6,-6,-4,-8,-4,-4,-4,-4,-7,-4,-5,
-4,-2,-4,-4,-4,-4,-5,-3,-5,-3,-2,-3,-2,-2,-4,-3,-2,-3,-3,-3,
-1,-3,-4,-2,-2,-5,-3,-3,-4,-4,0,-2,-4,-2,-2,-4,-5,-2,-4,-2,
-3,-2,-4,-4,-2,-1,-5,-4,-6,-4,-5,-3,-2,-2,-4,-3,-2,-1,0,-1,
-1,-1,-2,-1,-3,-1,0,-3,0,0,0,0,0,-1,-1,2,0,1,0,0,
1,2,-1,1,1,1,3,3,1,0,2,1,3,1,2,4,3,3,4,4,
3,2,4,0,5,4,2,5,4,4,3,6,6,3,3,4,4,5,5,4,
4,4,4,5,6,5,3,5,5,6,1,6,4,4,4,3,2,4,2,2,
4,4,1,2,1,0,2,3,2,-1,3,1,2,0,-1,0,1,0,-1,0,
2,0,2,0,-1,2,0,1,-1,-3,-2,-2,0,-1,0,0,0,3,0,-1,
3,-1,0,1,0,0,1,2,0,0,0,-1,0,0,-1,-1,2,-1,0,-1,
-1,0,0,1,-1,0,3,0,-1,-1,0,1,-1,-2,0,-1,-2,0,-2,-1,
-1,0,3,0,0,0,-3,-1,0,-2,0,-3,-1,-3,-1,-3,-4,-2,-4,-3,
-2,-2,-2,-1,-2,-3,0,-4,0,-1,-2,0,-2,-4,-1,0,-2,-2,-1,-1,
-1,-3,-1,-1,-1,-1,1,1,-2,-1,-2,0,-1,0,-2,-2,-1,-1,0,-1,
-2,-2,-1,-1,-2,-3,-1,-2,-2,-3,-1,-2,-2,-1,-2,-2,0,-1,1,0,
2,0,0,2,1,0,2,2,1,3,3,1,4,1,1,3,0,2,2,1,
2,0,2,0,1,2,3,1,1,1,-1,0,1,2,0,2,0,-1,-1,0,
-1,1,0,-1,-1,-1,-1,0,-1,-1,1,-1,-1,0,0,0,1,0,-1,-1,
0,-2,-3,0,-2,-2,0,-1,0,-1,0,1,2,1,1,2,0,1,1,2,
1,1,2,0,1,-1,2,0,1,1,0,1,1,1,2,-1,2,1,1,3,
0,0,2,2,2,2,3,1,1,2,1,0,1,3,-1,3,0,2,1,0,
2,-1,3,2,1,1,2,1,3,3,2,1,1,1,1,1,2,2,-1,0,
1,0,-1,3,1,3,5,2,1,4,2,4,2,0,1,1,2,3,0,3,
1,1,2,2,1,1,1,0,-1,1,2,3,1,0,2,2,0,2,1,0,
1,1,3,3,1,1,1,3,2,3,2,3,5,3,4,6,2,2,3,2,
1,2,3,4,3,2,3,2,0,3,3,2,4,4,3,3,2,3,3,4,
3,2,5,3,4,1,2,3,2,3,3,2,2,2,1,2,2,2,0,0,
2,1,0,3,0,1,2,1,2,3,-1,1,1,2,1,4,3,3,2,1,
4,3,3,3,2,2,4,2,2,3,5,3,2,6,4,3,5,4,5,4,
4,5,3,2,3,4,3,3,3,2,4,4,6,3,5,2,5,3,3,5,
5,4,6,6,5,3,4,4,7,6,5,7,5,2,4,3,3,3,6,4,
3,6,5,4,2,5,1,4,2,3,3,2,3,2,3,0,3,2,1,1,
2,3,0,2,2,3,0,1,2,1,0,3,0,3,0,0,1,0,-1,0,
-2,2,-1,1,-3,1,1,1,3,0,0,0,-1,1,0,0,-1,-2,0,-1,
-1,-1,-3,-2,-3,-3,-3,-2,-2,1,0,-2,0,-1,-2,0,-2,0,0,-2,
-1,-2,-1,-2,-3,-2,-2,-2,-4,-4,-3,-3,-2,-2,-1,-4,-2,-1,-1,-2,
-3,-1,0,-1,-1,-4,-3,-1,-4,-3,-4,-3,-2,-1,-1,-3,-4,-2,-3,-2,
-1,1,-1,-1,-3,-1,0,1,0,0,-2,-2,-2,-5,-1,0,0,0,-2,0,
-3,-2,-2,0,-2,-1,1,1,0,1,-2,0,-2,-2,0,0,2,0,1,-1,
-1,-5,-1,-1,-3,1,-2,-1,0,2,-1,1,3,-1,1,-2,0,1,2,0,
0,2,0,-1,-3,0,1,-1,2,0,1,1,0,1,1,3,0,1,-1,2,
3,1,3,1,2,3,0,1,-1,0,-1,1,1,0,1,1,0,1,-1,1,
0,0,0,0,0,0,2,0,-1,1,-1,2,-1,-2,0,-1,-1,1,0,-3,
0,-1,-2,-2,0,-1,0,0,-2,-1,1,-3,-2,-1,-4,0,0,-1,-1,-2,
-1,0,-3,-1,-1,-1,-1,0,2,-3,0,-1,-1,0,0,1,0,0,0,-2,
-2,0,-2,-2,-2,-1,-1,-6,-1,0,-2,0,-3,-3,-2,-2,-4,-2,-1,-3,
0,-1,-1,-4,-1,-4,-4,-3,-3,-1,-3,-5,-6,-5,-2,-5,-5,-5,-5,-5,
-4,-5,-5,-4,-1,-4,-3,-4,-3,-3,-3,-3,-2,-1,0,-2,-1,-1,-4,-3,
-2,-3,-2,0,-2,-1,1,3,1,0,-1,1,-1,1,2,1,-1,0,2,-1,
1,2,2,2,2,3,2,1,3,3,6,4,4,5,6,3,5,7,4,5,
4,4,4,1,3,3,1,4,3,2,4,5,3,2,4,1,5,4,3,5,
3,3,6,4,4,4,6,4,4,6,6,5,6,4,8,5,5,5,7,6,
5,6,4,4,3,4,2,4,2,4,4,5,7,3,6,8,7,7,5,8,
7,6,8,8,7,7,4,6,9,5,8,6,6,4,6,7,4,7,6,5,
7,4,6,6,4,7,5,7,7,5,8,4,6,5,5,8,5,7,7,2,
6,6,4,4,7,5,6,5,6,5,6,5,5,7,7,7,5,7,9,8,
6,7,7,7,6,6,7,6,9,5,7,7,5,8,5,5,7,5,6,7,
8,6,6,9,7,5,7,6,4,7,5,3,5,2,4,5,1,4,5,3,
7,6,4,4,6,4,3,3,3,4,6,4,4,4,4,5,4,3,4,5,
5,3,7,5,4,5,5,6,5,4,6,5,5,7,6,5,4,6,5,5,
7,5,4,6,4,5,6,4,5,5,8,7,8,8,4,4,6,6,5,3,
5,3,5,2,4,2,4,4,2,3,4,3,4,1,2,3,4,3,3,3,
3,0,1,0,-1,0,0,1,-1,-1,2,-1,-1,0,-1,-4,-1,-2,-2,-2,
-3,-4,-4,-5,-2,-3,-4,-1,-2,-3,-4,-3,-1,-4,-2,-6,-5,-4,-4,-5,
-6,-2,-4,-3,-8,-6,-7,-5,-7,-8,-5,-8,-8,-9,-7,-4,-5,-7,-5,-5,
-4,-7,-6,-6,-6,-7,-5,-8,-7,-7,-8,-6,-7,-5,-6,-7,-4,-6,-6,-5,
-4,-8,-5,-3,-6,-6,-5,-5,-8,-7,-7,-4,-6,-9,-8,-7,-7,-6,-7,-6,
-7,-5,-5,-6,-4,-4,-5,-4,-5,-6,-7,-5,-6,-7,-8,-5,-4,-6,-4,-2,
-2,-2,-2,-2,0,-2,-3,-2,0,-3,0,-1,-3,-5,-3,-5,-3,-2,-1,-2,
-1,0,0,1,0,1,0,3,2,2,2,2,2,-1,1,0,0,1,1,0,
0,3,1,1,5,2,2,2,4,2,2,5,2,3,3,0,2,2,3,0,
1,1,2,-1,2,0,3,2,2,3,3,3,1,3,2,2,1,2,2,1,
3,2,3,3,2,3,1,1,-1,1,0,1,4,1,3,2,1,2,2,2,
0,2,2,1,1,0,1,0,1,2,0,1,1,1,-2,2,1,1,-1,2,
0,1,0,2,-1,-2,-2,-1,1,1,0,-2,-4,-1,-3,-1,-2,-3,-4,-5,
-3,-2,-4,-3,-2,-3,-3,-4,-3,-4,-2,-5,-3,-4,-4,-4,-3,-4,-5,-2,
-4,-2,-1,-1,-1,-2,-3,-4,-5,-3,-1,-4,-3,-1,0,-3,0,-1,-1,-2,
0,-1,1,0,2,1,0,1,0,1,0,3,1,3,0,3,2,6,2,4,
5,4,6,5,5,6,3,6,6,6,7,6,6,5,5,6,6,7,6,4,
8,6,7,5,5,4,6,7,5,7,5,7,6,4,5,4,4,6,3,5,
5,7,5,5,3,5,3,6,4,2,5,2,3,5,3,2,3,3,4,1,
2,4,3,2,4,4,4,3,4,4,3,4,4,4,6,5,5,6,5,4,
3,4,4,2,0,2,2,2,0,1,0,-1,0,2,0,2,1,3,1,2,
3,0,2,-1,2,0,1,1,2,0,-1,-2,1,-2,-2,-2,-1,-2,-3,0,
-2,-2,0,-2,-2,-4,-1,-2,0,0,-2,-1,-1,-1,0,-4,-1,1,1,-2,
0,-1,-1,-1,0,-3,-3,-1,-4,-2,-3,-2,-1,-3,-1,0,-2,0,-2,-2,
0,-1,0,-2,-3,-2,-3,-1,-2,-3,-1,-2,-5,-1,-1,-4,-2,-3,-1,-1,
-2,-1,-3,0,-2,1,-1,-2,0,-2,1,-1,0,0,-2,2,-3,-2,-1,-1,
-1,-2,-2,-1,-3,0,-1,-1,-2,-1,-2,-1,0,1,-1,-1,1,0,-2,1,
1,2,-1,1,1,-1,1,3,0,2,2,3,0,2,5,2,2,3,1,3,
4,1,2,2,1,2,4,2,2,2,2,2,1,2,2,1,3,0,3,0,
1,2,0,1,-1,-2,1,0,-2,0,0,-1,1,2,1,0,1,0,0,-1,
2,-1,2,-3,-2,-2,0,0,-1,-1,2,0,-2,-1,1,0,-1,-2,-1,0,
-2,-1,0,-1,-3,0,-2,-2,-3,-1,-2,-4,0,-1,-2,-2,1,-1,1,0,
2,1,0,0,4,1,1,0,0,0,2,-2,0,-2,0,2,2,-2,1,1,
-1,1,3,2,3,1,3,2,2,1,1,2,-1,1,0,-1,1,4,1,2,
2,3,4,7,3,4,5,4,3,5,4,5,3,3,4,2,4,3,4,5,
6,4,4,4,4,4,4,5,3,6,4,5,4,6,4,5,4,3,2,5,
1,2,4,5,4,4,2,1,3,5,2,1,4,3,2,2,2,2,-1,1,
1,2,1,2,-1,0,-1,0,-1,1,2,0,1,1,3,2,0,1,0,1,
1,-2,-2,-2,-3,-2,-2,-3,-2,-1,-1,-2,-1,0,-4,-2,-1,-5,-3,-3,
-5,-4,-2,-2,-3,-2,-2,-1,-1,-2,1,0,-1,1,1,-2,-1,0,-2,0,
1,-1,-1,1,-1,0,1,-1,0,-1,-1,2,0,-2,0,1,-1,-1,0,-3,
-2,-3,-2,0,0,-1,0,1,-1,-1,1,2,1,1,1,3,3,3,1,1,
0,-2,1,-1,0,1,1,1,-1,2,-1,0,0,1,1,0,3,2,0,3,
1,2,2,3,2,3,3,1,3,2,2,2,2,1,2,3,2,2,3,1,
2,3,2,0,0,2,4,2,1,2,1,2,0,-1,1,-1,0,2,1,1,
-1,2,2,0,0,-1,-3,0,1,0,2,2,-2,0,2,0,2,1,0,1,
-1,-1,-3,0,-4,1,1,-3,-1,-2,-2,0,-1,-1,-2,-2,-1,1,0,0,
-3,0,0,-2,-3,-3,-2,-2,-2,0,-2,-1,-3,-1,-2,-2,-2,-1,-3,-2,
-3,0,-1,-2,0,-3,-1,-2,-3,-2,-3,-3,-3,-1,-4,-2,-1,-4,-3,-4,
-3,-1,-1,1,-1,0,-1,-1,-1,-1,-3,1,0,-1,1,-1,0,1,3,1,
2,3,0,2,0,1,2,1,4,3,2,-1,3,1,0,1,2,1,2,2,
3,4,3,5,2,2,3,2,5,3,4,3,4,5,3,5,3,4,2,2,
4,5,4,4,1,3,2,2,3,4,5,2,3,2,4,3,3,5,6,3,
3,6,5,5,5,6,5,6,2,6,2,3,3,4,2,4,2,3,0,5,
4,4,4,4,4,7,1,2,5,4,3,4,7,4,6,3,4,4,5,4,
2,4,4,5,4,6,6,5,5,7,2,5,5,2,4,5,4,4,3,2,
5,4,5,4,4,5,3,7,7,4,3,5,4,3,5,2,4,4,3,3,
5,3,2,3,4,3,2,3,2,2,1,3,1,3,1,1,2,2,1,3,
2,1,3,3,3,3,1,1,1,3,1,1,1,1,2,2,1,3,2,2,
2,4,2,2,4,5,4,5,4,4,2,5,5,3,5,5,4,5,4,4,
0,4,2,3,5,5,3,2,4,2,2,3,2,2,3,3,4,4,2,4,
3,1,3,3,1,2,4,4,2,2,5,4,4,4,3,3,4,4,4,2,
3,4,4,5,3,3,3,2,1,4,2,2,3,1,2,2,3,0,2,2,
3,4,3,1,4,1,1,3,0,0,1,3,2,3,4,2,5,3,4,3,
3,2,4,3,1,3,1,0,1,3,1,2,2,1,1,1,0,4,1,3,
2,3,3,4,2,2,3,3,4,2,3,3,2,1,3,2,2,5,3,3,
4,3,2,0,1,2,0,-1,2,2,1,2,1,4,3,4,3,4,2,5,
3,7,4,5,6,4,5,5,7,5,4,6,6,3,6,4,5,5,5,2,
4,6,5,4,7,6,4,6,4,4,4,5,5,7,4,5,7,5,4,5,
5,4,4,3,5,3,5,3,4,2,4,4,0,4,1,6,3,2,4,2,
4,4,3,4,3,3,5,0,2,4,4,3,5,0,1,3,1,3,2,3,
3,3,3,5,1,3,3,2,2,4,5,4,3,5,3,4,2,0,2,3,
2,2,2,2,0,3,2,4,4,2,4,3,2,4,3,2,4,2,3,2,
4,2,4,2,3,3,4,4,3,3,1,4,3,4,4,4,3,6,4,5,
5,6,5,7,6,5,8,6,6,4,6,5,6,6,6,6,5,8,6,7,
5,6,7,8,6,7,6,8,8,8,8,9,6,9,8,6,7,7,7,7,
5,6,8,5,7,4,7,7,8,4,8,4,7,4,4,5,2,5,3,4,
4,1,2,4,4,5,3,5,3,5,4,3,4,3,2,2,2,1,4,4,
3,4,2,1,6,3,4,4,3,2,2,5,4,2,3,4,1,3,1,3,
2,4,2,0,2,2,4,1,4,1,2,0,2,1,3,2,2,2,3,2,
4,4,2,1,4,1,1,2,0,-2,-2,-1,-1,3,-1,2,0,-1,1,-3,
-2,-1,-1,0,2,0,-2,-1,-1,-2,-3,-2,-2,-1,-3,-5,-2,-5,-4,-4,
-4,-3,-2,-3,-3,0,-3,-2,-2,-2,-4,-1,-1,-2,-2,-4,-3,-2,-3,-2,
-5,-5,-6,-4,-5,-5,-4,-4,-3,-5,-3,-4,-2,-4,-3,-4,-4,-4,-4,-3,
-6,-4,-5,-4,-5,-5,-5,-8,-3,-5,-3,-2,-4,-3,-3,-1,-2,-2,0,-1,
0,0,-1,-3,-1,-2,-3,-2,0,-2,-2,2,-2,-1,2,0,-1,2,1,1,
2,2,1,1,2,1,0,1,2,3,0,0,1,1,-1,0,2,0,1,0,
3,1,-1,0,1,0,-1,-2,0,0,1,1,-1,1,1,0,-3,1,-1,1,
2,2,3,4,-1,3,5,1,2,1,1,2,3,4,3,3,4,3,2,4,
4,2,2,1,2,3,2,0,1,4,1,0,-2,0,-2,-1,3,0,1,-1,
-2,-1,1,-1,0,-2,0,-1,0,0,0,-1,2,0,2,3,0,-1,1,2,
2,4,4,2,2,2,2,2,5,0,2,0,3,4,5,4,5,3,5,5,
3,3,5,2,4,5,4,4,5,6,6,5,5,7,5,2,4,2,4,3,
2,4,1,4,5,4,5,5,4,3,4,5,5,6,4,6,5,4,2,3,
4,1,4,2,2,2,1,1,1,2,4,4,2,4,4,3,4,4,4,2,
3,5,1,4,4,3,2,3,3,3,3,0,2,1,2,1,2,3,1,2,
1,-1,-2,-1,-1,0,-3,-1,0,-2,1,2,1,3,2,3,0,3,2,4,
2,5,4,3,-1,3,2,0,1,4,1,2,2,2,3,4,1,3,-1,1,
1,3,1,2,1,3,5,3,7,4,5,5,5,7,5,3,5,5,7,5,
2,6,4,2,4,4,3,5,4,4,3,5,3,3,8,3,5,6,4,5,
5,4,4,5,5,3,5,6,4,3,5,4,4,2,4,1,5,3,3,3,
6,6,4,2,3,2,3,2,3,3,2,1,0,-1,0,-2,1,-1,1,0,
-2,0,1,1,-1,0,1,-1,0,0,0,1,2,-1,3,1,2,0,2,1,
3,0,2,2,0,2,3,2,2,3,2,4,2,5,4,5,5,3,2,2,
4,2,3,3,2,2,2,1,3,1,1,1,1,2,4,2,1,1,2,2,
2,2,0,2,0,0,2,-1,2,0,1,2,1,0,2,0,2,1,-1,1,
0,1,3,2,0,0,1,-1,-1,0,-1,-3,0,0,3,0,0,1,2,2,
1,4,1,4,3,0,1,1,1,0,1,1,1,3,1,0,5,3,3,4,
4,3,2,4,5,4,4,5,4,4,4,3,6,5,7,5,4,5,5,6,
7,5,6,5,3,3,5,3,1,3,3,2,0,3,3,2,2,5,2,2,
3,0,2,4,2,4,2,3,5,5,6,3,5,4,3,4,6,5,5,6,
5,5,5,4,3,3,4,7,4,5,4,6,6,5,6,5,4,5,7,5,
4,6,3,4,4,4,4,2,5,4,4,6,3,3,2,3,2,3,4,2,
6,6,5,5,8,4,5,5,6,5,2,2,2,3,1,3,3,3,3,4,
1,3,3,2,3,5,1,3,2,5,4,3,1,5,5,3,2,4,2,2,
4,0,3,2,3,3,4,3,2,2,6,0,2,3,2,3,1,2,0,-1,
-2,-1,-2,-1,0,-2,-1,0,-1,1,-1,0,0,-1,-1,0,-4,0,-2,-3,
-4,0,-1,-2,-4,-2,0,0,0,-1,0,1,-2,-1,1,1,1,-3,-2,-3,
-2,-3,-3,-3,-7,-4,-3,-3,-3,-1,-1,-2,-2,-3,-2,-3,-4,-2,-3,-4,
-4,-3,-5,-4,-2,-3,-4,-3,-3,-4,-5,-4,-4,-3,-3,-3,-3,-2,-5,-3,
-4,-4,-5,-3,-3,-6,-2,-3,-4,-3,-1,-2,-1,-3,-2,-3,-1,-2,-4,-2,
-1,-2,-1,-2,-3,-4,-1,-4,-2,-4,-3,-3,-1,-2,-4,-3,-4,-4,-3,-7,
-4,-5,-4,-4,-5,-2,-4,-5,-2,-2,-5,-2,-3,-2,-4,-4,-4,-2,-1,-2,
-4,-4,-5,-3,-2,-1,-3,-1,-2,-1,-3,-3,-4,-4,-1,-6,-3,-4,-4,-1,
-3,-4,-3,-3,-3,-3,-3,-4,-4,-3,-6,-3,-4,-5,-5,-5,-4,-5,-3,-2,
-4,-4,-3,-2,-2,-2,-1,-3,-1,-2,-3,-1,-3,-2,-3,-2,-1,-4,-2,-2,
-4,-1,-3,-3,-3,-3,-2,-2,-3,-1,-1,-2,-1,-4,0,-2,-2,0,-1,0,
0,-1,1,1,-1,0,-1,0,-1,3,-2,3,3,-1,4,2,2,1,3,0,
-1,2,-1,1,1,-1,-2,0,1,2,1,1,1,1,2,0,0,1,1,2,
0,2,0,0,2,1,0,0,2,-1,2,-2,2,0,2,0,0,-1,-1,-1,
0,1,-1,0,0,0,-1,1,0,1,0,2,0,4,2,2,2,3,4,3,
3,3,2,3,3,1,3,3,2,0,4,0,4,2,2,1,4,2,3,3,
3,4,4,1,2,1,3,2,2,4,3,0,4,4,3,4,4,3,5,5,
2,2,5,5,3,6,5,5,2,4,2,3,3,5,5,3,4,6,6,4,
3,4,4,3,6,1,4,5,3,5,1,2,3,4,3,5,2,5,2,4,
4,5,4,5,4,4,6,4,4,2,5,4,2,4,2,3,4,4,3,5,
6,5,5,6,6,7,8,7,7,8,6,7,9,6,5,6,6,5,5,5,
5,4,5,4,5,8,5,8,7,7,7,8,7,9,8,7,8,7,8,7,
6,4,6,8,3,6,9,7,5,7,8,12,10,8,8,7,7,7,7,6,
5,5,6,6,6,6,7,8,9,8,6,9,8,9,10,6,7,7,7,10,
9,7,5,7,7,7,6,7,8,7,10,8,9,7,9,10,9,8,10,6,
7,8,10,7,9,10,8,8,8,9,6,8,7,5,6,6,5,7,6,7,
9,4,5,4,4,4,3,4,3,3,5,3,3,5,4,4,4,4,5,3,
4,3,2,3,1,2,1,1,0,1,2,1,-2,1,0,3,2,0,1,1,
2,2,4,1,-1,2,0,2,2,-1,1,-1,1,1,1,1,1,0,1,0,
-1,1,0,0,0,0,-1,2,1,-1,-1,-1,1,-1,0,1,1,3,1,0,
3,3,2,3,1,0,3,4,5,3,5,3,3,3,4,3,3,0,3,3,
4,2,5,4,4,5,5,5,5,3,2,4,4,4,5,3,6,3,3,5,
5,3,5,3,3,4,2,1,6,3,3,4,5,4,2,5,4,3,1,4,
3,4,5,6,4,6,4,6,4,4,3,6,2,7,3,3,6,5,2,2,
0,2,1,2,1,2,1,0,1,1,0,0,1,1,0,1,0,1,-1,1,
1,-1,0,0,-1,-1,2,-1,-1,1,4,0,3,-1,1,-1,2,1,1,3,
1,0,1,2,1,2,0,1,3,4,1,-1,3,1,2,1,0,0,0,3,
0,0,2,0,0,0,-2,0,-1,2,1,0,0,0,2,-1,1,-1,1,0,
1,2,3,1,1,1,1,0,1,0,-1,1,1,1,0,1,0,0,0,0,
2,-1,0,1,-2,1,0,-2,1,1,2,-1,1,1,1,-1,2,2,0,1,
0,2,1,-2,1,2,1,0,2,1,0,2,0,2,0,3,1,1,4,5,
2,3,5,2,3,3,3,3,3,4,3,4,3,5,7,3,4,8,5,3,
5,5,5,3,7,4,7,8,6,9,8,8,9,6,8,9,7,5,6,6,
7,7,7,9,9,6,8,9,8,9,8,8,9,8,7,8,8,9,7,10,
7,8,7,7,8,8,7,8,7,8,7,11,10,8,10,8,9,8,9,8,
10,8,8,10,7,7,9,7,8,7,8,7,7,8,9,8,7,6,9,7,
8,7,6,7,5,7,5,7,5,6,4,5,6,6,5,5,8,6,8,7,
5,7,4,3,3,4,2,3,4,1,3,3,2,4,2,4,1,4,2,2,
3,2,2,2,0,0,1,-2,1,2,3,0,1,-1,1,0,1,-3,2,0,
-1,0,0,1,1,0,1,0,1,1,0,0,-1,-1,-1,0,-1,-4,-1,-1,
-2,-2,-3,-3,-2,-5,-1,-2,-3,-1,-4,-2,0,-3,-1,-1,2,-2,0,-4,
-1,0,-1,-2,-3,-2,-2,-1,-4,-2,-1,-1,-1,-3,-4,-2,-3,-2,-3,-1,
-1,-2,-3,-3,-5,-3,-1,-1,-4,-3,-3,-2,-3,-3,-2,-2,-3,-2,-3,0,
-2,0,0,-2,-3,0,-1,0,0,-2,-1,-1,0,1,1,0,0,0,-2,-1,
-2,0,-2,1,1,0,1,0,2,2,2,2,1,1,0,1,3,1,0,1,
0,0,0,2,4,0,0,-1,1,-1,-1,4,1,0,2,2,1,2,3,1,
0,2,0,1,1,1,-1,1,1,-1,3,2,3,1,3,3,0,1,4,1,
0,3,-2,2,0,0,-1,-1,-1,-1,0,-2,-2,0,-1,0,0,0,2,-2,
1,0,0,1,2,0,-1,1,2,-3,-1,-3,-3,0,-1,-1,-1,-1,-1,2,
-1,0,1,0,0,0,1,1,1,0,-1,-2,-1,-5,0,2,-3,-1,-2,1,
-2,-1,1,-2,0,1,-1,0,1,-2,-1,-2,0,-2,-1,-1,-3,-3,-2,-1,
-2,-2,-1,-2,2,1,-1,2,-2,0,0,-2,0,0,0,1,0,0,1,0,
2,2,2,-1,-1,1,0,-1,1,0,1,-1,2,0,-1,2,2,1,1,4,
1,1,1,4,2,3,4,3,4,4,4,3,0,3,4,3,2,5,3,4,
4,5,6,2,4,7,5,5,4,6,6,8,4,8,5,5,8,7,6,6,
8,5,6,9,8,7,11,10,8,7,10,7,9,9,8,10,7,8,8,8,
5,7,8,6,7,7,6,5,8,8,7,6,10,9,8,6,7,6,7,4,
6,5,4,4,7,8,6,8,8,8,7,9,6,9,10,6,7,7,6,7,
7,5,5,5,5,5,7,6,7,5,6,5,6,7,6,7,8,6,5,5,
7,7,3,4,1,4,3,1,2,1,2,2,2,2,1,0,2,3,3,2,
1,1,0,1,1,0,1,-1,-1,-1,1,0,-2,0,-2,0,-2,-1,-4,-5,
-2,-2,-3,-4,-3,-4,-3,-4,-3,-3,-4,-1,-3,-4,-3,-3,-2,-2,-3,-4,
-2,-5,-5,-4,-2,-5,-5,-2,-1,-2,-1,-2,-2,-2,-3,-2,0,-3,-2,-4,
-3,0,-3,-2,-4,-4,-3,-1,-1,-3,-3,0,-3,-1,-2,-2,0,0,-1,0,
0,-4,0,-2,-1,1,-1,0,-1,0,0,1,3,-1,0,2,1,0,2,3,
2,1,3,1,1,2,3,1,1,5,1,2,6,2,2,1,1,2,4,2,
1,2,5,2,4,4,3,3,3,2,2,3,3,3,5,1,0,3,3,1,
1,3,3,2,2,1,0,2,2,3,2,3,2,3,3,1,3,2,1,0,
1,5,1,4,2,1,1,1,1,4,2,0,2,2,0,1,4,2,3,3,
4,3,4,2,2,5,2,0,1,-1,1,0,0,-2,-3,-1,-2,-3,-3,0,
-1,-1,0,-1,1,2,1,-1,2,-2,0,-2,1,1,1,-1,-1,1,-2,0,
0,1,0,1,2,-1,-1,1,-1,1,0,1,1,1,0,0,-3,0,-3,-1,
-1,-1,1,1,0,-2,0,0,-1,-1,1,0,0,2,1,2,-1,2,0,0,
2,1,-1,-1,0,1,-2,-1,-1,0,0,2,0,-2,3,0,0,4,1,1,
2,1,-3,2,2,0,1,0,0,-2,0,-4,2,1,1,0,2,1,3,1,
2,0,0,1,1,4,1,3,1,2,2,2,1,0,0,0,-1,1,0,2,
0,-1,0,1,-2,-1,0,1,0,0,-2,0,1,-1,0,-1,-3,-3,-1,-2,
-2,-2,-3,-3,1,-4,0,-3,-2,-5,-3,-2,-3,-3,-1,-2,-2,-2,-1,-2,
-2,-5,-5,-3,-3,0,-3,-4,-1,-4,-3,-4,-5,-3,-5,-4,-2,-4,-2,-3,
-2,-4,-4,-2,-3,-3,-3,-4,-3,-3,-1,-3,-2,1,-1,-1,0,0,-1,-1,
-1,-1,-3,1,0,-2,0,-4,0,-3,-1,-1,-3,-2,-1,-2,-4,-2,-1,-1,
-2,-1,0,-1,1,-2,-2,-1,-1,0,-1,0,1,0,0,0,2,-2,-1,1,
1,0,1,1,-1,1,1,1,-1,3,-1,1,0,4,2,1,-1,0,1,1,
2,2,3,4,2,4,4,2,3,3,1,1,2,3,1,3,1,5,3,5,
6,5,6,6,5,5,4,4,6,2,4,4,1,3,3,2,2,3,4,4,
3,6,1,4,2,3,3,3,3,1,0,2,0,0,0,2,1,-1,2,1,
0,0,0,1,0,2,0,0,-1,-1,-1,-1,0,0,2,-1,0,0,-1,-1,
0,-1,1,-3,0,-1,-1,1,0,-1,-1,-2,-2,0,-1,-2,0,-3,-2,-2,
-5,-3,-3,-2,-1,-3,-3,-4,-1,-3,-3,-1,-4,-2,-3,-2,-3,-4,-2,-2,
-2,-4,-3,-4,-5,-3,-5,-3,-7,-5,-6,-6,-7,-6,-8,-5,-5,-5,-6,-7,
-6,-5,-6,-6,-2,-7,-6,-6,-8,-5,-7,-3,-7,-6,-4,-7,-5,-6,-6,-5,
-8,-5,-6,-5,-8,-8,-9,-11,-8,-9,-7,-7,-9,-7,-6,-8,-8,-7,-10,-11,
-9,-10,-9,-6,-9,-7,-9,-7,-7,-9,-9,-10,-6,-9,-10,-10,-10,-12,-9,-8,
-11,-9,-10,-11,-10,-10,-11,-10,-12,-9,-11,-7,-11,-8,-6,-9,-7,-6,-6,-9,
-6,-6,-8,-6,-7,-7,-8,-8,-8,-11,-6,-5,-7,-8,-8,-7,-8,-8,-6,-6,
-4,-8,-5,-8,-6,-5,-6,-7,-6,-5,-7,-5,-5,-3,-4,-4,-3,-1,-3,-5,
-2,-2,-2,-2,-2,-3,-2,0,-2,-2,-2,-3,-2,-1,-2,-2,-2,0,-1,0,
0,-1,2,1,3,1,2,2,-2,1,3,-1,2,1,1,0,2,1,0,1,
3,3,1,1,2,0,2,2,3,5,2,2,-1,2,-1,2,2,0,1,2,
0,-1,2,-1,1,2,-1,2,1,1,3,0,0,4,0,3,2,3,4,2,
1,3,1,1,2,4,2,3,4,4,2,4,4,6,4,4,5,3,4,4,
6,1,3,5,4,2,3,2,3,2,5,5,3,3,2,3,4,2,3,2,
0,1,2,-1,2,1,1,0,1,0,0,3,2,3,2,1,0,1,0,3,
2,0,1,0,-1,0,-1,0,-3,3,1,-1,1,1,0,0,2,0,0,-1,
0,-2,1,2,1,2,1,1,1,-1,-1,1,-2,0,-2,0,-2,2,-1,-1,
1,2,-1,0,1,0,1,3,3,2,0,2,3,-1,3,2,3,3,2,3,
2,3,1,3,3,4,3,2,6,4,1,4,5,3,5,7,7,4,5,5,
3,6,7,4,3,5,5,1,5,4,4,4,3,4,6,3,3,4,5,4,
5,7,6,5,6,6,4,4,5,6,6,6,7,7,5,6,7,6,7,7,
8,7,5,4,5,4,4,6,8,6,5,4,6,5,3,4,3,3,6,2,
4,5,3,2,3,3,3,3,3,2,2,2,2,3,3,2,2,3,3,0,
1,4,2,1,2,3,1,1,1,2,0,3,0,1,0,1,0,2,0,1,
0,1,-2,2,-3,1,-1,0,0,-2,0,3,2,1,-1,0,-3,-3,-1,0,
1,-1,-1,-1,-2,-2,1,-1,0,2,0,2,3,1,2,3,1,-1,3,4,
3,2,2,3,2,3,2,1,5,1,5,5,1,2,3,4,3,2,3,3,
4,2,2,4,3,6,2,2,4,1,0,4,6,3,3,5,3,5,8,4,
7,6,4,8,8,6,8,7,5,7,7,7,6,8,6,4,8,7,8,8,
7,8,6,8,5,5,6,6,6,5,4,5,4,5,5,5,3,4,6,7,
4,8,9,10,9,8,8,9,9,9,6,8,8,7,8,8,8,8,7,9,
7,8,8,9,10,6,7,9,10,10,6,9,7,7,6,6,8,7,8,8,
11,10,7,8,7,8,7,8,5,5,9,7,5,5,5,4,8,6,6,6,
6,6,6,6,3,4,4,3,2,5,3,4,5,4,3,4,5,3,4,3,
4,3,2,5,2,3,3,3,3,3,2,4,3,3,4,5,4,5,3,5,
5,4,4,2,2,3,5,4,4,6,2,7,4,4,5,4,3,2,2,3,
1,3,2,3,4,3,3,2,5,3,2,2,3,2,5,3,3,4,6,5,
4,6,6,4,4,5,3,4,4,5,6,3,4,7,4,7,6,8,8,6,
5,8,6,8,8,6,6,6,6,7,9,9,8,10,7,6,9,6,6,9,
10,6,9,8,8,9,7,6,8,7,6,7,7,6,7,8,7,7,5,6,
9,6,9,8,9,8,9,6,7,9,7,8,6,9,9,8,6,8,9,8,
7,8,7,5,10,8,8,9,8,8,5,9,4,6,7,6,5,6,6,7,
6,7,5,5,5,5,5,5,6,5,6,7,6,5,5,4,4,6,4,6,
8,8,6,8,7,8,9,6,7,7,7,7,6,9,6,7,8,8,6,4,
5,5,4,4,2,2,3,2,3,1,4,1,4,1,2,4,2,4,0,3,
1,3,1,1,0,2,0,1,1,0,2,0,3,0,0,2,4,0,2,0,
1,0,1,2,3,2,1,3,3,1,5,2,1,2,3,4,3,2,3,5,
4,5,6,5,3,4,3,5,6,5,4,4,5,5,3,3,3,6,5,3,
6,4,6,6,5,6,6,5,7,6,5,5,3,4,4,5,4,7,6,6,
6,5,7,6,7,7,9,7,5,5,7,8,7,7,10,8,9,10,6,7,
9,11,10,8,9,8,7,10,7,8,5,7,6,4,9,7,6,8,6,5,
9,7,8,6,8,8,9,7,10,9,10,11,9,6,9,9,9,8,7,7,
8,11,7,10,12,8,15,10,11,11,11,10,9,11,9,11,8,11,9,11,
8,8,10,8,9,8,9,9,8,8,8,10,6,8,10,6,7,9,6,7,
4,4,5,4,3,5,6,5,7,4,5,3,5,4,4,5,3,6,1,0,
3,3,1,1,0,3,1,1,0,-1,2,1,1,3,2,2,3,0,1,2,
2,1,2,0,0,0,0,0,1,-1,1,0,0,2,0,0,-1,0,-2,-2,
-1,0,0,-1,1,0,-2,-2,0,-3,-2,-4,-3,-3,0,2,-3,0,-4,-3,
-3,-2,-2,-2,-2,-2,-1,-1,-1,-2,-2,-3,-2,-3,-2,-3,-4,-4,-2,-5,
-2,-1,-2,0,-1,-2,-1,-2,1,0,-1,1,-2,-1,-3,1,0,-2,-3,-3,
-3,-1,-2,-2,-2,0,-3,-1,-1,-1,-2,-1,0,-2,-1,-1,-2,3,-1,0,
1,1,1,2,1,2,-2,-1,0,1,2,1,4,2,0,4,4,1,4,2,
3,1,3,1,3,2,3,5,1,3,2,4,2,4,2,3,4,6,5,4,
5,4,7,5,6,6,3,5,8,7,7,7,5,6,8,6,7,5,6,3,
6,7,7,5,4,3,6,6,4,4,4,5,5,4,6,3,4,3,6,7,
5,4,4,2,6,3,4,5,5,3,4,4,2,3,0,2,3,2,0,3,
3,3,2,2,0,2,1,3,2,2,1,3,1,2,1,-1,0,-1,-1,-4,
-1,-1,-3,0,0,-2,-1,-1,-2,0,0,2,0,0,2,0,1,0,2,-1,
-1,0,2,0,0,0,-2,1,0,-2,-1,0,-1,1,1,-2,3,0,0,2,
1,1,0,1,1,0,2,1,2,2,1,1,2,2,3,2,3,3,1,3,
2,3,2,2,4,1,3,5,1,1,2,2,3,4,3,1,4,1,1,3,
5,3,2,3,4,0,2,4,1,1,1,0,3,3,2,4,2,2,2,2,
3,3,4,3,4,3,2,2,3,5,1,3,1,1,0,0,0,-1,-1,1,
0,1,-3,1,0,-2,-2,-1,-1,-2,-3,-2,-2,-1,-1,-2,0,-1,2,-1,
-4,-2,0,0,-2,1,-2,-2,-2,0,-2,-1,-2,-2,0,1,1,0,-1,2,
-1,0,-1,-1,-1,-1,0,-1,0,-1,0,-1,0,1,-2,2,-1,0,0,2,
1,1,1,0,1,3,-1,2,2,1,4,-2,0,1,-1,2,2,0,1,2,
2,2,1,1,2,4,4,5,3,2,3,2,2,1,4,1,1,3,2,2,
1,3,3,5,3,3,2,2,4,3,1,1,2,0,2,2,2,3,2,2,
2,4,1,4,3,2,5,5,1,4,3,3,3,4,2,4,3,2,3,3,
2,3,3,1,2,1,2,3,0,1,2,2,2,3,0,1,1,1,-1,2,
2,2,2,0,1,-1,2,3,2,1,-1,1,0,-1,0,3,2,1,2,1,
0,1,0,2,-1,1,0,1,1,-1,3,1,2,1,0,1,1,1,0,1,
3,2,1,2,2,2,2,1,0,1,0,0,-1,0,1,1,2,-2,-1,0,
0,-4,0,-1,-2,-2,0,-2,-2,-2,-3,-4,-4,-1,-2,-4,-2,-3,-2,-1,
-1,-2,-3,-1,-3,-3,-2,-3,-2,-5,0,-3,-4,-2,-2,-2,-1,-2,0,0,
-1,0,-1,1,0,1,0,-2,0,1,0,-1,1,2,1,-1,2,2,2,2,
4,1,2,1,3,2,1,4,1,2,2,4,3,1,4,3,3,2,4,3,
3,5,2,3,5,2,4,2,4,5,2,1,4,6,3,2,4,5,5,2,
3,3,5,1,3,4,0,3,0,-1,2,2,0,3,1,2,2,4,1,4,
3,3,3,5,0,5,3,3,2,1,3,3,4,4,2,2,2,4,2,3,
2,4,4,3,2,5,2,2,5,2,1,2,4,3,2,4,3,2,2,4,
4,2,5,5,2,4,1,3,1,2,3,3,2,5,2,3,4,3,3,3,
2,2,1,2,3,2,1,3,2,1,3,3,0,0,2,1,3,1,2,3,
2,4,2,3,3,3,4,4,3,3,3,6,3,3,5,3,2,2,3,3,
3,4,4,1,4,1,2,2,2,3,2,4,1,2,4,1,3,3,1,4,
1,1,2,1,4,2,4,3,3,3,3,3,2,0,3,2,1,1,2,1,
2,4,1,3,2,2,3,3,2,2,3,2,3,1,1,2,2,0,2,2,
-1,-1,-1,-1,1,-1,0,-2,0,2,3,1,1,3,0,0,3,3,2,1,
2,3,2,1,2,5,5,0,5,2,3,0,2,1,4,3,4,3,1,3,
3,2,4,3,4,3,3,5,2,4,1,3,5,6,2,4,4,4,4,2,
4,2,2,1,2,4,2,0,2,0,2,2,-1,-1,0,1,-1,-1,-2,-2,
-1,-2,-3,-3,-2,-1,-2,-3,-2,-2,-4,-2,-2,-1,-3,-3,-2,-1,-3,-5,
-1,-5,-5,-3,-1,-4,-4,-3,-3,-4,-1,-3,-4,-4,-3,-3,-5,-3,-3,-4,
-5,-1,-4,-3,-3,-4,-3,-3,-4,-4,-1,-4,-2,-6,-5,-4,-4,-4,-5,-3,
-4,-2,-4,-2,-2,-4,-2,-3,-4,-4,-5,-4,-3,-5,-2,-5,-3,-6,-4,-7,
-4,-6,-5,-3,-6,-3,-3,-4,-4,-4,-2,-4,-3,-5,-3,-5,-4,-4,-3,-3,
-4,-3,-5,-3,-5,-2,-6,-4,-4,-4,-5,-3,-6,-5,-2,-2,-5,-2,-1,-3,
-5,-2,-2,-5,-2,-4,-3,-4,-2,-5,-5,-5,-3,-2,-2,-3,-2,-3,-2,-3,
-4,-1,-1,-4,1,-1,-1,0,0,0,-3,-1,0,-2,0,0,1,1,0,-1,
1,0,-1,0,0,0,-1,-1,-1,0,0,-2,0,2,0,2,-1,2,3,1,
3,0,2,3,4,5,2,2,3,4,4,3,3,1,4,1,1,2,3,1,
3,1,2,1,2,4,1,3,3,5,3,2,1,2,0,4,4,2,4,4,
5,2,3,4,4,5,1,2,2,3,2,1,2,2,2,2,0,1,2,0,
3,1,3,2,-1,0,0,2,0,-2,-3,-1,-1,-4,-1,-2,0,0,-1,-1,
-1,-2,-1,-3,-2,-3,-2,-4,-3,-2,-3,-4,0,-2,-2,-4,-2,-3,-6,-2,
-4,-4,-2,-5,-2,-3,-3,-4,-3,-3,-4,-2,-5,0,-3,-4,-1,-2,-5,-5,
-4,-5,-6,-5,-4,-3,-5,-4,-6,-5,-4,-1,-1,-1,-2,-3,-2,-1,-1,-4,
-1,-2,-1,-3,-3,-2,-2,-3,-1,-1,-3,-2,-4,0,-1,-1,-1,-1,-2,-1,
-2,-1,2,-2,-1,-1,-1,-2,-1,0,-2,-1,-1,0,2,0,1,-1,1,0,
-1,-2,-1,0,-2,0,-1,1,-1,0,0,0,-3,-3,0,0,-1,-1,-3,0,
0,0,-2,2,-1,-2,-2,0,1,-1,-1,1,-2,1,-1,0,0,0,0,1,
-1,-2,-1,-2,-4,-2,-4,-1,-4,-2,-2,-3,-3,-2,-2,-3,-3,-3,0,-2,
-1,-2,-3,-3,-3,-1,-3,-2,-2,-4,-2,0,0,-1,-3,-1,-3,-2,-1,-1,
-2,-2,1,-3,-1,-2,-4,-3,-2,-4,-2,-3,-3,-2,-1,0,-1,-2,0,-1,
-2,-3,-3,-4,-1,-1,-2,-3,-3,-2,-2,-5,-3,-3,-2,-2,-2,-1,-1,0,
-1,-2,0,-1,1,0,2,-1,0,0,0,2,0,-1,-1,3,-2,0,0,1,
0,3,2,-1,2,-1,-3,-1,0,1,1,1,5,2,2,4,2,0,2,2,
0,0,1,2,-1,0,2,1,0,2,1,3,0,0,1,2,1,4,1,2,
2,-1,1,1,1,1,0,2,2,2,1,1,1,1,2,2,3,3,3,5,
1,2,2,0,3,2,3,4,3,2,3,3,3,1,1,3,1,2,2,2,
-1,4,0,0,0,4,1,1,2,1,3,2,2,0,1,1,2,2,0,0,
2,3,3,3,1,2,2,3,1,2,3,2,2,2,3,0,3,2,3,0,
4,2,4,1,1,1,3,1,0,2,2,2,2,3,1,0,3,0,0,2,
-2,1,-1,1,0,-4,0,-1,-2,0,1,-1,2,0,2,4,1,3,2,2,
2,1,1,3,-1,3,0,2,1,0,2,2,3,1,-1,0,1,-1,4,1,
1,2,1,2,3,2,1,0,2,0,1,1,1,-1,1,2,-1,0,2,1,
1,0,2,2,2,3,0,1,3,2,2,2,2,2,3,2,4,3,2,2,
2,2,3,1,4,1,1,2,4,2,5,2,4,5,5,3,3,5,3,4,
4,7,6,4,6,5,7,6,2,3,5,7,5,5,4,6,5,4,3,5,
3,3,4,2,2,4,2,3,4,5,2,6,4,2,3,3,4,1,3,3,
4,3,5,4,3,1,2,3,2,1,2,4,1,1,3,2,3,4,3,2,
4,2,1,2,2,4,1,2,5,4,2,3,3,5,3,4,2,3,1,4,
0,3,3,1,2,-2,1,-1,0,0,1,-1,1,2,0,-2,0,0,-1,-1,
-1,0,-2,-1,-2,-2,0,-2,-1,2,-1,-3,-1,-2,1,-2,-2,-1,1,0,
1,0,1,0,-1,0,-2,-1,-2,-2,-2,-3,0,-1,-2,-4,-3,-3,-3,-2,
-1,-1,-1,-1,-2,-3,1,0,2,-1,1,-1,0,-2,-1,-1,-1,-1,0,-1,
-1,1,-1,-1,-3,-2,1,-1,-2,-2,0,1,-2,0,-1,0,0,0,-2,-2,
-1,0,-1,0,0,0,0,-1,0,2,-2,1,1,0,0,0,-1,-1,-1,-3,
-1,0,0,-3,-2,-1,-2,0,-3,-2,-2,-2,-1,-2,-1,-1,-1,-1,-1,0,
0,-1,0,-3,-1,-1,-2,-2,-3,0,-2,-3,-2,-1,-3,-2,-3,-1,-1,-1,
0,-2,1,-1,0,0,3,0,2,0,1,-4,0,2,-2,0,-2,0,-2,-1,
0,-2,2,0,1,1,2,-1,1,-2,1,0,0,0,0,0,0,0,-1,2,
-2,3,1,1,4,3,3,2,2,4,3,4,0,2,1,2,3,0,3,1,
1,3,1,0,2,1,2,3,0,3,1,0,3,1,3,2,2,2,1,0,
2,1,0,1,0,0,0,2,0,1,0,1,0,-1,1,-1,2,-1,-1,0,
1,1,0,0,-1,1,-1,-2,-1,-2,0,-2,-1,0,-1,0,-1,-1,-1,-1,
-1,0,-2,1,-2,1,2,0,2,1,0,-1,1,-1,-1,-1,-3,0,-2,1,
0,-2,-2,0,0,-2,0,-2,0,-3,-2,-1,-2,-4,-1,-3,-2,-2,-4,-2,
-2,-4,-1,-4,0,-2,-1,-3,-1,-4,-1,0,-2,-1,-1,-2,-2,0,-1,-2,
-3,-1,0,0,-3,-4,-5,-4,-2,-5,-1,-4,-1,-2,-1,0,1,-1,-2,-1,
-3,-2,-2,-2,0,-3,-3,-3,-4,-2,-1,-3,-3,0,-3,-3,-3,-1,0,0,
0,-1,0,0,1,-3,-1,0,-1,-1,0,0,-1,0,1,3,2,3,3,2,
1,0,3,5,4,4,5,4,3,4,4,3,3,3,6,4,3,3,5,2,
3,5,4,4,3,1,5,5,5,2,6,5,5,6,4,3,5,3,4,6,
6,4,5,2,3,5,4,4,4,2,1,3,5,6,3,4,3,5,4,2,
6,3,6,3,6,5,7,8,6,7,6,4,6,5,3,8,5,6,5,5,
5,5,5,5,5,6,5,4,4,5,6,5,7,6,6,5,7,4,7,5,
9,6,5,10,6,7,7,8,5,7,6,8,4,5,6,5,3,7,3,6,
6,5,3,7,3,5,4,4,5,2,3,3,6,5,6,5,4,4,4,3,
2,5,3,2,3,4,2,3,5,3,4,3,5,4,2,4,5,3,5,7,
4,4,3,3,2,5,3,2,5,2,5,3,6,3,5,5,3,3,3,4,
2,3,2,3,4,3,3,2,0,3,4,4,4,4,1,5,3,2,3,3,
3,1,2,1,5,3,0,2,3,1,1,2,2,4,3,1,0,2,0,0,
2,0,0,1,0,3,2,2,5,4,1,1,2,1,1,2,1,0,3,4,
2,2,0,2,1,4,5,2,4,2,3,5,4,4,4,3,2,5,4,4,
3,5,4,5,3,4,0,3,4,2,3,4,4,1,3,2,4,2,2,2,
3,2,1,3,2,0,-1,2,2,3,0,4,2,0,4,2,1,2,0,1,
1,3,1,3,2,2,3,3,0,1,0,3,2,2,1,3,4,1,4,5,
3,4,5,1,6,5,5,3,4,5,6,3,3,4,2,2,6,6,4,4,
5,5,5,4,4,5,2,1,3,4,-1,4,2,2,0,1,1,1,3,2,
1,0,2,2,1,2,1,1,0,-1,0,3,1,2,0,0,0,0,-1,0,
0,0,0,0,0,0,1,4,2,0,0,2,-1,2,1,1,3,3,-1,3,
1,3,2,4,4,4,2,5,2,2,1,3,4,4,5,4,2,5,5,4,
4,3,3,3,1,2,3,4,2,4,5,5,5,4,5,4,3,2,3,5,
3,4,4,1,6,3,3,7,4,3,4,4,0,1,4,3,4,1,-1,1,
1,1,-2,1,0,1,-2,-1,0,0,-2,1,3,0,0,1,0,0,2,1,
-1,0,-1,-2,-2,-2,-2,-1,-2,-1,-1,-2,-2,-2,0,-1,1,-3,3,0,
1,-2,0,0,-1,-1,-2,0,-2,-1,-1,-1,-3,1,-2,-1,-1,-1,-1,0,
-2,1,2,2,0,-1,1,-2,1,0,-1,-1,-1,-1,0,1,-1,2,1,1,
2,3,1,1,1,0,2,3,5,2,4,-1,1,0,2,0,3,1,1,1,
2,4,3,3,5,5,4,3,2,1,0,0,1,2,2,1,0,-1,-1,1,
4,0,2,3,0,0,3,1,0,1,2,2,1,1,2,0,1,0,-2,-1,
0,-1,1,1,-1,1,0,0,3,2,1,2,1,0,1,1,2,1,3,-1,
-1,1,1,-1,2,-1,0,-1,0,-2,0,0,0,0,-1,-1,0,0,-1,0,
-1,-1,-1,-1,-1,1,0,0,3,1,2,2,1,0,2,3,2,3,0,0,
4,1,0,0,3,0,1,1,1,2,0,4,1,0,-1,1,0,2,2,1,
-1,2,1,1,0,-1,-3,0,-1,-1,0,-1,-1,-2,0,-1,0,-1,0,-1,
0,0,-1,-3,2,-1,-2,-2,2,1,-1,-2,-2,-1,-2,-1,-3,-1,0,-2,
-1,1,2,0,-2,0,-1,-2,-2,0,-2,-2,0,-1,-1,0,1,1,1,-2,
0,0,1,-1,-1,0,1,0,-1,0,-1,1,-1,-1,-1,-2,1,0,-1,2,
1,2,0,2,-2,1,0,-1,-1,-1,0,1,1,0,1,1,2,2,0,-1,
0,-1,-2,0,0,1,0,2,2,1,2,0,-1,3,0,2,3,0,2,3,
2,0,-1,2,1,1,2,0,1,0,2,2,2,0,0,1,0,0,1,3,
3,0,2,0,0,3,1,2,2,5,1,5,3,3,3,4,4,2,3,2,
2,2,4,4,3,1,3,4,1,4,3,2,3,4,5,3,5,3,3,3,
2,5,3,2,1,5,1,3,0,3,3,3,4,4,5,2,5,2,2,4,
5,3,5,4,3,4,2,1,4,4,1,2,3,4,4,3,3,3,5,4,
4,2,2,5,2,2,3,2,4,4,3,4,3,6,2,4,4,2,2,3,
2,1,3,1,0,1,-1,-1,0,2,1,0,0,1,2,-2,0,-2,0,-1,
-1,-2,-2,-1,-1,-2,0,-3,2,-1,-3,-4,-3,-2,-1,-2,-1,-2,-2,-1,
-3,-3,-2,-4,-2,-1,-3,-2,-1,-3,-1,-2,-4,-1,-3,-5,-2,-2,-2,-1,
-2,-3,-3,-2,-2,-3,-1,-3,-3,-1,-2,0,-2,-2,-4,-2,-2,-3,-3,-2,
-4,-1,-1,-1,-2,-2,-1,-2,-1,-1,-1,-2,0,-1,0,0,-2,-2,0,1,
2,-1,2,-2,2,3,-1,1,3,-2,-1,0,1,-2,0,1,2,0,2,-3,
-2,0,-2,-1,0,-1,0,1,2,-1,1,2,3,2,2,1,2,-1,-3,3,
0,-2,-1,2,1,3,3,3,1,3,3,2,2,3,2,4,1,4,1,4,
3,0,2,3,0,2,2,1,3,1,1,5,1,4,4,2,2,1,2,2,
0,2,0,1,0,-1,1,0,0,-1,-1,-1,-1,-1,1,0,1,0,1,-1,
0,-1,0,1,0,0,2,1,2,-1,1,0,0,1,1,2,0,1,-1,-1,
-1,-1,-3,-1,0,-2,0,0,-2,0,1,1,1,1,1,1,3,2,0,1,
2,3,1,0,0,1,1,4,1,1,2,1,1,3,2,2,4,1,1,2,
5,2,7,4,5,5,5,5,7,8,6,7,7,6,6,6,7,6,7,5,
7,4,7,7,4,5,9,5,4,8,7,6,6,6,5,6,4,5,5,5,
4,4,4,3,6,6,5,7,5,6,5,3,7,4,2,5,5,5,3,6,
5,3,5,6,5,8,4,6,5,5,5,6,7,6,6,7,7,4,7,5,
3,5,3,5,6,5,6,4,5,8,6,6,7,4,5,8,4,7,6,6,
6,5,5,4,6,9,6,10,5,9,7,5,6,5,8,5,5,7,6,9,
7,7,6,6,4,7,4,4,6,5,4,5,3,4,4,2,2,2,4,3,
2,3,1,4,1,2,2,2,2,-1,1,0,2,-1,-1,3,1,-1,-2,1,
1,2,0,-1,0,0,-2,-2,1,-1,0,0,-3,0,-1,-2,-2,-2,-2,1,
1,-2,0,-1,-2,1,-1,-1,2,2,2,0,1,1,2,0,2,4,0,0,
4,3,1,3,2,3,1,4,0,1,2,0,0,-1,-1,2,-1,0,1,1,
0,1,0,1,0,3,2,0,1,-1,1,0,2,2,0,-4,1,-1,-1,0,
1,0,0,1,2,-1,2,-1,1,2,-1,0,-1,-1,-1,0,-3,-1,1,-1,
-3,-2,-1,0,-2,1,-2,-1,-2,-1,-4,-1,-2,-2,-3,-3,0,-5,-3,-3,
-3,1,-1,-2,-2,-2,-1,0,-1,-2,-1,-3,-1,-2,-2,-3,-3,-3,-4,0,
-4,-3,-2,0,-1,0,0,-3,-1,-1,-2,-1,-1,-1,-1,1,-2,1,-1,-4,
-1,0,-4,0,-1,-2,-3,-3,-2,-3,-4,-5,-4,-4,-3,1,-3,-2,-4,0,
-4,-2,-2,-3,-3,-3,-2,-1,-3,-1,-2,-2,1,-1,-3,-3,-2,-1,-3,1,
-1,0,0,-2,1,1,-2,1,0,-1,2,0,2,-1,0,2,-3,-1,0,0,
1,2,3,0,-1,-1,-1,1,-1,0,0,-1,2,-1,-2,0,0,0,-1,3,
0,2,-1,2,1,3,0,2,0,1,-3,0,-2,-2,0,-2,1,0,3,1,
0,2,0,2,2,0,2,1,4,2,1,0,1,0,1,0,3,1,2,-1,
2,2,0,0,1,1,0,2,5,3,6,3,4,1,5,3,4,2,4,4,
3,4,6,4,5,3,4,6,6,4,6,4,5,4,3,3,4,6,5,5,
3,3,5,5,3,4,4,4,8,5,4,3,5,5,3,6,5,6,4,5,
4,3,4,6,4,6,6,0,3,4,4,2,5,5,5,4,4,3,5,2,
2,2,1,0,2,2,3,0,3,1,3,5,2,2,3,4,5,3,4,3,
2,3,3,4,7,5,6,4,5,4,6,4,4,4,4,6,5,3,4,5,
5,5,6,4,5,4,8,4,5,8,5,7,7,6,7,7,7,6,7,9,
6,8,7,7,7,9,8,6,8,8,9,8,9,7,9,7,6,7,6,8,
8,6,7,5,7,7,8,5,5,5,6,6,6,6,7,6,4,5,6,4,
5,5,3,4,3,5,2,4,3,5,5,5,5,4,5,3,6,4,4,3,
5,2,4,2,3,3,1,1,3,1,1,2,2,3,4,2,4,2,1,1,
-1,1,1,0,-1,0,2,-1,0,1,0,-2,0,-4,-2,-2,0,0,-3,-1,
3,1,1,0,2,1,3,3,2,3,4,2,2,-1,1,-1,2,0,3,4,
1,2,1,1,0,-1,2,3,3,2,1,3,1,4,2,1,3,0,1,0,
2,2,4,0,3,1,1,0,1,1,0,0,1,0,0,2,3,0,-1,0,
-1,0,-1,-1,-1,-2,1,1,0,0,2,1,0,2,0,1,1,2,3,1,
0,1,0,3,1,2,0,0,1,0,-1,0,1,-2,-2,-1,-2,0,-3,-1,
-3,-2,-1,-1,-4,-1,-4,-5,-3,-3,-4,-6,-5,-5,-5,-4,-2,-3,-4,-3,
-5,-3,-2,-3,-2,0,-1,-5,-6,-4,-4,-3,-2,-4,-4,-3,-4,-3,-5,-4,
-5,-5,-3,-4,-5,-3,-4,-3,-4,-3,-4,-5,-2,-4,-2,-1,-1,-2,0,-1,
1,-1,1,-3,-1,0,-1,1,1,1,-1,1,1,2,1,-1,0,-1,0,2,
1,2,1,0,2,3,3,2,2,1,1,2,1,2,3,0,2,2,-1,2,
3,5,5,5,5,6,6,6,4,4,6,4,4,6,5,5,3,8,6,5,
8,6,5,3,6,4,7,6,6,8,5,5,8,5,8,9,5,7,8,7,
6,6,5,6,6,5,4,8,4,4,6,4,6,4,4,5,5,4,5,4,
3,5,3,3,4,2,2,2,4,0,4,3,2,3,3,4,3,4,4,3,
4,3,4,3,3,2,3,2,1,3,5,4,1,1,3,2,4,4,2,4,
3,5,5,5,3,5,5,6,6,8,6,5,6,5,4,7,7,6,6,6,
6,7,8,8,9,8,8,9,9,9,9,10,9,11,10,10,8,9,9,9,
8,6,8,8,6,8,7,11,9,8,6,9,6,8,4,7,3,3,3,4,
4,5,5,4,6,6,5,4,3,2,1,2,1,2,0,4,4,1,3,1,
0,2,4,1,3,-1,1,-2,-3,-2,-4,-2,-3,-1,-2,-4,0,-2,-3,-2,
-2,-1,-2,0,0,-2,0,-1,-2,-2,-3,-3,0,-1,-1,-1,-1,1,0,1,
1,-1,1,0,-2,-1,-1,0,-2,-1,-2,-1,0,1,0,-3,1,0,-3,-2,
-1,-2,-2,-3,-3,-1,-2,-2,-1,-2,-4,-4,-4,-2,-1,-3,0,-2,-2,-1,
0,-2,-2,1,0,0,-2,-1,0,-1,-1,0,3,-1,-2,3,-2,0,-1,2,
-1,-1,0,-1,-3,-1,4,-1,2,2,3,2,2,3,-1,3,2,2,1,2,
1,6,4,4,2,3,2,3,1,4,1,3,1,4,2,4,4,2,5,2,
2,4,3,4,3,4,3,4,4,3,3,1,3,3,7,4,3,3,2,4,
3,4,4,4,2,5,5,5,8,5,5,5,4,5,4,4,3,2,6,3,
4,4,5,4,6,4,4,7,7,6,5,6,5,6,4,5,4,6,6,6,
7,5,5,4,5,3,4,1,4,4,4,2,3,4,2,3,4,3,5,4,
5,6,4,4,4,4,3,3,4,4,4,5,4,6,5,7,3,4,5,6,
6,5,5,5,5,4,1,4,3,2,2,2,2,4,2,4,4,4,4,4,
4,1,3,2,3,4,3,3,2,2,0,4,3,1,4,3,0,3,2,4,
2,4,1,3,1,1,1,4,1,1,2,2,2,1,1,2,-1,0,1,1,
2,3,0,1,-1,1,-1,1,2,2,0,5,1,2,2,0,0,1,-1,3,
0,1,3,1,0,-1,2,0,1,0,0,1,0,1,1,-1,0,1,0,-1,
0,0,0,0,0,0,2,3,-1,2,3,2,3,3,2,3,2,2,0,1,
0,2,1,-1,2,0,1,4,1,3,3,2,3,3,2,3,4,4,-1,2,
3,3,0,0,-1,-1,1,-2,-2,-1,0,-2,-1,-1,-1,1,0,2,1,0,
1,-1,1,-1,-2,0,3,0,1,1,2,0,1,1,0,2,1,1,1,3,
2,1,0,3,2,1,-1,0,1,0,1,4,4,4,3,1,4,-1,1,3,
3,2,2,-1,1,-2,2,-1,0,-1,-1,0,-1,-3,0,1,-2,-2,-2,-1,
0,0,0,-1,1,1,1,0,1,-1,1,-1,-1,0,1,1,0,0,-1,1,
0,0,0,1,1,2,2,2,3,1,2,3,1,-1,3,-1,2,0,1,3,
-2,0,-2,-1,-2,0,-1,-3,-2,-1,-3,-2,-2,-6,-4,-4,-3,-4,-2,-3,
-3,-2,-4,-2,-2,-3,0,-2,-2,0,-4,-2,-1,-2,-1,-2,-4,-4,-3,-4,
-3,-4,-2,-2,-1,-3,-2,-1,-1,-2,-1,0,-1,2,-1,-4,0,-1,-1,1,
-1,1,1,1,2,1,1,0,1,0,2,1,1,2,3,3,6,2,2,3,
2,3,4,2,3,1,2,2,3,3,1,2,2,5,4,4,3,4,4,4,
3,4,2,-1,3,4,2,2,3,3,4,4,2,4,2,1,5,0,0,2,
3,1,0,-2,1,-3,-2,-2,-1,-2,-1,-2,-4,-1,-2,-4,-2,-2,-4,-5,
-7,-3,-6,-6,-6,-6,-6,-9,-6,-7,-8,-8,-6,-8,-7,-8,-6,-8,-4,-5,
-8,-6,-4,-4,-5,-3,-6,-2,-4,-3,-2,-4,-5,-2,-1,-4,-3,-4,-3,-1,
-3,-2,-1,-2,-1,-3,-3,-4,-5,-6,-4,-6,-5,-7,-4,-6,-6,-3,-5,-5,
-5,-4,-4,-1,-2,-1,-3,-3,-3,-2,-1,-2,-1,-2,-5,-2,-4,-2,-4,-2,
-4,-5,-6,-7,-5,-4,-5,-4,-5,-3,-5,-1,-5,-4,-5,-6,-6,-4,-4,-2,
-3,-4,-4,-2,-3,-3,-4,-7,-5,-4,-3,-5,-3,-2,-3,-2,-3,-2,-5,-5,
-5,-6,-5,-5,-3,-6,-3,-3,-4,-3,0,0,-2,-1,0,-3,-2,-3,-3,-1,
-5,-1,-1,-4,-2,-3,-5,-2,-3,-4,-3,-3,-1,-2,-2,-3,-2,0,-1,-1,
0,1,0,0,0,-1,0,0,2,2,0,3,0,2,3,3,3,0,2,1,
3,1,4,2,3,4,2,2,2,2,3,2,2,3,5,2,3,4,2,3,
3,4,2,1,0,0,2,2,0,1,-1,1,0,0,1,1,3,2,3,3,
4,2,5,2,3,3,3,2,2,1,-1,2,0,2,2,1,2,3,3,4,
4,3,3,4,3,4,2,3,2,3,0,0,-1,2,1,-1,1,-1,2,0,
3,2,3,4,5,4,3,1,5,3,1,1,1,2,2,2,2,1,1,3,
3,2,1,5,4,0,3,4,2,1,4,3,4,3,4,0,2,1,-1,1,
3,2,1,3,3,2,3,1,4,1,5,3,2,1,2,2,1,3,0,2,
1,2,2,-1,1,1,1,1,4,2,3,0,0,3,1,2,2,1,3,2,
1,1,0,1,-1,-2,1,-2,1,0,-1,-3,1,0,-1,-1,0,0,-1,-1,
1,0,-1,0,1,-1,1,-1,-2,-1,-1,-1,0,0,0,1,-1,1,3,0,
3,0,-1,1,1,2,-2,1,2,1,3,1,1,1,2,0,2,2,0,0,
2,1,2,-1,1,3,2,2,2,1,3,1,5,2,2,1,3,3,2,1,
2,3,3,1,3,1,2,2,1,3,2,3,4,3,4,5,3,3,7,4,
4,3,4,4,5,7,4,4,7,4,5,2,4,2,4,4,4,3,4,5,
6,4,4,3,4,4,4,4,6,3,2,4,3,4,6,4,5,4,4,5,
4,5,6,7,6,5,8,6,4,7,5,4,9,7,7,8,8,5,7,8,
9,6,7,8,10,8,9,7,7,6,10,9,9,9,8,8,10,8,9,11,
10,8,9,8,9,8,6,8,9,10,9,10,9,9,8,8,10,8,9,8,
11,9,8,10,6,7,9,8,7,7,6,7,7,7,7,8,6,7,7,7,
5,8,5,6,7,6,7,3,5,4,5,6,3,4,4,6,5,5,3,6,
6,3,3,4,2,6,8,5,5,3,3,3,2,5,2,3,0,2,3,3,
2,-1,1,2,2,-2,1,1,0,3,2,1,1,-2,1,2,2,-1,2,1,
1,0,2,0,-2,-1,-1,-1,-1,0,-3,-2,-2,2,-1,-2,-3,-1,-3,-3,
-1,-2,-1,0,-2,-1,-1,-2,0,-1,0,-1,-1,0,0,2,-1,0,-1,0,
-1,0,-2,-1,-1,0,0,1,-1,1,-1,1,-2,-3,1,-2,1,-2,0,-2,
-1,-2,-2,2,2,2,1,2,0,2,1,0,1,0,0,1,2,3,-1,1,
1,1,2,2,1,0,1,-1,0,4,3,1,2,0,2,0,1,1,1,0,
1,3,0,-2,1,-3,-1,0,-1,-2,-2,0,0,-1,1,0,1,-1,1,0,
-1,0,1,0,0,2,2,0,2,1,1,2,0,0,3,1,0,2,0,1,
-1,4,3,2,0,3,1,2,2,5,1,3,2,4,3,4,5,3,5,2,
6,3,0,2,4,2,5,2,4,1,2,3,3,3,4,5,1,5,2,3,
6,4,3,3,5,3,4,6,3,6,7,4,6,7,5,4,5,5,3,2,
5,4,4,5,4,6,2,3,3,3,2,3,5,5,3,4,5,4,1,4,
2,4,1,3,0,3,2,1,-1,2,0,-3,1,-1,2,2,0,1,0,3,
0,1,1,0,-1,2,1,0,-1,0,-2,0,-1,-1,-1,0,-2,2,-1,1,
2,2,2,0,2,1,2,2,3,-1,0,0,1,0,0,-1,-1,-2,0,1,
1,3,1,3,2,2,1,1,2,4,3,-1,2,4,2,3,4,2,1,-2,
0,0,-1,2,2,3,2,4,4,3,3,0,1,1,1,1,1,3,0,1,
4,0,0,2,2,2,0,4,2,5,3,4,5,4,2,4,0,5,3,2,
4,1,2,2,2,2,0,1,3,3,1,2,3,2,2,4,3,4,2,3,
3,4,4,3,2,1,4,3,2,4,3,4,5,2,3,3,1,2,-1,1,
-1,-1,2,3,3,5,4,6,4,5,4,6,4,5,4,1,2,-2,-2,-4,
-1,-2,-5,-1,0,-3,1,0,-3,1,-1,-1,-1,-1,0,1,-2,-3,-3,-2,
-2,-2,-3,-4,-5,-5,-5,-6,-6,-4,-4,-7,-5,-7,-4,-4,-4,-2,-4,-2,
-3,-3,-6,-6,-6,-5,-6,-4,-5,-5,-4,-6,-8,-7,-5,-6,-5,-2,-3,-2,
-1,-3,-4,-1,-3,-1,-3,-3,-4,-1,-1,-1,-1,0,-1,-1,-2,-4,-2,-2,
-2,-1,-1,-2,-3,0,1,1,0,-2,-2,-1,-2,0,-2,-2,-3,-2,-1,-2,
-1,-1,-1,1,1,1,1,1,3,1,1,0,0,1,0,2,-1,1,2,1,
0,4,0,1,2,1,1,1,1,0,0,0,1,0,0,2,-1,1,-2,1,
0,0,-2,-1,0,-2,-2,2,2,-1,0,-1,1,-1,1,1,1,0,-1,-1,
0,0,0,2,-3,-2,-1,-1,-3,1,-1,1,-2,0,-1,2,0,0,-2,-2,
3,2,1,3,1,3,5,4,1,4,2,2,4,2,1,4,4,6,5,3,
2,3,1,2,2,3,1,2,1,1,1,0,1,2,-2,1,1,-1,1,2,
2,0,3,3,2,1,2,2,3,-1,0,3,-1,2,2,-1,-1,0,0,0,
0,-2,-1,-1,0,-3,1,-1,0,1,2,1,2,2,1,0,0,0,1,2,
1,-1,0,1,3,2,0,-1,2,-2,0,0,0,1,1,1,0,2,1,2,
3,2,2,3,3,4,4,5,4,4,1,4,3,2,3,3,1,4,4,1,
2,2,3,1,1,3,0,4,3,3,1,-2,2,1,2,0,-1,0,-1,-1,
-1,1,-2,1,-2,-2,-1,-3,-3,-1,-3,-2,-3,0,-1,0,1,0,-3,0,
-3,0,-1,-2,-2,0,-1,0,0,-3,-2,-1,-2,-1,-1,-2,-1,0,-4,-1,
1,-2,-3,-1,-1,-1,-1,-1,-4,-2,-1,-1,1,-2,0,-3,-2,-1,-1,-1,
1,0,1,-3,1,1,-1,-3,-1,0,-2,-2,-2,-1,-1,-1,-1,-2,-3,0,
-1,0,-2,0,-1,0,1,-2,-1,-1,0,-3,0,-2,-2,-3,-1,-1,-2,-2,
-4,-2,-1,-1,-2,2,-2,0,1,2,1,0,1,-2,1,1,0,1,1,0,
2,0,2,0,0,-1,0,0,0,0,1,1,0,4,1,2,3,0,1,2,
2,0,1,1,3,3,2,4,6,3,2,5,4,3,4,3,4,5,4,3,
4,2,3,1,1,2,0,4,3,4,2,3,3,5,2,4,5,3,3,4,
6,5,6,5,5,5,4,0,4,3,4,3,3,2,3,3,2,4,3,3,
5,5,2,4,5,6,6,4,3,3,4,5,4,5,5,4,6,5,5,6,
7,7,7,7,7,7,8,7,8,9,10,7,7,8,6,6,8,7,8,6,
6,8,8,8,9,7,6,8,8,6,7,8,5,7,7,8,6,8,7,9,
5,6,7,6,7,6,8,4,7,9,6,5,6,6,7,5,6,5,4,4,
4,3,5,2,6,3,4,5,7,7,5,5,6,7,5,6,4,5,6,5,
5,5,6,5,6,6,8,6,7,4,6,6,7,6,5,7,8,5,6,6,
7,6,7,7,5,8,6,8,7,8,5,7,5,6,8,5,5,6,8,5,
6,4,5,6,4,3,6,5,6,7,8,6,7,7,5,7,6,5,7,6,
4,7,8,8,7,5,4,5,4,2,4,4,2,5,5,2,3,2,3,3,
2,0,2,0,-2,-1,1,0,0,-1,-1,-3,0,-1,0,-3,-2,-2,-3,-5,
-4,-3,-4,-3,-2,-3,-1,-2,-1,-2,-1,-2,-2,-3,-4,-4,-2,-3,0,-2,
-1,-2,-2,-1,-2,-2,-1,-3,-5,-1,-1,0,-1,2,2,-3,-1,-1,-1,-5,
-3,-5,-5,-3,-5,-1,-5,-1,-3,-4,-3,-4,-2,-2,-2,-2,-2,-1,0,-3,
-3,-3,-4,-5,-5,-6,-7,-5,-5,-4,-2,-5,-2,-4,-3,-5,-6,-3,-4,-5,
-3,-2,-6,-2,-4,-3,-3,-1,-3,-2,-5,-3,-2,-3,-4,-3,-3,-1,-3,1,
-1,-1,0,0,-1,0,0,0,1,-2,-1,0,-4,-2,-1,-3,-2,-2,-1,-4,
0,-3,1,-2,0,-1,1,2,3,2,4,2,1,1,4,2,3,1,1,1,
4,3,2,-1,1,0,-1,1,-1,0,0,0,-1,1,0,3,2,1,1,0,
-1,2,0,3,3,3,2,2,1,2,4,2,2,2,3,2,3,3,6,3,
3,5,2,3,1,3,3,3,4,5,5,4,7,5,7,5,5,5,6,7,
7,6,8,8,8,7,9,9,8,10,9,7,7,10,10,10,10,10,9,12,
8,9,10,8,10,7,10,8,7,9,9,9,5,8,8,8,7,8,10,8,
11,8,6,5,7,7,7,5,7,6,7,8,5,6,5,5,8,5,5,6,
4,3,3,3,1,2,3,5,3,2,6,2,2,5,3,3,3,3,4,-2,
2,4,2,2,2,3,4,2,3,1,4,3,3,2,3,1,4,1,2,3,
3,1,2,2,0,3,3,4,4,3,5,0,1,2,1,1,0,0,3,0,
3,1,1,1,2,3,0,2,2,2,2,1,2,2,2,0,1,0,-1,0,
0,1,1,-2,1,0,2,0,1,0,-3,0,1,1,2,3,-1,2,-1,0,
0,1,0,0,1,-2,2,1,0,1,-1,2,-2,1,1,1,-2,2,3,-1,
-3,1,-1,-2,0,-1,1,-1,1,1,4,1,-1,1,3,3,-1,0,2,2,
-1,0,-1,-1,0,0,-3,0,-2,-2,-6,-3,-4,-4,-2,-3,-1,-1,-2,-2,
-1,0,1,-1,1,0,0,2,-1,0,1,0,0,2,0,0,-1,-2,-2,1,
-1,-2,1,2,0,1,0,1,-2,-2,0,1,0,2,-1,-1,0,-2,-1,-3,
-1,-2,-2,1,0,-2,0,0,0,0,0,-2,-1,0,1,-1,2,1,-1,-1,
0,-1,-2,-1,-1,-1,0,0,-2,-2,-2,-1,-1,-4,-3,-3,-1,-1,-2,-3,
-6,-6,-3,-4,-3,-4,-5,-2,-4,-1,-4,-2,-4,-4,-3,-5,-4,-6,-3,-7,
-5,-5,-2,-4,-4,-2,-3,-2,-3,-4,-6,-4,-5,-6,-3,-5,-4,-4,-5,-5,
-1,-3,-3,-2,-2,-4,-4,-3,-3,-3,-2,-4,-4,-2,-4,-4,-3,-4,-3,-5,
-3,-4,-4,-6,-5,-5,-7,-3,-3,-1,-2,0,-2,0,-3,-3,-2,-4,-4,-3,
-4,-4,-5,-4,-4,-3,-2,-5,-5,-6,-4,-4,-2,-3,-1,-2,-4,0,-4,-2,
-4,-4,-4,-2,-4,-3,-1,-4,-3,-3,-4,-4,-5,-4,-3,-4,-1,-6,-4,-5,
-4,-3,-3,-5,-4,-2,-4,-2,-4,-4,-4,-4,-2,-2,-4,-4,-4,-1,-2,-3,
-6,-4,-4,-3,-2,-4,-4,-4,-4,-4,-3,-4,-4,-3,0,-3,-2,-2,0,1,
1,-2,-2,0,-2,-4,-2,-1,-2,-3,-1,-4,-2,-3,-2,-2,-3,-2,-2,-3,
-1,-1,-1,0,0,-2,-1,-1,0,-1,-1,-5,-2,0,-1,-3,-4,-5,-4,-6,
-4,-3,-3,-3,-2,-2,-1,-2,-2,-3,-1,-3,1,-2,-1,-1,-3,-1,-3,-2,
-4,-5,-4,-7,-5,-7,-7,-6,-5,-4,-5,-6,-7,-6,-6,-5,-4,-5,-2,-4,
-5,-4,-5,-5,-5,-6,-4,-5,-8,-4,-7,-8,-7,-6,-7,-10,-8,-10,-8,-8,
-8,-7,-8,-9,-7,-8,-8,-9,-9,-9,-10,-8,-6,-9,-8,-8,-8,-11,-9,-10,
-10,-11,-10,-9,-12,-10,-10,-10,-8,-9,-9,-10,-10,-9,-10,-9,-7,-8,-7,-8,
-5,-9,-9,-6,-8,-8,-8,-8,-9,-11,-9,-8,-8,-7,-7,-7,-7,-7,-7,-10,
-9,-8,-9,-9,-10,-9,-8,-10,-9,-9,-8,-8,-7,-10,-9,-6,-8,-4,-7,-8,
-6,-8,-9,-8,-6,-7,-5,-6,-8,-8,-6,-9,-6,-9,-6,-7,-6,-6,-7,-5,
-4,-4,-3,-4,-4,-5,-2,-1,-1,-4,0,0,-5,-3,-3,-2,-3,-2,-6,-2,
0,0,0,2,2,2,1,2,3,5,3,3,5,4,6,5,4,5,5,6,
6,5,5,8,5,4,5,6,7,6,6,8,7,6,7,6,8,7,7,6,
5,6,8,6,3,3,4,4,3,3,2,6,4,3,2,2,2,5,4,4,
4,6,5,3,4,4,2,4,4,3,3,1,1,2,3,4,2,1,1,-1,
-1,2,1,2,1,1,2,0,3,1,4,0,4,4,2,3,2,2,0,0,
-1,-3,-1,-2,-4,-4,-2,0,-2,0,2,1,0,1,-1,0,3,2,0,1,
1,-1,2,3,0,0,-1,-1,0,1,3,3,2,4,4,5,4,4,4,4,
5,6,3,3,3,1,6,3,2,1,4,0,1,1,2,3,3,4,1,2,
3,4,3,3,3,3,1,5,3,2,2,3,1,8,5,3,4,7,5,3,
4,4,6,5,4,3,3,2,4,6,4,4,3,3,3,2,4,7,5,5,
6,4,4,7,6,7,7,6,6,6,5,6,5,5,1,4,2,4,4,2,
3,5,3,6,3,6,4,3,5,5,3,0,3,3,0,0,1,2,0,-1,
1,-1,0,-1,1,0,0,-2,0,0,-1,-2,0,-4,1,-1,-1,1,-1,-1,
-2,1,0,2,0,0,0,1,2,-1,-2,0,1,1,3,2,0,-1,0,1,
0,3,2,0,1,1,0,0,1,-2,1,0,-1,-1,-1,-4,0,-1,0,-2,
2,-1,-1,-2,0,-1,0,2,0,2,0,1,-1,-1,1,-1,0,-1,0,0,
-1,-3,0,-2,-2,-2,-2,1,0,-1,-2,-2,-1,1,1,1,-1,0,0,0,
0,1,1,0,3,0,2,0,2,1,2,1,1,-1,1,-2,0,-3,-2,-2,
-2,-1,-4,-1,-4,-1,-2,-3,-4,-3,-2,-3,-1,-3,-1,-4,-1,-1,-2,-3,
-3,-3,-3,-5,-5,-3,-3,-4,-3,-4,-4,-6,-6,-2,-7,-6,-5,-4,-5,-8,
-8,-5,-5,-6,-6,-6,-7,-4,-7,-5,-7,-5,-6,-3,-6,-7,-4,-5,-3,-4,
-4,-5,-4,-4,-3,-3,-3,-6,-3,-2,-3,-4,-2,-6,-3,-5,-7,-5,-4,-5,
-5,-3,-5,-5,-4,-5,-5,-4,-3,-6,-3,-4,-6,-5,-4,-7,-5,-7,-2,-3,
-5,-5,-4,-7,-4,-5,-6,-4,-4,-6,-4,-6,-7,-4,-6,-5,-3,-4,-5,-2,
-2,-4,-2,-2,-4,0,-3,-2,0,-3,-2,-1,-2,-2,-1,-3,-4,-2,1,-1,
2,0,-3,-1,0,-1,2,0,2,2,2,3,1,4,4,0,1,0,1,2,
2,4,2,3,3,2,1,2,3,5,3,2,3,1,3,3,3,2,3,4,
3,2,1,2,2,3,2,2,1,3,4,5,3,3,1,3,5,3,7,5,
6,6,7,8,5,7,6,8,7,6,8,5,8,7,8,7,10,10,10,11,
13,11,12,11,10,12,12,11,12,12,9,10,11,11,11,12,12,12,13,13,
10,12,12,11,9,9,12,10,10,8,12,10,11,10,10,10,10,10,10,9,
8,7,7,7,6,6,5,5,4,5,3,4,4,3,5,3,4,5,4,2,
2,3,0,3,1,2,1,-1,0,-1,1,-2,0,-2,-1,-2,-4,-2,-1,0,
-3,-2,-1,-2,-1,-2,-1,-3,0,-1,-2,-1,-2,1,2,-1,-2,0,0,0,
0,0,-1,2,-2,-1,-2,-2,-2,-2,-3,-1,-1,0,-1,0,0,-2,-1,-2,
-1,0,-2,-1,-1,1,-1,2,0,-1,1,0,-2,1,-2,-1,1,-1,-1,1,
-1,2,0,3,1,1,3,2,5,2,3,4,4,6,5,4,4,4,5,6,
5,5,1,4,3,2,5,3,6,3,5,6,8,7,6,7,6,4,6,6,
8,8,7,8,7,6,8,6,7,6,5,9,7,7,7,8,10,6,4,6,
4,7,5,4,2,5,6,3,5,5,5,4,7,4,7,4,4,4,6,3,
4,5,5,3,2,3,5,2,3,3,5,5,5,4,4,6,3,3,4,3,
3,2,3,3,3,2,1,2,3,1,-2,-3,-1,3,1,0,0,1,0,0,
3,3,2,4,4,4,6,3,1,3,3,3,4,4,2,3,2,0,4,2,
2,4,3,2,4,5,5,2,3,6,1,7,6,6,5,5,5,4,7,3,
6,8,6,5,5,5,4,7,5,6,6,4,4,5,4,6,9,7,8,7,
8,9,9,6,6,5,5,5,2,5,5,6,5,3,3,4,6,6,6,5,
6,7,4,4,7,4,4,7,6,9,6,7,5,6,8,6,7,8,6,6,
8,7,6,7,8,8,8,6,6,7,6,7,7,4,8,8,6,4,5,5,
5,4,5,6,5,6,6,3,6,4,4,5,5,4,6,3,5,3,3,5,
7,5,5,3,4,4,5,4,4,5,5,4,6,7,6,6,6,3,6,4,
2,4,4,5,5,5,8,7,8,7,4,6,5,7,6,7,6,5,3,5,
5,6,5,6,5,7,4,5,6,7,6,7,6,6,7,6,8,7,7,6,
6,7,7,6,8,8,5,7,6,9,6,9,9,8,9,7,6,9,7,8,
9,8,7,8,7,7,10,8,8,7,9,8,9,8,10,7,8,9,4,7,
5,5,7,6,7,6,5,7,7,8,7,6,7,7,9,7,6,9,6,7,
10,7,7,7,6,5,6,5,4,7,6,5,7,7,6,6,6,7,4,7,
4,6,5,4,7,6,6,7,7,8,7,7,7,5,5,5,5,8,5,4,
5,5,6,5,2,6,4,4,4,3,4,3,1,4,3,3,1,2,2,3,
4,1,3,3,3,3,2,3,2,0,1,1,1,1,3,1,1,2,1,0,
-1,2,0,1,2,2,0,3,1,2,2,4,5,2,1,1,4,1,4,2,
1,2,0,5,4,0,1,5,5,7,4,6,5,4,4,5,9,7,7,9,
8,5,8,8,8,6,7,7,7,7,10,6,9,7,8,9,7,9,9,9,
6,9,9,9,7,10,9,6,8,9,6,7,8,8,8,6,10,8,8,7,
9,8,9,6,6,7,7,7,8,6,6,7,6,5,7,6,6,7,7,7,
6,8,5,6,4,4,7,7,4,3,5,6,2,4,1,3,6,2,3,3,
4,1,4,4,3,5,4,2,5,2,4,2,1,5,2,1,4,2,2,4,
5,1,2,4,3,4,2,1,3,3,0,0,1,2,3,0,5,3,3,3,
0,3,4,5,5,4,3,2,3,2,2,1,4,3,4,5,3,4,4,3,
5,3,6,7,5,4,4,7,3,2,5,2,3,1,3,1,0,2,2,-1,
2,0,2,3,1,3,1,1,3,2,3,2,4,1,3,3,1,2,2,1,
1,1,3,2,1,2,3,2,2,4,2,5,4,3,3,4,2,5,2,4,
6,1,5,4,4,5,2,4,4,3,7,5,4,5,7,4,4,5,6,4,
3,3,3,5,2,4,4,3,2,2,3,4,4,3,4,4,5,6,4,3,
5,3,6,5,4,4,3,4,3,3,0,4,0,2,4,5,4,4,4,3,
3,3,3,3,5,3,2,2,1,2,2,1,1,1,4,0,-1,-1,-1,-1,
1,1,-1,-1,0,-1,-3,-3,-3,-3,-1,-3,-3,-2,-1,-5,-3,-2,-2,-3,
-2,-3,-5,-3,-2,-4,0,-3,-4,-2,-4,-1,-2,-2,-2,-1,-1,-1,0,0,
0,0,1,1,1,1,1,1,1,-1,0,1,0,-2,-2,-3,-1,1,-2,2,
-1,-1,1,-1,-1,1,2,3,1,2,3,0,2,1,3,1,3,2,2,3,
1,1,2,1,3,1,3,2,3,5,3,5,4,5,7,8,7,6,6,5,
6,7,7,6,5,7,7,9,5,8,8,8,8,9,6,8,7,7,7,5,
4,7,4,6,6,6,4,3,7,3,4,7,6,5,3,5,2,3,4,4,
3,3,3,3,2,1,1,2,-3,0,0,1,2,2,3,2,3,2,4,1,
2,5,2,1,4,4,1,3,4,2,3,4,4,3,4,3,3,3,1,3,
4,0,1,2,2,3,3,2,-1,2,3,4,2,2,5,2,3,1,1,3,
2,1,2,0,2,2,-2,0,0,0,-1,1,3,-2,1,0,1,3,2,5,
7,9,7,9,11,15,17,20,19,21,22,23,29,25,28,28,29,27,25,27,
26,28,25,26,26,24,26,24,24,22,21,21,18,16,17,13,11,11,10,9,
7,6,1,2,-2,-4,-4,-3,-3,-3,-3,-5,-6,-5,-5,-5,-4,-3,-3,-3,
0,-2,-3,-2,-2,-3,-1,-3,-2,-2,-1,-1,-2,0,-3,-3,-2,0,0,-4,
-1,1,0,-2,-1,2,-1,0,-1,1,1,0,1,3,1,2,1,3,0,2,
-1,1,3,-1,1,1,-1,1,3,2,-1,3,3,2,0,3,4,2,1,1,
0,0,1,1,-1,3,1,1,3,4,2,2,2,4,2,3,2,1,1,-2,
2,-3,0,-1,3,3,1,2,1,-1,4,2,0,-1,-3,-2,-2,-1,-1,2,
2,2,1,3,3,1,-1,1,2,0,2,2,0,-1,-1,-2,-7,-3,-4,-2,
-3,-3,-2,-2,1,0,0,0,1,2,0,0,0,0,-2,2,0,-1,-1,1,
1,0,2,-1,4,2,3,3,1,1,3,4,1,1,1,0,2,-1,0,1,
4,2,3,2,2,4,3,2,1,1,2,0,1,0,1,0,1,2,0,1,
5,0,2,4,3,2,2,1,3,1,2,2,4,0,0,-2,0,0,-1,1,
2,3,2,1,3,1,1,1,2,0,2,1,1,3,2,0,2,0,0,2,
0,2,1,2,1,2,2,-1,2,-1,-2,-1,2,1,2,0,2,1,-1,-1,
1,1,1,0,-3,-1,0,-2,-1,-3,-2,-3,-3,-3,-1,-2,-3,-1,-3,-2,
-2,-2,-3,-4,-1,-4,-4,-2,-3,-3,-3,-3,-4,-1,-3,-1,0,-2,-2,-2,
0,-4,-2,-3,1,-1,-1,1,-1,-1,1,-2,1,-2,-1,0,1,1,1,1,
2,1,3,0,-2,-2,1,0,2,2,2,2,4,1,4,2,4,3,1,2,
6,2,4,5,3,0,1,0,1,-1,-2,-1,-2,0,2,-2,1,1,0,1,
0,0,2,4,2,4,3,5,5,3,3,4,3,4,2,4,4,2,2,1,
1,0,1,2,0,1,3,2,5,2,1,5,4,3,4,3,3,4,1,3,
1,2,3,5,3,3,2,5,3,4,5,3,4,4,4,5,3,6,2,6,
3,3,4,3,3,5,5,2,3,0,0,4,2,3,3,1,0,4,1,0,
3,2,1,3,0,-2,1,0,0,1,-1,0,2,0,-2,3,-1,1,-1,0,
0,1,1,0,0,-2,-1,2,-1,0,-2,-1,-2,-2,-2,-3,-2,-2,-4,-4,
-3,-2,-3,-5,-2,-3,-2,-1,-2,-4,-4,-4,-3,-6,-4,-3,-2,-2,-2,-1,
-3,-4,-2,0,-3,-2,1,0,1,0,1,1,-1,-1,0,0,0,1,0,0,
1,1,-1,0,1,0,1,5,1,0,3,2,0,1,1,1,2,1,0,1,
2,2,2,4,4,3,2,5,5,1,3,5,1,3,1,2,1,2,1,0,
1,1,4,1,1,1,1,2,1,3,2,2,3,2,5,3,5,1,3,4,
3,3,1,4,3,3,4,5,3,5,5,3,4,4,4,4,5,3,2,5,
4,2,5,3,3,3,0,1,2,2,1,1,2,3,3,2,0,3,0,3,
1,2,1,1,0,1,0,-1,1,2,-1,-1,-2,0,0,0,0,-1,-2,-3,
-1,-1,-2,-3,-1,-4,-3,-2,-4,-2,-3,-2,-3,0,-1,-4,-4,-3,-3,-2,
-5,-3,0,0,-2,-4,-2,0,0,0,-1,-1,-4,-3,0,-3,-4,-3,-2,-3,
-3,-3,-4,-3,-6,-4,-4,-6,-6,-6,-6,-7,-4,-6,-5,-4,-4,-4,-4,-4,
-6,-3,-4,-7,-3,-5,-3,-3,-4,-3,-3,-3,-3,-5,-3,-3,-7,-3,-4,-3,
-5,-2,-3,-6,-6,-4,-4,-4,-2,-3,-3,-2,-4,-1,-2,-3,-4,-2,-2,-3,
-5,-5,-2,-1,-3,-3,-1,-1,-2,-2,-2,-2,0,-2,-2,-2,-1,0,-3,-2,
0,0,0,-1,-3,-3,-1,-1,-2,-2,0,-1,0,1,-1,-1,3,2,0,3,
0,0,0,-1,2,1,2,1,4,2,1,3,3,5,3,1,3,4,3,2,
5,3,6,4,2,2,3,2,2,2,2,-1,1,2,-1,0,0,-1,1,-2,
-2,1,0,1,0,2,0,1,2,1,1,0,3,0,1,2,2,-1,-2,1,
1,-2,1,0,-1,-2,0,-3,-3,1,0,0,2,1,3,1,2,0,0,1,
0,1,0,0,0,3,4,1,1,3,-2,0,-1,2,1,1,1,-2,1,0,
-1,1,-1,-1,-1,-2,0,-1,2,0,4,0,0,3,0,-1,0,2,-1,-2,
-2,-1,-2,-2,-4,-4,-3,-3,-3,-3,-3,-5,-4,-4,-1,-1,-2,-1,-3,-3,
-3,-5,-5,-3,-4,-6,-5,-5,-7,-5,-6,-6,-5,-8,-7,-8,-9,-5,-6,-6,
-5,-5,-5,-5,-5,-6,-5,-5,-5,-4,-3,-4,-4,-4,-4,-4,-6,-5,-3,-4,
-1,-6,-3,-4,-3,-4,-3,-2,-4,-1,-5,-4,-4,-5,-2,-3,-2,-3,-2,-3,
-3,-3,-1,0,-2,-2,-1,0,-1,-1,0,-1,-2,-2,0,2,-1,-2,0,1,
0,2,0,4,3,3,3,5,2,2,4,4,2,6,5,6,4,5,4,6,
6,5,6,5,5,6,7,7,6,7,6,6,6,5,4,5,4,5,4,3,
5,1,5,4,4,4,2,4,2,4,2,2,3,5,1,4,1,4,3,2,
3,5,5,2,5,3,6,2,6,3,2,3,3,2,0,2,1,1,0,3,
1,4,2,3,4,0,2,-1,-1,0,2,0,-2,1,-1,0,-1,-1,-4,-2,
-3,-5,-3,-2,-3,-4,-5,-6,-4,-4,-6,-4,-5,-6,-6,-5,-5,-6,-4,-2,
-4,-3,-7,-3,-4,-7,-6,-7,-3,-4,-2,-5,-7,-5,-4,-5,-4,-4,-4,-6,
-4,-2,-4,0,-4,-2,-2,-7,-6,-5,-5,-3,-4,-4,-2,-2,-5,-5,-5,-4,
-6,-7,-8,-6,-8,-5,-4,-7,-8,-5,-7,-8,-7,-7,-8,-6,-7,-7,-5,-3,
-8,-6,-6,-9,-7,-7,-7,-4,-4,-5,-4,-2,-4,-2,-3,-4,-5,-4,-5,-4,
-5,-8,-4,-5,-6,-7,-4,-5,-5,-4,-5,-3,-3,-4,-1,-3,-1,-1,-3,-3,
0,-3,-6,-2,-2,-3,-3,0,0,-2,2,1,2,2,2,2,1,1,-1,1,
0,1,0,0,1,2,-1,-2,-5,-1,-3,-3,-1,-2,-1,-2,0,0,-2,-2,
-1,-4,0,-4,-3,0,-4,-3,-1,-1,-3,-2,0,-3,-2,0,-1,-2,-3,-1,
-4,-3,0,-3,-2,-3,-3,-4,-3,-2,-3,0,-5,-2,-1,-1,0,0,2,-2,
-3,-1,0,-1,0,1,0,-1,-1,0,-3,-1,-2,-3,-2,1,0,-1,-1,1,
-2,0,2,0,-2,-1,1,-1,1,3,0,1,2,3,-1,-1,-1,1,0,1,
1,0,4,4,3,4,1,3,0,3,1,2,1,1,1,3,1,4,2,3,
3,3,4,5,2,6,6,4,6,6,4,4,5,7,6,4,6,4,6,5,
4,7,7,8,6,8,5,4,4,6,3,5,6,5,5,7,4,3,1,3,
2,3,3,4,2,4,6,5,5,5,3,4,4,4,2,2,1,2,1,4,
1,3,2,1,2,1,1,3,2,1,2,0,2,3,3,1,3,1,1,3,
2,0,3,5,1,1,1,0,-1,0,-1,0,-2,-2,0,-1,-2,-2,-3,-2,
-2,-1,0,-2,-1,-3,0,-3,-1,1,-1,0,0,-2,1,2,-1,-2,-5,-1,
0,0,1,2,2,0,1,-1,-2,0,3,2,6,5,6,1,0,1,-2,0,
-1,0,1,2,0,3,1,2,3,3,2,1,3,2,2,-2,-1,2,1,2,
0,0,1,0,0,-1,-2,0,-1,0,0,1,1,-1,-1,1,-4,-1,1,2,
2,2,3,1,2,1,0,0,1,1,1,0,0,1,1,-1,1,1,1,1,
3,2,2,1,1,1,1,0,3,-2,0,1,1,0,3,1,-1,0,2,2,
3,2,2,0,1,-1,0,-2,2,1,1,0,-1,-2,2,1,2,3,-1,1,
1,3,2,3,3,4,3,2,2,0,4,1,2,5,6,3,4,2,4,4,
1,3,1,3,3,3,5,4,4,-1,0,3,2,0,0,2,-1,1,0,2,
1,4,2,1,0,-1,1,-1,1,0,2,4,3,0,5,4,3,5,5,5,
6,6,7,4,8,9,11,8,9,10,9,6,10,11,11,10,11,11,10,8,
13,12,12,17,13,16,16,17,22,23,22,24,26,28,29,33,32,34,41,42,
45,49,50,57,63,67,73,77,83,88,90,97,104,115,134,143,154,164,197,219,
237,-59,-1009,-3528,-6311,-7096,-7040,-6611,-1203,4188,4504,4555,4603,4545,4611,3497,-390,-3455,-3472,-814,
2616,4215,3712,-1957,-6943,-5668,-1510,-246,-4033,-6709,-6853,-2439,3533,4537,4606,3489,-2829,-7098,-6909,-6773,
-1500,4114,4546,3895,-1740,-6822,-6934,-7034,-6833,-2988,2840,4573,4513,4688,3785,-1748,-6845,-6839,-7208,-4339,
2482,4737,4465,4630,4669,3133,-3415,-7133,-6865,-7095,-7011,-7190,-7120,-7012,-6976,-6968,-6939,-6971,-6903,-7029,
-5804,332,4660,4538,4759,4650,4621,2559,870,3210,4669,4596,4664,4264,-431,-6174,-6942,-6978,-6132,-503,
4235,4664,3520,-2198,-6702,-6835,-6969,-6944,-6471,-1371,4052,4643,4664,4713,4687,4739,3950,-696,-5895,-5889,
-949,3810,4631,4659,3342,-2159,-6468,-6912,-6766,-4890,-3461,-3962,-1976,2619,4711,4071,-1150,-6455,-6931,-5869,
-225,3103,-1853,-6154,-7047,-4419,2230,4899,4552,4825,4660,4786,4642,4767,2895,-3165,-6865,-6816,-6976,-6922,
-6524,-2034,3232,4054,3763,3006,-375,-4955,-6838,-6865,-6965,-5349,-419,3944,4738,4713,4770,4744,4746,4721,
4713,4706,4686,4392,1342,-4225,-6821,-6871,-6917,-6975,-6455,-4439,-3573,-5187,-6648,-6939,-6800,-6861,-3642,2554,
4801,4662,4817,4722,4800,4720,4793,3600,-142,-3089,-3313,-1562,1061,1733,-543,-4324,-6598,-6848,-6837,-6902,
-6520,-3617,19,970,-7,-664,881,3525,4734,4718,4272,3672,4102,4726,4738,4781,4075,479,-2264,-309,
2692,3632,1625,-2957,-6388,-6837,-6869,-6844,-6865,-4638,-602,191,-387,305,2078,4093,4784,4768,4776,4766,
4580,3882,3630,3909,3248,843,-3071,-6168,-6400,-4443,-1410,1557,2537,983,-2622,-5969,-6668,-2852,2930,4757,
4680,2995,-1886,-5513,-5417,-2414,1128,3037,3678,4137,4493,4720,4755,4758,3381,-427,-2310,238,3645,3629,
-440,-4896,-6652,-6901,-6823,-6769,-2498,3647,4789,4680,4813,4727,4826,3588,-2040,-6577,-6478,-1885,3904,3570,
-1301,-5085,-5474,-5913,-6317,-5009,-2351,379,2944,4626,4715,4863,3101,-3380,-6886,-6818,-4699,1920,4755,4717,
3026,-3158,-6856,-5246,-1296,-363,-3301,-4233,-1518,2370,4091,1634,-2257,-3232,-3194,-4313,-4440,-1534,2764,4697,
4425,3017,738,-1337,-2011,-631,1783,3887,4736,4753,4760,3584,594,-2161,-3404,-2243,206,493,-905,-2093,
-3713,-5872,-6830,-6920,-6213,-2827,1890,4489,4790,4746,4794,4799,4735,3647,2002,1236,806,950,2446,4129,
4730,3471,-762,-5032,-6311,-5744,-4289,-2286,-245,1031,122,-3223,-5784,-5013,-2460,-590,310,1003,1161,623,
570,1420,2161,2574,3602,4612,4771,4761,4770,4771,4559,2480,-1949,-5789,-6873,-6907,-5876,-3361,-1707,-1763,
-3183,-4895,-5381,-3616,-399,2397,4018,4662,4804,4793,4784,4070,2309,874,652,1491,2243,1769,438,-909,
-1577,-1114,370,1925,2349,1520,214,-1229,-2473,-3532,-5000,-6310,-6636,-5527,-2981,-412,1171,1871,2297,3424,
4568,4795,4788,4364,2731,1110,1657,3715,4745,3920,86,-4589,-6384,-6179,-5326,-3859,-2036,-303,1025,661,
-1305,-2675,-1748,668,2323,2724,2547,2136,1538,97,-1814,-2669,-2110,-692,879,2182,3210,3933,4317,3918,
2291,940,1152,1355,1040,1132,1445,2149,2391,955,-903,-2127,-2670,-2599,-2633,-3250,-4523,-5529,-4622,-2682,
-2064,-2550,-1718,1587,4357,4776,4626,2788,-186,-863,680,2322,2384,1273,370,265,1191,2523,2514,71,
-3291,-4898,-3613,-520,2203,2794,1087,-1686,-3369,-1967,1476,3905,3524,392,-2708,-2752,549,3905,4778,4271,
2002,-252,41,1725,3061,2137,-1220,-4755,-6550,-6100,-3777,-477,2826,4528,3595,682,-2176,-3721,-3738,-1581,
1947,4289,4617,3138,-425,-4124,-5922,-4804,-1456,1345,2090,660,-1225,-586,2529,4593,4373,1724,-2427,-4075,
-1973,1925,4343,4738,3508,-381,-4417,-5936,-4977,-3412,-2528,-859,1603,2647,1500,-228,-957,-577,610,2814,
4533,4739,4740,3769,1018,-1835,-3839,-5108,-5446,-4874,-3978,-2688,-671,1251,2124,1453,11,-340,1326,3697,
4717,4740,4755,4188,2518,0,-2523,-3434,-2231,-658,-361,-1089,-1704,-2095,-1979,-704,808,1635,1680,699,
-1034,-2781,-2892,-868,548,22,-458,-537,-824,537,3317,4661,4694,3785,1969,719,273,70,-400,-943,
-1145,-1066,-964,-1100,-1672,-2197,-1617,-118,920,930,578,877,2224,3004,1498,-1220,-3056,-3346,-2542,-1223,
747,2879,3510,2159,118,-953,-249,1876,3912,4698,4733,4759,4009,878,-2691,-4235,-3694,-1817,-993,-2428,
-5084,-6768,-6766,-5092,-982,3066,4508,4431,1872,152,571,-1312,-2808,-2452,-1782,-150,1875,2579,2985,3640,
2939,1143,-41,-182,-1083,-1794,340,3274,4536,4061,942,-3682,-6437,-6459,-4127,-161,2424,1638,354,1801,
4070,4755,4742,4527,2114,-2263,-4619,-3760,-1006,1900,3609,3082,-12,-2845,-3235,-3091,-3971,-4082,-3035,-1071,
1786,3690,3683,2128,1000,1411,1800,1851,2575,3536,3481,1918,-125,-1403,-2567,-4224,-4832,-3768,-2003,-16,
1265,856,-347,-79,2117,3406,1687,-1116,-2069,-521,2394,4374,4686,4026,2396,908,10,-444,90,675,
217,-1202,-3218,-4834,-4762,-2305,1191,3048,2640,1772,796,-1507,-3501,-3359,-2273,-1857,-1850,-486,1839,3190,
3385,3788,4552,4780,3981,410,-3469,-3838,-1949,240,956,-392,-2559,-4466,-5227,-3911,-675,2343,2928,1358,
785,1955,1885,1261,2256,3884,4076,1514,-1259,-1455,256,2573,3176,1295,-1107,-2626,-2109,65,1446,1046,
-266,-1673,-2206,-881,1504,2722,1356,-1645,-4438,-6299,-6210,-3321,790,3239,2700,576,-788,-249,2068,4200,
4729,4717,4416,3229,1542,470,297,-65,-1128,-2832,-4913,-6415,-6349,-4355,-1162,1832,2907,1354,-899,-822,
2210,4489,4710,3917,1745,37,402,2329,3501,2046,-1222,-4073,-4454,-2502,-336,1260,2710,2867,303,-3766,
-6492,-6172,-2910,440,991,-491,-993,137,1261,2136,2963,3720,4441,4742,4530,3306,1607,-89,-1572,-1766,
-1514,-1660,-1925,-1960,-1789,-2000,-2153,-1424,-913,-1200,-1343,-1080,-382,546,1164,1623,2305,2523,1516,267,
351,2171,3919,3459,1076,-1252,-1739,-42,2428,3619,2850,716,-1594,-2603,-2314,-1906,-1868,-2369,-2851,-2371,
-588,2162,4192,4720,4721,4690,3001,-1871,-6134,-6679,-4007,-199,1837,1769,375,-1133,-834,1655,3656,3439,
1747,-747,-3536,-4971,-3618,-364,2241,2112,-701,-3862,-4720,-3082,-232,2796,4420,3245,78,-1793,-329,2628,
4172,3526,1306,-1639,-3275,-2915,-2011,-990,669,3149,4630,4708,4166,2108,-862,-3703,-5052,-4571,-3945,-3592,
-3230,-2045,169,2266,3965,4359,896,-3915,-4231,-394,3400,4656,4451,4064,2942,93,-3472,-4604,-2100,1736,
3178,764,-2369,-2921,-2405,-1293,1244,3179,3104,2038,1147,870,1889,3581,4092,2305,-2002,-5866,-7031,-6068,
-2018,2697,4525,4658,2810,-1420,-3656,-1840,1611,3338,1336,-2114,-3821,-3644,-2006,1487,4262,4267,1262,-3074,
-5090,-3717,-14,3062,3232,1471,-528,-2078,-1790,266,1840,2261,1768,-110,-2719,-3707,-1796,1662,4062,4367,
3101,1514,693,1041,2541,3711,2976,752,-1576,-3703,-5104,-4618,-2834,-1482,-874,-364,90,405,756,1048,
849,342,288,837,2402,4180,4580,2784,-908,-3187,-2062,608,2585,3100,1403,-2167,-4623,-4107,-2171,-736,
-96,-461,-985,-150,1463,2644,2512,756,-964,-1160,-543,228,1176,2322,3730,4449,3215,468,-1551,-1765,
-1528,-1492,-689,653,1441,1686,1157,-565,-3010,-4845,-4996,-3674,-1154,1883,3417,2694,1266,335,52,548,
1711,2923,3582,3699,3212,1620,-440,-1244,-362,700,630,-877,-2969,-3521,-2157,-927,-1333,-3266,-5088,-4469,
-1158,2573,4443,4628,3569,1117,-1510,-2809,-1843,1230,3983,4711,3914,976,-2160,-3163,-2645,-1348,-293,-24,
-269,-369,121,1369,2560,2098,-541,-3783,-5537,-5313,-3477,-559,2052,2713,1640,1229,2522,3790,3662,2400,
980,516,1785,3837,4648,4608,3035,-780,-3722,-4799,-5244,-5323,-4639,-3056,-1130,680,1724,1387,430,164,
549,817,690,319,-275,-664,70,1497,2034,977,-788,-1943,-1865,-647,826,1648,1661,937,-75,-550,
-256,257,318,-463,-1472,-1495,-631,-53,-196,-611,-633,-185,548,1454,2317,2952,2979,2665,2187,800,
-1312,-2904,-2597,-124,2615,3876,3403,1063,-2543,-5001,-4084,-671,2071,2286,301,-2063,-2442,-424,2176,3272,
2255,101,-1982,-2809,-1603,645,2313,2585,1453,-63,-677,34,1332,1908,1389,468,-263,-434,-104,-50,
-654,-1645,-2621,-3158,-3136,-2177,-130,2252,4006,4400,2799,-363,-3113,-3718,-1686,1492,3483,3695,2527,426,
-1387,-1868,-1522,-1144,-535,252,944,1746,2585,2938,2519,1228,-640,-2204,-2848,-2330,-638,1208,1774,727,
-965,-2098,-2453,-2046,-739,772,1548,1570,1547,2031,2633,2859,2701,1916,585,-590,-518,1159,2991,3017,
392,-3718,-6540,-6740,-4635,-1325,1598,2848,1982,90,-1486,-1592,-58,1474,1188,-763,-2393,-2375,-899,1235,
2725,2953,2688,2364,1913,1563,1321,665,-589,-1468,-1568,-1470,-1197,-697,-435,-629,-1029,-1197,-762,-137,
435,1261,2118,2764,3023,2724,2237,1858,1615,1573,1396,389,-1220,-1992,-1629,-1246,-852,-162,187,387,
549,-237,-1671,-2411,-1955,-787,542,1801,2207,1408,433,-146,-153,815,2268,3087,2259,-227,-2419,-2628,
-1097,1129,2796,2649,544,-2041,-3848,-4214,-2694,-162,1610,1537,399,-164,265,1189,2044,2290,1160,-1466,
-3823,-4044,-1726,1575,3512,3418,2240,1284,771,66,-499,-719,-1632,-3353,-4426,-4027,-2659,-1197,-465,-593,
-848,-398,999,2919,4320,4490,3523,2050,919,368,617,1724,2776,2952,2095,193,-2110,-3399,-3031,-1543,
255,1468,1767,1703,1417,585,-700,-1925,-2958,-3709,-3333,-1633,232,1356,1941,2308,2223,1643,776,-282,
-1173,-1312,-418,1026,2360,3258,3311,2297,446,-1465,-2349,-1730,-45,1642,1758,-119,-2726,-4860,-5573,-4379,
-1821,792,2018,1652,685,-37,-210,47,671,1849,3327,4336,4526,3830,2298,802,89,-270,-1157,-2839,
-4312,-4032,-1967,423,1771,1572,4,-2407,-4023,-3135,126,3368,4538,3476,663,-1658,-1803,-598,578,950,
709,611,1440,2618,2807,1536,-514,-2009,-2114,-1380,-697,121,1162,1290,-22,-2009,-3571,-3936,-3429,-2127,
19,1658,1919,1149,-260,-1682,-2191,-1180,1456,3929,4662,4184,2505,586,-410,-331,614,1206,585,-896,
-2009,-1995,-1102,261,1597,2153,1171,-1209,-3347,-3770,-2345,351,2847,3373,1786,-474,-1902,-1383,843,2987,
3738,3076,1581,-95,-1116,-782,389,808,-361,-1966,-2389,-1109,970,2002,1149,-904,-2790,-3248,-2358,-827,
1064,2419,2485,1606,182,-1228,-1731,-1387,-786,66,1175,1993,2071,1656,777,-544,-1625,-2310,-2525,-1591,
188,1548,1764,1126,189,-994,-2212,-2405,-657,2110,3918,3733,1815,-181,-723,25,1025,1369,860,-189,
-1273,-1568,-524,953,1479,656,-1164,-2896,-3050,-1208,1194,2393,1992,736,-367,-580,338,1797,2410,1713,
641,-183,-541,-277,39,-253,-931,-1219,-817,-132,482,874,847,233,-647,-1382,-1928,-2056,-1392,-251,
439,370,-133,-565,-237,1149,2647,2697,1439,612,670,862,912,787,229,-858,-2075,-2816,-2689,-2001,
-1220,-492,191,807,1106,726,-136,-636,-414,321,1277,1737,1104,93,234,1542,2510,2718,2913,3117,
2682,1497,134,-849,-1498,-2125,-2344,-1762,-1003,-801,-822,-630,-373,87,608,570,137,-116,-12,300,
296,-253,-766,-702,-187,452,1049,1436,1347,655,-234,-647,-292,555,1141,1066,537,-92,-351,-267,
-438,-895,-1102,-1255,-1602,-1719,-1562,-1495,-1507,-1128,-60,1379,2201,1809,994,629,593,588,428,216,
267,555,1131,2019,2481,1787,-18,-1865,-2369,-1376,69,829,439,-559,-1118,-718,268,981,925,310,
-440,-950,-708,313,1429,1960,1450,274,-466,-329,404,1343,1933,1802,1205,513,-309,-1012,-1235,-1278,
-1356,-1290,-1042,-1029,-1282,-1123,-372,372,659,650,455,117,-28,130,266,266,479,871,1019,995,
888,546,182,50,56,145,394,587,185,-495,-723,-515,-135,90,-38,-171,-322,-801,-1409,-1712,
-1476,-667,280,774,905,1033,997,711,743,1137,1220,1015,938,913,596,-121,-662,-391,345,778,
418,-686,-1587,-1514,-694,408,1139,608,-865,-1924,-1842,-694,760,1720,1999,1617,850,314,357,773,
1049,705,-327,-1352,-1580,-819,466,1035,-61,-1999,-3008,-2653,-1816,-829,334,1223,1777,1921,1378,736,
632,959,1255,1031,600,454,461,750,1365,1547,738,-685,-1885,-2249,-1338,501,1725,1431,333,-561,
-766,-459,-112,215,-19,-1068,-1701,-1309,-352,651,1236,1272,480,-703,-865,404,2211,3407,3109,1187,
-1191,-2392,-1732,184,1833,1965,576,-1025,-1390,-319,1006,1264,343,-841,-1600,-1886,-1702,-1140,-675,-552,
-515,-363,-224,-290,-552,-765,-406,515,1153,1147,922,1015,1490,1676,1134,6,-1413,-2035,-813,1410,
2814,2469,680,-1245,-2045,-1431,-116,678,257,-747,-1078,-448,400,811,674,310,79,188,586,960,
962,609,276,267,482,460,54,-198,-33,244,307,65,-384,-827,-929,-663,-371,-129,113,216,
116,-45,-141,-174,-117,169,612,962,1019,660,115,-388,-970,-1628,-1993,-1814,-1285,-777,-490,-111,
771,1653,2042,2085,1483,195,-899,-874,456,2129,2863,2412,1243,-176,-1146,-1007,24,949,919,-19,
-1284,-2316,-2473,-1747,-959,-569,-467,-522,-652,-455,382,1232,1673,1699,1205,635,522,1131,2065,2422,
1947,733,-841,-1769,-1339,-163,517,318,-327,-1186,-1968,-1993,-1286,-443,9,-155,-782,-1311,-1117,-148,
1037,1646,1419,838,211,-384,-238,1077,2783,3580,2636,572,-985,-1099,-138,897,1286,728,-596,-1823,
-2272,-1981,-1035,183,710,82,-1080,-1605,-833,704,1943,2349,1895,788,-398,-798,-74,1029,1488,816,
-528,-1381,-1283,-646,207,878,794,52,-507,-527,-277,-46,12,59,135,-112,-785,-1375,-1258,-386,
607,1292,1468,1034,493,214,137,105,-119,-532,-866,-819,-345,157,496,588,-97,-1235,-1824,-1814,
-1468,-756,358,1641,2540,2714,1853,292,-617,-370,349,907,1078,700,-53,-536,-620,-499,-259,-57,
145,326,228,-19,-12,328,681,608,129,-381,-717,-609,15,808,1360,1277,382,-803,-1325,-769,
266,924,870,214,-836,-1869,-2136,-1559,-746,15,590,722,304,-285,-276,437,1103,1089,434,-248,
-241,592,1642,2284,2326,1661,461,-747,-1453,-1380,-790,-226,45,92,-25,-337,-554,-377,-155,-302,
-649,-1014,-1340,-1276,-594,525,1528,1911,1700,1249,910,791,758,756,639,177,-458,-1013,-1416,-1455,
-1047,-310,583,1059,553,-665,-1707,-1832,-905,530,1476,1436,928,552,431,477,476,283,-87,-492,
-594,-346,52,595,1151,1332,903,-65,-965,-1036,-196,919,1464,1124,99,-1133,-1844,-1793,-1436,-1147,
-1000,-935,-800,-460,76,721,1328,1595,1422,1060,700,570,874,1483,1874,1507,543,-327,-667,-466,
205,970,1200,537,-826,-2058,-2339,-1570,-357,334,-141,-1270,-1985,-1752,-563,1051,2114,2016,1018,71,
-93,455,1175,1385,833,15,-393,-250,326,1172,1719,1416,322,-990,-1760,-1588,-883,-304,-321,-923,
-1515,-1591,-1077,-328,294,644,770,965,1302,1404,1161,920,892,815,407,-9,-4,237,449,505,
119,-745,-1530,-1672,-1153,-322,489,902,822,519,103,-433,-947,-1303,-1266,-752,-205,36,246,695,
1141,1282,1038,573,205,101,288,750,1273,1373,818,-50,-644,-746,-524,-196,-123,-453,-782,-696,
-164,322,296,-149,-663,-782,-371,197,588,658,438,62,-260,-342,-253,-76,177,442,717,999,
1148,822,37,-446,-90,650,933,470,-271,-677,-685,-530,-309,-71,116,83,-257,-737,-1076,-1011,
-536,68,512,609,381,107,82,211,231,124,-6,-46,11,62,83,53,-103,-297,-380,-271,
171,714,751,214,-515,-1048,-968,-242,600,1010,787,134,-299,-3,884,1759,1930,1219,99,-734,
-871,-399,216,347,-231,-1030,-1376,-961,0,829,917,333,-445,-843,-522,182,610,475,139,114,
464,838,929,653,220,10,58,207,426,683,782,503,-152,-848,-1232,-1166,-648,89,505,344,
-114,-533,-646,-265,360,649,477,262,334,628,821,650,169,-309,-350,120,733,1126,1105,617,
-45,-600,-1055,-1284,-1253,-1285,-1449,-1371,-812,169,1316,2149,2205,1418,218,-570,-499,124,699,733,
101,-814,-1384,-1111,-188,622,769,239,-550,-1053,-859,-114,497,452,-9,-337,-253,169,596,724,
408,-276,-980,-1229,-749,206,1068,1431,1067,161,-644,-843,-458,180,703,875,653,238,-29,45,
344,619,716,557,131,-234,-230,-34,63,-34,-351,-709,-867,-718,-176,719,1520,1558,581,-852,
-1850,-1953,-1191,-8,939,1205,811,235,-53,-27,28,-62,-264,-390,-210,235,800,1247,1115,316,
-607,-999,-721,-77,609,1051,956,230,-889,-1846,-2069,-1468,-423,465,750,486,65,-106,96,411,
556,535,504,601,838,1140,1477,1624,1186,171,-922,-1394,-881,181,992,1051,298,-832,-1680,-1822,
-1343,-589,-19,1,-349,-521,-193,478,966,893,380,-127,-194,302,968,1343,1261,791,193,-302,
-602,-497,89,682,668,-20,-979,-1797,-2161,-1848,-1028,-279,-89,-488,-1033,-1096,-273,1113,2104,2034,
1117,196,-7,526,1166,1365,1031,384,-117,-117,292,708,761,409,-91,-474,-737,-944,-1046,-822,
-175,549,853,613,154,-95,-63,64,187,408,765,1010,1062,1134,1220,996,253,-641,-1073,-885,
-374,19,-29,-497,-1022,-1209,-1009,-702,-423,-61,300,450,342,90,-46,138,445,530,312,-74,
-426,-531,-308,89,322,187,-108,-234,-162,-12,57,-100,-332,-463,-409,12,659,999,763,152,
-423,-658,-554,-270,5,106,3,-45,216,721,1202,1334,998,452,100,124,462,847,955,561,
-266,-971,-1060,-610,5,402,370,-29,-570,-1013,-1206,-1171,-974,-628,-201,162,457,721,860,796,
599,282,-116,-397,-409,-151,263,532,469,273,117,-28,-141,-196,-262,-397,-541,-502,-195,82,
-4,-416,-819,-782,-75,986,1711,1675,1010,277,-15,134,361,378,173,-9,34,210,325,221,
-113,-555,-917,-1100,-1101,-963,-664,-231,211,503,530,385,293,313,488,844,1079,892,426,-3,
-169,-3,303,362,54,-354,-554,-378,157,679,728,213,-536,-1019,-949,-472,52,293,35,-562,
-1071,-1188,-909,-444,11,333,553,819,1122,1299,1262,1034,694,364,-33,-520,-771,-551,-97,163,
-19,-450,-703,-703,-696,-698,-528,-280,-184,-214,-149,171,669,1005,954,697,571,679,857,877,
730,559,440,407,503,605,559,340,-38,-482,-770,-803,-750,-766,-740,-585,-383,-150,57,9,
-283,-422,-194,186,520,688,654,580,591,584,521,506,482,272,-96,-410,-478,-343,-210,-211,
-310,-413,-494,-612,-670,-541,-303,-143,-164,-242,-146,214,679,936,800,385,-48,-252,-167,148,
523,713,571,170,-281,-509,-384,-94,34,-89,-325,-488,-508,-468,-474,-507,-510,-332,209,923,
1336,1202,653,60,-208,2,563,1112,1287,1065,767,575,410,168,-127,-366,-482,-501,-542,-682,
-791,-668,-386,-274,-501,-887,-1103,-934,-412,180,597,772,723,481,149,-26,138,528,803,635,
86,-458,-745,-791,-710,-562,-290,44,263,303,110,-246,-494,-462,-221,156,631,1078,1298,1216,
924,578,297,91,1,2,0,-83,-265,-481,-723,-922,-914,-655,-311,-52,115,267,461,686,
803,698,464,274,258,426,661,830,772,432,-1,-273,-331,-286,-246,-204,-90,73,168,115,
-177,-721,-1259,-1471,-1241,-674,-16,473,595,342,-10,-163,-28,241,334,196,113,370,872,1221,
1127,679,139,-275,-405,-270,9,319,414,75,-529,-1006,-1139,-947,-518,14,452,658,638,507,
404,371,288,131,45,128,307,452,455,266,-26,-283,-384,-273,-73,40,52,37,96,296,
423,201,-234,-592,-776,-802,-703,-521,-351,-280,-401,-635,-667,-225,617,1427,1746,1427,737,124,
-132,-72,73,98,-50,-233,-254,-81,124,121,-161,-461,-550,-555,-579,-486,-214,118,338,357,
207,-12,-150,-40,352,883,1254,1236,844,387,229,394,641,661,298,-302,-808,-1053,-1028,-815,
-585,-414,-296,-249,-225,-169,-66,108,371,686,959,1038,864,592,443,459,491,335,-8,-427,
-729,-675,-280,106,69,-468,-1085,-1300,-1021,-473,-33,19,-224,-410,-238,273,805,1026,867,559,
414,521,707,790,764,723,587,242,-268,-675,-690,-352,-9,26,-312,-733,-927,-809,-466,-127,
-60,-326,-660,-672,-174,649,1260,1240,639,-108,-549,-482,-45,464,689,497,98,-175,-145,169,
488,568,379,99,-54,-20,98,201,167,-115,-577,-950,-975,-515,233,793,836,424,-187,-745,
-957,-599,132,754,946,792,588,529,636,751,659,308,-120,-364,-318,-97,108,154,-21,-419,
-885,-1149,-1019,-549,-137,-97,-313,-432,-276,75,401,595,684,678,620,616,689,776,789,619,
236,-256,-611,-654,-428,-188,-141,-291,-557,-833,-939,-760,-352,49,168,-13,-251,-245,80,596,
1078,1279,1131,844,674,754,978,1085,916,511,-43,-614,-999,-1109,-990,-751,-583,-620,-781,-876,
-776,-473,-48,288,291,-30,-361,-362,77,759,1322,1414,959,282,-156,-165,98,358,336,-30,
-435,-541,-378,-137,40,38,-242,-628,-732,-417,98,533,635,317,-165,-478,-390,103,720,1112,
1071,642,113,-128,89,621,1132,1281,927,246,-400,-781,-948,-1096,-1275,-1437,-1455,-1171,-609,67,
600,747,581,378,287,331,606,1081,1375,1201,682,35,-556,-891,-898,-671,-326,33,375,591,
539,257,-109,-424,-639,-799,-826,-580,-144,223,351,231,-15,-255,-391,-254,201,677,883,828,
710,639,481,168,-165,-321,-194,-1,-4,-165,-401,-646,-786,-827,-762,-467,20,448,690,767,
736,677,621,536,393,205,75,88,196,271,212,79,-64,-205,-331,-353,-133,236,488,420,
23,-424,-650,-638,-490,-361,-374,-469,-523,-459,-271,-64,56,101,95,130,264,461,667,742,
554,171,-189,-278,-69,228,385,295,17,-246,-286,-126,57,128,99,68,165,417,680,825,
812,597,179,-276,-524,-479,-232,28,74,-182,-533,-673,-475,-80,248,346,256,83,-73,-144,
-153,-104,6,40,-128,-360,-433,-253,129,569,823,639,54,-490,-549,-59,666,1164,1117,613,
76,-219,-281,-222,-230,-439,-797,-1023,-856,-377,114,395,420,326,270,226,159,144,235,302,
204,-23,-192,-249,-220,-80,139,324,374,266,51,-139,-166,-10,161,168,-15,-248,-360,-307,
-154,41,242,363,322,172,51,22,69,131,117,-7,-103,-36,97,121,16,-157,-310,-304,
-134,115,364,505,434,135,-272,-572,-650,-510,-249,29,275,425,422,264,81,59,261,500,
512,255,-34,-79,132,379,411,190,-152,-454,-613,-579,-321,25,193,46,-300,-591,-634,-454,
-194,15,99,117,205,373,506,517,453,335,172,65,98,170,157,72,-14,-140,-345,-479,
-420,-231,-75,-1,25,21,-13,-31,22,151,328,465,461,310,124,68,186,349,388,295,
154,14,-67,-90,-148,-305,-459,-496,-436,-421,-471,-530,-581,-545,-333,-56,107,146,154,239,
470,771,956,948,761,478,213,74,94,221,387,503,449,164,-218,-475,-542,-518,-484,-435,
-417,-452,-439,-275,-15,214,290,166,-49,-112,105,475,756,763,501,151,-47,-17,130,269,
314,182,-108,-384,-494,-416,-253,-123,-109,-161,-174,-83,75,205,205,36,-231,-394,-314,-44,
207,264,131,-40,-41,164,375,426,292,71,-77,-75,-6,33,26,-48,-154,-249,-316,-315,
-209,-73,2,-7,-70,-90,-6,131,206,164,63,-17,-61,-53,21,179,420,652,705,506,
195,-39,-77,62,269,357,215,-42,-227,-245,-142,-97,-235,-460,-588,-464,-92,298,432,259,
-13,-165,-154,-61,85,226,324,415,507,497,301,-9,-302,-501,-613,-658,-609,-508,-429,-359,
-273,-193,-150,-127,-68,75,269,437,523,510,383,261,237,293,360,424,475,471,354,135,
-75,-201,-285,-367,-382,-309,-203,-104,-32,-7,-43,-131,-186,-150,-4,201,368,479,538,486,
315,128,12,-13,10,28,36,53,83,64,-32,-142,-227,-308,-386,-410,-359,-271,-189,-113,
-61,-49,-20,95,217,235,139,15,-7,68,138,148,105,46,32,106,226,268,160,-17,
-134,-154,-132,-113,-78,-61,-79,-71,38,223,360,364,229,40,-105,-89,96,339,498,517,
430,270,78,-82,-199,-270,-279,-248,-251,-281,-235,-111,-63,-166,-324,-423,-410,-290,-60,246,
523,681,662,457,197,65,77,132,172,152,36,-120,-196,-144,-37,9,-59,-192,-293,-275,
-154,13,125,75,-152,-404,-463,-250,109,385,384,132,-170,-320,-221,62,369,573,645,569,
338,58,-142,-211,-220,-278,-471,-709,-793,-614,-246,108,269,235,114,46,107,290,441,415,
230,32,-52,3,112,185,184,132,62,28,26,-30,-180,-335,-417,-390,-231,-26,122,167,
124,56,7,-10,10,92,248,439,597,664,619,464,264,54,-108,-150,-82,-11,-7,-53,
-102,-109,-106,-102,-96,-93,-103,-129,-130,-99,-15,92,161,163,76,-48,-72,51,215,291,
241,128,28,-34,-73,-88,-103,-148,-194,-245,-318,-365,-356,-310,-247,-163,-82,15,126,231,
337,431,467,386,202,16,-113,-174,-155,-42,110,221,232,126,-15,-58,33,150,146,-13,
-264,-481,-507,-280,99,379,381,146,-136,-269,-141,145,385,447,358,253,274,417,542,481,
190,-194,-501,-624,-557,-370,-185,-115,-192,-367,-490,-405,-144,139,311,307,149,14,36,188,
333,371,294,181,130,189,301,328,169,-132,-405,-482,-346,-101,136,273,251,70,-181,-377,
-451,-439,-380,-264,-131,-48,-40,-57,-40,8,63,111,138,146,153,174,194,161,38,-113,
-165,-36,195,334,285,114,-34,-91,-84,-61,-3,69,82,9,-113,-174,-104,21,81,49,
-11,-3,131,307,395,362,280,162,2,-127,-151,-44,143,264,202,-1,-235,-380,-347,-158,
70,209,242,245,294,367,379,259,31,-179,-244,-151,14,120,77,-81,-263,-361,-347,-263,
-176,-116,-64,-3,61,91,66,-13,-111,-149,-81,95,307,482,569,516,346,102,-159,-346,
-354,-182,30,106,-21,-261,-451,-470,-319,-139,-82,-145,-170,-61,115,244,281,263,226,183,
150,184,261,303,283,202,90,-22,-114,-151,-121,-30,55,27,-121,-265,-318,-288,-219,-157,
-113,-72,-9,90,219,312,310,250,201,234,333,433,472,415,237,-22,-247,-386,-447,-446,
-397,-329,-267,-232,-247,-286,-250,-68,216,468,554,444,226,52,-14,-31,-34,-13,48,136,
227,281,283,203,63,-88,-203,-271,-285,-241,-208,-219,-264,-313,-346,-324,-237,-139,-80,-37,
46,173,314,452,554,572,515,416,315,222,146,73,-16,-106,-170,-218,-259,-298,-304,-253,
-147,-34,42,25,-72,-203,-278,-261,-182,-60,75,180,230,203,115,22,-48,-98,-114,-75,
63,270,423,409,252,90,10,-19,-45,-92,-150,-142,-61,42,105,76,-33,-143,-164,-78,
65,188,226,178,121,145,244,337,333,175,-52,-213,-259,-235,-186,-152,-150,-166,-178,-143,
-54,53,117,105,21,-77,-106,-4,172,295,291,201,104,41,22,20,-9,-86,-207,-334,
-409,-396,-331,-256,-191,-113,-15,63,122,196,267,292,268,224,214,267,371,450,436,339,
224,127,40,-40,-103,-155,-224,-298,-324,-255,-124,-33,-73,-196,-285,-257,-86,147,317,333,
229,127,127,199,231,175,73,-11,-55,-16,77,145,102,-58,-272,-408,-376,-185,23,128,
94,-36,-179,-242,-166,6,207,361,391,271,80,-41,-2,122,193,143,22,-72,-86,-26,
34,16,-109,-272,-360,-309,-172,-27,75,126,153,159,126,97,116,173,231,249,193,93,
35,37,60,68,30,-58,-138,-155,-111,-62,-54,-71,-79,-57,-28,8,63,121,150,141,
93,19,-74,-154,-198,-208,-173,-95,7,121,214,224,133,13,-54,-48,-1,11,-49,-172,
-279,-284,-178,-42,38,39,-13,-69,-62,42,189,291,327,294,209,131,89,74,59,26,
-28,-99,-146,-136,-70,24,98,111,55,-39,-100,-70,11,36,-53,-178,-226,-150,16,187,
272,262,170,30,-96,-115,-18,96,112,11,-109,-151,-88,29,90,34,-93,-195,-220,-170,
-109,-75,-75,-98,-90,-9,100,170,188,177,133,41,-75,-136,-58,114,219,157,-21,-187,
-254,-244,-197,-145,-100,-36,79,230,340,349,271,167,102,93,133,218,303,302,212,75,
-72,-218,-303,-283,-194,-100,-37,-31,-77,-104,-75,-13,28,22,-34,-68,-12,121,222,200,
54,-118,-172,-47,184,383,460,405,267,91,-80,-212,-267,-247,-169,-75,-47,-107,-218,-302,
-317,-287,-237,-163,-66,40,148,242,265,198,59,-78,-134,-60,100,266,310,199,22,-116,
-173,-141,-61,10,43,29,-26,-98,-147,-154,-129,-106,-86,-55,6,76,119,103,17,-59,
-50,37,165,271,302,259,191,125,52,-39,-125,-164,-134,-53,0,-26,-114,-186,-161,-45,
77,95,-19,-186,-263,-141,119,302,278,92,-95,-143,-35,119,200,137,-56,-303,-476,-475,
-315,-113,40,86,11,-128,-192,-109,94,281,322,207,39,-50,5,166,316,347,261,116,
-17,-80,-38,45,85,36,-99,-227,-240,-134,-7,46,-10,-121,-183,-126,8,135,184,159,
110,104,130,151,138,90,25,-67,-187,-273,-273,-196,-86,-9,-5,-59,-137,-190,-174,-105,
-33,22,42,24,-22,-96,-161,-170,-99,9,108,175,197,182,157,156,171,145,72,-38,
-125,-137,-65,68,150,119,23,-37,-28,29,75,73,24,-48,-86,-95,-70,32,148,186,
135,72,40,56,120,215,300,321,220,24,-155,-212,-161,-106,-149,-251,-300,-227,-92,25,
78,71,16,-53,-118,-156,-149,-94,-20,33,31,-19,-48,-21,57,144,188,149,54,-54,
-106,-62,48,123,112,36,-52,-121,-151,-149,-114,-58,3,38,45,63,102,175,228,200,
79,-70,-162,-142,-55,1,-18,-95,-192,-243,-191,-25,154,233,155,-32,-192,-207,-66,118,
206,143,-27,-182,-207,-93,63,184,211,132,-3,-76,-35,75,164,149,51,-67,-155,-182,
-124,12,131,133,26,-94,-160,-142,-60,25,38,-44,-149,-183,-124,-33,4,-30,-93,-125,
-100,-24,81,170,205,202,181,157,139,132,112,85,38,-25,-81,-110,-112,-108,-111,-149,
-213,-258,-241,-171,-65,27,90,135,168,159,109,50,8,-11,-3,27,55,64,40,-30,
-115,-158,-135,-77,-24,-5,-19,-33,-29,-1,36,43,21,-19,-35,-14,28,61,72,74,
66,45,24,8,-7,-43,-78,-81,-40,0,10,0,-11,-40,-84,-116,-110,-49,46,131,
160,142,111,99,130,196,231,197,108,3,-71,-88,-59,-27,-53,-163,-296,-373,-346,-221,
-57,83,159,168,140,117,121,161,199,189,126,40,-17,-17,20,51,35,-24,-96,-151,
-151,-86,-25,-11,-54,-119,-154,-132,-70,10,58,46,-7,-71,-114,-123,-77,11,86,114,
104,93,93,98,95,67,-10,-114,-193,-191,-112,-18,17,-6,-57,-75,-51,-4,7,-34,
-92,-94,-30,71,136,151,132,113,102,102,104,72,-5,-89,-120,-68,0,32,9,-27,
-35,-22,-24,-44,-53,-41,-37,-55,-74,-74,-44,19,107,169,187,160,101,37,2,17,
51,64,49,8,-43,-74,-89,-89,-89,-90,-82,-70,-63,-54,-27,25,78,74,10,-63,
-88,-63,-23,1,-12,-50,-77,-82,-70,-34,17,75,110,100,53,-6,-48,-38,10,44,
27,-30,-84,-100,-73,-29,-19,-26,-21,28,83,113,108,95,57,-4,-74,-126,-153,-173,
-177,-154,-113,-71,-51,-49,-47,-26,10,34,57,83,108,124,113,75,36,0,-19,-19,
7,41,73,66,25,-23,-54,-74,-79,-82,-88,-107,-120,-119,-102,-62,13,105,164,168,
140,116,112,130,150,151,113,49,-23,-58,-51,-29,-52,-132,-221,-264,-223,-114,-13,14,
-41,-113,-110,-25,92,158,145,63,-30,-73,-45,15,58,60,29,18,50,110,135,113,
52,-23,-77,-105,-112,-100,-73,-44,-33,-40,-63,-78,-56,0,49,62,50,28,2,-22,
-37,-31,-18,4,29,81,139,156,101,2,-80,-105,-82,-60,-69,-99,-110,-77,-7,53,
44,-24,-103,-138,-98,-11,55,77,55,26,11,31,76,128,149,128,70,3,-37,-48,
-56,-73,-105,-149,-171,-154,-95,-20,41,50,-2,-71,-112,-94,-2,104,152,109,30,-17,
-10,27,55,62,36,4,-13,-18,-34,-71,-141,-231,-288,-268,-192,-96,-17,19,14,5,
17,40,82,129,165,186,176,152,110,51,1,-11,-2,2,-21,-72,-128,-158,-142,-94,
-50,-46,-76,-108,-106,-70,-31,-10,-12,-19,3,72,145,191,190,153,102,60,40,47,
57,16,-98,-220,-278,-251,-171,-96,-60,-80,-139,-198,-198,-125,-19,68,103,83,48,31,
62,126,167,151,80,1,-24,32,132,187,145,37,-68,-97,-47,16,32,-27,-131,-223,
-238,-170,-62,18,49,46,22,9,38,110,184,207,172,106,60,52,85,114,104,38,
-67,-163,-212,-199,-153,-116,-115,-140,-157,-144,-115,-99,-86,-59,-12,48,102,131,142,139,
119,97,79,80,90,96,80,53,19,-7,-22,-23,-25,-49,-82,-96,-103,-101,-108,-117,
-128,-132,-119,-85,-12,79,157,186,163,102,39,-4,-14,13,60,98,107,77,33,12,
6,-23,-75,-133,-159,-149,-131,-102,-72,-63,-70,-81,-78,-57,-28,15,58,85,85,54,
20,19,59,106,115,59,-31,-84,-63,8,80,113,82,-21,-147,-224,-212,-150,-91,-71,
-92,-138,-150,-113,-40,37,84,88,74,92,155,220,245,195,100,4,-47,-71,-68,-40,
0,30,28,-7,-55,-76,-58,-27,-24,-57,-105,-141,-121,-61,-6,10,-5,-30,-37,-12,
28,70,106,145,165,143,72,-16,-79,-83,-32,24,27,-43,-151,-209,-193,-123,-45,-9,
-19,-42,-43,-26,-11,-3,-2,-10,-29,-36,-20,33,107,163,164,99,16,-36,-43,-11,
25,44,31,12,8,24,42,31,-4,-49,-75,-77,-51,-15,9,15,13,16,16,2,
-31,-62,-67,-34,3,23,23,23,37,50,56,38,-5,-55,-74,-47,-10,-5,-39,-90,
-124,-123,-87,-45,-32,-44,-67,-66,-41,-3,32,56,59,53,55,66,86,92,86,60,
17,-37,-89,-109,-79,-25,28,51,34,-12,-50,-52,-13,44,87,98,75,40,14,-4,
-9,-10,5,25,27,13,11,35,79,114,116,86,52,54,72,76,43,-19,-96,-159,
-194,-181,-129,-57,-19,-33,-87,-129,-114,-39,45,84,55,-9,-54,-58,-16,29,45,14,
-44,-95,-95,-44,32,87,68,-30,-154,-222,-192,-84,16,47,-7,-80,-102,-63,-2,25,
5,-31,-49,-38,-13,15,36,44,30,12,5,13,40,69,92,93,76,46,-3,-40,
-47,-18,9,3,-41,-89,-104,-77,-56,-67,-93,-116,-111,-68,-2,59,97,106,104,95,
77,53,26,23,45,71,72,49,28,16,14,11,0,-26,-59,-80,-87,-80,-79,-83,
-74,-58,-51,-60,-71,-70,-42,2,57,87,93,80,65,55,53,49,38,20,-2,-26,
-40,-48,-53,-55,-48,-37,-53,-92,-132,-138,-114,-83,-65,-71,-85,-87,-58,-7,43,75,
76,54,32,29,48,85,124,139,115,53,-7,-34,-29,-14,-23,-71,-137,-183,-184,-130,
-58,3,28,21,10,21,55,96,127,141,132,107,73,50,50,57,50,4,-67,-134,
-170,-161,-131,-102,-95,-104,-125,-132,-112,-74,-34,-17,-28,-38,-14,44,99,124,120,95,
74,73,83,82,78,72,56,20,-46,-105,-136,-115,-66,-36,-54,-114,-185,-221,-192,-103,
-2,57,45,6,-3,39,110,157,140,72,-4,-45,-38,-14,13,36,51,60,51,23,
-1,-17,-10,-4,-13,-38,-68,-79,-76,-70,-75,-93,-97,-81,-47,-10,11,2,-6,2,
40,78,89,67,25,-17,-50,-77,-91,-94,-71,-22,32,65,67,44,6,-40,-69,-75,
-71,-71,-80,-80,-61,-40,-19,-10,-4,-9,-14,-12,0,31,59,59,30,-16,-32,4,
73,141,172,152,87,17,-29,-45,-57,-65,-74,-67,-52,-28,-20,-27,-59,-103,-137,-129,
-71,3,57,78,90,86,62,25,-12,-18,-5,13,21,12,-17,-47,-61,-59,-43,-33,
-36,-55,-61,-44,0,39,43,20,-14,-39,-38,-15,22,48,47,16,-17,-36,-29,14,
73,127,138,110,54,-2,-33,-31,-19,-18,-41,-73,-94,-88,-68,-54,-69,-103,-128,-120,
-75,-11,38,39,7,-22,-25,7,46,68,59,30,-3,-40,-71,-74,-53,-18,11,16,
-2,-31,-32,-20,-10,-31,-66,-102,-111,-80,-27,28,49,29,-14,-56,-64,-29,38,99,
120,89,39,-14,-58,-76,-72,-58,-50,-52,-46,-24,6,23,12,-15,-46,-45,-17,43,
95,99,44,-34,-91,-94,-53,3,32,28,-15,-59,-75,-60,-21,19,37,25,12,9,
36,70,78,47,-11,-61,-74,-61,-40,-22,-10,-9,-10,-17,-19,-24,-31,-39,-48,-61,
-68,-58,-45,-34,-30,-25,-21,-27,-34,-20,4,15,-12,-53,-78,-63,-22,21,40,20,
-25,-77,-111,-121,-104,-57,-7,24,28,25,34,50,71,95,105,92,58,20,-4,-16,
-26,-44,-69,-85,-90,-81,-54,-23,-1,0,-22,-44,-41,-23,-4,6,-4,-35,-65,-73,
-55,-18,12,14,-8,-32,-28,2,52,93,103,81,38,-14,-46,-52,-26,-1,-2,-32,
-70,-89,-76,-44,-7,9,-6,-36,-69,-86,-75,-44,-5,31,62,84,81,55,24,-1,
-19,-28,-36,-45,-36,-19,-2,6,-7,-31,-60,-71,-73,-77,-88,-96,-80,-29,24,60,
74,61,52,41,36,36,44,35,24,17,21,30,25,-8,-59,-102,-104,-82,-69,-82,
-117,-153,-161,-132,-95,-59,-38,-18,10,48,80,84,69,45,30,23,31,52,74,82,
62,22,-18,-50,-61,-49,-26,-13,-13,-22,-43,-66,-79,-71,-59,-49,-42,-34,-17,2,
24,42,59,75,74,57,25,-14,-45,-57,-41,-19,-7,-22,-50,-74,-86,-79,-60,-54,
-67,-88,-99,-79,-42,-4,19,20,0,-26,-44,-39,-15,12,25,26,25,24,40,49,
33,-13,-64,-90,-90,-78,-76,-86,-94,-106,-102,-83,-54,-19,6,5,0,0,28,76,
117,117,93,70,65,70,82,77,49,-4,-65,-113,-127,-104,-58,-31,-35,-67,-93,-93,
-74,-51,-51,-56,-54,-34,-1,39,66,79,59,22,-12,-29,-20,9,44,54,27,-25,
-73,-94,-90,-90,-110,-144,-170,-156,-109,-49,-4,12,14,13,18,30,49,68,68,55,
34,10,-4,-12,-10,0,3,-2,-18,-34,-53,-67,-76,-82,-72,-56,-41,-32,-29,-31,
-26,-24,-29,-47,-60,-52,-18,31,65,79,72,59,45,30,23,23,37,42,40,21,
-14,-50,-76,-83,-77,-63,-49,-49,-64,-73,-62,-31,2,20,12,1,-1,14,27,30,
17,0,-8,-4,4,11,8,7,5,-12,-46,-83,-102,-89,-57,-27,-11,-12,-18,-15,
2,20,26,16,-9,-34,-47,-45,-25,-2,19,23,19,11,7,6,9,13,9,0,
-22,-36,-33,-23,-17,-24,-46,-80,-104,-103,-73,-24,18,31,14,-17,-48,-57,-52,-36,
-27,-34,-44,-47,-35,-12,12,21,20,6,-8,-17,-9,13,28,31,14,-16,-46,-56,
-33,-14,-23,-55,-90,-107,-96,-66,-24,11,33,28,10,-11,-21,-9,18,33,29,7,
-18,-31,-26,-9,12,14,0,-22,-38,-42,-36,-33,-41,-54,-58,-53,-42,-27,-7,9,
20,26,27,32,46,55,42,13,-16,-27,-13,2,-1,-21,-54,-84,-102,-90,-70,-50,
-46,-73,-106,-108,-70,-10,38,48,27,-11,-34,-27,9,48,68,64,36,-1,-21,-9,
16,27,15,-18,-57,-77,-70,-46,-29,-34,-60,-94,-110,-90,-48,2,41,60,49,27,
12,19,42,63,74,63,33,-2,-18,-23,-25,-34,-49,-61,-57,-42,-18,-8,-5,-18,
-44,-61,-64,-50,-25,-6,0,-10,-23,-37,-37,-31,-19,-16,-21,-39,-49,-36,-1,26,
29,-5,-49,-85,-93,-82,-70,-60,-47,-32,-17,-2,12,16,14,4,-13,-24,-22,-8,
9,21,26,21,14,9,1,-8,-18,-30,-49,-59,-52,-25,13,34,32,7,-19,-36,
-35,-30,-19,-4,7,11,6,-3,-16,-23,-27,-37,-51,-74,-92,-101,-100,-90,-74,-48,
-20,5,27,38,44,34,13,-7,-20,-17,-7,8,16,15,2,-10,-18,-27,-41,-59,
-71,-77,-69,-54,-41,-37,-38,-58,-73,-84,-69,-43,-13,9,22,27,38,55,78,82,
63,27,-10,-30,-25,-14,-3,-21,-59,-101,-127,-128,-110,-89,-73,-64,-57,-43,-21,2,
17,27,32,40,48,53,53,40,13,-14,-32,-30,-18,-2,7,1,-13,-25,-28,-28,
-29,-35,-38,-39,-33,-18,0,10,18,17,15,14,20,25,22,7,-19,-34,-33,-19,
-10,-16,-33,-57,-74,-75,-59,-31,-16,-24,-44,-55,-46,-23,-9,-6,-12,-16,-1,18,
34,49,58,60,57,38,14,-6,-27,-53,-66,-60,-42,-23,-10,-12,-28,-47,-64,-70,
-58,-36,-15,-4,-3,-14,-19,-13,-5,-5,-14,-17,-12,0,10,14,4,-9,-17,-23,
-24,-28,-35,-34,-31,-24,-28,-40,-52,-61,-60,-46,-28,-4,12,22,22,13,10,14,
28,40,41,31,17,-1,-11,-22,-30,-43,-49,-59,-65,-60,-48,-37,-27,-25,-31,-37,
-33,-25,-15,-14,-12,-13,-8,-8,1,13,32,42,42,37,24,14,8,2,-15,-37,
-62,-81,-92,-96,-92,-84,-77,-68,-61,-42,-16,7,24,28,26,25,25,26,23,17,
7,-7,-18,-33,-34,-27,-19,-18,-35,-64,-85,-86,-66,-37,-24,-25,-30,-38,-37,-27,
-17,-4,1,4,7,17,27,33,27,22,12,8,6,11,15,9,4,-11,-39,-76,
-107,-111,-96,-72,-47,-34,-31,-21,-10,-1,-3,-16,-27,-34,-21,2,23,26,14,-2,
-17,-21,-21,-18,-14,-11,-8,-10,-31,-52,-58,-50,-31,-17,-14,-17,-23,-21,-16,-13,
-6,-9,-11,-12,0,23,45,50,36,8,-21,-40,-51,-50,-53,-62,-70,-73,-64,-52,
-34,-25,-29,-36,-45,-48,-40,-27,-23,-29,-32,-30,-17,0,9,9,3,-8,-11,-16,
-22,-29,-38,-44,-47,-50,-51,-58,-55,-47,-39,-34,-36,-41,-38,-27,0,20,24,16,
2,1,11,29,30,14,-18,-46,-53,-37,-9,10,23,23,18,5,-15,-31,-43,-56,
-61,-60,-46,-27,-8,-8,-21,-44,-61,-62,-49,-24,0,28,44,49,41,32,19,-1,
-29,-49,-55,-44,-28,-21,-31,-46,-55,-53,-44,-31,-26,-28,-32,-30,-19,-10,-3,-5,
-5,-1,6,13,11,9,4,4,1,-4,-24,-40,-46,-40,-29,-21,-23,-33,-46,-55,
-51,-38,-22,-12,-10,-5,2,8,16,20,16,8,5,4,-1,-2,-5,-10,-13,-19,
-33,-51,-63,-73,-72,-61,-47,-32,-18,-9,-4,-5,-2,2,14,21,30,26,12,-3,
-14,-17,-16,-16,-18,-21,-30,-28,-34,-46,-61,-70,-70,-61,-35,-7,15,25,19,5,
-8,-16,-11,0,7,8,-6,-22,-28,-25,-26,-27,-31,-33,-35,-32,-28,-26,-26,-39,
-58,-80,-88,-78,-60,-34,-16,-6,-3,-5,0,5,13,21,23,16,6,-2,-10,-20,
-31,-46,-51,-48,-37,-22,-16,-21,-35,-51,-57,-58,-56,-48,-37,-26,-15,-9,-5,-5,
-2,6,20,28,28,17,4,-8,-24,-32,-38,-37,-39,-38,-35,-33,-40,-49,-67,-71,
-64,-51,-34,-26,-16,-14,-14,-13,-7,1,4,2,4,7,12,15,14,11,3,-8,
-22,-33,-37,-31,-30,-25,-33,-41,-40,-36,-24,-11,-13,-17,-16,-15,-2,3,2,-9,
-14,-12,-5,11,20,19,7,-6,-19,-17,-8,-1,-3,-10,-22,-29,-35,-43,-48,-56,
-65,-68,-65,-51,-28,-8,5,1,-19,-35,-34,-13,10,26,21,13,11,10,12,4,
-9,-20,-27,-34,-41,-43,-40,-33,-26,-24,-29,-29,-25,-13,-3,4,12,14,18,16,
18,25,26,26,25,25,29,21,4,-27,-57,-67,-55,-32,-20,-18,-27,-40,-45,-40,
-31,-28,-31,-40,-39,-25,-13,4,9,8,9,12,14,17,13,6,-6,-13,-22,-26,
-34,-42,-47,-49,-44,-45,-51,-59,-67,-63,-53,-35,-19,-11,-2,-2,0,0,1,-1,
-5,-10,-16,-15,-20,-28,-33,-36,-45,-51,-56,-49,-32,-14,-4,-7,-18,-36,-49,-49,
-38,-23,-13,-13,-14,-15,-14,-9,-2,-3,-5,-7,-9,0,2,-11,-32,-56,-68,-75,
-63,-46,-25,-14,-15,-21,-24,-18,-12,-1,8,8,5,2,1,5,7,15,14,-1,
-20,-34,-31,-24,-17,-16,-23,-27,-26,-23,-17,-17,-22,-29,-35,-38,-37,-38,-41,-38,
-30,-15,-5,1,-3,-8,-5,1,9,15,10,0,-11,-16,-23,-35,-42,-52,-53,-55,
-55,-53,-51,-43,-36,-24,-12,-3,0,3,5,0,-8,-17,-19,-17,-13,-11,-9,-8,
-4,-5,-12,-22,-32,-37,-35,-36,-36,-39,-39,-31,-19,-12,-13,-17,-22,-18,-6,7,
16,19,10,1,-8,-14,-22,-24,-20,-21,-23,-30,-43,-47,-41,-33,-28,-30,-36,-39,
-35,-21,-13,-13,-18,-24,-19,-14,-4,8,9,3,-3,-4,-4,3,8,4,-4,-13,
-15,-4,6,14,14,-1,-16,-26,-24,-20,-14,-16,-22,-23,-19,-12,-13,-15,-22,-28,
-25,-22,-17,-14,-17,-18,-22,-28,-36,-38,-34,-23,-8,2,5,2,-1,0,0,-3,
-10,-21,-24,-17,-13,-14,-12,-21,-28,-28,-31,-27,-20,-14,-10,-2,-2,-6,-8,-12,
-15,-12,-7,-3,3,0,-4,-8,-16,-22,-24,-22,-16,-3,-1,1,-3,-10,-12,-18,
-17,-15,-15,-15,-19,-18,-15,-14,-18,-28,-30,-26,-14,-6,-2,-2,-6,-2,-2,-8,
-12,-18,-21,-21,-21,-23,-22,-26,-26,-20,-16,-7,-3,-1,-1,-8,-16,-26,-29,-28,
-18,-9,-9,-20,-30,-32,-28,-16,-14,-12,-14,-14,-10,-9,-11,-20,-29,-40,-45,-47,
-38,-25,-11,-6,-12,-20,-28,-30,-31,-30,-26,-24,-16,-17,-18,-25,-30,-30,-25,-24,
-20,-20,-16,-11,-1,3,2,-3,-13,-17,-17,-21,-23,-29,-38,-41,-45,-36,-28,-16,
-8,-4,-4,0,-1,-3,-10,-11,-13,-7,2,8,2,0,-6,-15,-26,-37,-48,-49,
-35,-19,-5,-2,-5,-13,-18,-21,-22,-30,-35,-38,-38,-31,-26,-24,-23,-23,-15,-5,
2,8,4,-2,-6,-9,-13,-19,-20,-17,-16,-16,-19,-29,-34,-41,-39,-34,-28,-21,
-19,-22,-25,-27,-24,-18,-13,-11,-11,-14,-14,-7,2,8,10,6,0,-7,-11,-10,
-18,-22,-30,-37,-40,-34,-32,-31,-32,-36,-35,-28,-21,-17,-21,-23,-23,-25,-24,-19,
-12,-7,-8,-16,-18,-21,-18,-13,-13,-13,-15,-22,-26,-25,-21,-22,-24,-30,-38,-36,
-24,-6,4,7,-1,-16,-24,-27,-20,-11,-1,5,5,4,-6,-8,-14,-16,-15,-12,
-12,-14,-16,-17,-19,-20,-14,-12,-19,-20,-20,-19,-13,-15,-15,-22,-24,-20,-19,-13,
-14,-21,-25,-26,-26,-23,-27,-39,-48,-53,-43,-29,-15,-12,-20,-25,-28,-23,-15,-14,
-14,-12,-13,-7,-7,-11,-20,-26,-28,-25,-24,-25,-23,-22,-16,-11,-12,-18,-21,-25,
-22,-14,-6,-5,-10,-15,-20,-16,-10,-3,-4,-11,-15,-19,-28,-30,-32,-39,-41,-41,
-37,-27,-21,-15,-12,-14,-21,-22,-24,-22,-20,-19,-14,-12,-10,-15,-20,-24,-25,-31,
-33,-34,-38,-35,-28,-20,-14,-13,-16,-17,-20,-16,-8,-9,-12,-20,-29,-33,-29,-17,
-11,-7,-10,-16,-13,-10,-2,-3,-4,-11,-20,-21,-24,-20,-14,-15,-23,-38,-49,-46,
-32,-15,-6,-6,-15,-19,-18,-10,1,8,-2,-10,-20,-21,-12,1,7,2,-12,-25,
-24,-18,-8,2,-5,-19,-31,-36,-33,-27,-26,-30,-37,-40,-36,-24,-14,-3,-1,-8,
-15,-18,-16,-9,2,5,-1,-11,-15,-14,-7,1,0,-7,-15,-25,-28,-25,-21,-16,
-21,-28,-32,-32,-28,-19,-17,-13,-15,-15,-11,-7,-3,-1,-3,-7,-11,-13,-14,-17,
-18,-23,-29,-38,-42,-35,-24,-14,-9,-9,-13,-20,-23,-27,-31,-35,-34,-26,-18,-8,
-1,-1,-7,-14,-18,-17,-14,-10,-8,-7,-12,-19,-22,-23,-27,-29,-29,-29,-28,-22,
-20,-17,-16,-22,-23,-24,-22,-14,-11,0,3,5,0,-10,-19,-17,-11,-9,-14,-21,
-26,-23,-17,-6,-2,-2,-5,-10,-9,-4,3,8,12,2,-10,-17,-19,-18,-20,-15,
-14,-11,-16,-21,-26,-22,-13,-10,-6,-12,-17,-20,-18,-9,-4,-4,-9,-9,-10,-6,
-1,-2,-10,-25,-30,-29,-21,-17,-16,-24,-28,-30,-28,-20,-18,-12,-15,-15,-16,-15,
-10,-6,-10,-12,-15,-11,-6,0,7,7,5,-9,-21,-29,-28,-25,-21,-19,-24,-29,
-36,-37,-31,-24,-13,-11,-13,-23,-23,-21,-17,-12,-16,-22,-28,-25,-20,-9,0,6,
-4,-18,-34,-45,-41,-31,-23,-21,-27,-37,-47,-50,-42,-32,-21,-18,-25,-35,-34,-28,
-11,0,3,-3,-15,-19,-17,-9,-6,-8,-23,-38,-44,-40,-28,-19,-16,-22,-33,-39,
-38,-33,-26,-22,-21,-17,-17,-13,-8,-2,-1,-5,-12,-15,-16,-8,8,16,14,4,
-12,-26,-29,-29,-25,-27,-30,-41,-42,-36,-24,-15,-16,-21,-28,-28,-20,-11,-5,0,
-7,-13,-17,-14,-9,-2,4,-1,-10,-19,-29,-35,-37,-34,-26,-17,-13,-9,-10,-10,
-7,-9,-10,-9,-7,-3,1,1,-1,-7,-15,-14,-16,-14,-16,-19,-23,-24,-25,-27,
-29,-28,-30,-31,-25,-13,-12,-16,-23,-31,-33,-23,-14,-8,-6,-13,-19,-24,-23,-24,
-24,-24,-26,-28,-28,-23,-16,-17,-20,-26,-34,-35,-34,-24,-14,-10,-7,-8,-8,-8,
-6,-5,-8,-8,-11,-9,-7,-12,-12,-24,-32,-37,-38,-38,-35,-33,-32,-29,-28,-24,
-21,-22,-21,-13,-9,-4,-1,2,-6,-9,-12,-12,-11,-7,-2,-7,-16,-26,-33,-35,
-30,-29,-32,-36,-38,-38,-27,-18,-15,-18,-26,-31,-32,-21,-9,0,6,5,2,6,
9,16,18,14,5,-9,-20,-26,-26,-27,-34,-42,-48,-48,-41,-27,-16,-14,-15,-19,
-19,-16,-5,4,7,6,2,-4,-6,-5,-10,-16,-23,-30,-28,-30,-24,-22,-22,-30,
-38,-43,-49,-43,-38,-34,-28,-25,-27,-22,-21,-12,-4,3,7,9,4,2,1,-3,
-5,-11,-10,-15,-19,-23,-32,-38,-49,-51,-50,-44,-34,-26,-26,-28,-27,-24,-15,-9,
-3,-3,-2,-3,0,0,4,-3,-7,-16,-24,-25,-22,-19,-16,-17,-28,-33,-38,-38,
-34,-27,-23,-30,-37,-39,-36,-29,-20,-22,-22,-21,-18,-9,3,12,14,9,-1,-9,
-13,-8,-11,-13,-25,-32,-43,-40,-37,-30,-30,-29,-27,-25,-16,-8,-6,-7,-11,-12,
-13,-7,-2,1,-1,-6,-13,-19,-19,-12,-6,-2,-3,-11,-18,-26,-28,-29,-24,-23,
-22,-25,-24,-19,-12,-10,-7,-13,-17,-16,-9,1,4,5,-2,-8,-13,-14,-10,-6,
-5,-4,-5,-5,-7,-14,-20,-23,-26,-28,-24,-23,-18,-14,-14,-13,-18,-22,-24,-25,
-18,-14,-10,-3,-2,-6,-10,-20,-26,-27,-21,-18,-11,-14,-21,-31,-38,-37,-32,-26,
-24,-23,-16,-14,-14,-8,-6,-9,-10,-8,-11,-9,-8,-14,-20,-26,-26,-28,-26,-25,
-21,-17,-15,-15,-16,-19,-24,-29,-26,-27,-22,-16,-12,-13,-13,-16,-19,-18,-12,-11,
-8,-12,-11,-10,-11,-12,-17,-21,-23,-30,-32,-32,-29,-26,-19,-15,-10,-7,-2,-4,
-2,-4,-4,-7,-14,-19,-17,-14,-13,-15,-21,-27,-26,-23,-20,-21,-24,-25,-24,-24,
-21,-20,-20,-19,-22,-25,-22,-21,-15,-13,-9,-10,-10,-12,-12,-6,-4,-3,-13,-22,
-32,-33,-29,-23,-18,-16,-17,-20,-14,-11,-9,-10,-16,-23,-26,-20,-12,-5,-5,-9,
-13,-16,-16,-14,-11,-9,-7,-10,-7,-8,-5,-3,-4,-10,-17,-21,-26,-22,-13,-6,
-7,-12,-20,-22,-21,-17,-14,-17,-21,-26,-27,-27,-23,-20,-19,-25,-26,-24,-15,-4,
6,3,0,-8,-12,-12,-8,-10,-10,-14,-21,-28,-28,-27,-21,-19,-22,-18,-15,-13,
-9,-14,-15,-21,-19,-18,-15,-12,-14,-12,-12,-12,-10,-14,-14,-14,-17,-17,-13,-12,
-9,-9,-16,-19,-26,-23,-18,-16,-12,-13,-13,-12,-12,-16,-14,-12,-9,-6,-7,-6,
-8,-11,-17,-21,-24,-20,-14,-8,-2,0,-3,-6,-12,-15,-13,-13,-8,-5,-7,-6,
-11,-11,-16,-18,-17,-19,-17,-13,-13,-14,-13,-15,-15,-10,-12,-11,-12,-10,-6,-8,
-9,-13,-18,-16,-15,-13,-8,-9,-8,-2,-4,-6,-12,-15,-20,-20,-18,-11,-9,-6,
-9,-9,-8,-9,-10,-9,-10,-10,-12,-8,-7,-7,-3,-1,-4,-4,-11,-15,-19,-22,
-22,-24,-25,-22,-15,-13,-9,-11,-15,-11,-11,-12,-6,-3,0,-1,3,-1,-4,-7,
-9,-13,-13,-11,-16,-17,-19,-20,-22,-17,-16,-12,-11,-15,-17,-20,-18,-18,-19,-17,
-16,-13,-9,-8,-7,-5,-2,-6,-8,-11,-15,-18,-20,-18,-15,-16,-14,-14,-14,-16,
-18,-20,-22,-20,-21,-18,-14,-12,-10,-11,-12,-12,-10,-13,-9,-12,-13,-14,-13,-10,
-12,-7,-9,-17,-18,-24,-28,-29,-26,-29,-24,-24,-24,-26,-25,-27,-25,-25,-19,-14,
-15,-15,-20,-19,-18,-16,-13,-15,-20,-21,-25,-22,-16,-15,-12,-11,-13,-12,-13,-15,
-13,-15,-16,-18,-18,-20,-20,-18,-17,-16,-17,-21,-20,-20,-16,-10,-10,-10,-14,-18,
-19,-18,-14,-11,-10,-12,-12,-11,-7,-11,-11,-10,-14,-19,-18,-17,-13,-9,-7,-6,
-10,-14,-15,-14,-14,-10,-4,0,-2,-1,-1,-2,-6,-10,-16,-19,-20,-18,-19,-16,
-14,-12,-14,-15,-14,-13,-11,-9,-10,-10,-14,-15,-13,-10,-7,-6,-2,-4,-8,-8,
-10,-10,-8,-13,-13,-14,-14,-14,-10,-11,-11,-15,-19,-22,-19,-14,-8,-7,-10,-18,
-22,-15,-9,-3,-3,-3,-9,-12,-11,-9,-7,-9,-7,-6,-5,-4,-2,-2,-3,-5,
-9,-6,-8,-7,-4,-6,-6,-8,-11,-8,-7,-6,-4,-7,-6,-7,-10,-10,-14,-14,
-15,-16,-11,-6,-5,-3,-7,-4,-11,-9,-6,-10,-11,-16,-16,-17,-10,-8,-9,-11,
-14,-18,-15,-15,-12,-10,-14,-17,-17,-19,-19,-19,-19,-20,-21,-21,-17,-13,-14,-14,
-19,-21,-19,-20,-21,-20,-17,-15,-18,-20,-17,-17,-16,-13,-13,-14,-14,-14,-15,-13,
-17,-16,-18,-18,-13,-11,-8,-9,-11,-12,-16,-12,-8,-9,-6,-11,-15,-18,-22,-20,
-20,-24,-25,-26,-22,-14,-12,-7,-5,-7,-10,-7,-6,-7,-9,-11,-15,-16,-16,-16,
-15,-15,-15,-16,-14,-12,-8,-11,-6,-12,-9,-9,-10,-9,-7,-9,-7,-2,-3,-7,
-12,-14,-14,-13,-10,-8,-11,-13,-16,-14,-16,-11,-15,-16,-15,-17,-14,-9,-11,-12,
-14,-18,-16,-9,-6,-3,-6,-1,-2,-2,-5,-6,-10,-10,-13,-9,-11,-8,-10,-14,
-17,-20,-22,-22,-19,-14,-9,-2,-4,-6,-11,-11,-7,-9,-8,-9,-8,-7,-9,-5,
-5,-9,-13,-16,-18,-13,-9,-6,-5,-8,-6,-6,-7,-7,-5,-13,-14,-17,-15,-13,
-12,-13,-13,-13,-11,-12,-8,-8,-7,-10,-13,-19,-16,-13,-12,-11,-13,-14,-18,-17,
-18,-21,-23,-24,-22,-22,-16,-10,-8,-7,-8,-9,-11,-13,-13,-11,-10,-14,-14,-17,
-21,-19,-19,-20,-21,-21,-20,-23,-21,-18,-19,-21,-18,-18,-18,-15,-15,-15,-15,-14,
-19,-19,-20,-17,-16,-11,-7,-7,-7,-12,-14,-16,-19,-19,-17,-18,-18,-16,-16,-16,
-17,-20,-17,-15,-15,-11,-12,-12,-12,-8,-8,-8,-11,-14,-15,-14,-12,-12,-15,-16,
-16,-17,-16,-19,-17,-17,-21,-22,-21,-19,-15,-16,-16,-18,-18,-17,-9,-7,-8,-6,
-11,-14,-12,-8,-7,-8,-9,-13,-17,-20,-16,-17,-16,-19,-22,-22,-21,-17,-16,-15,
-14,-17,-17,-17,-15,-13,-11,-15,-13,-13,-12,-8,-9,-6,-7,-8,-9,-13,-14,-16,
-12,-9,-9,-11,-11,-13,-12,-13,-13,-16,-19,-14,-12,-11,-8,-6,-11,-10,-8,-9,
-8,-8,-11,-10,-12,-9,-8,-6,-5,-8,-11,-12,-11,-15,-13,-15,-19,-16,-17,-17,
-17,-16,-16,-16,-12,-11,-9,-9,-8,-8,-8,-8,-5,-1,-2,-3,-5,-11,-15,-18,
-12,-10,-9,-12,-9,-8,-7,-5,-5,-7,-6,-6,-8,-5,-2,-2,-5,-7,-11,-10,
-10,-6,-5,-3,-5,-4,-5,-3,-7,-4,-5,-6,-7,-7,-7,-5,-5,-3,-9,-8,
-7,-3,-2,1,3,0,-5,-7,-9,-9,-7,-8,-9,-10,-10,-11,-9,-9,-8,-8,
-8,-7,-5,-1,2,2,-1,-5,-9,-10,-11,-13,-11,-13,-14,-15,-14,-11,-11,-10,
-15,-17,-17,-15,-9,-3,-4,-2,-7,-7,-3,-4,-1,-7,-8,-12,-15,-17,-13,-18,
-14,-20,-23,-23,-23,-19,-15,-13,-12,-12,-9,-12,-6,-6,-7,-10,-15,-16,-19,-16,
-11,-9,-9,-10,-12,-11,-10,-9,-2,-6,-10,-10,-13,-10,-4,-4,-8,-12,-17,-18,
-17,-9,-4,-4,-7,-10,-10,-11,-9,-8,-10,-12,-14,-12,-12,-7,-7,-5,-8,-10,
-8,-8,-9,0,2,0,-4,-7,-14,-16,-10,-6,-6,-5,-11,-13,-12,-11,-9,-8,
-12,-9,-9,-6,-3,1,-2,-6,-9,-13,-15,-16,-12,-8,-7,-6,-8,-9,-10,-10,
-8,-6,-3,0,-2,-7,-11,-8,-7,-8,-5,-5,-9,-8,-7,-5,-5,-7,-8,-9,
-9,-7,-8,-10,-11,-12,-14,-18,-14,-16,-11,-9,-2,-1,0,-1,0,1,-1,1,
2,0,1,0,-4,-5,-6,-6,-8,-10,-9,-5,-2,-1,-5,-9,-5,-8,-5,-3,
-2,-2,3,-1,-3,-3,-3,-4,-6,0,-4,-3,-4,-3,-2,-3,-7,-8,-8,-8,
-8,-9,-10,-8,-7,-8,-8,-9,-8,-7,-5,-3,-3,-3,-6,-8,-10,-10,-8,-7,
-8,-6,-9,-11,-13,-12,-11,-9,-10,-8,-7,-5,-4,-4,-2,1,-4,-2,-6,-5,
-5,-6,-7,-8,-10,-10,-9,-10,-8,-4,-8,-6,-8,-9,-10,-6,-6,-3,-4,-7,
-8,-6,-10,-4,-3,-3,-6,-4,-6,-4,-3,-7,-6,-8,-8,-7,-5,-3,1,-2,
-5,-12,-15,-16,-10,-8,-7,-4,-7,-7,-3,-2,-2,0,-5,-11,-12,-11,-10,-12,
-8,-9,-11,-12,-12,-10,-10,-8,-7,-11,-13,-15,-16,-13,-9,-9,-9,-7,-12,-8,
-7,-6,-5,-8,-7,-11,-12,-11,-9,-8,-5,-6,-6,-6,-8,-7,-8,-6,-7,-11,
-10,-13,-11,-12,-10,-9,-11,-11,-12,-12,-9,-9,-9,-7,-8,-10,-12,-11,-13,-13,
-13,-15,-14,-16,-15,-15,-11,-9,-6,-5,-7,-7,-5,-9,-11,-10,-11,-12,-12,-13,
-14,-15,-16,-20,-21,-19,-16,-15,-16,-13,-14,-12,-11,-13,-16,-14,-15,-15,-14,-11,
-12,-16,-13,-10,-9,-6,-5,-1,-6,-3,-8,-8,-11,-11,-12,-12,-13,-13,-15,-14,
-13,-15,-18,-18,-20,-19,-19,-17,-13,-14,-15,-14,-13,-13,-15,-15,-17,-14,-12,-15,
-15,-13,-15,-13,-10,-10,-12,-12,-15,-18,-17,-18,-16,-20,-20,-21,-19,-17,-13,-13,
-13,-14,-15,-17,-15,-16,-15,-14,-16,-17,-15,-13,-15,-14,-18,-18,-22,-22,-21,-22,
-13,-13,-15,-14,-13,-12,-12,-13,-14,-15,-15,-14,-15,-16,-17,-18,-14,-14,-14,-17,
-13,-10,-11,-10,-12,-11,-15,-15,-17,-17,-15,-19,-19,-17,-16,-14,-12,-13,-14,-16,
-15,-17,-14,-13,-8,-9,-11,-12,-15,-15,-16,-12,-14,-14,-9,-11,-15,-16,-17,-15,
-12,-14,-14,-13,-16,-13,-12,-10,-11,-13,-13,-13,-11,-6,-5,-3,-7,-7,-12,-12,
-12,-7,-4,-4,-6,-7,-10,-8,-9,-7,-8,-8,-9,-5,-4,-5,-2,-4,-6,-10,
-8,-5,-7,-2,-1,-1,-2,-4,-5,-3,-4,-3,-2,-3,-6,-5,-2,-5,-3,-3,
-6,-5,-6,-5,-6,-6,-9,-9,-9,-7,-5,-5,-5,-5,-2,-1,-2,-1,-1,-4,
-5,-4,-4,-3,-2,-2,-4,-6,-8,-8,-7,-5,-7,-5,-7,-7,-8,-8,-3,-4,
-5,-4,-8,-5,-4,-2,-3,-2,-4,-2,-3,-2,-5,-7,-6,-11,-9,-11,-13,-13,
-14,-13,-13,-17,-12,-12,-11,-9,-5,-7,-9,-10,-7,-5,-6,-4,-6,-6,-6,-7,
-6,-8,-6,-8,-6,-7,-7,-7,-5,-7,-9,-10,-8,-10,-8,-5,-7,-7,-4,-3,
-5,-4,-4,-4,-2,-3,-1,-2,-2,-2,-2,-4,-2,-4,-6,-8,-9,-10,-10,-13,
-9,-12,-11,-10,-11,-11,-10,-9,-10,-12,-11,-11,-8,-7,-6,-6,-6,-8,-5,-3,
1,1,-1,-3,-5,-6,-7,-6,-9,-4,-5,-7,-10,-10,-12,-13,-10,-13,-10,-12,
-10,-5,-5,-1,-4,-1,-5,-7,-7,-5,-4,-1,-2,-4,-7,-7,-9,-6,-5,-7,
-8,-10,-12,-10,-9,-10,-10,-11,-13,-13,-12,-8,-8,-7,-6,-6,-6,-5,-2,0,
-2,-3,-5,-8,-10,-10,-9,-9,-7,-10,-9,-13,-11,-11,-11,-10,-12,-9,-9,-6,
-5,-3,-6,-8,-10,-9,-9,-6,-6,-6,-9,-11,-11,-10,-10,-11,-10,-10,-12,-9,
-9,-11,-10,-11,-11,-10,-13,-12,-11,-11,-9,-12,-10,-12,-9,-8,-8,-7,-1,-2,
-3,-6,-8,-9,-8,-7,-7,-12,-10,-8,-10,-8,-7,-7,-9,-11,-7,-7,-5,0,
0,-2,0,0,-3,-1,-2,-2,-1,-3,-1,-4,-6,-2,-5,-6,-8,-9,-10,-11,
-10,-9,-7,-9,-9,-8,-6,-5,-6,-3,-3,0,0,1,-2,-3,-2,-2,-2,-1,
-2,-4,-2,-2,-5,-4,-6,-8,-6,-7,-7,-8,-7,-7,-3,-6,-2,-6,-1,0,
-1,1,-1,0,-2,0,1,2,2,-1,-2,-3,-4,-5,-6,-6,-7,-5,-6,-4,
-4,-4,-6,-6,-4,-5,-4,-2,-2,-4,-2,0,-1,-3,-4,-2,-6,-2,-2,-4,
-6,-10,-8,-7,-8,-5,-6,-6,-9,-7,-7,-7,-6,-3,-6,-8,-7,-5,-5,-2,
-1,-2,-5,-3,-2,-2,-2,-1,-2,-4,-2,-5,-5,-8,-4,-7,-6,-7,-7,-10,
-6,-8,-8,-7,-6,-6,-6,-7,-6,-6,-6,-6,-8,-11,-8,-7,-3,-5,-4,-7,
-10,-8,-10,-8,-8,-9,-9,-7,-9,-7,-9,-10,-9,-12,-10,-11,-8,-8,-8,-7,
-5,-6,-9,-6,-7,-6,-10,-9,-13,-14,-12,-12,-10,-8,-9,-5,-8,-6,-4,-4,
-1,-4,-4,-5,-7,-9,-4,-7,-5,-3,-5,-7,-8,-7,-8,-8,-6,-5,-5,-4,
-5,-7,-3,0,-1,-4,0,-1,-3,0,-1,-2,-2,-3,-5,-4,-1,0,-1,-2,
-2,-1,-3,-2,-1,-1,0,0,-1,-1,-1,-2,-5,-6,-7,-5,-3,-2,-1,-1,
-3,-3,-3,-4,-6,-6,-4,-2,-3,-2,-1,-5,-6,-9,-7,-6,-6,-2,-3,-1,
0,1,0,-1,-1,1,0,1,1,-1,1,-2,-3,-3,-5,-4,-3,-4,-3,-3,
-2,-3,-4,-3,-4,-3,-3,0,0,0,0,2,-4,-2,-3,-3,-1,-2,-2,-3,
-3,-4,-5,-3,-3,-5,-4,-4,-4,-5,-3,-6,-5,-7,-7,-4,-1,0,4,1,
-1,-1,0,-1,-1,1,-1,-3,-2,-6,-5,-7,-8,-6,-7,-8,-7,-6,-8,-8,
-7,-5,-8,-5,-4,-4,-5,-3,0,0,2,0,-1,1,-1,-2,0,0,0,-1,
-2,-3,-3,-6,-9,-7,-8,-8,-5,-7,-7,-5,-5,-5,-5,-5,-6,-7,-7,-7,
-8,-9,-9,-10,-8,-8,-7,-7,-7,-9,-9,-8,-5,-8,-9,-10,-8,-9,-6,-4,
-9,-8,-11,-11,-8,-9,-7,-6,-8,-10,-8,-10,-10,-9,-9,-8,-10,-13,-11,-12,
-14,-11,-12,-10,-11,-9,-9,-6,-7,-9,-9,-8,-12,-9,-8,-9,-9,-8,-10,-11,
-10,-8,-9,-11,-8,-13,-14,-13,-15,-14,-12,-11,-9,-7,-9,-7,-7,-8,-7,-8,
-9,-10,-10,-7,-8,-10,-7,-9,-8,-7,-6,-4,-5,-4,-5,-9,-7,-8,-7,-4,
-6,-6,-4,-2,-2,1,0,-2,-4,-4,-3,-3,-3,-2,-4,-5,-7,-7,-7,-8,
-9,-7,-10,-8,-6,-7,-7,-6,-4,-3,-3,-4,-5,-8,-9,-9,-8,-7,-3,-5,
-8,-6,-8,-6,-8,-6,-8,-9,-9,-7,-9,-7,-7,-7,-8,-9,-11,-7,-8,-4,
-7,-8,-7,-5,-6,-6,-8,-8,-8,-5,-9,-6,-3,-5,-8,-10,-9,-8,-8,-8,
-7,-6,-4,-7,-9,-8,-9,-4,-5,-4,-3,-6,-6,-3,-6,-6,-3,-8,-4,-4,
-6,-1,-1,-4,-3,-3,-4,-6,-6,-5,-7,-8,-5,-6,-5,-7,-6,-7,-7,-7,
-5,-8,-4,-4,-6,-4,-4,-3,-2,-2,-4,-3,-2,-1,-1,-1,-3,-3,-4,-7,
-5,-4,-4,-6,-6,-7,-8,-8,-5,-4,-3,-3,-3,-4,-6,-2,-2,-6,-5,-3,
-3,-3,-3,-1,-1,-1,-1,-3,-3,-2,1,-1,3,-2,-3,-4,-2,-3,-4,-2,
-2,-3,-1,-1,0,-1,2,2,1,0,1,2,5,4,4,1,-1,-1,-1,1,
1,1,3,2,1,1,1,2,1,0,1,1,2,1,1,-1,0,2,3,-2,
1,-1,2,0,-2,-2,-1,-3,-3,-3,-4,-1,-5,2,-1,-2,0,2,0,3,
2,0,2,1,1,2,3,2,0,1,1,-1,0,-3,1,-1,-1,0,0,-1,
-2,0,-1,0,-2,-2,-1,-2,-2,-1,-3,-2,-2,1,-4,0,-1,-2,-2,0,
-3,-1,-3,-4,-3,-4,-5,-3,-5,-4,-5,-2,-2,-1,-3,-4,-4,-6,-5,-4,
-6,-5,-6,-7,-9,-7,-8,-10,-8,-11,-11,-13,-10,-8,-8,-6,-6,-10,-5,-8,
-7,-7,-7,-8,-6,-8,-7,-8,-8,-11,-10,-10,-9,-10,-10,-12,-11,-11,-12,-11,
-13,-13,-12,-11,-10,-9,-8,-10,-9,-10,-11,-12,-11,-11,-11,-9,-11,-10,-9,-10,
-10,-11,-12,-10,-12,-11,-9,-9,-10,-11,-7,-9,-10,-8,-10,-8,-8,-9,-8,-8,
-5,-7,-4,-6,-4,-6,-4,-2,-5,-5,-6,-7,-7,-5,-4,-4,-6,-7,-5,-7,
-7,-6,-6,-4,-5,-6,-7,-7,-7,-8,-10,-9,-10,-8,-7,-10,-7,-9,-8,-8,
-5,-6,-6,-4,-3,-4,-7,-7,-6,-3,-6,-6,-6,-4,-2,-2,-3,-4,-3,-7,
-8,-6,-6,-7,-3,-4,-6,-5,-5,-4,-2,-5,-4,-4,-3,-1,-4,-2,-4,-4,
-5,-4,-5,-5,0,-1,-1,1,0,-2,1,3,1,-1,1,1,0,-1,-1,0,
-2,-5,-3,0,-3,-2,1,-1,2,0,2,-1,0,0,1,2,0,3,2,2,
2,3,1,0,2,0,-2,-1,-2,-1,-3,-4,0,-2,-1,-4,-4,-3,-4,-3,
0,-3,-2,-1,-2,-3,-2,-1,0,2,2,0,-1,0,-1,2,-2,0,-1,-1,
1,-2,1,0,-2,-4,-1,-5,-2,0,-1,0,2,0,1,0,-2,1,-2,-1,
-2,-1,-1,2,1,-1,2,1,0,-1,1,1,0,0,-3,-1,0,-2,-2,1,
0,1,2,1,2,0,0,-1,-1,0,1,1,0,0,1,-1,-1,1,2,1,
-3,0,-2,-1,-4,-2,-5,-3,-5,-3,-4,-3,-2,-4,-5,-6,-4,-5,-5,-4,
-1,-5,-1,0,-2,-2,0,0,-2,-2,-3,-2,-2,-4,-6,-4,-8,-5,-6,-7,
-7,-8,-6,-6,-7,-6,-4,-3,-5,-3,-3,-5,-3,-5,-3,-3,-1,-3,-4,-7,
-4,-8,-6,-5,-7,-8,-7,-8,-9,-7,-7,-4,-8,-6,-7,-8,-7,-7,-7,-8,
-8,-5,-6,-8,-5,-9,-6,-9,-7,-8,-5,-7,-5,-5,-9,-12,-12,-12,-10,-12,
-11,-10,-12,-8,-9,-7,-9,-8,-9,-9,-7,-8,-6,-5,-5,-5,-8,-5,-7,-5,
-6,-5,-8,-7,-4,-7,-4,-6,-6,-7,-7,-7,-7,-6,-5,-7,-6,-6,-8,-7,
-5,-5,-7,-8,-8,-7,-8,-8,-6,-6,-6,-4,-3,-6,-7,-5,-8,-9,-9,-7,
-9,-8,-8,-10,-8,-8,-7,-7,-8,-5,-7,-6,-8,-6,-7,-7,-6,-7,-7,-7,
-5,-4,-3,-6,-7,-8,-11,-9,-7,-6,-4,-4,-5,-6,-7,-8,-7,-8,-9,-11,
-11,-7,-7,-7,-6,-8,-8,-8,-10,-10,-12,-8,-11,-11,-11,-9,-11,-11,-13,-11,
-12,-12,-12,-9,-7,-9,-8,-9,-7,-7,-5,-6,-9,-8,-10,-10,-7,-10,-8,-8,
-9,-8,-7,-6,-9,-8,-6,-7,-5,-9,-7,-8,-9,-6,-9,-8,-11,-8,-10,-7,
-10,-9,-8,-9,-9,-12,-9,-10,-11,-11,-7,-10,-9,-8,-7,-7,-7,-5,-8,-8,
-9,-8,-7,-6,-10,-9,-8,-7,-9,-9,-9,-10,-14,-9,-12,-11,-10,-10,-10,-9,
-7,-8,-9,-8,-6,-6,-8,-10,-8,-8,-8,-9,-6,-5,-9,-6,-8,-10,-9,-9,
-10,-14,-12,-8,-9,-9,-9,-8,-10,-8,-9,-7,-9,-7,-6,-8,-8,-5,-8,-6,
-9,-6,-5,-8,-6,-4,-6,-7,-6,-8,-9,-6,-4,-7,-6,-7,-7,-7,-8,-6,
-4,-5,-5,-4,-3,-2,-1,-2,1,1,0,0,-1,-1,0,-3,-5,-4,-3,-5,
-4,-7,-7,-5,-6,-5,-4,-4,-6,-7,-3,-5,-6,-3,-2,-1,-2,-4,-2,-2,
0,-3,0,-4,-2,-3,-4,-3,-5,-7,-6,-7,-7,-9,-8,-7,-8,-5,-6,-3,
-3,-4,-2,-2,-3,0,-2,-2,-4,-3,-4,-6,-5,-4,-6,-2,-4,-2,-3,-3,
-3,-6,-5,-6,-5,-4,-4,-4,-6,-6,-4,-5,-2,-2,-2,-5,-4,-2,-3,-4,
-2,-4,-6,-7,-5,-3,-5,-5,-4,-4,-5,-4,-3,0,-3,0,0,-4,-2,-2,
-1,0,-3,-2,-4,-3,-2,-3,-3,-1,-4,-2,-2,-2,-2,1,0,0,-2,-2,
-1,-4,-1,-2,-3,-3,0,-1,-1,0,-3,-2,0,-1,0,-2,2,0,-1,-4,
-1,-2,-4,-4,-5,-3,-4,-5,-4,-5,-3,-3,-1,0,-3,-2,0,-1,1,0,
-2,0,-2,0,-3,1,-3,1,-2,-3,-4,-3,-2,-4,-5,-3,-4,-2,-2,-2,
-1,-2,-1,-3,-1,1,0,0,-2,1,1,-1,1,3,3,3,2,1,0,0,
-2,-4,-3,-1,-2,-1,-1,-2,0,-2,-1,-1,-1,-3,1,-3,-1,0,-2,-1,
1,-1,-1,-2,-1,0,1,0,0,2,2,5,2,0,2,4,2,4,0,1,
2,2,1,-3,-1,1,0,1,2,2,1,4,3,4,3,7,6,6,5,7,
6,6,7,4,5,4,7,9,7,4,4,6,3,2,5,6,7,5,6,6,
6,7,6,7,6,7,9,6,7,4,6,7,7,8,6,8,7,4,5,6,
6,5,6,4,2,4,5,4,5,5,4,4,6,6,5,4,6,7,6,6,
8,7,8,3,4,6,3,6,3,5,3,3,5,3,4,4,3,4,5,6,
2,4,4,6,4,7,3,4,6,4,5,4,4,3,4,3,3,4,4,2,
3,0,5,2,3,3,2,5,5,4,3,3,3,1,3,1,2,2,2,0,
1,1,3,1,0,1,0,0,0,-1,-1,-1,-1,-1,-3,-2,0,-1,-2,1,
2,0,1,2,1,1,4,1,3,1,0,-2,2,1,0,-2,-4,-2,-1,-1,
0,0,-2,-2,-2,1,0,1,2,3,0,1,2,1,3,3,2,2,2,2,
2,5,1,3,3,2,-2,-1,2,-2,0,1,-2,0,1,0,3,2,1,-2,
1,0,1,-1,0,1,1,1,-1,1,1,1,-1,-1,-2,-2,-2,-2,-3,-3,
-4,-2,-1,-2,-1,-3,0,-3,-1,-2,0,0,-1,1,0,-3,0,-4,-3,-4,
-1,-3,-2,-2,-3,-2,-5,-3,-3,-3,-1,-2,-1,-2,0,-2,-1,-3,-2,-2,
-3,-3,-2,-1,-3,-1,1,-2,-3,-2,-4,-3,-2,-1,-4,-4,0,-4,-5,-3,
-5,-4,-7,-6,-6,-4,-4,-7,-6,-6,-5,-4,-4,-3,-5,-4,-2,-4,-2,-2,
-3,-2,0,-2,-4,-2,-5,-5,-4,-2,-1,-3,-3,-4,-4,-4,-5,-1,-3,-3,
-5,-3,-5,-5,-5,-4,-4,-2,-3,-3,-2,0,-2,0,0,-1,1,1,-1,0,
1,1,0,1,-1,-3,-1,0,-2,-1,0,-2,-1,1,-1,2,1,1,1,2,
0,1,0,0,2,0,0,3,0,3,2,2,0,4,1,1,5,1,-1,2,
-1,2,1,3,0,1,2,2,4,3,2,1,2,-1,-1,0,2,1,3,1,
0,-1,1,2,-2,0,-4,-2,-1,-2,0,-1,-1,-3,-1,-2,0,-2,0,0,
-2,0,1,-2,-2,0,-3,-2,-4,-3,-4,-3,-2,0,-4,-2,-2,-3,-2,-2,
-2,-3,-3,-2,-2,-2,-1,0,-1,-2,-2,-2,-1,1,0,-3,-2,-2,-1,-2,
1,1,-2,0,-2,-5,-3,-2,-4,-4,-3,-6,-5,-5,-4,-5,-5,-5,-3,-5,
-2,-1,-3,0,-1,-2,-1,0,-1,-1,-2,-1,-4,-5,-5,-4,-2,-4,-4,-2,
-4,0,-4,-3,-3,-1,-3,-3,-2,-2,-3,-1,-2,-3,-3,-1,-5,-2,-4,-5,
-6,-2,-6,-5,-3,-5,-4,-2,-6,-8,-7,-4,-5,-5,-3,-6,-4,-7,-5,-4,
1,-4,-3,-4,-3,-4,-5,-2,-4,-2,-3,-3,-3,-4,-3,-5,-3,-2,-6,-5,
-3,-6,-1,-3,-2,0,-4,-4,-3,-2,-3,0,1,-1,-1,-1,-2,-1,-2,-1,
-2,-1,-4,-3,-4,-3,-4,-3,-6,-3,-2,-6,-3,-3,-5,-3,-4,-4,-3,-3,
-4,-2,-3,-3,-2,-3,-2,-2,-3,-2,-2,-3,-4,0,-2,-2,-3,-2,-3,-2,
-2,-3,-1,-2,-1,-3,0,-3,0,0,-1,2,1,1,2,0,4,-1,2,0,
4,1,1,1,-1,-2,-1,-3,1,-3,0,-1,1,1,-2,0,2,1,3,2,
2,1,4,1,2,1,3,0,1,1,2,0,0,-2,0,-3,-1,-2,-1,0,
0,-2,-2,-1,-3,-1,-4,-1,-1,0,0,1,0,-1,-2,-3,-3,-3,-1,-4,
-4,-4,-3,-2,-3,-3,0,-3,-4,-1,1,-1,1,1,0,2,0,-2,0,0,
2,-1,-1,-2,0,2,2,1,0,1,-2,0,1,2,0,0,2,0,-2,1,
1,1,0,0,-1,-4,1,0,0,0,2,-1,-2,-1,-1,-1,-3,-1,-2,-3,
-3,-1,-6,-5,-3,-5,-3,-3,-3,-4,-3,-4,-4,-3,-2,-1,-4,-2,1,-2,
-1,-1,-2,-3,-4,-4,-5,-3,-6,-8,-3,-7,-4,-7,-7,-7,-5,-4,-6,-6,
-4,-3,-5,-5,-6,-4,-3,-7,-5,-3,-5,-4,-5,-6,-7,-3,-6,-6,-5,-6,
-7,-8,-7,-7,-6,-6,-7,-7,-9,-5,-5,-4,-2,-5,-5,-4,-4,-2,-3,-2,
-2,-3,-1,-3,-1,-1,0,-1,0,-2,-1,-3,-2,-3,-3,0,-1,0,1,-1,
0,0,0,3,1,-1,2,3,1,1,2,3,4,2,4,2,2,2,1,1,
1,1,2,1,1,5,4,3,1,4,3,2,5,1,2,2,2,3,3,3,
2,4,3,3,1,6,5,5,6,3,7,5,4,3,5,1,3,2,2,1,
3,0,3,3,2,2,4,3,3,4,3,4,3,3,3,2,0,1,1,2,
0,2,1,1,2,2,0,0,-1,0,2,-1,0,2,1,-1,2,-1,2,-2,
-1,2,-2,1,-1,1,0,1,3,0,0,1,0,1,-2,-1,-2,-2,-5,-1,
-1,-4,-2,-4,-1,-4,-2,-2,-3,-3,-3,-4,-4,-1,0,-1,-2,-2,-2,-5,
-2,-2,-3,-4,-3,-3,-5,-3,-3,-5,-3,-6,-6,-6,-5,-3,-6,-6,-6,-3,
-3,-1,-1,-2,0,-2,-1,-2,-1,-1,0,0,0,0,-1,-1,-1,-2,-2,-1,
-1,-4,0,-2,-4,-3,-2,-5,-1,-1,0,2,-1,-1,-1,0,-1,-2,-2,-3,
-1,2,0,1,2,2,1,1,2,1,1,1,2,3,1,2,1,2,-2,0,
-1,-1,0,2,0,-1,1,2,-1,1,1,1,2,1,2,1,2,2,0,2,
1,3,0,1,-1,0,-2,0,-1,-2,-3,0,-1,0,2,3,1,1,0,1,
1,-1,1,-2,-2,-1,-1,-3,-2,-1,0,0,-1,0,-1,-2,1,-3,-1,1,
-1,-2,3,2,1,1,3,2,6,2,3,6,2,2,2,3,0,-1,2,0,
-2,-2,0,1,-1,1,2,0,0,1,1,3,4,2,4,3,3,4,3,2,
2,4,1,2,1,3,2,4,3,3,2,3,3,5,3,3,5,6,4,4,
3,5,3,5,4,5,5,5,4,6,5,4,5,6,5,6,5,4,3,3,
4,5,5,4,5,4,1,1,3,3,3,5,3,2,3,4,3,4,4,5,
4,3,4,5,6,3,6,3,2,2,3,2,2,0,2,2,3,1,2,1,
3,3,3,2,3,4,6,5,3,5,2,5,3,3,4,3,3,2,0,2,
4,2,2,2,1,2,1,4,4,3,3,4,3,2,2,3,1,3,4,5,
2,5,1,2,5,3,5,4,3,4,6,4,3,4,4,4,7,7,6,5,
5,6,6,7,5,8,7,9,6,5,5,4,2,4,4,2,2,4,0,1,
4,3,3,5,2,6,3,3,2,3,3,3,5,4,3,2,3,3,3,3,
2,3,5,5,2,4,3,3,1,2,3,0,3,2,3,5,3,2,1,2,
3,3,3,3,5,3,3,2,2,5,1,3,3,2,2,2,3,-1,3,2,
3,2,0,0,1,-2,-2,-2,-1,2,-1,0,1,0,0,2,1,3,2,-1,
1,0,1,2,3,0,2,2,-2,-1,-1,-1,0,0,-2,1,0,0,1,1,
4,4,5,2,2,3,3,2,2,4,0,2,0,1,4,3,2,2,4,5,
5,6,7,2,3,6,7,7,5,3,2,6,5,5,8,6,6,6,8,4,
9,7,6,5,4,4,4,4,1,3,6,2,4,4,3,1,5,3,5,3,
2,2,3,4,3,4,0,4,4,4,6,7,2,5,2,4,1,4,3,2,
3,4,1,-1,2,-1,1,1,2,0,1,2,2,1,1,4,0,1,0,1,
1,0,0,3,3,2,1,3,3,2,1,2,-1,1,1,1,0,1,0,0,
-1,2,0,0,1,3,1,3,2,5,4,3,3,5,5,3,1,3,1,3,
0,0,0,-2,0,0,0,2,4,4,2,0,3,2,4,4,6,6,4,8,
3,6,3,7,6,5,4,6,3,3,6,3,4,3,2,2,2,3,4,2,
2,3,2,5,3,5,3,6,5,4,5,4,7,4,5,5,5,5,2,4,
2,2,3,3,2,4,4,4,5,3,6,6,5,4,6,5,3,3,7,4,
3,3,5,2,5,2,3,4,4,7,2,6,4,4,5,5,6,5,6,5,
4,5,4,5,5,3,5,5,5,3,6,2,4,6,3,2,3,5,1,3,
4,0,2,4,-1,1,1,0,1,3,1,1,0,1,2,1,-2,1,-1,2,
3,1,2,0,3,-1,2,1,0,0,0,0,4,5,4,1,3,0,3,3,
2,2,2,5,3,7,7,4,6,6,4,5,7,6,6,6,6,5,7,5,
3,2,5,2,2,5,3,1,2,3,2,1,2,2,2,1,3,1,1,2,
0,-2,1,1,0,0,2,4,0,0,1,-1,2,3,0,0,2,1,0,1,
0,0,1,1,2,2,2,2,3,1,2,0,2,1,2,-2,2,0,1,0,
0,1,0,0,-3,-1,-1,0,-2,-1,0,1,1,0,2,1,0,-1,-1,-2,
0,1,1,-1,1,-2,-2,-1,-2,-3,0,-1,1,-1,4,0,1,1,3,-1,
0,0,1,-1,2,0,2,1,3,2,2,0,3,0,0,2,0,1,1,-2,
2,1,0,2,1,0,1,0,0,1,2,1,0,-1,-1,1,0,2,2,1,
2,3,0,1,2,2,3,5,1,2,4,2,2,0,3,0,0,1,0,1,
3,0,-1,1,1,2,-1,2,1,0,1,1,0,1,1,-1,0,3,-3,-1,
2,-2,-2,-2,-3,-3,-2,-4,-1,-1,-1,-2,3,-1,2,0,-1,0,1,2,
-2,1,1,1,0,-1,-3,0,0,-2,0,-2,-2,-2,-1,0,-1,-1,0,-2,
-1,0,0,-1,0,-2,-1,-1,1,0,2,-1,1,1,1,1,2,2,1,4,
4,4,2,4,3,3,1,3,0,3,2,3,1,2,1,4,2,5,3,4,
4,5,5,3,3,4,5,6,6,4,7,3,4,3,3,6,2,1,4,1,
1,3,3,3,3,2,2,1,0,2,-1,3,0,2,4,2,3,1,3,0,
2,1,1,1,-1,1,1,-1,0,1,0,0,0,1,1,4,3,1,5,5,
4,2,4,5,1,4,2,3,3,2,4,0,2,2,1,1,3,2,2,2,
5,2,3,2,4,6,1,3,3,0,3,5,5,4,4,2,2,4,3,5,
4,3,5,5,5,3,4,4,4,5,3,2,2,4,3,1,3,2,3,2,
3,2,5,3,5,5,2,4,2,3,5,4,1,2,0,0,2,0,2,2,
2,1,1,2,3,1,3,2,3,3,3,2,2,2,3,2,2,5,1,2,
5,3,2,3,3,1,3,4,4,7,5,4,5,4,5,5,8,6,5,4,
6,4,5,5,7,5,6,8,7,6,10,5,7,7,8,6,6,8,5,7,
4,6,5,5,4,5,5,4,6,6,7,7,8,10,9,5,8,9,7,5,
8,6,4,6,4,4,4,3,4,2,4,5,4,4,5,7,6,6,9,7,
6,6,7,3,4,5,5,5,4,4,4,3,5,4,3,4,2,3,4,1,
2,3,1,2,0,1,0,1,3,1,1,4,5,4,2,3,4,2,1,4,
1,2,4,3,3,1,4,2,0,1,2,1,4,5,4,3,4,6,5,6,
8,8,9,9,6,8,6,5,6,7,4,5,4,5,3,5,2,1,4,1,
2,0,1,2,1,-2,-1,-3,-2,-3,-2,-2,-1,0,-1,-3,0,-1,-2,-2,
-2,-3,-1,-1,-3,-2,-2,-7,-2,-1,-5,-4,-2,-6,-6,-4,-6,-5,-5,-4,
-3,-4,-7,-6,-6,-7,-5,-7,-4,-2,-4,-3,-5,-1,-5,-6,-6,-4,-5,-6,
-5,-5,-7,-7,-7,-5,-7,-3,-6,-6,-3,-3,0,-2,-1,-3,-1,-2,-2,-1,
-2,-2,-1,0,-1,-2,-1,1,-1,0,-3,1,1,0,-1,-1,-1,-1,-1,0,
2,-1,-2,3,0,1,3,3,-1,2,4,0,5,6,5,3,5,3,4,5,
4,5,6,3,3,5,3,2,3,3,3,3,3,5,4,6,8,4,5,6,
3,5,7,5,5,6,5,6,8,5,4,8,5,1,6,5,5,6,6,7,
6,7,8,7,8,8,7,5,9,9,9,6,7,9,6,8,8,8,7,8,
9,8,7,7,8,6,6,7,7,8,8,7,7,9,8,7,9,8,8,9,
9,7,5,7,6,7,8,5,4,4,5,6,4,5,4,6,3,2,3,4,
5,3,5,6,6,4,8,5,5,7,5,5,9,7,5,7,7,3,8,6,
4,5,5,5,4,7,5,5,8,6,6,4,7,4,6,6,5,5,4,6,
2,4,4,5,7,8,6,7,8,6,5,6,7,1,5,5,4,2,3,3,
2,3,5,2,3,5,2,6,4,2,7,3,5,4,4,4,3,4,2,3,
5,4,3,3,4,2,2,2,2,0,2,3,4,2,4,3,4,2,3,3,
3,1,0,3,1,3,3,1,2,1,2,0,2,1,2,0,1,1,0,-1,
-3,-2,-2,-6,-4,-2,-3,-2,-2,-4,-2,-3,-1,-4,-3,-2,1,-2,-2,1,
0,-1,-2,-1,0,-3,-4,-2,-2,-6,-2,-6,-5,-2,-4,-5,-7,-5,-3,-5,
-5,-6,-3,-6,-4,-5,-6,-6,-5,-7,-2,-6,-7,-5,-1,-5,-5,-4,-3,-4,
-4,-4,-4,-3,-3,-3,-1,-1,-3,-3,-1,-1,-2,-3,0,-2,-1,-2,1,0,
-3,0,0,1,2,1,2,1,1,2,1,3,1,5,2,3,4,5,5,4,
5,4,3,4,2,5,4,5,5,3,3,2,4,6,4,5,3,3,5,4,
4,4,4,3,4,4,7,4,5,7,5,5,8,7,8,7,5,8,6,8,
8,9,8,9,8,7,6,10,7,8,7,6,6,9,9,8,7,6,5,7,
7,5,7,6,6,8,8,7,8,7,7,5,6,6,7,7,6,7,7,5,
7,6,4,6,7,7,6,8,6,6,8,6,7,6,6,5,8,2,4,3,
4,5,4,2,2,3,0,2,3,3,2,3,3,3,2,2,4,5,5,3,
3,4,3,3,3,6,3,1,3,1,3,0,2,1,2,3,3,2,3,3,
5,2,4,3,4,2,3,3,4,3,5,4,3,6,6,3,4,0,4,4,
2,4,4,2,4,1,2,4,3,2,3,3,4,5,4,6,5,5,5,4,
5,5,6,5,3,5,2,3,6,4,2,3,3,5,1,2,3,2,5,3,
3,5,4,4,4,5,3,2,4,6,4,5,4,3,1,2,2,3,4,4,
1,3,3,3,3,1,3,0,3,1,2,3,2,0,2,2,1,1,3,4,
2,4,5,4,2,3,2,2,1,2,3,4,2,2,3,1,3,5,4,1,
0,3,3,3,1,2,2,6,4,6,6,5,5,7,4,6,8,5,6,6,
5,8,6,8,5,9,7,8,7,7,8,7,7,8,7,7,8,8,6,5,
7,6,9,6,6,8,6,4,7,8,7,8,8,6,7,7,4,4,6,2,
3,1,3,2,3,2,4,3,1,3,1,6,2,5,3,6,4,0,2,3,
3,3,3,3,1,3,2,1,1,2,3,2,1,0,0,0,0,-1,2,0,
0,2,1,-2,0,-1,-2,1,2,-1,0,2,1,2,2,2,1,2,2,4,
2,5,2,3,2,2,0,1,2,4,1,3,4,3,3,5,6,4,4,4,
3,5,5,4,2,3,4,4,1,4,2,1,3,1,3,0,0,1,1,2,
2,2,4,2,3,2,4,1,3,3,2,4,6,3,1,2,2,3,2,0,
0,2,1,1,3,-1,2,0,2,-1,0,1,0,0,2,3,1,-1,2,2,
2,0,-1,0,0,0,1,-1,2,-1,-1,0,-1,1,-1,0,3,3,0,3,
3,3,4,4,3,4,2,5,2,5,2,3,4,3,2,5,3,1,3,1,
4,3,3,2,3,2,1,4,3,4,4,3,6,2,5,4,4,4,4,4,
4,4,2,3,4,3,3,2,4,1,5,2,3,1,4,4,4,3,2,4,
4,3,3,3,3,5,3,3,4,1,3,5,5,4,8,4,7,7,7,5,
9,6,6,6,4,3,6,5,6,5,4,5,4,5,7,8,7,9,8,9,
8,8,7,6,9,7,5,6,8,6,5,6,5,6,4,6,5,6,7,6,
7,5,4,5,7,5,8,5,6,7,6,5,4,3,4,2,4,1,3,4,
3,3,2,2,1,1,1,3,0,1,1,0,2,2,2,0,-2,-2,0,0,
0,-1,0,0,1,0,1,1,0,1,0,2,1,2,1,-2,1,2,-1,-1,
3,1,1,3,-1,0,-2,1,1,1,1,3,1,2,3,3,3,2,1,2,
0,0,-3,0,4,2,2,3,3,5,3,4,2,6,3,3,3,4,3,2,
2,4,3,1,3,2,1,0,2,1,2,2,0,3,3,2,1,2,-1,4,
3,1,1,3,-1,3,1,3,3,3,3,3,1,4,1,5,3,2,4,1,
1,3,0,3,3,4,1,3,6,4,6,6,7,4,6,5,4,5,6,3,
5,4,2,4,3,5,2,4,4,3,5,3,4,4,4,4,4,4,4,4,
4,6,2,2,2,1,2,-1,2,0,-1,-2,-2,-2,-1,0,-1,0,0,-2,
0,-1,-3,-1,-2,-1,-3,-2,-3,-3,-3,-3,-4,-2,-5,-5,-3,-5,-5,-1,
-2,-5,-2,-4,-4,-2,-3,-5,-4,-3,-5,-6,-5,-4,-5,-5,-4,-4,-6,-5,
-7,-7,-6,-5,-4,-4,-3,-6,-6,-5,-6,-6,-4,-7,-4,-5,-5,-6,-4,-4,
-5,-5,-5,-5,-4,-5,-3,-3,-5,-4,-5,-7,-7,-7,-5,-5,-3,-7,-4,-8,
-6,-6,-8,-7,-6,-6,-5,-6,-4,-5,-4,-6,-5,-6,-6,-5,-7,-6,-5,-4,
-2,-3,-3,-4,-3,-4,-1,-2,-2,-3,-3,-2,-5,-2,0,-1,-4,-1,-2,-2,
-2,1,-1,0,-2,0,-1,0,-1,-2,-1,-2,-2,0,-2,-1,1,1,-2,-1,
-1,-2,0,0,0,2,1,0,0,2,-2,0,0,1,-2,-1,0,-1,-2,0,
0,-2,0,2,1,2,3,0,-1,2,-1,1,-1,1,-3,2,-1,1,0,-1,
-2,0,-1,-2,-1,-1,-3,1,0,0,-1,1,-1,-1,1,0,-2,0,-1,1,
0,4,3,0,2,3,3,3,2,3,2,3,0,3,3,1,3,0,3,0,
4,-2,1,0,-1,1,1,-1,3,-1,4,2,0,4,1,3,-1,4,-1,1,
2,3,1,0,1,0,0,0,1,-1,2,-1,-4,-1,-1,-2,0,-1,1,-2,
0,3,0,2,1,2,-1,0,1,-1,-1,1,-2,-2,-1,-1,-2,-3,1,0,
-1,0,0,1,2,3,2,0,1,3,2,3,3,3,3,4,2,1,1,2,
0,3,0,1,4,2,1,0,-2,-1,-2,0,-2,-1,-2,0,-1,-1,1,-1,
0,0,1,0,0,1,1,0,2,-2,0,1,-2,0,-1,-1,-3,-2,-2,0,
-2,-3,0,-1,-3,-1,1,-1,-1,-1,-2,-2,0,2,-1,3,1,-1,1,1,
0,-1,-1,-2,-3,-1,1,0,-2,-1,2,2,-2,0,-2,-1,-1,1,1,1,
0,3,1,3,3,1,2,4,3,2,3,5,3,2,2,3,5,2,0,3,
4,0,3,1,1,2,4,3,3,5,3,5,4,5,7,6,4,5,6,5,
5,4,2,6,4,5,5,3,2,5,6,8,5,8,5,5,5,5,6,5,
4,6,6,5,4,4,5,4,4,4,2,4,3,5,4,5,5,4,6,7,
5,3,5,4,4,4,2,3,4,4,0,1,-1,1,1,2,0,0,1,1,
0,2,0,1,0,3,0,2,2,0,1,1,0,0,2,2,1,1,3,1,
-2,4,3,1,5,4,2,3,3,2,5,3,1,1,1,1,-1,0,1,-1,
0,-1,1,0,0,1,2,1,0,1,2,1,2,-1,3,1,1,0,-1,-1,
-1,-1,1,1,2,-2,1,-3,-1,-1,-1,-2,-1,-1,-2,-1,2,1,2,1,
-1,3,0,1,0,2,0,0,2,1,1,3,3,2,4,4,4,4,3,5,
4,6,3,5,6,6,6,5,7,7,7,5,6,7,8,5,8,6,8,9,
8,8,6,7,8,7,7,7,10,8,8,8,8,9,8,8,7,7,8,8,
5,8,7,7,7,5,7,6,6,8,6,7,5,6,5,8,5,5,6,6,
4,4,3,7,4,6,4,5,4,3,5,6,3,6,4,8,7,5,9,7,
8,7,7,9,8,8,9,7,8,7,7,6,5,3,6,4,9,5,6,3,
7,7,6,6,7,8,7,8,8,8,6,10,7,7,7,7,9,9,7,7,
11,9,10,7,8,9,10,10,11,10,13,11,12,11,10,9,9,14,11,12,
15,13,15,13,14,13,13,8,10,11,11,13,14,12,13,13,14,11,13,14,
13,11,12,14,12,13,15,13,13,13,13,14,13,12,13,13,15,12,12,14,
10,10,10,8,10,7,7,9,8,8,9,7,6,8,10,7,8,8,11,7,
9,9,7,10,9,11,11,10,9,10,7,7,10,8,7,8,7,8,7,6,
8,8,7,8,8,9,7,9,11,7,7,8,7,6,9,7,7,7,6,5,
4,3,5,4,5,6,6,7,10,8,7,6,9,8,8,10,6,7,7,7,
7,9,7,7,5,7,8,7,8,8,7,7,8,9,6,9,6,6,8,6,
7,5,7,6,6,8,9,10,8,6,8,9,5,8,9,7,9,7,7,7,
8,6,3,5,6,4,4,5,6,6,8,6,4,7,6,6,8,8,8,7,
8,8,5,10,7,8,8,10,6,7,6,6,7,6,3,4,7,7,5,7,
6,7,9,8,8,7,6,7,8,7,8,7,6,7,5,6,8,7,9,4,
6,5,5,8,5,3,2,3,3,5,2,5,2,2,1,4,2,3,3,2,
3,3,5,3,4,4,4,3,3,5,5,4,4,4,5,4,6,4,3,3,
4,3,2,4,3,3,3,4,4,3,2,3,2,2,4,4,5,4,4,5,
4,2,5,3,3,2,3,5,4,6,5,4,1,6,2,5,2,5,5,6,
7,6,7,4,8,6,7,8,7,8,10,9,7,7,8,5,8,8,6,6,
5,6,4,7,6,6,7,9,5,6,8,6,9,8,8,9,6,8,9,9,
6,6,6,5,7,6,8,5,6,7,4,5,7,5,7,6,5,5,5,3,
6,7,6,7,3,5,6,4,7,5,4,4,6,5,4,6,6,7,6,5,
6,5,4,6,6,6,7,7,9,6,9,8,7,6,5,5,6,4,5,4,
5,6,8,6,7,7,7,6,6,5,6,6,6,3,7,5,5,2,5,6,
3,7,5,5,7,4,5,5,5,5,5,4,6,6,4,6,3,1,2,4,
1,2,3,2,3,0,4,5,3,3,4,2,0,3,3,3,3,3,3,0,
2,0,3,0,0,3,1,0,0,0,-2,0,0,0,-1,-2,1,-2,3,0,
1,2,-1,1,1,-2,0,0,1,2,2,-1,2,2,0,0,2,1,1,4,
-1,3,2,2,3,2,0,2,3,0,2,2,4,1,2,2,0,2,-2,1,
0,1,0,1,0,1,3,2,2,0,1,1,0,1,0,0,2,0,1,0,
1,2,0,1,2,3,0,2,1,1,1,3,2,4,3,5,1,2,4,4,
2,4,3,1,1,3,3,4,2,0,2,1,1,4,2,1,3,4,2,4,
3,0,2,3,2,2,1,0,1,3,2,3,2,3,2,1,2,1,3,4,
4,6,5,3,4,4,3,2,2,2,1,2,2,2,1,1,3,1,1,3,
1,1,2,3,2,4,3,2,1,4,2,1,1,2,1,0,2,2,0,0,
1,0,-1,1,1,2,2,0,0,1,-1,2,-2,0,0,0,0,0,2,0,
2,2,-1,-1,-1,0,-1,1,1,2,1,-3,1,-1,0,-2,-1,-2,-1,-2,
-2,-1,0,-1,1,0,1,0,-1,3,0,2,1,-1,0,2,1,-1,-1,-3,
-1,0,-2,0,-5,-2,0,-3,-5,-2,-3,-1,-3,0,-1,-2,-1,-3,-2,0,
-2,0,-1,-2,-2,-1,-2,-3,-2,0,-3,-3,-1,-1,-2,-1,-4,-2,-1,-4,
-1,-1,-3,-2,-1,-1,0,-2,1,-2,1,1,1,3,2,2,2,2,3,1,
2,3,0,2,-1,-1,1,2,1,2,1,0,1,3,0,1,2,0,0,3,
2,1,-1,3,0,2,2,0,4,3,1,3,0,4,3,1,4,1,2,2,
0,1,4,0,3,1,-1,3,3,-2,3,1,0,0,1,0,0,1,2,2,
3,2,3,3,-1,3,0,2,-1,0,0,0,0,1,1,1,0,2,2,-2,
2,1,5,2,5,4,2,1,2,1,2,0,1,-3,1,1,0,-1,2,-2,
1,1,2,3,1,-2,3,-1,1,0,0,3,1,-1,1,2,3,3,2,0,
2,1,2,0,1,0,4,2,2,3,3,1,1,-2,0,1,0,0,1,1,
6,2,4,3,5,6,4,7,5,4,7,3,5,6,5,6,5,4,7,5,
6,7,8,6,4,6,5,6,7,5,7,9,7,8,10,7,5,8,6,8,
5,7,8,7,8,8,6,6,7,7,4,6,4,4,6,5,4,2,4,4,
5,5,5,7,5,7,5,6,4,7,7,6,7,8,8,5,7,9,8,7,
9,6,6,6,5,5,3,5,2,0,5,1,3,2,4,-1,3,4,4,1,
1,1,0,0,-1,0,-1,-2,-3,-2,-1,-1,-2,-1,-4,0,-2,-4,-1,-1,
-3,0,0,-1,-1,-1,0,-3,-3,-1,-1,-3,-1,1,-1,0,1,1,1,1,
2,0,3,5,4,5,4,4,1,2,0,0,0,0,3,1,3,3,0,2,
1,3,2,4,2,4,5,5,3,3,3,3,4,2,5,3,2,5,3,3,
4,3,6,6,6,5,3,7,5,7,7,4,5,6,6,5,6,3,7,7,
5,6,5,5,5,5,7,5,6,7,5,6,7,5,4,3,4,6,2,3,
4,4,4,4,4,4,4,1,5,3,4,5,3,6,4,4,5,1,4,2,
4,2,2,6,5,4,5,4,4,6,4,4,6,3,3,4,5,4,5,2,
5,3,0,1,4,3,2,3,6,3,5,4,6,5,4,4,3,4,4,3,
5,4,2,6,2,4,5,5,5,5,3,5,6,6,3,5,2,5,4,4,
6,2,3,6,5,8,4,4,4,5,4,2,3,4,1,2,4,3,4,4,
2,1,2,2,2,3,2,2,1,-1,1,0,0,-1,-1,3,0,2,3,3,
2,1,3,3,-2,3,0,1,1,0,0,3,0,1,1,2,0,2,0,2,
-1,2,-1,0,1,0,0,4,2,0,2,0,0,-1,-1,-1,0,0,0,-1,
-2,-2,-1,-3,-1,-1,-2,-4,-3,0,-3,0,-2,0,1,-2,0,0,-3,0,
0,-1,-2,0,-1,-3,-2,1,-2,-1,1,-1,3,0,1,1,4,2,0,2,
1,0,2,2,1,1,2,1,0,3,3,2,3,3,1,2,6,2,4,2,
5,3,4,4,3,3,3,2,1,3,2,2,2,4,3,2,1,3,2,1,
3,3,3,5,2,5,2,4,4,4,2,4,2,5,3,2,2,2,2,3,
1,3,5,2,5,1,6,5,6,5,5,5,3,4,3,5,4,4,4,3,
3,3,5,4,5,4,3,3,4,6,6,4,4,7,5,4,5,7,7,9,
4,6,5,7,4,5,7,5,8,4,5,5,7,7,5,6,4,7,7,7,
7,7,5,7,5,7,7,6,7,7,5,7,4,6,6,5,6,4,6,4,
6,6,6,5,5,5,6,7,6,4,7,7,6,4,8,4,6,3,5,5,
3,3,6,5,3,5,4,4,4,5,3,3,1,2,3,4,1,4,3,1,
3,1,1,1,3,0,0,1,1,2,2,2,2,1,0,0,3,-1,3,-1,
1,0,-2,0,0,-3,-1,1,-1,-2,-4,-2,-1,-1,0,0,-1,-2,-3,0,
-2,0,0,1,0,-2,-2,-1,-3,-4,-5,-2,-3,-5,-3,-3,-5,-4,-5,-3,
-1,-4,-2,-2,-1,-2,-4,-1,-1,-3,-2,-2,-1,1,-3,-1,-2,-2,-2,-1,
-1,0,0,-2,0,-1,0,-1,-3,0,1,-1,-1,-1,-1,-3,-2,-2,2,2,
1,2,3,0,1,2,2,0,1,1,2,0,1,1,1,-3,1,0,0,-1,
1,0,-3,-1,-1,1,1,5,2,2,1,4,4,2,4,5,1,3,2,2,
1,2,5,1,3,3,1,3,3,3,3,0,3,2,3,2,3,3,0,3,
3,2,2,3,3,1,1,4,2,3,4,3,1,2,2,4,2,4,4,3,
4,2,3,3,0,1,0,4,2,2,6,2,4,3,3,3,3,4,3,3,
2,4,3,2,1,1,1,2,0,0,0,2,0,3,2,3,1,2,1,3,
0,1,-2,2,0,1,-1,0,1,1,-2,0,0,1,0,0,1,0,-1,-1,
0,1,0,-2,1,-2,2,-2,-1,0,-2,-1,1,-2,-2,1,-2,-3,-2,0,
-1,0,1,-2,-1,0,-3,-3,-3,-5,-5,-5,-5,-5,-3,-4,-4,-5,-2,-5,
-3,-5,-4,-4,-5,-4,-6,-4,-4,-3,-1,-1,-3,-3,-4,-3,-7,-2,-3,-4,
-3,-5,-3,-3,-6,-4,-5,-4,-5,-3,-3,-4,-4,-1,-3,-5,-3,-6,-2,-4,
-2,-3,-1,-3,-3,-2,-6,-4,-3,-4,-5,-3,-5,-3,-3,-5,-3,-3,-3,-2,
-3,-3,0,-3,-2,-1,-1,-1,-1,1,-1,-2,1,-1,0,1,0,1,2,-2,
1,0,3,-1,2,-1,-2,0,0,1,0,3,1,3,2,6,4,4,5,6,
6,6,7,5,5,2,4,5,6,6,7,3,4,6,6,5,5,8,6,7,
8,6,8,7,7,7,6,8,6,7,6,8,7,7,7,8,8,7,7,6,
10,8,8,9,9,9,8,11,10,12,9,8,10,7,10,8,10,8,8,10,
10,12,11,9,11,9,10,11,11,8,11,8,9,9,10,9,10,11,9,12,
9,11,11,9,8,10,11,11,12,14,12,11,13,10,13,13,13,12,12,14,
12,14,14,13,11,12,9,11,10,10,8,11,10,10,9,11,11,11,10,10,
8,9,12,7,7,7,6,7,7,4,4,6,7,6,6,4,6,5,4,3,
4,4,3,3,2,4,6,2,3,5,3,5,4,2,4,4,4,5,4,1,
4,4,2,3,5,3,0,2,4,3,2,4,3,5,3,3,6,3,2,6,
4,3,4,3,5,3,4,6,4,3,2,5,5,6,6,7,3,5,5,8,
6,4,5,7,5,7,5,6,5,4,4,4,5,5,7,4,5,5,3,3,
4,3,5,6,5,3,5,4,2,5,5,3,3,3,5,4,2,3,4,3,
4,4,2,3,3,1,4,3,3,2,1,2,3,-1,1,2,1,4,1,0,
1,2,0,1,1,1,0,2,1,5,1,4,3,4,2,4,3,4,3,4,
4,3,4,5,6,4,5,7,3,3,6,5,5,6,5,8,4,6,8,5,
4,6,4,6,8,7,6,5,6,4,5,7,6,4,5,6,5,6,5,5,
5,6,4,7,6,5,7,6,7,5,7,6,6,5,6,3,4,5,6,5,
6,4,4,5,4,5,5,3,2,4,4,2,2,4,3,5,4,3,2,4,
1,3,2,6,5,3,3,4,3,3,3,1,1,0,0,1,1,3,2,0,
4,2,3,4,1,1,2,2,0,2,3,6,3,3,5,3,1,3,2,1,
2,4,4,2,2,1,4,4,2,2,2,2,2,2,0,1,0,1,3,3,
3,1,1,2,1,1,0,-1,2,-2,0,-1,-1,0,-1,-1,0,-1,0,0,
1,-2,2,0,0,-1,0,0,-2,1,-3,-2,-4,0,-1,-3,-3,-6,-4,-5,
-3,-6,-8,-4,-6,-5,-5,-6,-6,-5,-6,-6,-4,-2,-4,-4,-2,-4,-3,-6,
-2,-5,-3,-5,-3,-2,-5,-5,-3,-4,-2,-5,-7,-5,-5,-5,-3,-4,-3,-4,
-3,-3,-4,-4,-4,-5,-5,-5,-5,-5,-5,-2,-3,-6,-2,-1,-3,1,-1,-2,
-3,-2,-2,-1,-2,-1,-2,-3,-3,-3,-2,-2,-3,-2,-4,0,-3,-2,-2,-3,
-2,-4,-1,-4,-3,-2,-5,-3,-3,-3,-2,-3,-4,-3,-1,0,-3,-1,-2,-1,
0,1,-2,0,0,-1,0,-1,-2,-3,1,-1,0,0,1,2,1,3,2,4,
1,4,1,3,2,3,5,3,2,4,5,2,5,3,5,3,4,5,3,7,
6,6,3,5,4,4,3,3,4,3,8,7,5,4,6,6,2,4,3,3,
3,3,4,2,3,2,3,2,2,3,1,1,3,3,1,1,2,1,1,1,
1,2,-2,0,2,2,-1,1,-1,-1,-1,-2,0,-1,-2,-2,0,-2,1,-1,
-1,-2,-2,0,-1,1,-1,-1,-1,-1,-2,-1,-1,-2,0,-3,-4,-2,-4,-3,
-4,-3,-4,-5,-2,-4,-5,-1,-3,-1,-3,-2,-2,-1,-2,-1,-2,-3,-1,-4,
0,-3,-2,-1,-2,-2,-2,-3,-2,-4,-1,-3,-3,-1,-2,-2,-2,1,-1,-1,
1,0,-1,0,1,0,1,0,2,1,2,2,1,2,3,4,4,3,2,3,
6,3,6,3,6,5,6,5,5,5,7,6,4,3,7,6,6,6,6,6,
9,6,9,9,7,9,7,7,8,7,6,7,5,3,5,5,7,5,4,5,
7,5,6,8,7,6,6,8,8,9,7,8,7,4,4,4,4,4,4,5,
3,1,4,3,2,3,2,3,4,5,6,5,4,6,5,4,6,5,5,5,
6,5,5,4,5,4,5,7,5,5,6,5,6,6,5,7,6,3,3,5,
5,4,4,4,5,4,4,4,2,3,4,2,3,3,6,3,4,4,5,2,
2,1,1,2,1,3,2,2,2,1,1,1,4,2,2,4,2,3,2,4,
4,5,5,3,3,4,5,4,1,4,3,6,3,3,2,4,0,4,3,3,
4,3,1,2,4,5,3,3,4,4,2,4,3,3,3,1,2,0,0,4,
3,0,2,0,2,1,2,1,-1,0,0,1,2,1,2,3,2,3,4,4,
1,4,3,3,2,3,3,6,4,2,5,4,4,3,2,4,5,5,5,8,
4,5,5,6,6,5,4,6,7,7,7,7,6,7,6,4,5,7,3,7,
5,8,6,8,6,6,4,7,7,5,8,6,8,7,6,4,6,7,3,7,
7,5,5,6,7,4,5,6,5,5,6,5,5,8,6,4,8,6,7,7,
5,5,7,5,6,7,8,9,7,8,8,5,10,8,6,10,8,10,10,9,
10,8,10,9,7,6,6,10,9,8,7,7,6,7,7,9,6,7,7,7,
6,8,8,4,7,8,5,6,6,6,5,2,2,3,2,4,4,4,5,5,
4,4,4,2,5,2,2,6,5,4,2,4,5,2,6,2,4,3,3,2,
1,2,1,2,2,3,4,3,3,4,3,2,3,0,3,0,-3,2,-1,0,
2,1,2,1,3,1,4,4,2,3,5,4,4,4,1,1,3,4,2,4,
1,0,3,4,3,5,2,4,4,3,4,3,3,5,4,4,0,3,3,4,
3,4,4,3,4,4,4,5,4,4,2,5,6,2,4,1,4,5,5,5,
3,6,4,1,1,4,2,4,4,4,2,1,1,3,3,2,1,1,1,0,
2,2,1,0,2,2,1,3,1,4,2,4,4,5,5,3,6,4,3,4,
7,4,6,5,4,4,5,6,5,7,5,8,6,8,5,9,6,7,7,6,
6,4,6,4,5,4,7,4,4,2,3,3,3,3,4,2,5,2,4,3,
2,3,4,1,4,5,4,2,6,4,3,4,6,2,5,4,4,6,3,3,
4,6,3,2,2,1,2,3,3,1,2,3,2,1,4,3,2,4,3,4,
1,4,2,3,5,3,2,2,3,2,1,2,2,0,4,0,2,2,3,3,
1,3,3,5,4,4,3,4,5,3,4,4,3,3,3,2,5,5,4,4,
3,4,4,2,5,5,4,4,6,3,5,5,4,4,2,6,7,6,4,7,
5,3,5,9,6,7,5,9,5,6,8,5,5,3,4,3,2,3,4,3,
4,6,2,3,2,3,3,6,3,3,5,4,3,3,1,2,4,2,1,0,
1,-2,3,2,3,2,1,3,2,1,1,1,0,0,-2,-1,-2,1,1,1,
0,-1,-1,-2,-1,0,2,0,-3,-2,0,1,1,2,-3,0,1,-2,-1,1,
-1,2,-1,0,0,-1,1,1,1,0,-2,0,0,1,2,2,1,1,2,0,
1,2,0,1,1,2,0,1,1,-1,-1,2,4,3,4,2,2,3,3,4,
4,3,4,2,5,3,3,0,3,2,0,1,2,2,1,2,2,2,4,5,
4,5,4,4,9,5,8,5,5,4,5,7,5,5,3,6,6,6,4,4,
7,5,6,6,8,6,7,8,8,6,5,6,6,5,6,5,8,8,7,6,
5,9,6,5,5,6,6,9,7,8,7,8,8,8,8,9,6,9,8,6,
8,10,8,9,8,9,8,7,8,10,8,9,8,9,8,8,7,9,10,9,
9,8,10,9,8,8,9,8,8,10,8,8,8,6,7,7,6,8,10,6,
7,7,5,8,7,6,8,7,6,6,7,6,7,5,5,6,8,6,6,6,
9,7,9,7,10,8,9,9,8,9,8,4,8,6,7,3,7,5,3,6,
6,5,5,6,5,5,3,2,2,1,4,3,4,4,4,5,3,2,3,4,
2,3,3,1,0,2,2,2,1,1,-1,1,2,1,2,2,-1,0,2,2,
-1,1,1,-1,-1,2,0,1,0,-3,-1,1,1,2,1,1,0,3,1,4,
4,3,2,2,2,0,1,4,1,1,2,2,2,4,3,2,6,0,2,4,
3,3,2,5,5,3,4,5,4,2,6,6,5,4,5,5,6,4,6,3,
4,5,5,3,4,2,4,3,2,3,1,2,1,5,2,4,6,4,5,4,
2,2,3,4,1,2,1,2,1,2,0,1,0,1,2,2,1,1,1,1,
2,3,1,2,0,1,2,0,0,3,0,-1,1,2,1,1,-3,0,1,0,
1,-1,0,-2,1,1,-2,1,1,-3,0,-1,0,-2,1,-2,-2,-1,-2,-1,
1,0,1,1,0,1,1,2,-1,2,-2,0,2,0,-1,0,0,-2,0,-1,
0,0,0,0,2,-1,0,3,0,1,2,4,2,-1,1,2,-1,1,0,1,
2,4,2,2,3,2,3,4,3,4,4,5,4,3,5,6,6,5,6,7,
7,4,4,6,6,7,5,7,6,8,9,8,7,9,10,9,9,9,9,9,
6,10,9,9,12,8,11,9,11,11,11,12,10,11,9,9,11,10,9,9,
7,10,10,8,9,8,9,8,9,9,9,7,9,7,8,8,7,6,7,9,
8,6,10,7,7,7,8,7,7,6,7,6,5,6,5,5,5,7,7,4,
5,7,6,6,5,6,5,7,7,4,5,5,6,6,6,6,5,6,2,5,
7,5,5,6,7,5,7,6,8,4,5,5,4,3,4,4,5,7,6,3,
5,7,5,5,3,6,8,6,3,5,6,5,6,6,5,3,5,4,4,5,
6,8,5,5,5,5,2,4,4,4,5,6,5,4,1,3,5,2,0,4,
3,3,3,5,5,4,3,3,3,6,4,1,4,3,5,4,2,5,5,5,
4,3,5,2,3,6,4,4,4,5,4,2,5,0,5,3,1,4,3,5,
3,4,4,2,5,3,4,1,2,4,2,4,4,6,4,5,4,4,3,4,
1,3,2,3,4,5,3,4,3,2,6,6,4,5,6,3,4,5,3,4,
2,4,2,2,3,2,3,1,7,2,4,3,3,3,0,3,1,1,4,1,
0,0,3,0,1,3,1,1,2,3,1,1,0,3,4,4,2,2,4,2,
3,4,3,0,2,3,1,0,2,2,2,3,2,0,3,2,0,3,4,5,
4,4,5,4,4,5,3,4,3,3,2,3,3,3,3,1,3,2,2,4,
2,6,2,5,2,2,4,2,1,5,3,2,4,6,3,4,4,4,3,5,
2,1,3,4,3,3,4,1,0,2,2,0,0,3,2,3,2,2,3,3,
3,3,5,0,1,3,3,3,2,3,4,3,5,2,5,5,4,5,4,7,
4,5,5,6,5,4,4,6,5,3,5,3,4,2,3,6,5,3,6,3,
2,4,2,3,1,0,3,2,2,0,3,1,2,3,3,2,3,4,0,1,
1,0,1,2,0,0,-1,2,1,2,0,1,1,-2,-1,-3,0,-2,-3,-2,
-3,-2,-2,-4,-2,-4,-3,-3,-4,-5,-6,-4,-2,-1,-5,-2,-2,-4,-4,-3,
-6,-4,-3,-6,-5,-7,-5,-4,-3,-3,-3,-5,-3,-4,-5,-3,-2,-3,-6,-4,
-4,-4,-4,-4,-5,-3,-3,-5,-5,-5,-6,-8,-7,-5,-7,-7,-8,-7,-7,-8,
-9,-7,-8,-8,-10,-7,-7,-5,-6,-6,-7,-7,-5,-11,-7,-9,-8,-7,-8,-7,
-7,-7,-7,-7,-5,-4,-5,-4,-3,-3,-1,0,-4,0,1,-1,1,-1,0,1,
1,0,0,1,1,0,4,0,0,0,1,2,-1,0,-1,-1,-4,-1,1,1,
0,-1,0,0,1,0,-1,1,0,-1,1,-1,0,1,-1,0,1,0,2,-1,
-3,-1,-1,-2,-2,-3,0,2,0,1,1,-1,0,2,0,1,-1,-2,0,1,
1,2,0,4,1,0,2,0,0,1,-1,-1,-1,-2,-1,0,0,-3,-3,-4,
-1,-2,-1,-2,-2,-1,-3,-2,-4,0,-2,-1,-1,-1,-2,-1,-1,-3,-3,0,
-5,-1,-3,-5,-7,-4,-6,-7,-5,-6,-4,-5,-5,-5,-4,-4,-5,-4,-5,-4,
-6,-8,-6,-6,-5,-5,-4,-6,-4,-5,-5,-3,-5,-4,-4,-3,-4,-5,-4,-5,
-4,-4,-3,-2,-3,-2,-1,-1,-4,-2,-2,-2,-1,-1,-2,-1,-1,-2,-2,0,
-3,-2,0,1,1,0,-1,1,1,3,2,2,1,2,3,1,2,0,2,2,
0,0,3,4,2,2,3,1,2,1,1,1,2,0,-1,1,2,2,3,1,
3,1,3,4,1,1,4,0,2,2,3,2,3,3,1,4,3,4,1,4,
3,2,2,4,5,3,3,4,3,1,2,2,0,1,3,2,3,4,4,1,
4,2,3,5,5,2,5,3,3,4,2,4,2,4,1,2,2,0,0,0,
2,0,2,1,5,1,3,1,1,1,1,0,1,1,-1,-1,1,-3,-2,0,
-5,-1,-2,-4,-2,-1,-2,-3,-2,-2,-4,-4,-2,-2,-2,-1,-1,-2,-4,-2,
-5,-4,-2,-2,-3,-5,-3,-2,-3,-3,-4,-1,-2,-3,-2,1,-3,-1,-2,-1,
-1,-3,-1,-2,-4,0,-2,-3,-2,-3,-2,0,0,-2,-3,-1,1,0,2,0,
1,2,1,2,2,1,2,1,-1,1,1,2,1,0,3,0,1,3,1,1,
1,1,2,3,0,-2,2,0,2,2,2,1,0,2,2,2,2,3,2,1,
2,4,5,1,3,4,2,3,2,2,2,3,4,3,4,4,4,4,4,6,
4,6,6,3,8,6,4,6,5,4,4,6,4,4,6,3,6,4,5,6,
4,4,6,3,4,5,4,4,3,5,3,3,2,2,3,4,2,4,5,3,
4,7,3,3,4,1,2,4,1,1,2,1,1,0,0,1,1,2,1,4,
1,2,3,2,4,4,3,1,3,1,2,3,2,4,0,1,1,1,-1,1,
2,3,3,2,0,2,4,1,4,1,2,0,3,-1,-1,1,1,0,0,1,
-1,2,0,2,2,2,1,2,2,3,2,1,1,3,2,2,3,0,3,2,
4,0,1,1,4,1,3,2,1,2,3,3,4,2,4,2,3,2,1,2,
0,0,1,1,3,0,3,0,0,-1,1,0,1,0,1,-1,1,-1,-2,-1,
1,-1,0,0,-1,-2,-2,-2,0,1,-2,-1,1,0,-1,-1,2,-3,0,-1,
0,2,1,0,-1,0,1,-2,0,-3,0,1,-2,1,-1,2,1,0,0,-2,
-1,1,1,1,1,0,4,-1,1,0,2,0,2,3,2,-2,1,1,2,1,
3,0,2,2,1,2,2,0,-1,1,1,0,2,-1,1,2,0,0,0,1,
-1,0,0,-3,1,-1,-1,-2,1,0,1,0,-1,0,3,0,2,1,1,0,
0,1,2,0,2,0,1,1,1,1,2,3,2,1,2,1,-1,2,2,0,
0,3,1,2,3,2,2,1,3,1,3,0,2,1,1,4,3,1,4,2,
3,3,1,2,2,3,2,1,1,0,0,1,-1,3,-1,0,0,0,-1,1,
1,-2,0,1,0,1,0,2,3,2,0,0,2,1,2,1,1,-1,3,-1,
2,2,1,1,0,0,0,2,4,3,1,3,3,3,4,2,5,3,3,4,
2,3,3,2,1,2,2,1,1,3,2,3,0,-1,1,-1,2,2,0,3,
1,-1,0,-2,-1,0,-1,0,0,0,-1,1,2,2,0,1,0,2,2,0,
3,3,3,4,2,0,2,2,1,2,1,3,-1,0,2,-1,0,0,0,1,
1,3,1,0,2,-1,0,-1,-1,-2,-1,-4,-1,-4,-2,-3,-4,-3,-1,-2,
0,-3,-3,-2,-1,-2,-1,-3,-2,0,-2,1,0,1,-1,-1,-1,-1,0,0,
-1,-4,-1,-3,-3,-2,-1,-2,-4,-3,-1,0,1,-4,-1,-2,0,-1,2,1,
1,1,1,-1,-1,-2,1,0,3,1,5,1,1,5,4,5,5,4,3,3,
6,4,7,4,3,5,3,5,3,7,3,3,5,3,2,5,5,3,4,3,
4,4,0,2,2,0,3,2,1,1,2,1,1,0,-1,-1,0,2,1,0,
0,1,2,-1,2,0,-1,1,2,0,0,3,1,2,2,3,1,3,2,3,
3,1,2,5,3,2,4,1,3,4,2,2,3,1,4,4,2,3,0,0,
1,1,-2,-1,1,-3,-1,0,-4,-2,-3,-3,-2,-1,-1,-1,-1,-2,-2,1,
-3,0,-1,-3,-1,0,-3,-1,-2,-5,-3,-2,-4,-2,-4,-3,-1,-3,0,-1,
-2,-3,-1,-2,-4,-1,-3,-4,-1,-3,-5,-4,-3,-4,-2,-1,0,-1,1,1,
1,0,1,-2,-1,0,-2,0,-2,0,2,0,2,2,1,-1,3,1,0,1,
1,2,0,1,2,1,3,1,2,-1,3,1,1,3,4,2,2,1,3,4,
2,2,3,4,3,6,4,2,5,6,3,4,5,3,6,4,5,7,5,6,
4,6,3,4,7,5,7,6,6,6,5,5,5,6,6,9,10,7,9,7,
10,8,9,9,9,10,10,11,9,8,10,10,11,9,12,11,12,12,11,12,
9,10,9,12,13,9,12,11,10,10,10,12,10,11,9,11,9,9,9,9,
9,9,11,8,11,9,10,9,10,11,11,11,11,11,10,7,7,5,3,5,
4,6,6,5,7,4,4,5,4,4,6,6,5,7,7,6,5,7,6,4,
3,4,6,7,3,5,6,4,6,6,6,6,6,6,7,7,6,8,6,7,
9,7,8,7,8,6,5,9,7,8,5,7,7,6,6,6,3,5,3,5,
5,5,2,6,3,5,3,5,3,3,2,1,4,4,2,4,2,3,3,3,
2,3,3,1,1,3,0,3,2,1,3,4,3,4,1,1,0,1,2,1,
3,3,1,2,1,0,1,0,2,4,1,0,2,2,1,5,3,4,4,1,
3,0,0,0,3,2,1,1,2,1,1,2,2,3,2,-1,2,-1,1,1,
0,3,1,0,3,3,2,3,2,2,0,3,1,0,4,3,1,2,3,-1,
2,1,2,0,0,2,-1,5,1,0,2,0,0,-2,1,0,2,1,-1,1,
0,0,-1,1,1,0,2,1,0,0,2,-1,3,1,0,-2,3,3,0,2,
3,4,2,2,5,3,5,5,6,6,5,2,6,7,7,6,5,6,6,4,
6,4,5,8,7,8,9,6,8,10,7,7,9,9,7,8,10,8,9,10,
9,10,10,10,11,11,11,11,9,10,8,9,6,9,6,9,9,6,9,10,
7,8,7,8,8,8,8,11,11,12,11,12,12,9,12,13,12,14,13,14,
13,11,13,12,9,12,8,10,13,9,12,9,10,8,10,10,10,11,10,11,
7,12,12,11,13,10,12,9,11,9,12,8,9,10,11,9,10,10,9,10,
9,7,10,8,8,10,8,5,9,9,7,7,6,7,7,8,5,5,10,5,
5,7,4,5,6,6,5,5,4,7,4,4,5,4,5,6,6,4,5,6,
2,5,6,3,2,3,3,2,1,0,4,3,1,2,1,2,0,1,0,0,
1,3,1,4,0,4,4,4,5,4,3,6,3,5,5,5,6,5,7,4,
6,5,2,5,4,4,6,5,4,5,5,5,5,3,3,3,3,5,2,4,
0,3,3,1,2,5,4,2,4,2,3,4,5,4,4,3,6,4,7,4,
2,5,4,3,3,3,1,1,1,1,2,4,1,3,5,3,5,4,5,3,
3,4,4,0,2,4,1,2,3,2,3,0,3,3,3,2,5,4,6,4,
3,5,6,6,5,6,5,4,5,5,5,5,5,6,5,4,6,7,6,8,
6,6,7,7,5,6,4,6,8,6,5,6,5,4,3,5,6,5,7,3,
7,4,6,4,8,7,4,6,4,1,5,3,4,6,3,6,3,4,3,3,
2,3,3,3,4,3,3,3,4,2,4,2,1,2,2,3,2,3,3,2,
2,1,1,0,1,-1,1,0,1,1,3,3,2,1,3,4,3,2,4,2,
2,1,2,4,2,3,3,2,1,4,2,3,4,3,2,2,2,3,2,0,
4,2,5,2,3,4,4,3,4,2,5,5,3,2,5,3,6,4,6,3,
6,3,5,6,5,6,2,5,4,5,7,4,6,6,5,6,8,10,6,7,
7,8,9,9,9,9,8,9,10,11,11,10,12,13,12,12,11,10,11,10,
10,9,9,10,9,11,8,10,9,11,9,8,9,12,10,8,10,9,10,11,
10,9,8,9,8,8,9,7,9,8,9,8,8,5,8,6,8,6,8,6,
9,9,9,7,6,5,7,5,4,4,5,6,5,6,5,2,7,5,4,4,
4,5,8,5,5,7,3,4,3,2,6,4,4,5,5,4,5,2,4,2,
4,2,3,4,5,7,5,7,6,6,5,5,8,5,7,7,6,8,6,4,
7,9,5,3,5,6,7,6,5,7,5,5,8,5,4,8,4,6,6,6,
7,4,5,7,5,5,5,5,3,4,5,5,2,3,3,2,3,4,3,4,
1,2,6,3,3,2,4,2,3,4,2,1,4,2,1,-1,1,4,1,2,
2,3,-1,1,3,1,5,6,4,2,1,1,4,1,2,0,1,1,0,1,
0,1,1,-1,3,0,1,4,1,1,2,1,2,1,1,-1,1,-2,-2,0,
-1,0,-2,0,-2,-1,0,0,2,2,-1,2,-1,1,3,0,-1,0,1,0,
2,-1,2,4,2,1,2,2,1,3,1,2,2,1,0,0,5,2,0,1,
2,2,1,0,2,1,0,-1,0,0,0,1,1,0,1,-1,1,-1,1,-1,
1,-1,-4,-1,0,0,-1,1,1,0,1,3,2,1,4,1,3,3,4,4,
4,3,2,2,3,3,2,1,3,3,2,2,3,3,1,3,3,3,4,4,
2,3,4,2,2,0,3,1,2,1,2,0,-1,2,3,1,3,3,1,0,
0,0,0,1,0,0,-1,-2,-2,-3,-2,-5,-2,-2,-3,-4,-3,-6,-4,-6,
-7,-5,-5,-5,-4,-7,-6,-8,-11,-8,-9,-7,-10,-8,-8,-8,-7,-9,-9,-9,
-8,-8,-8,-8,-9,-7,-8,-8,-5,-9,-7,-7,-5,-4,-7,-7,-10,-8,-6,-7,
-5,-9,-7,-8,-9,-10,-7,-9,-10,-8,-9,-9,-10,-11,-9,-11,-9,-11,-10,-9,
-10,-8,-10,-7,-8,-8,-7,-9,-8,-7,-8,-6,-5,-5,-8,-5,-7,-9,-7,-6,
-9,-8,-5,-8,-6,-7,-7,-5,-7,-7,-6,-3,-6,-3,-4,-7,-3,-4,-3,-4,
-3,-1,-2,-4,-2,-3,1,-2,0,1,0,2,2,2,1,0,0,1,1,2,
1,-1,-2,-1,0,-1,0,1,-1,0,-2,-2,-2,1,-3,-1,-3,-2,-1,-4,
-1,-2,-5,-6,-3,-5,-7,-6,-6,-3,-5,-5,-7,-6,-8,-6,-6,-8,-5,-6,
-4,-2,-4,-4,-4,-5,-7,-3,-3,-4,-3,-5,-4,-5,-3,-4,-5,-5,-4,-9,
-6,-4,-5,-3,-4,-4,-2,-4,-5,-6,-5,-6,-5,-5,-3,-3,-5,-8,-4,-2,
-7,-3,-3,-5,-5,-5,-3,-6,-4,-6,-7,-5,-6,-6,-7,-5,-8,-6,-6,-5,
-6,-5,-6,-5,-4,-5,-5,-8,-8,-7,-6,-7,-8,-8,-5,-6,-5,-6,-7,-5,
-3,-3,-3,-2,-2,-4,-3,-4,-4,-5,-3,-6,-5,-6,-2,-3,-3,-3,-3,-4,
-3,-5,-4,-4,-5,-3,-2,-4,-2,-3,-1,-4,-4,-3,-3,-3,-4,-3,-5,-3,
-3,-4,-3,-2,-3,-2,-4,-2,-3,-4,-5,-4,-3,-5,-2,-3,-2,0,-1,1,
1,0,0,1,-1,0,0,3,1,1,1,3,2,3,3,2,2,3,0,4,
1,4,2,5,3,2,3,3,1,0,3,2,2,1,4,4,5,6,3,5,
5,2,4,5,5,3,6,3,7,5,4,7,7,7,10,10,7,9,10,6,
6,7,7,7,7,9,7,6,7,7,4,7,9,5,7,6,6,7,8,4,
6,8,7,6,7,5,6,7,7,6,3,6,4,6,7,3,4,4,5,4,
3,3,3,2,4,4,4,1,3,4,2,3,3,6,3,4,4,3,4,2,
4,2,5,3,4,4,3,5,3,3,4,5,3,2,3,5,4,2,6,4,
5,3,6,5,5,5,6,6,7,7,8,5,5,7,6,7,7,8,4,5,
5,7,7,5,8,7,6,5,7,6,4,6,5,2,7,5,5,6,6,4,
4,3,3,2,4,5,2,3,3,2,3,3,3,2,1,2,0,1,-1,-2,
-1,1,-2,-1,1,-1,3,2,0,0,1,0,0,0,1,-1,3,0,-1,0,
-1,1,-2,-1,1,-1,2,1,0,1,1,3,1,1,0,1,3,1,3,1,
2,-2,0,0,2,3,2,1,1,3,1,0,4,2,1,2,5,2,4,5,
7,4,5,2,2,1,1,2,2,3,2,0,2,0,0,0,-1,0,-1,-1,
0,0,1,0,2,-1,-3,0,0,-2,-1,-2,-2,0,-2,0,-1,-3,0,-4,
1,-2,0,-1,-1,1,-1,-1,-2,0,-2,-1,2,-1,-1,1,-3,-3,-3,-2,
-4,-2,-1,-2,-2,-2,-4,-2,-1,-5,-2,-2,-3,-4,-2,-4,-4,-3,-2,-1,
-4,-1,-2,-1,-1,3,1,0,0,1,-1,3,1,1,0,1,-1,1,2,1,
0,1,-2,2,1,1,0,3,2,3,2,2,2,3,0,4,5,3,5,3,
2,6,5,4,3,4,2,3,2,4,4,3,3,3,5,4,2,5,1,4,
4,4,7,7,3,8,6,6,4,6,6,3,3,3,2,4,1,3,2,1,
0,2,0,2,2,1,-1,2,2,2,2,2,1,3,1,2,2,2,0,3,
0,1,1,-1,1,0,2,3,0,0,1,2,0,1,1,0,1,2,2,-1,
3,1,1,0,-1,0,-1,-2,-1,-1,-1,1,-2,0,-1,-1,-1,-2,0,-1,
0,2,-1,1,0,-1,1,1,1,1,0,-2,1,0,2,0,1,-2,4,1,
0,2,5,2,0,6,3,3,0,6,4,2,5,3,4,4,2,5,4,4,
4,5,7,6,8,6,6,4,7,5,4,4,4,6,5,5,6,6,6,7
};


