/*
 * $Id: IngexRecorderImpl.cpp,v 1.1 2006/12/20 12:28:24 john_f Exp $
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

#include <sstream>

#include <Database.h>
#include <DBException.h>
#include "IngexShm.h"
#include "RecorderSettings.h"
#include "IngexRecorderImpl.h"
#include "FileUtils.h"

const ProdAuto::Rational EDIT_RATE = { 25, 1 };

IngexRecorderImpl * IngexRecorderImpl::mInstance = 0;

// To be called on completion of recording
void recording_completed(IngexRecorder * rec)
{
    // notify observers
    IngexRecorderImpl::Instance()->NotifyCompletion(rec);

    // destroy recorder
    delete rec;
}

// Implementation skeleton constructor
IngexRecorderImpl::IngexRecorderImpl (void)
: mRecording(false)//, mpDatabase(0)
{
    mInstance = this;
}

// Initialise the recorder
bool IngexRecorderImpl::Init(const char * name)
{
    mName = name;

    // Database
    try
    {
        prodauto::Database::initialise("prodautodb","ingex","ingex",4,12);
    }
    catch (...)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database init failed!\n")));
    }

    bool result = IngexShm::Instance()->Init();
    if (result == false)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Shared Memory init failed!\n")));
    }

    // Each card has 1 video and 4 audio tracks
    mTrackCount = IngexShm::Instance()->Cards() * 5;

    // Create tracks
    mTracks = new ProdAuto::TrackList;
    mTracks->length(mTrackCount);

    // Set track names (currently hard-coded) and other parameters
    for(CORBA::ULong i = 0; i < mTrackCount; ++i)
    {
        ProdAuto::Track & track = mTracks[i];
        if(i % 5 == 0)
        {
            // video track
            track.type = ProdAuto::VIDEO;
            std::ostringstream s;
            s << "V" << "  (card " << i / 5 << ")";
            track.name = CORBA::string_dup(s.str().c_str());
        }
        else
        {
            // audio track
            track.type = ProdAuto::AUDIO;
            std::ostringstream s;
            s << "A" << i % 5 << " (card " << i / 5 << ")";
            track.name = CORBA::string_dup(s.str().c_str());
        }
        track.id = i;
        /*
        track.edit_rate = EDIT_RATE;
        ProdAuto::UMID & umid = track.source_clip.src.package_id;
        umid.length(32);
        for(unsigned int j = 0; j < umid.length(); ++j)
        {
            track.source_clip.src.package_id[j] = 0;
        }
        track.source_clip.src.track_id = 0;
        track.source_clip.length = 0;
        track.source_clip.position = 0;
        */
    }

    // Recorder settings and track sources
    UpdateConfig();

    // Tracks status
    mTracksStatus = new ProdAuto::TrackStatusList;
    mTracksStatus->length(mTrackCount);
    for(unsigned int i = 0; i < mTrackCount; ++i)
    {
        ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);
        ts.rec = 0;
        ts.signal_present = 1;
        // only initialise timecode rate at the moment
        ts.timecode.undefined = true;
        ts.timecode.edit_rate = EDIT_RATE;
    }

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
    IngexShm::Instance()->TcMode(settings->tc_mode);

    // Start copy process
    mCopyManager.Command(settings->copy_command);
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Copy command: %C\n"),
        settings->copy_command.c_str()));
    mCopyManager.StartCopying();

    return result;
}

// Implementation skeleton destructor
IngexRecorderImpl::~IngexRecorderImpl (void)
{
    IngexShm::Destroy();
    prodauto::Database::close();
}

::ProdAuto::MxfDuration IngexRecorderImpl::MaxPreRoll (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    return mMaxPreRoll;
}

::ProdAuto::MxfDuration IngexRecorderImpl::MaxPostRoll (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    return mMaxPostRoll;
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

::ProdAuto::Rational IngexRecorderImpl::EditRate (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
    return EDIT_RATE;
}

::ProdAuto::TrackList * IngexRecorderImpl::Tracks (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    // Update from database
    UpdateSources();

    // Make a copy to return
    ProdAuto::TrackList_var tracks = mTracks;
    return tracks._retn();
}

::ProdAuto::TrackStatusList * IngexRecorderImpl::TracksStatus (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    // Update member with current timecode
    for(unsigned int card = 0; card < IngexShm::Instance()->Cards(); ++card)
    {
        framecount_t timecode = IngexShm::Instance()->CurrentTimecode(card);
        for(int card_track = 0; card_track < 5; ++card_track)
        {
            ProdAuto::TrackStatus & ts = mTracksStatus->operator[](5 * card + card_track);
            if(timecode >= 0)
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

    // Make a copy to return
    ProdAuto::TrackStatusList_var tracks_status = mTracksStatus;
    return tracks_status._retn();
}

::ProdAuto::Recorder::ReturnCode IngexRecorderImpl::Start (
    const ::ProdAuto::MxfTimecode & start_timecode,
    const ::ProdAuto::MxfDuration & pre_roll,
    const ::ProdAuto::BooleanList & rec_enable,
    const char * tag
  )
  throw (
    ::CORBA::SystemException
  )
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorderImpl::Start()\n")));

    // Enforce start-stop-start sequence
    if(mRecording)
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
    bool card_enable[MAX_CARDS];
    for(int card_i = 0; card_i < MAX_CARDS; ++card_i)
    {
        // Start with card enables false.
        card_enable[card_i] = false;
    }

    bool track_enable[MAX_CARDS * 5];
    for(unsigned int card_i = 0; card_i < IngexShm::Instance()->Cards(); ++card_i)
    {
        for(int j = 0; j < 5; ++j)
        {
            CORBA::ULong track_i = card_i * 5 + j;
            track_enable[track_i] = rec_enable[track_i];
            if(rec_enable[track_i])
            {
                // If one track enabled, set card enable.
                card_enable[card_i] = true;
            }
        }
    }
    // NB. We don't do anything about quad-split here, that is a
    // lower level matter.


    // Make a new IngexRecorder to manage this recording.
    mpIngexRecorder = new IngexRecorder(mName);

    // Set-up callback for completion of recording
    mpIngexRecorder->SetCompletionCallback(&recording_completed);

    // Determine start timecode or crash-record
    unsigned long start;
    bool crash;
    if(start_timecode.undefined)
    //if(1)  // tmp force "start now"
    {
        start = 0;
        crash = true;
    }
    else
    {
        start = start_timecode.samples;
        crash = false;
    }

    // Prepare start
    bool ok = mpIngexRecorder->PrepareStart(
        start,
        pre_roll.undefined ? 0 : pre_roll.samples,
        card_enable,
        track_enable,
        crash,
        tag);

    // Start
    if(ok)
    {
        ok = mpIngexRecorder->Start();
        mRecording = true;
    }
    else
    {
        delete mpIngexRecorder;
        mpIngexRecorder = 0;
    }

    // Set status info
    if(ok)
    {
        ProdAuto::StatusItem status;
        status.name = CORBA::string_dup("state");
        status.value = CORBA::string_dup("recording");
        mStatusDist.SendStatus(status);

        for(CORBA::ULong i = 0; i < mTrackCount; ++i)
        {
            ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);
            ts.rec = rec_enable[i];
        }
    }

    // Return
    return (ok ? ProdAuto::Recorder::SUCCESS : ProdAuto::Recorder::FAILURE);
}

::ProdAuto::Recorder::ReturnCode IngexRecorderImpl::Stop (
    const ::ProdAuto::MxfTimecode & mxf_stop_timecode,
    const ::ProdAuto::MxfDuration & mxf_post_roll
  )
  throw (
    ::CORBA::SystemException
  )
{
    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorderImpl::Stop()\n")));

    // Translate parameters.
    framecount_t stop_timecode = (mxf_stop_timecode.undefined ? -1 : mxf_stop_timecode.samples);

    framecount_t post_roll = (mxf_post_roll.undefined ? 0 : mxf_post_roll.samples);

    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorderImpl::Stop(%d, %d)\n"),
    //    stop_timecode, post_roll));


    // Tell recorder when to stop.
    if(mpIngexRecorder)
    {
        mpIngexRecorder->Stop(stop_timecode, post_roll);
    }
    mpIngexRecorder = 0;  // It will be deleted when it signals completion

    // Update status.
    mRecording = false;
    for(CORBA::ULong i = 0; i < mTrackCount; ++i)
    {
        ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);
        ts.rec = 0;
    }

    // Return
    return ProdAuto::Recorder::SUCCESS;
}

void IngexRecorderImpl::DoStop(framecount_t timecode, framecount_t post_roll)
{
    // Tell recorder when to stop.
    if(mpIngexRecorder)
    {
        mpIngexRecorder->Stop(timecode, post_roll);
    }
    mpIngexRecorder = 0;  // It will be deleted when it signals completion

    // Update status.
    mRecording = false;
    for(CORBA::ULong i = 0; i < mTrackCount; ++i)
    {
        ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);
        ts.rec = 0;
    }
}

void IngexRecorderImpl::UpdateConfig (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    // Settings
    // No need to update as IngexRecorder does this for each new recording

    // Sources
    UpdateSources();
}

void IngexRecorderImpl::NotifyCompletion(IngexRecorder * rec)
{
    ACE_DEBUG((LM_INFO, "IngexRecorderImpl::NotifyCompletion()\n"));

    ProdAuto::StatusItem status;
    status.name = CORBA::string_dup("state");
    status.value = CORBA::string_dup("stopped");

    mStatusDist.SendStatus(status);

    mCopyManager.Command(RecorderSettings::Instance()->copy_command);
    mCopyManager.StartCopying();
}


void IngexRecorderImpl::ReadTrackConfig(unsigned int n_tracks)
{
    const char * delim = "\t\n";
    FILE * fp = fopen("config.txt", "r");
    const int bufsize = 1024;
    char buf[bufsize];
    for(CORBA::ULong i = 0; i < n_tracks && NULL != fgets(buf, bufsize, fp); ++i)
    {
        ProdAuto::Source & source = mTracks[i].src;
        const char * s = strtok(buf, delim);
        const char * t = strtok(NULL, delim);
        if(NULL != t)
        {
            source.package_name = CORBA::string_dup(s);
            source.track_name = CORBA::string_dup(t);
        }
    }
}

/**
Update source information from database
*/
void IngexRecorderImpl::UpdateSources()
{
    prodauto::Database * db;
    bool db_ok = true;
    try
    {
        db = prodauto::Database::getInstance();
    }
    catch(const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
        db_ok = false;
    }

    prodauto::Recorder * rec = 0;
    if(db_ok)
    {
        const char * name = IngexRecorderImpl::Instance()->Name();
        try
        {
            rec = db->loadRecorder(name);
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Loaded Recorder \"%C\"\n"), rec->name.c_str()));
        }
        catch(const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
            db_ok = false;
        }
    }
    std::auto_ptr<prodauto::Recorder> recorder(rec); // so it gets deleted

    prodauto::RecorderConfig * rc = 0;
    if (rec && rec->hasConfig())
    {
        rc = rec->getConfig();
    }

    // Now we have RecorderConfig, we can set the source track names
    if (db_ok && rc)
    {
        for (unsigned int i = 0;
            i < mTrackCount / 5 && i < rc->recorderInputConfigs.size(); ++i)
        {
            prodauto::RecorderInputConfig * ric = rc->getInputConfig(i + 1);
            for(unsigned int j = 0; j < ric->trackConfigs.size() && j < 5; ++j)
            {
                prodauto::RecorderInputTrackConfig * ritc = 0;
                if (ric)
                {
                    ritc = ric->trackConfigs[j];
                }
                prodauto::SourceConfig * sc = 0;
                if (ritc)
                {
                    sc = ritc->sourceConfig;
                }
                prodauto::SourceTrackConfig * stc = 0;
                if (sc)
                {
                    stc = sc->getTrackConfig(ritc->sourceTrackID);
                }

		ProdAuto::Track & track = mTracks[(CORBA::ULong)i * 5 + j];
		ProdAuto::Source & source = track.src;

                if (sc && stc)
                {
		    track.has_source = 1;
		    source.package_name = CORBA::string_dup(sc->name.c_str());
		    source.track_name = CORBA::string_dup(stc->name.c_str());
                }
		else
		{
		    // No connection to this input
		    track.has_source = 0;
		    source.package_name = CORBA::string_dup("zz No Connection");
		    source.track_name = CORBA::string_dup("");
		}
            }
        }
    }
}

void IngexRecorderImpl::Destroy()
{
  // Get the POA used when activating the object.
  PortableServer::POA_var poa =
    this->_default_POA ();

  // Get the object ID associated with this servant.
  PortableServer::ObjectId_var oid =
    poa->servant_to_id (this);

  // Now deactivate the object.
  poa->deactivate_object (oid.in ());

  // Decrease the reference count on ourselves.
  this->_remove_ref ();
}

