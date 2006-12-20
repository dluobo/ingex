/*
 * $Id: IngexRecorder.h,v 1.1 2006/12/20 12:28:24 john_f Exp $
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

const int MAX_RECORD = MAX_CARDS + 1;   // real + virtual (quad split source)
const int QUAD_SOURCE = MAX_CARDS;  // "card" id of quad source where ids are 0,1,2,3...

// Assuming here that MAX_CARDS == 4
static const char * SOURCE_NAME[] = { "card0", "card1", "card2", "card3", "quad" };


// This interval ensures that timecode found during PrepareStart() is still
// available in buffer for Start().
const int SEARCH_GUARD = 5;

#include "Database.h"  // For Recorder and SourceSession
#include "RecordOptions.h"  // also defines framecount_t

class IngexRecorder;

/**
Data passed when invoking record thread.
*/
typedef struct {
    int         invoke_card;
    IngexRecorder   *p_rec;
} ThreadInvokeArg;


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

    bool PrepareStart(
                framecount_t start_timecode,
                framecount_t pre_roll,
                bool enable[],
                bool track_enable[],
                bool crash_record,
                const char * tag);

    bool Start(void);

    bool Stop(framecount_t stop_timecode, framecount_t post_roll);

    void SetCompletionCallback(void(*p_fn)(IngexRecorder *)) { mpCompletionCallback = p_fn; }

    bool WriteMetadataFile(const char * meta_name);

    // Current RecorderConfig
    // May be better to return a unique ptr for thread safety
    prodauto::Recorder * Recorder() const { return mRecorder.get(); }

    // Current SourceSession
    // May be better to return a unique ptr for thread safety
    prodauto::SourceSession * SourceSession() const { return mSourceSession.get(); }

private:
    bool GetDbInfo();
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
    std::auto_ptr<prodauto::SourceSession> mSourceSession;

// Per-card per-recording data
public:
    RecordOptions   record_opt[MAX_RECORD];
private:
    ThreadInvokeArg invoke_arg[MAX_RECORD];

// Thread functions are friends
    friend ACE_THR_FUNC_RETURN start_record_thread(void *p_arg);
    friend ACE_THR_FUNC_RETURN start_quad_thread(void *p_arg);
    friend ACE_THR_FUNC_RETURN manage_record_thread(void *p_arg);

};

#endif // ifndef IngexRecorder_h

