#ifndef __MEDIA_PLAYER_H__
#define __MEDIA_PLAYER_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"
#include "media_sink.h"
#include "media_control.h"
#include "connection_matrix.h"
#include "frame_info.h"


typedef struct MediaPlayer MediaPlayer;

typedef struct
{
    int lockedChanged;
    int locked; /* 1 == controls are locked */
    
    int playChanged;
    int play; /* 0 == pause, 1 == play */
    
    int stopChanged;
    int stop; /* 1 == stopped */
    
    int speedChanged;
    int speed; /* 1 == normal, > 0 means playing forwards, < 0 backwards, units is frames */

    FrameInfo displayedFrameInfo;
} MediaPlayerStateEvent;
    
typedef struct
{
    void* data; /* passed to functions */
    
    void (*frame_displayed_event)(void* data, const FrameInfo* frameInfo);
    void (*frame_dropped_event)(void* data, const FrameInfo* lastFrameInfo);
    void (*state_change_event)(void* data, const MediaPlayerStateEvent* event);
    void (*end_of_source_event)(void* data, const FrameInfo* lastReadFrameInfo);
    void (*start_of_source_event)(void* data, const FrameInfo* firstReadFrameInfo);
    
    /* called when the player is closed (free'd) */
    void (*player_closed)(void* data);
} MediaPlayerListener;


void mpl_frame_displayed_event(MediaPlayerListener* listener, const FrameInfo* frameInfo);
void mpl_frame_dropped_event(MediaPlayerListener* listener, const FrameInfo* lastFrameInfo);
void mpl_state_change_event(MediaPlayerListener* listener, const MediaPlayerStateEvent* event);
void mpl_end_of_source_event(MediaPlayerListener* listener, const FrameInfo* lastFrameInfo);
void mpl_start_of_source_event(MediaPlayerListener* listener, const FrameInfo* firstFrameInfo);
void mpl_player_closed(MediaPlayerListener* listener);



typedef struct
{
    void* data; /* passed to functions */
    
    void (*refresh_required)(void* data);
} MenuHandlerListener;

typedef struct
{
    void* data; /* passed to functions */

    void (*set_listener)(void* data, MenuHandlerListener* listener);
   
    void (*next_menu_item)(void* data); 
    void (*previous_menu_item)(void* data); 
    void (*select_menu_item_left)(void* data);
    void (*select_menu_item_right)(void* data);
    void (*select_menu_item_center)(void* data);
    void (*select_menu_item_extra)(void* data);
    
    void (*free)(void* data);
} MenuHandler;

void mhl_refresh_required(MenuHandlerListener* listener);

void mnh_set_listener(MenuHandler* handler, MenuHandlerListener* listener); 
void mnh_next_menu_item(MenuHandler* handler); 
void mnh_previous_menu_item(MenuHandler* handler); 
void mnh_select_menu_item_left(MenuHandler* handler);
void mnh_select_menu_item_right(MenuHandler* handler);
void mnh_select_menu_item_center(MenuHandler* handler);
void mnh_select_menu_item_extra(MenuHandler* handler);
void mnh_free(MenuHandler* handler);



typedef struct 
{
    int64_t position;
    int type;
} Mark;

/* note: source and sink ownership is not transferred to player */
int ply_create_player(MediaSource* source, MediaSink* sink, int initialLock, 
    int closeAtEnd, int numFFMPEGThreads, int useWorkerThreads, int loop, int showFieldSymbol, 
    const Timecode* startVITC, const Timecode* startLTC, 
    FILE* bufferStateLogFile, MediaPlayer** player);
int ply_register_player_listener(MediaPlayer* player, MediaPlayerListener* playerListener);
MediaControl* ply_get_media_control(MediaPlayer* player);
int ply_start_player(MediaPlayer* player);
void ply_close_player(MediaPlayer** player);
int ply_get_marks(MediaPlayer* player, Mark** marks);
MediaSink* ply_get_media_sink(MediaPlayer* player);
void ply_set_menu_handler(MediaPlayer* player, MenuHandler* handler);



#ifdef __cplusplus
}
#endif


#endif

