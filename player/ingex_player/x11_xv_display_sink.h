#ifndef __X11_XV_DISPLAY_SINK_H__
#define __X11_XV_DISPLAY_SINK_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"
#include "media_control.h"
#include "x11_common.h"


/* X11 XV display sink */

typedef struct X11XVDisplaySink X11XVDisplaySink;

int xvsk_check_is_available();

int xvsk_open(int reviewDuration, int disableOSD, const Rational* pixelAspectRatio, 
    const Rational* monitorAspectRatio, float scale, X11XVDisplaySink** sink);
void xvsk_set_media_control(X11XVDisplaySink* sink, ConnectMapping mapping, MediaControl* control);
void xvsk_unset_media_control(X11XVDisplaySink* sink);
MediaSink* xvsk_get_media_sink(X11XVDisplaySink* sink);

void xvsk_set_window_name(X11XVDisplaySink* sink, const char* name);

void xvsk_register_window_listener(X11XVDisplaySink* sink, X11WindowListener* listener);
void xvsk_unregister_window_listener(X11XVDisplaySink* sink, X11WindowListener* listener);

void xvsk_register_keyboard_listener(X11XVDisplaySink* sink, KeyboardInputListener* listener);
void xvsk_unregister_keyboard_listener(X11XVDisplaySink* sink, KeyboardInputListener* listener);

void xvsk_register_progress_bar_listener(X11XVDisplaySink* sink, ProgressBarInputListener* listener);
void xvsk_unregister_progress_bar_listener(X11XVDisplaySink* sink, ProgressBarInputListener* listener);



#ifdef __cplusplus
}
#endif


#endif
