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
#include "connection_matrix.h"
#include "on_screen_display.h"
#include "video_switch_sink.h"
#include "half_split_sink.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


#define HAVE_PLAYER_LISTENER(player) \
    (player->listeners.listener != NULL)

    
typedef struct MediaPlayerListenerElement
{
    struct MediaPlayerListenerElement* next;
    
    MediaPlayerListener* listener;
} MediaPlayerListenerElement;

typedef MediaPlayerListenerElement MediaPlayerListenerList;

typedef struct UserMark 
{
    struct UserMark* next;
    struct UserMark* prev;
    
    int64_t position;
    int type;
} UserMark;

typedef struct
{
    UserMark* first;
    UserMark* current;
    UserMark* last;
    int count;
} UserMarks;

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
    int isTimecodeStream;
} MediaSinkStreamInfoElement;

typedef MediaSinkStreamInfoElement MediaSinkStreamInfoList;

struct MediaPlayer
{
    int closeAtEnd;
    int loop;
    
    /* player state */
    MediaPlayerState state;
    MediaPlayerState prevState;
    pthread_mutex_t stateMutex;

    /* source, sink and connection matrix (via timecode extractor) linking them */
    MediaSource* mediaSource;
    MediaSink* mediaSink;
    ConnectionMatrix* connectionMatrix;
    MediaSink timecodeExtractor;
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

    /* user mark points */
    UserMarks userMarks;
    pthread_mutex_t userMarksMutex;
    OSDMarksModel* marksModel;
    
    /* menu handler */
    MenuHandler* menuHandler;
    MenuHandlerListener menuListener;
    
    /* buffer state log file */
    FILE* bufferStateLogFile;
};


/* caller must lock the user marks mutex */
static void _clear_marks_model(MediaPlayer* player)
{
    if (player->marksModel == NULL)
    {
        return;
    }
    
    PTHREAD_MUTEX_LOCK(&player->marksModel->marksMutex)
    memset(player->marksModel->markCounts, 0, player->marksModel->numMarkCounts * sizeof(int));
    player->marksModel->updated = -1; /* set all bits to 1 */
    PTHREAD_MUTEX_UNLOCK(&player->marksModel->marksMutex)
}

/* caller must lock the user marks mutex */
static void _add_mark_to_marks_model(MediaPlayer* player, int64_t position)
{
    int index;

    if (player->marksModel == NULL || player->sourceLength <= 0)
    {
        return;
    }
    
    PTHREAD_MUTEX_LOCK(&player->marksModel->marksMutex)
    if (player->marksModel->numMarkCounts > 0)
    {
        index = player->marksModel->numMarkCounts * position / player->sourceLength;
        if (player->marksModel->markCounts[index] == 0)
        {
            player->marksModel->updated = -1; /* set all bits to 1 */
        }
        player->marksModel->markCounts[index]++;
    }
    PTHREAD_MUTEX_UNLOCK(&player->marksModel->marksMutex)
}

/* caller must lock the user marks mutex */
static void _remove_mark_from_marks_model(MediaPlayer* player, int64_t position)
{
    int index;

    if (player->marksModel == NULL || player->sourceLength <= 0)
    {
        return;
    }
    
    PTHREAD_MUTEX_LOCK(&player->marksModel->marksMutex)
    if (player->marksModel->numMarkCounts > 0)
    {
        index = player->marksModel->numMarkCounts * position / player->sourceLength;
        if (player->marksModel->markCounts[index] == 1)
        {
            player->marksModel->updated = -1; /* set all bits to 1 */
        }
        if (player->marksModel->markCounts[index] > 0)
        {
            player->marksModel->markCounts[index]--;
        }
    }
    PTHREAD_MUTEX_UNLOCK(&player->marksModel->marksMutex)
}

/* caller must lock the user marks mutex */
static int _find_mark(MediaPlayer* player, int64_t position, UserMark** matchedMark)
{
    UserMark* mark;

    /* start the search from the current mark, otherwise the first */
    mark = player->userMarks.current;
    if (mark == NULL)
    {
        mark = player->userMarks.first;
    }
    
    /* try search forwards and if neccessary backwards */
    while (mark != NULL && mark->position < position)
    {
        mark = mark->next;
    }
    while (mark != NULL && mark->position > position)
    {
        mark = mark->prev;
    }
    
    if (mark == NULL || mark->position != position)
    {
        return 0;
    }
    
    
    *matchedMark = mark;
    return 1;
}

/* caller must lock the user marks mutex */
static int _find_next_mark(MediaPlayer* player, int64_t position, UserMark** matchedMark)
{
    UserMark* mark;

    /* start the search from the current mark, otherwise the first */
    mark = player->userMarks.current;
    if (mark == NULL)
    {
        mark = player->userMarks.first;
    }
    
    while (mark != NULL)
    {
        if (mark->position == position)
        {
            /* either found it or is NULL */
            mark = mark->next;
            break;
        }
        else if (mark->position > position)
        {
            if (mark->prev == NULL)
            {
                /* found it */
                break;
            }
            else if (mark->prev->position <= position)
            {
                /* found it */
                break;
            }
            else /* mark->prev->position > position */
            {
                /* go further backwards */
                mark = mark->prev;
            }
        }
        else /* mark->position < position */
        {
            /* go further fowards */
            mark = mark->next;
        }
    }
    
    if (mark == NULL)
    {
        return 0;
    }
    
    *matchedMark = mark;
    return 1;
}

/* caller must lock the user marks mutex */
static int _find_prev_mark(MediaPlayer* player, int64_t position, UserMark** matchedMark)
{
    UserMark* mark;

    /* start the search from the current mark, otherwise the first */
    mark = player->userMarks.current;
    if (mark == NULL)
    {
        mark = player->userMarks.first;
    }
    
    while (mark != NULL)
    {
        if (mark->position == position)
        {
            /* found it or is NULL */
            mark = mark->prev;
            break;
        }
        else if (mark->position < position)
        {
            if (mark->next == NULL)
            {
                /* found it */
                break;
            }
            else if (mark->next->position >= position)
            {
                /* found it */
                break;
            }
            else /* mark->next->position < position */
            {
                /* go further forwards */
                mark = mark->next;
            }
        }
        else /* mark->position > position */
        {
            /* go further backwards */
            mark = mark->prev;
        }
    }
    
    if (mark == NULL)
    {
        return 0;
    }
    
    *matchedMark = mark;
    return 1;
}

/* caller must lock the user marks mutex */
static void _update_current_mark(MediaPlayer* player, int64_t position)
{
    UserMark* currentMark;
    
    if (_find_mark(player, position, &currentMark))
    {
        player->userMarks.current = currentMark;
    }
}

static int find_next_mark(MediaPlayer* player, int64_t position, UserMark** matchedMark)
{
    int result;
    
    PTHREAD_MUTEX_LOCK(&player->userMarksMutex)
    result = _find_next_mark(player, position, matchedMark);
    PTHREAD_MUTEX_UNLOCK(&player->userMarksMutex)
    
    return result;
}

static int find_prev_mark(MediaPlayer* player, int64_t position, UserMark** matchedMark)
{
    int result;
    
    PTHREAD_MUTEX_LOCK(&player->userMarksMutex)
    result = _find_prev_mark(player, position, matchedMark);
    PTHREAD_MUTEX_UNLOCK(&player->userMarksMutex)
    
    return result;
}

/* caller must lock the user marks mutex */
static void __remove_mark(MediaPlayer* player, UserMark* mark)
{
    /* remove from list */    
    if (mark->prev != NULL)
    {
        mark->prev->next = mark->next;
    }
    if (mark->next != NULL)
    {
        mark->next->prev = mark->prev;
    }
    
    /* reset first, current, last if neccessary */
    if (mark == player->userMarks.first)
    {
        player->userMarks.first = mark->next;
    }
    if (mark == player->userMarks.current)
    {
        if (mark->prev != NULL)
        {
            player->userMarks.current = mark->prev;
        }
        else
        {
            player->userMarks.current = mark->next;
        }
    }
    if (mark == player->userMarks.last)
    {
        player->userMarks.last = mark->prev;
    }

    player->userMarks.count--;
    _remove_mark_from_marks_model(player, mark->position);
    
    free(mark);
}

/* caller must lock the user marks mutex */
/* mark is only removed if the resulting type == 0 */
static void _remove_mark(MediaPlayer* player, int64_t position, int typeMask)
{
    UserMark* mark = NULL; 
    
    if (_find_mark(player, position, &mark))
    {
        mark->type &= ~typeMask;
        
        if (mark->type == 0)
        {
            __remove_mark(player, mark);
        }
    }
}

/* mark is only removed if the resulting type == 0 */
static void remove_mark(MediaPlayer* player, int64_t position, int typeMask)
{
    PTHREAD_MUTEX_LOCK(&player->userMarksMutex)
    _remove_mark(player, position, typeMask);
    PTHREAD_MUTEX_UNLOCK(&player->userMarksMutex)
}

/* caller must lock the user marks mutex */
/* mark is removed if the resulting type == 0 */  
static void _add_mark(MediaPlayer* player, int64_t position, int type, int toggle)
{
    UserMark* mark;
    UserMark* newMark;
    
    if (position < 0 || type == 0)
    {
        return;
    }

    mark = player->userMarks.current;
    if (mark == NULL)
    {
        mark = player->userMarks.first;
    }
    while (mark != NULL && mark->position < position)
    {
        mark = mark->next;
    }
    while (mark != NULL && mark->position > position)
    {
        mark = mark->prev;
    }
    
    
    if (mark != NULL && mark->position == position)
    {
        /* add type to existing mark */
        if (toggle)
        {
            mark->type ^= type & ALL_MARK_TYPE;
        }
        else
        {
            mark->type |= type & ALL_MARK_TYPE;
        }

        /* remove mark if type == 0 */
        if (mark->type == 0)
        {
            __remove_mark(player, mark);
        }
    }
    else
    {
        /* new mark */
        CALLOC_OFAIL(newMark, UserMark, 1);
        newMark->type = type;
        newMark->position = position;
        
        if (mark != NULL)
        {
            /* insert before or after mark */
            
            if (mark->position > position)
            {
                /* insert before */
                if (mark->prev != NULL)
                {
                    mark->prev->next = newMark;
                }
                else
                {
                    player->userMarks.first = newMark;
                }
                newMark->prev = mark->prev;
                newMark->next = mark;
                mark->prev = newMark;
            }
            else
            {
                /* insert after */
                if (mark->next != NULL)
                {
                    mark->next->prev = newMark;
                }
                else
                {
                    player->userMarks.last = newMark;
                }
                newMark->next = mark->next;
                newMark->prev = mark;
                mark->next = newMark;
            }
        }
        else
        {
            /* append or prepend */
            
            if (player->userMarks.first == NULL)
            {
                /* new first and last */
                player->userMarks.first = newMark;
                player->userMarks.last = newMark;
            }
            else if (player->userMarks.first->position > position)
            {
                /* new first */
                newMark->next = player->userMarks.first;
                player->userMarks.first = newMark;
                player->userMarks.first->next->prev = player->userMarks.first;
            }
            else
            {
                /* new last */
                newMark->prev = player->userMarks.last;
                player->userMarks.last = newMark;
                player->userMarks.last->prev->next = player->userMarks.last;
            }
        }

        player->userMarks.count++;
        _add_mark_to_marks_model(player, position);
    }
    
    return;
    
fail:
    return;    
}

/* mark is removed if the resulting type == 0 */  
static void add_mark(MediaPlayer* player, int64_t position, int type, int toggle)
{
    PTHREAD_MUTEX_LOCK(&player->userMarksMutex)
    _add_mark(player, position, type, toggle);
    PTHREAD_MUTEX_UNLOCK(&player->userMarksMutex)
}

/* caller must lock the user marks mutex */
static void _free_marks(MediaPlayer* player, int typeMask)
{
    UserMark* mark;
    UserMark* nextMark;
    
    if ((typeMask & ALL_MARK_TYPE) == ALL_MARK_TYPE)
    {
        /* clear marks */
        mark = player->userMarks.first;
        while (mark != NULL)
        {
            nextMark = mark->next;
            free(mark);
            mark = nextMark;
        }
        player->userMarks.first = NULL;
        player->userMarks.current = NULL;
        player->userMarks.last = NULL;
        player->userMarks.count = 0;

        _clear_marks_model(player);        
    }
    else
    {
        /* go through one by one and remove marks with type not masked */
        mark = player->userMarks.first;
        while (mark != NULL)
        {
            nextMark = mark->next;
            
            mark->type &= ~typeMask;
            
            if (mark->type == 0)
            {
                __remove_mark(player, mark);
            }
            
            mark = nextMark;
        }

    }
}

static void free_marks(MediaPlayer* player, int typeMask)
{
    PTHREAD_MUTEX_LOCK(&player->userMarksMutex)
    _free_marks(player, typeMask);
    PTHREAD_MUTEX_UNLOCK(&player->userMarksMutex)
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

    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    
    if (!player->state.locked)
    {
        player->state.stop = 1;
    }
    
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
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

static void ply_step(void* data, int forward, PlayUnit unit)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    
    if (!player->state.locked)
    {
        player->state.play = 0;
        player->state.speed = 1;
        
        if (unit == FRAME_PLAY_UNIT || player->sourceLength <= 0)
        {
            player->state.nextPosition = player->state.nextPosition + 
                (forward ? 1 : -1);
        }
        else /* PERCENTAGE_PLAY_UNIT */
        {
            player->state.nextPosition = player->state.nextPosition + 
                (forward ? player->sourceLength / 100 : - player->sourceLength / 100);
        }

        switch_source_info_screen(player);
    }
    
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}
    
static void ply_mark_position(void* data, int64_t position, int type, int toggle)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    
    add_mark(player, position, type, toggle);
    player->state.refreshRequired = 1;
    
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_mark(void* data, int type, int toggle)
{
    MediaPlayer* player = (MediaPlayer*)data;
    
    ply_mark_position(data, player->state.lastFrameDisplayed.position, type, toggle);

    switch_source_info_screen(player);
}

static void ply_clear_mark(void* data, int typeMask)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    
    remove_mark(player, player->state.lastFrameDisplayed.position, typeMask);
    player->state.refreshRequired = 1;
    
    switch_source_info_screen(player);
    
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_clear_all_marks(void* data, int typeMask)
{
    MediaPlayer* player = (MediaPlayer*)data;

    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    
    free_marks(player, typeMask);
    player->state.refreshRequired = 1;
    
    switch_source_info_screen(player);
    
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_seek_next_mark(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;
    UserMark* mark = NULL;
    
    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    
    if (!player->state.locked)
    {
        if (find_next_mark(player, player->state.lastFrameDisplayed.position, &mark))
        {
            player->state.play = 0;
            player->state.nextPosition = mark->position;
        }
    }
    
    switch_source_info_screen(player);
    
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}

static void ply_seek_prev_mark(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;
    UserMark* mark = NULL;
    
    PTHREAD_MUTEX_LOCK(&player->stateMutex)
    
    if (!player->state.locked)
    {
        if (find_prev_mark(player, player->state.lastFrameDisplayed.position, &mark))
        {
            player->state.play = 0;
            player->state.nextPosition = mark->position;
        }
    }

    switch_source_info_screen(player);
    
    PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
}
 
static void ply_set_osd_screen(void* data, OSDScreen screen)
{
    MediaPlayer* player = (MediaPlayer*)data;
    
    if (!player->state.locked)
    {
        if (osd_set_screen(msk_get_osd(player->mediaSink), screen))
        {
            player->state.refreshRequired = 1;
        }
    }
}

static void ply_next_osd_screen(void* data)
{
    MediaPlayer* player = (MediaPlayer*)data;
    
    if (!player->state.locked)
    {
        if (osd_next_screen(msk_get_osd(player->mediaSink)))
        {
            player->state.refreshRequired = 1;
        }
    }
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

static void ply_set_half_split_orientation(void* data, int vertical)
{
    MediaPlayer* player = (MediaPlayer*)data;
    
    if (!player->state.locked)
    {
        hss_set_half_split_orientation(msk_get_half_split(player->mediaSink), vertical);

        switch_source_info_screen(player);
    }
}

static void ply_set_half_split_type(void* data, int type)
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


static int ply_tex_accept_stream(void* data, const StreamInfo* streamInfo)
{
    MediaPlayer* player = (MediaPlayer*)data;
    
    if (streamInfo->type == TIMECODE_STREAM_TYPE)
    {
        /* the timecodeExtractor is always interested in timecode */ 
        return 1;
    }
    else
    {
        return msk_accept_stream(player->mediaSink, streamInfo);
    }
}

static int ply_tex_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
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
    ele->isTimecodeStream = (streamInfo->type == TIMECODE_STREAM_TYPE);

    /* the timecodeExtractor is always interested in timecode, or sink has accepted and registered */ 
    return streamInfo->type == TIMECODE_STREAM_TYPE || sinkRegisterResult;
}

static int ply_tex_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
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
    
    /* the timecodeExtractor is always interested in timecode, or sink has accepted frame */ 
    return ele->isTimecodeStream || ele->acceptedFrame;
}
    
static int ply_tex_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
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
        /* if it is timecode, then the sink will get it and we have to extract it later in receive_stream_frame */
        
        return msk_get_stream_buffer(player->mediaSink, streamId, bufferSize, buffer);
    }
    else    
    {
        /* if the sink hasn't accepted the timecode then point the media source to the timecode in the frame info */
        
        if (msc_get_stream_info(player->mediaSource, streamId, &streamInfo) && 
            streamInfo->type == TIMECODE_STREAM_TYPE)
        {
            if (sizeof(Timecode) != bufferSize)
            {
                ml_log_error("Buffer size (%d) != timecode event data size (%d)\n", bufferSize, sizeof(Timecode));
                return 0;
            }
            
            if ((size_t)player->frameInfo.numTimecodes < sizeof(player->frameInfo.timecodes))
            {
                player->frameInfo.timecodes[player->frameInfo.numTimecodes].streamId = streamId;
                player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecodeType = streamInfo->timecodeType;
                player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecodeSubType = streamInfo->timecodeSubType;

                *buffer = (unsigned char*)&player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecode;
                
                player->frameInfo.numTimecodes++;
            }
        }

        return 1;
    }
}

static int ply_tex_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    MediaPlayer* player = (MediaPlayer*)data;
    const StreamInfo* streamInfo;
    MediaSinkStreamInfoElement* ele = &player->sinkStreamInfo;
    int result;
    
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
        result = msk_receive_stream_frame(player->mediaSink, streamId, buffer, bufferSize);
        
        /* copy timecodes from the sink into the frameInfo */
        
        if (msc_get_stream_info(player->mediaSource, streamId, &streamInfo) && 
            streamInfo->type == TIMECODE_STREAM_TYPE)
        {
            if (sizeof(Timecode) == bufferSize &&
                (size_t)player->frameInfo.numTimecodes < sizeof(player->frameInfo.timecodes))
            {
                player->frameInfo.timecodes[player->frameInfo.numTimecodes].streamId = streamId;
                player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecode = *(Timecode*)buffer;
                player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecodeType = streamInfo->timecodeType;
                player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecodeSubType = streamInfo->timecodeSubType;
                player->frameInfo.numTimecodes++;
            }
        }
    }
    /* else if it was a timecode stream then it has already been written to the frame info timecodes */
    
    return result;
}

static int ply_tex_receive_stream_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    MediaPlayer* player = (MediaPlayer*)data;
    const StreamInfo* streamInfo;
    MediaSinkStreamInfoElement* ele = &player->sinkStreamInfo;
    int result;
    unsigned char* nonconstBuffer;
    
    if (player->mediaSink->receive_stream_frame_const == NULL)
    {
        /* use the non-const functions */
        result = ply_tex_get_stream_buffer(data, streamId, bufferSize, &nonconstBuffer);
        if (result)
        {
            memcpy(nonconstBuffer, buffer, bufferSize);
            result = ply_tex_receive_stream_frame(data, streamId, nonconstBuffer, bufferSize);
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
    
    /* copy timecodes into the frameInfo */
    
    if (msc_get_stream_info(player->mediaSource, streamId, &streamInfo) && 
        streamInfo->type == TIMECODE_STREAM_TYPE)
    {
        if (sizeof(Timecode) == bufferSize &&
            (size_t)player->frameInfo.numTimecodes < sizeof(player->frameInfo.timecodes))
        {
            player->frameInfo.timecodes[player->frameInfo.numTimecodes].streamId = streamId;
            player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecode = *(Timecode*)buffer;
            player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecodeType = streamInfo->timecodeType;
            player->frameInfo.timecodes[player->frameInfo.numTimecodes].timecodeSubType = streamInfo->timecodeSubType;
            player->frameInfo.numTimecodes++;
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
    FILE* bufferStateLogFile, MediaPlayer** player)
{
    MediaPlayer* newPlayer;
    int64_t sourceLength;
    
    if (mediaSource == NULL || mediaSink == NULL)
    {
        ml_log_error("Invalid parameters to ply_create_player\n");
        return 0;
    }
    
    CALLOC_ORET(newPlayer, MediaPlayer, 1);
    
    newPlayer->closeAtEnd = closeAtEnd;
    newPlayer->loop = loop;
    newPlayer->bufferStateLogFile = bufferStateLogFile;
    
    newPlayer->frameInfo.position = -1; /* make sure that first frame at position 0 is not marked as a repeat */
    
    newPlayer->sinkListener.data = newPlayer;
    newPlayer->sinkListener.frame_displayed = ply_frame_displayed;
    newPlayer->sinkListener.frame_dropped = ply_frame_dropped;
    newPlayer->sinkListener.refresh_required = ply_refresh_required;
    newPlayer->sinkListener.osd_screen_changed = ply_osd_screen_changed;
    CHK_OFAIL(msk_register_listener(mediaSink, &newPlayer->sinkListener));
    
    CHK_OFAIL(init_mutex(&newPlayer->stateMutex));
    CHK_OFAIL(init_mutex(&newPlayer->userMarksMutex));

    newPlayer->state.play = 1;
    newPlayer->state.speed = 1;
    newPlayer->state.nextPosition = 0;
    newPlayer->state.lastPosition = -1;
    newPlayer->state.reviewEndPosition = -1;

    newPlayer->mediaSource = mediaSource;
    newPlayer->mediaSink = mediaSink;
    
    newPlayer->timecodeExtractor = *mediaSink; /* copy function pointers */
    newPlayer->timecodeExtractor.data = newPlayer;
    newPlayer->timecodeExtractor.accept_stream = ply_tex_accept_stream;
    newPlayer->timecodeExtractor.register_stream = ply_tex_register_stream;
    newPlayer->timecodeExtractor.accept_stream_frame = ply_tex_accept_stream_frame;
    newPlayer->timecodeExtractor.get_stream_buffer = ply_tex_get_stream_buffer;
    newPlayer->timecodeExtractor.receive_stream_frame = ply_tex_receive_stream_frame;
    newPlayer->timecodeExtractor.receive_stream_frame_const = ply_tex_receive_stream_frame_const;
    newPlayer->sinkStreamInfo.streamId = -1;
    
    CHK_OFAIL(stm_create_connection_matrix(mediaSource, &newPlayer->timecodeExtractor, 
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
    newPlayer->control.step = ply_step;
    newPlayer->control.mark = ply_mark;
    newPlayer->control.mark_position = ply_mark_position;
    newPlayer->control.clear_mark = ply_clear_mark;
    newPlayer->control.clear_all_marks = ply_clear_all_marks;
    newPlayer->control.seek_next_mark = ply_seek_next_mark;
    newPlayer->control.seek_prev_mark = ply_seek_prev_mark;
    newPlayer->control.set_osd_screen = ply_set_osd_screen;
    newPlayer->control.next_osd_screen = ply_next_osd_screen;
    newPlayer->control.set_osd_timecode = ply_set_osd_timecode;
    newPlayer->control.next_osd_timecode = ply_next_osd_timecode;
    newPlayer->control.switch_next_video = ply_switch_next_video;
    newPlayer->control.switch_prev_video = ply_switch_prev_video;
    newPlayer->control.switch_video = ply_switch_video;
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
    newPlayer->menuListener.refresh_required = ply_refresh_required;
    
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
    
    /* create marks model for OSD (if present) */
    if (msk_get_osd(mediaSink))
    {
        CHK_OFAIL(osd_create_marks_model(msk_get_osd(mediaSink), &newPlayer->marksModel));
        osd_set_marks_model(msk_get_osd(mediaSink), 0x01, newPlayer->marksModel);
    }
    
    
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

int ply_start_player(MediaPlayer* player)
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

    
    /* play frames from media source to media sink */
    
    while (!player->state.stop)
    {
        updatedOSD = 0;
        readFrame = 0;
        completedFrame = 0;
        haveSeeked = 0;
        seekOK = 0;
        moveBeyondLimits = 0;

        
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
            sourceProcessingCompleted = msc_post_complete(player->mediaSource, &player->control);
        }
        

        /* update the sink OSD */
        
        PTHREAD_MUTEX_LOCK(&player->stateMutex)
        currentState = player->state;
        PTHREAD_MUTEX_UNLOCK(&player->stateMutex)
        
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
                                    player->sourceLength = sourceLength;
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
                player->frameInfo.position = currentState.nextPosition;
                player->frameInfo.sourceLength = player->sourceLength;
                player->frameInfo.availableSourceLength = availableSourceLength;
                player->frameInfo.frameCount = player->frameCount;
                player->frameInfo.rateControl = rateControl;
                player->frameInfo.reversePlay = reversePlay;
                PTHREAD_MUTEX_LOCK(&player->userMarksMutex)
                _update_current_mark(player, currentState.nextPosition);
                if (player->userMarks.current != NULL &&
                    player->userMarks.current->position == currentState.nextPosition)
                {
                    player->frameInfo.isMarked = 1;
                    player->frameInfo.markType = player->userMarks.current->type;
                }
                PTHREAD_MUTEX_UNLOCK(&player->userMarksMutex)
                player->frameInfo.isRepeat = isRepeat;
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
                                    player->sourceLength = sourceLength;
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
    
    free_marks((*player), ALL_MARK_TYPE);
    
    eleA = &(*player)->sinkStreamInfo;
    eleA = eleA->next;
    while (eleA != NULL)
    {
        nextEleA = eleA->next;
        SAFE_FREE(&eleA);
        eleA = nextEleA;
    }
    
    stm_close(&(*player)->connectionMatrix);
    
    if ((*player)->marksModel != NULL)
    {
        osd_free_marks_model(msk_get_osd((*player)->mediaSink), &(*player)->marksModel);
    }
    
	destroy_mutex(&(*player)->stateMutex);
	destroy_mutex(&(*player)->userMarksMutex);

    SAFE_FREE(player);
}

int ply_get_marks(MediaPlayer* player, Mark** marks)
{
    UserMark* userMark;
    Mark* markP;
    int count;
    
    if (player->userMarks.first == NULL)
    {
        /* empty list */
        *marks = NULL;
        return 0;
    }

    PTHREAD_MUTEX_LOCK(&player->userMarksMutex)
    
    /* allocate array and copy data */
    count = player->userMarks.count;
    CALLOC_OFAIL(*marks, Mark, count);
    userMark = player->userMarks.first;  
    markP = *marks;
    while (userMark != NULL)
    {
        markP->position = userMark->position;
        markP->type = userMark->type;
        
        userMark = userMark->next;
        markP++;
    }

    PTHREAD_MUTEX_UNLOCK(&player->userMarksMutex)
    
    return count;
    
fail:
    PTHREAD_MUTEX_UNLOCK(&player->userMarksMutex)
    return 0;
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


