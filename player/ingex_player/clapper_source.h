#ifndef __CLAPPER_SOURCE_H__
#define __CLAPPER_SOURCE_H__



#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"


/* clapper source */

int clp_create(const StreamInfo* videoStreamInfo, const StreamInfo* audioStreamInfo, 
    int64_t length, MediaSource** source);



#ifdef __cplusplus
}
#endif




#endif


