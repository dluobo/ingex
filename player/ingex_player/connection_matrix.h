#ifndef __CONNECTION_MATRIX_H__
#define __CONNECTION_MATRIX_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"
#include "media_sink.h"


typedef struct ConnectionMatrix ConnectionMatrix;


int stm_create_connection_matrix(MediaSource* source, MediaSink* sink, int numFFMPEGThreads, 
    int useWorkerThreads, ConnectionMatrix** matrix);
MediaSourceListener* stm_get_stream_listener(ConnectionMatrix* matrix);
int stm_sync(ConnectionMatrix* matrix);
void stm_close(ConnectionMatrix** matrix);


#ifdef __cplusplus
}
#endif


#endif


