/*
 * $Id: SimplerouterloggerImpl.cpp,v 1.2 2008/02/06 16:59:00 john_f Exp $
 *
 * Servant class for RouterRecorder.
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

#include <ace/Log_Msg.h>

#include <sstream>

#include "SimplerouterloggerImpl.h"
#include "FileUtils.h"
#include "ClockReader.h"
#include "EasyReader.h"
#include "DateTime.h"
#include "Timecode.h"
#include "SourceReader.h"
#include "quartzRouter.h"
#include "CutsDatabase.h"

const bool USE_EASYREADER = true;
const std::string DEFAULT_TC_PORT = "com4";

SimplerouterloggerImpl * SimplerouterloggerImpl::mInstance = 0;


// Implementation skeleton constructor
SimplerouterloggerImpl::SimplerouterloggerImpl (void)
: mpRouter(0), mpTcReader(0), mpSrcReader(0), mpCutsDatabase(0), mpFile(0), mDestination(0)
{
    mInstance = this;

    mpRouter = new Router;
    if (USE_EASYREADER)
    {
        // Use external timecode reader
        mpTcReader = new EasyReader;
    }
    else
    {
        // Use PC clock
        mpTcReader = new ClockReader;
    }
    mpSrcReader = new SourceReader;

    mpCutsDatabase = new CutsDatabase();
}

// Implementation skeleton destructor
SimplerouterloggerImpl::~SimplerouterloggerImpl (void)
{
    if (mpRouter)
    {
        mpRouter->Stop();
        mpRouter->wait();
        delete mpRouter;
    }

    delete mpTcReader;
    delete mpSrcReader;
    delete mpCutsDatabase;
}


// Initialise the routerlogger
bool SimplerouterloggerImpl::Init(const std::string & rport, const std::string & tcport, unsigned int dest, const std::string & db_file)
{
    // Setup pre-roll
    mMaxPreRoll.undefined = false;
    mMaxPreRoll.edit_rate = EDIT_RATE;
    mMaxPreRoll.samples = 0;

    // Setup post-roll
    mMaxPostRoll.undefined = true; // no limit to post-roll
    mMaxPostRoll.edit_rate = EDIT_RATE;
    mMaxPostRoll.samples = 0;

    // Create tracks
    mTracks->length(1);
    ProdAuto::Track & track = mTracks->operator[](0);
    track.type = ProdAuto::VIDEO;
    track.name = "V";
    track.has_source = 1;

    mTracksStatus->length(1);
    ProdAuto::TrackStatus & ts = mTracksStatus->operator[](0);
    ts.signal_present = true;
    ts.timecode.edit_rate = EDIT_RATE;
    ts.timecode.undefined = 0;
    ts.timecode.samples = 0;

    // Init database connection
    mpSrcReader->Init("KW-Router", "prodautodb", "bamzooki", "bamzooki");

    if (mpCutsDatabase)
    {
        mpCutsDatabase->Filename(db_file);
    }

    // Init router
    mpRouter->Init(rport);
    mpRouter->SetObserver(this);
    mpRouter->activate();

    // Init timecode reader, if present
    if (USE_EASYREADER)
    {
        EasyReader * er = static_cast<EasyReader *>(mpTcReader);
        if (!tcport.empty())
        {
            er->Init(tcport.c_str());
        }
        else
        {
            er->Init(DEFAULT_TC_PORT.c_str());
        }
    }

    // Set destination
    mDestination = dest;

    // Get info about current routing to write at start of file
    // and/or in database
    std::string tc = mpTcReader->Timecode();

    std::string src = mpRouter->CurrentSrc(mDestination);
    int src_index = atoi(src.substr(6).c_str());

    std::string srcstring;
    uint32_t track_i;
    mpSrcReader->GetSource(src_index, srcstring, track_i);

    mLastSrc = srcstring;
    mLastTc = tc;

    return true;
}

::ProdAuto::TrackStatusList * SimplerouterloggerImpl::TracksStatus (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    std::string tc_string = mpTcReader->Timecode();
    Timecode tc(tc_string.c_str());

    ProdAuto::TrackStatus & ts = mTracksStatus->operator[](0);
    ts.timecode.samples = tc.FramesSinceMidnight();

    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("TracksStatus - timecode %C\n"), tc.Text()));

    // Make a copy to return
    ProdAuto::TrackStatusList_var tracks_status = mTracksStatus;
    return tracks_status._retn();
}

::ProdAuto::Recorder::ReturnCode SimplerouterloggerImpl::Start (
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
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("SimplerouterloggerImpl::Start()\n")));

    const std::string dir = "C:\\TEMP\\RouterLogs\\";

    FileUtils::CreatePath(dir);

    std::string date = DateTime::DateNoSeparators();
    Timecode tc = mpTcReader->Timecode().c_str();
    std::ostringstream ss;
    ss << dir << date << "_" << tc.TextNoSeparators() << "_" << mName << ".txt";


    //start saving to file
    StartSavingFile(ss.str());

    ProdAuto::TrackStatus & ts = mTracksStatus->operator[](0);
    ts.rec = 1;

    // Return
    return ProdAuto::Recorder::SUCCESS;
}

::ProdAuto::Recorder::ReturnCode SimplerouterloggerImpl::Stop (
    ::ProdAuto::MxfTimecode & mxf_stop_timecode,
    const ::ProdAuto::MxfDuration & mxf_post_roll,
    ::CORBA::StringSeq_out files
  )
  throw (
    ::CORBA::SystemException
  )
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("SimplerouterloggerImpl::Stop()\n")));

    StopSavingFile();

    ProdAuto::TrackStatus & ts = mTracksStatus->operator[](0);
    ts.rec = 0;

    // Create out parameter
    files = new ::CORBA::StringSeq;
    files->length(1);
    // TODO: Put the actual filename in here
    files->operator[](0) = CORBA::string_dup("filename.txt");

    // Return
    return ProdAuto::Recorder::SUCCESS;
}

void SimplerouterloggerImpl::SetTimecodePort(std::string tcp)
{
    mTimecodePort = tcp;
}

void SimplerouterloggerImpl::SetRouterPort(std::string rp)
{
    mRouterPort = rp;
}


void SimplerouterloggerImpl::StartSavingFile(const std::string & filename)
{
    // Get info about current routing to write at start of file
    // and/or in database
    std::string tc = mpTcReader->Timecode();

    std::string src = mpRouter->CurrentSrc(mDestination);
    int src_index = atoi(src.substr(6).c_str());

    std::string srcstring;
    uint32_t track;
    mpSrcReader->GetSource(src_index, srcstring, track);
    std::string deststring;
    mpSrcReader->GetDestination(mDestination, deststring);

    // Open file
    mpFile = ACE_OS::fopen (filename.c_str(), ACE_TEXT ("w+"));

    if (mpFile == 0)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT ("problem with filename %p\n"), ACE_TEXT (filename.c_str()) ));
    }
    else
    {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT ("filename ok %C\n"), filename.c_str()));

        // Write info at head of file
	    ACE_OS::fprintf (mpFile, "Destination index = %d, name = %s\n", mDestination, deststring.c_str() );
	    ACE_OS::fprintf (mpFile, "%s start   source index = %3d, name = %s\n", tc.c_str(), src_index, srcstring.c_str() );
    }

    // Open database file
    if (mpCutsDatabase)
    {
        mpCutsDatabase->OpenAppend();
        if (! mLastSrc.empty())
        {
            mpCutsDatabase->AppendEntry(mLastSrc, mLastTc);
        }
        else
        {
            mpCutsDatabase->AppendEntry(srcstring, tc);
        }
    }
}

void SimplerouterloggerImpl::StopSavingFile()
{
	// Put finish time at end of file and close
    if (mpFile)
    {
        std::string tc = mpTcReader->Timecode();
	    ACE_OS::fprintf (mpFile, "%s end\n", tc.c_str() );

        if (ACE_OS::fclose (mpFile) == -1)
        {
            ACE_DEBUG((LM_DEBUG, ACE_TEXT ("problem closing file\n")));
        }
        mpFile = 0;
    }

    if (mpCutsDatabase)
    {
        mpCutsDatabase->Close();
    }
}

void SimplerouterloggerImpl::Observe(std::string msg)
{
    // Get timestamp
    std::string tc = mpTcReader->Timecode();

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C %C\n"), tc.c_str(), msg.c_str()));
    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C %C\n"), tc.c_str(), srcstring.c_str()));

    unsigned int dest = ACE_OS::atoi(msg.substr(2, 3).c_str());

    // Filter destinations
    if (dest == mDestination)
    {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT ("destination match\n")));
        std::string src = msg.substr(6);
        //ACE_DEBUG((LM_DEBUG, ACE_TEXT("%s\n"), src.c_str()));

        // convert to integer
        //std::string srcstring = mpSrcReader->SourceName(atoi(src.c_str()));
        int src_index = atoi(src.c_str());
	    //std::string srcstring = mpSrcReader->SourceName(atoi(src.c_str()));
        std::string srcstring;
        uint32_t track;
        mpSrcReader->GetSource(src_index, srcstring, track);

        // Remember last cut for when we start a new recording
        mLastSrc = srcstring;
        mLastTc = tc;

        //save if file is open
        if (mpFile != 0)
        {
    	    ACE_OS::fprintf (mpFile, "%s update  source index = %3d, name = %s\n", tc.c_str(), src_index, srcstring.c_str() );
        }

        // update database file
        if (mpCutsDatabase)
        {
            mpCutsDatabase->AppendEntry(srcstring, tc);
        }
    }
}

char * SimplerouterloggerImpl::RecordingFormat (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
    return CORBA::string_dup("Router Logger");
}

::ProdAuto::TrackList * SimplerouterloggerImpl::Tracks (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
  // Add your implementation here
    ProdAuto::TrackList_var tracks = mTracks;
    return tracks._retn();
}



