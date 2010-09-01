/*
 * $Id: MXFEventFile.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
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

#ifndef __RECORDER_MXF_EVENT_FILE_H__
#define __RECORDER_MXF_EVENT_FILE_H__

#include <string>

#include <archive_types.h>



namespace rec
{


class MXFEventFile
{
public:
    MXFEventFile();
    ~MXFEventFile();
    
    bool Read(std::string filename);

    long GetPSEFailures(PSEFailure **failures) const { *failures = mPSEFailures; return mNumPSEFailures; }
    long GetVTRErrors(VTRErrorAtPos **errors) const { *errors = mVTRErrors; return mNumVTRErrors; }
    long GetDigiBetaDropouts(DigiBetaDropout **digiBetaDropouts) const { *digiBetaDropouts = mDigiBetaDropouts; return mNumDigiBetaDropouts; }
    long GetTimecodeBreaks(TimecodeBreak **timecodebreaks) const { *timecodebreaks = mTimecodeBreaks; return mNumTimecodeBreaks; }
    
private:
    PSEFailure *mPSEFailures;
    long mNumPSEFailures;
    VTRErrorAtPos *mVTRErrors;
    long mNumVTRErrors;
    DigiBetaDropout *mDigiBetaDropouts;
    long mNumDigiBetaDropouts;
    TimecodeBreak *mTimecodeBreaks;
    long mNumTimecodeBreaks;
};



};


#endif
