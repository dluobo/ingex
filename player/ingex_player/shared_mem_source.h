#ifndef __SHARED_MEM_SOURCE_H__
#define __SHARED_MEM_SOURCE_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"


/* shared memory source */

int shared_mem_open(const char *card_name, MediaSource** source);




#ifdef __cplusplus
}
#endif


#endif

