/*
 * $Id: ffmpeg_resolutions.h,v 1.1 2010/06/02 13:10:46 john_f Exp $
 *
 * Info on ffmpeg parameters for a particular MaterialResolution
 *
 * Copyright (C) 2010  British Broadcasting Corporation
 * All Rights Reserved.
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


#ifndef ffmpeg_resolutions_h
#define ffmpeg_resolutions_h

#ifdef __cplusplus
extern "C" {
#endif

#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/avcodec.h>
#else
#include <libavcodec/avcodec.h>
#endif

#ifdef __cplusplus
}
#endif

#include "MaterialResolution.h"
#include "VideoRaster.h"

void get_ffmpeg_params(MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster, CodecID & codec_id, CodecType & codec_type, ::PixelFormat & pix_fmt);

#endif //ifndef ffmpeg_resolutions_h

