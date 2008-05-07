#ifndef __X11_COMMON_H__
#define __X11_COMMON_H__

#include <pthread.h>
#include <X11/Xlib.h>

#include "media_sink.h"
#include "keyboard_input.h"
#include "keyboard_input_connect.h"
#include "progress_bar_input_connect.h"


typedef struct
{
    void* data; /* passed to functions */
    
    void (*close_request)(void* data);
    
} X11WindowListener;

/* Store X11 display and window information created by browser and
 * need by plugin */
typedef struct
{
    Display *pluginDisplay;     /* NULL means open new connection to X server */
                                /* e.g. when plugin is using fork()+exec() */
    Window pluginWindow;
} X11PluginWindowInfo;

typedef struct
{
    int reviewDuration;
    OnScreenDisplay* osd;

    X11PluginWindowInfo pluginInfo;    /* required when run as a plugin */
    Display* display;
    Window window;
    GC gc;
    int haveWindow;
    char* windowName;
    Atom deleteAtom;
    unsigned int displayWidth;
    unsigned int displayHeight;
    unsigned int imageWidth;
    unsigned int imageHeight;
    unsigned int windowWidth;
    unsigned int windowHeight;
    
    MediaSinkListener* sinkListener;
    
    X11WindowListener* windowListener;

    ProgressBarInput progressBarInput;
    ProgressBarConnect* progressBarConnect;
    ProgressBarInputListener* progressBarListener;
    ProgressBarInputListener* separateProgressBarListener;
    KeyboardInput keyboardInput;    
    KeyboardConnect* keyboardConnect;    
    KeyboardInputListener* keyboardListener;
    KeyboardInputListener* separateKeyboardListener;
    pthread_mutex_t eventMutex;
    
    pthread_t processEventThreadId;
    int stopped;
    struct timeval processEventTime;
} X11Common;


/* X11WindowListener helper functions */

void x11wl_close_request(X11WindowListener* listener);


int x11c_initialise(X11Common* x11Common, int reviewDuration, OnScreenDisplay* osd, X11PluginWindowInfo *pluginInfo);
void x11c_clear(X11Common* x11Common);
int x11c_reset(X11Common* x11Common);

void x11c_register_window_listener(X11Common* x11Common, X11WindowListener* listener);
void x11c_unregister_window_listener(X11Common* x11Common, X11WindowListener* listener);

void x11c_register_keyboard_listener(X11Common* x11Common, KeyboardInputListener* listener);
void x11c_unregister_keyboard_listener(X11Common* x11Common, KeyboardInputListener* listener);

void x11c_register_progress_bar_listener(X11Common* x11Common, ProgressBarInputListener* listener);
void x11c_unregister_progress_bar_listener(X11Common* x11Common, ProgressBarInputListener* listener);


int x11c_open_display(X11Common* x11Common);
int x11c_get_screen_dimensions(X11Common* x11Common, int* width, int* height);
int x11c_create_window(X11Common* x11Common, unsigned int displayWidth, unsigned int displayHeight,
    unsigned int imageWidth, unsigned int imageHeight);

void x11c_set_media_control(X11Common* x11Common, ConnectMapping mapping, MediaControl* control);
void x11c_unset_media_control(X11Common* x11Common);
int x11c_process_events(X11Common* common, int sync);

int x11c_set_window_name(X11Common* common, const char* name);

int x11c_shared_memory_available(X11Common* common);


#endif


