/*
 * $Id: video_switch_sink.h,v 1.12 2011/10/27 13:45:37 philipn Exp $
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

#ifndef __VIDEO_SWITCH_SINK_H__
#define __VIDEO_SWITCH_SINK_H__



#include "media_sink.h"
#include "video_switch_database.h"


#define MAX_SPLIT_COUNT                 9



struct VideoSwitchSink
{
    void* data;

    MediaSink* (*get_media_sink)(void* data);

    int (*switch_next_video)(void* data);
    int (*switch_prev_video)(void* data);
    int (*switch_video)(void* data, int index);
    void (*show_source_name)(void* data, int enable);
    void (*toggle_show_source_name)(void* data);
    int (*get_video_index)(void* data, int* index);
    int (*get_video_index_at_pos)(void* data, int imageWidth, int imageHeight, int xPos, int yPos, int* index);
    int (*get_active_clip_ids)(void *data, char clipIds[MAX_SPLIT_COUNT][CLIP_ID_SIZE], int sourceIds[MAX_SPLIT_COUNT],
                               int *numIds);
};

/* utility functions for calling VideoSwitchSink functions */

MediaSink* vsw_get_media_sink(VideoSwitchSink* swtch);
int vsw_switch_next_video(VideoSwitchSink* swtch);
int vsw_switch_prev_video(VideoSwitchSink* swtch);
int vsw_switch_video(VideoSwitchSink* swtch, int index);
void vsw_show_source_name(VideoSwitchSink* swtch, int enable);
void vsw_toggle_show_source_name(VideoSwitchSink* swtch);
int vsw_get_video_index(VideoSwitchSink* swtch, int* index);
int vsw_get_video_index_at_pos(VideoSwitchSink* swtch, int imageWidth, int imageHeight, int xPos, int yPos, int* index);
int vsw_get_active_clip_ids(VideoSwitchSink *swtch, char clipId[MAX_SPLIT_COUNT][CLIP_ID_SIZE],
                            int sourceId[MAX_SPLIT_COUNT], int *numIds);


/* video switch */

typedef enum
{
    NO_SPLIT_VIDEO_SWITCH = 1,
    QUAD_SPLIT_VIDEO_SWITCH = 4,
    NONA_SPLIT_VIDEO_SWITCH = 9
} VideoSwitchSplit;

int qvs_create_video_switch(MediaSink* sink, VideoSwitchSplit split, int splitSelect, int prescaledSplit,
    VideoSwitchDatabase* database, int masterTimecodeIndex, int masterTimecodeType, int masterTimecodeSubType,
    VideoSwitchSink** swtch);

void qvs_set_slave_video_switch(VideoSwitchSink *swtch, VideoSwitchSink *slave_swtch);



#endif


