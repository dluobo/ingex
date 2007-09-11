#ifndef __PLANAR_YUV_IO__
#define __PLANAR_YUV_IO__

#include "YUV_frame.h"

int read_all(int fd, void *buf, size_t count);
int write_all(int fd, const void *buf, size_t count);
int skip_frames(YUV_frame* frame, int fd, int frames);
int read_frame(YUV_frame* frame, int fd);
int write_frame(YUV_frame* frame, int fd);

#endif // __PLANAR_YUV_IO__
