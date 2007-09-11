#ifndef __MEDIA_SINK_FRAME_H__
#define __MEDIA_SINK_FRAME_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "frame_info.h"
#include "on_screen_display.h"



typedef struct
{
    void* data;

    int (*register_stream)(void* data, int streamId, const StreamInfo* streamInfo);
    
    void (*reset)(void* data);

    int (*accept_stream_frame)(void* data, int streamId, const FrameInfo* frameInfo);
    int (*allocate_stream_buffer)(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer);
    int (*set_is_present)(void* data, int streamId);
    int (*complete_frame)(void* data, const OnScreenDisplayState* osdState, const FrameInfo* frameInfo);

    int (*get_stream_buffer)(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer);
    
    void (*free)(void* data);
} MediaSinkFrame;



/* utility functions for calling MediaSinkFrame functions */

int msf_register_stream(MediaSinkFrame* sinkFrame, int streamId, const StreamInfo* streamInfo);
void msf_reset(MediaSinkFrame* sinkFrame);
int msf_accept_stream_frame(MediaSinkFrame* sinkFrame, int streamId, const FrameInfo* frameInfo);
int msf_allocate_stream_buffer(MediaSinkFrame* sinkFrame, int streamId, unsigned int bufferSize, unsigned char** buffer);
int msf_set_is_present(MediaSinkFrame* sinkFrame, int streamId);
int msf_complete_frame(MediaSinkFrame* sinkFrame, const OnScreenDisplayState* osdState, const FrameInfo* frameInfo);
void msf_free(MediaSinkFrame* sinkFrame);



#ifdef __cplusplus
}
#endif


#endif


