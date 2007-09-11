#ifndef __MULTIPLE_SOURCES_H__
#define __MULTIPLE_SOURCES_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"


typedef struct MultipleMediaSources MultipleMediaSources;


int mls_create(const Rational* aspectRatio, int64_t maxLength, MultipleMediaSources** multSource);
int mls_assign_source(MultipleMediaSources* multSource, MediaSource** source);
MediaSource* mls_get_media_source(MultipleMediaSources* multSource);
int mls_finalise_blank_sources(MultipleMediaSources* multSource);



#ifdef __cplusplus
}
#endif


#endif


