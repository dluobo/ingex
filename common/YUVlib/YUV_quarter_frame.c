#include "YUV_frame.h"
#include "YUV_quarter_frame.h"
#include "YUV_small_pic.h"

void quarter_frame(YUV_frame* in_frame, YUV_frame* out_frame,
                   int x, int y, int intlc, int hfil, int vfil,
                   void* workSpace)
{
    small_pic(in_frame, out_frame, x, y, 2, 2,
              intlc, hfil ? 1 : 0, vfil ? 2 : 0, workSpace);
}
