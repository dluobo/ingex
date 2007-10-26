/*
 * $Id: IngexRecorderImpl.cpp,v 1.2 2007/10/26 15:52:26 john_f Exp $
 *
 * Servant class for Recorder.
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

#include <ace/OS_NS_time.h>
#include <ace/OS_NS_unistd.h>

#include <sstream>

#include <Database.h>
#include <DBException.h>

#include "IngexShm.h"
#include "RecorderSettings.h"
#include "IngexRecorderImpl.h"
#include "FileUtils.h"
#include "Timecode.h"
#include "DateTime.h"

IngexRecorderImpl * IngexRecorderImpl::mInstance = 0;

// To be called on completion of recording
void recording_completed(IngexRecorder * rec)
{
#if 0
    // Test - keep recording in chunks
    const framecount_t chunk_size = 60 * 25; // 1 min
    framecount_t restart = rec->OutTime();
    bool ok = rec->PrepareStart(restart, 0, false);
    if (ok)
    {
        // make new out time a round figure
        framecount_t out = restart + chunk_size;
        out = (out / chunk_size) * chunk_size;
        // set duration for this chunk
        rec->TargetDuration(out - restart);
        rec->Start();
    }
#else
    // notify observers
    IngexRecorderImpl::Instance()->NotifyCompletion(rec);

    // destroy recorder
    delete rec;
#endif
}

// Implementation skeleton constructor
IngexRecorderImpl::IngexRecorderImpl (void)
: mRecording(false)//, mpDatabase(0)
{
    mInstance = this;
}

// Initialise the recorder
bool IngexRecorderImpl::Init(std::string name, std::string db_user, std::string db_pw)
{
	ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorderImpl::Init(%C, %C, %C)\n"),
	    name.c_str(), db_user.c_str(), db_pw.c_str()));

    // Shared memory initialisation
    bool result = IngexShm::Instance()->Init();
    if (result == false)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Shared Memory init failed!\n")));
    }

    // Base class initialisation
    // Each card has 1 video and 4 audio tracks
    const unsigned int max_inputs = IngexShm::Instance()->Cards();
    const unsigned int max_tracks_per_input = 5;
    RecorderImpl::Init(name, db_user, db_pw, max_inputs, max_tracks_per_input);


    // Setup pre-roll
    mMaxPreRoll.undefined = false;
    mMaxPreRoll.edit_rate = EDIT_RATE;
    if (IngexShm::Instance()->RingLength() > (SEARCH_GUARD + 1))
    {
        mMaxPreRoll.samples = IngexShm::Instance()->RingLength() - (SEARCH_GUARD + 1);
    }
    else
    {
        mMaxPreRoll.samples = 0;
    }


    // Setup post-roll
    mMaxPostRoll.undefined = true; // no limit to post-roll
    mMaxPostRoll.edit_rate = EDIT_RATE;
    mMaxPostRoll.samples = 0;

    RecorderSettings * settings = RecorderSettings::Instance();
    settings->Update(mName);

    // Set timecode mode
    switch (settings->timecode_mode)
    {
    case LTC_PARAMETER_VALUE:
        IngexShm::Instance()->TcMode(IngexShm::LTC);
        break;
    case VITC_PARAMETER_VALUE:
        IngexShm::Instance()->TcMode(IngexShm::VITC);
        break;
    default:
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Unexpected timecode mode\n")));
        break;
    }

    // Start copy process
    mCopyManager.Command(settings->copy_command);
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Copy command: %C\n"),
        settings->copy_command.c_str()));
    mCopyManager.StartCopying();

    char buf[256];
    if (ACE_OS::hostname(buf, 256) == 0)
    {
        mHostname = buf;
    }

    return result;
}

// Implementation skeleton destructor
IngexRecorderImpl::~IngexRecorderImpl (void)
{
    IngexShm::Destroy();
    prodauto::Database::close();
}

char * IngexRecorderImpl::RecordingFormat (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
    return CORBA::string_dup("Ingex recorder");
}

::ProdAuto::TrackStatusList * IngexRecorderImpl::TracksStatus (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    // Update member with current timecode and status
    for (unsigned int card_i = 0; card_i < IngexShm::Instance()->Cards(); ++card_i)
    {
        bool signal = IngexShm::Instance()->SignalPresent(card_i);
        framecount_t timecode = IngexShm::Instance()->CurrentTimecode(card_i);

        // We know each input has mMaxTracksPerInput, see comments
        // in RecorderImpl::UpdateSources()
        for (unsigned int j = 0; j < mMaxTracksPerInput; ++j)
        {
            CORBA::ULong track_i = mMaxTracksPerInput * card_i + j;
            if (track_i < mTracksStatus->length())
            {
                ProdAuto::TrackStatus & ts = mTracksStatus->operator[](track_i);

                ts.signal_present = signal;
                if (timecode >= 0)
                {
                    ts.timecode.undefined = 0;
                    ts.timecode.samples = timecode;
                }
                else
                {
                    ts.timecode.undefined = 1;
                    ts.timecode.samples = 0;
                }
            }
        }
    }

    // Make a copy to return
    ProdAuto::TrackStatusList_var tracks_status = mTracksStatus;
    return tracks_status._retn();
}

::ProdAuto::Recorder::ReturnCode IngexRecorderImpl::Start (
    ::ProdAuto::MxfTimecode & start_timecode,
    const ::ProdAuto::MxfDuration & pre_roll,
    const ::CORBA::BooleanSeq & rec_enable,
    const char * project,
    const char * description,
    const ::CORBA::StringSeq & tapes,
    ::CORBA::Boolean test_only
  )
  throw (
    ::CORBA::SystemException
  )
{
    Timecode start_tc(start_timecode.undefined ? 0 : start_timecode.samples);
    ACE_DEBUG((LM_INFO, ACE_TEXT("IngexRecorderImpl::Start(), tc %C, time %C\n"),
        start_tc.Text(), DateTime::Timecode().c_str()));
    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("project \"%C\", description \"%C\"\n"), project, description));

    // Enforce start-stop-start sequence
    if (mRecording)
    {
        // Immediate stop
        DoStop(-1, 0);
    }

    // Stop any copy process to reduce disc activity.
    mCopyManager.StopCopying();

    // Make sure we are up-to-date with source config from database
    //UpdateSources();  Now done in IngexRecorder::PrepareStart()
    // No, source update not needed because MXF writing deals direct with database.
    // Our track config only used for CORBA Tracks method.
    // Do need to update record parameters and this is indeed on
    // in PrepareStart().

    // Translate enables to per-track and per-card.
    bool card_enable[MAX_CHANNELS];
    for (unsigned int card_i = 0; card_i < MAX_CHANNELS; ++card_i)
    {
        // Start with card enables false.
        card_enable[card_i] = false;
    }

    bool track_enable[MAX_CHANNELS * 5];
    for (unsigned int card_i = 0; card_i < IngexShm::Instance()->Cards(); ++card_i)
    {
        for (int j = 0; j < 5; ++j)
        {
            CORBA::ULong track_i = card_i * 5 + j;

            // Copy track enable into local bool array.
            if (track_i < mTracks->length() && mTracks[track_i].has_source)
            {
                track_enable[track_i] = rec_enable[track_i];
            }
            else
            {
                // If no source connected, disable track.
                track_enable[track_i] = false;
            }

            // Set card enable.
            if (track_enable[track_i])
            {
                // If one track enabled, set card enable.
                card_enable[card_i] = true;
            }
        }
    }
    // NB. We don't do anything about quad-split here, that is a
    // lower level matter.

    // Tape names
    if (tapes.length() != mVideoTrackCount)
    {
        ACE_DEBUG((LM_WARNING, ACE_TEXT("Wrong number of tape names: %d supplied, %d expected.\n"),
            tapes.length(), mVideoTrackCount));
    }
    std::vector<std::string> tape_list;
    for (CORBA::ULong i = 0; i < tapes.length(); ++i)
    {
        const char * s = tapes[i];
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("card%d tapeid \"%C\"\n"), i, s));
        tape_list.push_back(std::string(s));
    }

    // Make a new IngexRecorder to manage this recording.
    mpIngexRecorder = new IngexRecorder(mName);

    // Set-up callback for completion of recording
    mpIngexRecorder->SetCompletionCallback(&recording_completed);

    // Determine start timecode or crash-record
    framecount_t start;
    bool crash;
    if (start_timecode.undefined)
    //if (1)  // tmp force "start now"
    {
        start = 0;
        crash = true;
    }
    else
    {
        start = start_timecode.samples;
        crash = false;
    }

    // Setup IngexRecorder
    mpIngexRecorder->Setup(
        card_enable,
        track_enable,
        project,
        description,
        tape_list);

    // Prepare start
    bool ok = mpIngexRecorder->PrepareStart(
        start,
        pre_roll.undefined ? 0 : pre_roll.samples,
        crash);

    // Set return value for actual start timecode
    start_timecode.undefined = false;
    start_timecode.edit_rate = EDIT_RATE;
    start_timecode.samples = start;

    // Start
    if (!test_only)
    {
        if (ok)
        {
            ok = mpIngexRecorder->Start();
            mRecording = true;
        }

        // Set status info
        if (ok)
        {
            ProdAuto::StatusItem status;
            status.name = CORBA::string_dup("recording");
            status.value = CORBA::string_dup("starting");
            mStatusDist.SendStatus(status);

            for (CORBA::ULong i = 0; i < mTracksStatus->length(); ++i)
            {
                ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);
                ts.rec = rec_enable[i];
            }
        }
    }

    if (!ok || test_only)
    {
        delete mpIngexRecorder;
        mpIngexRecorder = 0;
    }

    // Return
    return (ok ? ProdAuto::Recorder::SUCCESS : ProdAuto::Recorder::FAILURE);
}

::ProdAuto::Recorder::ReturnCode IngexRecorderImpl::Stop (
    ::ProdAuto::MxfTimecode & mxf_stop_timecode,
    const ::ProdAuto::MxfDuration & mxf_post_roll,
    ::CORBA::StringSeq_out files
  )
  throw (
    ::CORBA::SystemException
  )
{
    ACE_DEBUG((LM_INFO, ACE_TEXT("IngexRecorderImpl::Stop()\n")));

    // Translate parameters.
    framecount_t stop_timecode = (mxf_stop_timecode.undefined ? -1 : mxf_stop_timecode.samples);

    framecount_t post_roll = (mxf_post_roll.undefined ? 0 : mxf_post_roll.samples);

    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorderImpl::Stop(%d, %d)\n"),
    //    stop_timecode, post_roll));

    // Create out parameter
    files = new ::CORBA::StringSeq;
    files->length(IngexShm::Instance()->Cards() * 5);

    // Tell recorder when to stop.
    if (mpIngexRecorder)
    {
        mpIngexRecorder->Stop(stop_timecode, post_roll);

        // Return the expected "out time"
        mxf_stop_timecode.undefined = false;
        mxf_stop_timecode.edit_rate = EDIT_RATE;
        mxf_stop_timecode.samples = stop_timecode;

        // Return the filenames
        for (unsigned int card_i = 0; card_i < IngexShm::Instance()->Cards(); ++card_i)
        {
            //RecordOptions & opt = mpIngexRecorder->record_opt[card_i];
            for (unsigned int j = 0; j < 5; ++j)
            {
                CORBA::ULong track_i = card_i * 5 + j;
                std::string name = mpIngexRecorder->mFileNames[track_i];
                if (!name.empty())
                {
                    name = "/" + mHostname + name;
                }
                files->operator[](track_i) = name.c_str();
            }
        }
    }
    mpIngexRecorder = 0;  // It will be deleted when it signals completion

    // Update status to "not recording".
    mRecording = false;
    for (CORBA::ULong i = 0; i < mTracksStatus->length(); ++i)
    {
        ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);
        ts.rec = 0;
    }

    // Return
    return ProdAuto::Recorder::SUCCESS;
}

/**
Simplified Stop without returning filenames etc.
*/
void IngexRecorderImpl::DoStop(framecount_t timecode, framecount_t post_roll)
{
    // Tell recorder when to stop.
    if (mpIngexRecorder)
    {
        mpIngexRecorder->Stop(timecode, post_roll);
    }
    mpIngexRecorder = 0;  // It will be deleted when it signals completion

    // Update status to "not recording".
    mRecording = false;
    for (CORBA::ULong i = 0; i < mTracksStatus->length(); ++i)
    {
        ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);
        ts.rec = 0;
    }
}

void IngexRecorderImpl::NotifyCompletion(IngexRecorder * rec)
{
    ACE_DEBUG((LM_INFO, "IngexRecorderImpl::NotifyCompletion()\n"));

    ProdAuto::StatusItem status;
    status.name = CORBA::string_dup("recording");
    if (rec->mRecordingOK)
    {
        status.value = CORBA::string_dup("completed");
    }
    else
    {
        status.value = CORBA::string_dup("failed");
    }

    mStatusDist.SendStatus(status);

    mCopyManager.Command(RecorderSettings::Instance()->copy_command);
    mCopyManager.StartCopying();
}





