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

#ifndef _TICKTREE_H_
#define _TICKTREE_H_

#include <wx/treectrl.h>
#include "RecorderC.h"
#include <vector>

DECLARE_EVENT_TYPE(wxEVT_TREE_MESSAGE, -1)

class wxXmlDocument;
class wxXmlNode;
class StringHash;

#define VIDEO_SOURCE_COLOUR wxColour(wxT("ORANGE"))
#define AUDIO_SOURCE_COLOUR wxColour(wxT("YELLOW"))
#define ROUTER_SOURCE_COLOUR wxColour(wxT("GREEN"))

/// A class derived from a tree control, allowing record enables to be set and status to be displayed for multiple recorders.
class TickTreeCtrl : public wxTreeCtrl
{
	public:
		TickTreeCtrl(wxWindow *, wxWindowID, const wxPoint& = wxDefaultPosition, const wxSize& = wxDefaultSize, const wxString & = wxT(""));
		bool AddRecorder(const wxString &, ProdAuto::TrackList_var, ProdAuto::TrackStatusList_var, bool, wxXmlDocument &);
		bool RemoveRecorder(const wxString &);

		void Clear();
		void EnableChanges(bool = true);
		bool GetEnableStates(const wxString &, CORBA::BooleanSeq &, CORBA::StringSeq &);
		void SetTrackStatus(const wxString &, bool, ProdAuto::TrackStatusList_var);
		bool SomeEnabled();
		bool TapeIdsOK();
		bool Problem();
		bool RecordingSuccessfully();
		void SetRecorderStateUnknown(const wxString &);
		void SetRecorderStateProblem(const wxString &);
		void GetPackageNames(wxArrayString &, std::vector<bool> &);
		void UpdateTapeIds(wxXmlDocument &);
	private:
		enum state { //must correspond to order images are loaded
			DISABLED,
			PARTIALLY_ENABLED,
			ENABLED,
			RECORDING,
			PARTIALLY_RECORDING,
			PROBLEM,
			UNKNOWN,
		};
		wxTreeItemId FindRecorder(const wxString &);
		void OnLMouseDown(wxMouseEvent &);
		bool SelectRecursively(wxTreeItemId, bool = true, bool = false, bool = false);
		void SetRecorderState(const wxString &, bool);
		void ReportState(wxTreeItemId, bool = true);
		wxXmlNode * GetFirstTapeIdNode(wxXmlDocument &);
		void ScanPackageNames(wxArrayString *, std::vector<bool> *, StringHash * = 0);

//		DECLARE_DYNAMIC_CLASS(TickTreeCtrl)
		bool mEnableChanges;
		wxXmlNode * mTapeIdsNode;
    		DECLARE_EVENT_TABLE()
};

/// Information about each node in the tree.  It is used as follows:
/// root node:
///	int: -
///	bool: true if enabled source(s) all have tape ID(s)
///	string: -
/// recorder node:
///	int: number of sources
///	bool: enabled source(s) that aren't router recorders all have tape ID(s)
///	string: -
/// package name node:
///	int: non-zero if a router recorder
///	bool: true if tape ID is present or if a router recorder
///	string: package name (without tape ID appended)
/// source node:
///	int: source index
///	bool: record enable state
///	string: -
class ItemData : public wxTreeItemData
{
	public:
		ItemData(int intValue = 0, bool boolValue = false) : mInt(intValue), mBool(boolValue) {}
		ItemData(const wxString & stringValue, int intValue = 0, bool boolValue = false) : mString(stringValue), mInt(intValue), mBool(boolValue) {}
		int GetInt() { return mInt; }
		void SetBool(bool state) { mBool = state; }
		bool GetBool() { return mBool; }
		const wxString & GetString() { return mString; }
	private:
		const wxString mString;
		const int mInt;
		bool mBool;
};

#endif //_TICKTREE_H_
