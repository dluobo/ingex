/*
 * $Id: clapper_source.c,v 1.11 2011/09/27 10:14:29 philipn Exp $
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
#include <math.h>

#include "clapper_source.h"
#include "clapper_source_data.h"
#include "yuvlib/YUV_frame.h"
#include "yuvlib/YUV_text_overlay.h"
#include "video_conversion.h"
#include "utils.h"
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
#define FLASH_X_MARGIN              12


typedef struct
{
    int isPAL; /* otherwise NTSC */
    int numFramesPerSec;

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
    unsigned int audioFrameSizeSeq[5];
    unsigned int audioFrameSizeSeqSize;
    unsigned int audioFrameGroupSamples;
} ClapperSource;



static void set_video_frame_rate(ClapperSource* source, const Rational* frameRate)
{
    source->length = convert_length(source->length, &source->videoStreamInfo.frameRate, frameRate);
    source->position = convert_length(source->position, &source->videoStreamInfo.frameRate, frameRate);

    source->videoStreamInfo.frameRate = *frameRate;
    source->audioStreamInfo.frameRate = *frameRate;

    source->isPAL = is_pal_frame_rate(frameRate);
    source->numFramesPerSec = (source->isPAL ? 25 : 30);

    if (source->isPAL)
    {
        source->audioFrameSizeSeq[0] = 1920 * 2;
        source->audioFrameSizeSeqSize = 1;
        source->audioFrameGroupSamples = 1920;
    }
    else
    {
        source->audioFrameSizeSeq[0] = 1602 * 2;
        source->audioFrameSizeSeq[1] = 1601 * 2;
        source->audioFrameSizeSeq[2] = 1602 * 2;
        source->audioFrameSizeSeq[3] = 1601 * 2;
        source->audioFrameSizeSeq[4] = 1602 * 2;
        source->audioFrameSizeSeqSize = 5;
        source->audioFrameGroupSamples = 1602 * 3 + 1601 * 2;
    }
}

static void get_audio_offset_and_frame_size(ClapperSource* source, unsigned int* frameSize, int* offset)
{
    int i;
    int64_t audioPosition;
    int indexInSeq;

    indexInSeq = source->position % source->audioFrameSizeSeqSize;

    audioPosition = source->audioFrameGroupSamples * (int64_t)(source->position / source->audioFrameSizeSeqSize);
    for (i = 0; i < indexInSeq; i++)
    {
        audioPosition += source->audioFrameSizeSeq[i] / 2;
    }


    *frameSize = source->audioFrameSizeSeq[indexInSeq];
    if (source->isPAL)
    {
        *offset = (audioPosition % 48000) * 2 /* 16 bit */;
    }
    else
    {
        *offset = (audioPosition % 48048) * 2 /* 16 bit */;
    }
}

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
    unsigned char* imagePtr1;
    unsigned char* imagePtr2;
    YUV_frame yuvFrame;
    char txtY, txtU, txtV, box;
    overlay textOverlay;
    p_info_rec p_info;
    char buf[3];
    float fontScale = source->videoStreamInfo.width / 720.0;
    float tickWidthScale = source->videoStreamInfo.width / 720.0;
    int xMargin;
    int firstNumber;
    int lastNumber;

    memset(&p_info, 0, sizeof(p_info));
    CHK_ORET(YUV_frame_from_buffer(&yuvFrame, source->image, source->videoStreamInfo.width, source->videoStreamInfo.height, source->yuvFormat) == 1);

    barHeight = source->videoStreamInfo.height / REL_PROGRESS_BAR_HEIGHT;
    barHeight = (barHeight < 4) ? 4 : barHeight;
    barHeight += barHeight % 2;
    tickHeight = source->videoStreamInfo.height / REL_TICK_HEIGHT;
    tickHeight = (tickHeight < 4) ? 4 : tickHeight;
    tickHeight += tickHeight % 2;
    zeroTickHeight = tickHeight * 2;
    tickDelta = (source->videoStreamInfo.width - 2 * source->videoStreamInfo.width / REL_X_MARGIN) / source->numFramesPerSec;
    xMargin = (source->videoStreamInfo.width - source->numFramesPerSec * tickDelta) / 2;

    /* black background */

    fill_black(source->videoStreamInfo.format, source->videoStreamInfo.width, source->videoStreamInfo.height, source->image);


    /* ticks */

    xPos = xMargin + tickDelta / 2 - (TICK_WIDTH * tickWidthScale) / 2;
    yPos = source->videoStreamInfo.height / 2 + barHeight / 2 + TICK_Y_MARGIN;
    for (i = 0; i < source->numFramesPerSec; i++)
    {
        imagePtr1 = source->image + yPos * (source->videoStreamInfo.width * 2) + xPos * 2;
        for (j = 0; j < (TICK_WIDTH * tickWidthScale) / 2; j++)
        {
            *imagePtr1++ = g_rec601YUVColours[GREEN_COLOUR].U;
            *imagePtr1++ = g_rec601YUVColours[GREEN_COLOUR].Y;
            *imagePtr1++ = g_rec601YUVColours[GREEN_COLOUR].V;
            *imagePtr1++ = g_rec601YUVColours[GREEN_COLOUR].Y;
        }

        xPos += tickDelta;
    }
    imagePtr1 = source->image + yPos * (source->videoStreamInfo.width * 2);
    for (i = 0; i < tickHeight - 1; i++)
    {
        yPos++;
        imagePtr2 = source->image + yPos * (source->videoStreamInfo.width * 2);

        memcpy(imagePtr2, imagePtr1, source->videoStreamInfo.width * 2);
    }

    /* zero tick for PAL */

    if (source->isPAL)
    {
        xPos = xMargin + tickDelta * 13 - tickDelta / 2 - (ZERO_TICK_WIDTH * tickWidthScale) / 2;
    }
    else
    {
        xPos = xMargin + tickDelta * 15 - tickDelta / 2 - (ZERO_TICK_WIDTH * tickWidthScale) / 2;
    }
    yPos = source->videoStreamInfo.height / 2 + barHeight / 2 + TICK_Y_MARGIN;
    imagePtr1 = source->image + yPos * (source->videoStreamInfo.width * 2) + xPos * 2;
    for (j = 0; j < (ZERO_TICK_WIDTH * tickWidthScale) / 2; j++)
    {
        *imagePtr1++ = g_rec601YUVColours[GREEN_COLOUR].U;
        *imagePtr1++ = g_rec601YUVColours[GREEN_COLOUR].Y;
        *imagePtr1++ = g_rec601YUVColours[GREEN_COLOUR].V;
        *imagePtr1++ = g_rec601YUVColours[GREEN_COLOUR].Y;
    }
    imagePtr1 = source->image + yPos * (source->videoStreamInfo.width * 2) + xPos * 2;
    for (i = 0; i < zeroTickHeight - 1; i++)
    {
        yPos++;
        imagePtr2 = source->image + yPos * (source->videoStreamInfo.width * 2) + xPos * 2;

        memcpy(imagePtr2, imagePtr1, (ZERO_TICK_WIDTH * tickWidthScale) * 2);
    }


    /* tick labels */

    txtY = g_rec601YUVColours[GREEN_COLOUR].Y;
    txtU = g_rec601YUVColours[GREEN_COLOUR].U;
    txtV = g_rec601YUVColours[GREEN_COLOUR].V;
    box = 100;

    yPos = source->videoStreamInfo.height / 2 + barHeight / 2 + TICK_Y_MARGIN + tickHeight + 5;
    xPos = xMargin + tickDelta / 2 - (TICK_WIDTH * tickWidthScale);
    if (source->isPAL)
    {
        firstNumber = -12;
        lastNumber = 12;
    }
    else
    {
        firstNumber = -14;
        lastNumber = 15;
    }
    for (i = firstNumber; i < lastNumber + 1; i++)
    {
        if (i == 0)
        {
            /* no label at zero tick */
            xPos += tickDelta;
            continue;
        }

        sprintf(buf, "%d", i < 0 ? -i : i);
        if (text_to_overlay_player(&p_info, &textOverlay,
            buf,
            source->videoStreamInfo.width, 0,
            0, 0,
            0,
            0,
            0,
            "Ariel", fontScale * 12, source->videoStreamInfo.aspectRatio.num, source->videoStreamInfo.aspectRatio.den) < 0)
        {
            ml_log_error("Failed to create text overlay\n");
            return 1;
        }

        CHK_ORET(add_overlay(&textOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == YUV_OK);
        free_overlay(&textOverlay);

        xPos += tickDelta;
    }


    /* frames label */

    if (text_to_overlay_player(&p_info, &textOverlay,
        "frames",
        source->videoStreamInfo.width, 0,
        0, 0,
        0,
        0,
        0,
        "Ariel", fontScale * 14, source->videoStreamInfo.aspectRatio.num, source->videoStreamInfo.aspectRatio.den) < 0)
    {
        ml_log_error("Failed to create text overlay\n");
        return 1;
    }

    yPos = source->videoStreamInfo.height / 2 + barHeight / 2 + TICK_Y_MARGIN + tickHeight + fontScale * 14 + 5;
    xPos = xMargin + tickDelta / 2 - (TICK_WIDTH * tickWidthScale);
    CHK_ORET(add_overlay(&textOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == YUV_OK);
    free_overlay(&textOverlay);


    /* video/audio late */

    if (text_to_overlay_player(&p_info, &textOverlay,
        "VIDEO LATE",
        source->videoStreamInfo.width, 0,
        0, 0,
        0,
        0,
        0,
        "Ariel", fontScale * 24, source->videoStreamInfo.aspectRatio.num, source->videoStreamInfo.aspectRatio.den) < 0)
    {
        ml_log_error("Failed to create text overlay\n");
        return 1;
    }

    xPos = xMargin + 3 * tickDelta / 2;
    yPos = source->videoStreamInfo.height / 2 + barHeight + TICK_Y_MARGIN + zeroTickHeight * 3;
    CHK_ORET(add_overlay(&textOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == YUV_OK);
    free_overlay(&textOverlay);

    if (text_to_overlay_player(&p_info, &textOverlay,
        "AUDIO LATE",
        source->videoStreamInfo.width, 0,
        0, 0,
        0,
        0,
        0,
        "Ariel", fontScale * 24, source->videoStreamInfo.aspectRatio.num, source->videoStreamInfo.aspectRatio.den) < 0)
    {
        ml_log_error("Failed to create text overlay\n");
        return 1;
    }

    xPos = source->videoStreamInfo.width - xMargin - tickDelta - textOverlay.w;
    yPos = source->videoStreamInfo.height / 2 + barHeight + TICK_Y_MARGIN + zeroTickHeight * 3;
    CHK_ORET(add_overlay(&textOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == YUV_OK);
    free_overlay(&textOverlay);


    /* BBC logo */

    if (source->videoStreamInfo.width / 720.0 > 1.5)
    {
        xPos = source->videoStreamInfo.width / 2 - g_bbcLargeLogo.w / 2;
        yPos = source->videoStreamInfo.height / 2 + barHeight + TICK_Y_MARGIN + zeroTickHeight * 4;
        CHK_ORET(add_overlay(&g_bbcLargeLogo, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == YUV_OK);
    }
    else
    {
        xPos = source->videoStreamInfo.width / 2 - g_bbcLogo.w / 2;
        yPos = source->videoStreamInfo.height / 2 + barHeight + TICK_Y_MARGIN + zeroTickHeight * 4;
        CHK_ORET(add_overlay(&g_bbcLogo, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == YUV_OK);
    }


    free_info_rec(&p_info);

    return 1;
}

static void add_green_progress_bar(ClapperSource* source)
{
    int i;
    int yPos;
    int barHeight;
    int tickDelta;
    unsigned char* imagePtr1;
    unsigned char* imagePtr2;
    int barPosition;
    int xMargin;

    barHeight = source->videoStreamInfo.height / REL_PROGRESS_BAR_HEIGHT;
    barHeight = (barHeight < 4) ? 4 : barHeight;
    barHeight += barHeight % 2;
    tickDelta = (source->videoStreamInfo.width - 2 * source->videoStreamInfo.width / REL_X_MARGIN) / source->numFramesPerSec;
    xMargin = (source->videoStreamInfo.width - source->numFramesPerSec * tickDelta) / 2;

    barPosition = source->position % source->numFramesPerSec;

    yPos = source->videoStreamInfo.height / 2 - barHeight / 2;
    yPos += (yPos % 2); /* field 1 */
    imagePtr1 = source->image + yPos * (source->videoStreamInfo.width * 2) + 2 * (xMargin - tickDelta / 2);
    imagePtr2 = source->image + (yPos + 1) * (source->videoStreamInfo.width * 2) + 2 * (xMargin - tickDelta / 2);
    for (i = xMargin - tickDelta / 2; i < source->videoStreamInfo.width - xMargin + tickDelta / 2; i += 2)
    {
        /* field 2 */
        if (i - (xMargin - tickDelta / 2) < tickDelta / 2 ||
            ((i - (xMargin - tickDelta / 2)) / tickDelta) <= barPosition)
        {
            /* green */
            *imagePtr1++ = g_rec601YUVColours[GREEN_COLOUR].U;
            *imagePtr1++ = g_rec601YUVColours[GREEN_COLOUR].Y;
            *imagePtr1++ = g_rec601YUVColours[GREEN_COLOUR].V;
            *imagePtr1++ = g_rec601YUVColours[GREEN_COLOUR].Y;
        }
        else
        {
            /* black */
            *imagePtr1++ = g_rec601YUVColours[BLACK_COLOUR].U;
            *imagePtr1++ = g_rec601YUVColours[BLACK_COLOUR].Y;
            *imagePtr1++ = g_rec601YUVColours[BLACK_COLOUR].V;
            *imagePtr1++ = g_rec601YUVColours[BLACK_COLOUR].Y;
        }

        /* field 2 */
        if (((i - xMargin) / tickDelta) <= barPosition)
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
    imagePtr1 = source->image + yPos * (source->videoStreamInfo.width * 2);
    for (i = 0; i < barHeight - 2; i += 2)
    {
        yPos += 2;
        memcpy(source->image + yPos * (source->videoStreamInfo.width * 2), imagePtr1, source->videoStreamInfo.width * 2 * 2);
    }

}

static void add_red_flash(ClapperSource* source)
{
    int i;
    int yPos;
    int barHeight;
    unsigned char* imagePtr1;
    int flash;

    barHeight = source->videoStreamInfo.height / REL_FLASH_BAR_HEIGHT;
    barHeight = (barHeight < 4) ? 4 : barHeight;
    barHeight += barHeight % 2;

    if (source->isPAL)
    {
        flash = (source->position % source->numFramesPerSec) == 12;
    }
    else
    {
        flash = (source->position % source->numFramesPerSec) == 14;
    }

    yPos = 0;
    imagePtr1 = source->image + yPos * (source->videoStreamInfo.width * 2) + 2 * FLASH_X_MARGIN;
    for (i = FLASH_X_MARGIN; i < source->videoStreamInfo.width - FLASH_X_MARGIN; i += 2)
    {
        if (flash)
        {
            /* red */
            /* NOTE: using components defined in ingex/common/avsync_analysis.c: red_diff_uyvy() */
            /* TODO: change ingex/common/avsync_analysis.c? */
            *imagePtr1++ = 0x5f; /* g_rec601YUVColours[RED_COLOUR].U; */
            *imagePtr1++ = 0x4b; /* g_rec601YUVColours[RED_COLOUR].Y; */
            *imagePtr1++ = 0xe6; /* g_rec601YUVColours[RED_COLOUR].V; */
            *imagePtr1++ = 0x4b; /* g_rec601YUVColours[RED_COLOUR].Y; */
        }
        else
        {
            /* black */
            *imagePtr1++ = g_rec601YUVColours[BLACK_COLOUR].U;
            *imagePtr1++ = g_rec601YUVColours[BLACK_COLOUR].Y;
            *imagePtr1++ = g_rec601YUVColours[BLACK_COLOUR].V;
            *imagePtr1++ = g_rec601YUVColours[BLACK_COLOUR].Y;
        }
    }

    /* copy line to complete bar */
    imagePtr1 = source->image + yPos * (source->videoStreamInfo.width * 2);
    for (i = 0; i < barHeight - 1; i += 2)
    {
        yPos += 2;
        memcpy(source->image + yPos * (source->videoStreamInfo.width * 2), imagePtr1, source->videoStreamInfo.width * 2 * 2);
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

static void clp_set_frame_rate_or_disable(void* data, const Rational* frameRate)
{
    ClapperSource* source = (ClapperSource*)data;

    if ((!is_pal_frame_rate(frameRate) && !is_ntsc_frame_rate(frameRate)) ||
        (source->videoStreamInfo.isHardFrameRate &&
            memcmp(frameRate, &source->videoStreamInfo.frameRate, sizeof(*frameRate)) != 0))
    {
        msc_disable_stream(&source->mediaSource, VIDEO_STREAM_INDEX);
        msc_disable_stream(&source->mediaSource, AUDIO_L_STREAM_INDEX);
        msc_disable_stream(&source->mediaSource, AUDIO_R_STREAM_INDEX);
        return;
    }

    set_video_frame_rate(source, frameRate);
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

static void clp_disable_audio(void* data)
{
    ClapperSource* source = (ClapperSource*)data;

    source->audioLIsDisabled = 1;
    source->audioRIsDisabled = 1;
}

static void clp_disable_video(void* data)
{
    ClapperSource* source = (ClapperSource*)data;

    source->videoIsDisabled = 1;
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
    int audioOffset;
    unsigned int frameSize;

    get_audio_offset_and_frame_size(source, &frameSize, &audioOffset);


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
            if (!sdl_receive_frame_const(listener, AUDIO_L_STREAM_INDEX, &source->audioSec[audioOffset], frameSize))
            {
                return -1;
            }
        }
    }

    if (!source->audioRIsDisabled)
    {
        if (sdl_accept_frame(listener, AUDIO_R_STREAM_INDEX, frameInfo))
        {
            if (!sdl_receive_frame_const(listener, AUDIO_R_STREAM_INDEX, &source->audioSec[audioOffset], frameSize))
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

static void clp_set_source_name(void* data, const char* name)
{
    ClapperSource* source = (ClapperSource*)data;

    add_known_source_info(&source->videoStreamInfo, SRC_INFO_NAME, name);
    add_known_source_info(&source->audioStreamInfo, SRC_INFO_NAME, name);
}

static void clp_set_clip_id(void* data, const char* id)
{
    ClapperSource* source = (ClapperSource*)data;

    set_stream_clip_id(&source->videoStreamInfo, id);
    set_stream_clip_id(&source->audioStreamInfo, id);
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
        videoStreamInfo->format != UYVY_FORMAT ||
        (!stream_is_pal_frame_rate(videoStreamInfo) &&
            !stream_is_ntsc_frame_rate(videoStreamInfo)))
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

    set_video_frame_rate(newSource, &videoStreamInfo->frameRate);

    if (videoStreamInfo->format == UYVY_FORMAT)
    {
        newSource->imageSize = videoStreamInfo->width * videoStreamInfo->height * 2;
        newSource->yuvFormat = UYVY;
    }
    else if (videoStreamInfo->format == YUV422_FORMAT)
    {
        newSource->imageSize = videoStreamInfo->width * videoStreamInfo->height * 2;
        newSource->yuvFormat = Y42B;
    }
    else /* videoStreamInfo->format == YUV420_FORMAT */
    {
        newSource->imageSize = videoStreamInfo->width * videoStreamInfo->height * 3 / 2;
        newSource->yuvFormat = I420;
    }
    MALLOC_OFAIL(newSource->image, unsigned char, newSource->imageSize);

    newSource->audioSec = (unsigned char*)g_audioSec;


    newSource->mediaSource.data = newSource;
    newSource->mediaSource.get_num_streams = clp_get_num_streams;
    newSource->mediaSource.get_stream_info = clp_get_stream_info;
    newSource->mediaSource.set_frame_rate_or_disable = clp_set_frame_rate_or_disable;
    newSource->mediaSource.disable_stream = clp_disable_stream;
    newSource->mediaSource.disable_audio = clp_disable_audio;
    newSource->mediaSource.disable_video = clp_disable_video;
    newSource->mediaSource.stream_is_disabled = clp_stream_is_disabled;
    newSource->mediaSource.read_frame = clp_read_frame;
    newSource->mediaSource.is_seekable = clp_is_seekable;
    newSource->mediaSource.seek = clp_seek;
    newSource->mediaSource.get_length = clp_get_length;
    newSource->mediaSource.get_position = clp_get_position;
    newSource->mediaSource.get_available_length = clp_get_available_length;
    newSource->mediaSource.eof = clp_eof;
    newSource->mediaSource.set_source_name = clp_set_source_name;
    newSource->mediaSource.set_clip_id = clp_set_clip_id;
    newSource->mediaSource.close = clp_close;

    newSource->videoStreamInfo = *videoStreamInfo;
    newSource->videoStreamInfo.sourceId = msc_create_id();
    newSource->audioStreamInfo = *audioStreamInfo;
    newSource->audioStreamInfo.frameRate = newSource->videoStreamInfo.frameRate;
    newSource->audioStreamInfo.isHardFrameRate = newSource->videoStreamInfo.isHardFrameRate;
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






