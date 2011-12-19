/*
 * $Id: MaterialResolution.cpp,v 1.14 2011/12/19 16:20:54 john_f Exp $
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
std::string FileFormat::Name(FileFormat::EnumType format)
{
    std::string name;
    switch (format)
    {
    case NONE:
        name = "None";
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
    case MP4:
	name = "MPEG-4 Part 14";
	break;
    }
    return name;
}

// Get OperationalPattern name
std::string OperationalPattern::Name(OperationalPattern::EnumType pattern)
{
    std::string name;
    switch (pattern)
    {
    case NONE:
        name = "None";
    case OP_ATOM:
        name = "OP-Atom";
        break;
    case OP_1A:
        name = "OP-1A";
        break;
    }
    return name;
}

/// Get MaterialResolution name
std::string MaterialResolution::Name(MaterialResolution::EnumType res)
{
    std::string name;

    switch (res)
    {
    case NONE:
        name = "None";
        break;

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
        name = "VC3-36/45p MXF OP-ATOM";
        break;

    case DNX120I_MXF_ATOM:
        name = "VC3-120/145i MXF OP-ATOM";
        break;

    case DNX185I_MXF_ATOM:
        name = "VC3-185/220i MXF OP-ATOM";
        break;

    case DNX120P_MXF_ATOM:
        name = "VC3-120/145p MXF OP-ATOM";
        break;

    case DNX185P_MXF_ATOM:
        name = "VC3-185/220p MXF OP-ATOM";
        break;

    case DVD:
        name = "MPEG2 for DVD";
        break;

    case MPEG4_MP3_MOV:
        name = "MPEG4/MP3 Quicktime";
        break;

    case MPEG4_PCM_MOV:
        name = "MPEG4/PCM Quicktime";
        break;
    
    case MPEG4BP_AAC_MP4:
        name = "H264 Baseline Profile 512 kbit/s and AAC";
        break;

    case MPEG4MP_AAC_MP4:
        name = "H264 Main Profile 1024 kbit/s and AAC";
        break;

    case MP3:
        name = "MP3 Audio only";
        break;

    case CUTS:
        name = "Vision Cuts";
        break;

    case XDCAMHD422_RAW:
        name = "MPEG2 422 Long GOP 50 Mbit/s Raw";
        break;
    
    case XDCAMHD422_MOV:
        name = "MPEG2 422 Long GOP 50 Mbit/s Quicktime";
        break;

    case XDCAMHD422_MXF_1A:
        name = "MPEG2 422 Long GOP 50 Mbit/s MXF OP-1A";
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
        format = FileFormat::MXF;
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

    case MPEG4_MP3_MOV:
    case MPEG4_PCM_MOV:
        format = FileFormat::MOV;
        op = OperationalPattern::OP_1A;
        break;

    case MPEG4BP_AAC_MP4:
    case MPEG4MP_AAC_MP4:
        format = FileFormat::MP4;
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
    
    case XDCAMHD422_RAW:
        format = FileFormat::RAW;
        op = OperationalPattern::OP_1A;
        break;

    case XDCAMHD422_MOV:
        format = FileFormat::MOV;
        op = OperationalPattern::OP_1A;
        break;

    case XDCAMHD422_MXF_1A:
        format = FileFormat::MXF;
        op = OperationalPattern::OP_1A;
        break;

    case NONE:
        format = FileFormat::NONE;
        op = OperationalPattern::NONE;
        break;
    }
}

/**
Check whether raster and pixel format are suitable for encoding to specified resolution
and also return approximate bytes per minute.
*/
bool MaterialResolution::CheckVideoFormat(MaterialResolution::EnumType res,
            Ingex::VideoRaster::EnumType raster, Ingex::PixelFormat::EnumType format,
            uint32_t & kbyte_per_minute)
{
    bool result = false;
    kbyte_per_minute = 0;
    switch (res)
    {
    case NONE:
        result = true;
        kbyte_per_minute = 0;
        break;
    case DV25_RAW:
    case DV25_MXF_ATOM:
    case DV25_MOV:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3_B:
        case Ingex::VideoRaster::PAL_16x9_B:
            if (Ingex::PixelFormat::YUV_PLANAR_420_DV == format)
            {
                result = true;
                kbyte_per_minute = 187500;
            }
            break;
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            if (Ingex::PixelFormat::YUV_PLANAR_411 == format)
            {
                result = true;
                kbyte_per_minute = 187500;
            }
            break;
        default:
            break;
        }
        break;
    case DV50_RAW:
    case DV50_MXF_ATOM:
    case DV50_MOV:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3_B:
        case Ingex::VideoRaster::PAL_16x9_B:
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            if (Ingex::PixelFormat::YUV_PLANAR_422 == format)
            {
                result = true;
                kbyte_per_minute = 375000;
            }
            break;
        default:
            break;
        }
        break;
    case DV100_RAW:
    case DV100_MXF_ATOM:
    case DV100_MOV:
        switch (raster)
        {
        case Ingex::VideoRaster::SMPTE274_25I:
        case Ingex::VideoRaster::SMPTE274_25PSF:
        case Ingex::VideoRaster::SMPTE274_25P:
        case Ingex::VideoRaster::SMPTE274_29I:
        case Ingex::VideoRaster::SMPTE274_29PSF:
        case Ingex::VideoRaster::SMPTE274_29P:
        //case Ingex::VideoRaster::SMPTE296_50P:
        //case Ingex::VideoRaster::SMPTE296_59P:
            if (Ingex::PixelFormat::YUV_PLANAR_422 == format)
            {
                result = true;
                kbyte_per_minute = 750000;
            }
            break;
        default:
            break;
        }
        break;
    case IMX30_MXF_ATOM:
    case IMX40_MXF_ATOM:
    case IMX50_MXF_ATOM:
    case IMX30_MXF_1A:
    case IMX40_MXF_1A:
    case IMX50_MXF_1A:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3:
        case Ingex::VideoRaster::PAL_16x9:
        case Ingex::VideoRaster::PAL_592_4x3:
        case Ingex::VideoRaster::PAL_592_16x9:
        case Ingex::VideoRaster::PAL_608_4x3:
        case Ingex::VideoRaster::PAL_608_16x9:
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
        case Ingex::VideoRaster::NTSC_502_4x3:
        case Ingex::VideoRaster::NTSC_502_16x9:
            if (Ingex::PixelFormat::YUV_PLANAR_422 == format)
            {
                result = true;
                kbyte_per_minute = 375000; // 50 Mbit/s
            }
            break;
        default:
            break;
        }
        break;
    case MJPEG21_MXF_ATOM:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3:
        case Ingex::VideoRaster::PAL_16x9:
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            if (Ingex::PixelFormat::YUV_PLANAR_422 == format)
            {
                result = true;
                kbyte_per_minute = 500000; // approx for 2:1
            }
            break;
        default:
            break;
        }
        break;
    case MJPEG31_MXF_ATOM:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3:
        case Ingex::VideoRaster::PAL_16x9:
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            if (Ingex::PixelFormat::YUV_PLANAR_422 == format)
            {
                result = true;
                kbyte_per_minute = 340000; // approx for 3:1
            }
            break;
        default:
            break;
        }
        break;
    case MJPEG101_MXF_ATOM:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3:
        case Ingex::VideoRaster::PAL_16x9:
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            if (Ingex::PixelFormat::YUV_PLANAR_422 == format)
            {
                result = true;
                kbyte_per_minute = 140000; // approx for 10:1
            }
            break;
        default:
            break;
        }
        break;
    case MJPEG101M_MXF_ATOM:
    case MJPEG151S_MXF_ATOM:
    case MJPEG201_MXF_ATOM:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3:
        case Ingex::VideoRaster::PAL_16x9:
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            if (Ingex::PixelFormat::YUV_PLANAR_422 == format)
            {
                result = true;
                kbyte_per_minute = 90000; // approx for 20:1
            }
            break;
        default:
            break;
        }
        break;
    case DNX120I_MXF_ATOM:
    case DNX185I_MXF_ATOM:
        switch (raster)
        {
        case Ingex::VideoRaster::SMPTE274_25I:
        case Ingex::VideoRaster::SMPTE274_29I:
            if (Ingex::PixelFormat::YUV_PLANAR_422 == format)
            {
                result = true;
                kbyte_per_minute = 1387500; // 185
            }
            break;
        default:
            break;
        }
        break;
    case DNX36P_MXF_ATOM:
        switch (raster)
        {
        case Ingex::VideoRaster::SMPTE274_25PSF:
        case Ingex::VideoRaster::SMPTE274_25P:
        case Ingex::VideoRaster::SMPTE274_29PSF:
        case Ingex::VideoRaster::SMPTE274_29P:
            if (Ingex::PixelFormat::YUV_PLANAR_422 == format)
            {
                result = true;
                kbyte_per_minute = 270000;
            }
            break;
        default:
            break;
        }
        break;
    case DNX120P_MXF_ATOM:
    case DNX185P_MXF_ATOM:
        switch (raster)
        {
        case Ingex::VideoRaster::SMPTE274_25PSF:
        case Ingex::VideoRaster::SMPTE274_25P:
        case Ingex::VideoRaster::SMPTE274_29PSF:
        case Ingex::VideoRaster::SMPTE274_29P:
            if (Ingex::PixelFormat::YUV_PLANAR_422 == format)
            {
                result = true;
                kbyte_per_minute = 1387500; // 185
            }
            break;
        default:
            break;
        }
        break;
    case XDCAMHD422_RAW:
    case XDCAMHD422_MOV:
    case XDCAMHD422_MXF_1A:
        switch (raster)
        {
        case Ingex::VideoRaster::SMPTE274_25I:
        case Ingex::VideoRaster::SMPTE274_25PSF:
        case Ingex::VideoRaster::SMPTE274_25P:
        case Ingex::VideoRaster::SMPTE274_29I:
        case Ingex::VideoRaster::SMPTE274_29PSF:
        case Ingex::VideoRaster::SMPTE274_29P:
            if (Ingex::PixelFormat::YUV_PLANAR_422 == format)
            {
                result = true;
                kbyte_per_minute = 375000; // 50 Mbit/s
            }
            break;
        default:
            break;
        }
        break;
    case DVD:
    case MPEG4_MP3_MOV:
    case MPEG4_PCM_MOV:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3:
        case Ingex::VideoRaster::PAL_16x9:
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            if (Ingex::PixelFormat::YUV_PLANAR_420_MPEG == format)
            {
                result = true;
                kbyte_per_minute = 37500; // 5 Mbit/s
            }
            break;
        default:
            break;
        }
        break;
    case MPEG4BP_AAC_MP4:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3:
        case Ingex::VideoRaster::PAL_16x9:
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            if (Ingex::PixelFormat::YUV_PLANAR_420_MPEG == format)
            {
                result = true;
                kbyte_per_minute = 3750; // 512 kbit/s
            }
            break;
        default:
            break;
        }
        break;

    case MPEG4MP_AAC_MP4:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3:
        case Ingex::VideoRaster::PAL_16x9:
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            if (Ingex::PixelFormat::YUV_PLANAR_420_MPEG == format)
            {
                result = true;
                kbyte_per_minute = 7500; // 1 Mbit/s
            }
            break;
        default:
            break;
        }
        break;

    case UNC_RAW:
    case UNC_MXF_ATOM:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3:
        case Ingex::VideoRaster::PAL_16x9:
        case Ingex::VideoRaster::PAL_592_4x3:
        case Ingex::VideoRaster::PAL_592_16x9:
        case Ingex::VideoRaster::PAL_608_4x3:
        case Ingex::VideoRaster::PAL_608_16x9:
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
        case Ingex::VideoRaster::NTSC_502_4x3:
        case Ingex::VideoRaster::NTSC_502_16x9:
            if (Ingex::PixelFormat::UYVY_422 == format)
            {
                result = true;
                kbyte_per_minute = 1244160;
            }
            break;
        case Ingex::VideoRaster::SMPTE274_25I:
        case Ingex::VideoRaster::SMPTE274_29I:
        case Ingex::VideoRaster::SMPTE296_50P:
        case Ingex::VideoRaster::SMPTE296_59P:
            if (Ingex::PixelFormat::UYVY_422 == format)
            {
                result = true;
                kbyte_per_minute = 6220800; // 25I
            }
            break;
        default:
            break;
        }
        break;
    case MP3:
        result = true;
        kbyte_per_minute = 960; // 128 kbit/s
        break;
    case CUTS:
        result = true;
        kbyte_per_minute = 1;
        break;
    }
    return result;
}

