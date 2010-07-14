/***************************************************************************
 *   $Id: recordbutton.h,v 1.7 2010/07/14 13:06:36 john_f Exp $            *
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

#ifndef RECORD_BUTTON_H_
#define RECORD_BUTTON_H_

#include <wx/wx.h>

#define ENABLED_COLOUR wxColour(wxT("#B00000"))
#define RECORDING_COLOUR wxColour(wxT("RED"))

/// A derived button which has a bright red but disabled state for "recording".
class RecordButton : public wxButton
{
public:
	RecordButton(wxWindow *, wxWindowID, const wxString & = wxT(""));
	virtual bool Enable(bool = true);
	virtual void Disable();
	virtual void SetLabel(const wxString &);
	void Record();
	void Pending();
	void Normal();
	bool IsEnabled();
private:
	void OnTimer(wxTimerEvent &);
	void OnLMouseDown(wxMouseEvent &);
	wxColour mInitialColour;
	wxTimer * mTimer;
	wxString mLabel;
	bool mEnabled;
	DECLARE_EVENT_TABLE()
};

#endif
