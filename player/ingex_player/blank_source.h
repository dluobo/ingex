#ifndef __BLANK_SOURCE_H__
#define __BLANK_SOURCE_H__



#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"


/* blank video source */

int bks_create(const StreamInfo* videoStreamInfo, int64_t length, MediaSource** source);



#ifdef __cplusplus
}
#endif




#endif


