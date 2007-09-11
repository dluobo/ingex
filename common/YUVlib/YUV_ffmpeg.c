#include "YUV_frame.h"
#include "YUV_ffmpeg.h"

int YUV_frame_from_AVFrame(YUV_frame* dest,
                           AVCodecContext *avctx, AVFrame* source)
{
    int		ssx, ssy;

    switch (avctx->pix_fmt)
    {
    case PIX_FMT_YUV420P:
        ssx = 2;
        ssy = 2;
        break;
    case PIX_FMT_YUYV422: case PIX_FMT_YUV422P:
        ssx = 2;
        ssy = 1;
        break;
    case PIX_FMT_YUV444P:
        ssx = 1;
        ssy = 1;
        break;
    default:
        return YUV_unknown_format;
    }
    if ((avctx->width < 1) || (avctx->height < 1))
        return YUV_size_error;
    if ((avctx->width % ssx != 0) || (avctx->height % ssy != 0))
        return YUV_size_error;
    dest->Y.w = avctx->width;
    dest->Y.h = avctx->height;
    dest->Y.lineStride = source->linesize[0];
    dest->Y.buff = source->data[0];
    dest->U.w = avctx->width / ssx;
    dest->U.h = avctx->height / ssy;
    dest->U.lineStride = source->linesize[1];
    dest->U.buff = source->data[1];
    switch (avctx->pix_fmt)
    {
    case PIX_FMT_YUV420P: case PIX_FMT_YUV422P: case PIX_FMT_YUV444P:
        dest->Y.pixelStride = 1;
        dest->U.pixelStride = 1;
        break;
    case PIX_FMT_YUYV422:
        dest->Y.pixelStride = 2;
        dest->U.pixelStride = 4;
        break;
    default:
        return YUV_unknown_format;
    }
    dest->V = dest->U;
    dest->V.lineStride = source->linesize[2];
    dest->V.buff = source->data[2];
    return YUV_OK;
}
