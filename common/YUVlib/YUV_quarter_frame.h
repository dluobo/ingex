#ifndef __YUVLIB_QUARTER_FRAME__
#define __YUVLIB_QUARTER_FRAME__

#include "YUV_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

// Make a 2:1 down coonverted copy of in_frame in out_frame at position x,y
void quarter_frame(YUV_frame* in_frame, YUV_frame* out_frame,
                   int x, int y, int intlc, int hfil, int vfil,
                   void* workSpace);

#ifdef __cplusplus
}
#endif

#endif // __YUVLIB_QUARTER_FRAME__
