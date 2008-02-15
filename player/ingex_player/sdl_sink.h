#ifndef __SDL_SINK_H__
#define __SDL_SINK_H__



#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"
#include "x11_common.h"
/* TODO: remove window listener def from x11 common */


typedef struct SDLSink SDLSink;



int sdls_open(SDLSink** sink);
MediaSink* sdls_get_media_sink(SDLSink* sink);

void sdls_register_window_listener(SDLSink* sink, X11WindowListener* listener);
void sdls_unregister_window_listener(SDLSink* sink, X11WindowListener* listener);



#ifdef __cplusplus
}
#endif


#endif


