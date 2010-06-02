/*
 * $Id: VideoRaster.cpp,v 1.1 2010/06/02 10:52:38 philipn Exp $
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
    case PAL:
        name = "PAL";
        break;
    case PAL_592:
        name = "PAL 592";
        break;
    case PAL_608:
        name = "PAL 608";
        break;
    case PAL_B:
        name = "PAL (reversed field order)";
        break;
    case PAL_592_B:
        name = "PAL 592 (reversed field order)";
        break;
    case PAL_608_B:
        name = "PAL 608 (reversed field order)";
        break;
    case NTSC:
        name = "NTSC";
        break;
    case SMPTE274_25I:
        name = "1920x1080 25i";
        break;
    case SMPTE274_29I:
        name = "1920x1080 29.97i";
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
    case PAL:
        width = 720;
        height = 576;
        fps_num = 25;
        fps_den = 1;
        interlace = Ingex::Interlace::TOP_FIELD_FIRST;
        break;
    case PAL_592:
        width = 720;
        height = 592;
        fps_num = 25;
        fps_den = 1;
        interlace = Ingex::Interlace::TOP_FIELD_FIRST;
        break;
    case PAL_608:
        width = 720;
        height = 608;
        fps_num = 25;
        fps_den = 1;
        interlace = Ingex::Interlace::TOP_FIELD_FIRST;
        break;
    case PAL_B:
        width = 720;
        height = 576;
        fps_num = 25;
        fps_den = 1;
        interlace = Ingex::Interlace::BOTTOM_FIELD_FIRST;
        break;
    case PAL_592_B:
        width = 720;
        height = 592;
        fps_num = 25;
        fps_den = 1;
        interlace = Ingex::Interlace::BOTTOM_FIELD_FIRST;
        break;
    case PAL_608_B:
        width = 720;
        height = 608;
        fps_num = 25;
        fps_den = 1;
        interlace = Ingex::Interlace::BOTTOM_FIELD_FIRST;
        break;
    case NTSC:
        width = 720;
        height = 480;
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
        interlace = Ingex::Interlace::BOTTOM_FIELD_FIRST;
        break;
    case SMPTE274_25P:
        width = 1920;
        height = 1080;
        fps_num = 50;
        fps_den = 1;
        interlace = Ingex::Interlace::NONE;
        break;
    case SMPTE274_29P:
        width = 1920;
        height = 1080;
        fps_num = 60000;
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


