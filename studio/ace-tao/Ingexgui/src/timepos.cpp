/***************************************************************************
 *   $Id: timepos.cpp,v 1.13 2010/08/25 17:51:06 john_f Exp $             *
 *                                                                         *
 *   Copyright (C) 2006-2010 British Broadcasting Corporation              *
 *   - all rights reserved.                                                *
 *   Author: Matthew Marks                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "timepos.h"
#include "Timecode.h"
#include "ingexgui.h"

DEFINE_EVENT_TYPE(EVT_TIMEPOS_EVENT)

BEGIN_EVENT_TABLE( Timepos, wxEvtHandler )
    EVT_TIMER( -1, Timepos::OnRefreshTimer )
END_EVENT_TABLE()

/// Sets displays to default, and starts update timer.
/// @param timecodeDisplay The studio timecode display control.
/// @param positionDisplay The position display control.
Timepos::Timepos(wxEvtHandler * parent, wxStaticText * timecodeDisplay, wxStaticText * positionDisplay)
: mTimecodeDisplay(timecodeDisplay), mPositionDisplay(positionDisplay), mTriggerTimecode(InvalidMxfTimecode), mLastDisplayedTimecode(InvalidMxfTimecode)
{
    SetNextHandler(parent);
    Reset();
    wxTimer * refreshTimer = new wxTimer(this);
    refreshTimer->Start(TC_DISPLAY_UPDATE_INTERVAL);
}

/// Responds to the update timer event.
/// If timecode is running, updates the timecode display based the time since SetTimecode() was called.
/// If the position display is running, updates it based on the time since Record() was called, or if the end of postroll has been reached, stops it and displays the no position indication.
/// If a trigger is active and its time has been reached, despatch it.
/// @param event The timer event.
void Timepos::OnRefreshTimer(wxTimerEvent& WXUNUSED(event))
{
    bool justWrapped = false;
    //timecode display
    if (mTimecodeRunning && mLastKnownTimecode.edit_rate.denominator) { //latter a sanity check
        ProdAuto::MxfTimecode tc = mLastKnownTimecode; //to set everything except sample value
        wxDateTime timecodeTime = wxDateTime::UNow() + mTimecodeOffset; //correct for timecode offset
        wxTimeSpan sinceMidnight = timecodeTime - timecodeTime.GetDateOnly(); //mod one day to remove all overflows
        tc.samples = sinceMidnight.GetMilliseconds().GetLo() * mLastKnownTimecode.edit_rate.numerator / mLastKnownTimecode.edit_rate.denominator / 1000;
        mTimecodeDisplay->SetLabel(FormatTimecode(tc));
        justWrapped = tc.samples < mLastDisplayedTimecode.samples; //has crossed midnight
        mLastDisplayedTimecode = tc;
    }
    //duration/position display
    if (mPositionRunning) {
        if (mPostrolling && wxDateTime::UNow() >= mStopTime) { //finished postrolling
            //display exact duration and stop
            mPostrolling = false;
            mPositionRunning = false;
            SetPositionUnknown(true); //display "no position"
        }
        else {
            //display position based on current time
            mPositionDisplay->SetLabel(FormatPosition(GetDuration(), mStartTimecode)); //work out from start timecode every time to take into account system clock drift
        }
    }
    //trigger
    if (
     !mTriggerTimecode.undefined //there's an active trigger
     && !mTriggerCarry //it's happening today
     && (
      mLastDisplayedTimecode.samples >= mTriggerTimecode.samples //have reached the trigger timecode
      || justWrapped //it happened very late yesterday
     )
    ) {
        Trigger();
    }
    if (justWrapped) mTriggerCarry = false; //it's now the day of the trigger
}

/// Sends a trigger event and sets the trigger timecode to undefined to prevent repeat triggering
void Timepos::Trigger()
{
    wxCommandEvent event(EVT_TIMEPOS_EVENT);
    ProdAuto::MxfTimecode * tc = new ProdAuto::MxfTimecode(mTriggerTimecode); //deleted by event handler
    event.SetClientData(tc);
    mTriggerHandler->AddPendingEvent(event);
    mTriggerTimecode.undefined = true; //prevent trigger happening again
}

/// Sets the timecode for an event to be triggered or cancels a pending trigger.
/// @param tc Timecode when trigger will happen.  If undefined flag is set, stops a pending trigger.
/// @param handler Event handler where event will be sent to.  Only needed if tc is defined.
/// @param alwaysInFuture True to indicate the timecode is always in the future (even if its sample value is less than the current timecode).  If false, trigger timecode will be considered to be in the past if it is up to a minute before the current timecode, whereupon a trigger will occur immediately
/// @return True if trigger was set
bool Timepos::SetTrigger(const ProdAuto::MxfTimecode * tc, wxEvtHandler * handler, bool alwaysInFuture)
{
    bool ok = true;
    mTriggerTimecode = *tc;
    if (mTriggerTimecode.undefined || !mTriggerTimecode.edit_rate.numerator) { //latter a sanity check
        mTriggerTimecode.undefined = true;
        ok = false;
    }
    else {
        mTriggerHandler = handler;
        //If it looks like the trigger timecode is earlier in the day or now, set the carry flag which will prevent a trigger until after the displayed timecode wraps at midnight
        mTriggerCarry = mLastDisplayedTimecode.samples >= mTriggerTimecode.samples;
        //If triggers in the past are allowed, trigger immediately if trigger timecode is up to a minute before the last displayed timecode
        if (!alwaysInFuture) {
            if (mLastDisplayedTimecode.samples > mTriggerTimecode.samples) {
                //assume last displayed timecode hasn't wrapped relative to trigger timecode
                if ((mLastDisplayedTimecode.samples - mTriggerTimecode.samples) / mTriggerTimecode.edit_rate.numerator * mTriggerTimecode.edit_rate.denominator < 60) Trigger(); //trigger if trigger timecode is less than a minute behind last displayed timecode
            }
            else {
                //assume last displayed timecode has wrapped relative to trigger timecode
                if ((mLastDisplayedTimecode.samples - mTriggerTimecode.samples) / mTriggerTimecode.edit_rate.numerator * mTriggerTimecode.edit_rate.denominator + 86400 < 60) Trigger(); //trigger if trigger timecode is less than a minute behind last displayed timecode
            }
        }
    }
    return ok;
}

/// Returns the current recording duration, or a zero duration if recording is not happening
wxTimeSpan Timepos::GetDuration()
{
    wxTimeSpan duration;
    if (mPositionRunning) {
        wxDateTime now = wxDateTime::UNow();
        wxDateTime timecodeTime = now + mTimecodeOffset; //correct for timecode offset
        duration = timecodeTime - timecodeTime.GetDateOnly(); //mod one day to remove overflows
        duration -= wxTimeSpan(0, 0, 0, (unsigned long) 1000 * mStartTimecode.samples * mStartTimecode.edit_rate.denominator / mStartTimecode.edit_rate.numerator); //subtract the start timecode; use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
        duration += now.GetDateOnly() - mStartDate; //add the number of days' duration
    }
    return duration;
}

/// Gets the position display as a frame number, or returns 0 if the position display is not running.
/// @return The frame count.
int64_t Timepos::GetFrameCount()
{
    int64_t frameCount = 0;
    if (mPositionRunning && mStartTimecode.edit_rate.denominator) { //latter a sanity check
        wxTimeSpan duration = GetDuration();
        frameCount = (duration - wxTimeSpan::Days(duration.GetDays())).GetMilliseconds().GetLo() * mStartTimecode.edit_rate.numerator / mStartTimecode.edit_rate.denominator / 1000; //keep duration within a day so using GetLo() only is safe
        frameCount += (int64_t) duration.GetDays() * 86400 / mStartTimecode.edit_rate.denominator * mStartTimecode.edit_rate.numerator;
    }
    return frameCount;
}


/// Converts a duration into a string for display.
/// Default value if undefined or either edit rate value is zero (which would cause a divide by zero if not checked).
const wxString Timepos::FormatPosition(const ProdAuto::MxfDuration duration)
{
    wxString msg = UNKNOWN_POSITION;
    if (!duration.undefined && duration.edit_rate.numerator) { //latter a sanity check
        wxTimeSpan timespan(0, 0, 0, (unsigned long) 1000 * duration.samples * duration.edit_rate.denominator / duration.edit_rate.numerator); //use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
        ProdAuto::MxfTimecode editRate;
        editRate.undefined = false;
        editRate.edit_rate.numerator = duration.edit_rate.numerator;
        editRate.edit_rate.denominator = duration.edit_rate.denominator;
        msg = FormatPosition(timespan, editRate);
    }
    return msg;
}

/// Converts a time span object into a string for display.
/// If negative, a day is added to wrap it.
/// @param timespan Contains the duration.
/// @param editRate Contains edit rate needed for conversion, checked to prevent divide by zero.
const wxString Timepos::FormatPosition(const wxTimeSpan timespan, const ProdAuto::MxfTimecode editRate)
{
    wxString msg;
    if (editRate.undefined || !editRate.edit_rate.denominator) {
        msg = UNKNOWN_POSITION;
    }
    else {
        wxTimeSpan positive = timespan;
        if (positive < 0) positive += wxTimeSpan::Day();
        //minutes
        msg = positive.GetMinutes() ? wxString::Format(wxT(" %02d:"), positive.GetMinutes()) : wxT("    "); //Minutes count to >59
        //seconds
        msg += wxString::Format(wxT("%02d:"), (int) (positive.GetSeconds().GetLo() % 60));
        //frames
        msg += wxString::Format(wxT("%02d"), (int) (positive.GetMilliseconds().GetLo() % 1000 * editRate.edit_rate.numerator / editRate.edit_rate.denominator / 1000));
    }
    return msg;
}

/// If timecode isn't running (so we don't know studio timecode's offset from the system clock), or given timecode isn't valid,
/// shows default position indication.
/// Otherwise, sets position counter origin to the correct date and the given timecode, minus its offset from real time,
/// and starts position counter running.
/// @param tc The timecode of the start of the recording.
const wxString Timepos::Record(const ProdAuto::MxfTimecode tc)
{
    if (mTimecodeRunning && !tc.undefined && tc.edit_rate.numerator && tc.edit_rate.denominator) { //sensible values: no chance of divide by zero!
        mStartTimecode = tc; //for calculating exact duration
        mStartDate = wxDateTime::Today();
        mPositionRunning = true;
        mPostrolling = false; //could still be postrolling a previous recording
    }
    else {
        mStartTimecode.undefined = true;
        mPositionDisplay->SetLabel(UNKNOWN_POSITION);
    }
    return mPositionDisplay->GetLabel();
}

/// If position is not running, does nothing.
/// Otherwise, if given timecode is acceptable, works out the exact recording duration and the stop time in system clock time,
/// and sets the postrolling flag.
/// @param stopTimecode The timecode of the end of the recording (which can be in the future).  Must be compatible with the start timecode supplied to Record().
void Timepos::Stop(const ProdAuto::MxfTimecode stopTimecode)
{
    if (mPositionRunning) {
        if (!stopTimecode.undefined && stopTimecode.edit_rate.numerator == mStartTimecode.edit_rate.numerator && stopTimecode.edit_rate.denominator == mStartTimecode.edit_rate.denominator) { //sensible values and can calculate exact duration (as edit rate is the same)
            //Work out the stop time and date of the recording
            ProdAuto::MxfTimecode now;
            GetTimecode(&now);
            int64_t samples = stopTimecode.samples + 86400 * stopTimecode.edit_rate.numerator / stopTimecode.edit_rate.denominator - now.samples; //diff between stop timecode and now, plus a day to cater for wrapping
            samples %= 86400 * stopTimecode.edit_rate.numerator / stopTimecode.edit_rate.denominator; //mod 1 day
            mStopTime = wxDateTime::UNow() + wxTimeSpan(0, 0, 0, (unsigned long) 1000 * samples * stopTimecode.edit_rate.denominator / stopTimecode.edit_rate.numerator); //timecode as a time span; use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
            mPostrolling = true; //start checking for stop time
        }
        else {
            //something very odd going on!
            mPositionRunning = false;
            mPositionDisplay->SetLabel(UNKNOWN_POSITION);
        }
    }
}

/// Supplies the timecode corresponding to now, both as a timecode structure and a formatted string.
/// @param tc Timecode structure to be populated.
/// @return The formatted string displayed on the timecode control.
const wxString Timepos::GetTimecode(ProdAuto::MxfTimecode * tc)
{
    if (tc) {
        if (mTimecodeRunning) {
            *tc = mLastKnownTimecode; //to set everything except sample value
            wxDateTime timecodeTime = wxDateTime::UNow() + mTimecodeOffset; //correct for timecode offset
            wxTimeSpan sinceMidnight = timecodeTime - timecodeTime.GetDateOnly(); //mod one day to remove all overflows
            tc->samples = sinceMidnight.GetMilliseconds().GetLo() * mLastKnownTimecode.edit_rate.numerator / mLastKnownTimecode.edit_rate.denominator / 1000;
        }
        else {
            tc->undefined = true;
        }
    }
    return mTimecodeDisplay->GetLabel();
}

/// Supplies the timecode corresponding to the value sent to Record(), both as a timecode structure and a formatted string.
/// @param tc Timecode structure to be populated.
/// @return Timecode as a formatted string.
const wxString Timepos::GetStartTimecode(ProdAuto::MxfTimecode * tc)
{
    if (tc) {
        *tc = mStartTimecode;
    }
    return FormatTimecode(mStartTimecode);
}

/// Formats the supplied timecode as a displayable string.
/// Displays in real time (i.e. drop frame for 29.97/59.94 fps) and wraps at 24 hours.
/// @param tc The timecode to format.
/// @return The formatted string.
const wxString Timepos::FormatTimecode(const ProdAuto::MxfTimecode tc)
{
    if (tc.undefined || !tc.edit_rate.numerator || !tc.edit_rate.denominator) { //arithmetic fault
        return UNKNOWN_TIMECODE;
    }
    else {
        Ingex::Timecode timecode(tc.samples, tc.edit_rate.numerator, tc.edit_rate.denominator, tc.edit_rate.numerator == 30000 || tc.edit_rate.numerator == 60000);
        return wxString(timecode.Text(), *wxConvCurrent);
    }
}

/// @return The formatted string representing the very start of a recording.
const wxString Timepos::GetStartPosition()
{
    return FormatPosition(wxTimeSpan(0, 0, 0, 0), mLastKnownTimecode);
}

/// Returns the system to its initial state.
void Timepos::Reset()
{
    DisableTimecode();
    mLastKnownTimecode.undefined = true;
    mPositionDisplay->SetLabel(NO_POSITION);
    mPostrolling = false;
    mPositionRunning = false;
    mTriggerTimecode.undefined = true;
}

/// Manually sets the position display to a given position, based on the edit rate in use.
/// @param samples The number of frames into the recording.
void Timepos::SetPosition(unsigned long samples) {
    if (!mLastKnownTimecode.undefined && mLastKnownTimecode.edit_rate.numerator) {
        mPositionDisplay->SetLabel(FormatPosition(wxTimeSpan(0, 0, 0, (unsigned long) 1000 * samples * mLastKnownTimecode.edit_rate.denominator / mLastKnownTimecode.edit_rate.numerator), mLastKnownTimecode)); //use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
    }
}

/// Stops the position display running and shows the desired indication.
/// @param noPosition If true, the indication corresponds to no position; if false, unknown position.
void Timepos::SetPositionUnknown(bool noPosition)
{
    if (noPosition) {
        mPositionDisplay->SetLabel(NO_POSITION);
    }
    else {
        mPositionDisplay->SetLabel(UNKNOWN_POSITION);
    }
    mPositionRunning = false;
}

// Stops the timecode display running and displays the given message.
/// Stops the timecode display running and displays the given message.
/// @param message The string to display.
void Timepos::DisableTimecode(const wxString & message)
{
    mTimecodeRunning = false;
    mTimecodeDisplay->SetLabel(message);
//  mLastKnownTimecode.undefined = true;
}

/// Calculates given timecode's offset from the system clock and starts the timecode display running, or displays it as a stuck timecode.
/// If timecode has a problem, displays the appropriate indication instead.
/// @param tc Timecode reference.
/// @param stuck If true, indicates that the timecode is stuck so the display should not auto-update.
void Timepos::SetTimecode(const ProdAuto::MxfTimecode tc, bool stuck)
{
    if (tc.undefined) { //no timecode available
        DisableTimecode();
    }
    else if (tc.edit_rate.numerator && tc.edit_rate.denominator) { //sensible values: no chance of divide by zero!
        mLastKnownTimecode = tc;
        mLastDisplayedTimecode = tc; //to prevent problems with functions that use this variable if they are called before the timecode display is updated
        if (stuck) {
            mTimecodeRunning = false;
            //show the stuck value
            mTimecodeDisplay->SetLabel(FormatTimecode(tc));
        }
        else {
            mTimecodeRunning = true;
            //calculate timecode offset to system clock as a positive value - will work regardless of relative phases of the two times, so long as any date component is removed from the result
            mTimecodeOffset = wxTimeSpan(0, 0, 0, (unsigned long) 1000 * tc.samples * tc.edit_rate.denominator / tc.edit_rate.numerator); //timecode as a time span; use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
            mTimecodeOffset += wxTimeSpan::Day(); //give it dominance by making it range from 24 to 48 hours
            wxDateTime now = wxDateTime::UNow();
            mTimecodeOffset -= wxTimeSpan(now - now.GetDateOnly()); //how much later the difference is between inflated timecode value and system time/date (anything over 24 hours, i.e. date and other overflows, will be removed when offset is used)
        }
    }
    else {
        //duff data
        DisableTimecode(UNKNOWN_TIMECODE);
    }
}

/// Sets the edit rate
/// @param rate Edit rate - this is checked for sensible values.
void Timepos::SetDefaultEditRate(const ProdAuto::MxfTimecode rate) {
    mLastKnownTimecode = rate;
    if (!rate.edit_rate.numerator || !rate.edit_rate.denominator) {
        mLastKnownTimecode.undefined = true;
    }
}

/// Returns the edit rate
const ProdAuto::MxfTimecode Timepos::GetDefaultEditRate()
{
    return mLastKnownTimecode;
}
