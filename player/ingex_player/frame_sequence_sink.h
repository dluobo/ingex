#ifndef __FRAME_SEQUENCE_SINK_H__
#define __FRAME_SEQUENCE_SINK_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"

int fss_create_frame_sequence(MediaSink* sink, FrameSequenceSink** sequence);
MediaSink* fss_get_media_sink(FrameSequenceSink* sequence);



#ifdef __cplusplus
}
#endif


#endif

