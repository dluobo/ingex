#ifndef __CLIP_SOURCE_H__
#define __CLIP_SOURCE_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"


/* clip source */

typedef struct ClipSource ClipSource;


int cps_create(MediaSource* targetSource, int64_t start, int64_t duration, ClipSource** clipSource);
MediaSource* cps_get_media_source(ClipSource* clipSource);


#ifdef __cplusplus
}
#endif


#endif

