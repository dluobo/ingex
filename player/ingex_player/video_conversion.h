#ifndef __VIDEO_CONVERSION_H__
#define __VIDEO_CONVERSION_H__


#ifdef __cplusplus
extern "C" 
{
#endif

#include <ffmpeg/avcodec.h>
#include "frame_info.h"


void uyvy_to_yuv420(int width, int height, uint8_t *input, uint8_t *output);

void yuv4xx_to_uyvy(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output);

void yuv422_to_uyvy(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output);

void yuv444_to_uyvy(int width, int height, uint8_t *input, uint8_t *output);


void yuv4xx_to_yuv4xx(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output);

void yuv422_to_yuv422(int width, int height, int shift_picture_up, AVFrame *input, uint8_t *output);


void fill_black(StreamFormat format, int width, int height, unsigned char* image);



#ifdef __cplusplus
}
#endif


#endif

