/*
 * $Id: IngexRecorderImpl.cpp,v 1.25 2011/02/18 16:31:15 john_f Exp $
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

#define __STDC_FORMAT_MACROS
#define __STDC_LIMIT_MACROS

#include <ace/OS_NS_time.h>
#include <ace/OS_NS_unistd.h>

#include <sys/statvfs.h>
#include <sstream>

#include <Database.h>
#include <DBException.h>

#include "IngexShm.h"
#include "RecorderSettings.h"
#include "IngexRecorderImpl.h"
#include "IngexRecorder.h"
#include "FileUtils.h"
#include "Timecode.h"
#include "DateTime.h"
#include "MaterialResolution.h"

IngexRecorderImpl * IngexRecorderImpl::mInstance = 0;

// To be called on completion of recording
void recording_completed(IngexRecorder * rec)
{
    bool complete;
    if (rec && rec->Chunking() && rec->ChunkSize() != 0)
    {
        // Next In time is last Out time
        Ingex::Timecode restart_tc = rec->OutTime();
        framecount_t next_duration = rec->CalculateChunkDuration(restart_tc);
        /*
        framecount_t restart = restart_tc.FramesSinceMidnight();

        // Next Out time
        const framecount_t chunk_size = rec->ChunkSize() * rec->FrameRateNumerator() / rec->FrameRateDenominator();
        framecount_t out = restart + chunk_size;
        // make new out time a round figure
        out = (out / chunk_size) * chunk_size;
        */

        // Set duration for next chunk
        rec->TargetDuration(next_duration);

        // Start new recording
        bool ok = rec->CheckStartTimecode(restart_tc, 0);
        rec->Setup(restart_tc);
        ok = ok && rec->Start();
        complete = !ok;
    }
    else
    {
        complete = true;
    }

    if (complete)
    {
        // notify observers
        IngexRecorderImpl::Instance()->NotifyCompletion(rec);

        // destroy recorder
        delete rec;
    }
}

// Implementation constructor
// Note that CopyManager mode is set here.
IngexRecorderImpl::IngexRecorderImpl (void)
: mFfmpegThreads(-1), mRecording(false), mRecordingIndex(1), mpIngexRecorder(0), mCopyManager(CopyMode::NEW)
{
    mInstance = this;
}

// Initialise the recorder
bool IngexRecorderImpl::Init()
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorderImpl::Init(\"%C\")\n"), mName.c_str()));

    bool ok = true;

    // Shared memory initialisation
    IngexShm::Instance()->RecorderName(mName);

    // Base class initialisation
    RecorderImpl::Init(mName);

    // Get hostname
    char buf[256];
    if (ACE_OS::hostname(buf, 256) == 0)
    {
        mHostname = buf;
    }

    // Test only
    //ChunkSize(300);
    //ChunkAlignment(60);

    return ok;
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
    return CORBA::string_dup("Ingex recorder");
}

namespace
{
// Used in calculation of available record time
class kbytes
{
public:
    kbytes() : kbytes_per_minute(0) {}
    uint32_t kbytes_per_minute;
    uint64_t kbytes_available;
};
}

::CORBA::ULong IngexRecorderImpl::RecordTimeAvailable (void)
{
    // Get video parameters of capture buffers
    Ingex::VideoRaster::EnumType primary_video_raster = IngexShm::Instance()->PrimaryVideoRaster();
    Ingex::VideoRaster::EnumType secondary_video_raster = IngexShm::Instance()->SecondaryVideoRaster();
    Ingex::PixelFormat::EnumType primary_pixel_format = IngexShm::Instance()->PrimaryPixelFormat();
    Ingex::PixelFormat::EnumType secondary_pixel_format = IngexShm::Instance()->SecondaryPixelFormat();
    unsigned int channels = IngexShm::Instance()->Channels();

    // Get current recorder settings
    RecorderSettings * settings = RecorderSettings::Instance();

    CORBA::ULong minutes_available = UINT32_MAX;

    // Get record resolutions and destinations
    // and calculate record time available
    std::map<unsigned long, kbytes> kbytes_by_filesystem;
    for (std::vector<EncodeParams>::iterator it = settings->encodings.begin();
        it != settings->encodings.end(); ++it)
    {
        // Resolution
        MaterialResolution::EnumType resolution = MaterialResolution::EnumType(it->resolution);

        // kbytes per minute
        uint32_t kbytes_per_minute;
        
        if (MaterialResolution::CheckVideoFormat(resolution, primary_video_raster, primary_pixel_format, kbytes_per_minute)
            || MaterialResolution::CheckVideoFormat(resolution, secondary_video_raster, secondary_pixel_format, kbytes_per_minute))
        {
            struct statvfs stats;
            if (0 == statvfs(it->dir.c_str(), &stats))
            {
                kbytes_by_filesystem[stats.f_fsid].kbytes_per_minute += (kbytes_per_minute * channels);
                kbytes_by_filesystem[stats.f_fsid].kbytes_available = uint64_t(stats.f_bavail) * stats.f_bsize / 1000;
                //fprintf(stderr, "Filesystem %s, id %lu, kbytes_per_minute %"PRIu32", kbytes_avail %"PRIu64"\n", it->dir.c_str(), stats.f_fsid, kbytes_by_filesystem[stats.f_fsid].kbytes_per_minute, kbytes_by_filesystem[stats.f_fsid].kbytes_available);
            }
        }

        for (std::map<unsigned long, kbytes>::const_iterator
            it = kbytes_by_filesystem.begin(); it != kbytes_by_filesystem.end(); ++it)
        {
            uint64_t mins = it->second.kbytes_available / it->second.kbytes_per_minute;
            if (mins < minutes_available)
            {
                minutes_available = mins;
            }
        }
        
    }

    return minutes_available;
}

::ProdAuto::MxfDuration IngexRecorderImpl::MaxPreRoll (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    ProdAuto::Rational edit_rate;
    edit_rate.numerator = IngexShm::Instance()->FrameRateNumerator();
    edit_rate.denominator = IngexShm::Instance()->FrameRateDenominator();

    ProdAuto::MxfDuration pre_roll;

    pre_roll.undefined = false;
    pre_roll.edit_rate = edit_rate;
    if (IngexShm::Instance()->RingLength() > (SEARCH_GUARD + 1))
    {
        pre_roll.samples = IngexShm::Instance()->RingLength() - (SEARCH_GUARD + 1);
    }
    else
    {
        pre_roll.samples = 0;
    }

    return pre_roll;
}

::ProdAuto::MxfDuration IngexRecorderImpl::MaxPostRoll (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    ProdAuto::Rational edit_rate;
    edit_rate.numerator = IngexShm::Instance()->FrameRateNumerator();
    edit_rate.denominator = IngexShm::Instance()->FrameRateDenominator();

    ProdAuto::MxfDuration post_roll;

    post_roll.undefined = true; // no limit to post-roll
    post_roll.edit_rate = edit_rate;
    post_roll.samples = 0;

    return post_roll;
}

::ProdAuto::Rational IngexRecorderImpl::EditRate (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    ProdAuto::Rational edit_rate;
    edit_rate.numerator = IngexShm::Instance()->FrameRateNumerator();
    edit_rate.denominator = IngexShm::Instance()->FrameRateDenominator();

    return edit_rate;
}

::ProdAuto::TrackList * IngexRecorderImpl::Tracks (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    // The IngexGUI controller invokes the Tracks operation
    // when it connects to a recorder.

    // Get current capture buffer and database settings
    this->UpdateConfig();

    return RecorderImpl::Tracks();
}

::ProdAuto::TrackStatusList * IngexRecorderImpl::TracksStatus (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    // Update member with current timecode and status
#if 1
    // Alternative to method below
    for (unsigned int i = 0; i < mTracks->length(); ++i)
    {
        ProdAuto::Track & track = mTracks->operator[](i);
        HardwareTrack hw_trk = mTrackMap[track.id];

        bool signal_present = IngexShm::Instance()->SignalPresent(hw_trk.channel);
        Ingex::Timecode timecode = IngexShm::Instance()->CurrentTimecode(hw_trk.channel);

        // We can rely on mTracksStatus->length() == mTracks->length()
        ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);

        ts.signal_present = signal_present;
        
        ts.timecode.undefined = 0;
        ts.timecode.samples = timecode.FramesSinceMidnight();
        ts.timecode.edit_rate.numerator = timecode.FrameRateNumerator();
        ts.timecode.edit_rate.denominator = timecode.FrameRateDenominator();
        ts.timecode.drop_frame = timecode.DropFrame();

        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Track %d: edit_rate %d/%d\n"), i, ts.timecode.edit_rate.numerator, ts.timecode.edit_rate.denominator));

        // Update filenames
        if (mpIngexRecorder)
        {
            ts.filename = CORBA::string_dup(mpIngexRecorder->mFileNames[i].c_str());
        }
        else
        {
            ts.filename =  CORBA::string_dup("");
        }
    }
#else
    // Below we are relying on hardware order to get same order of tracks in
    // TracksStatus() as we have in Tracks().  It would be preferable to use
    // the order of Tracks directly to allow freedom to present Tracks in a
    // different order without need to change TracksStatus code.
    for (unsigned int channel_i = 0; channel_i < IngexShm::Instance()->Channels(); ++channel_i)
    {
        bool signal = IngexShm::Instance()->SignalPresent(channel_i);
        Ingex::Timecode timecode = IngexShm::Instance()->CurrentTimecode(channel_i);

        // We know each input has mMaxTracksPerInput, see comments
        // in RecorderImpl::UpdateSources()
        for (unsigned int j = 0; j < mMaxTracksPerInput; ++j)
        {
            CORBA::ULong track_i = mMaxTracksPerInput * channel_i + j;
            if (track_i < mTracksStatus->length())
            {
                ProdAuto::TrackStatus & ts = mTracksStatus->operator[](track_i);

                ts.signal_present = signal;

                ts.timecode.undefined = 0;
                ts.timecode.samples = timecode;
                ts.timecode.edit_rate = edit_rate;
            }
        }
    }
#endif

    // Make a copy to return
    ProdAuto::TrackStatusList_var tracks_status = mTracksStatus;
    return tracks_status._retn();
}

::ProdAuto::Recorder::ReturnCode IngexRecorderImpl::Start (
    ::ProdAuto::MxfTimecode & start_timecode,
    const ::ProdAuto::MxfDuration & pre_roll,
    const ::CORBA::BooleanSeq & rec_enable,
    const char * project,
    ::CORBA::Boolean test_only
  )
  throw (
    ::CORBA::SystemException
  )
{
    framecount_t pre = (pre_roll.undefined ? 0 : pre_roll.samples);
    Ingex::Timecode start_tc; // initialises to null
    if (!start_timecode.undefined)
    {
        start_tc = Ingex::Timecode(start_timecode.samples, start_timecode.edit_rate.numerator, start_timecode.edit_rate.denominator, start_timecode.drop_frame);
    }

    ACE_DEBUG((LM_INFO, ACE_TEXT("%C IngexRecorderImpl::Start(), tc %C, pre-roll %d\n"),
        DateTime::Timecode().c_str(), start_tc.IsNull() ? "immediate" : start_tc.Text(), pre));

    bool ok = true;

    // Enforce start-stop-start sequence
    if (mRecording)
    {
        // Immediate stop
        DoStop(Ingex::Timecode(), 0);
    }

    // Clear previous monitoring data for enabled channels
    IngexShm::Instance()->InfoResetChannels();

    // Make sure we are up-to-date with source config and
    // settings from database.
    // Also happens when controller requests tracks.
    // By updating again here, the controller may get
    // out of sync if track config changes.  However, the
    // alternative (not updating here) would mean you rely
    // on controller disconnecting and reconnecting to
    // force an update.
    // This can cause a delay to start recording because of
    // database access.
#if 0
    UpdateFromDatabase();
#endif

    // Latest parameters from shared memory
    const unsigned int max_inputs = IngexShm::Instance()->Channels();
    const unsigned int max_tracks_per_input = 1 + IngexShm::Instance()->AudioTracksPerChannel();

    // Translate enables to per-track and per-channel.
    std::vector<bool> channel_enables;
    std::vector<bool> track_enables;
#if 0
    // Without assuming mTracks in hardware order
    // - not yet finished

    // Start with channel enables false.
    for (unsigned int channel_i = 0; channel_i < max_inputs; ++channel_i)
    {
        channel_enables.push_back(false);
    }
    for (unsigned int i = 0; i < mTracks->length(); ++i)
    {
        ProdAuto::Track & track = mTracks->operator[](i);
        HardwareTrack hw_trk = mTrackMap[track.id];
        
        channel_enables[hw_trk.channel] = true;

        if (track.has_source && rec_enable[i])
        {
        }
    }
#else
    // Older method
    for (unsigned int channel_i = 0; channel_i < max_inputs; ++channel_i)
    {
        // Start with channel enables false.
        bool ce = false;

        // Go through tracks
        for (unsigned int j = 0; j < max_tracks_per_input; ++j)
        {
            CORBA::ULong track_i = channel_i * max_tracks_per_input + j;

            bool te = false;
            if (track_i < mTracks->length() && mTracks[track_i].has_source)
            {
                te = rec_enable[track_i];
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("Track %2d %C\n"),
                    track_i, te ? "enabled" : "not enabled"));
            }
            else
            {
                // If no source connected, track is left disabled.
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("Track %2d not connected\n"),
                    track_i));
            }
            track_enables.push_back(te);

            if (te)
            {
                // If one track enabled, enable channel.
                ce = true;
            }
        }

        // Set channel enable
        channel_enables.push_back(ce);
    }
#endif
    // Check that project name is loaded.
    // Only load if necessary to avoid delay of 
    // database access.
    if (mProjectName.name != project)
    {
        prodauto::Database * db = 0;
        try
        {
            db = prodauto::Database::getInstance();
            if (db)
            {
                mProjectName = db->loadOrCreateProjectName(project);
            }
        }
        catch (const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
        }
    }

    // Get index for this recording
    unsigned int recording_index = mRecordingIndex++;

    // Make a new IngexRecorder to manage this recording.
    mpIngexRecorder = new IngexRecorder(this, recording_index);

    // Set-up callback for completion of recording
    mpIngexRecorder->SetCompletionCallback(&recording_completed);

    // Setup track enables
    mpIngexRecorder->SetTrackEnables(track_enables);

    // Check for start timecode.
    // This may modify the channel_enable array.
    ok = ok && mpIngexRecorder->CheckStartTimecode(
        start_tc,
        pre_roll.undefined ? 0 : pre_roll.samples);

    // Set return value for actual start timecode
    start_timecode.undefined = false;
    start_timecode.samples = start_tc.FramesSinceMidnight();

    // Setup IngexRecorder
    mpIngexRecorder->ProjectName(mProjectName);
    mpIngexRecorder->Setup(start_tc);

    // Recorder-controlled chunking
    if (ChunkSize() > 0)
    {
        mpIngexRecorder->Chunking(true);
        mpIngexRecorder->ChunkSize(ChunkSize());
        mpIngexRecorder->ChunkAlignment(ChunkAlignment());

        framecount_t chunk_duration = mpIngexRecorder->CalculateChunkDuration(start_tc);

        mpIngexRecorder->TargetDuration(chunk_duration);
    }

    // Setup copy parameters
    this->InitCopying();

    // Control copy process to reduce disc activity.
    this->StopCopying(recording_index);

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
            mStatusDist.SendStatus("event", "recording starting");

            for (CORBA::ULong i = 0; i < mTracksStatus->length(); ++i)
            {
                ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);
                ts.rec = rec_enable[i];
                ts.rec_error = 0;
            }
        }
    }

    if (!ok || test_only)
    {
        delete mpIngexRecorder;
        mpIngexRecorder = 0;
        StartCopying(recording_index);
    }

    // Debug to measure delay in responding to Start command
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorderImpl::Start() Returning %C at time          %C\n"),
        (ok ? "SUCCESS" : "FAILURE"),
        DateTime::Timecode().c_str()));

    // Return
    return (ok ? ProdAuto::Recorder::SUCCESS : ProdAuto::Recorder::FAILURE);
}

::ProdAuto::MxfDuration IngexRecorderImpl::RecordedDuration (
    void)
{
    ProdAuto::MxfDuration duration;
    duration.edit_rate.numerator = 0;
    duration.edit_rate.denominator = 0;
    duration.samples = 0;
    duration.undefined = 1;

    if (mpIngexRecorder)
    {
        duration.edit_rate.numerator = mpIngexRecorder->FrameRateNumerator();
        duration.edit_rate.denominator = mpIngexRecorder->FrameRateDenominator();
        duration.samples = mpIngexRecorder->RecordedDuration();
        duration.undefined = 0;
    }

    return duration;
}

::ProdAuto::Recorder::ReturnCode IngexRecorderImpl::Stop (
    ::ProdAuto::MxfTimecode & mxf_stop_timecode,
    const ::ProdAuto::MxfDuration & mxf_post_roll,
    const char * description,
    const ::ProdAuto::LocatorSeq & locators,
    ::CORBA::StringSeq_out files
  )
  throw (
    ::CORBA::SystemException
  )
{
    /*
    ACE_DEBUG((LM_INFO, ACE_TEXT("IngexRecorderImpl::Stop(), samples %d, fps_num %d, fps_den %d\n"),
        mxf_stop_timecode.samples,
        mxf_stop_timecode.edit_rate.numerator,  mxf_stop_timecode.edit_rate.denominator));
    */

    framecount_t post_roll = (mxf_post_roll.undefined ? 0 : mxf_post_roll.samples);
    Ingex::Timecode stop_tc; // initialised to null
    if (!mxf_stop_timecode.undefined)
    {
        stop_tc = Ingex::Timecode(mxf_stop_timecode.samples, mxf_stop_timecode.edit_rate.numerator,  mxf_stop_timecode.edit_rate.denominator, mxf_stop_timecode.drop_frame);
    }

    ACE_DEBUG((LM_INFO, ACE_TEXT("%C IngexRecorderImpl::Stop(), tc %C, post-roll %d\n"),
        DateTime::Timecode().c_str(), stop_tc.IsNull() ? "immediate" : stop_tc.Text(), post_roll));

    // Create out parameter
    files = new ::CORBA::StringSeq;
    files->length(mTracks->length());

    // Translate the locators
    std::vector<Locator> locs;
    for (CORBA::ULong i = 0; i < locators.length(); ++i)
    {
        Locator lo;
        lo.comment = locators[i].comment;
        lo.colour = TranslateLocatorColour(locators[i].colour);
        lo.timecode = locators[i].timecode.samples;
        //ACE_DEBUG((LM_DEBUG, ACE_TEXT("Locator \"%C\" position %d colour %d\n"),
        //    lo.comment.c_str(), lo.timecode, lo.colour));
        locs.push_back(lo);
    }

    // Tell recorder when to stop.
    if (mpIngexRecorder)
    {
        mpIngexRecorder->Chunking(false);
        mpIngexRecorder->Stop(stop_tc, post_roll,
            description,
            locs);

        // Return the expected "out time"
        mxf_stop_timecode.undefined = false;
        //mxf_stop_timecode.edit_rate = mEditRate;
        mxf_stop_timecode.samples = stop_tc.FramesSinceMidnight();

        // Return the filenames
        for (CORBA::ULong track_i = 0; track_i < mTracks->length(); ++track_i)
        {
            std::string name = mpIngexRecorder->mFileNames[track_i];
            if (!name.empty())
            {
                name = "/" + mHostname + name;
            }
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Track %d \"%C\"\n"), track_i, name.c_str()));
            files->operator[](track_i) = name.c_str();
        }
        mpIngexRecorder = 0;  // It will be deleted when it signals completion
    }
    else
    {
        // Not recording - return undefined "out time"
        mxf_stop_timecode.undefined = true;
    }

    mRecording = false;

    // Debug to measure delay in responding to Stop command
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorderImpl::Stop() Returning %C at time           %C\n"),
        "SUCCESS",
        DateTime::Timecode().c_str()));

    // Return
    return ProdAuto::Recorder::SUCCESS;
}

/**
Simplified Stop without returning filenames etc.
*/
void IngexRecorderImpl::DoStop(Ingex::Timecode timecode, framecount_t post_roll)
{
    // Tell recorder when to stop.
    std::vector<Locator> locs;
    if (mpIngexRecorder)
    {
        mpIngexRecorder->Chunking(false);
        mpIngexRecorder->Stop(timecode, post_roll, "recording halted", locs);
    }
    mpIngexRecorder = 0;  // It will be deleted when it signals completion

    mRecording = false;
}

/**
Means of externally forcing a re-reading of config from database.
*/
void IngexRecorderImpl::UpdateConfig (
    
  )
  throw (
    ::CORBA::SystemException
  )
{
    const unsigned int max_inputs = IngexShm::Instance()->Channels();
    const unsigned int max_tracks_per_input = 1 + IngexShm::Instance()->AudioTracksPerChannel();

    prodauto::Rational edit_rate;
    edit_rate.numerator = IngexShm::Instance()->FrameRateNumerator();
    edit_rate.denominator = IngexShm::Instance()->FrameRateDenominator();

    RecorderImpl::EditRate(edit_rate);
    RecorderImpl::DropFrame(IngexShm::Instance()->CurrentTimecode(0).DropFrame());

    RecorderImpl::UpdateFromDatabase(max_inputs, max_tracks_per_input);
}

void IngexRecorderImpl::NotifyCompletion(IngexRecorder * rec)
{
    //ACE_DEBUG((LM_INFO, "IngexRecorderImpl::NotifyCompletion()\n"));

    if (!rec->mRecordingOK)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Recording failed!\n")));
        mStatusDist.SendStatus("error", "recording failed");
    }
    else if (rec->DroppedFrames())
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Recording dropped frames!\n")));
        mStatusDist.SendStatus("error", "recording dropped frames");
    }
    else
    {
        ACE_DEBUG((LM_INFO, ACE_TEXT("Recording completed ok.\n")));
        mStatusDist.SendStatus("event", "recording completed ok");
    }
    
    // rec is about to get deleted so make sure mpIngexRecorder isn't
    // still pointing to it.  (This can happen if the recording
    // stops itself.)
    if (rec == mpIngexRecorder)
    {
        mpIngexRecorder = 0;
    }

    // Update status to "not recording".
    // But only if we haven't started another recording in the meantime
    if (0 == mpIngexRecorder)
    {
        for (CORBA::ULong i = 0; i < mTracksStatus->length(); ++i)
        {
            ProdAuto::TrackStatus & ts = mTracksStatus->operator[](i);
            ts.rec = 0;
            ts.rec_error = 0;
        }
    }

    this->StartCopying(rec->mIndex);
}

void IngexRecorderImpl::InitCopying()
{
    //mCopyManager.Command(RecorderSettings::Instance()->copy_command);
    mCopyManager.ClearSrcDest();
    RecorderSettings * settings = RecorderSettings::Instance();

    for (std::vector<EncodeParams>::const_iterator it = settings->encodings.begin(); it != settings->encodings.end(); ++it)
    {
        std::string src = it->dir;
        std::string dest = it->copy_dest;
        if (!settings->project_subdir.empty())
        {
            src += PATH_SEPARATOR;
            src += settings->project_subdir;
            dest += PATH_SEPARATOR;
            dest += settings->project_subdir;
        }
        mCopyManager.AddSrcDest(src, dest, it->copy_priority);
    }
}

void IngexRecorderImpl::StartCopying(unsigned int index)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorderImpl::StartCopying(%d)\n"), index));
    mCopyManager.StartCopying(index);
}

void IngexRecorderImpl::StopCopying(unsigned int index)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorderImpl::StopCopying(%d)\n"), index));
    mCopyManager.StopCopying(index);
}

void IngexRecorderImpl::UpdateShmSourceNames()
{
    const unsigned int max_inputs = IngexShm::Instance()->Channels();
    try
    {
        if (mRecorder.get())
        {
            const unsigned int n_inputs = ACE_MIN((unsigned int)mRecorder->recorderInputConfigs.size(), max_inputs);

            for (unsigned int i = 0; i < n_inputs; ++i)
            {
                prodauto::RecorderInputConfig * ric = mRecorder->getInputConfig(i + 1);

                prodauto::RecorderInputTrackConfig * ritc = 0;
                if (ric)
                {
                    ritc = ric->trackConfigs[0]; // video track
                }
                prodauto::SourceConfig * sc = 0;
                if (ritc)
                {
                    sc = ritc->sourceConfig;
                }
                if (sc)
                {
                    // Store name in shared memory
                    //IngexShm::Instance()->SourceName(i, sc->getSourcePackage()->name);
                    IngexShm::Instance()->SourceName(i, sc->name);
                }
            } // for
        }
    } // try
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
    }
}

int IngexRecorderImpl::TranslateLocatorColour(ProdAuto::LocatorColour::EnumType e)
{
    // database values are defined in DatabaseEnums.h

    int i;
    switch (e)
    {
    case ProdAuto::LocatorColour::WHITE:
        i = USER_COMMENT_WHITE_COLOUR;
        break;
    case ProdAuto::LocatorColour::RED:
        i = USER_COMMENT_RED_COLOUR;
        break;
    case ProdAuto::LocatorColour::YELLOW:
        i = USER_COMMENT_YELLOW_COLOUR;
        break;
    case ProdAuto::LocatorColour::GREEN:
        i = USER_COMMENT_GREEN_COLOUR;
        break;
    case ProdAuto::LocatorColour::CYAN:
        i = USER_COMMENT_CYAN_COLOUR;
        break;
    case ProdAuto::LocatorColour::BLUE:
        i = USER_COMMENT_BLUE_COLOUR;
        break;
    case ProdAuto::LocatorColour::MAGENTA:
        i = USER_COMMENT_MAGENTA_COLOUR;
        break;
    case ProdAuto::LocatorColour::BLACK:
        i = USER_COMMENT_BLACK_COLOUR;
        break;
    default:
        i = USER_COMMENT_RED_COLOUR;
        break;
    }

    return i;
}

/**
Get various global parameters in a thread-safe way.
*/
void IngexRecorderImpl::GetGlobals(int & ffmpeg_threads)
{
    ACE_Guard<ACE_Thread_Mutex> guard(mGlobalsMutex);

    ffmpeg_threads = mFfmpegThreads;
}

