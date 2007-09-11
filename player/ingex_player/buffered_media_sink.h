#ifndef __BUFFERED_MEDIA_SINK_H__
#define __BUFFERED_MEDIA_SINK_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"


typedef struct BufferedMediaSink BufferedMediaSink;


int bms_create(MediaSink** targetSink, int size, int dropFrameWhenFull, BufferedMediaSink** bufSink);
MediaSink* bms_get_sink(BufferedMediaSink* bufSink);



#ifdef __cplusplus
}
#endif


#endif


