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

#include "recordbutton.h"

BEGIN_EVENT_TABLE(RecordButton, wxButton)
	EVT_LEFT_DOWN(RecordButton::OnLMouseDown)
END_EVENT_TABLE()

/// @param parent The parent window.
/// @param id The button's window ID.
RecordButton::RecordButton(wxWindow * parent, wxWindowID id) : wxButton(parent, id)
{
	SetNextHandler(parent);
	mInitialColour = GetBackgroundColour(); //on some machines disabling doesn't grey out the button
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

/// Prevents the button being clicked.
void RecordButton::Disable()
{
	Enable(false);
}

/// Makes the button clickable or not.
/// If enabled, the button is dull red; if disabled, greyed out.
/// @param state True to enable.
/// @return Not much use.
bool RecordButton::Enable(bool state)
{
	SetLabel(wxT("Record"));
	if (state) {
		SetBackgroundColour(wxColour(0xB0, 0x00, 0x00)); //dull red
	}
	else {
		SetBackgroundColour(mInitialColour);
	}
	mEnabled = true; //let the underlying button handle this
	return wxButton::Enable(state); //logic of return value is a bit broken
}

/// Puts the button into the bright red textless and disabled "record" state.
void RecordButton::Record() {
	wxButton::Enable(); //so it's not greyed out
	mEnabled = false; //so it doesn't respond to mouse clicks
	SetLabel(wxT(""));
	SetBackgroundColour(wxColour(wxT("RED")));
}

