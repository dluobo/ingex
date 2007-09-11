/*
 * $Id: RecordOptions.h,v 1.1 2007/09/11 14:08:31 stuart_hc Exp $
 *
 * Class for card-specific (i.e. thread-specific) recording data.
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
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

#ifndef RecordOptions_h
#define RecordOptions_h

#include <string>

#include <ace/Thread_Mutex.h>
#include <ace/Guard_T.h>

#include "integer_types.h"
#include "recorder_types.h" // for framecount_t

namespace Wrapping
{
    enum EnumType { NONE, MXF };
}

namespace Coding
{
    enum EnumType { UNCOMPRESSED, DV25, DV50, MJPEG21, MJPEG31, MJPEG101, MJPEG101M, MJPEG151S, MJPEG201, MPEG2 };
}

// Description of a particular encoding, to be performed in a thread.
class RecordOptions
{
public:
    RecordOptions();

    int card_num;
    int index; ///< To distinguish multiple encodings on the same input/card.
    bool quad; ///< True for encoding from multiple inputs/cards.

    Wrapping::EnumType wrapping;
    Coding::EnumType coding;
    int resolution; // using this rather than coding
    bool bitc; ///< True for burnt-in timecode

    std::string file_ident;
    std::string description;
    std::string project; ///< Avid project name.

    framecount_t FramesWritten()
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mFramesWrittenMutex);
        return mFramesWritten;
    }
    void FramesWritten(framecount_t f)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mFramesWrittenMutex);
        mFramesWritten = f;
    }
    void IncFramesWritten(framecount_t n = 1)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mFramesWrittenMutex);
        mFramesWritten += n;
    }

    framecount_t FramesDropped()
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mFramesDroppedMutex);
        return mFramesDropped;
    }
    void FramesDropped(framecount_t f)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mFramesDroppedMutex);
        mFramesDropped = f;
    }
    void IncFramesDropped(framecount_t n = 1)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mFramesDroppedMutex);
        mFramesDropped += n;
    }

private:
    framecount_t mFramesWritten;        ///< Frames written during capture
    ACE_Thread_Mutex mFramesWrittenMutex;
    framecount_t mFramesDropped;        ///< Frames dropped during capture
    ACE_Thread_Mutex mFramesDroppedMutex;
};


#endif // ifndef RecordOptions_h

