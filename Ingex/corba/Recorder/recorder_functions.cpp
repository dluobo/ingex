/*
 * $Id: recorder_functions.cpp,v 1.2 2007/01/30 12:29:23 john_f Exp $
 *
 * Functions which execute in recording threads.
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

#include "IngexShm.h"
#include "IngexRecorder.h"
#include "IngexRecorderImpl.h"
#include "RecorderSettings.h"
#include "dvd_encoder.h"
#include "AudioMixer.h"
#include "audio_utils.h"
#include "quad_split_I420.h"
#include "dv_encoder.h"

// prodautodb lib
#include "Database.h"
#include "DBException.h"

// prodautodb recordmxf
#include "MXFWriter.h"
#include "MXFWriterException.h"

#include <ace/Thread.h>
#include <ace/OS_NS_unistd.h>

#include <iostream>
#include <sstream>

/// Mutex to ensure only one thread at a time can call avcodec open/close
static ACE_Thread_Mutex avcodec_mutex;


/**
Thread function to record a single source as uncompressed
audio/video and as DVD compatible MPEG-2.
*/
ACE_THR_FUNC_RETURN start_record_thread(void * p_arg)
{
    IngexRecorder * p = ((ThreadInvokeArg *)p_arg)->p_rec;
    int cardnum = ((ThreadInvokeArg *)p_arg)->invoke_card;
    RecordOptions *p_opt = &(p->record_opt[cardnum]);

    int start_tc = p_opt->start_tc;
    int start_frame = p_opt->start_frame;

    ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("Starting start_record_thread(cardnum=%d, start_tc=%d start_frame=%d)\n"),
        cardnum, start_tc, start_frame));

    // This makes the record start at the target timecode
    int last_saved = start_frame - 1;


    int uyvy_video_size = IngexShm::Instance()->sizeVideo422();
    const int audio_samples_per_frame = 1920;

    // Track enables
    bool enable_video = p_opt->track_enable[0];
    bool enable_audio12 = p_opt->track_enable[1] || p_opt->track_enable[2];
    bool enable_audio34 = p_opt->track_enable[3] || p_opt->track_enable[4];

    // Mask for MXF track enables
    uint32_t mxf_mask = 0;
    if (enable_video)    mxf_mask |= 0x00000001;
    if (enable_audio12)  mxf_mask |= 0x00000006;
    if (enable_audio12)  mxf_mask |= 0x00000018;

    // Settings pointer
    RecorderSettings * settings = RecorderSettings::Instance();

    // Types of output
    bool raw = settings->raw;
    bool mxf = settings->mxf;
    int mxf_resolution = settings->mxf_resolution;
    bool enable_dvd = false;
    bool browse_audio = (cardnum == 0 && settings->browse_audio);

    // Directories
    const std::string & raw_dir = settings->raw_dir;
    const std::string & browse_dir = settings->browse_dir;
    const std::string & mxf_dir = settings->mxf_dir;
    const std::string & mxf_subdir_creating = settings->mxf_subdir_creating;
    const std::string & mxf_subdir_failures = settings->mxf_subdir_failures;

    // Encode parameters
    prodauto::Rational image_aspect = settings->image_aspect;
    int raw_audio_bits = settings->raw_audio_bits;
    int mxf_audio_bits = settings->mxf_audio_bits;
    int browse_audio_bits = settings->browse_audio_bits;
    int mpeg2_bitrate = settings->mpeg2_bitrate;

    // File pointers
    FILE * fp_video = NULL;
    FILE * fp_audio12 = NULL;
    FILE * fp_audio34 = NULL;
    FILE * fp_audio_browse = NULL;

    // Initialisation for raw uncompressed files
    if (raw && enable_video)
    {
        std::ostringstream fname;
        fname << raw_dir << p_opt->file_ident << ".uyvy";
        const char * f = fname.str().c_str();

        if (NULL == (fp_video = fopen(f, "wb")))
        {
            enable_video = false;
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open %C\n"), f));
        }
    }
    if (raw && enable_audio12)
    {
        std::ostringstream fname;
        fname << raw_dir << p_opt->file_ident << "_12.wav";
        const char * f = fname.str().c_str();

        if (NULL == (fp_audio12 = fopen(f, "wb")))
        {
            enable_audio12 = false;
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open %C\n"), f));
        }
        else
        {
            writeWavHeader(fp_audio12, raw_audio_bits);
        }
    }
    if (raw && enable_audio34)
    {
        std::ostringstream fname;
        fname << raw_dir << p_opt->file_ident << "_34.wav";
        const char * f = fname.str().c_str();

        if (NULL == (fp_audio34 = fopen(f, "wb")))
        {
            enable_audio34 = false;
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open %s\n"), f));
        }
        else
        {
            writeWavHeader(fp_audio34, raw_audio_bits);
        }
    }

    // Initialisation for browse audio
    if (browse_audio)
    {
        std::ostringstream fname;
        fname << browse_dir << p_opt->file_ident << ".wav";
        const char * f = fname.str().c_str();

        if (NULL == (fp_audio_browse = fopen(f, "wb")))
        {
            browse_audio = false;
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open %s\n"), f));
        }
        else
        {
            writeWavHeader(fp_audio_browse, browse_audio_bits);
        }
    }
    
    // DV encoder
    dv_encoder_t * dv_encoder = 0;

    if (mxf && (mxf_resolution == DV50_MATERIAL_RESOLUTION
        || mxf_resolution == DV25_MATERIAL_RESOLUTION))
    {
        // Prevent "insufficient thread locking around avcodec_open/close()"
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        dv_encoder_resolution res;
        switch (mxf_resolution)
        {
        case DV25_MATERIAL_RESOLUTION:
            res = DV_ENCODER_RESOLUTION_DV25;
            break;
        case DV50_MATERIAL_RESOLUTION:
            res = DV_ENCODER_RESOLUTION_DV50;
            break;
        default:
            break;
        }

        dv_encoder = dv_encoder_init(res);
        if (!dv_encoder)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("card %d: DV encoder init failed.\n"), cardnum));
            mxf = false;
        }
    }

    // Initialisation for MXF files
    prodauto::Recorder * rec = p->Recorder();
    prodauto::SourceSession * ss = p->SourceSession();

    prodauto::RecorderInputConfig * ric = 0;
    if (rec && rec->hasConfig())
    {
        prodauto::RecorderConfig * rc = rec->getConfig();
        ric = rc->getInputConfig(cardnum + 1); // Index starts from 1
        //ric = rc->recorderInputConfigs[cardnum];
    }
    
    if (mxf && (0 == ric || 0 == ss))
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("MXF metadata unavailable.\n")));
        mxf = false;
    }

    std::auto_ptr<prodauto::MXFWriter> writer;
    if (mxf)
    {
        std::vector<prodauto::UserComment> user_comments;
        user_comments.push_back(
            prodauto::UserComment(AVID_UC_DESCRIPTION_NAME, p_opt->description));

        std::ostringstream destination_path;
        std::ostringstream creating_path;
        std::ostringstream failures_path;
        destination_path << mxf_dir;
        creating_path << mxf_dir << mxf_subdir_creating;
        failures_path << mxf_dir << mxf_subdir_failures;

        try
        {
            prodauto::MXFWriter * p = new prodauto::MXFWriter(
                                            ric, ss,
                                            mxf_resolution,
                                            image_aspect,
                                            mxf_audio_bits,
                                            mxf_mask,
                                            start_tc,
                                            creating_path.str().c_str(),
                                            destination_path.str().c_str(),
                                            failures_path.str().c_str(),
                                            p_opt->file_ident,
                                            user_comments,
                                            p_opt->project
                                            );
            writer.reset(p);
        }
        catch (const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database exception: %C\n"),
                dbe.getMessage().c_str()));
            mxf = false;
        }
		ACE_DEBUG((LM_DEBUG, ACE_TEXT("Project name: %C\n"), p_opt->project.c_str()));

        // Temp: Incomplete filename for each track.
        // Need to get correct filenames from the MXFWriter.
        //for (unsigned int i = 0; i < 5; ++i)
        //{
        //   p_opt->file_name[i] = mxf_dir + p_opt->file_ident;
        //}

        // Get filenames for each track.
        for (unsigned int i = 0; i < 5; ++i)
        {
            unsigned int track = i + 1;
            if (writer->trackIsPresent(track))
            {
                p_opt->file_name[i] = mxf_dir + writer->getFilename(track);
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("File name: %C\n"), p_opt->file_name[i].c_str()));
            }
            else
            {
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("Track %d not present\n"), track));
            }
        }
    }

    // Initialisation for MPEG-2 files
    std::ostringstream mpeg2_filename;
    mpeg2_filename << browse_dir << p_opt->file_ident << ".mpg";

    dvd_encoder_t * dvd;
    if (enable_dvd)
    {
        // Prevent simultaneous calls to dvd_encoder_init causing
        // "insufficient thread locking around avcodec_open/close()"
        // and also "dvd format not registered".
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);
        if (0 == (dvd = dvd_encoder_init(mpeg2_filename.str().c_str(), mpeg2_bitrate)))
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("card %d: dvd_encoder_init() failed\n"), cardnum));
            enable_dvd = false;         // give up on dvd
        }
    }

    AudioMixer mixer;
    if (cardnum == 0)
    {
        mixer.SetMix(AudioMixer::CH12);
        //mixer.SetMix(AudioMixer::COMMENTARY3);
        //mixer.SetMix(AudioMixer::COMMENTARY4);
    }
    else
    {
        mixer.SetMix(AudioMixer::ALL);
    }



    // This is the loop which records audio/video frames
    bool finished_record = false;
    while (1)
    {
        // Read lastframe counter from shared memory in a thread safe manner
        int lastframe = IngexShm::Instance()->LastFrame(cardnum);

        if (last_saved == lastframe)
        {
            // Caught up to latest available frame
            // sleep for half a frame worth
            //ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C sleeping (a)\n"), SOURCE_NAME[cardnum]));
            ACE_OS::sleep(ACE_Time_Value(0, 20 * 1000));    // 0.020 seconds
            continue;
        }

        int diff = lastframe - last_saved;
        int allowed_backlog = IngexShm::Instance()->RingLength() - 3;
        if (diff > allowed_backlog / 10)
        {
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C frames waiting = %d\n"),
                SOURCE_NAME[cardnum], diff));
        }
        if (diff > allowed_backlog)
        {
            int drop = diff - allowed_backlog;
            diff -= drop;
            last_saved += drop;
                p_opt->IncFramesDropped(drop);
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C dropped frames worth %d (last_frame=%d)\n"),
                SOURCE_NAME[cardnum], drop, lastframe));
        }

        framecount_t last_tc = IngexShm::Instance()->Timecode(cardnum, last_saved);

        // Save all frames which have not been saved
        for (int fi = diff - 1; fi >= 0; fi--)
        {
            // Pointer to 422 video
            void * p_uyvy_video = IngexShm::Instance()->pVideo422(cardnum, lastframe - fi);

            // Pointer to 420 video
            void * p_yuv_video = IngexShm::Instance()->pVideo420(cardnum, lastframe - fi);

            // Pointer to audio12
            int32_t * p_audio12 = IngexShm::Instance()->pAudio12(cardnum, lastframe - fi);

            // Pointer to audio34
            int32_t * p_audio34 = IngexShm::Instance()->pAudio34(cardnum, lastframe - fi);

            // Timecode value
            framecount_t tc_i = IngexShm::Instance()->Timecode(cardnum, lastframe - fi);

            // Check for timecode irregularities
            if (tc_i != last_tc + 1)
            {
                ACE_DEBUG((LM_ERROR, ACE_TEXT("%C Timecode discontinuity: %d frames missing at frame=%d tc=%d\n"),
                SOURCE_NAME[cardnum],
                tc_i - last_tc - 1,
                lastframe - fi,
                tc_i));
            }

            // Mix audio for browse version
            int16_t mixed_audio[1920*2];    // for stereo pair output
            if (browse_audio || enable_dvd)
            {
                mixer.Mix(p_audio12, p_audio34, mixed_audio, 1920);
            }

            // Save browse audio
            if(browse_audio)
            {
                write_audio(fp_audio_browse, (uint8_t *)mixed_audio, 1920*2, browse_audio_bits, false);
            }

            // encode to dvd
            if (enable_dvd && dvd_encoder_encode(dvd, (uint8_t *)p_yuv_video, mixed_audio, tc_i) != 0)
            {
                ACE_DEBUG((LM_ERROR, ACE_TEXT("dvd_encoder_encode failed\n")));
                enable_dvd = false;
            }

            // Save uncompressed video
            if (raw && enable_video)
            {
                fwrite(p_uyvy_video, uyvy_video_size, 1, fp_video);
            }

            // Save uncompressed audio
            if (raw && enable_audio12)
            {
                write_audio(fp_audio12, (uint8_t *)p_audio12, 1920*2, raw_audio_bits, true);
            }
            if (raw && enable_audio34)
            {
                write_audio(fp_audio34, (uint8_t *)p_audio34, 1920*2, raw_audio_bits, true);
            }

            // Save OP-Atom MXF files

            // Buffers for audio de-interleaving
            int16_t a0[audio_samples_per_frame];
            int16_t a1[audio_samples_per_frame];

            uint8_t * p_dv_video;
            int frame_number = 0; // Should be set to correct frame
            int size = 0;
            if (mxf && mxf_resolution == DV25_MATERIAL_RESOLUTION)
            {
                size = dv_encoder_encode(dv_encoder, (uint8_t *)p_yuv_video, &p_dv_video, frame_number);
            }
            if (mxf && mxf_resolution == DV50_MATERIAL_RESOLUTION)
            {
                size = dv_encoder_encode(dv_encoder, (uint8_t *)p_uyvy_video, &p_dv_video, frame_number);
            }
            if (mxf && enable_video)
            {
                try
                {
                    switch (mxf_resolution)
                    {
                    case DV25_MATERIAL_RESOLUTION:
                        writer->writeSample(1, 1, p_dv_video, size);
                        break;
                    case DV50_MATERIAL_RESOLUTION:
                        writer->writeSample(1, 1, p_dv_video, size);
                        break;
                    default:
                        // write uncompressed video data to input track 1
                        writer->writeSample(1, 1, (uint8_t *)p_uyvy_video, uyvy_video_size);
                        break;
                    }
                }
                catch (prodauto::MXFWriterException)
                {
                    mxf = false;
                }
            }

            if (mxf && enable_audio12)
            {
                // de-interleave audio channels 1/2 and convert to 16-bit
                deinterleave_32to16(p_audio12, a0, a1, audio_samples_per_frame);

                try
                {
                    // write pcm audio data to input track 2
                    writer->writeSample(2, audio_samples_per_frame, (uint8_t *)a0, audio_samples_per_frame * 2);

                    // write pcm audio data to input track 3
                    writer->writeSample(3, audio_samples_per_frame, (uint8_t *)a1, audio_samples_per_frame * 2);
                }
                catch (prodauto::MXFWriterException)
                {
                    mxf = false;
                }
            }

            if (mxf && enable_audio34)
            {
                // de-interleave audio channels 3/4 and convert to 16-bit
                deinterleave_32to16(p_audio34, a0, a1, audio_samples_per_frame);

                try
                {
                    // write pcm audio data to input track 4
                    writer->writeSample(4, audio_samples_per_frame, (uint8_t *)a0, audio_samples_per_frame * 2);

                    // write pcm audio data to input track 5
                    writer->writeSample(5, audio_samples_per_frame, (uint8_t *)a1, audio_samples_per_frame * 2);
                }
                catch (prodauto::MXFWriterException)
                {
                    mxf = false;
                }
            }

            // Completed this frame
            p_opt->IncFramesWritten();

            last_tc = tc_i;

            //int tc_diff = tc - last_tc;
            //char tcstr[32];
            //logF("card%d lastframe=%6d diff(fi=%d) =%6d tc_i=%7d tc_diff=%3d %s\n", cardnum, lastframe, fi, diff, tc_i, tc_diff, framesToStr(tc, tcstr));

            // Finish when we've reached target duration
            framecount_t target = p_opt->TargetDuration();
            framecount_t written = p_opt->FramesWritten();
            framecount_t dropped = p_opt->FramesDropped();
            framecount_t total = written + dropped;
            if (target > 0 && total >= target)
            {
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("  card %d duration %d reached (total=%d written=%d dropped=%d)\n"),
                            cardnum, target, total, written, dropped));
                finished_record = true;
                break;
            }
        }

        if (finished_record)
        {
            break;
        }

        last_saved = lastframe;
    }

    // update and close files
    if (raw && enable_video)
    {
        fclose(fp_video);
    }
    if (raw && enable_audio12)
    {
        update_WAV_header(fp_audio12);
    }
    if (raw && enable_audio34)
    {
        update_WAV_header(fp_audio34);
    }
    if (browse_audio)
    {
        update_WAV_header(fp_audio_browse);
    }

    // shutdown dvd encoder
    if (enable_dvd)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        if (dvd_encoder_close(dvd) != 0)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("card %d: dvd_encoder_close() failed\n"), cardnum));
        }
    }

    // shutdown dv encoder
    if (dv_encoder)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        if (dv_encoder_close(dv_encoder) != 0)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("card %d: dv_encoder_close() failed\n"), cardnum));
        }
    }

    if (mxf)
    {
        // complete the writing and save packages to database
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Record thread %d completing MXF save\n"), cardnum));
        writer->completeAndSaveToDatabase();
    }


    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Record thread %d exiting\n"), cardnum));
    return 0;
}

/**
Thread function to record quad-split as DVD-compatible MPEG-2 file.
*/
ACE_THR_FUNC_RETURN start_quad_thread(void * p_arg)
{
    IngexRecorder * p = ((ThreadInvokeArg *)p_arg)->p_rec;
    int cardnum = ((ThreadInvokeArg *)p_arg)->invoke_card; // will be QUAD_SOURCE
    RecordOptions * p_opt = &(p->record_opt[cardnum]);


    int start_tc = p_opt->start_tc;
    int start_frame = p_opt->start_frame;
    int quad1_offset = p_opt->quad1_frame_offset;
    int quad2_offset = p_opt->quad2_frame_offset;
    int quad3_offset = p_opt->quad3_frame_offset;

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Starting start_quad_thread(start_tc=%d start_frame=%d)\n"
          "     quad1_frame_offset=%d quad2_frame_offset=%d quad3_frame_offset=%d\n"), 
            start_tc, start_frame,
            quad1_offset, quad2_offset, quad3_offset));

    // TODO: If main not present quad currently segv's
    if (! p->record_opt[0].enabled)
    {
        ACE_DEBUG((LM_ERROR,
            ACE_TEXT("start_quad_thread: main not enabled - recording quad split not supported (returning)\n")));
        return NULL;
    }

    // This makes the record start at the target timecode
    int last_saved = start_frame - 1;

    int tc;
    int last_tc = -1;

    // Recorder settings
    RecorderSettings * settings = RecorderSettings::Instance();

    bool quad_mpeg2 = settings->quad_mpeg2;
    const std::string & browse_dir = settings->browse_dir;
    int mpeg2_bitrate = settings->mpeg2_bitrate;
    
    std::string quad_filename = browse_dir;
    quad_filename += p_opt->file_ident + ".mpg";
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Quad MPEG-2 filename: %C\n"), quad_filename.c_str()));

    dvd_encoder_t * dvd = 0;
    if (quad_mpeg2)
    {
        // Prevent simultaneous calls to dvd_encoder_init causing
        // "insufficient thread locking around avcodec_open/close()"
        // and also "dvd format not registered".
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        if ((dvd = dvd_encoder_init(quad_filename.c_str(), mpeg2_bitrate)) == 0)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("card %d: dvd_encoder_init() failed\n"), cardnum));
            quad_mpeg2 = false;         // give up on dvd
        }
    }

    AudioMixer mixer;
    mixer.SetMix(AudioMixer::CH12);
    //mixer.SetMix(AudioMixer::COMMENTARY3);
    //mixer.SetMix(AudioMixer::COMMENTARY4);

    bool finished_record = false;

    // Buffer to receive quad-split video
    unsigned char           p_quadvideo[720*576*3/2];
    memset(p_quadvideo, 0x10, 720*576);         // Y black
    memset(p_quadvideo + 720*576, 0x80, 720*576/4);         // U neutral
    memset(p_quadvideo + 720*576*5/4, 0x80, 720*576/4);         // V neutral
    
    while (1)
    {
        // Workaround for future echos which can occur when video arrives
        // at slightly different times
        //
        // Give a guard of 2 extra frame before lastframe since the calculated
        // lastframe on the other (non key) sources might not be ready in practice
        //
        // There is currently no need for the encoded quad split to be
        // absolutely up-to-date in real time.
        int guard = 2;

        // Read lastframe counter from shared memory in a thread safe manner
        int lastframe[4];
        for(unsigned int i = 0; i < IngexShm::Instance()->Cards(); ++i)
        {
            lastframe[i] = IngexShm::Instance()->LastFrame(i) - guard;
        }

        if (last_saved == lastframe[0])
        {
            // Caught up to latest available frame
            // sleep for half a frame worth
            ACE_OS::sleep(ACE_Time_Value(0, 20 * 1000));        // 0.020 seconds = 50 times a sec
            continue;
        }

        // read timecode value for first source
        //tc = *(int*)(ring[ 0 ] + elementsize * (lastframe0 % ringlen) + tc_offset);
        tc = IngexShm::Instance()->Timecode(0, lastframe[0]);

        // check for start timecode if any
        // we will record as soon as active tc is >= target timecode
        if (tc < start_tc)
        {
            ACE_OS::sleep(ACE_Time_Value(0, 20 * 1000));        // 0.020 seconds = 50 times a sec
            continue;
        }

        int diff = lastframe[0] - last_saved;
        if (diff >= IngexShm::Instance()->RingLength())
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("dropped frames worth %d (last_saved=%d lastframe0=%d)\n"),
                        diff, last_saved, lastframe[0]));
            last_saved = lastframe[0] - 1;
            diff = 1;
        }

        // Save all frames which have not been saved
        // All decisions taken from source 0
        //
        for (int fi = diff - 1; fi >= 0; fi--)
        {
            // lastframe adjusted for fi (frame back count index) and source offsets
            int lfa[MAX_CARDS];
            lfa[0] = lastframe[0] - fi;
            lfa[1] = lastframe[0] - fi + quad1_offset;
            lfa[2] = lastframe[0] - fi + quad2_offset;
            lfa[2] = lastframe[0] - fi + quad3_offset;

            // audio is taken from source 0
            const int32_t * p_audio12 = IngexShm::Instance()->pAudio12(0, lfa[0]);
            const int32_t * p_audio34 = IngexShm::Instance()->pAudio34(0, lfa[0]);

            // mix audio
            int16_t mixed[1920*2];  // stereo pair output
            mixer.Mix(p_audio12, p_audio34, mixed, 1920);

            // Make quad split

            I420_frame in;
            in.size = 720*576*3/2;
            in.w = 720;
            in.h = 576;
            // Other elements set below

            I420_frame out;
            out.cbuff = p_quadvideo;
            out.size = 720*576*3/2;
            out.Ybuff = out.cbuff;
            out.Ubuff = out.cbuff + 720*576;
            out.Vbuff = out.cbuff + 720*576 *5/4;
            out.w = 720;
            out.h = 576;

            // top left - card0
            if (p->record_opt[0].enabled)
            {
                in.cbuff = IngexShm::Instance()->pVideo420(0, lfa[0]);
                in.Ybuff = in.cbuff;
                in.Ubuff = in.cbuff + 720*576;
                in.Vbuff = in.cbuff + 720*576 *5/4;

                quarter_frame_I420(&in, &out,
                                0,      // x offset
                                0,      // y offset
                                1,      // interlace?
                                1,      // horiz filter?
                                1);     // vert filter?
            }

            // top right - card1
            if (p->record_opt[1].enabled)
            {
                in.cbuff = IngexShm::Instance()->pVideo420(1, lfa[1]);
                in.Ybuff = in.cbuff;
                in.Ubuff = in.cbuff + 720*576;
                in.Vbuff = in.cbuff + 720*576 *5/4;

                quarter_frame_I420(&in, &out,
                                720/2, 0,
                                1, 1, 1);
            }

            // bottom left - card2
            if (p->record_opt[2].enabled)
            {
                in.cbuff = IngexShm::Instance()->pVideo420(2, lfa[2]);
                in.Ybuff = in.cbuff;
                in.Ubuff = in.cbuff + 720*576;
                in.Vbuff = in.cbuff + 720*576 *5/4;

                quarter_frame_I420(&in, &out,
                                    0, 576/2,
                                    1, 1, 1);
            }

            // bottom right - card3
            if (p->record_opt[3].enabled)
            {
                 in.cbuff = IngexShm::Instance()->pVideo420(3, lfa[3]);
                in.Ybuff = in.cbuff;
                in.Ubuff = in.cbuff + 720*576;
                in.Vbuff = in.cbuff + 720*576 *5/4;

                quarter_frame_I420(&in, &out,
                                    720/2, 576/2,
                                    1, 1, 1);
            }

            // read timecodes for each card
            int a_tc[MAX_CARDS];
            for (unsigned int i = 0; i < IngexShm::Instance()->Cards(); i++)
            {
                a_tc[i] = IngexShm::Instance()->Timecode(i, lfa[i]);
            }
            // work out which timecode to burn in based on enable flags
            int tc_burn = a_tc[0];
            for (int i = IngexShm::Instance()->Cards() - 1; i >= 0; i--)
            {
                if (p->record_opt[i].enabled)
                {
                    tc_burn = a_tc[i];
                }
            }

            // encode to mpeg2
            if (quad_mpeg2)
            {
                if (0 != dvd_encoder_encode(dvd, p_quadvideo, mixed, tc_burn))
                {
                    ACE_DEBUG((LM_ERROR, ACE_TEXT("dvd_encoder_encode failed\n")));
                    quad_mpeg2 = false;
                }
            }

            p_opt->IncFramesWritten();

            //logF("quad fi=%2d lf0,lfa0=%6d,%6d lf1,lfa1=%6d,%6d lf2,lfa2=%6d,%6d tc0=%6d tc1=%6d tc2=%6d tc_burn=%d\n", fi,
            //  lastframe0, lfa[0], lastframe1, lfa[1], lastframe2, lfa[2],
            //  a_tc[0], a_tc[1], a_tc[2], tc_burn);

            // Finish when we've reached duration number of frames
            framecount_t duration = p_opt->TargetDuration();
            if (duration > 0 && p_opt->FramesWritten() >= duration)
            {
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("  quad   duration %d reached (frames_written=%d)\n"),
                            duration, p_opt->FramesWritten()));
                finished_record = true;
                break;
            }

            // tiny delay to avoid getting future frames
            // didn't work
            //usleep(1 * 1000);
        }

        if (finished_record)
        {
            break;
        }

        last_tc = tc;
        last_saved = lastframe[0];
    }

    // shutdown dvd encoder
    if (quad_mpeg2)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        if (dvd_encoder_close(dvd) != 0)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("quad  : dvd_encoder_close() failed\n")));
        }
    }

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Quad record thread exiting\n")));
    return 0;
}

/**
Thread function to wait for recording threads to finish and
then perform completion tasks.
*/
ACE_THR_FUNC_RETURN manage_record_thread(void *p_arg)
{
    IngexRecorder * p = (IngexRecorder *)p_arg;

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("manage_record_thread:\n")));

    // Wait for all threads to finish
    for (int card_i = 0; card_i < MAX_RECORD; card_i++)
    {
        if (! p->record_opt[card_i].enabled)
        {
            continue;
        }

        //logTF("Waiting for thread %d\n", card_i);
        if ( p->record_opt[card_i].thread_id == 0 )
        {
            //logTF("thread for card[%d] is 0.  skipping\n", card_i);
            continue;
        }

        int err = ACE_Thread::join(p->record_opt[card_i].thread_id, 0, 0);
        //int err = pthread_join( p->record_opt[card_i].thread_id, NULL);
        if(err)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to join record thread: %s\n"), strerror(err)));
        }
    }

#if 0
    // Set pathnames for metadata files
    std::string vid_meta_name = p->mVideoPath;
    vid_meta_name += "metadata.txt";
    std::string dvd_meta_name = p->mDvdPath;
    dvd_meta_name += "metadata.txt";

    // Write metadata file alongside uncompressed video files
    p->WriteMetadataFile(vid_meta_name.c_str());

    // If dvd path is different, write another copy alongside
    // the MPEG2 files.
    if (dvd_meta_name != vid_meta_name)
    {
        p->WriteMetadataFile(dvd_meta_name.c_str());
    }
#endif

    // Signal completion of recording
    p->DoCompletionCallback();

    return 0;
}



