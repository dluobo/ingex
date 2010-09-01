/*
 * $Id: ArchiveMXFFrameWriter.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * Archive MXF frame writer
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

#include "ArchiveMXFFrameWriter.h"
#include "ArchiveMXFContentPackage.h"

using namespace rec;



ArchiveMXFFrameWriter::ArchiveMXFFrameWriter(int element_size, bool palff_mode, ArchiveMXFWriter *writer)
: FrameWriter(element_size, palff_mode)
{
    _writer = writer;
}

ArchiveMXFFrameWriter::~ArchiveMXFFrameWriter()
{
    // _writer is not owned
}

void ArchiveMXFFrameWriter::writeFrame()
{
    ArchiveMXFContentPackage content_package;
    
    content_package.setNumAudioTracks(_writer->getNumAudioTracks());
    if (_writer->includeCRC32())
        content_package.setNumCRC32(1 + content_package.getNumAudioTracks());
    
    content_package.setPosition(0); // not set
    if (haveVITC())
        content_package.setVITC(getVITC());
    else
        content_package.setMissingVITC();
    if (haveLTC())
        content_package.setLTC(getLTC());
    else
        content_package.setMissingLTC();
    
    if (_writer->getComponentDepth() == 10) {
        content_package.setVideoIs8Bit(false);
        content_package.setVideoSize((720 + 5) / 6 * 16 * 576);
        content_package.setVideo(getVideoData(), content_package.getVideoSize(), false);
        content_package.setVideo8BitSize(720 * 576 * 2);
        content_package.setVideo8Bit(get8BitVideoData(), content_package.getVideo8BitSize(), false);
    } else {
        content_package.setVideoIs8Bit(true);
        content_package.setVideoSize(720 * 576 * 2);
        content_package.setVideo(get8BitVideoData(), content_package.getVideoSize(), false);
    }
    if (_writer->includeCRC32())
        content_package.setCRC32(0, MXFWriter::calcCRC32(content_package.getVideo(),
                                                         content_package.getVideoSize()));
    
    content_package.setAudioSize(1920 * 3);
    int i;
    for (i = 0; i < content_package.getNumAudioTracks(); i++) {
        content_package.setAudio(i, getAudioData(i), content_package.getAudioSize(), false);
        if (_writer->includeCRC32())
            content_package.setCRC32(i + 1, MXFWriter::calcCRC32(content_package.getAudio(i),
                                                                 content_package.getAudioSize()));
    }

    
    _writer->writeContentPackage(&content_package);
}

