/*
 * $Id: VideoRaster.cpp,v 1.7 2011/10/14 09:54:18 john_f Exp $
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

#include "VideoRaster.h"


std::string Ingex::Interlace::Name(Ingex::Interlace::EnumType e)
{
    std::string name;
    switch (e)
    {
    case Ingex::Interlace::NONE:
        name = "Progressive";
        break;
    case Ingex::Interlace::TOP_FIELD_FIRST:
        name = "Interlaced (top field first)";
        break;
    case Ingex::Interlace::BOTTOM_FIELD_FIRST:
        name = "Interlaced (bottom field first)";
        break;
    }
    return name;
}

std::string Ingex::PixelFormat::Name(Ingex::PixelFormat::EnumType e)
{
    std::string name;
    switch (e)
    {
    case Ingex::PixelFormat::NONE:
        name = "None";
        break;
    case Ingex::PixelFormat::UYVY_422:
        name = "UYVY 4:2:2";
        break;
    case Ingex::PixelFormat::YUV_PLANAR_422:
        name = "YUV Planar 4:2:2";
        break;
    case Ingex::PixelFormat::YUV_PLANAR_420_MPEG:
        name = "YUV Planar 4:2:0 (MPEG sampling)";
        break;
    case Ingex::PixelFormat::YUV_PLANAR_420_DV:
        name = "YUV Planar 4:2:0 (DV sampling)";
        break;
    case Ingex::PixelFormat::YUV_PLANAR_411:
        name = "YUV Planar 4:1:1";
        break;
    }
    return name;
}

std::string Ingex::VideoRaster::Name(Ingex::VideoRaster::EnumType e)
{
    std::string name;
    switch (e)
    {
    case PAL_4x3:
        name = "PAL 4x3";
        break;
    case PAL_16x9:
        name = "PAL 16x9";
        break;
    case PAL_592_4x3:
        name = "PAL 592 4x3";
        break;
    case PAL_608_4x3:
        name = "PAL 608 4x3";
        break;
    case PAL_592_16x9:
        name = "PAL 592 16x9";
        break;
    case PAL_608_16x9:
        name = "PAL 608 16x9";
        break;

    case PAL_4x3_B:
        name = "PAL 4x3 (reversed field order)";
        break;
    case PAL_16x9_B:
        name = "PAL 16x9 (reversed field order)";
        break;
    case PAL_592_4x3_B:
        name = "PAL 592 4x3 (reversed field order)";
        break;
    case PAL_608_4x3_B:
        name = "PAL 608 4x3 (reversed field order)";
        break;
    case PAL_592_16x9_B:
        name = "PAL 592 16x9 (reversed field order)";
        break;
    case PAL_608_16x9_B:
        name = "PAL 608 16x9 (reversed field order)";
        break;

    case NTSC_4x3:
        name = "NTSC 4x3";
        break;
    case NTSC_16x9:
        name = "NTSC 16x9";
        break;
    case NTSC_502_4x3:
        name = "NTSC 502 4x3";
        break;
    case NTSC_502_16x9:
        name = "NTSC 502 16x9";
        break;
    case SMPTE274_25I:
        name = "1920x1080 25i";
        break;
    case SMPTE274_29I:
        name = "1920x1080 29.97i";
        break;
    case SMPTE274_25PSF:
        name = "1920x1080 25p (segmented frame)";
        break;
    case SMPTE274_29PSF:
        name = "1920x1080 29.97p (segmented frame)";
        break;
    case SMPTE274_25P:
        name = "1920x1080 25p";
        break;
    case SMPTE274_29P:
        name = "1920x1080 29.97p";
        break;
    case SMPTE296_50P:
        name = "1280x720 50p";
        break;
    case SMPTE296_59P:
        name = "1280x720 59.94p";
        break;
    case NONE:
        name = "None";
        break;
    }
    return name;
}

void Ingex::VideoRaster::GetInfo(Ingex::VideoRaster::EnumType raster,
            int & width, int & height, int & fps_num, int & fps_den, Ingex::Interlace::EnumType & interlace)
{
    switch (raster)
    {
    case PAL_4x3:
    case PAL_16x9:
        width = 720;
        height = 576;
        fps_num = 25;
        fps_den = 1;
        interlace = Ingex::Interlace::TOP_FIELD_FIRST;
        break;
    case PAL_592_4x3:
    case PAL_592_16x9:
        width = 720;
        height = 592;
        fps_num = 25;
        fps_den = 1;
        interlace = Ingex::Interlace::TOP_FIELD_FIRST;
        break;
    case PAL_608_4x3:
    case PAL_608_16x9:
        width = 720;
        height = 608;
        fps_num = 25;
        fps_den = 1;
        interlace = Ingex::Interlace::TOP_FIELD_FIRST;
        break;
    case PAL_4x3_B:
    case PAL_16x9_B:
        width = 720;
        height = 576;
        fps_num = 25;
        fps_den = 1;
        interlace = Ingex::Interlace::BOTTOM_FIELD_FIRST;
        break;
    case PAL_592_4x3_B:
    case PAL_592_16x9_B:
        width = 720;
        height = 592;
        fps_num = 25;
        fps_den = 1;
        interlace = Ingex::Interlace::BOTTOM_FIELD_FIRST;
        break;
    case PAL_608_4x3_B:
    case PAL_608_16x9_B:
        width = 720;
        height = 608;
        fps_num = 25;
        fps_den = 1;
        interlace = Ingex::Interlace::BOTTOM_FIELD_FIRST;
        break;
    case NTSC_4x3:
    case NTSC_16x9:
        width = 720;
        height = 486;
        fps_num = 30000;
        fps_den = 1001;
        interlace = Ingex::Interlace::BOTTOM_FIELD_FIRST;
        break;
    case NTSC_502_4x3:
    case NTSC_502_16x9:
        width = 720;
        height = 502;
        fps_num = 30000;
        fps_den = 1001;
        interlace = Ingex::Interlace::BOTTOM_FIELD_FIRST;
        break;
    case SMPTE274_25I:
        width = 1920;
        height = 1080;
        fps_num = 25;
        fps_den = 1;
        interlace = Ingex::Interlace::TOP_FIELD_FIRST;
        break;
    case SMPTE274_29I:
        width = 1920;
        height = 1080;
        fps_num = 30000;
        fps_den = 1001;
        interlace = Ingex::Interlace::TOP_FIELD_FIRST;
        break;
    case SMPTE274_25PSF:
    case SMPTE274_25P:
        width = 1920;
        height = 1080;
        fps_num = 25;
        fps_den = 1;
        interlace = Ingex::Interlace::NONE;
        break;
    case SMPTE274_29PSF:
    case SMPTE274_29P:
        width = 1920;
        height = 1080;
        fps_num = 30000;
        fps_den = 1001;
        interlace = Ingex::Interlace::NONE;
        break;
    case SMPTE296_50P:
        width = 1280;
        height = 720;
        fps_num = 50;
        fps_den = 1;
        interlace = Ingex::Interlace::NONE;
        break;
    case SMPTE296_59P:
        width = 1280;
        height = 720;
        fps_num = 60000;
        fps_den = 1001;
        interlace = Ingex::Interlace::NONE;
        break;
    case NONE:
        width = 0;
        height = 0;
        fps_num = 0;
        fps_den = 0;
        interlace = Ingex::Interlace::NONE;
        break;
    }
}

bool Ingex::VideoRaster::IsRec601(VideoRaster::EnumType raster)
{
    bool is_rec_601;

    switch (raster)
    {
    case PAL_4x3:
    case PAL_4x3_B:
    case PAL_16x9:
    case PAL_16x9_B:
    case PAL_592_4x3:
    case PAL_592_4x3_B:
    case PAL_592_16x9:
    case PAL_592_16x9_B:
    case PAL_608_4x3:
    case PAL_608_4x3_B:
    case PAL_608_16x9:
    case PAL_608_16x9_B:
    case NTSC_4x3:
    case NTSC_16x9:
    case NTSC_502_4x3:
    case NTSC_502_16x9:
        is_rec_601 = true;
        break;
    default:
        is_rec_601 = false;
        break;
    }

    return is_rec_601;
}

bool Ingex::VideoRaster::Is4x3(VideoRaster::EnumType raster)
{
    bool is_4x3;

    switch (raster)
    {
    case PAL_4x3:
    case PAL_4x3_B:
    case PAL_592_4x3:
    case PAL_592_4x3_B:
    case PAL_608_4x3:
    case PAL_608_4x3_B:
    case NTSC_4x3:
    case NTSC_502_4x3:
        is_4x3 = true;
        break;
    default:
        is_4x3 = false;
        break;
    }

    return is_4x3;
}

void Ingex::VideoRaster::ModifyLineShift(VideoRaster::EnumType & raster, bool shifted)
{
    switch (raster)
    {
    case PAL_4x3:
    case PAL_4x3_B:
        raster = shifted ? PAL_4x3_B : PAL_4x3;
        break;
    case PAL_16x9:
    case PAL_16x9_B:
        raster = shifted ? PAL_16x9_B : PAL_16x9;
        break;
    case PAL_592_4x3:
    case PAL_592_4x3_B:
        raster = shifted ? PAL_592_4x3_B : PAL_592_4x3;
        break;
    case PAL_592_16x9:
    case PAL_592_16x9_B:
        raster = shifted ? PAL_592_16x9_B : PAL_592_16x9;
        break;
    case PAL_608_4x3:
    case PAL_608_4x3_B:
        raster = shifted ? PAL_608_4x3_B : PAL_608_4x3;
        break;
    case PAL_608_16x9:
    case PAL_608_16x9_B:
        raster = shifted ? PAL_608_16x9_B : PAL_608_16x9;
        break;
    default:
        break;
    }
}

int Ingex::VideoRaster::LineShift(VideoRaster::EnumType raster)
{
    int shift;
    switch (raster)
    {
    case PAL_4x3_B:
    case PAL_16x9_B:
    case PAL_592_4x3_B:
    case PAL_592_16x9_B:
    case PAL_608_4x3_B:
    case PAL_608_16x9_B:
        shift = 1;
        break;
    default:
        shift = 0;
        break;
    }

    return shift;
}

Ingex::Rational Ingex::VideoRaster::SampleAspectRatio(VideoRaster::EnumType raster)
{
    Ingex::Rational sar;

    switch (raster)
    {
    case PAL_4x3:
    case PAL_592_4x3:
    case PAL_608_4x3:
    case PAL_4x3_B:
    case PAL_592_4x3_B:
    case PAL_608_4x3_B:
        sar.numerator = 59;
        sar.denominator = 54;
        break;
    case PAL_16x9:
    case PAL_592_16x9:
    case PAL_608_16x9:
    case PAL_16x9_B:
    case PAL_592_16x9_B:
    case PAL_608_16x9_B:
        sar.numerator = 118;
        sar.denominator = 81;
        break;
    case NTSC_4x3:
    case NTSC_502_4x3:
        sar.numerator = 10;
        sar.denominator = 11;
        break;
    case NTSC_16x9:
    case NTSC_502_16x9:
        sar.numerator = 40;
        sar.denominator = 33;
        break;
    default:
        sar.numerator = 1;
        sar.denominator = 1;
        break;
    }

    return sar;
}

