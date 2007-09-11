#ifndef __BOUNCING_BALL_SOURCE_H__
#define __BOUNCING_BALL_SOURCE_H__



#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"


/* bouncing ball source */

int bbs_create(const StreamInfo* videoStreamInfo, int64_t length, int numBalls, MediaSource** source);




#ifdef __cplusplus
}
#endif







#endif


