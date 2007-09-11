#ifndef __AUDIO_SINK_H__
#define __AUDIO_SINK_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"

typedef struct AudioSink AudioSink;


int aus_create_audio_sink(MediaSink* targetSink, int audioDevice, AudioSink** sink);
MediaSink* aus_get_media_sink(AudioSink* sink);


#ifdef __cplusplus
}
#endif


#endif


