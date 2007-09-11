#ifndef __YUVLIB_FFMPEG__
#define __YUVLIB_FFMPEG__

#include <ffmpeg/avformat.h>

#include "YUV_frame.h"

/* Set a YUV_frame to point at the contents of an ffmpeg AVFrame.
 */
extern int YUV_frame_from_AVFrame(YUV_frame* dest,
                                  AVCodecContext *avctx, AVFrame* source);

#endif // __YUVLIB_FFMPEG__

