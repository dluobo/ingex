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

#include "dragbuttonlist.h"
#include "ingexgui.h"

/// @param parent The parent window.
DragButtonList::DragButtonList(wxWindow * parent)
: wxScrolledWindow(parent)
{
	SetNextHandler(parent); //the radio button events are not handled here
	mSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mSizer);
}

/// Replaces current state with a new column of video source radio buttons, one for each source with a file, and with a quad split button at the top.
/// Returns information about the current state.
/// Single source buttons are labelled with the source name have a tool tip showing the associated filename.
/// All buttons are disabled.
/// @param takeInfo The file names, the source names and the source types.
/// @param fileNames Returns the file name associated with each video source which has a file.
/// @param sourceNames Returns corresponding names of sources.
void DragButtonList::SetSources(TakeInfo * takeInfo, std::vector<std::string> * fileNames, std::vector<std::string> * sourceNames)
{
	mSizer->Clear(true); //delete all buttons
	fileNames->clear();
	sourceNames->clear();
	wxRadioButton * quadSplit = new wxRadioButton(this, 0, wxT("Quad Split")); //the quad split is always the first video source (id = 0)
	quadSplit->SetToolTip(wxT("Up to the first four successfully opened files"));
	mSizer->Add(quadSplit, -1, wxEXPAND);
	quadSplit->Enable(false); //enable later if any files successfully loaded
	if (takeInfo->GetFiles()->GetCount()) { //this take has files associated
		for (size_t i = 0; i < takeInfo->GetFiles()->GetCount(); i++) { //recorder loop
			for (size_t j = 0; j < (*takeInfo->GetFiles())[i]->length(); j++) { //file loop
				if (ProdAuto::VIDEO == (*takeInfo->GetTracks())[i][j].type && strlen((*takeInfo->GetFiles())[i][j])) {
					fileNames->push_back((*takeInfo->GetFiles())[i][j].in());
					wxRadioButton * rb = new wxRadioButton(this, fileNames->size(), wxString((*takeInfo->GetTracks())[i][j].src.package_name, *wxConvCurrent)); //ID corresponds to file index
					rb->Enable(false); //we don't know whether the player can open this file yet
					rb->SetToolTip(wxString((*takeInfo->GetFiles())[i][j], *wxConvCurrent));
					mSizer->Add(rb, -1, wxEXPAND);
					sourceNames->push_back((*takeInfo->GetTracks())[i][j].src.package_name.in());
				}
			}
		}
	}
	Layout();
}

/// Enables/disables and selects the source select buttons.
/// Hides the quad split button if there is only one source enabled.
/// @param enables Enable state of each button.
/// @param selected The button to select.
void DragButtonList::EnableAndSelectSources(std::vector<bool> * enables, const unsigned int selected)
{
	bool someOK = false;
	for (size_t i = 0; i < enables->size(); i++) {
		if (mSizer->GetItem(i + 1)) { //sanity check (the quad split button shifts all the rest down)
			mSizer->GetItem(i + 1)->GetWindow()->Enable(enables->at(i));
			someOK |= enables->at(i);
			if (selected == i + 1) {
				wxRadioButton * selectedButton = (wxRadioButton *) mSizer->GetItem(i + 1)->GetWindow();
				selectedButton->SetValue(true);
			}
		}
	}
	if (mSizer->GetItem((size_t) 0)) { //sanity check
		if (enables->size() < 2) {
			//no point in having a quad split if only one source to show
			mSizer->GetItem((size_t) 0)->GetWindow()->Hide();
		}
		else {
			//enable quad split if any files OK
			mSizer->GetItem((size_t) 0)->GetWindow()->Enable(someOK);
		}
	}
}

/// Removes all buttons.
void DragButtonList::Clear()
{
	mSizer->Clear(true); //delete all buttons
}

/// Determines whether there are selectable sources before the current one, and optionally selects the next earlier source.
/// @param select True to select (if possible).
/// @return True if there are earlier selectable sources than the currently (select = false) or newly selected (select = true) source.
bool DragButtonList::EarlierSource(bool select)
{
	int previous = -1; //no previous button found
	bool more = false;
	for (size_t i = 0; i < mSizer->GetChildren().GetCount(); i++) {
		wxRadioButton * radioButton = (wxRadioButton *) mSizer->GetItem(i)->GetWindow();
		if (radioButton->GetValue()) {
			//selected
			if (previous > -1) {
				//there is a previous button to select
				if (select) {
					//select it
					radioButton = (wxRadioButton *) mSizer->GetItem(previous)->GetWindow();
					radioButton->SetValue(true);
					//this doesn't generate an event, so do so manually
					wxCommandEvent event(wxEVT_COMMAND_RADIOBUTTON_SELECTED, previous);
					AddPendingEvent(event);
				}
				else {
					//it can be selected later
					more = true;
				}
			}
			//finished, whether or not we can select
			break;
		}
		else if (radioButton->IsEnabled()) {
			if (previous > -1) {
				//already found an enabled button, so the one that will be selected won't be the first that can be selected
				more = true;
			}
			//the latest previous button
			previous = i;
		}
	}
	return more;
}

/// Determines whether there are selectable sources after the current one, and optionally selects the next source.
/// @param select True to select (if possible).
/// @return True if there are later selectable sources than the currently (select = false) or newly selected (select = true) source.
bool DragButtonList::LaterSource(bool select)
{
	bool selected_found = false; //not found the selected source yet
	bool selected_new = false;
	bool more = false;
	for (size_t i = 0; i < mSizer->GetChildren().GetCount(); i++) {
		wxRadioButton * radioButton = (wxRadioButton *) mSizer->GetItem(i)->GetWindow();
		if (radioButton->GetValue()) {
			//selected
			selected_found = true;
		}
		else if (selected_found && mSizer->GetItem(i)->GetWindow()->IsEnabled()) {
			//the next selectable source after the selected source
			if (select) {
				//select it
				radioButton->SetValue(true);
				//this doesn't generate an event, so do so manually
				wxCommandEvent event(wxEVT_COMMAND_RADIOBUTTON_SELECTED, i);
				AddPendingEvent(event);
				selected_new = true;
			}
			else {
				//it can be selected later
				more = true;
				break;
			}
		}
		else if (selected_new && mSizer->GetItem(i)->GetWindow()->IsEnabled()) {
			//another enabled button after the one we've just selected, so it's not the last that can be selected
			more = true;
			break;
		}
	}
	return more;
}

//drag drop experiment

/*	wxArrayString names;
	names.Add(wxT("First"));
	names.Add(wxT("Second"));
	names.Add(wxT("Third"));
	names.Add(wxT("Fourth"));

	for (unsigned int i = 0; i < names.GetCount(); i++) {
		wxBoxSizer * bs = new wxBoxSizer(wxHORIZONTAL);
		playbackPageSizer->Add(bs);
		wxRadioButton * rb = new wxRadioButton(playbackPage, -1, wxT(""));
		bs->Add(rb);
		wxStaticText * tb = new wxStaticText(playbackPage, -1, names[i]);
		bs->Add(tb);
	} */
