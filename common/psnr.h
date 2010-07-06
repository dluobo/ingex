/*
 * $Id: psnr.h,v 1.2 2010/07/06 14:15:13 john_f Exp $
 *
 * Calculate Y, Cb and Cr PSNR figures from two uncompressed 'UYVY' files
 * as produced by e.g. mpeg2decode -o1 and Targa 3200 uncompressed capture.
 *
 * Output is of the form:   frame   Y-PSNR   Cb-PSNR   Cr-PSNR
 */

/*
 * Copyright (C) 2003 British Braodcasting Corporation
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

#ifndef common_psnr_h
#define common_psnr_h

#ifdef __cplusplus
extern "C" {
#endif

int psnr_uyvy(const uint8_t *data1, const uint8_t *data2, int width, int height, double *y, double *u, double *v);
int psnr_yuv(const uint8_t *data1, const uint8_t *data2, int format, int width, int height, double *y, double *u, double *v);

#ifdef __cplusplus
}
#endif

#endif //ifndef common_psnr_h

