/*
 * $Id: Timecode.cpp,v 1.9 2010/06/14 15:26:51 john_f Exp $
 *
 * Class to hold a Timecode
 *
 * Copyright (C) 2006 - 2010  British Broadcasting Corporation.
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
#include <cstring>
#include <cstdio>
#include <inttypes.h>
#include "Timecode.h"

// constants for 30 fps drop frame
const unsigned int DF_FRAMES_PER_MINUTE       = 60 * 30 - 2;                    // 1798
const unsigned int DF_FRAMES_PER_TEN_MINUTE   = 10 * DF_FRAMES_PER_MINUTE + 2;  // 17982
const unsigned int DF_FRAMES_PER_HOUR         = 6 * DF_FRAMES_PER_TEN_MINUTE;   // 107892

using namespace Ingex;

// default constructor initialises null timecode
Timecode::Timecode()
: mIsNull(true)
{
    UpdateText();
}


// constructor from frame count
Timecode::Timecode(int frames_since_midnight, int fps_num, int fps_den, bool df)
: mFramesSinceMidnight(frames_since_midnight), mFrameRateNumerator(fps_num), mFrameRateDenominator(fps_den), mDropFrame(df), mIsNull(false)
{
    //fprintf(stderr, "frames_since_midnight %d, fps_num %d, fps_den %d, %s\n", frames_since_midnight, fps_num, fps_den, df ? "DF" : "NDF");

    // Guard against garbage data
    if (0 == mFrameRateNumerator || 0 == mFrameRateDenominator)
    {
        mIsNull = true;
    }

    // Drop frame only supported for 30000/1001 fps
    if (df && fps_num != 30000)
    {
        mDropFrame = false;
    }

    UpdateHoursMinsEtc();
    UpdateText();
}

// constructor from hrs, mins, secs, frames
Timecode::Timecode(int hr, int min, int sec, int frame, int fps_num, int fps_den, bool df)
: mFrameRateNumerator(fps_num), mFrameRateDenominator(fps_den), mDropFrame(df),  mIsNull(false),
  mHours(hr), mMinutes(min), mSeconds(sec), mFrames(frame)
{
    // Guard against garbage data
    if (0 == mFrameRateNumerator || 0 == mFrameRateDenominator)
    {
        mIsNull = true;
    }

    // Drop frame only supported for 30000/1001 fps
    if (df && fps_num != 30000)
    {
        mDropFrame = false;
    }

    UpdateFramesSinceMidnight();
    UpdateText();
}

// constructor - from text
Timecode::Timecode(const char * s, int fps_num, int fps_den, bool df)
: mFrameRateNumerator(fps_num), mFrameRateDenominator(fps_den), mDropFrame(df), mIsNull(false)
{
    // Guard against garbage data
    if (0 == mFrameRateNumerator || 0 == mFrameRateDenominator)
    {
        mIsNull = true;
    }

    // Drop frame only supported for 30000/1001 frame rate
    if (mDropFrame && mFrameRateNumerator != 30000)
    {
        mDropFrame = false;
    }

    bool ok = false;
    if (11 == strlen(s))
    {
        if (4 == sscanf(s,"%2d:%2d:%2d:%2d", &mHours, &mMinutes, &mSeconds, &mFrames))
        {
            ok = true;
        }
        else if (4 == sscanf(s,"%2d:%2d:%2d.%2d", &mHours, &mMinutes, &mSeconds, &mFrames))
        {
            mDropFrame = true;
            ok = true;
        }
        else if (4 == sscanf(s,"%2d:%2d:%2d;%2d", &mHours, &mMinutes, &mSeconds, &mFrames))
        {
            mDropFrame = true;
            ok = true;
        }
    }
    else if (8 == strlen(s))
    {
        if (3 == sscanf(s,"%2d:%2d:%2d", &mHours, &mMinutes, &mSeconds))
        {
            mFrames = 0;
            ok = true;
        }
    }

    mIsNull = !ok;

    UpdateFramesSinceMidnight();
    UpdateText();
}

// Copy constructor
Timecode::Timecode(const Timecode & tc)
{
    mFrameRateNumerator = tc.mFrameRateNumerator;
    mFrameRateDenominator = tc.mFrameRateDenominator;
    mDropFrame = tc.mDropFrame;
    mIsNull = tc.mIsNull;
    mFramesSinceMidnight = tc.mFramesSinceMidnight;
    mHours = tc.mHours;
    mMinutes = tc.mMinutes;
    mSeconds = tc.mSeconds;
    mFrames = tc.mFrames;

    UpdateText();
}

Timecode & Timecode::operator=(const Timecode & tc)
{
    if (this != &tc)
    {
        mFrameRateNumerator = tc.mFrameRateNumerator;
        mFrameRateDenominator = tc.mFrameRateDenominator;
        mDropFrame = tc.mDropFrame;
        mIsNull = tc.mIsNull;
        mFramesSinceMidnight = tc.mFramesSinceMidnight;
        mHours = tc.mHours;
        mMinutes = tc.mMinutes;
        mSeconds = tc.mSeconds;
        mFrames = tc.mFrames;
        UpdateText();
    }
    return *this;
}

/*
Timecode & Timecode::operator=(int frames_since_midnight)
{
    // Guard against garbage data
    if (frames_since_midnight < 0)
    {
        mIsNull = true;
    }
    else
    {
        mFramesSinceMidnight = frames_since_midnight;
        mIsNull = false;
    }
    UpdateHoursMinsEtc();
    UpdateText();
    return *this;
}
*/

void Timecode::UpdateHoursMinsEtc()
{
    if (!mIsNull)
    {
        // Make sure frames in valid range
        const int64_t frames_per_day = 24 * 60 * 60 * (int64_t)mFrameRateNumerator / mFrameRateDenominator;
        if (mFramesSinceMidnight < 0)
        {
            mFramesSinceMidnight += frames_per_day;
        }
        mFramesSinceMidnight %= frames_per_day;

        int frames = mFramesSinceMidnight;
        int nominal_fps = mFrameRateNumerator / mFrameRateDenominator
            + (mFrameRateNumerator % mFrameRateDenominator ? 1 : 0);

        if (!mDropFrame)
        {
            mHours = frames / (nominal_fps * 60 * 60);
            frames %= (nominal_fps * 60 * 60);
            mMinutes = frames / (nominal_fps * 60);
            frames %= (nominal_fps * 60);
            mSeconds = frames / nominal_fps;
            mFrames = frames % nominal_fps;
        }
        else
        {
            // Assuming 30000/1001 frame rate here
            mHours = frames / DF_FRAMES_PER_HOUR;
            frames %= DF_FRAMES_PER_HOUR;

            int ten_minute = frames / DF_FRAMES_PER_TEN_MINUTE;
            frames %= DF_FRAMES_PER_TEN_MINUTE;

            // must adjust frame count to make minutes calculation work
            // calculations from here on in just assume that within the 10 minute cycle
            // there are only DF_FRAMES_PER_MINUTE (1798) frames per minute - even for the first minute
            // in the ten minute cycle. Hence we decrement the frame count by 2 to get the minutes count
            // So for the first two frames of the ten minute cycle we get a negative frames number

            frames -= 2;

            int unit_minute = frames / DF_FRAMES_PER_MINUTE;
            frames %= DF_FRAMES_PER_MINUTE;
            mMinutes = ten_minute * 10 + unit_minute;

            // frames now contains frame in minute @ 1798 frames per minute
            // put the 2 frame adjustment back in to get the correct frame count
            // For the first two frames of the ten minute cycle, frames is negative and this
            // adjustment makes it non-negative. For other minutes in the cycle the frames count
            // goes from 0 upwards, thus this adjusment gives the required 2 frame offset

            frames += 2;

            mSeconds = frames / nominal_fps;
            mFrames = frames % nominal_fps;
        }

        // Prevent hours exceeding one day
        mHours %= 24;
    }
}

void Timecode::UpdateFramesSinceMidnight()
{
    if (!mIsNull)
    {
        int nominal_fps = mFrameRateNumerator / mFrameRateDenominator
            + (mFrameRateNumerator % mFrameRateDenominator ? 1 : 0);

        if (!mDropFrame)
        {
            mFramesSinceMidnight = mFrames
                + mSeconds * nominal_fps
                + mMinutes * 60 * nominal_fps
                + mHours * 60 * 60 * nominal_fps;
        }
        else
        {
            // Assuming 30000/1001 frame rate here
            mFramesSinceMidnight = mFrames
                + mSeconds * nominal_fps
                + (mMinutes % 10) * DF_FRAMES_PER_MINUTE
                + (mMinutes / 10) * DF_FRAMES_PER_TEN_MINUTE
                + mHours * DF_FRAMES_PER_HOUR;
            /* alternative code
            mFramesSinceMidnight = mFrames
                + mSeconds * nominal_fps
                + mMinutes * DF_FRAMES_PER_MINUTE
                + (mMinutes / 10) * 2
                + mHours * DF_FRAMES_PER_HOUR;
            */
        }
    }
}

/**
This method is used to update text representation of timecode whenever numerical
values change.
Unfortunately you can't produce the text in Text() function because it would not
then be const.
*/
void Timecode::UpdateText()
{
    if (mIsNull)
    {
        sprintf(mText, "??:??:??:??");
        sprintf(mTextNoSeparators, "????????");
    }
    else
    {
        // Full text representation
        if (mDropFrame)
        {
            sprintf(mText,"%02d:%02d:%02d;%02d", mHours, mMinutes, mSeconds, mFrames);
        }
        else
        {
            sprintf(mText,"%02d:%02d:%02d:%02d", mHours, mMinutes, mSeconds, mFrames);
        }

        // No separators version
        sprintf(mTextNoSeparators,"%02d%02d%02d%02d", mHours, mMinutes, mSeconds, mFrames);
    }
}

/**
Add a number of frames.
*/
void Timecode::operator+=(int frames)
{
    mFramesSinceMidnight += frames;
    UpdateHoursMinsEtc();
    UpdateText();
}

/**
Subtract a number of frames.
*/
void Timecode::operator-=(int frames)
{
    mFramesSinceMidnight -= frames;

    UpdateHoursMinsEtc();
    UpdateText();
}



Duration::Duration(int frames, int fps_num, int fps_den, bool df)
: mFrameCount(frames), mFrameRateNumerator(fps_num), mFrameRateDenominator(fps_den), mDropFrame(df)
{
    UpdateHoursMinsEtc();
    UpdateText();
}

void Duration::UpdateHoursMinsEtc()
{
    int frames = mFrameCount;
    int nominal_fps = mFrameRateNumerator / mFrameRateDenominator
        + (mFrameRateNumerator % mFrameRateDenominator ? 1 : 0);

    if (!mDropFrame)
    {
        mHours = frames / (nominal_fps * 60 * 60);
        frames %= (nominal_fps * 60 * 60);
        mMinutes = frames / (nominal_fps * 60);
        frames %= (nominal_fps * 60);
        mSeconds = frames / nominal_fps;
        mFrames = frames % nominal_fps;
    }
    else
    {
        mHours = frames / DF_FRAMES_PER_HOUR;
        frames %= DF_FRAMES_PER_HOUR;

        int ten_minute = frames / DF_FRAMES_PER_TEN_MINUTE;
        frames %= DF_FRAMES_PER_TEN_MINUTE;

        // must adjust frame count to make minutes calculation work
        // calculations from here on in just assume that within the 10 minute cycle
        // there are only DF_FRAMES_PER_MINUTE (1798) frames per minute - even for the first minute
        // in the ten minute cycle. Hence we decrement the frame count by 2 to get the minutes count
        // So for the first two frames of the ten minute cycle we get a negative frames number

        frames -= 2;

        int unit_minute = frames / DF_FRAMES_PER_MINUTE;
        frames %= DF_FRAMES_PER_MINUTE;
        mMinutes = ten_minute * 10 + unit_minute;

        // frames now contains frame in minute @ 1798 frames per minute
        // put the 2 frame adjustment back in to get the correct frame count
        // For the first two frames of the ten minute cycle, frames is negative and this
        // adjustment makes it non-negative. For other minutes in the cycle the frames count
        // goes from 0 upwards, thus this adjusment gives the required 2 frame offset

        frames += 2;

        mSeconds = frames / nominal_fps;
        mFrames = frames % nominal_fps;
    }
}

/**
This method is used to update text representation of duration whenever numerical
values change.
*/
void Duration::UpdateText()
{
#if 1
    // Full string including frames
    if (mDropFrame)
    {
        sprintf(mText,"%02d:%02d:%02d;%02d", mHours, mMinutes, mSeconds, mFrames);
    }
    else
    {
        sprintf(mText,"%02d:%02d:%02d:%02d", mHours, mMinutes, mSeconds, mFrames);
    }
#else
    // Shorter version without frames
    if (mHours == 0)
    {
        sprintf(mText,"%02d:%02d", mMinutes, mSeconds);
    }
    else
    {
        sprintf(mText,"%d:%02d:%02d", mHours, mMinutes, mSeconds);
    }
#endif
}

namespace Ingex
{

/**
Get difference between two Timecodes.
*/
Duration operator-(const Timecode & lhs, const Timecode & rhs)
{
    // NB. Should check for mode compatibility
    return Duration(lhs.FramesSinceMidnight() - rhs.FramesSinceMidnight(), lhs.mFrameRateNumerator, lhs.mFrameRateDenominator, lhs.mDropFrame);
}

/**
Add a number of frames to a timecode.
*/
Timecode operator+(const Timecode & lhs, const int & frames)
{
    return Timecode(lhs.mFramesSinceMidnight + frames, lhs.mFrameRateNumerator, lhs.mFrameRateDenominator, lhs.mDropFrame);
}

} // namespace



