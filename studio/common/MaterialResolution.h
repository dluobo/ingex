/*
 * $Id: MaterialResolution.h,v 1.4 2010/07/21 16:29:34 john_f Exp $
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
 
#ifndef Database_MaterialResolution_h
#define Database_MaterialResolution_h

#include "VideoRaster.h"

#include <string>

class FileFormat
{
public:
    enum EnumType
    {
        NONE = 0,

        RAW,
        MXF,
        MOV,
        MPG
    };

    static std::string Name(FileFormat::EnumType format);
};

class OperationalPattern
{
public:
    enum EnumType
    {
        NONE = 0,

        OP_ATOM,
        OP_1A
    };

    static std::string Name(OperationalPattern::EnumType pattern);
};

class MaterialResolution
{
public:
    enum EnumType
    {
        NONE = 0,

        UNC_RAW = 10,
        UNC_MXF_ATOM = 12,

        DV25_RAW = 20,
        DV25_MXF_ATOM = 22,
        DV25_MOV = 24,
        DV50_RAW = 30,
        DV50_MXF_ATOM = 32,
        DV50_MOV = 34,
        DV100_RAW = 40,
        DV100_MXF_ATOM = 42,
        DV100_MOV = 44,

        MJPEG21_MXF_ATOM = 50,
        MJPEG31_MXF_ATOM = 52,
        MJPEG101_MXF_ATOM = 54,
        MJPEG101M_MXF_ATOM = 56,
        MJPEG151S_MXF_ATOM = 58,
        MJPEG201_MXF_ATOM = 60,

        IMX30_MXF_ATOM = 70,
        IMX40_MXF_ATOM = 72,
        IMX50_MXF_ATOM = 74,
        IMX30_MXF_1A = 80,
        IMX40_MXF_1A = 82,
        IMX50_MXF_1A = 84,

        DNX36P_MXF_ATOM = 100,
        DNX120I_MXF_ATOM = 102,
        DNX185I_MXF_ATOM = 104,
        DNX120P_MXF_ATOM = 106,
        DNX185P_MXF_ATOM = 108,

        XDCAMHD422_RAW = 120,
        //XDCAMHD422_MXF = 122,
        XDCAMHD422_MOV = 124,

        DVD = 200,
        MPEG4_MOV = 210,

        MP3 = 300,

        CUTS = 400
    };

    static std::string Name(MaterialResolution::EnumType res);

    static void GetInfo(MaterialResolution::EnumType res,
            FileFormat::EnumType & format, OperationalPattern::EnumType & op);

    static bool CheckVideoFormat(MaterialResolution::EnumType res,
            Ingex::VideoRaster::EnumType raster, Ingex::PixelFormat::EnumType format);
};

#endif //ifndef Database_MaterialResolution_h

