#ifndef __UDP_MEM_SOURCE_H__
#define __UDP_MEM_SOURCE_H__

#ifdef __cplusplus
extern "C" 
{
#endif

#include "media_source.h"

/* udp network source */
int udp_open(const char *address, MediaSource** source);


#ifdef __cplusplus
}
#endif

#endif
