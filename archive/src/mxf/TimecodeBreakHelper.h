/*
 * $Id: TimecodeBreakHelper.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#ifndef __RECORDER_TIMECODE_BREAK_HELPER_H__
#define __RECORDER_TIMECODE_BREAK_HELPER_H__


#include <vector>

#include <archive_types.h>

#include "MXFEventFileWriter.h"
#include "Types.h"



namespace rec
{


class TimecodeBreakHelper
{
public:
    typedef struct
    {
        int64_t start_position;
        int64_t duration;
        int64_t last_timecode;
        bool broken_timecodes;
    } TimecodeSegment;

public:
    TimecodeBreakHelper();
    virtual ~TimecodeBreakHelper();

    void ProcessTimecode(bool have_ltc, int64_t ltc, bool have_vitc, int64_t vitc);

    void WriteTimecodeBreakEvents(MXFEventFileWriter *event_file);

    int64_t GetStartTimecode(PrimaryTimecode primary_timecode);

    std::vector<TimecodeBreak>& GetBreaks() { return mTimecodeBreaks; }

private:
    void ProcessTimecode(TimecodeSegment &segment, int64_t &start_timecode, uint16_t timecode_type,
                         bool have_timecode, int64_t timecode);
    void AddBreak(int64_t position, uint16_t timecode_type);

private:
    TimecodeSegment mVITCSegment;
    TimecodeSegment mLTCSegment;
    int64_t mStartVITC;
    int64_t mStartLTC;
    std::vector<TimecodeBreak> mTimecodeBreaks;
};



};


#endif
