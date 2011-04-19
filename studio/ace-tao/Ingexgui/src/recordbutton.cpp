/***************************************************************************
 *   $Id: recordbutton.cpp,v 1.10 2011/04/19 07:04:02 john_f Exp $            *
 *                                                                         *
 *   Copyright (C) 2006-2011 British Broadcasting Corporation              *
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

#include "recordbutton.h"
#include "colours.h"

BEGIN_EVENT_TABLE(RecordButton, wxButton)
    EVT_LEFT_DOWN(RecordButton::OnLMouseDown)
    EVT_TIMER(wxID_ANY, RecordButton::OnTimer)
END_EVENT_TABLE()

/// @param parent The parent window.
/// @param id The button's window ID.
RecordButton::RecordButton(wxWindow * parent, wxWindowID id, const wxString & label) : wxButton(parent, id, label), mLabel(label), mEnabled(true)
{
    SetNextHandler(parent);
    mInitialColour = GetBackgroundColour(); //on some machines disabling doesn't grey out the button
    mTimer = new wxTimer(this);
    Enable();
}

/// Intercepts mouse clicks to prevent them propagating if the button is "disabled".
/// @param event The mouse event.
void RecordButton::OnLMouseDown(wxMouseEvent & event)
{
    if (mEnabled) {
        //act on the click
        event.Skip();
    }
}

/// Makes the button clickable or not.
/// In record and pending states, this doesn't affect the appearance of the button.  In normal state, it is greyed out if disabled.
/// @param state True to enable.
bool RecordButton::Enable(bool state)
{
    mEnabled = state;
    if (RECORD_BUTTON_RECORDING_COLOUR != GetBackgroundColour() && !mTimer->IsRunning()) { //in normal state
        wxButton::Enable(mEnabled);
        SetBackgroundColour(mEnabled ? RECORD_BUTTON_ENABLED_COLOUR : mInitialColour); //dull red or greyed out
    }
    return mEnabled;
}

/// Makes the button non-clickable.
void RecordButton::Disable()
{
    Enable(false);
}

/// Changes the label displayed on the button if in normal state.  If in recording/pending state, remembers it for when the state changes to normal
/// @param label The label to be displayed on the button
void RecordButton::SetLabel(const wxString & label)
{
    mLabel = label;
    if (RECORD_BUTTON_RECORDING_COLOUR != GetBackgroundColour() && !mTimer->IsRunning()) { //in normal state
        wxButton::SetLabel(mLabel);
    }
}

/// Puts the button into the normal (non recording or pending) state, and sets the tooltip accordingly.
void RecordButton::Normal() {
    if (RECORD_BUTTON_RECORDING_COLOUR == GetBackgroundColour() || mTimer->IsRunning()) { //not already in normal state
//std::cerr << "normal" << std::endl;
        wxButton::Enable(mEnabled); //can be greyed out or not
        SetLabel(mLabel);
        SetBackgroundColour(mEnabled ? RECORD_BUTTON_ENABLED_COLOUR : mInitialColour); //dull red or greyed out
        mTimer->Stop();
        SetToolTip(wxT("Start a new recording"));
    }
}

/// Puts the button into the bright red textless and disabled "record" state, and sets the tooltip accordingly.
void RecordButton::Record() {
    if (RECORD_BUTTON_RECORDING_COLOUR != GetBackgroundColour() || mTimer->IsRunning()) { //not already in record state
//std::cerr << "record" << std::endl;
        wxButton::Enable(); //so it's not greyed out
        SetLabel(wxT(""));
        SetBackgroundColour(RECORD_BUTTON_RECORDING_COLOUR);
        mTimer->Stop();
        SetToolTip(wxT("Recording"));
    }
}

/// Puts the button into the flashing textless "running up" or "running down" state, and sets the tooltip accordingly.
/// @param runningUp true for running up; false for running down
void RecordButton::Pending(bool runningUp) {
    if (!mTimer->IsRunning() || mRunningUp != runningUp) { //changing mode
//std::cerr << "pending" << std::endl;
        mRunningUp = runningUp;
        wxButton::Enable(); //so it's not greyed out
        wxButton::SetLabel(wxT(""));
        SetBackgroundColour(mRunningUp ? RECORD_BUTTON_RECORDING_COLOUR : RECORD_BUTTON_ENABLED_COLOUR);
        mTimer->Start(125); //for flashing
        SetToolTip(mRunningUp ? wxT("Running up") : wxT("Running down"));
    }
}

/// Responds to flash timer events by toggling colour
void RecordButton::OnTimer(wxTimerEvent & WXUNUSED(event)) {
//std::cerr << "timer" << std::endl;
    if (mTimer->IsRunning()) { //trap timer being stopped with event in the system - if this ever happens...
        if (mInitialColour == GetBackgroundColour()) {
            SetBackgroundColour(mRunningUp ? RECORD_BUTTON_RECORDING_COLOUR : RECORD_BUTTON_ENABLED_COLOUR);
        }
        else {
            SetBackgroundColour(mInitialColour);
        }
    }
}

/// Returns true if the button is clickable
bool RecordButton::IsEnabled() {
    return mEnabled;
}
