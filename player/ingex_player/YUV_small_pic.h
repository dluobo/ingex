#ifndef __YUVLIB_SMALL_PIC__
#define __YUVLIB_SMALL_PIC__

#include "YUV_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Make a reduced size copy of in_frame in out_frame at position x,y.
 * The reduction ratio is set by hsub and vsub, which should be 2 or 3.
 * The trade off between quality and speed is set by hfil & vfil. Fastest
 * mode is simple subsampling, selected with value 0. Simple area averaging
 * filtering is selected with value 1.
 * If the video is interlaced, setting intlc to 1 will have each field
 * processed separately.
 * 2:1 vertical reduction with interlace has a filter mode 2 which gets
 * the two fields correctly positioned.
 * workSpace is allocated by the caller, and must be large enough to store
 * three lines of one component, i.e. in_frame->Y.w * 3 bytes.
 * Return value is 0 for success, <0 for failure (e.g. invalid hsub or vsub)
 */
int small_pic(YUV_frame* in_frame, YUV_frame* out_frame,
              int x, int y, int hsub, int vsub,
              int intlc, int hfil, int vfil, void* workSpace);

#ifdef __cplusplus
}
#endif

#endif // __YUVLIB_SMALL_PIC__
