/*
 * $Id: media_player.c,v 1.17 2011/04/19 10:08:48 philipn Exp $
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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <malloc.h>
#include <inttypes.h>
#include <sys/types.h>
#include <signal.h>
#include <assert.h>


#include "media_player.h"
#include "media_player_marks.h"
#include "connection_matrix.h"
#include "on_screen_display.h"
#include "video_switch_sink.h"
#include "audio_switch_sink.h"
#include "half_split_sink.h"
#include "source_event.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


#define MAX_VTR_ERROR_SOURCES   16

#define HAVE_PLAYER_LISTENER(player) \
    (player->listeners.listener != NULL)

#define SHOW_ALL_MARK_FILTER_POS    (-1)
#define SHOW_NONE_MARK_FILTER_POS   32


typedef struct MediaPlayerListenerElement
{
    struct MediaPlayerListenerElement* next;

    MediaPlayerListener* listener;
} MediaPlayerListenerElement;

typedef MediaPlayerListenerElement MediaPlayerListenerList;

typedef struct
{
    MediaControlMode mode;

    int locked;

    int play; /* 0 == pause, 1 == play */
    int stop;
    int speed; /* 1 == normal, unit is frames */
    int64_t nextPosition;
    int64_t lastPosition; /* position of last frame sent to sink */

    int setOSDTimecode;
    int osdTimecodeIndex;
    int osdTimecodeType;
    int osdTimecodeSubType;
    int nextOSDTimecode;
    int disableOSDDisplay;
    int disableOSDDisplayChanged;

    VTRErrorLevel vtrErrorLevel;
    int nextVTRErrorLevel;
    unsigned int enabledShowMarks[MAX_MARK_SELECTIONS];
    int markFilterPos[MAX_MARK_SELECTIONS];
    int nextMarkFilterPos[MAX_MARK_SELECTIONS];

    int refreshRequired;

    int64_t reviewStartPosition;
    int64_t reviewEndPosition;
    int64_t reviewReturnPosition;

    int droppedFrame;
    FrameInfo lastFrameBeforeDroppedFrame;

    FrameInfo lastFrameDisplayed;
} MediaPlayerState;

typedef struct MediaSinkStreamInfoElement
{
    struct MediaSinkStreamInfoElement* next;

    int streamId;
    int registeredStream;
    int acceptedFrame;
    int isPlayerSinkStream;
    int sourceId;
    unsigned char* eventBuffer;
    unsigned int eventBufferSize;
    unsigned int eventBufferAllocSize;
} MediaSinkStreamInfoElement;

typedef MediaSinkStreamInfoElement MediaSinkStreamInfoList;

struct MediaPlayer
{
    int closeAtEnd;
    int loop;

    Rational frameRate;

    /* used in ply_get_position to allow absolute positions to be calculated */
    int64_t startOffset;

    /* player state */
    MediaPlayerState state;
    MediaPlayerState prevState;
    pthread_mutex_t stateMutex;

    /* source, sink and connection matrix (via timecode extractor) linking them */
    MediaSource* mediaSource;
    MediaSink* mediaSink;
    ConnectionMatrix* connectionMatrix;
    MediaSink playerSink;
    MediaSinkStreamInfoList sinkStreamInfo;

    /* listener for sink events */
    MediaSinkListener sinkListener;

    /* media control */
    MediaControl control;

    /* listeners for player events */
    MediaPlayerListenerList listeners;

    /* source length; 0 indicates it is unknown */
    int64_t sourceLength;

    /* info about the current frame read */
    FrameInfo frameInfo;

    /* frame info for the frame at the furthest read position */
    FrameInfo endReadFrameInfo;

    /* count of frames read and accepted */
    int64_t frameCount;

    /* timeline marks */
    MediaPlayerMarks playerMarks;
    VTRErrorLevel vtrErrorLevel;
    unsigned int enabledShowMarks[MAX_MARK_SELECTIONS];
    int markFilterPos[MAX_MARK_SELECTIONS];
    int numMarkSelections;

    /* menu handler */
    MenuHandler* menuHandler;
    MenuHandlerListener menuListener;

    /* buffer state log file */
    FILE* bufferStateLogFile;

    /* quality check validation */
    qc_quit_validator_func qcQuitValidator;
    void* qcQuitValidatorData;
    int qcValidateMark;
};


typedef struct
{
    int sourceId;
    int streamId;
    int isDisabled;
    const StreamInfo* streamInfo;
} SourceInfoMap;



static void add_source_info_to_map(SourceInfoMap* sources, int streamId, int isDisabled, const StreamInfo* streamInfo)
{
    SourceInfoMap* source;
    SourceInfoMap prevSource;
    SourceInfoMap nextSource;

    source = &sources[0];
    while (source->streamInfo != NULL)
    {
        if (source->sourceId == streamInfo->sourceId &&
            source->streamId > streamId)
        {
            /* insert */
            break;
        }
        else if (source->sourceId > streamInfo->sourceId)
        {
            /* insert */
            break;
        }

        source++;
    }

    if (source->streamInfo == NULL)
    {
        /* append */
        source->sourceId = streamInfo->sourceId;
        source->streamId = streamId;
        source->isDisabled = isDisabled;
        source->streamInfo = streamInfo;
    }
    else
    {
        /* insert */
        prevSource.sourceId = streamInfo->sourceId;
        prevSource.streamId = streamId;
        prevSource.isDisabled = isDisabled;
        prevSource.streamInfo = streamInfo;
        while (source->streamInfo != NULL)
        {
            nextSource = *source;
            *source = prevSource;
            prevSource = nextSource;
            source++;
        }
        *source = prevSource;
    }
}


static void set_source_length(MediaPlayer* player, int64_t sourceLength)
{
    player->sourceLength = sourceLength;
    mpm_set_source_length(&player->playerMarks, sourceLength);
}


static int64_t legitimise_position(MediaPlayer* player, int64_t position)
{
    if (position < 0)
    {
        return 0;
    }
    else if (player->sourceLength > 0 && position >= player->sourceLength)
    {
        return player->sourceLength - 1;
    }
    return position;
}


static void switch_source_info_screen(MediaPlayer* player)
{
    OSDScreen screen;

    /* switch to the play state screen if the source info screen is up */
    if (osd_get_screen(msk_get_osd(player->mediaSink), &screen) &&
        screen == OSD_SOURCE_INFO_SCREEN)
    {
        if (osd_set_screen(msk_get_osd(player->mediaSink), OSD_PLAY_STATE_SCREEN))
        {
            player->state.refreshRequired = 1;
        }
    }
}


static MediaControlMode ply_get_mode(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    return player->state.mode;
}

static void ply_toggle_lock(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (player->state.locked)
    {
        player->state.droppedFrame = 0;
    }

    player->state.locked = !player->state.locked;
    player->state.refreshRequired = 1;


    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_play(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        player->state.play = 1;
        player->state.speed = 1;

        switch_source_info_screen(player);
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_stop(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;
    int locked = 0;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    locked = player->state.locked;
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)

    if (!locked)
    {
        if (player->qcQuitValidator == NULL ||
            player->qcQuitValidator(player, player->qcQuitValidatorData))
        {
            PTHREAD_MUTEX_LOCK(&player->stateMutex)
            player->state.stop = 1;
            PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
        }
    }
}

/* not requiring player->state.locked - use internally only */
static void _ply_pause(MediaPlayer* player)
{
    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    player->state.play = 0;
    player->state.speed = 1;

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_pause(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        _ply_pause(player);

        switch_source_info_screen(player);
    }
}

static void ply_toggle_play_pause(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        player->state.play = !player->state.play;
        player->state.speed = 1;

        switch_source_info_screen(player);
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

/* not requiring player->state.locked - use internally only */
static void _ply_seek(MediaPlayer* player, int64_t offset, int whence, PlayUnit unit)
{
    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (whence == SEEK_SET)
    {
        if (unit == FRAME_PLAY_UNIT)
        {
            player->state.nextPosition = offset;
        }
        else  /* PERCENTAGE_PLAY_UNIT (1/1000 %)*/
        {
            if (offset < 0)
            {
                player->state.nextPosition = (int64_t)((offset / (1000.0 * 100.0)) * player->sourceLength - 0.5);
            }
            else
            {
                player->state.nextPosition = (int64_t)((offset / (1000.0 * 100.0)) * player->sourceLength + 0.5);
            }
        }
    }
    else if (whence == SEEK_CUR)
    {
        if (unit == FRAME_PLAY_UNIT)
        {
            player->state.nextPosition = player->state.nextPosition + offset;
        }
        else  /* PERCENTAGE_PLAY_UNIT (1/1000 %) */
        {
            if (offset < 0)
            {
                player->state.nextPosition = player->state.nextPosition +
                    (int64_t)((offset / (1000.0 * 100.0)) * player->sourceLength - 0.5);
            }
            else
            {
                player->state.nextPosition = player->state.nextPosition +
                    (int64_t)((offset / (1000.0 * 100.0)) * player->sourceLength + 0.5);
            }
        }
    }
    else /* SEEK_END */
    {
        if (player->sourceLength > 0)
        {
            player->state.nextPosition = player->sourceLength - 1 - offset;
        }
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_seek(void* data, int64_t offset, int whence, PlayUnit unit)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        _ply_seek(player, offset, whence, unit);

        switch_source_info_screen(player);
    }
}

static void ply_play_speed(void* data, int speed, PlayUnit unit)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (speed == 0)
    {
        /* zero speed no allowed */
        return;
    }

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        player->state.play = 1;
        if (unit == FRAME_PLAY_UNIT || player->sourceLength <= 0)
        {
            player->state.speed = speed;
        }
        else /* PERCENTAGE_PLAY_UNIT */
        {
            player->state.speed = speed * player->sourceLength / 100;
            player->state.speed =
                (player->state.speed >= 0 && player->state.speed < 1) ? 1 : player->state.speed;
            player->state.speed =
                (player->state.speed < 0 && player->state.speed > -1) ? -1 : player->state.speed;
        }

        switch_source_info_screen(player);
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_play_speed_factor(void* data, float factor)
{
    MediaPlayer* player = (MediaPlayer*)data;
    int newSpeed = 1;

    if (factor == 0.0)
    {
        /* zero speed not allowed */
        return;
    }

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (player->state.speed > 0 && factor <= -1.0)
    {
        /* switch from forward to reverse */
        newSpeed = -1;
    }
    else if (player->state.speed < 0 && factor >= 1.0)
    {
        /* switch from reverse to forward */
        newSpeed = 1;
    }
    else if (factor < 0.0)
    {
        newSpeed = (int)(player->state.speed * (-1) * factor);
    }
    else /* factor >= 0.0 */
    {
        newSpeed = (int)(player->state.speed * factor);
    }

    if (newSpeed == 0)
    {
        newSpeed = (player->state.speed > 0) ? 1 : -1;
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)


    ply_play_speed(data, newSpeed, FRAME_PLAY_UNIT);
}

static void ply_step(void* data, int forward, PlayUnit unit)
{
    MediaPlayer* player = (MediaPlayer*)data;
    int64_t offset;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        player->state.play = 0;
        player->state.speed = 1;

        if (unit == FRAME_PLAY_UNIT || player->sourceLength <= 0)
        {
            offset = (forward ? 1 : -1);
        }
        else /* PERCENTAGE_PLAY_UNIT */
        {
            offset = (forward ? player->sourceLength / 100 : - player->sourceLength / 100);
            if (offset == 0)
            {
                offset = (forward ? 1 : -1);
            }
        }
        player->state.nextPosition = player->state.nextPosition + offset;

        switch_source_info_screen(player);
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_mute_audio(void* data, int mute)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        msk_mute_audio(player->mediaSink, mute);

        switch_source_info_screen(player);
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

/* TODO: ensure type has only 1 bit position set so that we can assume it elsewhere */
static void ply_mark_position(void* data, int64_t position, int type, int toggle)
{
    MediaPlayer* player = (MediaPlayer*)data;
    unsigned int currentMarkType;
    int validMark;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    /* QC validation checks */
    validMark = 1;
    if (player->qcValidateMark)
    {
        currentMarkType = mpm_get_current_mark_type(&player->playerMarks, position);

        /* check that historic (M1_MARK_TYPE) or transfer (M2_MARK_TYPE) fault marks are set
        before any video (M3_MARK_TYPE) and audio fault (M3_MARK_TYPE) marks */
        if (((type & M3_MARK_TYPE) || (type & M4_MARK_TYPE)) &&
            !(currentMarkType & M1_MARK_TYPE) && !(currentMarkType & M2_MARK_TYPE))
        {
            validMark = 0;
        }

        /* if historic or transfer mark types are being toggled off then also
        remove video and audio fault marks */
        if (toggle &&
            ((type & currentMarkType & M1_MARK_TYPE) ||
                (type & currentMarkType & M2_MARK_TYPE)))
        {
            type |= (currentMarkType & M3_MARK_TYPE); /* results is removal (if present) of video fault mark */
            type |= (currentMarkType & M4_MARK_TYPE); /* results is removal (if present) of audio fault mark */
        }
    }

    if (validMark)
    {
        mpm_add_mark(&player->playerMarks, position, type, toggle);
        player->state.refreshRequired = 1;
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}


/* TODO: ensure type has only 1 bit position set so that we can assume it elsewhere */
static void ply_mark(void* data, int type, int toggle)
{
    MediaPlayer* player = (MediaPlayer*)data;

    ply_mark_position(data, player->state.lastFrameDisplayed.position, type, toggle);

    switch_source_info_screen(player);
}

static void ply_mark_vtr_error_position(void* data, int64_t position, int toggle, uint8_t errorCode)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    mpm_add_vtr_error_mark(&player->playerMarks, position, toggle, errorCode);
    player->state.refreshRequired = 1;
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_clear_mark(void* data, unsigned int typeMask)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    mpm_remove_mark(&player->playerMarks, player->state.lastFrameDisplayed.position, typeMask);
    player->state.refreshRequired = 1;

    switch_source_info_screen(player);

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_clear_mark_position(void* data, int64_t position, unsigned int typeMask)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    mpm_remove_mark(&player->playerMarks, position, typeMask);
    player->state.refreshRequired = 1;

    switch_source_info_screen(player);

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_clear_all_marks(void* data, int typeMask)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    mpm_free_marks(&player->playerMarks, typeMask);
    player->state.refreshRequired = 1;

    switch_source_info_screen(player);

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_seek_next_mark(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;
    int64_t position = -1;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        position = mpm_active_find_next_mark(&player->playerMarks, player->state.lastFrameDisplayed.position);
        if (position >= 0)
        {
            player->state.play = 0;
            player->state.nextPosition = position;
        }
    }

    switch_source_info_screen(player);

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_seek_prev_mark(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;
    int64_t position = -1;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        position = mpm_active_find_prev_mark(&player->playerMarks, player->state.lastFrameDisplayed.position);
        if (position >= 0)
        {
            player->state.play = 0;
            player->state.nextPosition = position;
        }
    }

    switch_source_info_screen(player);

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_seek_clip_mark(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;
    int64_t position = -1;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        position = mpm_find_clip_mark(&player->playerMarks, player->state.lastFrameDisplayed.position);
        if (position >= 0)
        {
            player->state.play = 0;
            player->state.nextPosition = position;
        }
    }

    switch_source_info_screen(player);

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_next_active_mark_selection(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        mpm_next_active_mark_selection(&player->playerMarks);
    }
    player->state.refreshRequired = 1;
    switch_source_info_screen(player);

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_set_vtr_error_level(void* data, VTRErrorLevel level)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        player->state.vtrErrorLevel = level;
    }
    player->state.refreshRequired = 1;
    switch_source_info_screen(player);

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_next_vtr_error_level(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        player->state.nextVTRErrorLevel = 1;
    }
    player->state.refreshRequired = 1;
    switch_source_info_screen(player);

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_show_vtr_error_level(void* data, int enable)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        osd_show_vtr_error_level(msk_get_osd(player->mediaSink), enable);

        switch_source_info_screen(player);
    }
}

static void ply_set_mark_filter(void* data, int selection, unsigned int typeMask)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        if (selection >= 0 && selection < player->numMarkSelections)
        {
            player->state.enabledShowMarks[selection] = typeMask;
            player->state.markFilterPos[selection] = SHOW_ALL_MARK_FILTER_POS;
            player->state.refreshRequired = 1;

            switch_source_info_screen(player);
        }
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_next_show_marks(void* data, int selection)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        if (selection >= 0 && selection < player->numMarkSelections)
        {
            player->state.nextMarkFilterPos[selection] = 1;
            player->state.refreshRequired = 1;

            switch_source_info_screen(player);
        }
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_set_osd_screen(void* data, OSDScreen screen)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        if (osd_set_screen(msk_get_osd(player->mediaSink), screen))
        {
            player->state.refreshRequired = 1;
        }
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_next_osd_screen(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        if (osd_next_screen(msk_get_osd(player->mediaSink)))
        {
            player->state.refreshRequired = 1;
        }
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_set_osd_timecode(void* data, int index, int type, int subType)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        player->state.setOSDTimecode = 1;
        player->state.osdTimecodeIndex = index;
        player->state.osdTimecodeType = type;
        player->state.osdTimecodeSubType = subType;

        switch_source_info_screen(player);
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_next_osd_timecode(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        player->state.nextOSDTimecode = 1;

        switch_source_info_screen(player);
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_toggle_show_audio_level(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        osd_toggle_audio_level_visibility(msk_get_osd(player->mediaSink));

        switch_source_info_screen(player);
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_set_osd_play_state_position(void* data, OSDPlayStatePosition position)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    if (!player->state.locked)
    {
        osd_set_play_state_position(msk_get_osd(player->mediaSink), position);

        switch_source_info_screen(player);
        player->state.refreshRequired = 1;
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_switch_next_video(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        vsw_switch_next_video(msk_get_video_switch(player->mediaSink));

        switch_source_info_screen(player);
    }
}

static void ply_switch_prev_video(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        vsw_switch_prev_video(msk_get_video_switch(player->mediaSink));

        switch_source_info_screen(player);
    }
}

static void ply_switch_video(void* data, int index)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        vsw_switch_video(msk_get_video_switch(player->mediaSink), index);

        switch_source_info_screen(player);
    }
}

static void ply_show_source_name(void* data, int enable)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        vsw_show_source_name(msk_get_video_switch(player->mediaSink), enable);

        switch_source_info_screen(player);
    }
}

static void ply_toggle_show_source_name(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        vsw_toggle_show_source_name(msk_get_video_switch(player->mediaSink));

        switch_source_info_screen(player);
    }
}

static void ply_switch_next_audio_group(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        asw_switch_next_audio_group(msk_get_audio_switch(player->mediaSink));

        switch_source_info_screen(player);
    }
}

static void ply_switch_prev_audio_group(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        asw_switch_prev_audio_group(msk_get_audio_switch(player->mediaSink));

        switch_source_info_screen(player);
    }
}

static void ply_switch_audio_group(void* data, int index)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        asw_switch_audio_group(msk_get_audio_switch(player->mediaSink), index);

        switch_source_info_screen(player);
    }
}

static void ply_snap_audio_to_video(void* data, int enable)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        asw_snap_audio_to_video(msk_get_audio_switch(player->mediaSink), enable);

        switch_source_info_screen(player);
    }
}

static void ply_set_half_split_orientation(void* data, int vertical)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        hss_set_half_split_orientation(msk_get_half_split(player->mediaSink), vertical);

        switch_source_info_screen(player);
    }
}

static void ply_set_half_split_type(void* data, HalfSplitType type)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        hss_set_half_split_type(msk_get_half_split(player->mediaSink), type);

        switch_source_info_screen(player);
    }
}

static void ply_show_half_split(void* data, int showSplitDivide)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        hss_show_half_split(msk_get_half_split(player->mediaSink), showSplitDivide);

        switch_source_info_screen(player);
    }
}

static void ply_move_half_split(void* data, int rightOrDown, int speed)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        hss_move_half_split(msk_get_half_split(player->mediaSink), rightOrDown, speed);

        switch_source_info_screen(player);
    }
}

static void setup_review(MediaPlayer* player, int64_t startPosition, int64_t duration,
    int64_t returnPosition)
{
    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    player->state.nextPosition = legitimise_position(player, startPosition);

    player->state.reviewStartPosition = legitimise_position(player, startPosition);
    player->state.reviewEndPosition = legitimise_position(player, player->state.reviewStartPosition + duration);
    player->state.reviewReturnPosition = legitimise_position(player, returnPosition);

    player->state.play = 1;
    player->state.speed = 1;

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_review_start(void* data, int64_t duration)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (duration <= 0)
    {
        return;
    }

    if (!player->state.locked)
    {
        setup_review(player, 0, duration, 0);

        switch_source_info_screen(player);
    }
}

static void ply_review_end(void* data, int64_t duration)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (duration <= 0)
    {
        return;
    }

    if (!player->state.locked && player->sourceLength > 0)
    {
        setup_review(player, player->sourceLength - duration, duration, player->sourceLength - 1);

        switch_source_info_screen(player);
    }
}

static void ply_review(void* data, int64_t duration)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (duration <= 0)
    {
        return;
    }

    if (!player->state.locked)
    {
        setup_review(player, player->state.lastFrameDisplayed.position - duration / 2,
            duration, player->state.lastFrameDisplayed.position);

        switch_source_info_screen(player);
    }
}

static void ply_next_menu_item(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        mnh_next_menu_item(player->menuHandler);
        /* TODO: menu should tell us if a refresh is required */
        player->state.refreshRequired = 1;
    }
}

static void ply_previous_menu_item(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        mnh_previous_menu_item(player->menuHandler);
        /* TODO: menu should tell us if a refresh is required */
        player->state.refreshRequired = 1;
    }
}

static void ply_select_menu_item_left(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        mnh_select_menu_item_left(player->menuHandler);
        /* TODO: menu should tell us if a refresh is required */
        player->state.refreshRequired = 1;
    }
}

static void ply_select_menu_item_right(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        mnh_select_menu_item_right(player->menuHandler);
        /* TODO: menu should tell us if a refresh is required */
        player->state.refreshRequired = 1;
    }
}

static void ply_select_menu_item_center(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        mnh_select_menu_item_center(player->menuHandler);
        /* TODO: menu should tell us if a refresh is required */
        player->state.refreshRequired = 1;
    }
}

static void ply_select_menu_item_extra(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (!player->state.locked)
    {
        mnh_select_menu_item_extra(player->menuHandler);
        /* TODO: menu should tell us if a refresh is required */
        player->state.refreshRequired = 1;
    }
}


static int ply_sink_accept_stream(void* data, const StreamInfo* streamInfo)
{
    MediaPlayer* player = (MediaPlayer*)data;

    if (streamInfo->type == TIMECODE_STREAM_TYPE ||
        (streamInfo->type == EVENT_STREAM_TYPE && streamInfo->format == SOURCE_EVENT_FORMAT))
    {
        /* the playerSink is always interested in timecode and source events */
        return 1;
    }
    else
    {
        return msk_accept_stream(player->mediaSink, streamInfo);
    }
}

static void allocate_event_buffer(MediaSinkStreamInfoElement* ele, unsigned int bufferSize)
{
    if (!ele->eventBuffer || ele->eventBufferAllocSize < bufferSize)
    {
        SAFE_FREE(&ele->eventBuffer);
        ele->eventBuffer = (unsigned char*)malloc(bufferSize);
        ele->eventBufferAllocSize = bufferSize;
    }
}

static int ply_sink_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    MediaPlayer* player = (MediaPlayer*)data;
    int sinkRegisterResult = 0;
    MediaSinkStreamInfoElement* ele = &player->sinkStreamInfo;
    MediaSinkStreamInfoElement* prevEle = &player->sinkStreamInfo;

    /* check if sink accepts and registers the stream */
    if (msk_accept_stream(player->mediaSink, streamInfo))
    {
        sinkRegisterResult = msk_register_stream(player->mediaSink, streamId, streamInfo);
    }


    /* record stream information */
    if (ele->streamId >= 0)
    {
        /* find the element or create a new one */
        while (ele != NULL && ele->streamId != streamId)
        {
            prevEle = ele;
            ele = ele->next;
        }
        if (ele == NULL)
        {
            /* create a new one */
            CALLOC_ORET(ele, MediaSinkStreamInfoElement, 1);
            prevEle->next = ele;
        }
    }
    ele->streamId = streamId;
    ele->registeredStream = sinkRegisterResult;
    ele->acceptedFrame = 0;
    ele->isPlayerSinkStream = (streamInfo->type == TIMECODE_STREAM_TYPE ||
           (streamInfo->type == EVENT_STREAM_TYPE && streamInfo->format == SOURCE_EVENT_FORMAT));
    ele->sourceId = streamInfo->sourceId;
    ele->eventBufferSize = 0;

    /* the playerSink is always interested in timecode, source events, or sink has accepted and registered */
    return ele->isPlayerSinkStream || sinkRegisterResult;
}

static int ply_sink_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    MediaPlayer* player = (MediaPlayer*)data;
    MediaSinkStreamInfoElement* ele = &player->sinkStreamInfo;

    /* find the stream */
    while (ele != NULL && ele->streamId != streamId)
    {
        ele = ele->next;
    }
    if (ele == NULL)
    {
        assert(ele != NULL);
        ml_log_error("Unknown stream %d\n", streamId);
        return 0;
    }

    /* check if sink accepts the frame */
    if (ele->registeredStream)
    {
        ele->acceptedFrame = msk_accept_stream_frame(player->mediaSink, streamId, frameInfo);
    }

    /* the playerSink is always interested in timecode, source events, or sink has accepted frame */
    return ele->isPlayerSinkStream || ele->acceptedFrame;
}

static int ply_sink_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    MediaPlayer* player = (MediaPlayer*)data;
    const StreamInfo* streamInfo;
    MediaSinkStreamInfoElement* ele = &player->sinkStreamInfo;

    /* find the stream */
    while (ele != NULL && ele->streamId != streamId)
    {
        ele = ele->next;
    }
    if (ele == NULL)
    {
        assert(ele != NULL);
        ml_log_error("Unknown stream %d\n", streamId);
        return 0;
    }

    if (ele->acceptedFrame)
    {
        /* if it is timecode or source event, then the sink will get it and we have to extract it later
           in receive_stream_frame */

        return msk_get_stream_buffer(player->mediaSink, streamId, bufferSize, buffer);
    }
    else
    {
        /* if the sink hasn't accepted the timecode or source event then point the media source to the
           timecode in the frame info or the source event buffer */

        if (msc_get_stream_info(player->mediaSource, streamId, &streamInfo))
        {
            if (streamInfo->type == TIMECODE_STREAM_TYPE)
            {
                if (sizeof(Timecode) != bufferSize)
                {
                    ml_log_error("Buffer size (%d) != timecode event data size (%d)\n", bufferSize, sizeof(Timecode));
                    return 0;
                }

                if ((size_t)player->frameInfo.numTimecodes < sizeof(player->frameInfo.timecodes) / sizeof(TimecodeInfo))
                {
                    player->frameInfo.timecodes[player->frameInfo.numTimecodes].streamId = streamId;
                    player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecodeType = streamInfo->timecodeType;
                    player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecodeSubType = streamInfo->timecodeSubType;

                    *buffer = (unsigned char*)&player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecode;

                    player->frameInfo.numTimecodes++;
                }
            }
            else if (streamInfo->type == EVENT_STREAM_TYPE && streamInfo->format == SOURCE_EVENT_FORMAT)
            {
                allocate_event_buffer(ele, bufferSize);
                *buffer = ele->eventBuffer;
            }
        }

        return 1;
    }
}

static int ply_sink_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    MediaPlayer* player = (MediaPlayer*)data;
    const StreamInfo* streamInfo;
    MediaSinkStreamInfoElement* ele = &player->sinkStreamInfo;
    int result;
    int haveStreamInfo;

    /* find the stream */
    while (ele != NULL && ele->streamId != streamId)
    {
        ele = ele->next;
    }
    if (ele == NULL)
    {
        assert(ele != NULL);
        ml_log_error("Unknown stream %d\n", streamId);
        return 0;
    }

    haveStreamInfo = msc_get_stream_info(player->mediaSource, streamId, &streamInfo);

    result = 1;
    if (ele->acceptedFrame)
    {
        result = msk_receive_stream_frame(player->mediaSink, streamId, buffer, bufferSize);

        if (haveStreamInfo)
        {
            if (streamInfo->type == TIMECODE_STREAM_TYPE)
            {
                if (sizeof(Timecode) == bufferSize &&
                    (size_t)player->frameInfo.numTimecodes < sizeof(player->frameInfo.timecodes) / sizeof(TimecodeInfo))
                {
                    player->frameInfo.timecodes[player->frameInfo.numTimecodes].streamId = streamId;
                    player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecode = *(Timecode*)buffer;
                    player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecodeType = streamInfo->timecodeType;
                    player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecodeSubType = streamInfo->timecodeSubType;
                    player->frameInfo.numTimecodes++;
                }
            }
            else if (streamInfo->type == EVENT_STREAM_TYPE && streamInfo->format == SOURCE_EVENT_FORMAT)
            {
                if (bufferSize > 0)
                {
                    allocate_event_buffer(ele, bufferSize);
                    memcpy(ele->eventBuffer, buffer, bufferSize);
                }
                ele->eventBufferSize = bufferSize;
            }
        }
    }
    else if (haveStreamInfo &&
        streamInfo->type == EVENT_STREAM_TYPE && streamInfo->format == SOURCE_EVENT_FORMAT)
    {
        /* event buffer (== buffer) was filled - actual size confirmed here */
        ele->eventBufferSize = bufferSize;
    }
    /* else if it was a timecode then the data has been put in frameInfo */

    return result;
}

static int ply_sink_receive_stream_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    MediaPlayer* player = (MediaPlayer*)data;
    const StreamInfo* streamInfo;
    MediaSinkStreamInfoElement* ele = &player->sinkStreamInfo;
    int result;
    unsigned char* nonconstBuffer;

    if (player->mediaSink->receive_stream_frame_const == NULL)
    {
        /* use the non-const functions */
        result = ply_sink_get_stream_buffer(data, streamId, bufferSize, &nonconstBuffer);
        if (result)
        {
            memcpy(nonconstBuffer, buffer, bufferSize);
            result = ply_sink_receive_stream_frame(data, streamId, nonconstBuffer, bufferSize);
        }
        return result;
    }

    /* find the stream */
    while (ele != NULL && ele->streamId != streamId)
    {
        ele = ele->next;
    }
    if (ele == NULL)
    {
        assert(ele != NULL);
        ml_log_error("Unknown stream %d\n", streamId);
        return 0;
    }

    result = 1;
    if (ele->acceptedFrame)
    {
        result = msk_receive_stream_frame_const(player->mediaSink, streamId, buffer, bufferSize);
    }

    if (msc_get_stream_info(player->mediaSource, streamId, &streamInfo))
    {
        if (streamInfo->type == TIMECODE_STREAM_TYPE)
        {
            if (sizeof(Timecode) == bufferSize &&
                (size_t)player->frameInfo.numTimecodes < sizeof(player->frameInfo.timecodes) / sizeof(TimecodeInfo))
            {
                player->frameInfo.timecodes[player->frameInfo.numTimecodes].streamId = streamId;
                player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecode = *(Timecode*)buffer;
                player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecodeType = streamInfo->timecodeType;
                player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecodeSubType = streamInfo->timecodeSubType;
                player->frameInfo.numTimecodes++;
            }
        }
        else if (streamInfo->type == EVENT_STREAM_TYPE && streamInfo->format == SOURCE_EVENT_FORMAT)
        {
            if (bufferSize > 0)
            {
                allocate_event_buffer(ele, bufferSize);
                memcpy(ele->eventBuffer, buffer, bufferSize);
            }
            ele->eventBufferSize = bufferSize;
        }
    }

    return result;
}




static void send_frame_displayed_event(MediaPlayer* player, const FrameInfo* frameInfo)
{
    MediaPlayerListenerElement* ele = &player->listeners;

    if (!HAVE_PLAYER_LISTENER(player))
    {
        return;
    }

    while (ele != NULL)
    {
        mpl_frame_displayed_event(ele->listener, frameInfo);
        ele = ele->next;
    }
}

static void send_frame_dropped_event(MediaPlayer* player, const FrameInfo* lastFrameInfo)
{
    MediaPlayerListenerElement* ele = &player->listeners;

    if (!HAVE_PLAYER_LISTENER(player))
    {
        return;
    }

    while (ele != NULL)
    {
        mpl_frame_dropped_event(ele->listener, lastFrameInfo);
        ele = ele->next;
    }
}

static void send_end_of_source_event(MediaPlayer* player, const FrameInfo* lastReadFrameInfo)
{
    MediaPlayerListenerElement* ele = &player->listeners;

    if (!HAVE_PLAYER_LISTENER(player))
    {
        return;
    }

    while (ele != NULL)
    {
        mpl_end_of_source_event(ele->listener, lastReadFrameInfo);
        ele = ele->next;
    }
}

static void send_start_of_source_event(MediaPlayer* player, const FrameInfo* firstReadFrameInfo)
{
    MediaPlayerListenerElement* ele = &player->listeners;

    if (!HAVE_PLAYER_LISTENER(player))
    {
        return;
    }

    while (ele != NULL)
    {
        mpl_start_of_source_event(ele->listener, firstReadFrameInfo);
        ele = ele->next;
    }
}

static void send_source_name_change_event(MediaPlayer* player)
{
    MediaPlayerListenerElement* listenerEle;
    MediaSinkStreamInfoElement* streamEle = &player->sinkStreamInfo;
    int numEvents;
    int i;
    SourceEvent event;

    if (!HAVE_PLAYER_LISTENER(player))
    {
        return;
    }

    /* extract source name events from event streams and send through to listeners */
    while (streamEle != NULL)
    {
        if (streamEle->eventBufferSize <= 0)
        {
            streamEle = streamEle->next;
            continue;
        }

        numEvents = svt_read_num_events(streamEle->eventBuffer);
        for (i = 0; i < numEvents; i++)
        {
            svt_read_event(streamEle->eventBuffer, i, &event);
            if (event.type == SOURCE_NAME_UPDATE_EVENT_TYPE)
            {
                listenerEle = &player->listeners;
                while (listenerEle != NULL)
                {
                    mpl_source_name_change_event(listenerEle->listener, streamEle->sourceId, event.value.nameUpdate.name);
                    listenerEle = listenerEle->next;
                }
            }
        }

        /* reset */
        streamEle->eventBufferSize = 0;

        streamEle = streamEle->next;
    }
}

static void send_start_state_event(MediaPlayer* player)
{
    MediaPlayerListenerElement* ele = &player->listeners;
    MediaPlayerStateEvent stateEvent;
    MediaPlayerState currentState;

    if (!HAVE_PLAYER_LISTENER(player))
    {
        /* no listeners, no event to send */
        return;
    }

    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    currentState = player->state;
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)

    memset(&stateEvent, 0, sizeof(MediaPlayerStateEvent));

    stateEvent.displayedFrameInfo = currentState.lastFrameDisplayed;
    stateEvent.displayedFrameInfo.position = -1; /* indicate to listener that a frame hasn't been displayed */
    stateEvent.lockedChanged = 1;
    stateEvent.locked = currentState.locked;
    stateEvent.playChanged = 1;
    stateEvent.play = currentState.play;
    stateEvent.stopChanged = 1;
    stateEvent.stop = currentState.stop;
    stateEvent.speedChanged = 1;
    stateEvent.speed = currentState.speed;

    while (ele != NULL)
    {
        mpl_state_change_event(ele->listener, &stateEvent);
        ele = ele->next;
    }
}

static void send_state_change_event(MediaPlayer* player)
{
    MediaPlayerListenerElement* ele = &player->listeners;
    MediaPlayerStateEvent stateEvent;
    MediaPlayerState currentState;
    int sendEvent;

    if (!HAVE_PLAYER_LISTENER(player))
    {
        /* no listeners, no event to send */
        return;
    }

    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    currentState = player->state;
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)

    memset(&stateEvent, 0, sizeof(MediaPlayerStateEvent));

    /* determine the changes and whether to send an event */
    sendEvent = 0;
    stateEvent.displayedFrameInfo = currentState.lastFrameDisplayed;
    if (currentState.locked != player->prevState.locked)
    {
        stateEvent.lockedChanged = 1;
        sendEvent = 1;
    }
    stateEvent.locked = currentState.locked;
    if (currentState.play != player->prevState.play)
    {
        stateEvent.playChanged = 1;
        sendEvent = 1;
    }
    stateEvent.play = currentState.play;
    if (currentState.stop != player->prevState.stop)
    {
        stateEvent.stopChanged = 1;
        sendEvent = 1;
    }
    stateEvent.stop = currentState.stop;
    if (currentState.speed != player->prevState.speed)
    {
        stateEvent.speedChanged = 1;
        sendEvent = 1;
    }
    stateEvent.speed = currentState.speed;

    /* send event to listeners */
    if (sendEvent)
    {
        while (ele != NULL)
        {
            mpl_state_change_event(ele->listener, &stateEvent);
            ele = ele->next;
        }
    }


    player->prevState = currentState;
}

static void send_player_closed(MediaPlayer* player)
{
    MediaPlayerListenerElement* ele = &player->listeners;

    if (!HAVE_PLAYER_LISTENER(player))
    {
        return;
    }

    while (ele != NULL)
    {
        mpl_player_closed(ele->listener);
        ele = ele->next;
    }
}


static void ply_frame_displayed(void* data, const FrameInfo* frameInfo)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    player->state.lastFrameDisplayed = *frameInfo;
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)

    send_frame_displayed_event(player, frameInfo);
}

static void ply_frame_dropped(void* data, const FrameInfo* lastFrameInfo)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)

    /* frame is dropped if player is locked when playing at normal speed and the
    end of source has not been reached */
    if (player->state.locked &&
        player->state.play &&
        player->state.speed == 1 &&
        !(player->sourceLength > 0 && player->state.lastPosition >= player->sourceLength - 1))
    {
        if (!player->state.droppedFrame)
        {
            player->state.refreshRequired = 1;
        }
        player->state.droppedFrame = 1;
        player->state.lastFrameBeforeDroppedFrame = *lastFrameInfo;
    }

    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)

    send_frame_dropped_event(player, lastFrameInfo);
}

static void ply_refresh_required(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    player->state.refreshRequired = 1;
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_refresh_required_for_menu(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    if (player->state.mode == MENU_MODE)
    {
        player->state.refreshRequired = 1;
    }
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_osd_screen_changed(void* data, OSDScreen screen)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    player->state.refreshRequired = 1;
    if (screen == OSD_MENU_SCREEN)
    {
        player->state.mode = MENU_MODE;
    }
    else
    {
        player->state.mode = PLAY_MODE;
    }
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}


void ply_source_updated(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    player->state.refreshRequired = 1;
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}



void mpl_frame_displayed_event(MediaPlayerListener* listener, const FrameInfo* frameInfo)
{
    if (listener && listener->frame_displayed_event)
    {
        listener->frame_displayed_event(listener->data, frameInfo);
    }
}

void mpl_frame_dropped_event(MediaPlayerListener* listener, const FrameInfo* lastFrameInfo)
{
    if (listener && listener->frame_dropped_event)
    {
        listener->frame_dropped_event(listener->data, lastFrameInfo);
    }
}

void mpl_state_change_event(MediaPlayerListener* listener, const MediaPlayerStateEvent* event)
{
    if (listener && listener->state_change_event)
    {
        listener->state_change_event(listener->data, event);
    }
}

void mpl_end_of_source_event(MediaPlayerListener* listener, const FrameInfo* lastReadFrameInfo)
{
    if (listener && listener->end_of_source_event)
    {
        listener->end_of_source_event(listener->data, lastReadFrameInfo);
    }
}

void mpl_start_of_source_event(MediaPlayerListener* listener, const FrameInfo* firstReadFrameInfo)
{
    if (listener && listener->start_of_source_event)
    {
        listener->start_of_source_event(listener->data, firstReadFrameInfo);
    }
}

void mpl_source_name_change_event(MediaPlayerListener* listener, int sourceId, const char* name)
{
    if (listener && listener->source_name_change_event)
    {
        listener->source_name_change_event(listener->data, sourceId, name);
    }
}

void mpl_player_closed(MediaPlayerListener* listener)
{
    if (listener && listener->player_closed)
    {
        listener->player_closed(listener->data);
    }
}


void mhl_refresh_required(MenuHandlerListener* listener)
{
    if (listener && listener->refresh_required)
    {
        listener->refresh_required(listener->data);
    }
}


void mnh_set_listener(MenuHandler* handler, MenuHandlerListener* listener)
{
    if (handler && handler->set_listener)
    {
        handler->set_listener(handler->data, listener);
    }
}

void mnh_next_menu_item(MenuHandler* handler)
{
    if (handler && handler->next_menu_item)
    {
        handler->next_menu_item(handler->data);
    }
}

void mnh_previous_menu_item(MenuHandler* handler)
{
    if (handler && handler->previous_menu_item)
    {
        handler->previous_menu_item(handler->data);
    }
}

void mnh_select_menu_item_left(MenuHandler* handler)
{
    if (handler && handler->select_menu_item_left)
    {
        handler->select_menu_item_left(handler->data);
    }
}

void mnh_select_menu_item_right(MenuHandler* handler)
{
    if (handler && handler->select_menu_item_right)
    {
        handler->select_menu_item_right(handler->data);
    }
}

void mnh_select_menu_item_center(MenuHandler* handler)
{
    if (handler && handler->select_menu_item_center)
    {
        handler->select_menu_item_center(handler->data);
    }
}

void mnh_select_menu_item_extra(MenuHandler* handler)
{
    if (handler && handler->select_menu_item_extra)
    {
        handler->select_menu_item_extra(handler->data);
    }
}

void mnh_free(MenuHandler* handler)
{
    if (handler && handler->free)
    {
        handler->free(handler->data);
    }
}



int ply_create_player(MediaSource* mediaSource, MediaSink* mediaSink,
    int initialLock, int closeAtEnd, int numFFMPEGThreads, int useWorkerThreads,
    int loop, int showFieldSymbol, const Timecode* startVITC, const Timecode* startLTC,
    FILE* bufferStateLogFile, unsigned int* markSelectionTypeMasks, int numMarkSelections, MediaPlayer** player)
{
    MediaPlayer* newPlayer;
    int64_t sourceLength;
    int i;
    int numStreams;
    int haveFrameRate;

    if (mediaSource == NULL || mediaSink == NULL)
    {
        ml_log_error("Invalid parameters to ply_create_player\n");
        return 0;
    }
    if (numMarkSelections < 0 || numMarkSelections > MAX_MARK_SELECTIONS)
    {
        ml_log_error("Invalid number of mark selections\n");
        return 0;
    }

    CALLOC_ORET(newPlayer, MediaPlayer, 1);

    newPlayer->closeAtEnd = closeAtEnd;
    newPlayer->loop = loop;
    newPlayer->bufferStateLogFile = bufferStateLogFile;
    newPlayer->vtrErrorLevel = VTR_ALMOST_GOOD_LEVEL;
    for (i = 0; i < MAX_MARK_SELECTIONS; i++)
    {
        newPlayer->enabledShowMarks[i] = ALL_MARK_TYPE;
        newPlayer->markFilterPos[i] = SHOW_ALL_MARK_FILTER_POS;
    }

    newPlayer->frameInfo.position = -1; /* make sure that first frame at position 0 is not marked as a repeat */


    /* choose a video frame rate and set this for all streams - streams that cannot change their
    video frame rate are disabled */

    haveFrameRate = 0;
    numStreams = msc_get_num_streams(mediaSource);

    /* first find a video stream with a unchangeable frame rate */
    for (i = 0; i < numStreams; i++)
    {
        const StreamInfo* streamInfo;
        if (msc_get_stream_info(mediaSource, i, &streamInfo))
        {
            if (streamInfo->type == PICTURE_STREAM_TYPE &&
                streamInfo->isHardFrameRate)
            {
                newPlayer->frameRate = streamInfo->frameRate;
                haveFrameRate = 1;
                break;
            }
        }
    }
    /* next find a non-video stream with a unchangeable frame rate */
    for (i = 0; i < numStreams; i++)
    {
        const StreamInfo* streamInfo;
        if (msc_get_stream_info(mediaSource, i, &streamInfo))
        {
            if (streamInfo->isHardFrameRate)
            {
                newPlayer->frameRate = streamInfo->frameRate;
                haveFrameRate = 1;
                break;
            }
        }
    }
    /* next try find a video stream with a changeable frame rate */
    if (!haveFrameRate)
    {
        for (i = 0; i < numStreams; i++)
        {
            const StreamInfo* streamInfo;
            if (msc_get_stream_info(mediaSource, i, &streamInfo))
            {
                if (streamInfo->type == PICTURE_STREAM_TYPE)
                {
                    newPlayer->frameRate = streamInfo->frameRate;
                    haveFrameRate = 1;
                    break;
                }
            }
        }
    }
    /* now default to the PAL video frame */
    if (!haveFrameRate)
    {
        newPlayer->frameRate = g_palFrameRate;
        haveFrameRate = 1;
    }

    msc_set_frame_rate_or_disable(mediaSource, &newPlayer->frameRate);
    newPlayer->frameInfo.frameRate = newPlayer->frameRate;



    newPlayer->sinkListener.data = newPlayer;
    newPlayer->sinkListener.frame_displayed = ply_frame_displayed;
    newPlayer->sinkListener.frame_dropped = ply_frame_dropped;
    newPlayer->sinkListener.refresh_required = ply_refresh_required;
    newPlayer->sinkListener.osd_screen_changed = ply_osd_screen_changed;
    CHK_OFAIL(msk_register_listener(mediaSink, &newPlayer->sinkListener));

    CHK_OFAIL(init_mutex(&newPlayer->stateMutex));

    newPlayer->state.play = 1;
    newPlayer->state.speed = 1;
    newPlayer->state.nextPosition = 0;
    newPlayer->state.lastPosition = -1;
    newPlayer->state.reviewEndPosition = -1;

    newPlayer->mediaSource = mediaSource;
    newPlayer->mediaSink = mediaSink;

    newPlayer->playerSink = *mediaSink; /* copy function pointers */
    newPlayer->playerSink.data = newPlayer;
    newPlayer->playerSink.accept_stream = ply_sink_accept_stream;
    newPlayer->playerSink.register_stream = ply_sink_register_stream;
    newPlayer->playerSink.accept_stream_frame = ply_sink_accept_stream_frame;
    newPlayer->playerSink.get_stream_buffer = ply_sink_get_stream_buffer;
    newPlayer->playerSink.receive_stream_frame = ply_sink_receive_stream_frame;
    newPlayer->playerSink.receive_stream_frame_const = ply_sink_receive_stream_frame_const;
    newPlayer->sinkStreamInfo.streamId = -1;

    CHK_OFAIL(stm_create_connection_matrix(mediaSource, &newPlayer->playerSink,
        numFFMPEGThreads, useWorkerThreads, &newPlayer->connectionMatrix));

    newPlayer->control.data = newPlayer;
    newPlayer->control.get_mode = ply_get_mode;
    newPlayer->control.toggle_lock = ply_toggle_lock;
    newPlayer->control.play = ply_play;
    newPlayer->control.stop = ply_stop;
    newPlayer->control.pause = ply_pause;
    newPlayer->control.toggle_play_pause = ply_toggle_play_pause;
    newPlayer->control.seek = ply_seek;
    newPlayer->control.play_speed = ply_play_speed;
    newPlayer->control.play_speed_factor = ply_play_speed_factor;
    newPlayer->control.step = ply_step;
    newPlayer->control.mute_audio = ply_mute_audio;
    newPlayer->control.mark = ply_mark;
    newPlayer->control.mark_position = ply_mark_position;
    newPlayer->control.mark_vtr_error_position = ply_mark_vtr_error_position;
    newPlayer->control.clear_mark = ply_clear_mark;
    newPlayer->control.clear_mark_position= ply_clear_mark_position;
    newPlayer->control.clear_all_marks = ply_clear_all_marks;
    newPlayer->control.seek_next_mark = ply_seek_next_mark;
    newPlayer->control.seek_prev_mark = ply_seek_prev_mark;
    newPlayer->control.seek_clip_mark = ply_seek_clip_mark;
    newPlayer->control.next_active_mark_selection = ply_next_active_mark_selection;
    newPlayer->control.set_vtr_error_level = ply_set_vtr_error_level;
    newPlayer->control.next_vtr_error_level = ply_next_vtr_error_level;
    newPlayer->control.show_vtr_error_level = ply_show_vtr_error_level;
    newPlayer->control.set_mark_filter = ply_set_mark_filter;
    newPlayer->control.next_show_marks = ply_next_show_marks;
    newPlayer->control.set_osd_screen = ply_set_osd_screen;
    newPlayer->control.next_osd_screen = ply_next_osd_screen;
    newPlayer->control.set_osd_timecode = ply_set_osd_timecode;
    newPlayer->control.next_osd_timecode = ply_next_osd_timecode;
    newPlayer->control.toggle_show_audio_level = ply_toggle_show_audio_level;
    newPlayer->control.set_osd_play_state_position = ply_set_osd_play_state_position;
    newPlayer->control.switch_next_video = ply_switch_next_video;
    newPlayer->control.switch_prev_video = ply_switch_prev_video;
    newPlayer->control.switch_video = ply_switch_video;
    newPlayer->control.show_source_name = ply_show_source_name;
    newPlayer->control.toggle_show_source_name = ply_toggle_show_source_name;
    newPlayer->control.switch_next_audio_group = ply_switch_next_audio_group;
    newPlayer->control.switch_prev_audio_group = ply_switch_prev_audio_group;
    newPlayer->control.switch_audio_group = ply_switch_audio_group;
    newPlayer->control.snap_audio_to_video = ply_snap_audio_to_video;
    newPlayer->control.set_half_split_orientation = ply_set_half_split_orientation;
    newPlayer->control.set_half_split_type = ply_set_half_split_type;
    newPlayer->control.show_half_split = ply_show_half_split;
    newPlayer->control.move_half_split = ply_move_half_split;
    newPlayer->control.review_start = ply_review_start;
    newPlayer->control.review_end = ply_review_end;
    newPlayer->control.review = ply_review;
    newPlayer->control.next_menu_item = ply_next_menu_item;
    newPlayer->control.previous_menu_item = ply_previous_menu_item;
    newPlayer->control.select_menu_item_left = ply_select_menu_item_left;
    newPlayer->control.select_menu_item_right = ply_select_menu_item_right;
    newPlayer->control.select_menu_item_center = ply_select_menu_item_center;
    newPlayer->control.select_menu_item_extra = ply_select_menu_item_extra;

    newPlayer->menuListener.data = newPlayer;
    newPlayer->menuListener.refresh_required = ply_refresh_required_for_menu;

    if (initialLock)
    {
        newPlayer->state.locked = 1;
    }

    if (!msc_get_length(mediaSource, &sourceLength))
    {
        ml_log_warn("Source media has unknown length\n");
    }
    else
    {
        newPlayer->sourceLength = sourceLength;
    }

    osd_show_field_symbol(msk_get_osd(mediaSink), showFieldSymbol);

    newPlayer->prevState = newPlayer->state;


    /* seek to start timecodes */
    if (memcmp(startVITC, &g_invalidTimecode, sizeof(Timecode)) != 0)
    {
        ml_log_info("Seeking to VITC\n");
        if (msc_seek_timecode(mediaSource, startVITC, SOURCE_TIMECODE_TYPE, VITC_SOURCE_TIMECODE_SUBTYPE) != 0)
        {
            ml_log_error("Failed to seek to VITC\n");
            goto fail;
        }
        CHK_OFAIL(msc_get_position(mediaSource, &newPlayer->state.lastPosition));
        newPlayer->state.lastPosition--;
        newPlayer->state.nextPosition = newPlayer->state.lastPosition + 1;
    }
    else if (memcmp(startLTC, &g_invalidTimecode, sizeof(Timecode)) != 0)
    {
        ml_log_info("Seeking to LTC\n");
        if (msc_seek_timecode(mediaSource, startLTC, SOURCE_TIMECODE_TYPE, LTC_SOURCE_TIMECODE_SUBTYPE) != 0)
        {
            ml_log_error("Failed to seek to LTC\n");
            goto fail;
        }
        CHK_OFAIL(msc_get_position(mediaSource, &newPlayer->state.lastPosition));
        newPlayer->state.lastPosition--;
        newPlayer->state.nextPosition = newPlayer->state.lastPosition + 1;
    }

    CHK_OFAIL(mpm_init_player_marks(&newPlayer->playerMarks, msk_get_osd(mediaSink), sourceLength,
                                    markSelectionTypeMasks, numMarkSelections));
    mpm_set_vtr_error_level_filter(&newPlayer->playerMarks, newPlayer->vtrErrorLevel);
    newPlayer->numMarkSelections = mpm_get_num_mark_selections(&newPlayer->playerMarks);

    *player = newPlayer;
    return 1;

fail:
    ply_close_player(&newPlayer);
    return 0;
}

int ply_register_player_listener(MediaPlayer* player, MediaPlayerListener* listener)
{
    MediaPlayerListenerElement* ele = &player->listeners;
    MediaPlayerListenerElement* newEle = NULL;

    /* handle first appended source */
    if (!HAVE_PLAYER_LISTENER(player))
    {
        ele->listener = listener;
        return 1;
    }

    /* move to end */
    while (ele->next != NULL)
    {
        ele = ele->next;
    }

    /* create element */
    CALLOC_ORET(newEle, MediaPlayerListenerElement, 1);

    /* append source and take ownership */
    ele->next = newEle;
    newEle->listener = listener;
    return 1;
}

MediaControl* ply_get_media_control(MediaPlayer* player)
{
    return &player->control;
}

int ply_start_player(MediaPlayer* player, int startPaused)
{
    MediaPlayerState currentState;
    int64_t oldNextPosition;
    int isEOF = 0;
    int reportedEOFLastTime = 0;
    int64_t sourceLength;
    int rateControl;
    int updatedOSD;
    int readFrame;
    int completedFrame;
    int haveSeeked;
    int readResult;
    int prevReversePlay = 0;
    int reversePlay = 0;
    int refreshRequired = 0;
    int seekResult;
    int syncResult;
    int moveBeyondLimits;
    int seekOK;
    int64_t prevAvailableSourceLength = 0;
    int64_t availableSourceLength = 0;
    int lastWrittenSeek = 0;
    int newFrameRequired;
    int isRepeat;
    int sourceProcessingCompleted = 0;
    unsigned int currentMarkTypes[MAX_MARK_SELECTIONS];
    unsigned int currentMarkTypeMasks[MAX_MARK_SELECTIONS];
    uint8_t currentMarkVTRErrorCode;
    int doStartPause = startPaused;
    int muteAudio = 0;
    unsigned int markFilter;
    int i;


    /* send starting state to the listeners */

    send_start_state_event(player);


    /* play frames from media source to media sink */

    while (!player->state.stop)
    {
        updatedOSD = 0;
        readFrame = 0;
        completedFrame = 0;
        haveSeeked = 0;
        seekOK = 0;
        moveBeyondLimits = 0;
        muteAudio = 0;

        /* get the available source length, which will be < source length if the file is still being written to */
        if (availableSourceLength < player->sourceLength)
        {
            if (!msc_get_available_length(player->mediaSource, &availableSourceLength))
            {
                availableSourceLength = 0;
                prevAvailableSourceLength = 0;
            }
            else
            {
                if (availableSourceLength != prevAvailableSourceLength)
                {
                    /* the OSD progress bar should be updated */
                    refreshRequired = 1;
                }
                prevAvailableSourceLength = availableSourceLength;
            }
        }

        /* finish processing a source if it has completed */
        if (!sourceProcessingCompleted && msc_is_complete(player->mediaSource))
        {
            sourceProcessingCompleted = msc_post_complete(player->mediaSource, player->mediaSource, &player->control);
        }

        /* execute paused start */

        if (doStartPause)
        {
            _ply_pause(player);
            doStartPause = 0;
            muteAudio = 1;
        }


        PTHREAD_MUTEX_LOCK(&player->stateMutex)
        currentState = player->state;
        PTHREAD_MUTEX_UNLOCK(&player->stateMutex)


        /* process VTR error level changes */

        if (currentState.vtrErrorLevel != player->vtrErrorLevel)
        {
            player->vtrErrorLevel = currentState.vtrErrorLevel;
            mpm_set_vtr_error_level_filter(&player->playerMarks, player->vtrErrorLevel);
        }
        else if (currentState.nextVTRErrorLevel)
        {
            player->vtrErrorLevel = (VTRErrorLevel) ((player->vtrErrorLevel + 1) % (VTR_NO_GOOD_LEVEL + 1));
            mpm_set_vtr_error_level_filter(&player->playerMarks, player->vtrErrorLevel);
        }

        /* process marks filter changes */

        for (i = 0; i < player->numMarkSelections; i++)
        {
            if (currentState.enabledShowMarks[i] != player->enabledShowMarks[i] ||
                currentState.markFilterPos[i] != player->markFilterPos[i])
            {
                if (currentState.markFilterPos[i] <= SHOW_ALL_MARK_FILTER_POS)
                {
                    mpm_set_mark_filter(&player->playerMarks, i, currentState.enabledShowMarks[i]);
                }
                else if (currentState.markFilterPos[i] >= SHOW_NONE_MARK_FILTER_POS)
                {
                    mpm_set_mark_filter(&player->playerMarks, i, 0);
                }
                else if (currentState.enabledShowMarks[i] & (0x00000001 << currentState.markFilterPos[i]))
                {
                    mpm_set_mark_filter(&player->playerMarks, i, 0x00000001 << currentState.markFilterPos[i]);
                }
                /* else ignore change */

                player->enabledShowMarks[i] = currentState.enabledShowMarks[i];
                player->markFilterPos[i] = currentState.markFilterPos[i];
            }
            else if (currentState.nextMarkFilterPos[i])
            {
                markFilter = 0;
                while (1)
                {
                    player->markFilterPos[i]++;
                    if (player->markFilterPos[i] > SHOW_NONE_MARK_FILTER_POS)
                    {
                        player->markFilterPos[i] = SHOW_ALL_MARK_FILTER_POS;
                    }

                    if (player->markFilterPos[i] >= SHOW_NONE_MARK_FILTER_POS) {
                        markFilter = 0;
                        break;
                    } else if (player->markFilterPos[i] <= SHOW_ALL_MARK_FILTER_POS) {
                        markFilter = player->enabledShowMarks[i];
                        break;
                    } else {
                        markFilter = 0x00000001 << player->markFilterPos[i];
                        if (player->enabledShowMarks[i] & markFilter)
                            break;
                    }
                }
                mpm_set_mark_filter(&player->playerMarks, i, markFilter);
            }
        }


        /* update the sink OSD */

        if (currentState.nextOSDTimecode)
        {
            if (osd_next_timecode(msk_get_osd(player->mediaSink)))
            {
                refreshRequired = 1;
            }
        }
        if (currentState.setOSDTimecode)
        {
            if (osd_set_timecode(msk_get_osd(player->mediaSink), currentState.osdTimecodeIndex,
                currentState.osdTimecodeType, currentState.osdTimecodeSubType))
            {
                refreshRequired = 1;
            }
        }
        if (osd_set_play_state(msk_get_osd(player->mediaSink), OSD_PLAY_STATE, currentState.play))
        {
            refreshRequired = 1;
        }
        if (osd_set_play_state(msk_get_osd(player->mediaSink), OSD_SPEED_STATE, currentState.speed))
        {
            refreshRequired = 1;
        }


        PTHREAD_MUTEX_LOCK(&player->stateMutex)
        player->state.setOSDTimecode = 0;
        player->state.nextOSDTimecode = 0;
        player->state.nextVTRErrorLevel = 0;
        player->state.vtrErrorLevel = player->vtrErrorLevel;
        memcpy(player->state.enabledShowMarks, player->enabledShowMarks, sizeof(player->state.enabledShowMarks));
        memcpy(player->state.markFilterPos, player->markFilterPos, sizeof(player->state.markFilterPos));
        memset(player->state.nextMarkFilterPos, 0, sizeof(player->state.nextMarkFilterPos));
        PTHREAD_MUTEX_UNLOCK(&player->stateMutex)



        /* check reviewing state */

        PTHREAD_MUTEX_LOCK(&player->stateMutex)
        currentState = player->state;
        PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
        if (currentState.reviewEndPosition >= 0)
        {
            if (currentState.nextPosition < currentState.reviewStartPosition ||
                currentState.nextPosition > currentState.reviewEndPosition)
            {
                /* review has ended because player is positioned outside review window */
                PTHREAD_MUTEX_LOCK(&player->stateMutex)
                player->state.reviewStartPosition = -1;
                player->state.reviewEndPosition = -1;
                player->state.reviewReturnPosition = -1;
                PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
            }
            else if (currentState.nextPosition == currentState.reviewEndPosition)
            {
                /* review has ended normally */
                _ply_pause(player);
                _ply_seek(player, currentState.reviewReturnPosition, SEEK_SET, FRAME_PLAY_UNIT);

                PTHREAD_MUTEX_LOCK(&player->stateMutex)
                player->state.reviewStartPosition = -1;
                player->state.reviewEndPosition = -1;
                player->state.reviewReturnPosition = -1;
                PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
            }
        }



        /* get current state that remains fixed until the end of this loop */

        PTHREAD_MUTEX_LOCK(&player->stateMutex)

        currentState = player->state;
        oldNextPosition = player->state.nextPosition;

        /* record whether a seek is beyond the end of the file, so later on we can send a end of stream event */
        if (currentState.nextPosition != legitimise_position(player, currentState.nextPosition))
        {
            moveBeyondLimits = 1;
        }

        /* make double sure that next position is a legit position */
        currentState.nextPosition = legitimise_position(player, currentState.nextPosition);

        refreshRequired = refreshRequired || /* could have failed to refresh last time */
            player->state.refreshRequired ||
            (!player->state.play && prevReversePlay); /* need to have field order back to normal */


        player->state.refreshRequired = 0;

        PTHREAD_MUTEX_UNLOCK(&player->stateMutex)



        /* move to next frame if position is unchanged and not at EOF */

        if (currentState.play &&
            currentState.lastPosition == currentState.nextPosition)
        {
            currentState.nextPosition = legitimise_position(player, currentState.nextPosition + currentState.speed);
            rateControl = 1;
        }
        else
        {
            rateControl = 0;
        }


        /* set if new frame must be read */

        newFrameRequired = currentState.lastPosition != currentState.nextPosition;


        /* read frame data if required */

        if (refreshRequired || newFrameRequired)
        {
            /* seek to position if required */
            if (currentState.nextPosition > currentState.lastPosition + 1 ||
                currentState.nextPosition < currentState.lastPosition ||
                currentState.lastPosition == currentState.nextPosition /* re-read frame */)
            {
                haveSeeked = 1;
                seekResult = msc_seek(player->mediaSource, currentState.nextPosition);
                if (seekResult != 0)
                {
                    /* NOTE: seeks will generally only fail for sources that read something as part of a
                    seek because a fseek doesn't fail if you seek past the end of the file
                    Eg. the MXF OP1A reader will fail but not the MXF OPAtom reader because the OP1A reader
                    will read the first KL to check it is the start of the content package */
                    seekOK = 0;

                    if (seekResult != -2) /* not timed out */
                    {
                        /* seek failed */
                        /* try set source length if unknown */
                        if (player->sourceLength <= 0 && msc_eof(player->mediaSource))
                        {
                            if (player->sourceLength <= 0)
                            {
                                if (msc_get_position(player->mediaSource, &sourceLength))
                                {
                                    set_source_length(player, sourceLength);
                                }
                            }
                        }
                        else
                        {
                            /* try seeking to the last frame written to the file, assuming writing is still
                            taking place */
                            if (!lastWrittenSeek &&
                                availableSourceLength > 0 &&
                                availableSourceLength - 1 > currentState.lastPosition &&
                                availableSourceLength - 1 < currentState.nextPosition)
                            {
                                lastWrittenSeek = 1;
                            }
                            else
                            {
                                lastWrittenSeek = 0;
                            }
                        }
                    }
                }
                else
                {
                    seekOK = 1;
                }
            }

            if (!haveSeeked || seekOK)
            {
                /* log the buffers state */

                if (player->bufferStateLogFile != NULL)
                {
                    int numBuffers;
                    int numBuffersFilled;
                    if (msc_get_buffer_state(player->mediaSource, &numBuffers, &numBuffersFilled))
                    {
                        fprintf(player->bufferStateLogFile, "%d", numBuffersFilled);
                    }
                    else
                    {
                        fprintf(player->bufferStateLogFile, "0");
                    }
                    if (msk_get_buffer_state(player->mediaSink, &numBuffers, &numBuffersFilled))
                    {
                        fprintf(player->bufferStateLogFile, "\t%d\n", numBuffersFilled);
                    }
                    else
                    {
                        fprintf(player->bufferStateLogFile, "\t0\n");
                    }
                }

                /* set the frame information */

                reversePlay = (currentState.play && currentState.nextPosition < currentState.lastPosition);
                isRepeat = player->frameInfo.position == currentState.nextPosition;

                memset(&player->frameInfo, 0, sizeof(FrameInfo));
                player->frameInfo.frameRate = player->frameRate;
                player->frameInfo.position = currentState.nextPosition;
                player->frameInfo.sourceLength = player->sourceLength;
                player->frameInfo.availableSourceLength = availableSourceLength;
                player->frameInfo.startOffset = player->startOffset;
                player->frameInfo.frameCount = player->frameCount;
                player->frameInfo.rateControl = rateControl;
                player->frameInfo.reversePlay = reversePlay;
                player->frameInfo.numMarkSelections = player->numMarkSelections;
                mpm_update_current_mark(&player->playerMarks, currentState.nextPosition,
                                        currentMarkTypes, currentMarkTypeMasks, &currentMarkVTRErrorCode);
                for (i = 0; i < player->numMarkSelections; i++)
                {
                    if (currentMarkTypes[i] != 0)
                    {
                        player->frameInfo.isMarked = 1;
                    }
                    player->frameInfo.markTypes[i] = currentMarkTypes[i];
                    player->frameInfo.markTypeMasks[i] = currentMarkTypeMasks[i];
                }
                player->frameInfo.vtrErrorCode = currentMarkVTRErrorCode;
                player->frameInfo.vtrErrorLevel = player->vtrErrorLevel;
                player->frameInfo.isRepeat = isRepeat;
                player->frameInfo.muteAudio = muteAudio;
                player->frameInfo.locked = currentState.locked;
                player->frameInfo.droppedFrame = currentState.droppedFrame;

                /* Read an input frame */
                readResult = msc_read_frame(player->mediaSource, &player->frameInfo, stm_get_stream_listener(player->connectionMatrix));

                /* make sure all stream connects have completed their work */
                syncResult = stm_sync(player->connectionMatrix);

                if (readResult == 0 && syncResult == 1)
                {
                    readFrame = 1;

                    /* signal to sink that frame is complete */
                    if (!msk_complete_frame(player->mediaSink, &player->frameInfo))
                    {
                        ml_log_error("Failed to complete frame - cancelling\n");
                        msk_cancel_frame(player->mediaSink);
                    }
                    else
                    {
                        if (player->frameInfo.position >= player->endReadFrameInfo.position)
                        {
                            player->endReadFrameInfo = player->frameInfo;
                        }
                        player->frameCount++;
                        completedFrame = 1;
                    }

                    currentState.lastPosition = currentState.nextPosition;
                    prevReversePlay = reversePlay;
                    refreshRequired = 0;

                    if (lastWrittenSeek)
                    {
                        _ply_pause(player);
                        lastWrittenSeek = 0;
                    }
                }
                else
                {
                    /* signal to sink that frame is cancelled */
                    msk_cancel_frame(player->mediaSink);

                    if (readResult != 0 && readResult != -2) /* timed out == -2 */
                    {
                        ml_log_error("Failed to read frame\n");

                        if (msc_eof(player->mediaSource))
                        {
                            /* set source length if unknown */
                            if (player->sourceLength <= 0)
                            {
                                if (msc_get_position(player->mediaSource, &sourceLength))
                                {
                                    set_source_length(player, sourceLength);
                                }
                            }
                            lastWrittenSeek = 0;
                        }
                        else
                        {
                            /* try seeking to the last frame written to the file, assuming writing is still
                            taking place */
                            if (!lastWrittenSeek &&
                                availableSourceLength > 0 &&
                                availableSourceLength - 1 > currentState.lastPosition &&
                                availableSourceLength - 1 < currentState.nextPosition)
                            {
                                lastWrittenSeek = 1;
                            }
                            else
                            {
                                lastWrittenSeek = 0;
                                /* pause the player because of the failure */
                                _ply_pause(player);
                            }

                            /* only force a refresh if we were not paused */
                            if (currentState.play)
                            {
                                refreshRequired = 1;
                            }
                            else
                            {
                                refreshRequired = 0;
                            }
                        }
                    }

                    if (syncResult != 1)
                    {
                        ml_log_error("Failed to sync streams - pausing\n");
                        _ply_pause(player);

                        /* only force a refresh if we were not paused */
                        if (currentState.play)
                        {
                            refreshRequired = 1;
                        }
                        else
                        {
                            refreshRequired = 0;
                        }
                    }

                    reversePlay = prevReversePlay;
                }
            }
        }


        /* update the player state */

        PTHREAD_MUTEX_LOCK(&player->stateMutex)
        player->state.lastPosition = currentState.lastPosition;
        if (lastWrittenSeek)
        {
            player->state.nextPosition = legitimise_position(player, availableSourceLength - 1);
        }
        else if (player->state.nextPosition == oldNextPosition)
        {
            /* input control has not changed the next position, so we set it to the current position */
            player->state.nextPosition = legitimise_position(player, currentState.lastPosition);
        }
        PTHREAD_MUTEX_UNLOCK(&player->stateMutex)


        /* send EOF event */

        if (player->sourceLength > 0 && currentState.lastPosition >= player->sourceLength - 1)
        {
            isEOF = 1;
            if (!reportedEOFLastTime || moveBeyondLimits)
            {
                send_end_of_source_event(player, &player->endReadFrameInfo);
                reportedEOFLastTime = 1;
            }
        }
        else
        {
            isEOF = 0;
            reportedEOFLastTime = 0;
        }

        /* send SOF event */
        /* note: must use player->prevState before send_state_change_event is called */

        if (readFrame &&
            currentState.lastPosition == 0 &&
            (player->prevState.lastPosition != 0 || moveBeyondLimits))
        {
            send_start_of_source_event(player, &player->frameInfo);
        }


        /* send state change events */

        send_state_change_event(player);


        /* send source name change events */

        send_source_name_change_event(player);


        /* close at end or loop if required */

        if (isEOF && player->closeAtEnd)
        {
            ply_stop(player);
        }
        else if (isEOF && player->loop)
        {
            ply_seek(player, 0, SEEK_SET, FRAME_PLAY_UNIT);
        }


        if (!readFrame)
        {
            /* wait a bit so we don't use 100% CPU looping */
            usleep(1000);
        }
    }


    /* send remaining state change events */
    send_state_change_event(player);


    return 1;
}

void ply_close_player(MediaPlayer** player)
{
    MediaPlayerListenerElement* eleL;
    MediaPlayerListenerElement* nextEleL;
    MediaSinkStreamInfoElement* eleA;
    MediaSinkStreamInfoElement* nextEleA;

    if (*player == NULL)
    {
        return;
    }

    send_player_closed(*player);

    msk_unregister_listener((*player)->mediaSink, &(*player)->sinkListener);

    eleL = &(*player)->listeners;
    if (eleL->listener != NULL)
    {
        eleL = eleL->next;
        while (eleL != NULL)
        {
            nextEleL = eleL->next;
            SAFE_FREE(&eleL);
            eleL = nextEleL;
        }
    }

    mnh_free((*player)->menuHandler);

    eleA = &(*player)->sinkStreamInfo;
    eleA = eleA->next;
    while (eleA != NULL)
    {
        nextEleA = eleA->next;
        SAFE_FREE(&eleA->eventBuffer);
        SAFE_FREE(&eleA);
        eleA = nextEleA;
    }

    stm_close(&(*player)->connectionMatrix);

    mpm_clear_player_marks(&(*player)->playerMarks, msk_get_osd((*player)->mediaSink));

    destroy_mutex(&(*player)->stateMutex);

    SAFE_FREE(player);
}

int ply_get_marks(MediaPlayer* player, Mark** marks)
{
    assert(sizeof(BasicMark) == sizeof(Mark));

    BasicMark *basicMarks = NULL;
    int numMarks = mpm_get_marks(&player->playerMarks, &basicMarks);
    if (numMarks == 0)
    {
        return 0;
    }

    *marks = (Mark*)basicMarks;
    return numMarks;
}

MediaSink* ply_get_media_sink(MediaPlayer* player)
{
    return player->mediaSink;
}

/* TODO: media control should be locked when we do this */
void ply_set_menu_handler(MediaPlayer* player, MenuHandler* handler)
{
    if (player->menuHandler != NULL)
    {
        mnh_free(player->menuHandler);
    }

    player->menuHandler = handler;
    mnh_set_listener(handler, &player->menuListener);
}

void ply_enable_clip_marks(MediaPlayer* player, int markType)
{
    mpm_enable_clip_marks(&player->playerMarks, markType);
}

void ply_set_start_offset(MediaPlayer* player, int64_t offset)
{
    player->startOffset = offset;
}

void ply_print_source_info(MediaPlayer* player)
{
    int i;
    int numStreams;
    const StreamInfo* streamInfo;
    SourceInfoMap* sources = NULL;
    int sourceId = -1;
    const char* sourceName;

    numStreams = msc_get_num_streams(player->mediaSource);

    CALLOC_OFAIL(sources, SourceInfoMap, numStreams);

    for (i = 0; i < numStreams; i++)
    {
        if (msc_get_stream_info(player->mediaSource, i, &streamInfo))
        {
            add_source_info_to_map(sources, i, msc_stream_is_disabled(player->mediaSource, i), streamInfo);
        }
    }

    printf("Source Information:\n");
    for (i = 0; i < numStreams; i++)
    {
        if (sources[i].streamInfo == NULL)
        {
            break;
        }

        sourceName = get_known_source_info_value(sources[i].streamInfo, SRC_INFO_FILE_NAME);
        if (sourceName == NULL)
        {
            sourceName = get_known_source_info_value(sources[i].streamInfo, SRC_INFO_TITLE);
        }

        if (sources[i].sourceId != sourceId)
        {
            printf("  Source #%d: %s\n", sources[i].sourceId, (sourceName == NULL ? "" : sourceName));
            sourceId = sources[i].sourceId;
        }

        printf("     %d: ", sources[i].streamId);
        if (sources[i].isDisabled)
        {
            printf("(disabled) ");
        }

        switch (sources[i].streamInfo->type)
        {
            case PICTURE_STREAM_TYPE:
                printf("Picture");
                break;
            case SOUND_STREAM_TYPE:
                printf("Sound");
                break;
            case TIMECODE_STREAM_TYPE:
                printf("Timecode");
                break;
            case EVENT_STREAM_TYPE:
                printf("Event");
                break;
            case UNKNOWN_STREAM_TYPE:
                printf("Unknown type");
                break;
        }
        if (sources[i].streamInfo->format != TIMECODE_FORMAT)  /* see below for reason */
        {
            printf(", ");
        }

        switch (sources[i].streamInfo->format)
        {
            case UYVY_FORMAT:
                printf("UYVY");
                break;
            case UYVY_10BIT_FORMAT:
                printf("UYVY 10 bit");
                break;
            case YUV420_FORMAT:
                printf("YUV420");
                break;
            case YUV411_FORMAT:
                printf("YUV411");
                break;
            case YUV422_FORMAT:
                printf("YUV422");
                break;
            case YUV444_FORMAT:
                printf("YUV444");
                break;
            case DV25_YUV420_FORMAT:
                printf("DV25 YUV420");
                break;
            case DV25_YUV411_FORMAT:
                printf("DV25 YUV411");
                break;
            case DV50_FORMAT:
                printf("DV50");
                break;
            case DV100_1080I_FORMAT:
                printf("DV100 1080i");
                break;
            case DV100_720P_FORMAT:
                printf("DV100 720p");
                break;
            case D10_PICTURE_FORMAT:
                printf("D10 Picture");
                break;
            case AVID_MJPEG_FORMAT:
                printf("AVID MJPEG");
                break;
            case AVID_DNxHD_FORMAT:
                printf("AVID DNxHD");
                break;
            case PCM_FORMAT:
                printf("PCM");
                break;
            case TIMECODE_FORMAT:
                /* omitting "Timecode" because it duplicates the streamInfo->type */
                /* printf("Timecode"); */
                break;
            case SOURCE_EVENT_FORMAT:
                printf("Source Event");
                break;
            case UNKNOWN_FORMAT:
                printf("Unknown format");
                break;
        }

        switch (sources[i].streamInfo->type)
        {
            case PICTURE_STREAM_TYPE:
                printf(", ");
                printf("%d/%d fps, ", sources[i].streamInfo->frameRate.num, sources[i].streamInfo->frameRate.den);
                printf("%dx%d, ", sources[i].streamInfo->width, sources[i].streamInfo->height);
                printf("%d:%d", sources[i].streamInfo->aspectRatio.num, sources[i].streamInfo->aspectRatio.den);
                break;
            case SOUND_STREAM_TYPE:
                printf(", ");
                printf("%d/%d Hz, ", sources[i].streamInfo->samplingRate.num, sources[i].streamInfo->samplingRate.den);
                printf("%d channel%s, ", sources[i].streamInfo->numChannels, (sources[i].streamInfo->numChannels > 1 ? "s" : ""));
                printf("%d bits/sample", sources[i].streamInfo->bitsPerSample);
                break;
            case TIMECODE_STREAM_TYPE:
                printf(", ");
                if (sources[i].streamInfo->timecodeType == SOURCE_TIMECODE_TYPE)
                {
                    switch (sources[i].streamInfo->timecodeSubType)
                    {
                        case VITC_SOURCE_TIMECODE_SUBTYPE:
                            printf("VITC");
                            break;
                        case LTC_SOURCE_TIMECODE_SUBTYPE:
                            printf("LTC");
                            break;
                        case DVITC_SOURCE_TIMECODE_SUBTYPE:
                            printf("DVITC");
                            break;
                        case DLTC_SOURCE_TIMECODE_SUBTYPE:
                            printf("DLTC");
                            break;
                        case NO_TIMECODE_SUBTYPE:
                            printf("source");
                            break;
                    }
                }
                else
                {
                    switch (sources[i].streamInfo->timecodeType)
                    {
                        case SYSTEM_TIMECODE_TYPE:
                            printf("system");
                            break;
                        case CONTROL_TIMECODE_TYPE:
                            printf("control");
                            break;
                        case SOURCE_TIMECODE_TYPE:
                            printf("source");
                            break;
                        case UNKNOWN_TIMECODE_TYPE:
                            printf("unknown type");
                            break;
                    }
                    if (sources[i].streamInfo->timecodeSubType != NO_TIMECODE_SUBTYPE)
                    {
                        printf(", ");
                    }
                    switch (sources[i].streamInfo->timecodeSubType)
                    {
                        case VITC_SOURCE_TIMECODE_SUBTYPE:
                            printf("VITC");
                            break;
                        case LTC_SOURCE_TIMECODE_SUBTYPE:
                            printf("LTC");
                            break;
                        case DVITC_SOURCE_TIMECODE_SUBTYPE:
                            printf("DVITC");
                            break;
                        case DLTC_SOURCE_TIMECODE_SUBTYPE:
                            printf("DLTC");
                            break;
                        case NO_TIMECODE_SUBTYPE:
                            break;
                    }
                }
                break;
            case EVENT_STREAM_TYPE:
                break;
            case UNKNOWN_STREAM_TYPE:
                break;
        }

        printf("\n");
    }

    SAFE_FREE(&sources);
    return;

fail:
    SAFE_FREE(&sources);
}

void ply_get_frame_rate(MediaPlayer* player, Rational* frameRate)
{
    *frameRate = player->frameRate;
}

void ply_set_qc_quit_validator(MediaPlayer* player, qc_quit_validator_func func, void* data)
{
    player->qcQuitValidator = func;
    player->qcQuitValidatorData = data;
}

int ply_qc_quit_validate(MediaPlayer* player)
{
    int64_t availableSourceLength;

    /* check 1: media source is complete */

    if (msc_get_available_length(player->mediaSource, &availableSourceLength))
    {
        if (availableSourceLength < player->sourceLength)
        {
            return 1;
        }
    }
    else
    {
        ml_log_warn("Failed to get available source length when doing qc quit validation\n");
    }


    /* check 2: clip out-point is set */

    if (mpm_clip_mark_active(&player->playerMarks))
    {
        return 2;
    }


    /* check 3: programme start and end are set */

    if (!mpm_have_clip_mark(&player->playerMarks))
    {
        return 3;
    }

    return 0;
}

void ply_activate_qc_mark_validation(MediaPlayer* player)
{
    player->qcValidateMark = 1;
}

