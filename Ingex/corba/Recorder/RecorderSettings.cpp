/*
 * $Id: RecorderSettings.cpp,v 1.1 2006/12/20 12:28:26 john_f Exp $
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

const bool MXF = false; // Can't do MXF without database.
const bool RAW = true;
const bool QUAD_MPEG2 = false;
const bool BROWSE_AUDIO = true;

const prodauto::Rational IMAGE_ASPECT = { 4, 3 };
const int MXF_RESOLUTION = DV50_MATERIAL_RESOLUTION;
const int MPEG2_BITRATE = 5000;
const int RAW_AUDIO_BITS = 32;
const int MXF_AUDIO_BITS = 16;
const int BROWSE_AUDIO_BITS = 16;

static const char * RAW_DIR = "/video/raw/";
static const char * MXF_DIR = "/video/mxf/";
static const char * MXF_SUBDIR_CREATING = "Creating/";
static const char * MXF_SUBDIR_FAILURES = "Failures/";
static const char * BROWSE_DIR = "/video/browse/";

static const char * COPY_COMMAND = "";

const int TIMECODE_MODE = LTC_PARAMETER_VALUE;


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

    int timecode_mode;
    if (rc)
    {
        mxf = rc->getBoolParam("MXF", MXF);
        raw = rc->getBoolParam("RAW", RAW);
        quad_mpeg2 = rc->getBoolParam("QUAD_MPEG2", QUAD_MPEG2);
        browse_audio = rc->getBoolParam("BROWSE_AUDIO", BROWSE_AUDIO);

        image_aspect = rc->getRationalParam("IMAGE_ASPECT", IMAGE_ASPECT);
        mxf_resolution = rc->getIntParam("MXF_RESOLUTION", MXF_RESOLUTION);
        mpeg2_bitrate = rc->getIntParam("MPEG2_BITRATE", MPEG2_BITRATE);
        raw_audio_bits = rc->getIntParam("RAW_AUDIO_BITS", RAW_AUDIO_BITS);
        mxf_audio_bits = rc->getIntParam("MXF_AUDIO_BITS", MXF_AUDIO_BITS);

        raw_dir = rc->getStringParam("RAW_DIR", RAW_DIR);
        mxf_dir = rc->getStringParam("MXF_DIR", MXF_DIR);
        mxf_subdir_creating = rc->getStringParam("MXF_SUBDIR_CREATING", MXF_SUBDIR_CREATING);
        mxf_subdir_failures = rc->getStringParam("MXF_SUBDIR_FAILURES", MXF_SUBDIR_FAILURES);
        browse_dir = rc->getStringParam("BROWSE_DIR", BROWSE_DIR);
        copy_command = rc->getStringParam("COPY_COMMAND", COPY_COMMAND);

        timecode_mode = rc->getIntParam("TIMECODE_MODE", TIMECODE_MODE);
    }
    else
    {
        mxf = MXF;
        raw = RAW;
        quad_mpeg2 = QUAD_MPEG2;
        browse_audio = BROWSE_AUDIO;

        image_aspect = IMAGE_ASPECT;
        mxf_resolution = MXF_RESOLUTION;
        mpeg2_bitrate = MPEG2_BITRATE;
        raw_audio_bits = RAW_AUDIO_BITS;
        mxf_audio_bits = MXF_AUDIO_BITS;

        raw_dir = RAW_DIR;
        mxf_dir = MXF_DIR;
        mxf_subdir_creating = MXF_SUBDIR_CREATING;
        mxf_subdir_failures = MXF_SUBDIR_FAILURES;
        browse_dir = BROWSE_DIR;
        copy_command = COPY_COMMAND;

        timecode_mode = TIMECODE_MODE;
    }

    switch (timecode_mode)
    {
    case LTC_PARAMETER_VALUE:
        tc_mode = IngexShm::LTC;
	break;
    case VITC_PARAMETER_VALUE:
        tc_mode = IngexShm::VITC;
	break;
    default:
	ACE_DEBUG((LM_ERROR, ACE_TEXT("Unexpected timecode mode\n")));
	break;
    }

    delete rec;

    return true;
}

