/*
 * $Id: media_player_marks.h,v 1.1 2010/06/02 11:12:14 philipn Exp $
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

#ifndef __MEDIA_PLAYER_MARKS_H__
#define __MEDIA_PLAYER_MARKS_H__

#include "on_screen_display.h"



/* BasicMark struct must match Mark struct defined in media_player.h */
typedef struct
{
    int64_t position;
    unsigned int type;
    int64_t pairedPosition;
} BasicMark;

typedef struct TimelineMark
{
    struct TimelineMark *next;
    struct TimelineMark *prev;
    struct TimelineMark *pairedMark;

    int64_t position;
    unsigned int type;

    uint8_t vtrErrorCode;
} TimelineMark;

typedef struct
{
    TimelineMark *first;
    TimelineMark *current;
    TimelineMark *last;
    int count;

    unsigned int typeMask;
    
    int64_t sourceLength;

    OSDMarksModel *marksModel;
    unsigned int modelTypeMask;
    VTRErrorLevel modelVTRErrorLevel;
} TimelineMarks;

typedef struct
{
    pthread_mutex_t mutex;

    OnScreenDisplay *osd;
    
    int numMarkSelections;
    int activeMarkSelection;
    TimelineMarks timelineMarks[MAX_MARK_SELECTIONS];

    unsigned int clipMarkType;
    TimelineMark *currentClipMark;
} MediaPlayerMarks;


int mpm_init_player_marks(MediaPlayerMarks *playerMarks, OnScreenDisplay *osd, int64_t sourceLength,
                          unsigned int *markSelectionTypeMasks, int numMarkSelections);
void mpm_clear_player_marks(MediaPlayerMarks *playerMarks, OnScreenDisplay *osd);

int mpm_get_num_mark_selections(MediaPlayerMarks *playerMarks);

void mpm_set_source_length(MediaPlayerMarks *playerMarks, int64_t sourceLength);

void mpm_set_mark_filter(MediaPlayerMarks *playerMarks, int selection, unsigned int typeMask);
void mpm_set_vtr_error_level_filter(MediaPlayerMarks *playerMarks, VTRErrorLevel level);

void mpm_enable_clip_marks(MediaPlayerMarks *playerMarks, unsigned int markType);

int mpm_clip_mark_active(MediaPlayerMarks *playerMarks);
int mpm_have_clip_mark(MediaPlayerMarks *playerMarks);

void mpm_add_mark(MediaPlayerMarks *playerMarks, int64_t position, unsigned int type, int toggle);
void mpm_active_add_mark(MediaPlayerMarks *playerMarks, int64_t position, unsigned int type, int toggle);
void mpm_add_vtr_error_mark(MediaPlayerMarks *playerMarks, int64_t position, int toggle, uint8_t errorCode);

void mpm_remove_mark(MediaPlayerMarks *playerMarks, int64_t position, unsigned int typeMask);
void mpm_active_remove_mark(TimelineMarks *marks, int64_t position, unsigned int typeMask);

void mpm_free_marks(MediaPlayerMarks *playerMarks, unsigned int typeMask);

int64_t mpm_active_find_next_mark(MediaPlayerMarks *playerMarks, int64_t currentPosition);
int64_t mpm_active_find_prev_mark(MediaPlayerMarks *playerMarks, int64_t currentPosition);
int64_t mpm_find_clip_mark(MediaPlayerMarks *playerMarks, int64_t currentPosition);

void mpm_update_current_mark(MediaPlayerMarks *playerMarks, int64_t position, unsigned int *markTypes,
                             unsigned int *markTypeMasks, uint8_t *vtrErrorCode);
unsigned int mpm_get_current_mark_type(MediaPlayerMarks *playerMarks, int64_t position);

void mpm_next_active_mark_selection(MediaPlayerMarks *playerMarks);

int mpm_get_marks(MediaPlayerMarks *playerMarks, BasicMark **marks);



#endif
