#ifndef __MJPEG_STREAM_CONNECT_H__
#define __MJPEG_STREAM_CONNECT_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "stream_connect.h"


/* connector that decodes DV */

int mjpeg_connect_accept(MediaSink* sink, const StreamInfo* streamInfo);
int create_mjpeg_connect(MediaSink* sink, int sinkStreamId, int sourceStreamId, 
    const StreamInfo* streamInfo, int numFFMPEGThreads, int useWorkerThread, 
    StreamConnect** connect);


/* use MJPEG decoders as a static and reusable resource */

int init_mjpeg_decoder_resources();
void free_mjpeg_decoder_resources();


#ifdef __cplusplus
}
#endif


#endif


