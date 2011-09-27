#ifndef __VIDEO_CONVERSION_10BITS_H__
#define __VIDEO_CONVERSION_10BITS_H__

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This routine converts a frame of 10-bit YUV "v210" format data
 * to 8-bit UYVY format. It uses error feedback to minimise visibility
 * of any quantising noise.
 *
 * See http://developer.apple.com/quicktime/icefloe/dispatch019.html#v210
 * for a definition of v210 format.
 * See http://www.fourcc.org/yuv.php#UYVY for a definition of UYVY.
 *
 * pInFrame and pOutFrame are pointers to the first sample of the input
 * and output frame buffers.
 * StrideIn and StrideOut are the width of the input and output buffers,
 * i.e. how many *bytes* separate the start of two adjacent lines.
 * xLen and yLen are the image dimensions in pixels and lines.
 */
void DitherFrameV210(uint8_t *pOutFrame, const uint8_t *pInFrame,
				     const int StrideOut, const int StrideIn,
				     const int xLen, const int yLen);

/*
 * This routine converts a frame of 10-bit (coded in 16-bit) YUV planar
 * format data to 8-bit YUV planar format. It uses error feedback to
 * minimise visibility of any quantising noise.
 * ssx and ssy are the horizontal and vertical sub-sampling factors,
 * e.g. ssx=2,ssy=1 for 4:2:2 and ssx=2,ssy=2 for 4:2:0
 */
void DitherFrameYUV10(uint8_t *pOutFrame,
                      const uint16_t *pYIn, const uint16_t *pUIn, const uint16_t *pVIn,
                      const int yStrideIn, int uStrideIn, int vStrideIn,
				      const int xLen, const int yLen,
				      const int ssx, const int ssy);

void DitherFrameYUV10_2(uint8_t *pOutFrame, const uint16_t *pInFrame,
				        const int xLen, const int yLen,
				        const int ssx, const int ssy);


/*
 * This does the same as DitherFrameV210, but with no error feedback to mask
 * quantisation error.
 */
void ConvertFrameV210to8(uint8_t *pOutFrame, const uint8_t *pInFrame,
                         const int StrideOut, const int StrideIn,
                         const int xLen, const int yLen);

/*
 * This does the same as DitherFrameYUV, but with no error feedback to mask
 * quantisation error.
 */
void ConvertFrameYUV10to8(uint8_t *pOutFrame,
                          const uint16_t *pYIn, const uint16_t *pUIn, const uint16_t *pVIn,
                          const int yStrideIn, int uStrideIn, int vStrideIn,
                          const int xLen, const int yLen,
                          const int ssx, const int ssy);

void ConvertFrameYUV10to8_2(uint8_t *pOutFrame, const uint16_t *pInFrame,
                            const int xLen, const int yLen,
                            const int ssx, const int ssy);

/*
 * This routine converts a frame of 8-bit UYVY data to 10-bit v210 format.
 * Zeros are inserted as required. Converting an 8-bit frame to 10-bit and back
 * should have no overall effect.
*/
void ConvertFrame8toV210(uint8_t *pOutFrame, const uint8_t *pInFrame,
                         const int StrideOut, const int StrideIn,
                         const int xLen, const int yLen);

/*
 * This routine converts a frame of 8-bit YUV planar format data to 10-bit
 * (coded in 16-bit) YUV planar format. 
 * Zeros are inserted as required. Converting an 8-bit frame to 10-bit and back
 * should have no overall effect.
*/
void ConvertFrame8toYUV10(uint16_t *pOutFrame,
                          const uint8_t *pYIn, const uint8_t *pUIn, const uint8_t *pVIn,
                          const int yStrideIn, int uStrideIn, int vStrideIn,
                          const int xLen, const int yLen,
                          const int ssx, const int ssy);

void ConvertFrame8toYUV10_2(uint16_t *pOutFrame, const uint8_t *pInFrame,
                            const int xLen, const int yLen,
                            const int ssx, const int ssy);

#ifdef __cplusplus
}
#endif

#endif // __VIDEO_CONVERSION_10BITS_H__
