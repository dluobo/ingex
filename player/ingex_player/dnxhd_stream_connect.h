#ifndef __DNXHD_STREAM_CONNECT_H__
#define __DNXHD_STREAM_CONNECT_H__


#ifdef __cplusplus
extern "C"
{
#endif


#include "stream_connect.h"


/* connector that decodes DNxHD */

int dnxhd_connect_accept(MediaSink* sink, const StreamInfo* streamInfo);
int create_dnxhd_connect(MediaSink* sink, int sinkStreamId, int sourceStreamId, 
    const StreamInfo* streamInfo, int numFFMPEGThreads, int useWorkerThread, 
    StreamConnect** connect);


#ifdef __cplusplus
}
#endif


#endif


