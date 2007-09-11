#ifndef __BUFFERED_MEDIA_SOURCE_H__
#define __BUFFERED_MEDIA_SOURCE_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"
#include "media_player.h"


typedef struct BufferedMediaSource BufferedMediaSource;


int bmsrc_create(MediaSource* targetSource, int size, int blocking, float byteRateLimit,
    BufferedMediaSource** bufSource);
MediaSource* bmsrc_get_source(BufferedMediaSource* bufSource);
void bmsrc_set_media_player(BufferedMediaSource* bufSource, MediaPlayer* player);



#ifdef __cplusplus
}
#endif


#endif


