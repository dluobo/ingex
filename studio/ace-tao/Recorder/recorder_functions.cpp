/*
 * $Id: recorder_functions.cpp,v 1.1 2007/09/11 14:08:31 stuart_hc Exp $
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
#include "Logfile.h"
#include "Timecode.h"
#include "dvd_encoder.h"
#include "AudioMixer.h"
#include "audio_utils.h"
#include "quad_split_I420.h"
#include "ffmpeg_encoder.h"
#include "ffmpeg_encoder_av.h"
#include "mjpeg_compress.h"
#include "tc_overlay.h"

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
Thread function to encode and record a particular source.
*/
ACE_THR_FUNC_RETURN start_record_thread(void * p_arg)
{
    ThreadParam * param = (ThreadParam *)p_arg;
    IngexRecorder * p_rec = param->p_rec;
    RecordOptions * p_opt = param->p_opt;

    bool quad_video = p_opt->quad;
    int card_i = p_opt->card_num;
    const char * src_name = (quad_video ? QUAD_NAME : SOURCE_NAME[card_i]);

    // ACE logging to file
    std::ostringstream id;
    id << "record_" << src_name << "_" << p_opt->index;
    Logfile::Open(id.str().c_str(), false);

    // Convenience variable
    const int ring_length = IngexShm::Instance()->RingLength();

    // Recording starts from start_frame.
    int start_frame = p_rec->mStartFrame[card_i];
    int start_tc = p_rec->mStartTimecode; // passed as metadata to MXFWriter

    // We make sure these offsets are positive numbers.
    // For quad, card_i is first enabled card.
    int quad0_offset = (p_rec->mStartFrame[0] - p_rec->mStartFrame[card_i] + ring_length) % ring_length;
    int quad1_offset = (p_rec->mStartFrame[1] - p_rec->mStartFrame[card_i] + ring_length) % ring_length;
    int quad2_offset = (p_rec->mStartFrame[2] - p_rec->mStartFrame[card_i] + ring_length) % ring_length;
    int quad3_offset = (p_rec->mStartFrame[3] - p_rec->mStartFrame[card_i] + ring_length) % ring_length;

    // This makes the record start at the target frame
    int last_saved = start_frame - 1;


    int size_422_video = IngexShm::Instance()->sizeVideo422();
    const int audio_samples_per_frame = 1920;
    const int width = 720;
    const int height = 576;
    const int SIZE_420 = width * height * 3/2;
    const int SIZE_422 = width * height * 2;

    // Track enables
    bool enable_video = p_rec->mCardEnable[card_i];
    bool enable_audio12 = p_rec->mTrackEnable[card_i * 5 + 1] || p_rec->mTrackEnable[card_i * 5 + 2];
    bool enable_audio34 = p_rec->mTrackEnable[card_i * 5 + 3] || p_rec->mTrackEnable[card_i * 5 + 4];

    // Mask for MXF track enables
    uint32_t mxf_mask = 0;
    if (enable_video)    mxf_mask |= 0x00000001;
    if (enable_audio12)  mxf_mask |= 0x00000006;
    if (enable_audio34)  mxf_mask |= 0x00000018;

    // Settings pointer
    RecorderSettings * settings = RecorderSettings::Instance();

    bool browse_audio = (card_i == 0 && !quad_video && settings->browse_audio);

    // Get encode settings for this thread
    int resolution = p_opt->resolution;
    bool uncompressed_video = false;

    bool mxf = (Wrapping::MXF == p_opt->wrapping);
    bool raw = !mxf;

    // burnt-in timecode settings
    bool bitc = p_opt->bitc;
    bitc = true; //tmp test
    unsigned int tc_xoffset;
    unsigned int tc_yoffset;
    if (quad_video)
    {
        // centre
        tc_xoffset = 260;
        tc_yoffset = 276;
    }
    else
    {
        // top centre
        tc_xoffset = 260;
        tc_yoffset = 30;
    }


    // Override some settings for testing
#if 0
    raw = true;
    mxf = false;
    resolution = DV25_MATERIAL_RESOLUTION;
#endif

    ACE_DEBUG((LM_INFO,
        ACE_TEXT("start_record_thread(%C, start_tc=%C res=%d)\n"),
        src_name, Timecode(start_tc).Text(), resolution));

    // Set some flags based on encoding type
    enum {PIXFMT_420, PIXFMT_422} pix_fmt;
    enum {ENCODER_NONE, ENCODER_FFMPEG, ENCODER_FFMPEG_AV, ENCODER_MJPEG} encoder;
    ffmpeg_encoder_resolution_t ff_res;
    ffmpeg_encoder_av_resolution_t ff_av_res;
    MJPEGResolutionID mjpeg_res;
    switch (resolution)
    {
    // DV formats
    case DV25_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_420;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DV25;
        break;
    case DV50_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DV50;
        break;
    // MJPEG formats
    case MJPEG21_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_MJPEG;
        mjpeg_res = MJPEG_2_1;
        break;
    case MJPEG31_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_MJPEG;
        mjpeg_res = MJPEG_3_1;
        break;
    case MJPEG101_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_MJPEG;
        mjpeg_res = MJPEG_10_1;
        break;
    case MJPEG151S_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_MJPEG;
        mjpeg_res = MJPEG_15_1s;
        break;
    case MJPEG201_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_MJPEG;
        mjpeg_res = MJPEG_20_1;
        break;
    case MJPEG101M_MATERIAL_RESOLUTION:
        encoder = ENCODER_MJPEG;
        pix_fmt = PIXFMT_422;
        mjpeg_res = MJPEG_10_1m;
        bitc = true; // special for Dragons' Den
        break;
    // IMX formats
    case IMX30_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_IMX30;
        break;
    case IMX40_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_IMX40;
        break;
    case IMX50_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_IMX50;
        break;
    // Browse formats
    case DVD_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_420;
        encoder = ENCODER_FFMPEG_AV;
        ff_av_res = FF_ENCODER_RESOLUTION_DVD;
        bitc = true;
        mxf = false;
        raw = false;
        break;
    case MOV_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_420;
        encoder = ENCODER_FFMPEG_AV;
        ff_av_res = FF_ENCODER_RESOLUTION_MOV;
        bitc = true;
        mxf = false;
        raw = false;
        break;
    default:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_NONE;
        break;
    }

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
    //int mpeg2_bitrate = settings->mpeg2_bitrate;

    // File pointers
    FILE * fp_video = NULL;
    FILE * fp_audio12 = NULL;
    FILE * fp_audio34 = NULL;
    FILE * fp_audio_browse = NULL;

    // Initialisation for raw (non-MXF) files
    if (raw && enable_video)
    {
        std::ostringstream fname;
        fname << raw_dir << p_opt->file_ident;
        switch (resolution)
        {
        case DV25_MATERIAL_RESOLUTION:
        case DV50_MATERIAL_RESOLUTION:
            fname << ".dv";
            break;
        case IMX30_MATERIAL_RESOLUTION:
        case IMX40_MATERIAL_RESOLUTION:
        case IMX50_MATERIAL_RESOLUTION:
            fname << ".m2v";
            break;
        default:
            fname << ".yuv";
            break;
        }
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
    
    // Initialise ffmpeg encoder
    ffmpeg_encoder_t * ffmpeg_encoder = 0;
    if (ENCODER_FFMPEG == encoder)
    {
        // Prevent "insufficient thread locking around avcodec_open/close()"
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        ffmpeg_encoder = ffmpeg_encoder_init(ff_res);
        if (!ffmpeg_encoder)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C: ffmpeg encoder init failed.\n"), src_name));
        }
    }
    
    // Initialise MJPEG encoder
    mjpeg_compress_t * mj_encoder = 0;
    if (ENCODER_MJPEG == encoder)
    {
        mjpeg_compress_init(mjpeg_res, width, height, &mj_encoder);
    }

    // Initialisation for browse av files
    std::ostringstream browse_filename;
    browse_filename << browse_dir << p_opt->file_ident;

    // ffmpeg av encoder
    ffmpeg_encoder_av_t * enc_av = 0;
    if (ENCODER_FFMPEG_AV == encoder)
    {
        // Prevent simultaneous calls to init function causing
        // "insufficient thread locking around avcodec_open/close()"
        // and also "format not registered".
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        if (0 == (enc_av = ffmpeg_encoder_av_init(browse_filename.str().c_str(),
            ff_av_res)))
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C: ffmpeg_encoder_av_init() failed\n"), src_name));
            encoder = ENCODER_NONE;
        }
    }

    // Initialisation for MXF files
    prodauto::Recorder * rec = p_rec->Recorder();

    //prodauto::RecorderInputConfig * ric = 0;
    prodauto::RecorderConfig * rc = 0;
    if (rec && rec->hasConfig())
    {
        rc = rec->getConfig();
        //ric = rc->getInputConfig(card_i + 1); // Index starts from 1
    }
    
    if (mxf && (0 == rc))
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("MXF metadata unavailable.\n")));
        mxf = false;
    }

    std::auto_ptr<prodauto::MXFWriter> writer;
    if (mxf)
    {
        std::vector<prodauto::UserComment> user_comments;
        if (!p_opt->description.empty())
        {
            user_comments.push_back(
                prodauto::UserComment(AVID_UC_DESCRIPTION_NAME, p_opt->description));
        }

        std::ostringstream destination_path;
        std::ostringstream creating_path;
        std::ostringstream failures_path;
        destination_path << mxf_dir;
        creating_path << mxf_dir << mxf_subdir_creating;
        failures_path << mxf_dir << mxf_subdir_failures;

        try
        {
            prodauto::MXFWriter * p = new prodauto::MXFWriter(
                                            rc,
                                            card_i + 1, // index starts from 1
                                            resolution,
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
        catch (const prodauto::MXFWriterException & e)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("MXFWriterException: %C\n"), e.getMessage().c_str()));
            mxf = false;
        }

        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Project name: %C\n"), p_opt->project.c_str()));

        // Get filenames for each track.
        if (mxf)
        {
            for (unsigned int i = 0; i < 5; ++i)
            {
                unsigned int track = i + 1;
                if (writer->trackIsPresent(track))
                {
                    //opt.file_name[i] = mxf_dir + writer->getFilename(track);
                    std::string fname = mxf_dir + writer->getFilename(track);
                    p_rec->mFileNames[card_i * 5 + i] = fname;
                    ACE_DEBUG((LM_DEBUG, ACE_TEXT("File name: %C\n"), fname.c_str()));
                }
                else
                {
                    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Track %d not present\n"), track));
                }
            }
        }
    }

    // Initialisation for timecode overlay
    tc_overlay_t * tco = 0;
    uint8_t * tc_overlay_buffer = 0;
    if (bitc)
    {
        tco = tc_overlay_init();
        if (pix_fmt == PIXFMT_420)
        {
            tc_overlay_buffer = new uint8_t[SIZE_420];
        }
        else
        {
            tc_overlay_buffer = new uint8_t[SIZE_422];
        }
    }
    if (tco)
    {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("TC overlay initialised\n")));
    }

    AudioMixer mixer;
    if (card_i == 0)
    {
        mixer.SetMix(AudioMixer::CH12);
        //mixer.SetMix(AudioMixer::COMMENTARY3);
        //mixer.SetMix(AudioMixer::COMMENTARY4);
    }
    else
    {
        mixer.SetMix(AudioMixer::ALL);
    }

    // Buffer to receive quad-split video
    //uint8_t  p_quadvideo[720*576*3/2];
    //memset(p_quadvideo,               0x10, 720*576);           // Y black
    //memset(p_quadvideo + 720*576,     0x80, 720*576/4);         // U neutral
    //memset(p_quadvideo + 720*576*5/4, 0x80, 720*576/4);         // V neutral

    uint8_t * p_quadvideo = 0;
    if (quad_video && pix_fmt == PIXFMT_420)
    {
        p_quadvideo = new uint8_t[SIZE_420];
        memset(p_quadvideo,               0x10, 720*576);           // Y black
        memset(p_quadvideo + 720*576,     0x80, 720*576/4);         // U neutral
        memset(p_quadvideo + 720*576*5/4, 0x80, 720*576/4);         // V neutral
    }
    else if (quad_video && pix_fmt == PIXFMT_422)
    {
        p_quadvideo = new uint8_t[SIZE_422];
        memset(p_quadvideo,               0x10, 720*576);           // Y black
        memset(p_quadvideo + 720*576,     0x80, 720*576/2);         // U neutral
        memset(p_quadvideo + 720*576*3/2, 0x80, 720*576/2);         // V neutral
    }
    

    // ***************************************************
    // This is the loop which records audio/video frames
    // ***************************************************
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("start_frame=%d\n"), start_frame));
    p_opt->FramesWritten(0);
    p_opt->FramesDropped(0);
    bool finished_record = false;
    while (1)
    {
        // We read lastframe counter from shared memory in a thread safe manner.
        // Capture daemon updates lastframe after the frame has been written
        // to shared memory.

        int lastframe;
        int diff;
        int guard = (quad_video ? 2 : 1);
        while ((diff = (lastframe = IngexShm::Instance()->LastFrame(card_i)) - last_saved) < guard)
        {
            // Caught up to latest available frame - sleep for a while.
            //ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C sleeping (a)\n"), src_name));
            ACE_OS::sleep(ACE_Time_Value(0, 20 * 1000));    // 0.020 seconds
        }
        //ACE_DEBUG((LM_DEBUG, ACE_TEXT("last_saved=%d, lastframe=%d\n"),
        //    last_saved, lastframe));

        int allowed_backlog = ring_length - 3;
        if (diff > allowed_backlog / 2)
        {
            ACE_DEBUG((LM_WARNING, ACE_TEXT("%C frames waiting = %d, max = %d\n"),
                src_name, diff, allowed_backlog));
        }
        if (diff > allowed_backlog)
        {
            int drop = diff - allowed_backlog;
            diff -= drop;
            last_saved += drop;
            p_opt->IncFramesDropped(drop);
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C dropped %d frames!\n"),
                src_name, drop));
        }

        framecount_t last_tc = IngexShm::Instance()->Timecode(card_i, last_saved);

        // Save all frames which have not been saved
        for (int fi = diff - 1; fi >= 0; fi--)
        {
            int frame = lastframe - fi;
            if (frame < 0)
            {
                frame += ring_length;
            }
            int frame0 = (frame + quad0_offset) % ring_length;
            int frame1 = (frame + quad1_offset) % ring_length;
            int frame2 = (frame + quad2_offset) % ring_length;
            int frame3 = (frame + quad3_offset) % ring_length;

            // Pointer to 422 video
            void * p_422_video = IngexShm::Instance()->pVideo422(card_i, frame);

            // Pointer to 420 video
            void * p_420_video = IngexShm::Instance()->pVideo420(card_i, frame);

            // Pointer to audio12
            int32_t * p_audio12 = IngexShm::Instance()->pAudio12(card_i, frame);

            // Pointer to audio34
            int32_t * p_audio34 = IngexShm::Instance()->pAudio34(card_i, frame);

            // Timecode value
            framecount_t tc_i = IngexShm::Instance()->Timecode(card_i, frame);

            // Check for timecode irregularities
            if (tc_i != last_tc + 1)
            {
                ACE_DEBUG((LM_ERROR, ACE_TEXT("%C Timecode discontinuity: %d frames missing at frame=%d tc=%C\n"),
                    src_name,
                    tc_i - last_tc - 1,
                    lastframe - fi,
                    Timecode(tc_i).Text()));
            }

            // Make quad split
            // Note that at the moment quad split is not supported for 422 pix_fmt
            if (quad_video && pix_fmt == PIXFMT_420)
            {
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

                // NB. p_quadvideo was filled with black earlier.

                // top left - card0
                if (p_rec->mCardEnable[0])
                {
                    in.cbuff = IngexShm::Instance()->pVideo420(0, frame0);
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
                if (p_rec->mCardEnable[1])
                {
                    in.cbuff = IngexShm::Instance()->pVideo420(1, frame1);
                    in.Ybuff = in.cbuff;
                    in.Ubuff = in.cbuff + 720*576;
                    in.Vbuff = in.cbuff + 720*576 *5/4;

                    quarter_frame_I420(&in, &out,
                                    720/2, 0,
                                    1, 1, 1);
                }

                // bottom left - card2
                if (p_rec->mCardEnable[2])
                {
                    in.cbuff = IngexShm::Instance()->pVideo420(2, frame2);
                    in.Ybuff = in.cbuff;
                    in.Ubuff = in.cbuff + 720*576;
                    in.Vbuff = in.cbuff + 720*576 *5/4;

                    quarter_frame_I420(&in, &out,
                                        0, 576/2,
                                        1, 1, 1);
                }

                // bottom right - card3
                if (p_rec->mCardEnable[3])
                {
                    in.cbuff = IngexShm::Instance()->pVideo420(3, frame3);
                    in.Ybuff = in.cbuff;
                    in.Ubuff = in.cbuff + 720*576;
                    in.Vbuff = in.cbuff + 720*576 *5/4;

                    quarter_frame_I420(&in, &out,
                                        720/2, 576/2,
                                        1, 1, 1);
                }

                // Source for encoding is now the quad buffer
                p_420_video = p_quadvideo;
            }

            // Add timecode overlay
            if (bitc)
            {
                tc_overlay_setup(tco, tc_i);
                if (pix_fmt == PIXFMT_420)
                {
                    // Need to copy video as can't overwrite shared memory
                    memcpy(tc_overlay_buffer, p_420_video, SIZE_420);
                    uint8_t * p_y = tc_overlay_buffer;
                    uint8_t * p_u = p_y + width * height;
                    uint8_t * p_v = p_u + width * height / 4;
                    tc_overlay_apply420(tco, p_y, p_u, p_v, width, height, tc_xoffset, tc_yoffset);
                    // Source for encoding is now the timecode overlay buffer
                    p_420_video = tc_overlay_buffer;
                }
                else
                {
                    // Need to copy video as can't overwrite shared memory
                    memcpy(tc_overlay_buffer, p_422_video, SIZE_422);
                    uint8_t * p_y = tc_overlay_buffer;
                    uint8_t * p_u = p_y + width * height;
                    uint8_t * p_v = p_u + width * height / 2;
                    tc_overlay_apply420(tco, p_y, p_u, p_v, width, height, tc_xoffset, tc_yoffset);
                    // Source for encoding is now the timecode overlay buffer
                    p_422_video = tc_overlay_buffer;
                }
            }

            // Mix audio for browse version
            int16_t mixed_audio[1920*2];    // for stereo pair output
            if (browse_audio || ENCODER_FFMPEG_AV == encoder)
            {
                mixer.Mix(p_audio12, p_audio34, mixed_audio, 1920);
            }

            // Save browse audio
            if (browse_audio)
            {
                write_audio(fp_audio_browse, (uint8_t *)mixed_audio, 1920*2, browse_audio_bits, false);
            }

            // encode to browse av formats
            if (ENCODER_FFMPEG_AV == encoder)
            {
                if (ffmpeg_encoder_av_encode(enc_av, (uint8_t *)p_420_video, mixed_audio) != 0)
                {
                    ACE_DEBUG((LM_ERROR, ACE_TEXT("dvd_encoder_encode failed\n")));
                    encoder = ENCODER_NONE;
                }
            }


            uint8_t * p_enc_video;
            int size_enc_video = 0;

            // Encode with FFMPEG (IMX, DV)
            if (ENCODER_FFMPEG == encoder)
            {
                if (PIXFMT_420 == pix_fmt)
                {
                    size_enc_video = ffmpeg_encoder_encode(ffmpeg_encoder, (uint8_t *)p_420_video, &p_enc_video);
                }
                else
                {
                    size_enc_video = ffmpeg_encoder_encode(ffmpeg_encoder, (uint8_t *)p_422_video, &p_enc_video);
                }
            }

            // Encode MJPEG
            if (ENCODER_MJPEG == encoder)
            {
                uint8_t * y = (uint8_t *)p_422_video;
                uint8_t * u = y + width * height;
                uint8_t * v = u + width * height / 2;
                size_enc_video = mjpeg_compress_frame_yuv(mj_encoder, y, u, v, width, width/2, width/2, &p_enc_video);
            }


            // Write raw (non-MXF) files
            if (raw && enable_video)
            {
                if (uncompressed_video)
                {
                    fwrite(p_422_video, size_422_video, 1, fp_video);
                }
                else
                {
                    fwrite(p_enc_video, size_enc_video, 1, fp_video);
                }
            }

            // Write uncompressed audio
            if (raw && enable_audio12)
            {
                write_audio(fp_audio12, (uint8_t *)p_audio12, 1920*2, raw_audio_bits, true);
            }
            if (raw && enable_audio34)
            {
                write_audio(fp_audio34, (uint8_t *)p_audio34, 1920*2, raw_audio_bits, true);
            }


            // Write OP-Atom MXF files

            // Buffers for audio de-interleaving
            int16_t a0[audio_samples_per_frame];
            int16_t a1[audio_samples_per_frame];

            // Write MXF video
            if (mxf && enable_video)
            {
                try
                {
                    if (UNC_MATERIAL_RESOLUTION == resolution)
                    {
                        // write uncompressed video data to input track 1
                        writer->writeSample(1, 1, (uint8_t *)p_422_video, size_422_video);
                    }
                    else
                    {
                        // write encoded video data to input track 1
                        writer->writeSample(1, 1, p_enc_video, size_enc_video);
                    }
                }
                catch (const prodauto::MXFWriterException & e)
                {
                    ACE_DEBUG((LM_ERROR, ACE_TEXT("MXFWriterException: %C\n"), e.getMessage().c_str()));
                    p_rec->NoteFailure();
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
                catch (const prodauto::MXFWriterException & e)
                {
                    ACE_DEBUG((LM_ERROR, ACE_TEXT("MXFWriterException: %C\n"), e.getMessage().c_str()));
                    p_rec->NoteFailure();
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
                catch (const prodauto::MXFWriterException & e)
                {
                    ACE_DEBUG((LM_ERROR, ACE_TEXT("MXFWriterException: %C\n"), e.getMessage().c_str()));
                    p_rec->NoteFailure();
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
            framecount_t target = p_rec->TargetDuration();
            framecount_t written = p_opt->FramesWritten();
            framecount_t dropped = p_opt->FramesDropped();
            framecount_t total = written + dropped;
            if (target > 0 && total >= target)
            {
                ACE_DEBUG((LM_INFO, ACE_TEXT("  %C index %d duration %d reached (total=%d written=%d dropped=%d)\n"),
                            src_name, p_opt->index, target, total, written, dropped));
                finished_record = true;
                break;
            }
        }

        last_saved = lastframe;

        if (finished_record)
        {
            break;
        }
    }
    // ************************
    // End of main record loop
    // ************************

    framecount_t last_saved_tc = IngexShm::Instance()->Timecode(card_i, last_saved);
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C last_saved=%d tc=%C\n"),
        src_name, last_saved, Timecode(last_saved_tc).Text()));


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
    if (enc_av)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        if (ffmpeg_encoder_av_close(enc_av) != 0)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C: dvd_encoder_close() failed\n"), src_name));
        }
    }

    // shutdown ffmpeg encoder
    if (ffmpeg_encoder)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        if (ffmpeg_encoder_close(ffmpeg_encoder) != 0)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C: ffmpeg_encoder_close() failed\n"), src_name));
        }
    }

    // shutdown MJPEG encoder
    if (mj_encoder)
    {
        mjpeg_compress_free(mj_encoder);
    }

    // cleanup timecode overlay
    if (tco)
    {
        tc_overlay_close(tco);
    }
    delete [] tc_overlay_buffer;

    // cleanup quad buffer
    delete [] p_quadvideo;


    // Complete MXF writing and save packages to database
    if (mxf)
    {
        try
        {
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C record thread %d completing MXF save\n"), src_name, p_opt->index));
            writer->completeAndSaveToDatabase();
        }
        catch (const prodauto::DBException & e)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database exception: %C\n"), e.getMessage().c_str()));
            p_rec->NoteFailure();
        }
        catch (const prodauto::MXFWriterException & e)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("MXFWriterException: %C\n"), e.getMessage().c_str()));
            p_rec->NoteFailure();
        }
    }


    ACE_DEBUG((LM_INFO, ACE_TEXT("%C record thread %d exiting\n"), src_name, p_opt->index));
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
    std::vector<ThreadParam>::iterator it;
    for (it = p->mThreadParams.begin(); it != p->mThreadParams.end(); ++it)
    {
        if (it->id)
        {
            int err = ACE_Thread::join(it->id, 0, 0);
            if (0 == err)
            {
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("Joined record thread %x\n"), *it));
            }
            else
            {
                ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to join record thread %x: %s\n"), *it, strerror(err)));
            }
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



