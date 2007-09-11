#ifndef __DUAL_SINK_H__
#define __DUAL_SINK_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "dvs_sink.h"
#include "x11_common.h"
#include "media_control.h"


typedef struct DualSink DualSink;


int dusk_open(int reviewDuration, SDIVITCSource sdiVITCSource, int extraSDIVITCSource, int numBuffers, 
    int useXV, int disableSDIOSD, int disableX11OSD, const Rational* pixelAspectRatio, 
    const Rational* monitorAspectRatio, float scale, int fitVideo, DualSink** dualSink);
MediaSink* dusk_get_media_sink(DualSink* dualSink);
void dusk_set_media_control(DualSink* dualSink, ConnectMapping mapping, MediaControl* control);
void dusk_unset_media_control(DualSink* dualSink);

void dusk_set_x11_window_name(DualSink* dualSink, const char* name);

void dusk_register_window_listener(DualSink* dualSink, X11WindowListener* listener);
void dusk_unregister_window_listener(DualSink* dualSink, X11WindowListener* listener);

void dusk_register_keyboard_listener(DualSink* dualSink, KeyboardInputListener* listener);
void dusk_unregister_keyboard_listener(DualSink* dualSink, KeyboardInputListener* listener);

void dusk_register_progress_bar_listener(DualSink* dualSink, ProgressBarInputListener* listener);
void dusk_unregister_progress_bar_listener(DualSink* dualSink, ProgressBarInputListener* listener);




#ifdef __cplusplus
}
#endif


#endif


