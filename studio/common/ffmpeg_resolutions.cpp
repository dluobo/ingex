/*
 * $Id: ffmpeg_resolutions.cpp,v 1.8 2011/12/19 16:20:54 john_f Exp $
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


#include "ffmpeg_resolutions.h"

void get_ffmpeg_params(MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster, CodecID & codec_id, CodecType & codec_type, ::PixelFormat & pix_fmt)
{
    switch (res)
    {
    case MaterialResolution::DV25_RAW:
    case MaterialResolution::DV25_MXF_ATOM:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3_B:
        case Ingex::VideoRaster::PAL_16x9_B:
            codec_id = CODEC_ID_DVVIDEO;
            codec_type = CODEC_TYPE_VIDEO;
            pix_fmt = PIX_FMT_YUV420P;
            break;
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            codec_id = CODEC_ID_DVVIDEO;
            codec_type = CODEC_TYPE_VIDEO;
            pix_fmt = PIX_FMT_YUV411P;
            break;
        default:
            codec_id = CODEC_ID_NONE;
            codec_type = CODEC_TYPE_UNKNOWN;
            pix_fmt = PIX_FMT_NONE;
            break;
        }
        break;
    case MaterialResolution::DV50_RAW:
    case MaterialResolution::DV50_MXF_ATOM:
        switch (raster)
        {
        case Ingex::VideoRaster::PAL_4x3_B:
        case Ingex::VideoRaster::PAL_16x9_B:
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            codec_id = CODEC_ID_DVVIDEO;
            codec_type = CODEC_TYPE_VIDEO;
            pix_fmt = PIX_FMT_YUV422P;
            break;
        default:
            codec_id = CODEC_ID_NONE;
            codec_type = CODEC_TYPE_UNKNOWN;
            pix_fmt = PIX_FMT_NONE;
            break;
        }
        break;
    case MaterialResolution::DV100_RAW:
    case MaterialResolution::DV100_MXF_ATOM:
        codec_id = CODEC_ID_DVVIDEO;
        codec_type = CODEC_TYPE_VIDEO;
        pix_fmt = PIX_FMT_YUV422P;
        break;
    case MaterialResolution::IMX30_MXF_ATOM:
    case MaterialResolution::IMX40_MXF_ATOM:
    case MaterialResolution::IMX50_MXF_ATOM:
    case MaterialResolution::IMX30_MXF_1A:
    case MaterialResolution::IMX40_MXF_1A:
    case MaterialResolution::IMX50_MXF_1A:
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
            codec_id = CODEC_ID_MPEG2VIDEO;
            codec_type = CODEC_TYPE_VIDEO;
            pix_fmt = PIX_FMT_YUV422P;
            break;
        default:
            codec_id = CODEC_ID_NONE;
            codec_type = CODEC_TYPE_UNKNOWN;
            pix_fmt = PIX_FMT_NONE;
            break;
        }
        break;
    case MaterialResolution::DNX36P_MXF_ATOM:
    case MaterialResolution::DNX120I_MXF_ATOM:
    case MaterialResolution::DNX185I_MXF_ATOM:
    case MaterialResolution::DNX120P_MXF_ATOM:
    case MaterialResolution::DNX185P_MXF_ATOM:
        switch (raster)
        {
        case Ingex::VideoRaster::SMPTE274_25I:
        case Ingex::VideoRaster::SMPTE274_25PSF:
        case Ingex::VideoRaster::SMPTE274_25P:
        case Ingex::VideoRaster::SMPTE274_29I:
        case Ingex::VideoRaster::SMPTE274_29PSF:
        case Ingex::VideoRaster::SMPTE274_29P:
            codec_id = CODEC_ID_DNXHD;
            codec_type = CODEC_TYPE_VIDEO;
            pix_fmt = PIX_FMT_YUV422P;
            break;
        default:
            codec_id = CODEC_ID_NONE;
            codec_type = CODEC_TYPE_UNKNOWN;
            pix_fmt = PIX_FMT_NONE;
            break;
        }
        break;

    case MaterialResolution::XDCAMHD422_RAW:
        codec_id = CODEC_ID_MPEG2VIDEO;
        codec_type = CODEC_TYPE_VIDEO;
        pix_fmt = PIX_FMT_YUV422P;
        break;

    case MaterialResolution::XDCAMHD422_MOV:
        codec_id = CODEC_ID_MPEG2VIDEO;
        codec_type = CODEC_TYPE_VIDEO;
        pix_fmt = PIX_FMT_YUV422P;
        break;

    case MaterialResolution::DVD:
        codec_id = CODEC_ID_MPEG2VIDEO;
        codec_type = CODEC_TYPE_VIDEO;
        pix_fmt = PIX_FMT_YUV420P;
        break;
    case MaterialResolution::MPEG4_MP3_MOV:
    case MaterialResolution::MPEG4_PCM_MOV:
        codec_id = CODEC_ID_MPEG4;
        codec_type = CODEC_TYPE_VIDEO;
        pix_fmt = PIX_FMT_YUV420P;
        break;
        
    case MaterialResolution::MPEG4BP_AAC_MP4:
    case MaterialResolution::MPEG4MP_AAC_MP4:
        codec_id = CODEC_ID_H264;
        codec_type = CODEC_TYPE_VIDEO;
        pix_fmt = PIX_FMT_YUV420P;
        break;

    case MaterialResolution::MP3:
        codec_id = CODEC_ID_MP3;
        codec_type = CODEC_TYPE_AUDIO;
        pix_fmt = PIX_FMT_NONE;
        break;
    default:
        codec_id = CODEC_ID_NONE;
        codec_type = CODEC_TYPE_UNKNOWN;
        pix_fmt = PIX_FMT_NONE;
        break;
    }
}

