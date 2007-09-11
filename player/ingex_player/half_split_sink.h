#ifndef __HALF_SPLIT_SINK_H__
#define __HALF_SPLIT_SINK_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_sink.h"

typedef enum
{
    CONTINUOUS_SPLIT_TYPE = 0, /* second image continues from where the first left off */
    SINGLE_PAN_SPLIT_TYPE, /* second image shown starts from the top/left */
    DUAL_PAN_SPLIT_TYPE  /* split stays in middle and images pan */
} HalfSplitType;


int hss_create_half_split(MediaSink* sink, int vertical, HalfSplitType type, int showSplitDivide, HalfSplitSink** split);
MediaSink* hss_get_media_sink(HalfSplitSink* split);


void hss_set_half_split_orientation(HalfSplitSink* split, int vertical);
void hss_set_half_split_type(HalfSplitSink* split, int type);
void hss_show_half_split(HalfSplitSink* split, int showSplitDivide);
void hss_move_half_split(HalfSplitSink* split, int rightOrDown, int speed);


#ifdef __cplusplus
}
#endif


#endif


