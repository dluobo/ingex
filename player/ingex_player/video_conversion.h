#ifndef __VIDEO_CONVERSION_H__
#define __VIDEO_CONVERSION_H__


#ifdef __cplusplus
extern "C" 
{
#endif

#if defined(HAVE_FFMPEG)

#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/avcodec.h>
#else
#include <libavcodec/avcodec.h>
#endif

#endif

#include "frame_info.h"


void uyvy_to_yuv420(int width, int height, uint8_t *input, uint8_t *output);

#if defined(HAVE_FFMPEG)

void yuv4xx_to_uyvy(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output);

void yuv422_to_uyvy(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output);

void yuv4xx_to_yuv4xx(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output);

void yuv422_to_yuv422(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output);

#endif /* defined(HAVE_FFMPEG) */

void yuv444_to_uyvy(int width, int height, uint8_t *input, uint8_t *output);

void yuv422_to_uyvy_2(int width, int height, int shift_picture_up, uint8_t *input, uint8_t *output);

void fill_black(StreamFormat format, int width, int height, unsigned char* image);

#ifdef __cplusplus
}
#endif


#endif

