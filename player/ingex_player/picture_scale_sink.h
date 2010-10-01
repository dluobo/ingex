/*
 * $Id: picture_scale_sink.h,v 1.1 2010/10/01 15:56:21 john_f Exp $
 *
 * Copyright (C) 2010 British Broadcasting Corporation, All Rights Reserved
 *
 * Author: Philip de Nier
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __PICTURE_SCALE_SINK_H__
#define __PICTURE_SCALE_SINK_H__


#include "media_sink.h"



typedef struct PictureScaleSink PictureScaleSink;

typedef enum
{
    PSS_UNKNOWN_RASTER = 0,
    PSS_SD_625_RASTER,
    PSS_SD_525_RASTER,
    PSS_HD_1080_RASTER,
} PSSRaster;



int pss_create_picture_scale(MediaSink *target_sink, int apply_scale_filter, int use_display_dimensions,
                             PictureScaleSink **sink);
MediaSink* pss_get_media_sink(PictureScaleSink *sink);

void pss_set_target_raster(PictureScaleSink *sink, PSSRaster raster);



#endif
