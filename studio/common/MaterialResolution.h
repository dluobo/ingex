/*
 * $Id: MaterialResolution.h,v 1.3 2010/06/25 14:26:00 philipn Exp $
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
        MPG,

        END
    };

    static void GetInfo(FileFormat::EnumType format, std::string & name);
};

class OperationalPattern
{
public:
    enum EnumType
    {
        NONE = 0,

        OP_ATOM,
        OP_1A,

        END
    };

    static void GetInfo(OperationalPattern::EnumType pattern, std::string & name);
};

class MaterialResolution
{
public:
    enum EnumType
    {
        NONE = 0,

        UNC_RAW,
        UNC_MXF_ATOM,

        DV25_RAW,
        DV25_MXF_ATOM,
        DV25_MOV,
        DV50_RAW,
        DV50_MXF_ATOM,
        DV50_MOV,
        DV100_RAW,
        DV100_MXF_ATOM,
        DV100_MOV,

        MJPEG21_MXF_ATOM,
        MJPEG31_MXF_ATOM,
        MJPEG101_MXF_ATOM,
        MJPEG101M_MXF_ATOM,
        MJPEG151S_MXF_ATOM,
        MJPEG201_MXF_ATOM,

        IMX30_MXF_ATOM,
        IMX40_MXF_ATOM,
        IMX50_MXF_ATOM,
        IMX30_MXF_1A,
        IMX40_MXF_1A,
        IMX50_MXF_1A,

        DNX36P_MXF_ATOM,
        DNX120I_MXF_ATOM,
        DNX185I_MXF_ATOM,
        DNX120P_MXF_ATOM,
        DNX185P_MXF_ATOM,

        XDCAMHD422_RAW,
        XDCAMHD422_MOV,

        DVD,
        MPEG4_MOV,
        MP3,

        CUTS,

        END
    };

    static std::string Name(MaterialResolution::EnumType res);

    static void GetInfo(MaterialResolution::EnumType res,
            FileFormat::EnumType & format, OperationalPattern::EnumType & op);

    static bool CheckVideoFormat(MaterialResolution::EnumType res,
            Ingex::VideoRaster::EnumType raster, Ingex::PixelFormat::EnumType format);
};

#endif //ifndef Database_MaterialResolution_h

