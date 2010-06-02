/*
 * $Id: MaterialResolution.cpp,v 1.1 2010/06/02 13:10:46 john_f Exp $
 *
 * Material resolution codes and details
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
 
#include "MaterialResolution.h"

// Get FileFormat name
void FileFormat::GetInfo(FileFormat::EnumType format, std::string & name)
{
    switch (format)
    {
    case RAW:
        name = "Raw";
        break;
    case MXF:
        name = "MXF";
        break;
    case MOV:
        name = "Quicktime";
        break;
    case MPG:
        name = "MPEG programme stream";
        break;
    default:
        name = "Unknown";
        break;
    }
}

// Get OperationalPattern name
void OperationalPattern::GetInfo(OperationalPattern::EnumType pattern, std::string & name)
{
    switch (pattern)
    {
    case OP_ATOM:
        name = "OP-Atom";
        break;
    case OP_1A:
        name = "OP-1A";
        break;
    default:
        name = "Unknown";
        break;
    }
}

/// Get MaterialResolution name
std::string MaterialResolution::Name(MaterialResolution::EnumType res)
{
    std::string name;

    switch (res)
    {
    case UNC_RAW:
        name = "Uncompressed UYVY";
        break;

    case UNC_MXF_ATOM:
        name = "Uncompressed UYVY MXF OP_ATOM";
        break;

    case DV25_RAW:
        name = "DV25 Raw";
        break;

    case DV25_MXF_ATOM:
        name = "DV25 MXF OP-ATOM";
        break;

    case DV25_MOV:
        name = "DV25 Quicktime";
        break;

    case DV50_RAW:
        name = "DV50 Raw";
        break;

    case DV50_MXF_ATOM:
        name = "DV50 MXF OP-ATOM";
        break;

    case DV50_MOV:
        name = "DV50 Quicktime";
        break;

    case DV100_RAW:
        name = "DVCPRO-HD Raw";
        break;

    case DV100_MXF_ATOM:
        name = "DVCPRO-HD MXF OP-ATOM";
        break;

    case DV100_MOV:
        name = "DVCPRO-HD Quicktime";
        break;

    case MJPEG21_MXF_ATOM:
        name = "Avid MJPEG 2:1 MXF OP-ATOM";
        break;

    case MJPEG31_MXF_ATOM:
        name = "Avid MJPEG 3:1 MXF OP-ATOM";
        break;

    case MJPEG101_MXF_ATOM:
        name = "Avid MJPEG 10:1 MXF OP-ATOM";
        break;

    case MJPEG101M_MXF_ATOM:
        name = "Avid MJPEG 10:1m MXF OP-ATOM";
        break;

    case MJPEG151S_MXF_ATOM:
        name = "Avid MJPEG 15:1s MXF OP-ATOM";
        break;

    case MJPEG201_MXF_ATOM:
        name = "Avid MJPEG 20:1 MXF OP-ATOM";
        break;

    case IMX30_MXF_ATOM:
        name = "IMX30 MXF OP-ATOM";
        break;

    case IMX40_MXF_ATOM:
        name = "IMX40 MXF OP-ATOM";
        break;

    case IMX50_MXF_ATOM:
        name = "IMX50 MXF OP-ATOM";
        break;

    case IMX30_MXF_1A:
        name = "IMX30 MXF OP-1A";
        break;

    case IMX40_MXF_1A:
        name = "IMX40 MXF OP-1A";
        break;

    case IMX50_MXF_1A:
        name = "IMX50 MXF OP-1A";
        break;

    case DNX36P_MXF_ATOM:
        name = "VC3-36p MXF OP-ATOM";
        break;

    case DNX120I_MXF_ATOM:
        name = "VC3-120i MXF OP-ATOM";
        break;

    case DNX185I_MXF_ATOM:
        name = "VC3-185i MXF OP-ATOM";
        break;

    case DNX120P_MXF_ATOM:
        name = "VC3-120p MXF OP-ATOM";
        break;

    case DNX185P_MXF_ATOM:
        name = "VC3-185p MXF OP-ATOM";
        break;

    case DVD:
        name = "MPEG2 for DVD";
        break;

    case MPEG4_MOV:
        name = "MPEG4 Quicktime";
        break;

    case MP3:
        name = "MP3 Audio only";
        break;

    case CUTS:
        name = "Vision Cuts";
        break;

    default:
        name = "Unknown";
        break;
    }

    return name;
}

/// Get MaterialResolution parameters
void MaterialResolution::GetInfo(MaterialResolution::EnumType res, FileFormat::EnumType & format, OperationalPattern::EnumType & op)
{
    switch (res)
    {
    case UNC_RAW:
        format = FileFormat::RAW;
        op = OperationalPattern::OP_ATOM;
        break;

    case UNC_MXF_ATOM:
        format = FileFormat::RAW;
        op = OperationalPattern::OP_ATOM;
        break;

    case DV25_RAW:
    case DV50_RAW:
    case DV100_RAW:
        format = FileFormat::RAW;
        op = OperationalPattern::OP_ATOM;
        break;

    case DV25_MXF_ATOM:
    case DV50_MXF_ATOM:
    case DV100_MXF_ATOM:
        format = FileFormat::MXF;
        op = OperationalPattern::OP_ATOM;
        break;

    case DV25_MOV:
    case DV50_MOV:
    case DV100_MOV:
        format = FileFormat::MOV;
        op = OperationalPattern::OP_1A;
        break;

    case MJPEG21_MXF_ATOM:
    case MJPEG31_MXF_ATOM:
    case MJPEG101_MXF_ATOM:
    case MJPEG101M_MXF_ATOM:
    case MJPEG151S_MXF_ATOM:
    case MJPEG201_MXF_ATOM:
        format = FileFormat::MXF;
        op = OperationalPattern::OP_ATOM;
        break;


    case IMX30_MXF_ATOM:
    case IMX40_MXF_ATOM:
    case IMX50_MXF_ATOM:
        format = FileFormat::MXF;
        op = OperationalPattern::OP_ATOM;
        break;

    case IMX30_MXF_1A:
    case IMX40_MXF_1A:
    case IMX50_MXF_1A:
        format = FileFormat::MXF;
        op = OperationalPattern::OP_1A;
        break;

    case DNX36P_MXF_ATOM:
    case DNX120I_MXF_ATOM:
    case DNX185I_MXF_ATOM:
    case DNX120P_MXF_ATOM:
    case DNX185P_MXF_ATOM:
        format = FileFormat::MXF;
        op = OperationalPattern::OP_ATOM;
        break;

    case DVD:
        format = FileFormat::MPG;
        op = OperationalPattern::OP_1A;
        break;

    case MPEG4_MOV:
        format = FileFormat::MOV;
        op = OperationalPattern::OP_1A;
        break;

    case MP3:
        format = FileFormat::RAW;
        op = OperationalPattern::OP_1A;
        break;

    case CUTS:
        format = FileFormat::RAW;
        op = OperationalPattern::OP_ATOM;
        break;

    case NONE:
    case END:
        format = FileFormat::NONE;
        op = OperationalPattern::NONE;
        break;
    }
}

/**
Check whether raster and pixel format are suitable for encoding to specified resolution.
*/
bool MaterialResolution::CheckVideoFormat(MaterialResolution::EnumType res,
            Ingex::VideoRaster::EnumType raster, Ingex::PixelFormat::EnumType format)
{
    bool result = false;
    switch (res)
    {
    case MaterialResolution::DV25_RAW:
    case MaterialResolution::DV25_MXF_ATOM:
        if ((Ingex::VideoRaster::PAL_B == raster && Ingex::PixelFormat::YUV_PLANAR_420_DV == format)
            || (Ingex::VideoRaster::NTSC == raster && Ingex::PixelFormat::YUV_PLANAR_411 == format))
        {
            result = true;
        }
        break;
    case MaterialResolution::DV50_RAW:
    case MaterialResolution::DV50_MXF_ATOM:
        if ((Ingex::VideoRaster::PAL_B == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format)
            || (Ingex::VideoRaster::NTSC == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format))
        {
            result = true;
        }
        break;
    case MaterialResolution::DV100_RAW:
    case MaterialResolution::DV100_MXF_ATOM:
        if ((Ingex::VideoRaster::SMPTE274_25I == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format)
            || (Ingex::VideoRaster::SMPTE274_29I == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format))
        {
            result = true;
        }
        break;
    case MaterialResolution::IMX30_MXF_ATOM:
    case MaterialResolution::IMX40_MXF_ATOM:
    case MaterialResolution::IMX50_MXF_ATOM:
    case MaterialResolution::IMX30_MXF_1A:
    case MaterialResolution::IMX40_MXF_1A:
    case MaterialResolution::IMX50_MXF_1A:
        if ((Ingex::VideoRaster::PAL == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format)
            || (Ingex::VideoRaster::PAL_592 == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format)
            || (Ingex::VideoRaster::PAL_608 == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format)
            || (Ingex::VideoRaster::NTSC == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format))
        {
            result = true;
        }
        break;
    case MaterialResolution::MJPEG21_MXF_ATOM:
    case MaterialResolution::MJPEG31_MXF_ATOM:
    case MaterialResolution::MJPEG101_MXF_ATOM:
    case MaterialResolution::MJPEG101M_MXF_ATOM:
    case MaterialResolution::MJPEG151S_MXF_ATOM:
    case MaterialResolution::MJPEG201_MXF_ATOM:
        if ((Ingex::VideoRaster::PAL == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format)
            || (Ingex::VideoRaster::NTSC == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format))
        {
            result = true;
        }
        break;
    case MaterialResolution::DNX36P_MXF_ATOM:
        if ((Ingex::VideoRaster::SMPTE274_25I == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format)
            || (Ingex::VideoRaster::SMPTE274_29I == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format)
            || (Ingex::VideoRaster::SMPTE274_25P == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format)
            || (Ingex::VideoRaster::SMPTE274_29P == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format))
        {
            result = true;
        }
        break;
    case MaterialResolution::DNX120I_MXF_ATOM:
    case MaterialResolution::DNX185I_MXF_ATOM:
        if ((Ingex::VideoRaster::SMPTE274_25I == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format)
            || (Ingex::VideoRaster::SMPTE274_29I == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format))
        {
            result = true;
        }
        break;
    case MaterialResolution::DNX120P_MXF_ATOM:
    case MaterialResolution::DNX185P_MXF_ATOM:
        if ((Ingex::VideoRaster::SMPTE274_25P == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format)
            || (Ingex::VideoRaster::SMPTE274_29P == raster && Ingex::PixelFormat::YUV_PLANAR_422 == format))
        {
            result = true;
        }
        break;
    case MaterialResolution::DVD:
    case MaterialResolution::MPEG4_MOV:
        if ((Ingex::VideoRaster::PAL == raster && Ingex::PixelFormat::YUV_PLANAR_420_MPEG == format)
            || (Ingex::VideoRaster::NTSC == raster && Ingex::PixelFormat::YUV_PLANAR_420_MPEG == format))
        {
            result = true;
        }
        break;
    case MaterialResolution::UNC_RAW:
    case MaterialResolution::UNC_MXF_ATOM:
        if (Ingex::PixelFormat::UYVY_422 == format)
        {
            result = true;
        }
        break;
    case MaterialResolution::MP3:
        result = true;
        break;
    default:
        break;
    }
    return result;
}

