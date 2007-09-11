#ifndef __STREAM_CONNECT_H__
#define __STREAM_CONNECT_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"
#include "media_sink.h"


typedef struct
{
    void* data; /* passed to functions */
    
    MediaSourceListener* (*get_source_listener)(void* data);
    
    /* complete all remaining tasks */
    int (*sync)(void* data);
    
    /* close and free resources */
    void (*close)(void* data);
} StreamConnect;


/* utility functions for calling StreamConnect functions */

MediaSourceListener* stc_get_source_listener(StreamConnect* connect);
int stc_sync(StreamConnect* connect);
void stc_close(StreamConnect* connect);



/* connector that passes data directly from source to sink */

int pass_through_accept(MediaSink* sink, const StreamInfo* streamInfo);
int create_pass_through_connect(MediaSink* sink, int sinkStreamId, int sourceStreamId, 
    const StreamInfo* streamInfo, StreamConnect** connect);

    

#ifdef __cplusplus
}
#endif


#endif


