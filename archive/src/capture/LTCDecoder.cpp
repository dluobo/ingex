/*
 * $Id: LTCDecoder.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * LTC audio signal decoder
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
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

#include "LTCDecoder.h"

using namespace rec;



LTCDecoder::LTCDecoder()
{
    _haveThisFrameTimecode = false;
    _havePrevFrameTimecode = false;
    
    _smpteDecoder = 0;
    reset();
}

LTCDecoder::~LTCDecoder()
{
    if (_smpteDecoder)
    {
        SMPTEFreeDecoder(_smpteDecoder);
    }
}

void LTCDecoder::processFrame(unsigned char *data, unsigned int dataSize)
{
    SMPTEFrameExt frame;
    SMPTETime stime;
    int result;
    
    SMPTEDecoderWrite(_smpteDecoder, data, dataSize, 0);
    result = SMPTEDecoderReadLast(_smpteDecoder, &frame);
    if (result == 1)
    {
        // if the sync word appears over 3/4 (more likely the ltc is delayed) in the frame then we assume it
        // is associated with this frame, otherwise we assume it is associated with the previous frame
        if (frame.endpos > (long)(dataSize * 3 / 4))
        {
            _haveThisFrameTimecode = true;
            _havePrevFrameTimecode = false;
        }
        else
        {
            _haveThisFrameTimecode = false;
            _havePrevFrameTimecode = true;
        }
            
        SMPTEFrameToTime(&frame.base, &stime);
        _timecode.hour = stime.hours;
        _timecode.min = stime.mins;
        _timecode.sec = stime.secs;
        _timecode.frame = stime.frame;
    }
    else
    {
        _havePrevFrameTimecode = false;
        _haveThisFrameTimecode = false;
    }
}

void LTCDecoder::reset()
{
    if (_smpteDecoder)
    {
        SMPTEFreeDecoder(_smpteDecoder);
        _smpteDecoder = 0;
    }
    
    FrameRate *fps = FR_create(25, 1, FRF_NONE);
    _smpteDecoder = SMPTEDecoderCreate(48000, fps, 8, 0);
    FR_free(fps);
}
