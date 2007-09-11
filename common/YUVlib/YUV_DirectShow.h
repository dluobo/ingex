#ifndef __YUVLIB_DIRECTSHOW__
#define __YUVLIB_DIRECTSHOW__

// Interface between DirectShow video samples and simple video frame

// Get bit map info header from a media type
extern BITMAPINFOHEADER*
GetBMI(const AM_MEDIA_TYPE *mt);

// Assign a DirectShow YUV sample to a YUV_frame
extern HRESULT
YUV_frame_from_DirectShow(const CMediaType* pMT, BYTE* buff, YUV_frame* pFrame);

#endif // __YUVLIB_DIRECTSHOW__
