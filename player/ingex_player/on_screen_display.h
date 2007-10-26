#ifndef __ON_SCREEN_DISPLAY__
#define __ON_SCREEN_DISPLAY__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "frame_info.h"



/* On screen display menu */

#define LAST_MENU_ITEM_INDEX        (-1)
#define CURRENT_MENU_ITEM_INDEX     (-2)

typedef enum
{
    MENU_ITEM_NORMAL = 0,
    MENU_ITEM_DISABLED,
    MENU_ITEM_HIGHLIGHTED
} OSDMenuListItemState;

typedef struct   
{ 
    long id; 
    void* data;    
    void (*free_func)(void* data);
} OSDPrivateData;

typedef struct OSDMenuListItem
{
    struct OSDMenuListItem* next;
    struct OSDMenuListItem* prev;
    
    char* text;
    int textUpdated;

    OSDMenuListItemState state;
    
    /* supports maximum 2 OSDs (eg dual sink) */
    OSDPrivateData osdPrivateData[2];
    
} OSDMenuListItem;

typedef struct
{
    char* title;
    int titleUpdated;
    
    char* status;
    int statusUpdated;
    
    char* comment;
    int commentUpdated;
    
    OSDMenuListItem* items;
    int numItems;
    int currentItemIndex;
    int itemsUpdated;

    pthread_mutex_t menuModelMutex;
    
    /* supports maximum 2 OSDs (eg dual sink) */
    OSDPrivateData osdPrivateData[2];
    
} OSDMenuModel;

typedef enum
{
    OSD_SOURCE_INFO_SCREEN = 0,
    OSD_PLAY_STATE_SCREEN,
    OSD_MENU_SCREEN,
    OSD_EMPTY_SCREEN /* always keep this one last */
} OSDScreen;

typedef enum
{
    OSD_PLAY_STATE,
    OSD_SPEED_STATE
} OSDPlayState;
 
typedef struct OnScreenDisplayState OnScreenDisplayState;

typedef struct
{
    int* markCounts;
    int numMarkCounts;
    int updated; /* indicates that the marks have been updated (change to/from count == 0) */
    pthread_mutex_t marksMutex;
} OSDMarksModel;

typedef struct
{
    void* data; /* passed as parameter to functions */
    
    void (*refresh_required)(void* data);
    void (*osd_screen_changed)(void* data, OSDScreen screen);
} OSDListener;

typedef struct
{
    void* data; /* passed as parameter to functions */
    
    int (*initialise)(void* data, const StreamInfo* streamInfo, const Rational* aspectRatio);
    
    void (*set_listener)(void* data, OSDListener* listener);

    int (*create_menu_model)(void* data, OSDMenuModel** menu);
    void (*free_menu_model)(void* data, OSDMenuModel** menu);
    void (*set_active_menu_model)(void* data, int updateMask, OSDMenuModel* menu);
    
    int (*set_screen)(void* data, OSDScreen screen);    
    int (*next_screen)(void* data);    
    int (*get_screen)(void* data, OSDScreen* screen);    
    int (*set_timecode)(void* data, int index, int type, int subType);
    int (*next_timecode)(void* data);
    int (*set_play_state)(void* data, OSDPlayState state, int value);
    int (*set_state)(void* data, const OnScreenDisplayState* state);
    
    void (*set_minimum_audio_stream_level)(void* data, double level);
    void (*set_audio_lineup_level)(void* data, float level);
    void (*reset_audio_stream_levels)(void* data);
    int (*register_audio_stream)(void* data, int streamId);
    void (*set_audio_stream_level)(void* data, int streamId, double level);

    void (*show_field_symbol)(void* data, int enable);
    
    void (*set_mark_display)(void* data, const MarkConfigs* markConfigs);
    int (*create_marks_model)(void* data, OSDMarksModel** model); 
    void (*free_marks_model)(void* data, OSDMarksModel** model); 
    void (*set_marks_model)(void* data, int updateMask, OSDMarksModel* model);
    
    void (*set_progress_bar_visibility)(void* data, int visible);
    float (*get_position_in_progress_bar)(void* data, int x, int y);
    
    int (*add_to_image)(void* data, const FrameInfo* frameInfo, unsigned char* image, 
        int width, int height);

    int (*reset)(void* data);

    void (*free)(void* data);
} OnScreenDisplay;


/* utility functions for calling OSDListener functions */

void osdl_refresh_required(OSDListener* listener);
void osdl_osd_screen_changed(OSDListener* listener, OSDScreen screen);


/* utility functions for calling OnScreenDisplay functions */

int osd_initialise(OnScreenDisplay* osd, const StreamInfo* streamInfo, const Rational* aspectRatio);
void osd_set_listener(OnScreenDisplay* osd, OSDListener* listener);
int osd_create_menu_model(OnScreenDisplay* osd, OSDMenuModel** menu);
void osd_free_menu_model(OnScreenDisplay* osd, OSDMenuModel** menu);
void osd_set_active_menu_model(OnScreenDisplay* osd, int updateMask, OSDMenuModel* menu);
int osd_set_screen(OnScreenDisplay* osd, OSDScreen screen);    
int osd_next_screen(OnScreenDisplay* osd);    
int osd_get_screen(OnScreenDisplay* osd, OSDScreen* screen);    
int osd_set_timecode(OnScreenDisplay* osd, int index, int type, int subType);
int osd_next_timecode(OnScreenDisplay* osd);
int osd_set_play_state(OnScreenDisplay* osd, OSDPlayState state, int value);
int osd_set_state(OnScreenDisplay* osd, const OnScreenDisplayState* state);
void osd_set_minimum_audio_stream_level(OnScreenDisplay* osd, double level);
void osd_set_audio_lineup_level(OnScreenDisplay* osd, float level);
void osd_reset_audio_stream_levels(OnScreenDisplay* osd);
int osd_register_audio_stream(OnScreenDisplay* osd, int streamId);
void osd_set_audio_stream_level(OnScreenDisplay* osd, int streamId, double level);
void osd_show_field_symbol(OnScreenDisplay* osd, int enable);
void osd_set_mark_display(OnScreenDisplay* osd, const MarkConfigs* markConfigs);
int osd_create_marks_model(OnScreenDisplay* osd, OSDMarksModel** model); 
void osd_free_marks_model(OnScreenDisplay* osd, OSDMarksModel** model); 
void osd_set_marks_model(OnScreenDisplay* osd, int updateMask, OSDMarksModel* model); 
void osd_set_progress_bar_visibility(OnScreenDisplay* osd, int visible);
float osd_get_position_in_progress_bar(OnScreenDisplay* osd, int x, int y);
int osd_add_to_image(OnScreenDisplay* osd, const FrameInfo* frameInfo, unsigned char* image, 
    int width, int height);
int osd_reset(OnScreenDisplay* osd);
void osd_free(OnScreenDisplay* osd);



/* Default On screen display */

int osdd_create(OnScreenDisplay** osd);



/* Menu model functions */

void osdm_lock(OSDMenuModel* menu);
void osdm_unlock(OSDMenuModel* menu);
int osdm_set_menu_private_data(OSDMenuModel* menu, long id, void* data, void (*free_func)(void* data));
int osdm_set_item_private_data(OSDMenuListItem* item, long id, void* data, void (*free_func)(void* data));
void* osdm_get_menu_private_data(OSDMenuModel* menu, long id);
void* osdm_get_item_private_data(OSDMenuListItem* item, long id);
void osdm_free_menu_private_data(OSDMenuModel* menu);
void osdm_free_item_private_data(OSDMenuListItem* item);
int osdm_set_title(OSDMenuModel* menu, const char* title);
int osdm_set_status(OSDMenuModel* menu, const char* status);
int osdm_set_comment(OSDMenuModel* menu, const char* comment);
int osdm_insert_list_item(OSDMenuModel* menu, int index, OSDMenuListItem** item);
void osdm_remove_list_item(OSDMenuModel* menu, int index);
OSDMenuListItem* osdm_get_list_item(OSDMenuModel* menu, int index);
void osdm_set_current_list_item(OSDMenuModel* menu, int index);
void osdm_set_next_current_list_item(OSDMenuModel* menu);
void osdm_set_prev_current_list_item(OSDMenuModel* menu);
int osdm_set_list_item_text(OSDMenuModel* menu, OSDMenuListItem* item, const char* text);
void osdm_set_list_item_state(OSDMenuModel* menu, OSDMenuListItem* item, OSDMenuListItemState state);







/* On screen display state */

#define MAX_AUDIO_LEVELS            16

typedef struct
{
    int streamId;
    double level; /*  MIN_AUDIO_LOG_LEVEL dB <= level <= 0dB */
} AudioStreamLevel;
    
struct OnScreenDisplayState
{
    /* the state */
    OSDScreen screen;
    int isPlaying;
    int playSpeed;
    int playForwards;
    int timecodeIndex;
    int timecodeStreamId;
    int showFieldSymbol;
    
    /* used to set the screen when adding the OSD to the image */
    int screenSet;
    int nextScreen;
    
    /* used to set the state when commit is called */
    int setTimecode;
    int setTimecodeIndex;
    int setTimecodeType;
    int setTimecodeSubType;
    int nextTimecode;

    /* (internal) */    
    OnScreenDisplay osd;
    AudioStreamLevel audioStreamLevels[MAX_AUDIO_LEVELS];
    int numAudioLevels;
    double minimumAudioLevel;
    float audioLineupLevel;
    MarkConfigs markConfigs;
};

int osds_create(OnScreenDisplayState** state);
void osds_free(OnScreenDisplayState** state);
OnScreenDisplay* osds_get_osd(OnScreenDisplayState* state);
void osds_complete(OnScreenDisplayState* state, const FrameInfo* frameInfo);
void osds_reset(OnScreenDisplayState* state);
void osds_sink_reset(OnScreenDisplayState* state);
void osds_reset_screen_state(OnScreenDisplayState* state);



#ifdef __cplusplus
}
#endif


#endif

