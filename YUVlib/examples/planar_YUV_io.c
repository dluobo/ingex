#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#include <unistd.h> /* read(), write() lseek() */
#include <stdio.h>  /* perror() */

#include "planar_YUV_io.h"

int read_all(int fd, void *buf, size_t count)
{
    size_t  n;

    while (count > 0)
    {
        n = read(fd, buf, count);
        if (n == 0)
            return 0;
        if (n < 0)
        {
            perror("read");
            return -1;
        }
        buf = buf + n;
        count = count - n;
    }
    return 1;
}

int write_all(int fd, const void *buf, size_t count)
{
    size_t  n;

    while (count > 0)
    {
        n = write(fd, buf, count);
        if (n < 0)
        {
            perror("write");
            return -1;
        }
        buf = buf + n;
        count = count - n;
    }
    return 1;
}

int skip_frames(YUV_frame* frame, int fd, int frames)
{
    if (lseek(fd, frames *
              ((frame->Y.w * frame->Y.h) +
               (frame->U.w * frame->U.h) +
               (frame->V.w * frame->V.h)),
              SEEK_SET) < 0)
    {
        perror("lseek");
        return -1;
    }
    return 1;
}

int read_frame(YUV_frame* frame, int fd)
{
    unsigned char*  ptr;
    int         result;
    int         y;

    ptr = frame->Y.buff;
    for (y = 0; y < frame->Y.h; y++)
    {
        result = read_all(fd, ptr, frame->Y.w);
        if (result < 1)
            return result;
        ptr += frame->Y.lineStride;
    }
    ptr = frame->U.buff;
    for (y = 0; y < frame->U.h; y++)
    {
        result = read_all(fd, ptr, frame->U.w);
        if (result < 1)
            return result;
        ptr += frame->U.lineStride;
    }
    ptr = frame->V.buff;
    for (y = 0; y < frame->V.h; y++)
    {
        result = read_all(fd, ptr, frame->V.w);
        if (result < 1)
            return result;
        ptr += frame->V.lineStride;
    }
    return result;
}

int write_frame(YUV_frame* frame, int fd)
{
    unsigned char*  ptr;
    int         result;
    int         y;

    ptr = frame->Y.buff;
    for (y = 0; y < frame->Y.h; y++)
    {
        result = write_all(fd, ptr, frame->Y.w);
        if (result < 1)
            return result;
        ptr += frame->Y.lineStride;
    }
    ptr = frame->U.buff;
    for (y = 0; y < frame->U.h; y++)
    {
        result = write_all(fd, ptr, frame->U.w);
        if (result < 1)
            return result;
        ptr += frame->U.lineStride;
    }
    ptr = frame->V.buff;
    for (y = 0; y < frame->V.h; y++)
    {
        result = write_all(fd, ptr, frame->V.w);
        if (result < 1)
            return result;
        ptr += frame->V.lineStride;
    }
    return result;
}
