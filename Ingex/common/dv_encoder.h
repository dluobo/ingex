/*
 * $Id: dv_encoder.h,v 1.1 2007/01/30 12:12:04 john_f Exp $
 *
 * Encode uncompressed video to DV frames using libavcodec
 *
 * Copyright (C) 2005  British Broadcasting Corporation
 * All Rights Reserved
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

#ifndef  DV_ENCODER_H
#define DV_ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void dv_encoder_t;
enum dv_encoder_resolution { DV_ENCODER_RESOLUTION_DV25, DV_ENCODER_RESOLUTION_DV50 };

/*
* dv_encoder_init : Creates a format context for a dv avcodec
* Input            : filename  - file name to write dv frames
* Return           : Pointer to object if successful
*                    NULL if a problem occurred
*/
extern dv_encoder_t * dv_encoder_init (enum dv_encoder_resolution res);

/*
* dv_encoder_video : Encodes the input video
* Input            : in_dv  - dv_encoder_t object returned by dv_encoder_init
*                  : p_video - pointer to one frame of 4:2:2 video data
*                  : int32_t - frame number for DV frame header
* Return           : 0 if operation is successful
*                  : -1 if operation failed
*                   NOTE: expects an SD VIDEO frames in planar 4:2:2 format
*/
extern int dv_encoder_encode (dv_encoder_t *in_dv, unsigned char *p_video, unsigned char * *pp_enc_video, int frame_number);

/*
* dv_encoder_close : Releases resources allocated by the dv_encoder_init
*                  : function
* Input            : in_dv  - dv_encoder_t object returned by dv_encoder_init
* Return           : compressed size (bytes) if operation is successful
*                  : -1 if operation failed
*/
extern int dv_encoder_close (dv_encoder_t *in_dv);

#ifdef __cplusplus
}
#endif

#endif /* DV_ENCODER_H */

