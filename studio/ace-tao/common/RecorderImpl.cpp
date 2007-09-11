/*
 * $Id: RecorderImpl.cpp,v 1.1 2007/09/11 14:08:33 stuart_hc Exp $
 *
 * Base class for Recorder servant.
 *
 * Copyright (C) 2007  British Broadcasting Corporation.
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
#include <ace/Min_Max.h>

#include <sstream>

#include <Database.h>
#include <DBException.h>

#include "RecorderSettings.h"
#include "RecorderImpl.h"


// Implementation skeleton constructor
RecorderImpl::RecorderImpl (void)
: mMaxInputs(0), mMaxTracksPerInput(0), mVideoTrackCount(0)
{
    mTracks = new ProdAuto::TrackList;
    mTracksStatus = new ProdAuto::TrackStatusList;
}

// Initialise the recorder
bool RecorderImpl::Init(std::string name, std::string db_user, std::string db_pw,
                        unsigned int max_inputs, unsigned int max_tracks_per_input)
{
	ACE_DEBUG((LM_DEBUG, ACE_TEXT("RecorderImpl::Init(%C, %C, %C)\n"),
	    name.c_str(), db_user.c_str(), db_pw.c_str()));

    // Save name
    mName = name;

    // Set maximum number of tracks for this particular implementation,
    // e.g. as determined from capture cards installed in the machine.
    mMaxInputs = max_inputs;
    mMaxTracksPerInput = max_tracks_per_input;

    // Database
    bool ok_so_far = true;
    try
    {
        prodauto::Database::initialise("prodautodb", db_user, db_pw, 4, 12);
    }
    catch (...)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database init failed!\n")));
        ok_so_far = false;
    }

    // Recorder track sources
    UpdateSources();

    return ok_so_far;
}

// Implementation skeleton destructor
RecorderImpl::~RecorderImpl (void)
{
    prodauto::Database::close();
}

::ProdAuto::MxfDuration RecorderImpl::MaxPreRoll (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    return mMaxPreRoll;
}

::ProdAuto::MxfDuration RecorderImpl::MaxPostRoll (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    return mMaxPostRoll;
}

char * RecorderImpl::RecordingFormat (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
    return CORBA::string_dup("Ingex recorder");
}

::ProdAuto::Rational RecorderImpl::EditRate (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
    return EDIT_RATE;
}

::ProdAuto::TrackList * RecorderImpl::Tracks (
    
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

/**
Means of externally forcing a re-reading of config from database.
*/
void RecorderImpl::UpdateConfig (
    
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

/**
Update source information from database
*/
void RecorderImpl::UpdateSources()
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
    if (db_ok)
    {
        try
        {
            rec = db->loadRecorder(mName);
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
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Loaded Recorder \"%C\", Config \"%C\"\n"),
            rec->name.c_str(), rc->name.c_str()));
    }

    // Now we have RecorderConfig, we can set the source track names
    if (db_ok && rc)
    {
        const unsigned int n_inputs = ACE_MIN(rc->recorderInputConfigs.size(), mMaxInputs);
        CORBA::ULong track_i = 0;
        unsigned int n_video_tracks = 0;
        for (unsigned int i = 0; i < n_inputs; ++i)
        {
            prodauto::RecorderInputConfig * ric = rc->getInputConfig(i + 1);
            // We force number of tracks per input to be the hardware max because
            // this makes it easier to map to hardware parameters when filling out
            // tracks status with "signal present" etc.
            //const unsigned int n_tracks = ACE_MIN(ric->trackConfigs.size(), mMaxTracksPerInput);
            const unsigned int n_tracks = mMaxTracksPerInput;
            for (unsigned int j = 0; j < n_tracks; ++j)
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

                mTracks->length(track_i + 1);
		        ProdAuto::Track & track = mTracks[track_i];

                // Assuming first track of an input is video, others audio.
                std::ostringstream s;
                if (j == 0)
                {
                    ++n_video_tracks;
                    track.type = ProdAuto::VIDEO;
                    s << "V";
                }
                else
                {
                    track.type = ProdAuto::AUDIO;
                    s << "A" << j;
                }
                s << "  (input " << i << ")";
                track.name = CORBA::string_dup(s.str().c_str());
                track.id = j; // Is this ever used?

                if (sc && stc)
                {
		            track.has_source = 1;
		            track.src.package_name = CORBA::string_dup(sc->name.c_str());
		            track.src.track_name = CORBA::string_dup(stc->name.c_str());
                }
		        else
		        {
		            // No connection to this input
		            track.has_source = 0;
		            track.src.package_name = CORBA::string_dup("zz No Connection");
		            track.src.track_name = CORBA::string_dup("");
		        }

                ++track_i;
            } // tracks
        } // inputs
        mVideoTrackCount = n_video_tracks;
    }

    // Initialise tracks status, details will be filled out
    // whenever client requests it.
    mTracksStatus->length(mTracks->length());

    for (unsigned int i = 0; i < mTracksStatus->length(); ++i)
    {
        ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);
        ts.rec = 0;
        ts.signal_present = 0;
        ts.timecode.undefined = true;
        ts.timecode.edit_rate = EDIT_RATE;
    }
}

void RecorderImpl::Destroy()
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

