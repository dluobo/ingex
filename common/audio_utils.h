/*
 * $Id: audio_utils.h,v 1.1 2007/10/26 16:48:36 john_f Exp $
 *
 * Write uncompressed audio in WAV format, and update WAV header.
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

#ifndef  AUDIO_UTILS_H
#define AUDIO_UTILS_H

#include <stdio.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int writeWavHeader(FILE *fp, int bits_per_sample, int num_ch);
extern void update_WAV_header(FILE *fp);
extern void write_audio(FILE *fp, uint8_t *p, int num_samples, int bits_per_sample);


#ifdef __cplusplus
}
#endif

#endif /* AUDIO_UTILS_H */
