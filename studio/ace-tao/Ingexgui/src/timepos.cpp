/***************************************************************************
 *   $Id: timepos.cpp,v 1.18 2012/02/10 15:12:56 john_f Exp $             *
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

#define __STDC_CONSTANT_MACROS
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
: mTimecodeDisplay(timecodeDisplay), mPositionDisplay(positionDisplay), mTriggerTimecode(InvalidMxfTimecode), mRunningTimecode(InvalidMxfTimecode)
{
    SetNextHandler(parent);
    Reset();
    wxTimer * refreshTimer = new wxTimer(this);
    refreshTimer->Start(TC_DISPLAY_UPDATE_INTERVAL);
}

/// Responds to the update timer event.
/// If timecode is running, updates the timecode display based the time since SetTimecode() was called with the last known timecode value.
/// If the position display is running, updates it based on the time since Record() was called, or if the end of postroll has been reached, stops it and displays the no position indication.
/// If a trigger is active and its time has been reached, despatch it.
/// @param event The timer event.
void Timepos::OnRefreshTimer(wxTimerEvent& WXUNUSED(event))
{
    UpdateTimecodeAndDuration();
    //timecode display
    if (mTimecodeRunning) mTimecodeDisplay->SetLabel(FormatTimecode(mRunningTimecode));
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
            mPositionDisplay->SetLabel(FormatPosition(mRunningDuration, mStartTimecode));
        }
    }
}

/// Returns the current recording duration, or a zero duration if recording is not happening
wxTimeSpan Timepos::GetDuration()
{
    UpdateTimecodeAndDuration();
    wxTimeSpan duration;
    if (mPositionRunning) duration = mRunningDuration;
    return duration;
}

/// Works out the current timecode and duration from the current timecode offset (which takes account of system clock drift)
/// Detects if a trigger point has been passed, and fires the trigger if so.
/// Must be called regularly to ensure duration and triggers work properly
#define THRESHOLD_SECONDS 10 //the value above which an increase in timecode or duration since the last call is not believable so we conclude that it has in fact moved backwards over midnight/one day due to a large enough change in mTimecodeOffset (presumably caused by varying response times from the recorder supplying timecode)
void Timepos::UpdateTimecodeAndDuration()
{
    if (mLastKnownTimecode.edit_rate.numerator && mLastKnownTimecode.edit_rate.denominator) { //sanity check
        //update timecode and trigger helper flags
        wxDateTime timecodeTime = wxDateTime::UNow() + mTimecodeOffset; //system time adjusted for timecode offset; date component meaningless
        wxTimeSpan timespan = timecodeTime - timecodeTime.GetDateOnly(); //remove meaningless date component
        ProdAuto::MxfTimecode newTimecode = mLastKnownTimecode; //to set everything except sample value
        newTimecode.samples = timespan.GetMilliseconds().GetValue() * mLastKnownTimecode.edit_rate.numerator / mLastKnownTimecode.edit_rate.denominator / 1000;
        bool justWrapped = false;
        if (newTimecode.samples < mRunningTimecode.samples) { //has crossed midnight forwards
            justWrapped = true;
            wxCommandEvent loggingEvent(EVT_LOGGING_MESSAGE);
            loggingEvent.SetString(wxT("Timepos wrap detected at displayed timecode ") + Timepos::FormatTimecode(newTimecode));
            AddPendingEvent(loggingEvent);
        }
        if ((newTimecode.samples - mRunningTimecode.samples) * mLastKnownTimecode.edit_rate.denominator / mLastKnownTimecode.edit_rate.numerator > THRESHOLD_SECONDS) { //has crossed midnight backwards due to a large change to mTimecodeOffset
            mTriggerCarry = true; //if there's a trigger set, prevent it from happening prematurely due to wrapping backwards
            wxCommandEvent loggingEvent(EVT_LOGGING_MESSAGE);
            loggingEvent.SetString(wxT("Timepos carry set due to backwards wrap at ") + Timepos::FormatTimecode(newTimecode));
            AddPendingEvent(loggingEvent);
        }
        mRunningTimecode = newTimecode;
        //update duration - work out based on previous duration to be able to keep track of number of days reliably
        if (mPositionRunning) { //assumes mStartTimecode is sane
            timespan -= wxTimeSpan(0, 0, 0, (long) (INT64_C(1000) * mStartTimecode.samples * mStartTimecode.edit_rate.denominator / mStartTimecode.edit_rate.numerator)); //relative duration within 24 hours
            if (timespan < 0) timespan += wxTimeSpan::Day(); //make sure positive
            timespan += wxTimeSpan::Days(mRunningDuration.GetDays()); //total duration assuming same day component as before
            if (mRunningDuration - timespan > wxTimeSpan(0, 0, THRESHOLD_SECONDS, 0)) { //new duration is a lot less than the old, so day component must have incremented
                timespan += wxTimeSpan::Day();
            }
            else if (timespan - mRunningDuration > wxTimeSpan(0, 0, THRESHOLD_SECONDS, 0)) { //day component must have decremented due to a proportionately large change to mTimecodeOffset
                timespan -= wxTimeSpan::Day();
            }
            mRunningDuration = timespan;
        }
        //trigger
        if (
         !mTriggerTimecode.undefined //there's an active trigger
         && !mTriggerCarry //it's happening today
         && (
          mRunningTimecode.samples >= mTriggerTimecode.samples //have reached the trigger timecode
          || justWrapped //today is in fact yesterday, when the trigger must have been just before midnight
         )
        ) {
            wxCommandEvent loggingEvent(EVT_LOGGING_MESSAGE);
            loggingEvent.SetString(wxT("Timepos trigger at displayed timecode ") + Timepos::FormatTimecode(mRunningTimecode));
            AddPendingEvent(loggingEvent);
            Trigger();
        }
        if (justWrapped) mTriggerCarry = false; //it's now the day of the trigger
    }
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
/// @param handler Event handler where event will be sent to.  Only needed if tc is a defined timecode.
/// @param alwaysInFuture True to indicate the timecode is always in the future (even if its sample value is less than the current timecode).  If false, trigger timecode will be considered to be in the past if it is up to a minute before the current timecode, whereupon a trigger will occur immediately
/// @return True if trigger was set
bool Timepos::SetTrigger(const ProdAuto::MxfTimecode * tc, wxEvtHandler * handler, bool alwaysInFuture)
{
    wxCommandEvent loggingEvent(EVT_LOGGING_MESSAGE);
    bool ok = true;
    mTriggerTimecode = *tc;
    if (mTriggerTimecode.undefined || !mTriggerTimecode.edit_rate.numerator) { //latter a sanity check
        mTriggerTimecode.undefined = true;
        ok = false;
        loggingEvent.SetString(wxT("Timepos trigger being cleared."));
    }
    else {
        mTriggerHandler = handler;
        //If it looks like the trigger timecode is earlier in the day or now, set the carry flag which will prevent a trigger until after the displayed timecode wraps at midnight
        mTriggerCarry = mRunningTimecode.samples >= mTriggerTimecode.samples;
        loggingEvent.SetString(wxT("Timepos trigger set for ") + Timepos::FormatTimecode(*tc) + wxString::Format(wxT(" with alwaysInFuture %s; carry being set to %s.  Trigger samples %d; running samples %d."), alwaysInFuture ? wxT("true") : wxT("false"), mTriggerCarry ? wxT("true") : wxT("false"), mTriggerTimecode.samples, mRunningTimecode.samples));
        //If triggers in the past are allowed, trigger immediately if trigger timecode is up to a minute before the last displayed timecode
        if (!alwaysInFuture) {
            if (mRunningTimecode.samples > mTriggerTimecode.samples) {
                //assume last displayed timecode hasn't wrapped relative to trigger timecode
                if ((mRunningTimecode.samples - mTriggerTimecode.samples) / mTriggerTimecode.edit_rate.numerator * mTriggerTimecode.edit_rate.denominator < 60) { //trigger timecode is less than a minute behind last displayed timecode
                    loggingEvent.SetString(loggingEvent.GetString() + wxT("  Immediate trigger due to trigger timecode being up to a minute behind last displayed timecode (no wrap)."));
                    Trigger();
                }
            }
            else {
                //assume last displayed timecode has wrapped relative to trigger timecode
                if ((mRunningTimecode.samples - mTriggerTimecode.samples) / mTriggerTimecode.edit_rate.numerator * mTriggerTimecode.edit_rate.denominator + 86400 < 60) { //trigger timecode is less than a minute behind last displayed timecode
                    loggingEvent.SetString(loggingEvent.GetString() + wxT("  Immediate trigger due to trigger timecode being up to a minute behind last displayed timecode (displayed timecode wrap)."));
                    Trigger();
                }
            }
        }
    }
    AddPendingEvent(loggingEvent);
    return ok;
}




/// Gets the position display as a frame number, or returns 0 if the position display is not running.
/// @return The frame count.
int64_t Timepos::GetFrameCount()
{
    int64_t frameCount = 0;
    if (mPositionRunning) { //mStartTimecode's denominator will be OK
        wxTimeSpan duration = GetDuration(); //make sure it's up to date
        frameCount = duration.GetMilliseconds().GetValue() * mStartTimecode.edit_rate.numerator / mStartTimecode.edit_rate.denominator / 1000;
    }
    return frameCount;
}


/// Converts a duration into a string for display.
/// Default value if undefined or either edit rate value is zero (which would cause a divide by zero if not checked).
const wxString Timepos::FormatPosition(const ProdAuto::MxfDuration duration)
{
    wxString msg = UNKNOWN_POSITION;
    if (!duration.undefined && duration.edit_rate.numerator) { //latter a sanity check
        wxTimeSpan timespan(0, 0, 0, (long) (INT64_C(1000) * duration.samples * duration.edit_rate.denominator / duration.edit_rate.numerator));
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
        //minutes
        msg = timespan.GetMinutes() ? wxString::Format(wxT(" %02d:"), timespan.GetMinutes()) : wxT("    "); //Minutes count to >59
        //seconds
        msg += wxString::Format(wxT("%02d:"), wxLongLong(timespan.GetSeconds() % 60).GetValue());
        //frames
        msg += wxString::Format(wxT("%02d"), wxLongLong(timespan.GetMilliseconds() % 1000).GetValue() * editRate.edit_rate.numerator / editRate.edit_rate.denominator / 1000);
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
        mRunningDuration = 0; //to clear day component
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
/// and sets the postrolling flag in preparation for stopping when the recording stops, taking account of postroll.
/// If timecode is not acceptable, stops immediately and gives the unknown timecode indication.
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
            mStopTime = wxDateTime::UNow() + wxTimeSpan(0, 0, 0, (long) (INT64_C(1000) * samples * stopTimecode.edit_rate.denominator / stopTimecode.edit_rate.numerator));
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
    wxString timecodeString;
    if (mTimecodeRunning) {
        //return the most up to date timecode
        UpdateTimecodeAndDuration();
        timecodeString = FormatTimecode(mRunningTimecode);
    }
    else {
        timecodeString = mTimecodeDisplay->GetLabel();
    }
    if (tc) {
        if (mTimecodeRunning) {
            *tc = mRunningTimecode;
        }
        else {
            tc->undefined = true;
        }
    }
    return timecodeString;
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
        Ingex::Timecode timecode(tc.samples, tc.edit_rate.numerator, tc.edit_rate.denominator, tc.drop_frame);
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

/// Manually sets the position display to a given position.
/// @param samples The number of frames into the recording.
/// @param editRate The edit rate corresponding to the frame count.
void Timepos::SetPosition(unsigned long samples, ProdAuto::MxfTimecode * editRate) {
    if (!editRate->undefined && editRate->edit_rate.numerator) { //sanity check
        mPositionDisplay->SetLabel(FormatPosition(wxTimeSpan(0, 0, 0, (long) (INT64_C(1000) * samples * editRate->edit_rate.denominator / editRate->edit_rate.numerator)), *editRate));
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

/// Stops the timecode display running and displays the given message.
/// @param message The string to display.
void Timepos::DisableTimecode(const wxString & message)
{
    mTimecodeRunning = false;
    mTimecodeDisplay->SetLabel(message);
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
        mRunningTimecode = tc; //to prevent problems with functions that use this variable if they are called before the timecode display is updated
        if (stuck) {
            mTimecodeRunning = false;
            //show the stuck value
            mTimecodeDisplay->SetLabel(FormatTimecode(tc));
        }
        else {
            mTimecodeRunning = true;
            //calculate timecode offset to system clock as a positive value - will work regardless of relative phases of the two times, so long as any date component is removed from the result
            mTimecodeOffset = wxTimeSpan(0, 0, 0, (long) (INT64_C(1000) * tc.samples * tc.edit_rate.denominator / tc.edit_rate.numerator));
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
