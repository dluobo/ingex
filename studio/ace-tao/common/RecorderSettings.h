/*
 * $Id: RecorderSettings.h,v 1.1 2007/09/11 14:08:33 stuart_hc Exp $
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

#ifndef RecorderSettings_h
#define RecorderSettings_h

#include <string>

#include "Database.h"


/**
Holds configuration parameters for the Recorder.
*/
class RecorderSettings
{
public:
    static RecorderSettings * Instance();

    // Methods
    bool Update(const std::string & name);

    // Record Parameters
    bool raw;
    bool mxf;
    bool quad_mpeg2;
    bool browse_audio;

    prodauto::Rational image_aspect;
    int mxf_resolution;
    int mpeg2_bitrate;
    int raw_audio_bits;
    int mxf_audio_bits;
    int browse_audio_bits;

    std::string raw_dir;
    std::string mxf_dir;
    std::string mxf_subdir_creating;
    std::string mxf_subdir_failures;
    std::string browse_dir;
    std::string copy_command;

    int timecode_mode;

private:
    static RecorderSettings * mInstance;
};

#endif //#ifndef RecorderSettings_h

