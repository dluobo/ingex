/*
 * $Id: IngexRecorder.h,v 1.1 2007/09/11 14:08:30 stuart_hc Exp $
 *
 * Class to manage an individual recording.
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

#ifndef IngexRecorder_h
#define IngexRecorder_h

#include <string>
#include <list>

#include <ace/Basic_Types.h>
#include <ace/Thread_Mutex.h>
#include <ace/Guard_T.h>

#include "IngexShm.h"  // Wrapper for shared memory API. Within this, MAX_CARDS is #defined

// Assuming here that MAX_CARDS == 4
const char * const SOURCE_NAME[] = { "card0", "card1", "card2", "card3" };
const char * const QUAD_NAME = "quad";


// This interval ensures that timecode found during PrepareStart() is still
// available in buffer for Start().
const int SEARCH_GUARD = 5;

#include "Database.h"  // For Recorder
#include "RecordOptions.h"
#include "recorder_types.h" // for framecount_t

class IngexRecorder;

/**
Data passed when invoking record thread.
*/
//typedef struct {
//    //int         invoke_card;
//    IngexRecorder * p_rec;
//    RecordOptions * p_opt;
//} ThreadInvokeArg;

struct ThreadParam
{
    ACE_thread_t id;
    IngexRecorder * p_rec;
    RecordOptions * p_opt;
};


// Thread functions
ACE_THR_FUNC_RETURN start_record_thread(void *p_arg);
ACE_THR_FUNC_RETURN start_quad_thread(void *p_arg);
ACE_THR_FUNC_RETURN manage_record_thread(void *p_arg);

/**
Low-level interface to Ingex recorder.
Make one instance of this class for each recording.
Within that recording there will be a number of threads dealing with
different sources etc.
*/
class IngexRecorder
{

// Per-recording functions
public:
    IngexRecorder(const std::string & name);
    ~IngexRecorder();

    void Setup(
                bool card_enable[],
                bool trk_enable[],
                const char * project,
                const char * description,
                const std::vector<std::string> & tapes);

    bool PrepareStart(
                framecount_t & start_timecode,
                framecount_t pre_roll,
                bool crash_record);

    bool Start(void);

    bool Stop(framecount_t & stop_timecode, framecount_t post_roll);

    void SetCompletionCallback(void(*p_fn)(IngexRecorder *)) { mpCompletionCallback = p_fn; }

    bool WriteMetadataFile(const char * meta_name);

    void NoteFailure()
    {
        // Should maybe use a mutex
        mRecordingOK = false;
    }

    framecount_t OutTime();

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
    // Current RecorderConfig
    // May be better to return a unique ptr for thread safety
    prodauto::Recorder * Recorder() const { return mRecorder.get(); }

    bool GetDbInfo(const std::vector<std::string> & tapes);
    void DoCompletionCallback() { if(mpCompletionCallback) (*mpCompletionCallback)(this); }

// Per-recording data
private:
    std::string mName;
    framecount_t mStartTimecode;

    // These are used to write metadata file on completion of recording
    char mVideoPath[FILENAME_MAX];
    char mDvdPath[FILENAME_MAX];

    void (* mpCompletionCallback)(IngexRecorder *);

    ACE_thread_t mManageThreadId;

// Database objects
    std::auto_ptr<prodauto::Recorder> mRecorder;

// Duration
    framecount_t mTargetDuration;           ///< To signal when capture should stop
    ACE_Thread_Mutex mDurationMutex;

// Per-card data
// Enables
    bool mCardEnable[MAX_CARDS];
    bool mTrackEnable[MAX_CARDS * 5];

public:
// MXF filenames
    std::string mFileNames[MAX_CARDS * 5];

    bool mRecordingOK;

    //RecordOptions   record_opt[MAX_RECORD];
private:
    int mStartFrame[MAX_CARDS];

// Per-thread data
    std::vector<ThreadParam> mThreadParams;

    //ThreadInvokeArg invoke_arg[MAX_RECORD];
    //std::vector<ACE_thread_t> mThreadIds;

// Thread functions are friends
    friend ACE_THR_FUNC_RETURN start_record_thread(void *p_arg);
    friend ACE_THR_FUNC_RETURN start_quad_thread(void *p_arg);
    friend ACE_THR_FUNC_RETURN manage_record_thread(void *p_arg);

};

#endif // ifndef IngexRecorder_h
