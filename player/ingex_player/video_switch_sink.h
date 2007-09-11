#ifndef __VIDEO_SWITCH_SINK_H__
#define __VIDEO_SWITCH_SINK_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"


struct VideoSwitchSink
{
    void* data;
    
    MediaSink* (*get_media_sink)(void* data);
    
    int (*switch_next_video)(void* data);
    int (*switch_prev_video)(void* data);
    int (*switch_video)(void* data, int index);
};

/* utility functions for calling VideoSwitchSink functions */

MediaSink* vsw_get_media_sink(VideoSwitchSink* swtch);
int vsw_switch_next_video(VideoSwitchSink* swtch);
int vsw_switch_prev_video(VideoSwitchSink* swtch);
int vsw_switch_video(VideoSwitchSink* swtch, int index);


/* video switch with quad split */

int qvs_create_video_switch(MediaSink* sink, int withQuadSplit, int applyQuadSplitFilter, VideoSwitchSink** swtch);




#ifdef __cplusplus
}
#endif


#endif


