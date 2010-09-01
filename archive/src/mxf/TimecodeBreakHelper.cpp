/*
 * $Id: TimecodeBreakHelper.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#include <memory>

#include "TimecodeBreakHelper.h"
#include "MXFEvents.h"
#include "RecorderException.h"
#include "Logging.h"

using namespace std;
using namespace rec;



// maximum number of timecode breaks recorded in the file
#define MAX_TIMECODE_BREAKS     2000



TimecodeBreakHelper::TimecodeBreakHelper()
{
    mStartVITC = 0;
    mStartLTC = 0;
    memset(&mVITCSegment, 0, sizeof(mVITCSegment));
    mVITCSegment.start_position = -1;
    memset(&mLTCSegment, 0, sizeof(mLTCSegment));
    mLTCSegment.start_position = -1;
}

TimecodeBreakHelper::~TimecodeBreakHelper()
{
}

void TimecodeBreakHelper::ProcessTimecode(bool have_ltc, int64_t ltc, bool have_vitc, int64_t vitc)
{
    if (mTimecodeBreaks.size() >= MAX_TIMECODE_BREAKS)
        return;

    ProcessTimecode(mLTCSegment, mStartLTC, TIMECODE_BREAK_LTC, have_ltc, ltc);
    ProcessTimecode(mVITCSegment, mStartVITC, TIMECODE_BREAK_VITC, have_vitc, vitc);
}

void TimecodeBreakHelper::WriteTimecodeBreakEvents(MXFEventFileWriter *event_file)
{
    if (mTimecodeBreaks.empty())
        return;

    TimecodeBreakFrameworkEvent::Register(event_file);

    event_file->StartNewEventTrack(TimecodeBreakFrameworkEvent::GetTrackId(),
                                   TimecodeBreakFrameworkEvent::GetTrackName());

    TimecodeBreakFrameworkEvent *event = TimecodeBreakFrameworkEvent::Create(event_file);

    size_t i;
    for (i = 0; i < mTimecodeBreaks.size(); i++) {
        if (i != 0)
            event->CreateNewInstance();

        event->SetTimecodeType(mTimecodeBreaks[i].timecodeType);
        event_file->AddEvent(mTimecodeBreaks[i].position, 1, event);
    }
}

int64_t TimecodeBreakHelper::GetStartTimecode(PrimaryTimecode primary_timecode)
{
    switch (primary_timecode)
    {
        case PRIMARY_TIMECODE_AUTO:
            if (mTimecodeBreaks.empty() || mTimecodeBreaks[0].position > 0 ||
                !(mTimecodeBreaks[0].timecodeType & TIMECODE_BREAK_LTC))
            {
                return (mStartLTC >= 0 ? mStartLTC : 0);
            }
            else if (!(mTimecodeBreaks[0].timecodeType & TIMECODE_BREAK_VITC))
            {
                return (mStartVITC >= 0 ? mStartVITC : 0);
            }
            else
            {
                return 0;
            }
            break;
        case PRIMARY_TIMECODE_LTC:
            return (mStartLTC >= 0 ? mStartLTC : 0);
            break;
        case PRIMARY_TIMECODE_VITC:
            return (mStartVITC >= 0 ? mStartVITC : 0);
            break;
    }

    return 0; // for the compiler
}

void TimecodeBreakHelper::ProcessTimecode(TimecodeSegment &segment, int64_t &start_timecode, uint16_t timecode_type,
                                          bool have_timecode, int64_t timecode)
{
    // first timecode
    if (segment.start_position < 0) {
        if (!have_timecode)
            AddBreak(0, timecode_type);

        start_timecode = have_timecode ? timecode : -2;

        segment.start_position = 0;
        segment.duration = 1;
        segment.last_timecode = have_timecode ? timecode : -2;
        segment.broken_timecodes = !have_timecode;

        return;
    }

    // non-broken timecode found after >1 broken timecodes
    if (segment.broken_timecodes && segment.duration > 1 &&
        have_timecode && segment.last_timecode + 1 == timecode)
    {
        // non-broken timecode starts with the previous timecode

        AddBreak(segment.start_position + segment.duration - 1, timecode_type);

        segment.start_position = segment.start_position + segment.duration - 1;
        segment.duration = 2;
        segment.last_timecode = timecode;
        segment.broken_timecodes = false;

        return;
    }

    // broken timecode found after 1 or more non-broken timecodes
    if (!segment.broken_timecodes &&
        (!have_timecode || segment.last_timecode + 1 != timecode))
    {
        AddBreak(segment.start_position + segment.duration, timecode_type);

        segment.start_position = segment.start_position + segment.duration;
        segment.duration = 1;
        segment.last_timecode = have_timecode ? timecode : -2;
        segment.broken_timecodes = true;

        return;
    }

    segment.duration++;
    segment.last_timecode = have_timecode ? timecode : -2;
}

void TimecodeBreakHelper::AddBreak(int64_t position, uint16_t timecode_type)
{
    if (mTimecodeBreaks.size() >= 2 && mTimecodeBreaks[mTimecodeBreaks.size() - 2].position == position) {
        mTimecodeBreaks[mTimecodeBreaks.size() - 2].timecodeType |= timecode_type;
    } else if (mTimecodeBreaks.size() >= 1 && mTimecodeBreaks.back().position == position) {
        mTimecodeBreaks.back().timecodeType |= timecode_type;
    } else if (mTimecodeBreaks.size() >= 1 && mTimecodeBreaks.back().position > position) {
        TimecodeBreak last_break = mTimecodeBreaks.back();
        
        mTimecodeBreaks.back().position = position;
        mTimecodeBreaks.back().timecodeType = timecode_type;

        mTimecodeBreaks.push_back(last_break);
    } else {
        TimecodeBreak timecode_break;
        timecode_break.position = position;
        timecode_break.timecodeType = timecode_type;
        mTimecodeBreaks.push_back(timecode_break);
    }
}

