#ifndef __AUDIO_LEVEL_SINK_H__
#define __AUDIO_LEVEL_SINK_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"

typedef struct AudioLevelSink AudioLevelSink;


int als_create_audio_level_sink(MediaSink* targetSink, int numAudioStreams, float audioLineupLevel,
    AudioLevelSink** sink);
MediaSink* als_get_media_sink(AudioLevelSink* sink);


#ifdef __cplusplus
}
#endif


#endif


