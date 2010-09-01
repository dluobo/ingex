/*
 * $Id: LTCDecoder.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#ifndef __RECORDER_LTC_DECODER_H__
#define __RECORDER_LTC_DECODER_H__


extern "C"
{
#include <ltcsmpte.h>
}

#include "Types.h"


class LTCDecoder
{
public:
    LTCDecoder();
    ~LTCDecoder();
    
    void processFrame(unsigned char *data, unsigned int dataSize);
    
    void reset();
    
    bool havePrevFrameTimecode() const { return _havePrevFrameTimecode; }
    bool haveThisFrameTimecode() const { return _haveThisFrameTimecode; }
    rec::Timecode getTimecode() const { return _timecode; }

private:
    SMPTEDecoder *_smpteDecoder;

    bool _havePrevFrameTimecode;
    bool _haveThisFrameTimecode;
    rec::Timecode _timecode;
};



#endif

