/***************************************************************************
 *   Copyright (C) 2006 by BBC Research   *
 *   info@rd.bbc.co.uk   *
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

BEGIN_EVENT_TABLE( Timepos, wxEvtHandler )
	EVT_TIMER( -1, Timepos::OnRefreshTimer )
END_EVENT_TABLE()

/// Sets displays to default, and starts update timer.
/// @param timecodeDisplay The studio timecode display control.
/// @param positionDisplay The position display control.
Timepos::Timepos(wxStaticText * timecodeDisplay, wxStaticText * positionDisplay)
: mTimecodeDisplay(timecodeDisplay), mPositionDisplay(positionDisplay)
{
	Reset();
	wxTimer * refreshTimer = new wxTimer(this);
	refreshTimer->Start(TC_DISPLAY_UPDATE_INTERVAL);
}

/// Responds to the update timer event.
/// If timecode is running, updates the timecode display based the time since SetTimecode() was called.
/// If the position display is running, updates it based on the time since Record() was called, or if the end of postroll has been reached, stops it and displays the exact recording length.
/// @param event The timer event.
void Timepos::OnRefreshTimer(wxTimerEvent& WXUNUSED(event))
{
	if (mTimecodeRunning) {
		wxDateTime timecode = wxDateTime::UNow() + mTimecodeOffset;
		mTimecodeDisplay->SetLabel(wxString::Format(timecode.Format(wxT("%H:%M:%S:")) + wxString::Format(wxT("%02d"), (int) (timecode.GetMillisecond() * mLastTimecodeReceived.edit_rate.numerator / mLastTimecodeReceived.edit_rate.denominator / 1000))));
	}
	if (mPositionRunning) {
		wxTimeSpan position;
		if (mPostrolling && wxDateTime::UNow() >= mStopTime) { //finished postrolling
			//display exact duration and stop
			mPostrolling = false;
			mPositionRunning = false;
			position = wxTimeSpan(0, 0, 0, (unsigned long) 1000 * mDuration.samples * mDuration.edit_rate.denominator / mDuration.edit_rate.numerator); // use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
		}
		else {
			//display position based on current time
			position = wxDateTime::UNow() - mPositionOrigin;
		}
		mPositionDisplay->SetLabel(FormatPosition(position));
	}
}

/// Gets the position display as a frame number, or returns 0 if the position display is not running.
/// @return The frame count.
long Timepos::GetFrameCount()
{
	long frameCount = 0;
	if (mPositionRunning) {
		wxTimeSpan position = wxDateTime::UNow() - mPositionOrigin;
		frameCount = position.GetMilliseconds().GetLo() * mLastTimecodeReceived.edit_rate.numerator / mLastTimecodeReceived.edit_rate.denominator / 1000;
	}
	return frameCount;
}

/// Converts a time span object into a string for display.
/// Default value if edit rate parameters not known.
/// @param position Time span object.
/// @return Formatted string.
const wxString Timepos::FormatPosition(const wxTimeSpan position)
{
	if (mLastTimecodeReceived.undefined) {
		return UNKNOWN_POSITION;
	}
	else {
		//show hours if any
		wxString msg = position.GetHours() ? wxString::Format(wxT(" %2d:"), position.GetHours()) : wxT(" ");
		//show minutes if any hours or minutes
		msg += position.GetHours() || position.GetMinutes() ? wxString::Format(wxT("%02d:"), position.GetMinutes()) : wxT("  ");
		//show seconds and frames
		msg += wxString::Format(wxT("%02d:"), (int) (position.GetSeconds().GetLo() % 60));
		msg += wxString::Format(wxT("%02d"), (int) (position.GetMilliseconds().GetLo() % 1000 * mLastTimecodeReceived.edit_rate.numerator / mLastTimecodeReceived.edit_rate.denominator / 1000));
		return msg;
	}
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
		mPositionRunning = true;
		mPostrolling = false; //could still be postrolling a previous recording
		//Work out the start time and date of the recording.  This allows recordings lasting >24 hours to display properly.
		mPositionOrigin = wxDateTime::UNow();
		mPositionOrigin.ResetTime(); //assume for the moment that recording started today
		mPositionOrigin += wxTimeSpan(0, 0, 0, (unsigned long) 1000 * tc.samples * tc.edit_rate.denominator / tc.edit_rate.numerator); //start timecode with today's date; use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
		mPositionOrigin -= mTimecodeOffset; //adjust to system clock
		if (mPositionOrigin.Subtract(wxDateTime::UNow()).GetHours()) { //the recording appears to be at least an hour long already so must have started yesterday
			mPositionOrigin.Subtract(wxDateSpan(0, 0, 0, 1));
		}
	}
	else {
		mStartTimecode.undefined = true;
		mPositionDisplay->SetLabel(UNKNOWN_POSITION);
	}
	return mPositionDisplay->GetLabel();
}

/// If position is not running, do nothing.
/// Otherwise, if given timecode is acceptable, works out the exact recording duration and the stop time in system clock time,
/// and sets the postrolling flag.
/// @param tc The timecode of the end of the recording (which can be in the future).  Must be compatible with the start timecode supplied to Record().
void Timepos::Stop(const ProdAuto::MxfTimecode tc)
{
	if (mPositionRunning) {
		if (!tc.undefined && tc.edit_rate.numerator == mStartTimecode.edit_rate.numerator && tc.edit_rate.denominator == mStartTimecode.edit_rate.denominator) { //sensible values and can calculate exact duration (as edit rate is the same)
			//Work out the stop time and date of the recording
			mStopTime = wxDateTime::UNow();
			mStopTime.ResetTime(); //assume for the moment that recording finishes today
			mStopTime += wxTimeSpan(0, 0, 0, (unsigned long) 1000 * tc.samples * tc.edit_rate.denominator / tc.edit_rate.numerator); //finish timecode with today's date; use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
			mStopTime -= mTimecodeOffset; //adjust to system clock
			if (wxDateTime::UNow().Subtract(mStopTime).GetHours() > 0) { //the recording appears to have stopped more than an hour ago so must be stopping tomorrow
				mStopTime.Add(wxDateSpan(0, 0, 0, 1));
			}
			mDuration = mStartTimecode; //for edit rate
			if (tc.samples <= mStartTimecode.samples) { //have rolled over midnight
				//add a day's worth of samples
				mDuration.samples = tc.samples - mStartTimecode.samples + 3600 * 24 * tc.edit_rate.denominator / tc.edit_rate.numerator;
			}
			else {
				mDuration.samples = tc.samples - mStartTimecode.samples;
			}
			mPostrolling = true; //start checking for stop time
		}
		else {
			//something very odd going on!
			mPositionRunning = false;
			mPositionDisplay->SetLabel(UNKNOWN_POSITION);
		}
	}
}

/// @param tc Timecode structure to be populated
/// @return The formatted string displayed on the timecode control.
const wxString Timepos::GetTimecode(ProdAuto::MxfTimecode * tc)
{
	if (tc) {
		if (mTimecodeRunning) {
			*tc = mLastTimecodeReceived; // for edit rate/defined
			wxDateTime timecode = wxDateTime::UNow() + mTimecodeOffset;
			tc->samples = ((((timecode.GetHour() * 60) + timecode.GetMinute()) * 60) + timecode.GetSecond()) * tc->edit_rate.numerator / tc->edit_rate.denominator;
		}
		else {
			tc->undefined = true;
		}
	}
	return mTimecodeDisplay->GetLabel();
}

/// @return A formatted string representing the last timecode supplied to Record().
const wxString Timepos::GetStartTimecode()
{
	if (mStartTimecode.undefined || !mStartTimecode.edit_rate.numerator || mStartTimecode.edit_rate.denominator > mStartTimecode.edit_rate.numerator) { //arithmetic fault
		return UNKNOWN_TIMECODE;
	}
	else {
		wxTimeSpan tc(0, 0, 0, (unsigned long) 1000 * mStartTimecode.samples * mStartTimecode.edit_rate.denominator / mStartTimecode.edit_rate.numerator); //use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)

		return wxString::Format(tc.Format(wxT("%H:%M:%S:"))) + wxString::Format(wxT("%02d"), mStartTimecode.samples % (mStartTimecode.edit_rate.numerator / mStartTimecode.edit_rate.denominator));
	}
}

/// @return The formatted string displayed on the position control, or a similarly formatted string showing the recording duration, if postrolling.
const wxString Timepos::GetPosition()
{
	if (mPostrolling) {
		//we're really at the end of the recording, even though the display is still moving
		return FormatPosition(wxTimeSpan(0, 0, 0, (unsigned long) 1000 * mDuration.samples * mDuration.edit_rate.denominator / mDuration.edit_rate.numerator)); //use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
	}
	else {
		return mPositionDisplay->GetLabel();
	}
}

/// @return The formatted string representing the very start of a recording.
const wxString Timepos::GetStartPosition()
{
	return FormatPosition(wxTimeSpan(0, 0, 0, 0));
}

/// Returns the system to its initial state.
void Timepos::Reset()
{
	DisableTimecode();
	mPositionDisplay->SetLabel(NO_POSITION);
	mPostrolling = false;
	mPositionRunning = false;
}

/// Manually sets the position display to a given position, based on the edit rate in use.
/// @param samples The number of frames into the recording.
void Timepos::SetPosition(long samples) {
	if (mLastTimecodeReceived.edit_rate.numerator) {
		mPositionDisplay->SetLabel(FormatPosition(wxTimeSpan(0, 0, 0, (unsigned long) 1000 * samples * mLastTimecodeReceived.edit_rate.denominator / mLastTimecodeReceived.edit_rate.numerator))); //use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
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

/// Stops the timecode display running, invalidates previously received timecode, and displays the given message.
/// @param message The string to display.
void Timepos::DisableTimecode(const wxString & message)
{
	mTimecodeRunning = false;
	mTimecodeDisplay->SetLabel(message);
	mLastTimecodeReceived.undefined = true;
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
		mLastTimecodeReceived = tc;
		if (stuck) {
			mTimecodeRunning = false;
			//show the stuck value
			wxTimeSpan stuckTimecode(0, 0, 0, (unsigned long) 1000 * tc.samples * tc.edit_rate.denominator / tc.edit_rate.numerator); //use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
			mTimecodeDisplay->SetLabel(wxString::Format(stuckTimecode.Format(wxT("%H:%M:%S:")) + wxString::Format(wxT("%02d"), (int) ((stuckTimecode.GetMilliseconds().GetLo() % 1000) * tc.edit_rate.numerator / tc.edit_rate.denominator / 1000)))); //GetLo() is sufficient because 24 hours'-worth of milliseconds is < 32 bits
		}
		else {
			mTimecodeRunning = true;
			wxDateTime now = wxDateTime::UNow();
			//express timecode as a date time object with today's date
			wxDateTime timecodeTime = now;
			timecodeTime.ResetTime(); //midnight today
			timecodeTime += wxTimeSpan(0, 0, 0, (unsigned long) 1000 * tc.samples * tc.edit_rate.denominator / tc.edit_rate.numerator); //timecode with today's date; use unsigned long because long isn't quite big enough and rolls over at about 23:51:38 (@25fps)
			//work out offset from system time
			mTimecodeOffset = timecodeTime.Subtract(now); //how much later the timecode is than the clock time
		}
	}
	else {
		//duff data
		DisableTimecode(UNKNOWN_TIMECODE);
	}
}