#ifndef __YUVLIB_SCALE_PIC__
#define __YUVLIB_SCALE_PIC__

#include "YUV_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Make a different size copy of in_frame in out_frame at position x,y.
 * The trade off between quality and speed is set by hfil & vfil. Fastest
 * mode is simple subsampling, selected with value 0. Simple area averaging
 * filtering is selected with value 1.
 * If the video is interlaced, setting intlc to 1 will have each field
 * processed separately.
 * workSpace is allocated by the caller and must be large enough to store
 * 2 lines of output as uint32_t, i.e.
 * (2 * (in_frame->w * yup / ydown) * 4) bytes.
 * Return value is 0 for success, <0 for failure.
 */
int resize_pic(YUV_frame* in_frame, YUV_frame* out_frame,
               int x, int y, int xup, int xdown, int yup, int ydown,
               int intlc, int hfil, int vfil, void* workSpace);

/* Make a different size copy of in_frame in out_frame at position x,y and
 * size w x h.
 * The trade off between quality and speed is set by hfil & vfil. Fastest
 * mode is simple subsampling, selected with value 0. Simple area averaging
 * filtering is selected with value 1.
 * If the video is interlaced, setting intlc to 1 will have each field
 * processed separately.
 * workSpace is allocated by the caller and must be large enough to store
 * 2 lines of output as uint32_t, i.e. (2 * w * 4) bytes.
 * Return value is 0 for success, <0 for failure.
 */
int scale_pic(YUV_frame* in_frame, YUV_frame* out_frame,
              int x, int y, int w, int h,
              int intlc, int hfil, int vfil, void* workSpace);

#ifdef __cplusplus
}
#endif

#endif // __YUVLIB_SCALE_PIC__
