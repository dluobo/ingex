/*
 * $Id: RecorderSettings.cpp,v 1.2 2007/10/26 15:44:20 john_f Exp $
 *
 * Recorder Configuration.
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
#include "Database.h"
#include "RecorderSettings.h"

// These are the default values that will be used if
// config cannot be read from the database.

const prodauto::Rational    IMAGE_ASPECT    = { 4, 3 };
const int                   TIMECODE_MODE   = LTC_PARAMETER_VALUE;
const char * const          COPY_COMMAND    = "";

const int           ENCODE1_RESOLUTION  = DV50_MATERIAL_RESOLUTION;
const int           ENCODE1_WRAPPING    = UNSPECIFIED_FILE_FORMAT_TYPE;
const char * const  ENCODE1_DIR         = "/video";

const int           ENCODE2_RESOLUTION  = 0;
const int           ENCODE2_WRAPPING    = UNSPECIFIED_FILE_FORMAT_TYPE;
const char * const  ENCODE2_DIR         = "/video";

const int           QUAD_RESOLUTION  = 0;
const int           QUAD_WRAPPING    = UNSPECIFIED_FILE_FORMAT_TYPE;
const char * const  QUAD_DIR         = "/video";

const bool BROWSE_AUDIO = false;

const int MPEG2_BITRATE = 5000;
const int RAW_AUDIO_BITS = 32;
const int MXF_AUDIO_BITS = 16;
const int BROWSE_AUDIO_BITS = 16;

const char * const MXF_SUBDIR_CREATING = "Creating/";
const char * const MXF_SUBDIR_FAILURES = "Failures/";



// static instance pointer
RecorderSettings * RecorderSettings::mInstance = 0;

// Singelton access
RecorderSettings * RecorderSettings::Instance()
{
    if (mInstance == 0)  // is it the first call?
    {  
        mInstance = new RecorderSettings; // create sole instance
    }
    return mInstance; // address of sole instance
}

// Constructor
RecorderSettings::RecorderSettings() :
    image_aspect(IMAGE_ASPECT),
    timecode_mode(TIMECODE_MODE),
    copy_command(COPY_COMMAND),
    browse_audio(BROWSE_AUDIO),
    mpeg2_bitrate(MPEG2_BITRATE),
    raw_audio_bits(RAW_AUDIO_BITS),
    mxf_audio_bits(MXF_AUDIO_BITS),
    browse_audio_bits(BROWSE_AUDIO_BITS),
    mxf_subdir_creating(MXF_SUBDIR_CREATING),
    mxf_subdir_failures(MXF_SUBDIR_FAILURES)
{
}

/**
Update the configuration parameters, for example from a database.
Currently, the parameters are hard-coded.
*/
bool RecorderSettings::Update(const std::string & name)
{
    prodauto::Database * db = 0;
    prodauto::Recorder * rec = 0;
    try
    {
        db = prodauto::Database::getInstance();
        rec = db->loadRecorder(name);
    }
    catch(...)
    {
    }

    prodauto::RecorderConfig * rc = 0;
    if (rec && rec->hasConfig())
    {
        rc = rec->getConfig();
    }

    int encode1_resolution;
    int encode1_wrapping;
    std::string encode1_dir;
    int encode2_resolution;
    int encode2_wrapping;
    std::string encode2_dir;
    int quad_resolution;
    int quad_wrapping;
    std::string quad_dir;

    if (rc)
    {
        image_aspect = rc->getRationalParam("IMAGE_ASPECT", IMAGE_ASPECT);
        timecode_mode = rc->getIntParam("TIMECODE_MODE", TIMECODE_MODE);
        copy_command = rc->getStringParam("COPY_COMMAND", COPY_COMMAND);

        encode1_resolution = rc->getIntParam("ENCODE1_RESOLUTION", ENCODE1_RESOLUTION);
        encode1_wrapping = rc->getIntParam("ENCODE1_WRAPPING", ENCODE1_WRAPPING);
        encode1_dir = rc->getStringParam("ENCODE1_DIR", ENCODE1_DIR);

        encode2_resolution = rc->getIntParam("ENCODE2_RESOLUTION", ENCODE2_RESOLUTION);
        encode2_wrapping = rc->getIntParam("ENCODE2_WRAPPING", ENCODE2_WRAPPING);
        encode2_dir = rc->getStringParam("ENCODE2_DIR", ENCODE2_DIR);

        quad_resolution = rc->getIntParam("QUAD_RESOLUTION", QUAD_RESOLUTION);
        quad_wrapping = rc->getIntParam("QUAD_WRAPPING", QUAD_WRAPPING);
        quad_dir = rc->getStringParam("QUAD_DIR", QUAD_DIR);

#if 0
        mxf = rc->getBoolParam("MXF", MXF);
        raw = rc->getBoolParam("RAW", RAW);
        quad = rc->getBoolParam("QUAD", QUAD);
        browse_audio = rc->getBoolParam("BROWSE_AUDIO", BROWSE_AUDIO);

        mpeg2_bitrate = rc->getIntParam("MPEG2_BITRATE", MPEG2_BITRATE);
        raw_audio_bits = rc->getIntParam("RAW_AUDIO_BITS", RAW_AUDIO_BITS);
        mxf_audio_bits = rc->getIntParam("MXF_AUDIO_BITS", MXF_AUDIO_BITS);

        raw_dir = rc->getStringParam("RAW_DIR", RAW_DIR);
        mxf_subdir_creating = rc->getStringParam("MXF_SUBDIR_CREATING", MXF_SUBDIR_CREATING);
        mxf_subdir_failures = rc->getStringParam("MXF_SUBDIR_FAILURES", MXF_SUBDIR_FAILURES);
        browse_dir = rc->getStringParam("BROWSE_DIR", BROWSE_DIR);
#endif
    }
    else
    {
        image_aspect = IMAGE_ASPECT;
        timecode_mode = TIMECODE_MODE;
        copy_command = COPY_COMMAND;

        encode1_resolution = ENCODE1_RESOLUTION;
        encode1_wrapping = ENCODE1_WRAPPING;
        encode1_dir = ENCODE1_DIR;

        encode2_resolution = ENCODE2_RESOLUTION;
        encode2_wrapping = ENCODE2_WRAPPING;
        encode2_dir = ENCODE2_DIR;

        quad_resolution = QUAD_RESOLUTION;
        quad_wrapping = QUAD_WRAPPING;
        quad_dir = QUAD_DIR;

#if 0
        mxf = MXF;
        raw = RAW;
        quad = QUAD;
        browse_audio = BROWSE_AUDIO;

        mpeg2_bitrate = MPEG2_BITRATE;
        raw_audio_bits = RAW_AUDIO_BITS;
        mxf_audio_bits = MXF_AUDIO_BITS;

        raw_dir = RAW_DIR;
        mxf_subdir_creating = MXF_SUBDIR_CREATING;
        mxf_subdir_failures = MXF_SUBDIR_FAILURES;
        browse_dir = BROWSE_DIR;
#endif
    }

    encodings.clear();
    if (encode1_resolution)
    {
        EncodeParams ep;
        ep.resolution = encode1_resolution;
        ep.wrapping = (encode1_wrapping == MXF_FILE_FORMAT_TYPE ? Wrapping::MXF : Wrapping::NONE);
        ep.source = Input::NORMAL;
        ep.dir = encode1_dir;
        ep.bitc = false;
        encodings.push_back(ep);
    }
    if (encode2_resolution)
    {
        EncodeParams ep;
        ep.resolution = encode2_resolution;
        ep.wrapping = (encode2_wrapping == MXF_FILE_FORMAT_TYPE ? Wrapping::MXF : Wrapping::NONE);
        ep.source = Input::NORMAL;
        ep.dir = encode2_dir;
        ep.bitc = false;
        encodings.push_back(ep);
    }
    if (quad_resolution)
    {
        EncodeParams ep;
        ep.resolution = quad_resolution;
        ep.wrapping = (quad_wrapping == MXF_FILE_FORMAT_TYPE ? Wrapping::MXF : Wrapping::NONE);
        ep.source = Input::QUAD;
        ep.dir = quad_dir;
        ep.bitc = true;
        encodings.push_back(ep);
    }

    delete rec;

    return true;
}

