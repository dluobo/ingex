/*
 * $Id: D10MXFFrameWriter.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * SMPTE D-10 (S386M) MXF frame writer
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
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

#include "D10MXFFrameWriter.h"
#include "D10MXFContentPackage.h"
#include "RecorderException.h"
#include "Logging.h"

using namespace rec;



D10MXFFrameWriter::D10MXFFrameWriter(int element_size, bool palff_mode, D10MXFWriter *writer)
: FrameWriter(element_size, palff_mode)
{
    _writer = writer;
    
    // TODO: support 30 and 40 Mbps
    // TODO: add support for palff_mode (PAL_592) to ffmpeg_encoder
    _encoder = new FFmpegEncoder(MaterialResolution::IMX50_MXF_1A, Ingex::VideoRaster::PAL, -1, true);
}

D10MXFFrameWriter::~D10MXFFrameWriter()
{
    delete _encoder;
    // _writer is not owned
}

void D10MXFFrameWriter::writeFrame()
{
    D10MXFContentPackage content_package;

    bool have_vitc = haveVITC();
    Timecode vitc = getVITC();
    bool have_ltc = haveLTC();
    Timecode ltc = getLTC();
    
    unsigned char *video_data = 0;
    unsigned int video_data_size;
    REC_CHECK(_encoder->Encode(get8BitVideoData(), 0,
                               have_vitc ? &vitc : 0,
                               &video_data, &video_data_size));


    content_package.setNumAudioTracks(_writer->getNumAudioTracks());

    if (have_vitc)
        content_package.setVITC(vitc);
    else
        content_package.setMissingVITC();
    if (have_ltc)
        content_package.setLTC(ltc);
    else
        content_package.setMissingLTC();

    content_package.setVideoSize(video_data_size);
    content_package.setVideo(video_data, video_data_size, false);
    
    content_package.setAudioSize(1920 * 3);
    int i;
    for (i = 0; i < content_package.getNumAudioTracks(); i++)
        content_package.setAudio(i, getAudioData(i), content_package.getAudioSize(), false);

    
    _writer->writeContentPackage(&content_package);
}

