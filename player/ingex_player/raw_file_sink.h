#ifndef __RAW_FILE_SINK_H__
#define __RAW_FILE_SINK_H__



#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"


/* stores all raw streams to files */

int rms_open(const char* filenameTemplate, MediaSink** sink);



#ifdef __cplusplus
}
#endif


#endif


