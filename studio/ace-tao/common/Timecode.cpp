/*
 * $Id: Timecode.cpp,v 1.1 2007/09/11 14:08:33 stuart_hc Exp $
 *
 * Class to hold a Timecode
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

#include "Timecode.h"


// constructor - timecode defaults to zero, mode defaults to TC25
Timecode::Timecode(int frames_since_midnight, int fps, bool df)
: mFramesSinceMidnight(frames_since_midnight), mFramesPerSecond(fps), mDropFrame(df)
{
    UpdateHoursMinsEtc();
    UpdateText();
}

// constructor - mode defaults to TC25
Timecode::Timecode(int hr, int min, int sec, int frame, int fps, bool df)
: mFramesPerSecond(fps), mDropFrame(df), mHours(hr), mMinutes(min), mSeconds(sec), mFrames(frame)
{
    UpdateFramesSinceMidnight();
    UpdateText();
}

// constructor - from text - mode defaults to TC25
Timecode::Timecode(const char * s, int fps, bool df)
: mFramesPerSecond(fps), mDropFrame(df)
{
    bool ok = false;
    if(11 == strlen(s))
    {
        if(4 == sscanf(s,"%2d:%2d:%2d:%2d", &mHours, &mMinutes, &mSeconds, &mFrames))
        {
            ok = true;
        }
        else if(4 == sscanf(s,"%2d:%2d:%2d.%2d", &mHours, &mMinutes, &mSeconds, &mFrames))
        {
            ok = true;
        }
    }
    else if(8 == strlen(s))
    {
        if(3 == sscanf(s,"%2d:%2d:%2d", &mHours, &mMinutes, &mSeconds))
        {
            mFrames = 0;
            ok = true;
        }
    }

    if(!ok)
    {
        mHours = 0;
        mMinutes = 0;
        mSeconds = 0;
        mFrames = 0;
    }

    UpdateFramesSinceMidnight();
    UpdateText();
}

// Copy constructor
Timecode::Timecode(const Timecode & tc)
{
    mFramesPerSecond = tc.mFramesPerSecond;
    mDropFrame = tc.mDropFrame;
    mFramesSinceMidnight = tc.mFramesSinceMidnight;
    mHours = tc.mHours;
    mMinutes = tc.mMinutes;
    mSeconds = tc.mSeconds;
    mFrames = tc.mFrames;

    UpdateText();
}

Timecode & Timecode::operator=(const Timecode & tc)
{
    if(this != &tc)
    {
        mFramesPerSecond = tc.mFramesPerSecond;
        mDropFrame = tc.mDropFrame;
        mFramesSinceMidnight = tc.mFramesSinceMidnight;
        mHours = tc.mHours;
        mMinutes = tc.mMinutes;
        mSeconds = tc.mSeconds;
        mFrames = tc.mFrames;
        UpdateText();
    }
    return *this;
}

Timecode & Timecode::operator=(int frames_since_midnight)
{
    mFramesSinceMidnight = frames_since_midnight;
    UpdateHoursMinsEtc();
    UpdateText();
    return *this;
}

void Timecode::UpdateHoursMinsEtc()
{
    if(!mDropFrame)
    {
        int frames = mFramesSinceMidnight;
        mHours = frames / (mFramesPerSecond * 60 * 60);
        frames %= (mFramesPerSecond * 60 * 60);
        mMinutes = frames / (mFramesPerSecond * 60);
        frames %= (mFramesPerSecond * 60);
        mSeconds = frames / mFramesPerSecond;
        mFrames = frames % mFramesPerSecond;
    }
    else
    {
    // Put clever drop-frame code here
    }
}

void Timecode::UpdateFramesSinceMidnight()
{
    if (!mDropFrame)
    {
        mFramesSinceMidnight = mFrames
            + mSeconds * mFramesPerSecond
            + mMinutes * 60 * mFramesPerSecond
            + mHours * 60 * 60 * mFramesPerSecond;
    }
    else
    {
    // Put clever drop-frame code here
    }
}

Duration operator-(const Timecode & lhs, const Timecode & rhs)
{
    // NB. Should check for mode compatibility
    return Duration(lhs.FramesSinceMidnight() - rhs.FramesSinceMidnight(), lhs.mFramesPerSecond, lhs.mDropFrame);
}

/**
This method is used to update text representation of timecode whenever numerical
values change.
Unfortunately you can't produce the text in Text() function because it would not
then be const.
*/
void Timecode::UpdateText()
{
    // Full text representation
    sprintf(mText,"%02d:%02d:%02d:%02d", mHours, mMinutes, mSeconds, mFrames);

    // No separators version
    sprintf(mTextNoSeparators,"%02d%02d%02d%02d", mHours, mMinutes, mSeconds, mFrames);
}

/**
To help with conversion to MXF-style timecode.
*/
int Timecode::EditRateNumerator() const
{
    int num = 0;
    if (!mDropFrame)
    {
        num = mFramesPerSecond;
    }
    else
    {
    // Put clever drop-frame code here
    }
    return num;
}

/**
To help with conversion to MXF-style timecode.
*/
int Timecode::EditRateDenominator() const
{
    int denom = 0;
    if (!mDropFrame)
    {
        denom = 1;
    }
    else
    {
    // Put clever drop-frame code here
    }
    return denom;
}



// Old code below

#if 0
/**
Express timecode as a frame count - 
not meaningful for drop-frame code.
*/
int Timecode::TotalFrames() const
{
    int mins = mHours * 60 + mMinutes;
    int secs = mins * 60 + mSeconds;
    int frames;
    switch(mMode)
    {
    case Timecode::TC25:
        frames = secs * 25 + mFrames;
        break;
    case Timecode::TC30:
        frames = secs * 30 + mFrames;
        break;
    case Timecode::TC30DF:
    default:
        frames = 0;
        break;
    }

    return frames;
}

/**
Set timecode from a frame count - 
not meaningful for drop-frame code.
*/
void Timecode::TotalFrames(int frames)
{
    int frames_per_second;
    switch(mMode)
    {
    case Timecode::TC25:
        frames_per_second = 25;
        break;
    case Timecode::TC30:
        frames_per_second = 30;
        break;
    default:
        frames_per_second = 0;
        break;
    }

    int secs = frames / frames_per_second;
    frames = frames % frames_per_second;
    int mins = secs / 60;
    secs = secs % 60;
    int hrs = mins / 60;
    mins = mins % 60;

    hrs = hrs % 60;

    mFrames = frames;
    mSeconds = secs;
    mMinutes = mins;
    mHours = hrs;

    SetText();
}


Timecode operator+(const Timecode & lhs, const Timecode & rhs)
{
    // NB. need to check for mode compatibility of lhs and rhs
    int frames_per_second;
    switch(lhs.mMode)
    {
    case Timecode::TC25:
        frames_per_second = 25;
        break;
    case Timecode::TC30:
        frames_per_second = 30;
        break;
    default:
        frames_per_second = 0; // should throw an exception here
        break;
    }

    Timecode result;
    int carry;

// Frames
    result.mFrames = lhs.mFrames + rhs.mFrames;
    if(result.mFrames < frames_per_second)
    {
        carry = 0;
    }
    else
    {
        result.mFrames -= frames_per_second;
        carry = 1;
    }

// Seconds
    result.mSeconds = lhs.mSeconds + rhs.mSeconds + carry;
    if(result.mSeconds < 60)
    {
        carry = 0;
    }
    else
    {
        result.mSeconds -= 60;
        carry = 1;
    }

// Minutes
    result.mMinutes = lhs.mMinutes + rhs.mMinutes + carry;
    if(result.mMinutes < 60)
    {
        carry = 0;
    }
    else
    {
        result.mMinutes -= 60;
        carry = 1;
    }

// Hours
    result.mHours = lhs.mHours + rhs.mHours + carry;
    if(result.mHours < 24)
    {
        carry = 0;
    }
    else
    {
        result.mHours -= 24;
        carry = 1;
    }

// NB. If we have a carry at this point, the hours have wrapped around

    result.SetText();

    return result;
}

Timecode operator-(const Timecode & lhs, const Timecode & rhs)
{
    // NB. need to check for mode compatibility
    Timecode result;
    int carry;

// frames
    if(lhs.mFrames >= rhs.mFrames)
    {
        result.mFrames = lhs.mFrames - rhs.mFrames;
        carry = 0;
    }
    else
    {
        switch(lhs.mMode)
        {
        case Timecode::TC25:
            result.mFrames = 25 + lhs.mFrames - rhs.mFrames;
            break;
        case Timecode::TC30:
            result.mFrames = 30 + lhs.mFrames - rhs.mFrames;
            break;
        default:
            break;
        }
        carry = 1;
    }

// seconds
    if(lhs.mSeconds >= (rhs.mSeconds + carry))
    {
        result.mSeconds = lhs.mSeconds - (rhs.mSeconds + carry);
        carry = 0;
    }
    else
    {
        result.mSeconds = 60 + lhs.mSeconds - (rhs.mSeconds + carry);
        carry = 1;
    }

// minutes
    if(lhs.mMinutes >= (rhs.mMinutes + carry))
    {
        result.mMinutes = lhs.mMinutes - (rhs.mMinutes + carry);
        carry = 0;
    }
    else
    {
        result.mMinutes = 60 + lhs.mMinutes - (rhs.mMinutes + carry);
        carry = 1;
    }

// hours
    if(lhs.mHours >= (rhs.mHours + carry))
    {
        result.mHours = lhs.mHours - (rhs.mHours + carry);
        carry = 0;
    }
    else
    {
        result.mHours = 24 + lhs.mHours - (rhs.mHours + carry);
        carry = 1;
    }

// NB. If we have a carry at this point, the hours have wrapped around

    result.SetText();

    return result;
}

void operator+=(Timecode & lhs, const Timecode & rhs)
{
    // NB. need to check for mode compatibility of lhs and rhs
    int frames_per_second;
    switch(lhs.mMode)
    {
    case Timecode::TC25:
        frames_per_second = 25;
        break;
    case Timecode::TC30:
        frames_per_second = 30;
        break;
    default:
        frames_per_second = 0; // should throw an exception here
        break;
    }

    int carry;

// Frames
    lhs.mFrames += rhs.mFrames;
    if(lhs.mFrames < frames_per_second)
    {
        carry = 0;
    }
    else
    {
        lhs.mFrames -= frames_per_second;
        carry = 1;
    }

// Seconds
    lhs.mSeconds += (rhs.mSeconds + carry);
    if(lhs.mSeconds < 60)
    {
        carry = 0;
    }
    else
    {
        lhs.mSeconds -= 60;
        carry = 1;
    }

// Minutes
    lhs.mMinutes += (rhs.mMinutes + carry);
    if(lhs.mMinutes < 60)
    {
        carry = 0;
    }
    else
    {
        lhs.mMinutes -= 60;
        carry = 1;
    }

// Hours
    lhs.mHours += (rhs.mHours + carry);
    if(lhs.mHours < 24)
    {
        carry = 0;
    }
    else
    {
        lhs.mHours -= 24;
        carry = 1;
    }

// NB. If we have a carry at this point, the hours have wrapped around

    lhs.SetText();
}

void operator-=(Timecode & lhs, const Timecode & rhs)
{
    // NB. need to check for mode compatibility of lhs and rhs

    int carry;

// frames
    if(lhs.mFrames >= rhs.mFrames)
    {
        lhs.mFrames -= rhs.mFrames;
        carry = 0;
    }
    else
    {
        switch(lhs.mMode)
        {
        case Timecode::TC25:
            lhs.mFrames = 25 + lhs.mFrames - rhs.mFrames;
            break;
        case Timecode::TC30:
            lhs.mFrames = 30 + lhs.mFrames - rhs.mFrames;
            break;
        default:
            break;
        }
        carry = 1;
    }

// seconds
    if(lhs.mSeconds >= (rhs.mSeconds + carry))
    {
        lhs.mSeconds -= (rhs.mSeconds + carry);
        carry = 0;
    }
    else
    {
        lhs.mSeconds = 60 + lhs.mSeconds - (rhs.mSeconds + carry);
        carry = 1;
    }

// minutes
    if(lhs.mMinutes >= (rhs.mMinutes + carry))
    {
        lhs.mMinutes -= (rhs.mMinutes + carry);
        carry = 0;
    }
    else
    {
        lhs.mMinutes = 60 + lhs.mMinutes - (rhs.mMinutes + carry);
        carry = 1;
    }

// hours
    if(lhs.mHours >= (rhs.mHours + carry))
    {
        lhs.mHours -= (rhs.mHours + carry);
        carry = 0;
    }
    else
    {
        lhs.mHours = 24 + lhs.mHours - (rhs.mHours + carry);
        carry = 1;
    }

// NB. If we have a carry at this point, the hours have wrapped around

    lhs.SetText();
}
#endif


Duration::Duration(int frames, int fps, bool df)
: mFrameCount(frames), mFramesPerSecond(fps), mDropFrame(df)
{
    UpdateHoursMinsEtc();
    UpdateText();
}

void Duration::UpdateHoursMinsEtc()
{
    if(!mDropFrame)
    {
        int frames = mFrameCount;
        mHours = frames / (mFramesPerSecond * 60 * 60);
        frames %= (mFramesPerSecond * 60 * 60);
        mMinutes = frames / (mFramesPerSecond * 60);
        frames %= (mFramesPerSecond * 60);
        mSeconds = frames / mFramesPerSecond;
        mFrames = frames % mFramesPerSecond;
    }
    else
    {
    // Put clever drop-frame code here
    }
}

/**
This method is used to update text representation of duration whenever numerical
values change.
*/
void Duration::UpdateText()
{
    // Text representation doesn't include frames
    if (mHours == 0)
    {
        sprintf(mText,"%02d:%02d", mMinutes, mSeconds);
    }
    else
    {
        sprintf(mText,"%d:%02d:%02d", mHours, mMinutes, mSeconds);
    }
}

