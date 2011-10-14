/*
 * $Id: VideoRaster.h,v 1.6 2011/10/14 09:54:18 john_f Exp $
 *
 * Video raster codes and details
 *
 * Copyright (C) 2010  British Broadcasting Corporation
 * All rights reserved
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#ifndef VideoRaster_h
#define VideoRaster_h

#include "Rational.h"

#include <string>

namespace Ingex
{

class Interlace
{
public:
    enum EnumType
    {
        NONE,
        TOP_FIELD_FIRST,
        BOTTOM_FIELD_FIRST
    };

    static std::string Name(Ingex::Interlace::EnumType e);
};

class VideoRaster
{
public:
    enum EnumType
    {
        NONE,

        // PAL formats
        PAL_4x3,
        PAL_16x9,
        // extended VBI variants
        PAL_592_4x3,
        PAL_592_16x9,
        PAL_608_4x3,
        PAL_608_16x9,
        // bottom-field-first variants
        PAL_4x3_B,
        PAL_16x9_B,
        PAL_592_4x3_B,
        PAL_592_16x9_B,
        PAL_608_4x3_B,
        PAL_608_16x9_B,

        // NTSC formats
        NTSC_4x3,
        NTSC_16x9,
        // extended VBI variants
        NTSC_502_4x3,
        NTSC_502_16x9,

        // HD formats
        SMPTE274_25I,
        SMPTE274_29I,
        SMPTE274_25PSF,
        SMPTE274_29PSF,
        SMPTE274_25P,
        SMPTE274_29P,
        
        SMPTE296_50P,
        SMPTE296_59P
    };

    static std::string Name(Ingex::VideoRaster::EnumType e);

    static void GetInfo(VideoRaster::EnumType raster,
            int & width, int & height, int & fps_num, int & fps_den, Interlace::EnumType & interlace);

    static bool IsRec601(VideoRaster::EnumType raster);

    static bool Is4x3(VideoRaster::EnumType raster);

    static void ModifyLineShift(VideoRaster::EnumType & raster, bool shifted);

    static int LineShift(VideoRaster::EnumType raster);

    static Ingex::Rational SampleAspectRatio(VideoRaster::EnumType raster);
};

class PixelFormat
{
public:
    enum EnumType
    {
        NONE,
        UYVY_422,
        YUV_PLANAR_422,
        YUV_PLANAR_420_MPEG,
        YUV_PLANAR_420_DV,
        YUV_PLANAR_411
    };

    static std::string Name(Ingex::PixelFormat::EnumType e);
};

} // namespace Ingex

#endif //ifndef VideoRaster_h

