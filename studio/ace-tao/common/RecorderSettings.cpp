/*
 * $Id: RecorderSettings.cpp,v 1.3 2008/04/18 16:03:29 john_f Exp $
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
const bool          ENCODE1_BITC        = false;
const char * const  ENCODE1_DIR         = "/video";
const char * const  ENCODE1_DEST        = "/store";

const int           ENCODE2_RESOLUTION  = 0;
const int           ENCODE2_WRAPPING    = UNSPECIFIED_FILE_FORMAT_TYPE;
const bool          ENCODE2_BITC        = false;
const char * const  ENCODE2_DIR         = "/video";
const char * const  ENCODE2_DEST        = "/store";

const int           QUAD_RESOLUTION  = 0;
const int           QUAD_WRAPPING    = UNSPECIFIED_FILE_FORMAT_TYPE;
const bool          QUAD_BITC        = true;
const char * const  QUAD_DIR         = "/video";
const char * const  QUAD_DEST        = "";

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
*/
bool RecorderSettings::Update(prodauto::Recorder * rec)
{
    // NB. Do we need a freah instance of rec here to
    // catch any changes/
    prodauto::RecorderConfig * rc = 0;
    if (rec && rec->hasConfig())
    {
        rc = rec->getConfig();
    }

    int encode1_resolution;
    int encode1_wrapping;
    bool encode1_bitc;
    std::string encode1_dir;
    std::string encode1_dest;

    int encode2_resolution;
    int encode2_wrapping;
    bool encode2_bitc;
    std::string encode2_dir;
    std::string encode2_dest;

    int quad_resolution;
    int quad_wrapping;
    bool quad_bitc;
    std::string quad_dir;
    std::string quad_dest;

    if (rc)
    {
        image_aspect = rc->getRationalParam("IMAGE_ASPECT", IMAGE_ASPECT);
        timecode_mode = rc->getIntParam("TIMECODE_MODE", TIMECODE_MODE);
        copy_command = rc->getStringParam("COPY_COMMAND", COPY_COMMAND);

        encode1_resolution = rc->getIntParam("ENCODE1_RESOLUTION", ENCODE1_RESOLUTION);
        encode1_wrapping = rc->getIntParam("ENCODE1_WRAPPING", ENCODE1_WRAPPING);
        encode1_bitc = rc->getBoolParam("ENCODE1_BITC", ENCODE1_BITC);
        encode1_dir = rc->getStringParam("ENCODE1_DIR", ENCODE1_DIR);
        encode1_dest = rc->getStringParam("ENCODE1_DEST", ENCODE1_DEST);

        encode2_resolution = rc->getIntParam("ENCODE2_RESOLUTION", ENCODE2_RESOLUTION);
        encode2_wrapping = rc->getIntParam("ENCODE2_WRAPPING", ENCODE2_WRAPPING);
        encode2_bitc = rc->getBoolParam("ENCODE2_BITC", ENCODE2_BITC);
        encode2_dir = rc->getStringParam("ENCODE2_DIR", ENCODE2_DIR);
        encode2_dest = rc->getStringParam("ENCODE2_DEST", ENCODE2_DEST);

        quad_resolution = rc->getIntParam("QUAD_RESOLUTION", QUAD_RESOLUTION);
        quad_wrapping = rc->getIntParam("QUAD_WRAPPING", QUAD_WRAPPING);
        quad_bitc = rc->getBoolParam("QUAD_BITC", QUAD_BITC);
        quad_dir = rc->getStringParam("QUAD_DIR", QUAD_DIR);
        quad_dest = rc->getStringParam("QUAD_DEST", QUAD_DEST);
    }
    else
    {
        image_aspect = IMAGE_ASPECT;
        timecode_mode = TIMECODE_MODE;
        copy_command = COPY_COMMAND;

        encode1_resolution = ENCODE1_RESOLUTION;
        encode1_wrapping = ENCODE1_WRAPPING;
        encode1_bitc = ENCODE1_BITC;
        encode1_dir = ENCODE1_DIR;
        encode1_dest = ENCODE1_DEST;

        encode2_resolution = ENCODE2_RESOLUTION;
        encode2_wrapping = ENCODE2_WRAPPING;
        encode2_bitc = ENCODE2_BITC;
        encode2_dir = ENCODE2_DIR;
        encode2_dest = ENCODE2_DEST;

        quad_resolution = QUAD_RESOLUTION;
        quad_wrapping = QUAD_WRAPPING;
        quad_bitc = ENCODE2_BITC;
        quad_dir = QUAD_DIR;
        quad_dest = QUAD_DEST;
    }

    encodings.clear();
    if (encode1_resolution)
    {
        EncodeParams ep;
        ep.resolution = encode1_resolution;
        ep.wrapping = (encode1_wrapping == MXF_FILE_FORMAT_TYPE ? Wrapping::MXF : Wrapping::NONE);
        ep.source = Input::NORMAL;
        ep.bitc = encode1_bitc;
        ep.dir = encode1_dir;
        ep.dest = encode1_dest;
        encodings.push_back(ep);
    }
    if (encode2_resolution)
    {
        EncodeParams ep;
        ep.resolution = encode2_resolution;
        ep.wrapping = (encode2_wrapping == MXF_FILE_FORMAT_TYPE ? Wrapping::MXF : Wrapping::NONE);
        ep.source = Input::NORMAL;
        ep.bitc = encode2_bitc;
        ep.dir = encode2_dir;
        ep.dest = encode2_dest;
        encodings.push_back(ep);
    }
    if (quad_resolution)
    {
        EncodeParams ep;
        ep.resolution = quad_resolution;
        ep.wrapping = (quad_wrapping == MXF_FILE_FORMAT_TYPE ? Wrapping::MXF : Wrapping::NONE);
        ep.source = Input::QUAD;
        ep.bitc = quad_bitc;
        ep.dir = quad_dir;
        ep.dest = quad_dest;
        encodings.push_back(ep);
    }
    if (true && quad_resolution) // bodge for browse tracks
    {
        EncodeParams ep;
        ep.resolution = quad_resolution;
        ep.wrapping = (quad_wrapping == MXF_FILE_FORMAT_TYPE ? Wrapping::MXF : Wrapping::NONE);
        ep.source = Input::NORMAL;
        ep.bitc = quad_bitc;
        ep.dir = quad_dir;
        ep.dest = quad_dest;
        encodings.push_back(ep);
    }

    return true;
}

