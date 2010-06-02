/*
 * $Id: media_player_marks.c,v 1.1 2010/06/02 11:12:14 philipn Exp $
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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "media_player_marks.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"



static void set_current_clip_mark(MediaPlayerMarks *playerMarks, TimelineMark *mark)
{
    osd_highlight_progress_bar_pointer(playerMarks->osd, mark != NULL);

    playerMarks->currentClipMark = mark;
}

static void clear_marks_model(TimelineMarks *marks)
{
    if (!marks->marksModel)
        return;

    PTHREAD_MUTEX_LOCK(&marks->marksModel->marksMutex)
    memset(marks->marksModel->markCounts, 0, marks->marksModel->numMarkCounts * sizeof(int));
    marks->marksModel->updated = -1; /* set all bits to 1 */
    PTHREAD_MUTEX_UNLOCK(&marks->marksModel->marksMutex)
}

static void add_mark_to_marks_model(TimelineMarks *marks, unsigned int type, int64_t position)
{
    int index;

    if (marks->sourceLength <= 0 || position >= marks->sourceLength ||
        (type & marks->typeMask) == 0 || (type & marks->modelTypeMask) == 0)
    {
        return;
    }

    PTHREAD_MUTEX_LOCK(&marks->marksModel->marksMutex)
    if (marks->marksModel->numMarkCounts > 0) {
        index = marks->marksModel->numMarkCounts * position / marks->sourceLength;
        if (marks->marksModel->markCounts[index] == 0)
            marks->marksModel->updated = -1; /* set all bits to 1 */
        marks->marksModel->markCounts[index]++;
    }
    PTHREAD_MUTEX_UNLOCK(&marks->marksModel->marksMutex)
}

static void remove_mark_from_marks_model(TimelineMarks *marks, unsigned int type, int64_t position)
{
    int index;

    if (marks->sourceLength <= 0 || position >= marks->sourceLength || !(type & marks->typeMask))
        return;

    PTHREAD_MUTEX_LOCK(&marks->marksModel->marksMutex)
    if (marks->marksModel->numMarkCounts > 0) {
        index = marks->marksModel->numMarkCounts * position / marks->sourceLength;
        if (marks->marksModel->markCounts[index] == 1)
            marks->marksModel->updated = -1; /* set all bits to 1 */
        if (marks->marksModel->markCounts[index] > 0)
            marks->marksModel->markCounts[index]--;
    }
    PTHREAD_MUTEX_UNLOCK(&marks->marksModel->marksMutex)
}

static void update_mark_and_model(TimelineMarks *marks, TimelineMark *mark, unsigned int type)
{
    int i;
    unsigned int mask;

    if (mark->type == type)
        return;

    /* add or remove mark from OSD marks model */

    unsigned int add = type & (mark->type ^ type);
    if ((add & VTR_ERROR_MARK_TYPE) &&
        (marks->modelVTRErrorLevel == VTR_NO_ERROR_LEVEL ||
            mark->vtrErrorCode < (uint8_t)marks->modelVTRErrorLevel))
    {
        add &= ~VTR_ERROR_MARK_TYPE; /* don't add VTR error mark if no errors shown or code below level */
    }
    unsigned int remove = mark->type & (mark->type ^ type);

    if (add) {
        mask = 0x00000001;
        for (i = 0; i < 32; i++) {
            if ((add & mask) != 0)
                add_mark_to_marks_model(marks, add & mask, mark->position);
            mask <<= 1;
        }
    }
    if (remove) {
        mask = 0x00000001;
        for (i = 0; i < 32; i++) {
            if ((remove & mask) != 0)
                remove_mark_from_marks_model(marks, remove & mask, mark->position);
            mask <<= 1;
        }
    }


    mark->type = type;
    if (!(type & VTR_ERROR_MARK_TYPE))
        mark->vtrErrorCode = 0;
}

static void rebuild_marks_model(TimelineMarks *marks)
{
    TimelineMark *mark;
    int index;
    unsigned int mask;
    unsigned int mark_type;
    int i;

    if (!marks->marksModel)
        return;

    PTHREAD_MUTEX_LOCK(&marks->marksModel->marksMutex)

    /* clear the marks */

    memset(marks->marksModel->markCounts, 0, marks->marksModel->numMarkCounts * sizeof(int));

    /* add marks to OSD marks model */

    if (marks->sourceLength > 0 && marks->marksModel->numMarkCounts > 0) {
        mark = marks->first;
        while (mark) {
            mark_type = (mark->type & marks->typeMask & marks->modelTypeMask);
            if ((mark_type & VTR_ERROR_MARK_TYPE) &&
                (marks->modelVTRErrorLevel == VTR_NO_ERROR_LEVEL ||
                    mark->vtrErrorCode < (uint8_t)marks->modelVTRErrorLevel))
            {
                mark_type &= ~VTR_ERROR_MARK_TYPE; /* don't add VTR error mark if no errors shown or code below level */
            }

            if (mark->position < marks->sourceLength && mark_type != 0) {
                mask = 0x00000001;
                for (i = 0; i < 32; i++) {
                    if ((mask & mark_type) != 0) {
                        index = marks->marksModel->numMarkCounts * mark->position / marks->sourceLength;
                        marks->marksModel->markCounts[index]++;
                    }

                    mask <<= 1;
                }
            }

            mark = mark->next;
        }
    }

    /* signal that marks have been updated */
    marks->marksModel->updated = -1; /* set all bits to 1 */

    PTHREAD_MUTEX_UNLOCK(&marks->marksModel->marksMutex)
}



static void _remove_mark(MediaPlayerMarks *playerMarks, TimelineMarks *marks, TimelineMark *mark)
{
    /* remove from list */
    if (mark->prev)
        mark->prev->next = mark->next;
    if (mark->next)
        mark->next->prev = mark->prev;

    /* reset first, current, last if neccessary */
    if (mark == marks->first)
        marks->first = mark->next;
    if (mark == marks->current) {
        if (mark->prev != NULL)
            marks->current = mark->prev;
        else
            marks->current = mark->next;
    }
    if (mark == marks->last)
        marks->last = mark->prev;

    /* reset current clip mark */
    if (playerMarks->currentClipMark == mark)
        set_current_clip_mark(playerMarks, NULL);

    marks->count--;
    free(mark);
}

static int find_mark(TimelineMarks *marks, int64_t position, TimelineMark **matchedMark)
{
    TimelineMark *mark;

    /* start the search from the current mark, otherwise the first */
    mark = marks->current;
    if (!mark)
        mark = marks->first;

    /* try search forwards and if neccessary backwards */
    while (mark && mark->position < position)
        mark = mark->next;
    while (mark && mark->position > position)
        mark = mark->prev;

    if (!mark || mark->position != position)
        return 0;

    *matchedMark = mark;
    return 1;
}

/* mark is only removed if the resulting type == 0 */
static void remove_mark(MediaPlayerMarks *playerMarks, TimelineMarks *marks, int64_t position, unsigned int typeMask)
{
    TimelineMark *mark = NULL;

    /* limit mask to the clip mark if a clip mark is active */
    if (playerMarks->currentClipMark)
        typeMask &= playerMarks->clipMarkType;

    if (typeMask == 0)
        return;

    if (find_mark(marks, position, &mark)) {
        update_mark_and_model(marks, mark, mark->type & ~typeMask);

        if ((mark->type & playerMarks->clipMarkType) == 0) {
            if (mark->pairedMark) {
                /* remove the clip mark pair */
                mark->pairedMark->pairedMark = NULL;
                update_mark_and_model(marks, mark->pairedMark, mark->pairedMark->type & ~playerMarks->clipMarkType);
                if (mark->pairedMark->type == 0)
                    _remove_mark(playerMarks, marks, mark->pairedMark);
                mark->pairedMark = NULL;

                /* deactivate clip mark */
                /* TODO: not neccessary because mark already paired and not active? */
                set_current_clip_mark(playerMarks, NULL);
            } else if (mark == playerMarks->currentClipMark) {
                /* deactivate clip mark */
                set_current_clip_mark(playerMarks, NULL);
            }
        }

        if (mark->type == 0) {
            _remove_mark(playerMarks, marks, mark);
            mark = NULL;
        }
    }
}

static void free_marks(MediaPlayerMarks *playerMarks, TimelineMarks *marks, unsigned int typeMask)
{
    TimelineMark *mark;
    TimelineMark *nextMark;

    /* don't allow all marks to be cleared when busy with clip mark */
    if (playerMarks->currentClipMark)
        return;

    if ((typeMask & ALL_MARK_TYPE) == ALL_MARK_TYPE) {
        /* clear marks */
        mark = marks->first;
        while (mark) {
            nextMark = mark->next;
            free(mark);
            mark = nextMark;
        }
        marks->first = NULL;
        marks->current = NULL;
        marks->last = NULL;
        marks->count = 0;

        clear_marks_model(marks);
    } else {
        /* go through one by one and remove marks with type not masked */
        mark = marks->first;
        while (mark) {
            nextMark = mark->next;

            update_mark_and_model(marks, mark, mark->type & ~typeMask);

            /* handle clip marks */
            if (mark->pairedMark) {
                /* remove paired clip mark */
                mark->pairedMark->pairedMark = NULL;
                update_mark_and_model(marks, mark->pairedMark, mark->pairedMark->type & ~playerMarks->clipMarkType);
                if (mark->pairedMark->type == 0)
                    _remove_mark(playerMarks, marks, mark->pairedMark);
                mark->pairedMark = NULL;
            }

            if (mark->type == 0) {
                _remove_mark(playerMarks, marks, mark);
                mark = NULL;
            }

            mark = nextMark;
        }
    }
}

/* mark is removed if the resulting type == 0 */
static int add_mark(MediaPlayerMarks *playerMarks, TimelineMarks *marks, int64_t position, unsigned int type, int toggle,
                    uint8_t vtrErrorCode, TimelineMark **addedMarkOut)
{
    TimelineMark *mark;
    TimelineMark *newMark;
    TimelineMark *addedMark = NULL;

    if (position < 0 || type == 0)
        return 0;

    /* find the insertion point */
    mark = marks->current;
    if (!mark)
        mark = marks->first;
    while (mark && mark->position < position)
        mark = mark->next;
    while (mark && mark->position > position)
        mark = mark->prev;


    if (mark && mark->position == position) {
        /* add type to existing mark */

        /* process clip marks first */
        if ((playerMarks->clipMarkType & type) != 0) {
            if (playerMarks->currentClipMark)
            {
                /* busy with a clip mark */

                if (mark == playerMarks->currentClipMark) {
                    if (toggle) {
                        /* remove the clip mark */
                        update_mark_and_model(marks, mark, mark->type & ~playerMarks->clipMarkType);
                        if (mark->type == 0) {
                            _remove_mark(playerMarks, marks, mark);
                            mark = NULL;
                        }

                        /* done with clip mark */
                        set_current_clip_mark(playerMarks, NULL);
                    }
                    /* else ignore the request to complete the clip with length 0 */
                } else if (!mark->pairedMark) {
                    /* complete the clip mark */
                    update_mark_and_model(marks, mark, mark->type | (playerMarks->clipMarkType & ALL_MARK_TYPE));
                    playerMarks->currentClipMark->pairedMark = mark;
                    mark->pairedMark = playerMarks->currentClipMark;

                    /* done with clip mark */
                    set_current_clip_mark(playerMarks, NULL);
                }
                /* else ignore the request to complete a clip mark at an existing clip mark */
            } else {
                /* not busy with a clip mark */

                if ((mark->type & playerMarks->clipMarkType) == 0) {
                    /* start clip mark */
                    update_mark_and_model(marks, mark, mark->type | (playerMarks->clipMarkType & ALL_MARK_TYPE));
                    set_current_clip_mark(playerMarks, mark);
                } else if (toggle) {
                    /* remove clip mark */

                    if (mark->pairedMark != NULL) {
                        mark->pairedMark->pairedMark = NULL;
                        update_mark_and_model(marks, mark->pairedMark, mark->pairedMark->type & ~playerMarks->clipMarkType);
                        if (mark->pairedMark->type == 0)
                            _remove_mark(playerMarks, marks, mark->pairedMark);
                        mark->pairedMark = NULL;
                    }

                    update_mark_and_model(marks, mark, mark->type & ~playerMarks->clipMarkType);
                    if (mark->type == 0) {
                        _remove_mark(playerMarks, marks, mark);
                        mark = NULL;
                    }
                }
                /* else ignore the request to start a clip mark on an existing one */
            }

            /* done with clip marks */
            type &= ~playerMarks->clipMarkType;
        }

        /* deal with non-clip marks */

        if (mark) {
            if (type & VTR_ERROR_MARK_TYPE)
                mark->vtrErrorCode = vtrErrorCode;

            if (toggle)
                update_mark_and_model(marks, mark, mark->type ^ (type & ALL_MARK_TYPE));
            else
                update_mark_and_model(marks, mark, mark->type | (type & ALL_MARK_TYPE));

            if (mark->type & type) {
                /* mark was added or was already present and !toggle */
                addedMark = mark;
            }

            /* remove mark if type == 0 */
            if (mark->type == 0) {
                _remove_mark(playerMarks, marks, mark);
                mark = NULL;
            }
        }
    } else {
        /* new mark */
        CALLOC_OFAIL(newMark, TimelineMark, 1);
        newMark->type = 0; /* set in update_mark_and_model call below */
        newMark->position = position;
        newMark->vtrErrorCode = vtrErrorCode;

        addedMark = newMark;

        update_mark_and_model(marks, newMark, type);

        if (mark) {
            /* insert before or after mark */

            if (mark->position > position) {
                /* insert before */
                if (mark->prev)
                    mark->prev->next = newMark;
                else
                    marks->first = newMark;
                newMark->prev = mark->prev;
                newMark->next = mark;
                mark->prev = newMark;
            } else {
                /* insert after */
                if (mark->next)
                    mark->next->prev = newMark;
                else
                    marks->last = newMark;
                newMark->next = mark->next;
                newMark->prev = mark;
                mark->next = newMark;
            }
        } else {
            /* append or prepend */

            if (!marks->first) {
                /* new first and last */
                marks->first = newMark;
                marks->last = newMark;
            } else if (marks->first->position > position) {
                /* new first */
                newMark->next = marks->first;
                marks->first = newMark;
                marks->first->next->prev = marks->first;
            } else {
                /* new last */
                newMark->prev = marks->last;
                marks->last = newMark;
                marks->last->prev->next = marks->last;
            }
        }

        /* handle clip marks */
        if ((playerMarks->clipMarkType & type) != 0) {
            if (playerMarks->currentClipMark) {
                /* complete the clip mark */
                playerMarks->currentClipMark->pairedMark = newMark;
                newMark->pairedMark = playerMarks->currentClipMark;
                set_current_clip_mark(playerMarks, NULL);
            } else {
                /* start the clip mark */
                set_current_clip_mark(playerMarks, newMark);
            }
        }

        marks->count++;
    }


    *addedMarkOut = addedMark;

    return 1;

fail:
    return 0;
}

static int mark_is_visible(TimelineMarks *marks, TimelineMark *mark)
{
    return (mark->type & marks->modelTypeMask) &&
           ((mark->type & marks->modelTypeMask) != VTR_ERROR_MARK_TYPE ||
               (marks->modelVTRErrorLevel != VTR_NO_ERROR_LEVEL &&
                   mark->vtrErrorCode >= (uint8_t)marks->modelVTRErrorLevel));
}

static int find_next_mark(TimelineMarks *marks, int64_t position, int visible_only, TimelineMark **matchedMark)
{
    TimelineMark *mark;

    /* start the search from the current mark, otherwise the first */
    mark = marks->current;
    if (!mark)
        mark = marks->first;

    while (mark) {
        if (mark->position == position) {
            /* either found it or is NULL */
            mark = mark->next;
            break;
        } else if (mark->position > position) {
            if (!mark->prev || mark->prev->position <= position) {
                break; /* found it */
            }
            else { /* mark->prev->position > position */
                /* go further backwards */
                mark = mark->prev;
            }
        } else { /* mark->position < position */
            /* go further fowards */
            mark = mark->next;
        }
    }

    /* position at a visible mark */
    if (visible_only) {
        while (mark && !mark_is_visible(marks, mark))
            mark = mark->next;
    }

    if (!mark)
        return 0;

    *matchedMark = mark;
    return 1;
}

static int find_prev_mark(TimelineMarks *marks, int64_t position, int visible_only, TimelineMark** matchedMark)
{
    TimelineMark *mark;

    /* start the search from the current mark, otherwise the first */
    mark = marks->current;
    if (!mark)
        mark = marks->first;

    while (mark) {
        if (mark->position == position) {
            /* found it or is NULL */
            mark = mark->prev;
            break;
        } else if (mark->position < position) {
            if (!mark->next || mark->next->position >= position){
                /* found it */
                break;
            } else { /* mark->next->position < position */
                /* go further forwards */
                mark = mark->next;
            }
        } else { /* mark->position > position */
            /* go further backwards */
            mark = mark->prev;
        }
    }

    /* position at a visible mark */
    if (visible_only) {
        while (mark && !mark_is_visible(marks, mark))
            mark = mark->prev;
    }

    if (!mark)
        return 0;

    *matchedMark = mark;
    return 1;
}



int mpm_init_player_marks(MediaPlayerMarks *playerMarks, OnScreenDisplay *osd, int64_t sourceLength,
                          unsigned int *markSelectionTypeMasks, int numMarkSelections)
{
    int i;

    memset(playerMarks, 0, sizeof(*playerMarks));
    playerMarks->osd = osd;
    if (numMarkSelections == 0) {
        playerMarks->numMarkSelections = 1;
        for (i = 0; i < playerMarks->numMarkSelections; i++)
            playerMarks->timelineMarks[i].typeMask = ALL_MARK_TYPE;
    } else {
        playerMarks->numMarkSelections = numMarkSelections;
        for (i = 0; i < playerMarks->numMarkSelections; i++)
            playerMarks->timelineMarks[i].typeMask = markSelectionTypeMasks[i];
    }
    for (i = 0; i < playerMarks->numMarkSelections; i++) {
        playerMarks->timelineMarks[i].sourceLength = sourceLength;
        playerMarks->timelineMarks[i].modelTypeMask = ALL_MARK_TYPE;
        playerMarks->timelineMarks[i].modelVTRErrorLevel = VTR_NO_ERROR_LEVEL;
    }

    CHK_ORET(init_mutex(&playerMarks->mutex));

    if (playerMarks->osd) {
        if (playerMarks->numMarkSelections >= 1) {
            CHK_OFAIL(osd_create_marks_model(playerMarks->osd, &playerMarks->timelineMarks[0].marksModel));
            osd_set_marks_model(playerMarks->osd, 0x01, playerMarks->timelineMarks[0].marksModel);
        }
        if (playerMarks->numMarkSelections >= 2) {
            CHK_OFAIL(osd_create_marks_model(playerMarks->osd, &playerMarks->timelineMarks[1].marksModel));
            osd_set_second_marks_model(playerMarks->osd, 0x01, playerMarks->timelineMarks[1].marksModel);
        }
    }

    return 1;

fail:
    mpm_clear_player_marks(playerMarks, osd);
    return 0;
}

void mpm_clear_player_marks(MediaPlayerMarks *playerMarks, OnScreenDisplay *osd)
{
    int i;

    /* Note: not relying on playerMarks->osd because the OSD could have been deleted already */

    for (i = 0; i < playerMarks->numMarkSelections; i++) {
        free_marks(playerMarks, &playerMarks->timelineMarks[i], ALL_MARK_TYPE);
        if (osd && playerMarks->timelineMarks[i].marksModel)
            osd_free_marks_model(osd, &playerMarks->timelineMarks[i].marksModel);
    }

    destroy_mutex(&playerMarks->mutex);

    memset(playerMarks, 0, sizeof(*playerMarks));
}

int mpm_get_num_mark_selections(MediaPlayerMarks *playerMarks)
{
    return playerMarks->numMarkSelections;
}

void mpm_set_source_length(MediaPlayerMarks *playerMarks, int64_t sourceLength)
{
    int i;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    for (i = 0; i < playerMarks->numMarkSelections; i++)
        playerMarks->timelineMarks[i].sourceLength = sourceLength;
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)
}

void mpm_set_mark_filter(MediaPlayerMarks *playerMarks, int selection, unsigned int typeMask)
{
    if (selection < 0 || selection > playerMarks->numMarkSelections)
        return;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    playerMarks->timelineMarks[selection].modelTypeMask = typeMask;
    rebuild_marks_model(&playerMarks->timelineMarks[selection]);
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)
}

void mpm_set_vtr_error_level_filter(MediaPlayerMarks *playerMarks, VTRErrorLevel level)
{
    int i;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    for (i = 0; i < playerMarks->numMarkSelections; i++) {
        playerMarks->timelineMarks[i].modelVTRErrorLevel = level;
        rebuild_marks_model(&playerMarks->timelineMarks[i]);
    }
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)
}

void mpm_enable_clip_marks(MediaPlayerMarks *playerMarks, unsigned int markType)
{
    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    playerMarks->clipMarkType = markType;
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)
}

int mpm_clip_mark_active(MediaPlayerMarks *playerMarks)
{
    int isActive;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    isActive = (playerMarks->currentClipMark != NULL);
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)

    return isActive;
}

int mpm_have_clip_mark(MediaPlayerMarks *playerMarks)
{
    TimelineMark *mark;
    int haveClipMark = 0;
    int i;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    for (i = 0; i < playerMarks->numMarkSelections && !haveClipMark; i++) {
        mark = playerMarks->timelineMarks[i].first;
        while (mark && !haveClipMark) {
            haveClipMark = (mark->pairedMark != NULL);
            mark = mark->next;
        }
    }
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)

    return haveClipMark;
}

void mpm_add_mark(MediaPlayerMarks *playerMarks, int64_t position, unsigned int type, int toggle)
{
    TimelineMark *addedMark;
    unsigned int selectionType;
    int i;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    for (i = 0; i < playerMarks->numMarkSelections; i++) {
        selectionType = type & playerMarks->timelineMarks[i].typeMask;
        if (selectionType != 0)
            add_mark(playerMarks, &playerMarks->timelineMarks[i], position, selectionType, toggle, 0, &addedMark);
    }
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)
}

void mpm_active_add_mark(MediaPlayerMarks *playerMarks, int64_t position, unsigned int type, int toggle)
{
    TimelineMark *addedMark;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    add_mark(playerMarks, &playerMarks->timelineMarks[playerMarks->activeMarkSelection],
             position, type, toggle, 0, &addedMark);
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)
}

void mpm_add_vtr_error_mark(MediaPlayerMarks *playerMarks, int64_t position, int toggle, uint8_t errorCode)
{
    TimelineMark *addedMark;
    unsigned int selectionType;
    int i;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    for (i = 0; i < playerMarks->numMarkSelections; i++) {
        selectionType = VTR_ERROR_MARK_TYPE & playerMarks->timelineMarks[i].typeMask;
        if (selectionType != 0) {
            add_mark(playerMarks, &playerMarks->timelineMarks[i], position, VTR_ERROR_MARK_TYPE, toggle,
                     errorCode, &addedMark);
        }
    }
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)
}

void mpm_remove_mark(MediaPlayerMarks *playerMarks, int64_t position, unsigned int typeMask)
{
    unsigned int selectionTypeMask;
    int i;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    for (i = 0; i < playerMarks->numMarkSelections; i++) {
        selectionTypeMask = typeMask & playerMarks->timelineMarks[i].typeMask;
        if (selectionTypeMask != 0)
            remove_mark(playerMarks, &playerMarks->timelineMarks[i], position, selectionTypeMask);
    }
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)
}

void mpm_active_remove_mark(MediaPlayerMarks *playerMarks, int64_t position, unsigned int typeMask)
{
    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    remove_mark(playerMarks, &playerMarks->timelineMarks[playerMarks->activeMarkSelection], position, typeMask);
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)
}

void mpm_free_marks(MediaPlayerMarks *playerMarks, unsigned int typeMask)
{
    int i;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    for (i = 0; i < playerMarks->numMarkSelections; i++)
        free_marks(playerMarks, &playerMarks->timelineMarks[i], typeMask);
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)
}

int64_t mpm_active_find_next_mark(MediaPlayerMarks *playerMarks, int64_t currentPosition)
{
    int64_t position = -1;
    TimelineMark *matchedMark;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    if (find_next_mark(&playerMarks->timelineMarks[playerMarks->activeMarkSelection], currentPosition, 1, &matchedMark))
        position = matchedMark->position;
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)

    return position;
}

int64_t mpm_active_find_prev_mark(MediaPlayerMarks *playerMarks, int64_t currentPosition)
{
    int64_t position = -1;
    TimelineMark *matchedMark;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    if (find_prev_mark(&playerMarks->timelineMarks[playerMarks->activeMarkSelection], currentPosition, 1, &matchedMark))
        position = matchedMark->position;
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)

    return position;
}

int64_t mpm_find_clip_mark(MediaPlayerMarks *playerMarks, int64_t currentPosition)
{
    int64_t position = -1;
    int i;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    if (playerMarks->currentClipMark) {
        position = playerMarks->currentClipMark->position;
    } else {
        /* check whether the player is on a clip mark */
        for (i = 0; i < playerMarks->numMarkSelections; i++) {
            if (playerMarks->timelineMarks[i].current &&
                playerMarks->timelineMarks[i].current->position == currentPosition &&
                playerMarks->timelineMarks[i].current->pairedMark)
            {
                /* player is on a clip mark - return the paired clip mark position */
                position = playerMarks->timelineMarks[i].current->pairedMark->position;
                break;
            }
        }
    }
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)

    return position;
}

void mpm_update_current_mark(MediaPlayerMarks *playerMarks, int64_t position, unsigned int *markTypes,
                             unsigned int *markTypeMasks, uint8_t *vtrErrorCode)
{
    TimelineMark *currentMark;
    int i;

    for (i = 0; i < playerMarks->numMarkSelections; i++) {
        markTypes[i] = 0;
        markTypeMasks[i] = 0;
    }
    *vtrErrorCode = 0;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)

    /* update current mark */
    for (i = 0; i < playerMarks->numMarkSelections; i++) {
        if (find_mark(&playerMarks->timelineMarks[i], position, &currentMark))
            playerMarks->timelineMarks[i].current = currentMark;
    }

    /* get current mark types and masks */
    for (i = 0; i < playerMarks->numMarkSelections; i++) {
        if (playerMarks->timelineMarks[i].current &&
            playerMarks->timelineMarks[i].current->position == position)
        {
            markTypes[i] = playerMarks->timelineMarks[i].current->type;
        }
        markTypeMasks[i] = playerMarks->timelineMarks[i].modelTypeMask;
    }

    /* get current vtr error code */
    for (i = 0; i < playerMarks->numMarkSelections; i++) {
        if (playerMarks->timelineMarks[i].current &&
            playerMarks->timelineMarks[i].current->position == position &&
            (playerMarks->timelineMarks[i].current->type & VTR_ERROR_MARK_TYPE) &&
            playerMarks->timelineMarks[i].current->vtrErrorCode != 0)
        {
            *vtrErrorCode = playerMarks->timelineMarks[i].current->vtrErrorCode;
            break;
        }
    }

    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)
}

unsigned int mpm_get_current_mark_type(MediaPlayerMarks *playerMarks, int64_t position)
{
    unsigned int markType = 0;
    int i;

    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    for (i = 0; i < playerMarks->numMarkSelections; i++) {
        if (playerMarks->timelineMarks[i].current &&
            playerMarks->timelineMarks[i].current->position == position)
        {
            markType |= playerMarks->timelineMarks[i].current->type;
        }
    }
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)

    return markType;
}

void mpm_next_active_mark_selection(MediaPlayerMarks *playerMarks)
{
    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)
    playerMarks->activeMarkSelection = (playerMarks->activeMarkSelection + 1) % playerMarks->numMarkSelections;
    osd_set_active_progress_bar_marks(playerMarks->osd, playerMarks->activeMarkSelection);
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)
}

int mpm_get_marks(MediaPlayerMarks *playerMarks, BasicMark **basicMarks)
{
    TimelineMark *mark[MAX_MARK_SELECTIONS];
    BasicMark *markP;
    int count;
    int i;
    int haveMarks;
    int64_t currentMarkPosition;


    PTHREAD_MUTEX_LOCK(&playerMarks->mutex)

    /* count marks */
    haveMarks = 0;
    for (i = 0; i < playerMarks->numMarkSelections; i++) {
        mark[i] = playerMarks->timelineMarks[i].first;
        haveMarks = haveMarks || mark[i];
    }
    if (!haveMarks) {
        *basicMarks = NULL;
        PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)
        return 0;
    }
    count = 0;
    while (1) {
        /* get next nearest user mark */
        currentMarkPosition = -1;
        for (i = 0; i < playerMarks->numMarkSelections; i++) {
            if (mark[i] && (currentMarkPosition == -1 || mark[i]->position < currentMarkPosition))
                currentMarkPosition = mark[i]->position;
        }
        if (currentMarkPosition == -1) {
            /* no more marks */
            break;
        }

        /* advance to past the nearest user mark */
        for (i = 0; i < playerMarks->numMarkSelections; i++) {
            if (mark[i] && mark[i]->position == currentMarkPosition)
                mark[i] = mark[i]->next;
        }

        count++;
    }

    /* allocate array*/
    CALLOC_OFAIL(*basicMarks, BasicMark, count);

    /* merge and copy data */
    for (i = 0; i < playerMarks->numMarkSelections; i++)
        mark[i] = playerMarks->timelineMarks[i].first;
    markP = *basicMarks;
    while (1) {
        /* get next nearest user mark */
        currentMarkPosition = -1;
        for (i = 0; i < playerMarks->numMarkSelections; i++) {
            if (mark[i] && (currentMarkPosition == -1 || mark[i]->position < currentMarkPosition))
                currentMarkPosition = mark[i]->position;
        }
        if (currentMarkPosition == -1) {
            /* no more marks */
            break;
        }

        markP->position = currentMarkPosition;
        markP->type = 0;
        markP->pairedPosition = -1;

        /* merge marks at same position and advance past the nearest user mark */
        for (i = 0; i < playerMarks->numMarkSelections; i++) {
            if (mark[i] && mark[i]->position == currentMarkPosition) {
                markP->type |= mark[i]->type;
                if (mark[i]->pairedMark) {
                    /* there is only 1 possible paired mark at a position */
                    markP->pairedPosition = mark[i]->pairedMark->position;
                }

                mark[i] = mark[i]->next;
            }
        }

        markP++;
    }

    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)

    return count;

fail:
    PTHREAD_MUTEX_UNLOCK(&playerMarks->mutex)
    SAFE_FREE(basicMarks);
    return 0;
}

