/*
 * $Id: VideoRaster.h,v 1.2 2010/07/21 16:29:33 john_f Exp $
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

        PAL,
        PAL_592,
        PAL_608,
        PAL_B,     ///< PAL modified to be bottom field first
        PAL_592_B, ///< PAL_592 modified to be bottom field first
        PAL_608_B, ///< PAL_608 modified to be bottom field first
        NTSC,

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

