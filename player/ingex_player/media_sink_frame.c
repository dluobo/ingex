#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "media_sink_frame.h"


int msf_register_stream(MediaSinkFrame* sinkFrame, int streamId, const StreamInfo* streamInfo)
{
    if (sinkFrame && sinkFrame->register_stream)
    {
        return sinkFrame->register_stream(sinkFrame->data, streamId, streamInfo);
    }
    return 0;
}

void msf_reset(MediaSinkFrame* sinkFrame)
{
    if (sinkFrame && sinkFrame->reset)
    {
        sinkFrame->reset(sinkFrame->data);
    }
}

int msf_accept_stream_frame(MediaSinkFrame* sinkFrame, int streamId, const FrameInfo* frameInfo)
{
    if (sinkFrame && sinkFrame->accept_stream_frame)
    {
        return sinkFrame->accept_stream_frame(sinkFrame->data, streamId, frameInfo);
    }
    return 0;
}

int msf_allocate_stream_buffer(MediaSinkFrame* sinkFrame, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    if (sinkFrame && sinkFrame->allocate_stream_buffer)
    {
        return sinkFrame->allocate_stream_buffer(sinkFrame->data, streamId, bufferSize, buffer);
    }
    return 0;
}

int msf_set_is_present(MediaSinkFrame* sinkFrame, int streamId)
{
    if (sinkFrame && sinkFrame->set_is_present)
    {
        return sinkFrame->set_is_present(sinkFrame->data, streamId);
    }
    return 0;
}

int msf_complete_frame(MediaSinkFrame* sinkFrame, const OnScreenDisplayState* osdState, const FrameInfo* frameInfo)
{
    if (sinkFrame && sinkFrame->complete_frame)
    {
        return sinkFrame->complete_frame(sinkFrame->data, osdState, frameInfo);
    }
    return 0;
}

void msf_free(MediaSinkFrame* sinkFrame)
{
    if (sinkFrame && sinkFrame->free)
    {
        sinkFrame->free(sinkFrame->data);
    }
}


