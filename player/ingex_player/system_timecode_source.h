#ifndef __SYSTEM_TIMECODE_SOURCE_H__
#define __SYSTEM_TIMECODE_SOURCE_H__



#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"


/* system timecode source */

int sts_create(int64_t startPosition, MediaSource** source);




#ifdef __cplusplus
}
#endif


#endif

