/*
 * $Id: ffmpeg_defs.h,v 1.3 2011/06/13 15:27:19 john_f Exp $
 *
 * Common definitions for ffmpeg_encoder and ffmpeg_encoder_av
 *
 * Copyright (C) 2009  British Broadcasting Corporation
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

#ifndef FFMPEG_DEFS_H
#define FFMPEG_DEFS_H


/* special value for argument num_threads to ffmpeg_encoder_init */
#define THREADS_USE_BUILTIN_TUNING -1

/* Avid-compatible intra quantisation matrices for IMX */
const uint16_t imx30_intra_matrix[] = {
  8, 16, 16, 17, 19, 22, 26, 34,
  16, 16, 18, 19, 21, 26, 33, 37,
  19, 18, 19, 23, 21, 28, 35, 45,
  18, 19, 20, 22, 28, 35, 32, 49,
  18, 20, 23, 29, 32, 33, 44, 45,
  18, 20, 20, 25, 35, 39, 52, 58,
  20, 22, 23, 28, 31, 44, 51, 57,
  19, 21, 26, 30, 38, 48, 45, 75
};

const uint16_t imx4050_intra_matrix[] = {
  8, 16, 16, 17, 19, 22, 23, 31,
  16, 16, 17, 19, 20, 23, 29, 29,
  19, 17, 19, 22, 19, 25, 28, 35,
  17, 19, 19, 20, 25, 28, 25, 33,
  17, 19, 22, 25, 26, 25, 31, 31,
  17, 19, 17, 20, 26, 28, 35, 36,
  19, 19, 19, 20, 22, 29, 32, 35,
  16, 17, 19, 22, 25, 31, 28, 45 
};


#endif //#ifndef FFMPEG_DEFS_H


