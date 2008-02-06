/*
 * $Id: video_conversion.c,v 1.2 2008/02/06 16:58:51 john_f Exp $
 *
 * MMX optimised video format conversion functions
 *
 * Copyright (C) 2005  Stuart Cunningham <stuart_hc@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
 
#include <inttypes.h>
#include <string.h>

#ifdef __MMX__
#include <mmintrin.h>
#endif


#include "video_conversion.h"


#ifdef __MMX__

void uyvy_to_yuv422(int width, int height, int shift_picture_down, uint8_t *input, uint8_t *output)
{
	__m64 chroma_mask = _mm_set_pi8(255, 0, 255, 0, 255, 0, 255, 0);
	__m64 luma_mask = _mm_set_pi8(0, 255, 0, 255, 0, 255, 0, 255);
	uint8_t *orig_input = input;
	uint8_t *y_comp = output;
	uint8_t *u_comp = output + width * height;
	uint8_t *v_comp = u_comp + (int)((width * height)/2);	// 4:2:2
	int i, j;

	// When preparing video for PAL DV50 encoding, the video must be shifted
	// down by one line to change the field order to be bottom-field-first
	int start_line = 0;
	if (shift_picture_down) {
		memset(y_comp, 0x10, width);		// write one line of black Y
		y_comp += width;
		memset(u_comp, 0x80, width/2);		// write one line of black U,V
		u_comp += width/2;
		memset(v_comp, 0x80, width/2);		// write one line of black U,V
		v_comp += width/2;
		start_line = 1;
	}

	/* Do the y component */
	for (j = start_line; j < height; j++)
	{
		// Consume 16 bytes of UYVY data per iteration (8 pixels worth)
		for (i = 0; i < width*2; i += 16)
		{
			//__m64 m1 = _mm_and_si64 (*(__m64 *)input, luma_mask);
			//__m64 m2 = _mm_and_si64 (*(__m64 *)(input+8), luma_mask);
			//__m64 m2 = _mm_set_pi8 (0, 0, 0, 0, 0, 0, 0, 0);
			//*(__m64 *)y_comp = _mm_packs_pu16 (m2, m1);
			__m64 m0 = *(__m64 *)input;
			__m64 m2 = _mm_srli_si64(m0, 8);
			__m64 m3 = _mm_slli_si64(m0, 8);
			m3 = _mm_and_si64 (m3, chroma_mask);
			m2 = _mm_and_si64 (m2, luma_mask);
			m2 = _mm_or_si64 (m2, m3);
			m2= _mm_and_si64 (m2, luma_mask);
			m0 = m2;
			__m64 m1 = *(__m64 *)(input+8);
			m2 = _mm_srli_si64(m1, 8);
			m3 = _mm_slli_si64(m1, 8);
			m3 = _mm_and_si64 (m3, chroma_mask);
			m2 = _mm_and_si64 (m2, luma_mask);
			m2 = _mm_or_si64 (m2, m3);
			m2= _mm_and_si64 (m2, luma_mask);
			m1 = m2;
			*(__m64 *)y_comp = _mm_packs_pu16 (m0, m1);

			y_comp += 8;
			input += 16;
		}
	}
	/* Do the chroma components */
	input = orig_input;
	for (j = start_line; j < height; j++)
	{
		/* Process every line for yuv 4:2:2 */
		for (i = 0; i < width*2; i += 16)
		{
			__m64 m1 = _mm_unpacklo_pi8 (*(__m64 *)input, *(__m64 *)(input+8));
			__m64 m2 = _mm_unpackhi_pi8 (*(__m64 *)input, *(__m64 *)(input+8));

			__m64 m3 = _mm_unpacklo_pi8 (m1, m2);
			__m64 m4 = _mm_unpackhi_pi8 (m1, m2);
			//*(__m64 *)u_comp = _mm_unpacklo_pi8 (m1, m2);
			//*(__m64 *)v_comp = _mm_unpackhi_pi8 (m1, m2);
			memcpy (u_comp, &m3, 4);
			memcpy (v_comp, &m4, 4);
			u_comp += 4;
			v_comp += 4;
			input += 16;
		}
	}
	_mm_empty();        // Clear aliased fp register state
}

void uyvy_to_yuv420(int width, int height, int shift_picture_down, uint8_t *input, uint8_t *output)
{
	__m64 chroma_mask = _mm_set_pi8(255, 0, 255, 0, 255, 0, 255, 0);
	__m64 luma_mask = _mm_set_pi8(0, 255, 0, 255, 0, 255, 0, 255);
	uint8_t *orig_input = input;
	uint8_t *y_comp = output;
	uint8_t *u_comp = output + width * height;
	uint8_t *v_comp = u_comp + (int)((width * height)/4);	// 4:2:0
	int i, j;

	// When preparing video for PAL DV25 encoding, the video must be shifted
	// down by one line to change the field order to be bottom-field-first
	int start_line = 0;
	if (shift_picture_down) {
		memset(y_comp, 0x10, width);		// write one line of black Y
		y_comp += width;
		memset(u_comp, 0x80, width/2);		// write one line of black U,V
		u_comp += width/2;
		memset(v_comp, 0x80, width/2);		// write one line of black U,V
		v_comp += width/2;
		start_line = 1;
	}

	/* Do the y component */
	for (j = start_line; j < height; j++)
	{
		// Consume 16 bytes of UYVY data per iteration (8 pixels worth)
		for (i = 0; i < width*2; i += 16)
		{
			//__m64 m1 = _mm_and_si64 (*(__m64 *)input, luma_mask);
			//__m64 m2 = _mm_and_si64 (*(__m64 *)(input+8), luma_mask);
			//__m64 m2 = _mm_set_pi8 (0, 0, 0, 0, 0, 0, 0, 0);
			//*(__m64 *)y_comp = _mm_packs_pu16 (m2, m1);
			__m64 m0 = *(__m64 *)input;
			__m64 m2 = _mm_srli_si64(m0, 8);
			__m64 m3 = _mm_slli_si64(m0, 8);
			m3 = _mm_and_si64 (m3, chroma_mask);
			m2 = _mm_and_si64 (m2, luma_mask);
			m2 = _mm_or_si64 (m2, m3);
			m2= _mm_and_si64 (m2, luma_mask);
			m0 = m2;
			__m64 m1 = *(__m64 *)(input+8);
			m2 = _mm_srli_si64(m1, 8);
			m3 = _mm_slli_si64(m1, 8);
			m3 = _mm_and_si64 (m3, chroma_mask);
			m2 = _mm_and_si64 (m2, luma_mask);
			m2 = _mm_or_si64 (m2, m3);
			m2= _mm_and_si64 (m2, luma_mask);
			m1 = m2;
			*(__m64 *)y_comp = _mm_packs_pu16 (m0, m1);

			y_comp += 8;
			input += 16;
		}
	}
	/* Do the chroma components */
	input = orig_input;
	for (j = start_line; j < height; j++)
	{
		/* Skip every odd line to subsample to yuv 4:2:0 */
		if (j %2)
		{
			input += width*2;
			continue;
		}
		for (i = 0; i < width*2; i += 16)
		{
			__m64 m1 = _mm_unpacklo_pi8 (*(__m64 *)input, *(__m64 *)(input+8));
			__m64 m2 = _mm_unpackhi_pi8 (*(__m64 *)input, *(__m64 *)(input+8));

			__m64 m3 = _mm_unpacklo_pi8 (m1, m2);
			__m64 m4 = _mm_unpackhi_pi8 (m1, m2);
			//*(__m64 *)u_comp = _mm_unpacklo_pi8 (m1, m2);
			//*(__m64 *)v_comp = _mm_unpackhi_pi8 (m1, m2);
			memcpy (u_comp, &m3, 4);
			memcpy (v_comp, &m4, 4);
			u_comp += 4;
			v_comp += 4;
			input += 16;
		}
	}
	_mm_empty();        // Clear aliased fp register state
}

#else		// no MMX

// Convert UYVY -> Planar YUV 4:2:0 (fourcc=I420 or YV12)
// U0 Y0 V0 Y1   U2 Y2 V2 Y3   ->   Y0 Y1 Y2... U0 U2... V0 V2...
// but U and V are skipped every second line
void uyvy_to_yuv420(int width, int height, int shift_picture_down, uint8_t *input, uint8_t *output)
{
	int i;

	// TODO:
	// support shift_picture_down arg
	// by shifting picture down one line

	// Copy Y plane as is
	for (i = 0; i < width*height; i++)
	{
		output[i] = input[ i*2 + 1 ];
	}

	// Copy U & V planes, downsampling in vertical direction
	// by simply skipping every second line.
	// Each U or V plane is 1/4 the size of the Y plane.
	int i_macropixel = 0;
	for (i = 0; i < width*height / 4; i++)
	{
		output[width*height + i] = input[ i_macropixel*4 ];			// U
		output[width*height*5/4 + i] = input[ i_macropixel*4 + 2 ];	// V

		// skip every second line
		if (i_macropixel % (width) == (width - 1))
			i_macropixel += width/2;

		i_macropixel++;
	}
}
#endif		// __MMX__

#ifdef __MMX__
void yuv422_to_uyvy(int width, int height, int shift_picture_up, uint8_t *input, uint8_t *output)
{
	int i, j, start_line;
	uint8_t *y, *u, *v;

	y = input;
	u = input + width*height;
	v = input + width*height * 3/2;
	start_line = 0;

	// Shifting picture up one line is necessary when decoding PAL DV50
	if (shift_picture_up) {
		// Skip one line of input picture and start one line lower
		y += width;
		u += width / 2;
		v += width / 2;
		start_line = 1;
	}

	// Convert to UYVY
	for (j = start_line; j < height; j++)
	{
		for (i = 0; i < width*2; i += 32)
		{
			__m64 m0 = *(__m64 *)u;			// load UUUU UUUU(0)
			__m64 m1 = *(__m64 *)v;			// load VVVV VVVV(0)
			__m64 m2 = m0;					// copy U for mix op
			m0 = _mm_unpacklo_pi8(m0, m1);	// mix -> UVUV UVUV(0) (in m0)
			m2 = _mm_unpackhi_pi8(m2, m1);	// mix -> UVUV UVUV(8) (in m2)

			__m64 m3 = *(__m64 *)y;			// load YYYY YYYY(0)
			__m64 m5 = *(__m64 *)(y+8);		// load YYYY YYYY(8)
			__m64 m4 = m0;
			__m64 m6 = m2;
			m0 = _mm_unpacklo_pi8(m0, m3);	// mix to UYVY UYVY(0)
			m4 = _mm_unpackhi_pi8(m4, m3);	// mix to UYVY UYVY(8)
			m2 = _mm_unpacklo_pi8(m2, m5);	// mix to UYVY UYVY(16)
			m6 = _mm_unpackhi_pi8(m6, m5);	// mix to UYVY UYVY(24)

			*(__m64 *)(output+0) = m0;
			*(__m64 *)(output+8) = m4;
			*(__m64 *)(output+16) = m2;
			*(__m64 *)(output+24) = m6;

			u += 8;
			v += 8;
			y += 16;
			output += 32;
		}
	}
	_mm_empty();        // Clear aliased fp register state

	if (shift_picture_up) {
		// Fill bottom line with one line of black UYVY
		for (i = 0; i < width*2; i += 4) {
			*output++ = 0x80;
			*output++ = 0x10;
			*output++ = 0x80;
			*output++ = 0x10;
		}
	}
}
#endif


// Convert YUV444 (planar) to UYVY (packed 4:2:2) using naive conversion where
// alternate UV samples are simply discarded.
void yuv444_to_uyvy(int width, int height, uint8_t *input, uint8_t *output)
{
	int i, j;
	uint8_t *y, *u, *v;
	y = input;
	u = y + width * height;
	v = y + width * height * 2;

	// Convert to UYVY
	for (j = 0; j < height; j++) {
		for (i = 0; i < width; i++)
		{
			if (i % 2 == 0) {		// even pixels, copy U, Y, V
				*output++ = *u++;
				*output++ = *y++;
				*output++ = *v++;
			}
			else {					// odd pixels, copy just Y
				*output++ = *y++;
				u++;
				v++;
			}
		}
	}
}
