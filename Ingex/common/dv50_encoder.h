/*
 * $Id: dv50_encoder.h,v 1.1 2006/12/19 16:48:20 john_f Exp $
 *
 * Encode uncompressed 4:2:2 video to DV50 frames using libavcodec
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

#ifndef  DV50_ENCODER_H
#define DV50_ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void dv50_encoder_t;

/*
* dv50_encoder_init : Creates a format context for a dv50 avcodec
* Input            : filename  - file name to write dv50 frames
* Return           : Pointer to object if successful
*                    NULL if a problem occurred
*/
//extern dv50_encoder_t *dv50_encoder_init (const char *filename);
extern dv50_encoder_t *dv50_encoder_init ();

/*
* dv50_encoder_video : Encodes the input video
* Input            : in_dv50  - dv50_encoder_t object returned by dv50_encoder_init
*                  : p_video - pointer to one frame of 4:2:2 video data
*                  : int32_t - frame number for DV frame header
* Return           : 0 if operation is successful
*                  : -1 if operation failed
*					NOTE: expects an SD VIDEO frames in planar 4:2:2 format
*/
//extern int dv50_encoder_encode (dv50_encoder_t *in_dv50, unsigned char *p_video, int frame_number);
extern int dv50_encoder_encode (dv50_encoder_t *in_dv50, unsigned char *p_video, unsigned char * *pp_enc_video, int compressed_size, int frame_number);

/*
* dv50_encoder_close : Releases resources allocated by the dv50_encoder_init
*                   : function
* Input            : in_dv50  - dv50_encoder_t object returned by dv50_encoder_init
* Return           : 0 if operation is successful
*                  : -1 if operation failed
*/
extern int dv50_encoder_close (dv50_encoder_t *in_dv50);

#ifdef __cplusplus
}
#endif

#endif /* DV50_ENCODER_H */
