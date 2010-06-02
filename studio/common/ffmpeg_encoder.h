/*
 * $Id: ffmpeg_encoder.h,v 1.8 2010/06/02 13:10:46 john_f Exp $
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

#ifndef FFMPEG_ENCODER_H
#define FFMPEG_ENCODER_H

#include "integer_types.h"
#include "ffmpeg_defs.h"
#include "MaterialResolution.h"

typedef void ffmpeg_encoder_t;


/*
* ffmpeg_encoder_init : Creates a format context for an avcodec
* Input            : width and height refer to input picture
* Return           : Pointer to object if successful
*                    NULL if a problem occurred
*/
extern ffmpeg_encoder_t * ffmpeg_encoder_init(MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster, int num_threads);

/*
* ffmpeg_encoder_video : Encodes the input video
* Input            : in_dv  - ffmpeg_encoder_t object returned by ffmpeg_encoder_init
*                  : p_video - pointer to one frame of 4:2:2 video data
*                  : int32_t - frame number for DV frame header
* Return           : compressed size (bytes) if operation is successful
*                  : -1 if operation failed
*                   NOTE: expects an SD VIDEO frames in planar 4:2:2 format
*/
extern int ffmpeg_encoder_encode (ffmpeg_encoder_t * in_encoder, const uint8_t * p_video, uint8_t * * pp_enc_video);

/*
* encode audio
*/
extern int ffmpeg_encoder_encode_audio (ffmpeg_encoder_t * in_encoder, int num_samples, short * p_audio, uint8_t * * pp_enc_audio);

/*
* ffmpeg_encoder_close : Releases resources allocated by the ffmpeg_encoder_init
*                  : function
* Input            : in_dv  - ffmpeg_encoder_t object returned by ffmpeg_encoder_init
* Return           : compressed size (bytes) if operation is successful
*                  : -1 if operation failed
*/
extern int ffmpeg_encoder_close (ffmpeg_encoder_t * in_encoder);


#endif /* FFMPEG_ENCODER_H */

