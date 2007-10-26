/*
 * $Id: psnr.c,v 1.1 2007/10/26 16:49:58 john_f Exp $
 *
 * Calculate Y, Cb and Cr PSNR figures from two uncompressed 'UYVY' files
 * as produced by e.g. mpeg2decode -o1 and Targa 3200 uncompressed capture.
 *
 * Output is of the form:   frame   Y-PSNR   Cb-PSNR   Cr-PSNR
 */

/*
 * Copyright (C) 2003 Peter Brightwell <peter.brightwell@bbc.co.uk>
 * and Stuart Cunningham <stuart_hc@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <inttypes.h>
#include <math.h>

/* returns 0 if frames identical */
int psnr_uyvy(const uint8_t *data1, const uint8_t *data2, int width, int height, double *y, double *u, double *v)
{
	int			i;
	int			y_size = width * height;
	int			u_size = width * height / 2;
	int			v_size = width * height / 2;
	int			frame_size = width * height * 2;
	uint64_t	ySumSqDiff = 0, cbSumSqDiff = 0, crSumSqDiff = 0;

	// In UYVY format data is packed as Cb,Y,Cr,Y
	for (i = 0; i < frame_size; i += 4) {
		cbSumSqDiff += ( data2[i] - data1[i] ) * ( data2[i] - data1[i] );
		ySumSqDiff += ( data2[i+1] - data1[i+1] ) * ( data2[i+1] - data1[i+1] );
		crSumSqDiff += ( data2[i+2] - data1[i+2] ) * ( data2[i+2] - data1[i+2] );
		ySumSqDiff += ( data2[i+3] - data1[i+3] ) * ( data2[i+3] - data1[i+3] );
	}

	if (ySumSqDiff == 0 && cbSumSqDiff == 0 && crSumSqDiff == 0) {
		return 0;
	}

	*y = 20.0 * log10( 255.0 / sqrt((double)ySumSqDiff / y_size) );
	*u = 20.0 * log10( 255.0 / sqrt((double)cbSumSqDiff / u_size) );
	*v = 20.0 * log10( 255.0 / sqrt((double)crSumSqDiff / v_size) );
	return 1;
}

/* format: 420, 422, 444 */
/* returns 0 if frames identical */
int psnr_yuv(const uint8_t *data1, const uint8_t *data2, int format, int width, int height, double *y, double *u, double *v)
{
	int			i;
	int			y_size = 0, u_size = 0, v_size = 0;
	uint64_t    ySumSqDiff = 0, crSumSqDiff = 0, cbSumSqDiff = 0;

	if (format == 420) {
		y_size = width * height;
		u_size = width * height / 4;
		v_size = width * height / 4;
	}
	else if (format == 420) {
		y_size = width * height;
		u_size = width * height / 2;
		v_size = width * height / 2;

	}
	else if (format == 444) {
		y_size = width * height;
		u_size = width * height;
		v_size = width * height;
	}

	for (i = 0; i < y_size; i++) {
		int diff = data2[i] - data1[i];
		ySumSqDiff += diff * diff;
	}
	
	for (i = y_size; i < y_size + u_size; i++) {
		int diff = data2[i] - data1[i];
		cbSumSqDiff += diff * diff;
	}
	
	for (i = y_size + u_size; i < y_size + u_size + v_size; i++) {
		int diff = data2[i] - data1[i];
		crSumSqDiff += diff * diff;
	}

	if (ySumSqDiff == 0 && cbSumSqDiff == 0 && crSumSqDiff == 0) {
		return 0;
	}

	*y = 20.0 * log10( 255.0 / sqrt((double)ySumSqDiff / y_size) );
	*u = 20.0 * log10( 255.0 / sqrt((double)cbSumSqDiff / u_size) );
	*v = 20.0 * log10( 255.0 / sqrt((double)crSumSqDiff / v_size) );
	return 1;
}
