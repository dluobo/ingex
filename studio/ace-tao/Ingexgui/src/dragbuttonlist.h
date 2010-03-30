/***************************************************************************
 *   Copyright (C) 2006-2009 British Broadcasting Corporation              *
 *   - all rights reserved.                                                *
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

#ifndef _DRAGBUTTONLIST_H_
#define _DRAGBUTTONLIST_H_

#include <wx/wx.h>
#include <vector>
#include <string>
#include "RecorderC.h"
#include "LocalIngexPlayer.h" //for prodauto

class ChunkInfo;

WX_DEFINE_ARRAY_INT(int, ArrayOfInts);

/// Ignore the "drag" bit - this class represents a set of source selection buttons for playback.
class DragButtonList : public wxScrolledWindow
{
	public:
		DragButtonList(wxWindow *, wxEvtHandler *);
		prodauto::PlayerInputType SetTracks(ChunkInfo*, std::vector<std::string> &, std::vector<std::string> &);
		prodauto::PlayerInputType SetMXFFiles(wxArrayString &, std::vector<std::string> &, std::vector<std::string> &, ProdAuto::MxfTimecode &);
#ifndef DISABLE_SHARED_MEM_SOURCE
		prodauto::PlayerInputType SetEtoE(std::vector<std::string> &, std::vector<std::string> &);
#endif
		void EnableAndSelectTracks(std::vector<bool> *, const unsigned int);
		void Clear();
		bool EarlierTrack(const bool);
		bool LaterTrack(const bool);
		void SelectQuadrant(const unsigned int source);
		const wxString GetProjectName();
//		std::vector<std::string> * GetFiles();
	private:
		void OnUpdateUI(wxUpdateUIEvent&);
		void OnRadioButton(wxCommandEvent& event);
		void Select(unsigned int);
		ArrayOfInts mEnableStates;
		unsigned int mSelected;
		wxString mProjectName;
		wxEvtHandler* mButtonEvtHandler;
	DECLARE_EVENT_TABLE();
};

#endif
