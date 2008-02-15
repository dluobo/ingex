#ifndef __X11_DISPLAY_SINK_H__
#define __X11_DISPLAY_SINK_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"
#include "media_control.h"
#include "x11_common.h"


/* X11 display sink */

typedef struct X11DisplaySink X11DisplaySink;


int xsk_open(int reviewDuration, int disableOSD, const Rational* pixelAspectRatio, 
    const Rational* monitorAspectRatio, float scale, int swScale, unsigned long windowId, X11DisplaySink** sink);
void xsk_set_media_control(X11DisplaySink* sink, ConnectMapping mapping, MediaControl* control);
void xsk_unset_media_control(X11DisplaySink* sink);
MediaSink* xsk_get_media_sink(X11DisplaySink* sink);

void xsk_set_window_name(X11DisplaySink* sink, const char* name);

void xsk_register_window_listener(X11DisplaySink* sink, X11WindowListener* listener);
void xsk_unregister_window_listener(X11DisplaySink* sink, X11WindowListener* listener);

void xsk_register_keyboard_listener(X11DisplaySink* sink, KeyboardInputListener* listener);
void xsk_unregister_keyboard_listener(X11DisplaySink* sink, KeyboardInputListener* listener);

void xsk_register_progress_bar_listener(X11DisplaySink* sink, ProgressBarInputListener* listener);
void xsk_unregister_progress_bar_listener(X11DisplaySink* sink, ProgressBarInputListener* listener);


#ifdef __cplusplus
}
#endif


#endif

