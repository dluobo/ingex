/*
 * $Id: FrameWriter.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * Frame writer
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

#ifndef __RECORDER_FRAME_WRITER_H__
#define __RECORDER_FRAME_WRITER_H__

#include "Types.h"


namespace rec
{

class FrameWriter
{
public:
    FrameWriter(int element_size, bool palff_mode);
    virtual ~FrameWriter();
    
    void setAudioPairOffset(int index, int offset);

    void setRingFrame(const unsigned char *ring_frame, const unsigned char *ring_frame_8bit);

public:
    virtual void writeFrame() = 0;

protected:
    const unsigned char* getVideoData();
    const unsigned char* get8BitVideoData();
    
    const unsigned char* getAudioData(int index);
    
    bool haveVITC();
    Timecode getVITC();
    bool haveLTC();
    Timecode getLTC();

private:
    Timecode intToTimecode(int tc_as_int);

private:
    int _elementSize;
    bool _palffMode;
    
    int _audioPairOffset[4];
    unsigned char *_audioData[8];
    bool _audioDataPresent[8];
    
    const unsigned char *_ringFrame;
    const unsigned char *_ringFrame8Bit;
};



};


#endif
