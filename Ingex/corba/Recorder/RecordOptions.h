/*
 * $Id: RecordOptions.h,v 1.2 2007/01/30 12:30:50 john_f Exp $
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
typedef int32_t framecount_t;

// Store card-specific (i.e. thread-specific) recording data.
// Things that change from recording to recording, not setup
// parameters like MPEG2 bitrate.
class RecordOptions
{
public:
    RecordOptions();
    bool        enabled;                // recording on this card?
    bool        track_enable[5];
    framecount_t        start_tc;
    int         start_frame;

    int         quad1_frame_offset; // diff between source 0 and source 1
    int         quad2_frame_offset; // diff between source 0 and source 2
    int         quad3_frame_offset; // diff between source 0 and source 3

    ACE_thread_t    thread_id;      // running thread id

    std::string file_ident;
    std::string description;
    std::string project; ///< Avid project name.
    std::string file_name[5]; // one per track

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

    framecount_t TargetDuration()
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mDurationMutex);
        return mTargetDuration;
    }
    void TargetDuration(framecount_t d)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mDurationMutex);
        mTargetDuration = d;
    }
private:
    framecount_t mFramesWritten;        ///< Frames written during capture
    ACE_Thread_Mutex mFramesWrittenMutex;
    framecount_t mFramesDropped;        ///< Frames dropped during capture
    ACE_Thread_Mutex mFramesDroppedMutex;
    framecount_t mTargetDuration;           ///< To signal when capture should stop
    ACE_Thread_Mutex mDurationMutex;
};


#endif // ifndef RecordOptions_h

