/*
 * $Id: FrameWriter.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#include <cstring>

#include "FrameWriter.h"
#include "RecorderException.h"
#include "Logging.h"

using namespace rec;



FrameWriter::FrameWriter(int element_size, bool palff_mode)
{
    _elementSize = element_size;
    _palffMode = palff_mode;
    memset(&_audioPairOffset, 0, sizeof(_audioPairOffset));
    _ringFrame = 0;
    _ringFrame8Bit = 0;
    
    int i;
    for (i = 0; i < 8; i++) {
        _audioData[i] = new unsigned char[1920 * 3];
        _audioDataPresent[i] = false;
    }
}

FrameWriter::~FrameWriter()
{
    int i;
    for (i = 0; i < 8; i++)
        delete [] _audioData[i];
}

void FrameWriter::setAudioPairOffset(int index, int offset)
{
    REC_ASSERT(index >= 0 && index < 4);

    _audioPairOffset[index] = offset;
}

void FrameWriter::setRingFrame(const unsigned char *ring_frame, const unsigned char *ring_frame_8bit)
{
    _ringFrame = ring_frame;
    _ringFrame8Bit = ring_frame_8bit;
    
    int i;
    for (i = 0; i < 8; i++)
        _audioDataPresent[i] = false;
}

const unsigned char* FrameWriter::getVideoData()
{
    if (_palffMode)
        // skip 16 lines of VBI
        return _ringFrame + (720 + 5) / 6 * 16 * 16;
    else
        return _ringFrame;
}

const unsigned char* FrameWriter::get8BitVideoData()
{
    if (_palffMode)
        // skip 16 lines of VBI
        return _ringFrame8Bit + 720 * 2 * 16;
    else
        return _ringFrame8Bit;
}

const unsigned char* FrameWriter::getAudioData(int index)
{
    REC_ASSERT(index >= 0 && index < 8);
    
    if (!_audioDataPresent[index]) {
        
        const unsigned char *audio_pair_data = _ringFrame + _audioPairOffset[index / 2];
        int channel_offset = 0;
        if (index % 2 == 1)
            channel_offset = 4;

        int i, j;
        for (i = 0, j = channel_offset; i < 1920 * 3 && j < 1920 * 4 * 2; i += 3, j += 8) {
            _audioData[index][i] = audio_pair_data[j + 1];
            _audioData[index][i + 1] = audio_pair_data[j + 2];
            _audioData[index][i + 2] = audio_pair_data[j + 3];
        }
        
        _audioDataPresent[index] = true;
    }
    
    return _audioData[index];
}

bool FrameWriter::haveVITC()
{
    int vitc_as_int;
    memcpy(&vitc_as_int, _ringFrame + (_elementSize - sizeof(int) * 1), sizeof(vitc_as_int));

    return vitc_as_int != -1;
}

Timecode FrameWriter::getVITC()
{
    int vitc_as_int;
    memcpy(&vitc_as_int, _ringFrame + (_elementSize - sizeof(int) * 1), sizeof(vitc_as_int));
    
    return intToTimecode(vitc_as_int);
}

bool FrameWriter::haveLTC()
{
    int ltc_as_int;
    memcpy(&ltc_as_int, _ringFrame + (_elementSize - sizeof(int) * 2), sizeof(ltc_as_int));

    return ltc_as_int != -1;
}

Timecode FrameWriter::getLTC()
{
    int ltc_as_int;
    memcpy(&ltc_as_int, _ringFrame + (_elementSize - sizeof(int) * 2), sizeof(ltc_as_int));
    
    return intToTimecode(ltc_as_int);
}

Timecode FrameWriter::intToTimecode(int tc_as_int)
{
    Timecode tc;
    
    if (tc_as_int == -1) {
        tc.hour = 0;
        tc.min = 0;
        tc.sec = 0;
        tc.frame = 0;
    } else {
        tc.hour = tc_as_int / (60 * 60 * 25);
        tc.min = (tc_as_int % (60 * 60 * 25)) / (60 * 25);
        tc.sec = ((tc_as_int % (60 * 60 * 25)) % (60 * 25)) / 25;
        tc.frame = ((tc_as_int % (60 * 60 * 25)) % (60 * 25)) % 25;
    }

    return tc;
}

