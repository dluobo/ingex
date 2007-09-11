#ifndef __MPEGI_STREAM_CONNECT_H__
#define __MPEGI_STREAM_CONNECT_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "stream_connect.h"


/* connector that decodes MPEG I-frame only */

int mpegi_connect_accept(MediaSink* sink, const StreamInfo* streamInfo);
int create_mpegi_connect(MediaSink* sink, int sinkStreamId, int sourceStreamId, 
    const StreamInfo* streamInfo, int numFFMPEGThreads, int useWorkerThread, 
    StreamConnect** connect);


/* use MPEG I-frame only decoders as a static and reusable resource */

int init_mpegi_decoder_resources();
void free_mpegi_decoder_resources();


#ifdef __cplusplus
}
#endif


#endif


