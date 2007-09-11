#ifndef __DV_STREAM_CONNECT_H__
#define __DV_STREAM_CONNECT_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "stream_connect.h"


/* connector that decodes DV */

int dv_connect_accept(MediaSink* sink, const StreamInfo* streamInfo);
int create_dv_connect(MediaSink* sink, int sinkStreamId, int sourceStreamId, 
    const StreamInfo* streamInfo, int numFFMPEGThreads, int useWorkerThread, 
    StreamConnect** connect);


/* use DV decoders as a static and reusable resource */

int init_dv_decoder_resources();
void free_dv_decoder_resources();


#ifdef __cplusplus
}
#endif


#endif


