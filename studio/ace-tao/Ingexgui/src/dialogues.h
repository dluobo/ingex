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

#ifndef _DIALOGUES_H_
#define _DIALOGUES_H_
#include <wx/wx.h>
#include "ingexgui.h"

/// Set preroll and postroll.
class SetRollsDlg : public wxDialog
{
	public:
		SetRollsDlg(wxWindow *, const ProdAuto::MxfDuration, const ProdAuto::MxfDuration, const ProdAuto::MxfDuration, const ProdAuto::MxfDuration);
		const ProdAuto::MxfDuration GetPreroll();
		const ProdAuto::MxfDuration GetPostroll();
	private:
		ProdAuto::MxfDuration SetRoll(const wxChar *, int, const ProdAuto::MxfDuration &, wxStaticBoxSizer *);
		void OnRollChange( wxScrollEvent& event );
		wxSlider * mPrerollCtrl;
		wxSlider * mPostrollCtrl;
		wxStaticBoxSizer * mPrerollBox;
		wxStaticBoxSizer * mPostrollBox;
		ProdAuto::MxfDuration mMaxPreroll;
		ProdAuto::MxfDuration mMaxPostroll;

	enum
	{
		SLIDER_Preroll,
		SLIDER_Postroll,
	};

	DECLARE_EVENT_TABLE()
};

class wxGrid;
class wxGridEvent;

/// Set project names.
class SetProjectDlg : public wxDialog
{
	public:
		SetProjectDlg(wxWindow *, wxXmlDocument &, bool);
		bool IsUpdated();
		const wxString GetSelectedProject();
	private:
		enum {
			ADD = wxID_HIGHEST + 1,
			DELETE,
			EDIT,
		};
		void OnAdd(wxCommandEvent&);
		void OnDelete(wxCommandEvent&);
		void OnEdit(wxCommandEvent&);
		void OnChoice(wxCommandEvent&);
		void EnterName(const wxString &, const wxString &, int = wxNOT_FOUND);
		void EnableButtons(bool);

//		wxListBox * mProjectList;
		wxButton * mDeleteButton;
		wxButton * mEditButton;
		wxButton * mOKButton;
		wxChoice * mProjectList;
		bool mUpdated;
		wxString mSelectedProject;
	DECLARE_EVENT_TABLE()
};

class BoolHash;
class wxGridRangeSelectEvent;

/// Set tape IDs.
class SetTapeIdsDlg : public wxDialog
{
	public:
		SetTapeIdsDlg(wxWindow *, wxXmlDocument &, wxArrayString &, std::vector<bool> &);
		~SetTapeIdsDlg();
		bool IsUpdated();
	private:
		void OnEditorShown(wxGridEvent &);
		void OnEditorHidden(wxGridEvent &);
		void OnCellRangeSelected(wxGridRangeSelectEvent &);
		void OnAutoNumber(wxCommandEvent &);
		void OnHelp(wxCommandEvent &);
		bool AutoNumber(bool);

		wxGrid * mGrid;
		wxButton * mAutoButton;

		BoolHash * mEnabledPackages;
		bool mEditing;
		bool mUpdated;

	DECLARE_EVENT_TABLE()
};

#endif
