/*
 * $Id: EncodeFrameBuffer.cpp,v 1.6 2010/12/03 14:31:13 john_f Exp $
 *
 * Buffer to handle video/audio data during encoding process.
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
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

#include <cstring>
#include <ace/Log_Msg.h>
#include <ace/Guard_T.h>

#include "EncodeFrameBuffer.h"

// EncodeFrameTrack class

EncodeFrameTrack::EncodeFrameTrack()
: mData(0), mSize(0), mSamples(0), mDel(false), mCoded(false), mFrameIndex(0), mpFrameIndex(0), mError(false)
{ }

/**
Add or replace a track.
Data at data, size size.
If copy true, take a copy of the data.
If del true, take ownership of data and free when done.
If coded true, data is coded rather than input data.
p_index, if non-zero, points to a location that should contain the index of the frame,
to allow checking that volatile input data is still valid.
*/
void EncodeFrameTrack::Init(void * data, size_t size, unsigned int samples, bool copy, bool del, bool coded,
                                int frame_index, int * p_frame_index)
{
    //ACE_DEBUG((LM_INFO, ACE_TEXT("EncodeFrameTrack::Init(frame_index=%d, coded=%s)\n"), frame_index, coded ? "true" : "false"));

    mSize = size;
    mSamples = samples;
    mFrameIndex = frame_index;

    if (mData && mDel)
    {
        free(mData);
    }
    if (data)
    {
        if (copy)
        {
            mpFrameIndex = 0; // no need to check for data disappearing
            void * local_data = malloc(size);
            if (local_data)
            {
                memcpy(local_data, data, size);
                mData = local_data;
                mDel = true;
            }
            else
            {
                // malloc failed!
                ACE_DEBUG((LM_ERROR, ACE_TEXT("EncodeFrameTrack::Init() malloc failed!\n")));
                mData = 0;
                mSize = 0;
                mSamples = 0;
                mDel = false;
            }
        }
        else
        {
            mData = data;
            mDel = del;
            mpFrameIndex = p_frame_index;
        }
    }
    else
    {
        mData = 0;
        mSize = 0;
        mSamples = 0;
        mDel = false;
        mpFrameIndex = 0;
    }
    mCoded = coded; // important to do this last - even better use a mutex
}

bool EncodeFrameTrack::Valid()
{
    return mpFrameIndex == 0 || *mpFrameIndex == mFrameIndex;
}

EncodeFrameTrack::~EncodeFrameTrack()
{
    //ACE_DEBUG((LM_INFO, ACE_TEXT("EncodeFrameTrack destructor for track of frame_index %d\n"), mFrameIndex));

    if (mDel)
    {
        free (mData);
    }
}

// EncodeFrame class

EncodeFrame::EncodeFrame()
{ }

EncodeFrame::~EncodeFrame()
{
    for (std::map<unsigned int, EncodeFrameTrack *>::iterator
        it = mTracks.begin(); it != mTracks.end(); ++it)
    {
        delete it->second;
    }
}

EncodeFrameTrack * & EncodeFrame::Track(unsigned int track_index)
{
    return mTracks[track_index];
}

/** Return true if all tracks coded. */
bool EncodeFrame::IsCoded() const
{
    bool result = true;
    for (std::map<unsigned int, EncodeFrameTrack *>::const_iterator
        it = mTracks.begin(); it != mTracks.end(); ++it)
    {
        result = result && it->second->Coded();
    }
    return mTracks.size() && result;
}

/** Return true if any track in error. */
bool EncodeFrame::Error() const
{
    bool result = false;
    for (std::map<unsigned int, EncodeFrameTrack *>::const_iterator
        it = mTracks.begin(); it != mTracks.end(); ++it)
    {
        result = result || it->second->Error();
    }
    return result;
}



/*

void EncodeFrame::Track(unsigned int trk, void * data, size_t size, bool copy, bool del, bool coded, int * p_index)
{
    mTracks[i].Track(data, size, copy, del, coded, p_index);
}

void * EncodeFrame::TrackData(unsigned int trk)
{
    void * data = 0;
    if (trk < mTracks.size())
    {
        data = mTracks[trk].Data();
    }
    return data;
}

size_t EncodeFrame::TrackSize(unsigned int trk)
{
    size_t size = 0;
    if (trk < mTracks.size())
    {
        size = mTracks[trk].Size();
    }
    return size;
}
*/

//  EncodeFrameBuffer class

EncodeFrameBuffer::EncodeFrameBuffer()
{ }

EncodeFrameBuffer::~EncodeFrameBuffer()
{
    for (std::map<unsigned int, EncodeFrame *>::iterator
        it = mFrameBuffer.begin(); it != mFrameBuffer.end(); ++it)
    {
        delete it->second;
    }
}

EncodeFrame * & EncodeFrameBuffer::Frame(unsigned int frame_index)
{
    ACE_Guard<ACE_Thread_Mutex> guard(mFrameBufferMutex);

    return mFrameBuffer[frame_index];
}

void EncodeFrameBuffer::EraseFrame(unsigned int index)
{
    ACE_Guard<ACE_Thread_Mutex> guard(mFrameBufferMutex);
    //ACE_DEBUG((LM_INFO, ACE_TEXT("EncodeFrameBuffer::EraseFrame(index %d)\n"), index));

    std::map<unsigned int, EncodeFrame *>::iterator it;
    if (mFrameBuffer.end() != (it = mFrameBuffer.find(index)))
    {
        delete it->second;
        mFrameBuffer.erase(it);
    }
}


size_t EncodeFrameBuffer::QueueSize()
{
    ACE_Guard<ACE_Thread_Mutex> guard(mFrameBufferMutex);

    return mFrameBuffer.size();
}

size_t EncodeFrameBuffer::CodedSize()
{
    ACE_Guard<ACE_Thread_Mutex> guard(mFrameBufferMutex);

    size_t coded_size = 0;
    for (std::map<unsigned int, EncodeFrame *>::const_iterator
        it = mFrameBuffer.begin(); it != mFrameBuffer.end(); ++it)
    {
        if (it->second->IsCoded())
        {
            ++coded_size;
        }
    }
    return coded_size;
}

void EncodeFrameBuffer::List()
{
    ACE_Guard<ACE_Thread_Mutex> guard(mFrameBufferMutex);

    for (std::map<unsigned int, EncodeFrame *>::iterator
        it = mFrameBuffer.begin(); it != mFrameBuffer.end(); ++it)
    {
        ACE_DEBUG((LM_INFO, ACE_TEXT("Buffer index %d, FrameTrack(0) index %d\n"),
            it->first, it->second->Track(0)->FrameIndex()));
    }
}

