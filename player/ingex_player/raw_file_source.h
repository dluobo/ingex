#ifndef __RAW_FILE_SOURCE_H__
#define __RAW_FILE_SOURCE_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"


/* raw file source */

int rfs_open(const char* filename, const StreamInfo* streamInfo, MediaSource** source);




#ifdef __cplusplus
}
#endif


#endif

