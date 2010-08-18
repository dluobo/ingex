/*
 * $Id: IngexRecorder.cpp,v 1.20 2010/08/18 10:13:13 john_f Exp $
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

#define __STDC_CONSTANT_MACROS

#include <iostream>
#include <cstdio>
#include <sstream>

#include <ace/OS_NS_unistd.h>
#include <ace/Time_Value.h>
#include <ace/Thread.h>
#include <ace/OS_NS_sys_shm.h>
#include <ace/Log_Msg.h>
#include <ace/OS_NS_time.h>
#include <ace/OS_NS_string.h>

#include "IngexRecorder.h"
#include "RecorderSettings.h"
#include "Timecode.h"
#include "DateTime.h"
#include "FileUtils.h"
#include "MaterialResolution.h"

#include <DBException.h>

#define max(a,b) (((a)>(b))?(a):(b))
#define min(a,b) (((a)<(b))?(a):(b))

const bool USE_PROJECT_SUBDIR = true;

static void clean_filename(std::string & filename)
{
    const std::string allowed_chars = "0123456789abcdefghijklmnopqrstuvwxyz_-ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char replacement_char = '_';

    size_t pos;
    while (std::string::npos != (pos = filename.find_first_not_of(allowed_chars)))
    {
        filename.replace(pos, 1, 1, replacement_char);
    }
}


/**
Constructor clears all member data.
The name parameter is used when reading config from database.
*/
IngexRecorder::IngexRecorder(IngexRecorderImpl * impl, unsigned int index)
: mpCompletionCallback(0), mpImpl(impl), mTargetDuration(0),
  mRecordingOK(true), mIndex(index), mDroppedFrames(false), mChunking(false), mChunkSize(0), mChunkAlignment(0)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::IngexRecorder()\n")));

    // Make our own copy of recorder config to avoid problems
    // if it is re-loaded during a recording.
    mRecorder.reset(impl->Recorder()->clone());

    // Clear track enables
    for (unsigned int i = 0; i < MAX_CHANNELS; ++i)
    {
        mChannelEnable.push_back(false);
    }

    // Debug RecorderConfig
    /*
    prodauto::RecorderConfig * rc = mRecorder->getConfig();
    for (std::vector<prodauto::RecorderInputConfig*>::const_iterator it = rc->recorderInputConfigs.begin(); it != rc->recorderInputConfigs.end(); ++it)
    {
        prodauto::RecorderInputConfig * ric = *it;
        for (std::vector<prodauto::RecorderInputTrackConfig*>::const_iterator it = ric->trackConfigs.begin(); it != ric->trackConfigs.end(); ++it)
        {
            prodauto::RecorderInputTrackConfig * ritc = *it;
            prodauto::SourceConfig * sc = ritc->sourceConfig;
            prodauto::SourcePackage * sp = 0;
            if (sc)
            {
                sp = sc->getSourcePackage();
                fprintf(stderr, "SourcePackage: %s (%s)\n", sp->name.c_str(), sp->isPersistent() ? "persistent" : "not persistent");
            }
        }
    }
    */
}

/**
Destructor
*/
IngexRecorder::~IngexRecorder()
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::~IngexRecorder()\n")));

    for (std::vector<ThreadParam>::iterator
        it = mThreadParams.begin(); it != mThreadParams.end(); ++it)
    {
        delete it->p_opt;
    }
}

/**
Calculate the duration of the next chunk, taking into account chunk size and alignment.
*/
framecount_t IngexRecorder::CalculateChunkDuration(const Ingex::Timecode & start_tc)
{
    framecount_t start = start_tc.FramesSinceMidnight();

    const framecount_t chunk_size = ChunkSize() * FrameRateNumerator() / FrameRateDenominator();
    framecount_t out = start + chunk_size;

    // Make new out time a round figure
    const framecount_t alignment_size = ChunkAlignment() * FrameRateNumerator() / FrameRateDenominator();
    if (chunk_size > alignment_size && alignment_size > 0)
    {
        out = (out / alignment_size) * alignment_size;
    }

    // For non-integer frame rate, may want to round the out-timecode to zero (or 2) frames
    // TODO

    return out - start;
}

/**
Setup track enables.
This should be called before IngexRecorder::CheckStartTimecode().
*/
void IngexRecorder::SetTrackEnables(const std::vector<bool> & track_enables)
{
    // Set up channel enables
    for (unsigned int i = 0; mpImpl && i < mpImpl->mTracks->length() && i < track_enables.size(); ++i)
    {
        ProdAuto::Track & track = mpImpl->mTracks->operator[](i);
        HardwareTrack hw_trk = mpImpl->mTrackMap[track.id];
        
        if (track.has_source && track_enables[i])
        {
            mChannelEnable[hw_trk.channel] = true;
        }
    }

    // Store track enables for use by recorder_functions
    mTrackEnable = track_enables;
}

/**
Prepare for a recording.  Search for target timecode and set some of the RecordOptions.
This should be called before IngexRecorder::Setup().
*/
bool IngexRecorder::CheckStartTimecode(
                Ingex::Timecode & start_timecode,
                framecount_t pre_roll)
{
    bool crash_record;
    if (start_timecode.IsNull())
    {
        crash_record = true;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::CheckStartTimecode() %C\n"), "crash-record"));
    }
    else
    {
        crash_record = false;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::CheckStartTimecode() %C\n"), start_timecode.Text()));
    }

    unsigned int n_channels = IngexShm::Instance()->Channels();

    Ingex::Timecode target_tc;

    // If crash record, search across all channels for the minimum current (lastframe) timecode
    if (crash_record)
    {
        Ingex::Timecode tc[MAX_CHANNELS];

        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Crash record:\n")));
        for (unsigned int channel_i = 0; channel_i < mChannelEnable.size(); channel_i++)
        {
            if (mChannelEnable[channel_i] && IngexShm::Instance()->SignalPresent(channel_i))
            {
                tc[channel_i] = IngexShm::Instance()->CurrentTimecode(channel_i);

                ACE_DEBUG((LM_DEBUG, ACE_TEXT("    tc[%d]=%C\n"),
                    channel_i, tc[channel_i].Text()));
            }
        }

        Ingex::Timecode min_tc;
        for (unsigned int channel_i = 0; channel_i < n_channels; channel_i++)
        {
            if (!tc[channel_i].IsNull())
            {
                if (min_tc.IsNull() || tc[channel_i].FramesSinceMidnight() < min_tc.FramesSinceMidnight())
                {
                    min_tc = tc[channel_i];
                }
            }
        }

        if (min_tc.IsNull())
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("    No channels enabled!\n")));
        }
        else
        {
            target_tc = min_tc;
        }
    }
    else
    {
        target_tc = start_timecode;
    }

    // Include pre-roll
    target_tc -= pre_roll;

    // NB. Should be keeping target_tc and pre-roll separate in case of discontinuous timecode.
    // i.e. find target_tc and then step back by pre-roll.

    // Start timecode for record threads.
    // Also used for setting stop duration (not the ideal way to do it).
    mStartTimecode = target_tc;

    // Passing back the actual start_timecode.
    start_timecode = target_tc;

    // local vars
    bool found_all_target = true;
    unsigned int search_limit;
    const int ring_length = IngexShm::Instance()->RingLength();
    if (ring_length > SEARCH_GUARD)
    {
        search_limit = ring_length - SEARCH_GUARD;
    }
    else
    {
        search_limit = 1;
    }

    // Search for desired timecode across all target sources
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Searching for timecode %C\n"), target_tc.Text()));
    for (unsigned int channel_i = 0; channel_i < n_channels; channel_i++)
    {
        if (! mChannelEnable[channel_i])
        {
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Channel %d not enabled\n"), channel_i));
            continue;
        }

        int lastframe = IngexShm::Instance()->LastFrame(channel_i);

        bool found_target = false;
        Ingex::Timecode first_tc_seen;
        Ingex::Timecode last_tc_seen;

        // Find target timecode.
        for (unsigned int i = 0; !found_target && i < search_limit; ++i)
        {
            // read timecode value
            int frame = lastframe - i;
            Ingex::Timecode tc = IngexShm::Instance()->Timecode(channel_i, frame);

            if (i == 0)
            {
                first_tc_seen = tc;
            }
            last_tc_seen = tc;

            int diff = target_tc.FramesSinceMidnight() - tc.FramesSinceMidnight();
            if (diff == 0)
            {
                mStartFrame[channel_i] = frame;
                found_target = true;

                ACE_DEBUG((LM_DEBUG, ACE_TEXT("Found channel%d lf=%6d lf-i=%8d tc=%C\n"),
                    channel_i, lastframe, lastframe - i, tc.Text()));
            }
            else if (i == 0 && diff > 0 && diff < 5)
            {
                // Target is slightly in the future.  We predict the start frame.
                mStartFrame[channel_i] = frame + diff;
                found_target = true;

                ACE_DEBUG((LM_WARNING, ACE_TEXT("Target timecode in future for channel[%d]: target=%C, most_recent=%C\n"),
                    channel_i,
                    target_tc.Text(),
                    first_tc_seen.Text()
                    ));
            }
        }

        if (! found_target)
        {
            ACE_DEBUG((LM_ERROR, "channel[%d] Target tc %C not found, buffer %C - %C\n",
                channel_i,
                target_tc.Text(),
                last_tc_seen.Text(),
                first_tc_seen.Text()
            ));

            if (IngexShm::Instance()->SignalPresent(channel_i))
            {
                found_all_target = false;
            }
            else
            {
                // As there is no signal at this input we won't
                // prevent recording from starting but will
                // simply disable this input.
                ACE_DEBUG((LM_ERROR,
                    ACE_TEXT("This channel has no signal present so will be disabled for the current recording.\n")
                    ));
                mChannelEnable[channel_i] = false;
            }
        }
    } // for channel_i


    // Return
    if (!found_all_target)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not start record - not all target timecodes found\n")));
        return false;
    }
    else
    {
        //ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::PrepareStart() returning true\n")));
        return true;
    }
}

/**
Final preparation for recording.
*/
void IngexRecorder::Setup(Ingex::Timecode start_timecode)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::Setup()\n")));

    // Get current recorder settings
    RecorderSettings * settings = RecorderSettings::Instance();

    //unsigned int n_channels = IngexShm::Instance()->Channels();
    // Tracks per channel
    // e.g. 5 (1 video, 4 audio) or 9 (1 video, 8 audio)
    // A bit dodgy as different inputs on a recorder can have
    // different numbers of tracks.
    //mTracksPerChannel = track_enables.size() / channel_enables.size();

    int first_enabled_channel = -1;
    for (unsigned int i = 0; first_enabled_channel < 0 && i < mChannelEnable.size(); ++i)
    {
        if (mChannelEnable[i])
        {
            first_enabled_channel = i;
        }
    }

    // Store project sub-directory (if using)
    std::string project_subdir;
    if (USE_PROJECT_SUBDIR)
    {
        project_subdir = mProjectName.name;
        clean_filename(project_subdir);
        settings->project_subdir = project_subdir;
    }

    // Create any needed paths.
    // NB. Paths need to be same as those used in recorder_fucntions.cpp
    for (std::vector<EncodeParams>::iterator it = settings->encodings.begin();
        it != settings->encodings.end(); ++it)
    {
        std::string path = it->dir;
        if (!project_subdir.empty())
        {
            path += PATH_SEPARATOR;
            path += project_subdir;
        }
        FileUtils::CreatePath(path);

        FileFormat::EnumType format;
        OperationalPattern::EnumType pattern;
        MaterialResolution::GetInfo(MaterialResolution::EnumType(it->resolution), format, pattern);

        // Make Creating, Failures and Metadata subdirs
        std::ostringstream creating_path;
        creating_path << path << PATH_SEPARATOR << settings->mxf_subdir_creating;
        FileUtils::CreatePath(creating_path.str());
        std::ostringstream failures_path;
        failures_path << path << PATH_SEPARATOR << settings->mxf_subdir_failures;
        FileUtils::CreatePath(failures_path.str());
        std::ostringstream metadata_path;
        metadata_path << path << PATH_SEPARATOR << settings->mxf_subdir_metadata;
        FileUtils::CreatePath(metadata_path.str());
    }

    // Set up encoding threads
    mThreadParams.clear();
    int encoding_i = 0;
    for (std::vector<EncodeParams>::const_iterator it = settings->encodings.begin();
        it != settings->encodings.end(); ++it, ++encoding_i)
    {
        if (it->source == Input::NORMAL)
        {
            for (unsigned int i = 0; i < mChannelEnable.size(); i++)
            {
                if (mChannelEnable[i])
                {
                    ThreadParam tp;
                    tp.p_rec = this;

                    tp.p_opt = new RecordOptions;
                    tp.p_opt->channel_num = i;
                    tp.p_opt->index = encoding_i;
                    
                    tp.p_opt->resolution = it->resolution;
                    tp.p_opt->bitc = it->bitc;
                    tp.p_opt->dir = it->dir;
                    if (!project_subdir.empty())
                    {
                        tp.p_opt->dir += PATH_SEPARATOR;
                        tp.p_opt->dir += settings->project_subdir;
                    }

                    mThreadParams.push_back(tp);
                }
            }
        }
        else if (it->source == Input::QUAD)
        {
            ThreadParam tp;
            tp.p_rec = this;

            tp.p_opt = new RecordOptions;
            tp.p_opt->channel_num = first_enabled_channel;
            tp.p_opt->index = encoding_i;
            tp.p_opt->quad = true;
            tp.p_opt->resolution = it->resolution;
            tp.p_opt->bitc = it->bitc;
            tp.p_opt->dir = it->dir;

            mThreadParams.push_back(tp);
        }
    }
#if 0
// For test only, add a further encoding
    {
        ThreadParam tp;
        tp.p_rec = this;

        tp.p_opt = new RecordOptions;
        tp.p_opt->channel_num = 0;
        tp.p_opt->index = 2;
        
        tp.p_opt->resolution = 8;
        tp.p_opt->file_format = MXF_FILE_FORMAT_TYPE;
        tp.p_opt->bitc = true;
        tp.p_opt->dir = "/video/mxf_offline";

        mThreadParams.push_back(tp);
    }
#endif

    // Create and store filenames based on source, target timecode and date.
    std::string date = DateTime::DateNoSeparators();
    std::string tcode = start_timecode.TextNoSeparators();

    // We also include project name to help with copying to project-based
    // directories on a file-server.

    // Note that we make sure ident does not contain any unsuitable
    // characters such as '/'

    // Set filename stems in RecordOptions.
    for (std::vector<ThreadParam>::iterator
        it = mThreadParams.begin(); it != mThreadParams.end(); ++it)
    {
        const char * src_name = (it->p_opt->quad ? QUAD_NAME : SOURCE_NAME[it->p_opt->channel_num]);
        std::ostringstream ss;
        ss << date << "_" << tcode
            << "_" << mProjectName.name
            << "_" << mpImpl->Name()
            << "_" << src_name
            << "_" << it->p_opt->index;

        std::string ident = ss.str();
        clean_filename(ident);

        it->p_opt->file_ident = ident;
    }

    // Set up some of the user comments
    mUserComments.clear();
    mUserComments.push_back(
        prodauto::UserComment(AVID_UC_SHOOT_DATE_NAME, date.c_str(), STATIC_COMMENT_POSITION, 0));
}


bool IngexRecorder::Start()
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::Start()\n")));

    // Now start all record threads, including quad
    bool result = false;
    for (std::vector<ThreadParam>::iterator
        it = mThreadParams.begin(); it != mThreadParams.end(); ++it)
    {
        // Select which encoding function to invoke
        //int channelnum = it->p_opt->channel_num;
        //ACE_THR_FUNC fn = (channelnum == QUAD_SOURCE ? start_quad_thread : start_record_thread);
        ACE_THR_FUNC fn = start_record_thread;

        // and start in new thread.
        ACE_thread_t thr;
        int err = ACE_Thread::spawn( fn, &(*it),
                      THR_NEW_LWP|THR_JOINABLE, &thr );
        const char * src_name = (it->p_opt->quad ? QUAD_NAME : SOURCE_NAME[it->p_opt->channel_num]);
        int index = it->p_opt->index;
        if (err == 0)
        {
            it->id = thr;
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Created %C record thread %d id %x\n"),
                src_name, index, thr));
            // If at least one thread starts, we return true as the thread will
            // trigger deletion of this IngexRecorder when it exits.
            result = true;
        }
        else
        {
            it->id = 0;
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to create %C record thread %d (returned %d, %C)\n"),
                src_name, index, err, strerror(errno)));
        }
    }


    // Create a thread to manage termination of recording threads
    if (result)
    {
        int err = ACE_Thread::spawn( manage_record_thread, this,
                                THR_NEW_LWP|THR_DETACHED, &mManageThreadId );
        if (err != 0)
        {
            ACE_DEBUG((LM_ERROR,
                ACE_TEXT("Failed to create termination managing thread\n")));
        }
    }

    return result;
}


bool IngexRecorder::Stop(Ingex::Timecode & stop_timecode,
                framecount_t post_roll,
                const char * description,
                const std::vector<Locator> & locs)
{
    if (stop_timecode.IsNull())
    {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::Stop(\"now\")\n")));
    }
    else
    {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::Stop(%C, %d)\n"),
            stop_timecode.Text(), post_roll));
    }

    // Store description
    if (description)
    {
        mUserComments.push_back(
            prodauto::UserComment(AVID_UC_DESCRIPTION_NAME, description, STATIC_COMMENT_POSITION, 0));
    }

    // Store locators
    for (std::vector<Locator>::const_iterator it = locs.begin(); it != locs.end(); ++it)
    {
        int64_t position = it->timecode - mStartTimecode.FramesSinceMidnight();
        mUserComments.push_back(
            prodauto::UserComment(POSITIONED_COMMENT_NAME, it->comment, position, it->colour));
    }

    // Really want to pass stop_timecode and post_roll to the record threads
    // or do some kind of "prepare_stop"
    // but for now will set an absolute duration.

    //post_roll = 0;  //tmp test

    framecount_t capture_length = 0;
    if (stop_timecode.IsNull())
    {
        // Stop timecode is "now".
        // Find longest current recorded duration and add post_roll.

        for (std::vector<ThreadParam>::iterator
            it = mThreadParams.begin(); it != mThreadParams.end(); ++it)
        {
            framecount_t frames_written = it->p_opt->FramesWritten();
            framecount_t frames_dropped = it->p_opt->FramesDropped();
            framecount_t frames_total = frames_written + frames_dropped;

            ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C frames total = %d (written = %d, dropped = %d)\n"),
                SOURCE_NAME[it->p_opt->channel_num], frames_total, frames_written, frames_dropped));

            if (capture_length < frames_total)
            {
                capture_length = frames_total;
            }
        }
        capture_length += post_roll;
    }
    else
    {
        // Calculate capture length.
        capture_length = stop_timecode.FramesSinceMidnight() - mStartTimecode.FramesSinceMidnight() + post_roll;
        if (capture_length < 0)
        {
            int frames_per_day = INT64_C(24 * 60 * 60) * stop_timecode.FrameRateNumerator() / stop_timecode.FrameRateDenominator();
            capture_length += frames_per_day;
        }
    }

    // Return the expected "out time" of the recording
    stop_timecode = mStartTimecode + capture_length;

    // guard against crazy capture lengths
    if (capture_length <= 0)
    {
        // force immediate-ish finish of recording
        // true duration will be updated from frames_written variable
        ACE_DEBUG((LM_ERROR, ACE_TEXT("recorder_stop: crazy duration of %d passed - stopping recording immediately by\n"
                "              signalling duration of 1.\n"), capture_length));
        capture_length = 1;
    }

    // Now stop record by signalling the duration in frames
    TargetDuration(capture_length);
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Target duration set to %d\n"), capture_length));


    return true;
}

Ingex::Timecode IngexRecorder::OutTime()
{
    Ingex::Timecode out;
    framecount_t d = TargetDuration();
    if (d > 0)
    {
        out = mStartTimecode;
        out += d;
    }
    return out;
}

int IngexRecorder::FrameRateNumerator()
{
    return mStartTimecode.FrameRateNumerator();
}

int IngexRecorder::FrameRateDenominator()
{
    return mStartTimecode.FrameRateDenominator();
}

/**
Return greatest current duration from the record threads.
*/
framecount_t IngexRecorder::RecordedDuration()
{
    framecount_t duration = 0;
    for (std::vector<ThreadParam>::const_iterator
        it = mThreadParams.begin(); it != mThreadParams.end(); ++it)
    {
        RecordOptions * opt = it->p_opt;
        framecount_t thread_duration = opt->FramesWritten() + opt->FramesDropped();
        if (duration < thread_duration)
        {
            duration = thread_duration;
        }
    }
    return duration;
}


bool IngexRecorder::GetProjectFromDb(const std::string & name, prodauto::ProjectName & project_name)
{
    // Note that we are assuming that Database::initialise() has already
    // been called somewhere.

    // Get ProjectName
    bool result = false;
    prodauto::Database * db = 0;
    try
    {
        db = prodauto::Database::getInstance();
        if (db)
        {
            project_name = db->loadOrCreateProjectName(name);
            result = true;
        }
    }
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
    }

    return result;
}

void IngexRecorder::GetMetadata(prodauto::ProjectName & project_name, std::vector<prodauto::UserComment> & user_comments)
{
    ACE_Guard<ACE_Thread_Mutex> guard(mMetadataMutex);

    project_name = mProjectName;
    user_comments = mUserComments;
}


