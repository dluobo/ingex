/*
 * $Id: IngexRecorder.cpp,v 1.1 2006/12/20 12:28:24 john_f Exp $
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


#include <iostream>
#include <cstdio>
#include <sstream>

#include <ace/OS_NS_unistd.h>
#include <ace/Time_Value.h>
#include <ace/Thread.h>
#include <ace/OS_NS_sys_shm.h>
#include <ace/Log_Msg.h>
#include <ace/OS_NS_time.h>

#include "IngexRecorder.h"
#include "RecorderSettings.h"
#include "Timecode.h"
#include "DateTime.h"
#include "FileUtils.h"

#include <DBException.h>

#define max(a,b) (((a)>(b))?(a):(b))
#define min(a,b) (((a)<(b))?(a):(b))

// Tmp bodges for debug output
#define logFF printf
#define logTF printf
char * framesToStr(int tc, char *s)
{
    int frames = tc % 25;
    int hours = (int)(tc / (60 * 60 * 25));
    int minutes = (int)((tc - (hours * 60 * 60 * 25)) / (60 * 25));
    int seconds = (int)((tc - (hours * 60 * 60 * 25) - (minutes * 60 * 25)) / 25);

    sprintf(s, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
    return s;
}


/**
Constructor clears all member data.
The name parameter is used when reading config from database.
*/
IngexRecorder::IngexRecorder(const std::string & name)
: mpCompletionCallback(0)
{
    mName = name;
    std::cerr << "IngexRecorder::IngexRecorder()" << std::endl;
}

/**
Destructor
*/
IngexRecorder::~IngexRecorder()
{
    std::cerr << "IngexRecorder::~IngexRecorder()" << std::endl;
}

/**
Prepare for a recording.  Search for target timecode and set some of the RecordOptions
*/
bool IngexRecorder::PrepareStart(
                framecount_t start_timecode,
                framecount_t pre_roll,
                bool card_enable[],
                bool trk_enable[],
                bool crash_record,
                const char * tag)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::PrepareStart()\n")));

    // Get current recorder settings from database
    RecorderSettings * settings = RecorderSettings::Instance();
    settings->Update(mName);
    IngexShm::Instance()->TcMode(settings->tc_mode);

    // Get MXF info from database.
    // Record threads will access this later.
    GetDbInfo();

    // Store paths for recorded files
    //strcpy(mVideoPath, video_path);
    //strcpy(mDvdPath, dvd_path);

    unsigned int n_cards = IngexShm::Instance()->Cards();

    // Set enable flags within RecordOptions
    for (unsigned int i = 0; i < n_cards; i++)
    {
        record_opt[i].enabled = card_enable[i];
        for (int j = 0; j < 5; ++j)
        {
            record_opt[i].track_enable[j] = trk_enable[5 * i + j];
        }
    }

    framecount_t target_tc;

    // If crash record, search across all card for the minimum current (lastframe) timecode
    if (crash_record)
    {
        int tc[MAX_CARDS];

        logTF("Crash record:\n");
        for (unsigned int card_i = 0; card_i < n_cards; card_i++)
        {
            if (! record_opt[card_i].enabled)
            {
                continue;
            }

            //int lastframe = LastFrame(card_i);
            //tc[card_i] = *(int*)(ring[card_i] + elementsize * (lastframe % ringlen) + tc_offset);
            tc[card_i] = IngexShm::Instance()->CurrentTimecode(card_i);

            char tmp[32];
            logTF("    tc[%d]=%d  %s\n", card_i, tc[card_i], framesToStr(tc[card_i], tmp));
        }

        int max_tc = 0;
        int min_tc = INT_MAX;
        for (unsigned int card_i = 0; card_i < n_cards; card_i++)
        {
            if (! record_opt[card_i].enabled)
            {
                continue;
            }
            max_tc = max(max_tc, tc[card_i]);
            min_tc = min(min_tc, tc[card_i]);
        }

#if 0
        // Old strategy
        int max_diff = max_tc - min_tc;

        // NB. could reduce sleep time by amount of pre-roll
        logTF("    crash record max diff=%d sleeping for %d frames...\n", max_diff, max_diff+2);
        ACE_OS::sleep(ACE_Time_Value(0, (max_diff+2) * 40 * 1000)); // sleep max_diff number of frames

        // Use lowest enabled card's timecode as the target.
        for (int card_i = n_cards - 1; card_i >= 0; card_i--)
        {
            if (record_opt[card_i].enabled)
            {
                target_tc = tc[card_i];
            }
        }
#else
        // Simpler strategy
        target_tc = min_tc;
	//target_tc = tc[1]; // just for testing
#endif

    }
    else
    {
        target_tc = start_timecode;
    }

    // Include pre-roll
    target_tc -= pre_roll;
    // NB. Should be keeping target_tc and pre-roll separate in case of discontinuous timecode.
    // i.e. ind target_tc and then step back by pre-roll.

    // For setting stop duration.  Once again, not the ideal way to do it.
    mStartTimecode = target_tc;

    // local vars
    bool found_all_target = true;
    char tcstr[32];

    // Search for desired timecode across all target sources
    logTF("Searching for timecode=%d %s\n", target_tc, framesToStr(target_tc, tcstr));
    for (unsigned int card_i = 0; card_i < n_cards; card_i++)
    {
        if (! record_opt[card_i].enabled)
        {
            continue;
        }

        // Set further record options
        record_opt[card_i].start_tc = target_tc;
        record_opt[card_i].TargetDuration(0); // Keep recording until a stop duration is set.

        int lastframe = IngexShm::Instance()->LastFrame(card_i);

        int last_tc = -1;
        int i = 0;
        int search_limit = 1;
        if (IngexShm::Instance()->RingLength() > SEARCH_GUARD)
        {
            search_limit = IngexShm::Instance()->RingLength() - SEARCH_GUARD;
        }
        bool found_target = false;
        int first_tc_seen = -1, last_tc_seen = -1;

        // find target timecode
        while (1)
        {
            // read timecode value
            //int tc = *(int*)(ring[card_i] + elementsize * ((lastframe-i) % ringlen) + tc_offset);
            int tc = IngexShm::Instance()->Timecode(card_i, lastframe - i);
            if (first_tc_seen == -1)
            {
                first_tc_seen = tc;
            }

            if (tc == target_tc)
            {
                record_opt[card_i].start_frame = lastframe - i;
                found_target = true;

                logTF("Found card%d lf[%d]=%6d lf-i=%8d tc=%d %s\n",
                    card_i, card_i, lastframe, lastframe - i, tc, framesToStr(tc, tcstr));
                break;
            }
            last_tc = tc;
            i++;
            if (i > search_limit)
            {
                logTF("Target tc not found for card[%d].  Searched %d frames, %.2f sec\n",
                            card_i, search_limit, search_limit / 25.0);
                char tcstr2[32];
                logTF("    first_tc_seen=%d %s  last_tc_seen=%d %s\n",
                            first_tc_seen, framesToStr(first_tc_seen, tcstr),
                            last_tc_seen, framesToStr(last_tc_seen, tcstr2));
                break;
            }
            last_tc_seen = tc;
        }

        if (! found_target)
        {
            found_all_target = false;
        }
    }

    // Set RecordOptions for quad-split.
    if (settings->quad_mpeg2 && record_opt[0].enabled)
    {
        record_opt[QUAD_SOURCE].enabled = true;
        record_opt[QUAD_SOURCE].TargetDuration(0);
        record_opt[QUAD_SOURCE].start_tc = target_tc;
        // Start frame copied from card 0
        record_opt[QUAD_SOURCE].start_frame = record_opt[0].start_frame;
        // Set frame offsets into sources 1, 2 and 3
        record_opt[QUAD_SOURCE].quad1_frame_offset =
                record_opt[1].start_frame - record_opt[0].start_frame;
        record_opt[QUAD_SOURCE].quad2_frame_offset =
                record_opt[2].start_frame - record_opt[0].start_frame;
        record_opt[QUAD_SOURCE].quad3_frame_offset =
                record_opt[3].start_frame - record_opt[0].start_frame;
    }
    else
    {
        record_opt[QUAD_SOURCE].enabled = false;
    }

    // Create and store filenames
    ::Timecode tc;
    tc = target_tc;
    const char * date = DateTime::DateNoSeparators();
    const char * tcode = tc.TextNoSeparators();

    // Create any needed paths.
    // NB. Paths need to be same as those used in recorder_fucntions.cpp
    if (settings->raw)
    {
        FileUtils::CreatePath(settings->raw_dir);
    }
    if (settings->mxf)
    {
        std::ostringstream creating_path;
        creating_path << settings->mxf_dir << settings->mxf_subdir_creating;
        FileUtils::CreatePath(creating_path.str());
        std::ostringstream failures_path;
        failures_path << settings->mxf_dir << settings->mxf_subdir_failures;
        FileUtils::CreatePath(failures_path.str());
    }
    if (settings->quad_mpeg2 || settings->browse_audio)
    {
        FileUtils::CreatePath(settings->browse_dir);
    }

    for (int i = 0; i < MAX_RECORD; i++)
    {
        RecordOptions & opt = record_opt[i];
        if(opt.enabled)
        {
            std::ostringstream ident;
            ident << date << "_" << tcode << "_" << SOURCE_NAME[i];
            opt.file_ident = ident.str();

            opt.description = tag;
        }
    }

    delete [] date; // Caller needs to deallocate the storage


    if (!found_all_target)
    {
        logTF("Could not start record - not all target timecodes found\n");
        return false;
    }

    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::PrepareStart() returning true\n")));
    return true;
}

bool IngexRecorder::Start()
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::Start()\n")));

    // Now start all record threads, including quad
    for (int card_i = 0; card_i < MAX_RECORD; card_i++)
    {
        if (! record_opt[card_i].enabled)
        {
            continue;
        }

        invoke_arg[card_i].invoke_card = card_i;
        invoke_arg[card_i].p_rec = this;

        ACE_THR_FUNC fn = (card_i == QUAD_SOURCE ? start_quad_thread : start_record_thread);
        int err = ACE_Thread::spawn( fn, &invoke_arg[card_i],
                      THR_NEW_LWP|THR_JOINABLE, &record_opt[card_i].thread_id );
        if (err == 0)
        {
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Created %C record thread id %x\n"),
                SOURCE_NAME[card_i], record_opt[card_i].thread_id));
        }
        else
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to create %C record thread: %C\n"),
                SOURCE_NAME[card_i], ACE_OS::strerror(err)));
        }
    }

    return true;
}

bool IngexRecorder::Stop( framecount_t stop_timecode, framecount_t post_roll )
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("IngexRecorder::Stop(%d, %d)\n"), stop_timecode, post_roll));

    // Really want to pass stop_timecode and post_roll to the record threads
    // or do some kind of "prepare_stop"
    // but for now will set an absolute duration.

    //post_roll = 0;  //tmp test

    framecount_t capture_length = 0;
    if(stop_timecode < 0)
    {
        // Stop timecode is "now".
        // Find longest current recorded duration and add post_roll.

        for (int i = 0; i < MAX_RECORD; ++i)
        {
	    framecount_t frames_written = record_opt[i].FramesWritten();
	    framecount_t frames_dropped = record_opt[i].FramesDropped();
	    framecount_t frames_total;
            if (record_opt[i].enabled)
            {
                frames_total = frames_written + frames_dropped;
            }
	    else
	    {
	        frames_total = 0;
	    }
	    ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C frames total = %d (written = %d, dropped = %d)\n"),
		SOURCE_NAME[i], frames_total, frames_written, frames_dropped));

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
        capture_length = stop_timecode - mStartTimecode + post_roll;
    }

    // guard against crazy capture lengths
    if (capture_length <= 0)
    {
        // force immediate-ish finish of recording
        // true duration will be updated from frames_written variable
        logTF("recorder_stop: crazy duration of %d passed - stopping recording immediately by\n"
                "              signalling duration of 1 - see metadata.txt for true durations\n", capture_length);
        capture_length = 1;
    }

    // Now stop record by signalling the duration in frames
    for (int i = 0; i < MAX_RECORD; ++i)
    {
        if (record_opt[i].enabled)
        {
	    record_opt[i].TargetDuration(capture_length);
	    ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C: Target duration set to %d\n"),
	        SOURCE_NAME[i], capture_length));
        }

    }


    // Create a new thread to manage termination of running threads
    int err;
    err = ACE_Thread::spawn( manage_record_thread, this, THR_NEW_LWP|THR_DETACHED, &mManageThreadId );
    //err = pthread_create( &manage_thread, NULL, manage_record_thread, this);
    if (err != 0)
    {
        logTF("Failed to create termination managing thread: %s\n", strerror(err));
        return 0;
    }

    //return m_capture_length;


    return true;
}

/**
Write a metadata file alongside the media files.
This is maintained for compatibility with an earlier version of the recorder
but is not ideal as this file can become inconsistent with the "true" metadata.
Can perhaps be used to determine when the recorded files are complete.
*/
bool IngexRecorder::WriteMetadataFile(const char * meta_name)
{
    FILE *fp_meta = NULL;
    char tmp_meta_name[FILENAME_MAX];
    strcpy(tmp_meta_name, meta_name);
    strcat(tmp_meta_name, ".tmp");
    char tmpstr[64];

    if ((fp_meta = fopen(tmp_meta_name, "wb")) == NULL)
    {
        logTF("Could not open for write: %s\n", tmp_meta_name);
        perror("fopen");
        return false;
    }
    fprintf(fp_meta, "path=%s\n", meta_name);
    //fprintf(fp_meta, "a_tape=");
    //for (int i = 0; i < mCards; i++)
    //{
    //  fprintf(fp_meta, "%s%s", record_opt[i].tapename.c_str(), i == MAX_CARDS - 1 ? "\n" : ",");
    //}
    fprintf(fp_meta, "enabled=");
    for (int i = 0; i < MAX_RECORD; i++)
    {
        fprintf(fp_meta, "%d%s", record_opt[i].enabled, i == MAX_RECORD - 1 ? "\n" : ",");
    }
    fprintf(fp_meta, "start_tc=%d\n", record_opt[0].start_tc);
    fprintf(fp_meta, "start_tc_str=%s\n", framesToStr(record_opt[0].start_tc, tmpstr));
    fprintf(fp_meta, "capture_length=%d\n", record_opt[0].FramesWritten());
#if 0
    fprintf(fp_meta, "a_start_tc=");
    for (int i = 0; i < mCards + 1; i++)
    {
        fprintf(fp_meta, "%d%s", record_opt[i].start_tc, i == mCards ? "\n" : " ");
    }
    fprintf(fp_meta, "a_start_tc_str=");
    for (int i = 0; i < mCards + 1; i++)
    {
        fprintf(fp_meta, "%s%s", framesToStr(record_opt[i].start_tc, tmpstr), i == mCards ? "\n" : " ");
    }
    fprintf(fp_meta, "a_duration=");
    for (int i = 0; i < mCards + 1; i++)
    {
        fprintf(fp_meta, "%d%s", record_opt[i].FramesWritten(), i == mCards ? "\n" : " ");
    }
    fprintf(fp_meta, "description=%s\n", ""); // not necessarily known at this time
#endif
    fclose(fp_meta);

    if (rename(tmp_meta_name, meta_name) != 0)
    {
        logTF( "Could not rename %s to %s\n", tmp_meta_name, meta_name);
        perror("rename");
        return false;
    }

    return true;
}

bool IngexRecorder::GetDbInfo()
{
    // Note that we are assuming that Database::initialise() has already been called
    // somewhere.
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

    // Get Recorder.
    prodauto::Recorder * rec = 0;
    if(db_ok)
    {
        try
        {
            rec = db->loadRecorder(mName);
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Loaded Recorder \"%C\"\n"), rec->name.c_str()));
        }
        catch(const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
            db_ok = false;
        }
    }
    mRecorder.reset(rec);

    // Get SourceSession for today, creating if necessary.
    prodauto::SourceSession * source_session = 0;
    if(db_ok)
    {
        time_t my_time = ACE_OS::time();
        tm my_tm;
        ACE_OS::localtime_r(&my_time, &my_tm);
        prodauto::Date date_today = { my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday};
        try
        {
            source_session = prodauto::SourceSession::load(rec, date_today);
            if(source_session == 0)
            {
                source_session = prodauto::SourceSession::create(rec, date_today);
                ACE_DEBUG((LM_INFO, ACE_TEXT("Created new source session.\n")));
            }
        }
        catch(const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
            db_ok = false;
        }
    }
    mSourceSession.reset(source_session);

    return db_ok;
}


