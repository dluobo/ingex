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

#include "dialogues.h"
#include <wx/grid.h>

BEGIN_EVENT_TABLE(SetRollsDlg, wxDialog)
	EVT_SCROLL(SetRollsDlg::OnRollChange)
END_EVENT_TABLE()

/// Sets up dialogue.
/// @param parent The parent window.
/// @param preroll The current preroll value.
/// @param maxPreroll The maximum allowed preroll.
/// @param postroll The current postroll value.
/// @param maxPostroll The maximum allowed postroll.
SetRollsDlg::SetRollsDlg(wxWindow * parent, const ProdAuto::MxfDuration preroll, const ProdAuto::MxfDuration maxPreroll, const ProdAuto::MxfDuration postroll, const ProdAuto::MxfDuration maxPostroll)
: wxDialog(parent, wxID_ANY, (const wxString &) wxT("Preroll and Postroll")), mMaxPreroll(maxPreroll), mMaxPostroll(maxPostroll)
{
	wxBoxSizer * sizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(sizer);

	//sliders
	wxBoxSizer * sliderSizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(sliderSizer, 0, wxEXPAND | wxALL);
	mPrerollBox = new wxStaticBoxSizer(wxHORIZONTAL, this);
	sliderSizer->Add(mPrerollBox, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	mPrerollCtrl = new wxSlider(this, SLIDER_Preroll, 0, 0, 1000); //slider can't handle floats so calculate real value elsewhere
	mPrerollCtrl->SetValue(((preroll.samples * 2) + 1) * 500 / mMaxPreroll.samples); //prevent rounding down by putting in centre of range
	mPrerollBox->Add(mPrerollCtrl, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	mPostrollBox = new wxStaticBoxSizer(wxHORIZONTAL, this);
	sliderSizer->Add(mPostrollBox, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	mPostrollCtrl = new wxSlider(this, SLIDER_Postroll, 0, 0, 1000); //slider can't handle floats so calculate real value elsewhere
	mPostrollCtrl->SetValue(((postroll.samples * 2) + 1) * 500 / mMaxPostroll.samples); //prevent rounding down by putting in centre of range
	mPostrollBox->Add(mPostrollCtrl, 1, wxEXPAND | wxALL, CONTROL_BORDER);

	//buttons
	wxBoxSizer * buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(buttonSizer, 0, wxEXPAND | wxALL);
	buttonSizer->AddStretchSpacer();
	wxButton * okButton = new wxButton(this, wxID_OK, wxT("OK"));
	buttonSizer->Add(okButton, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	buttonSizer->AddStretchSpacer();
	wxButton * cancelButton = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
	buttonSizer->Add(cancelButton, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	buttonSizer->AddStretchSpacer();

	//update box text
	wxScrollEvent event;
	event.SetId(SLIDER_Preroll);
	OnRollChange(event);
	event.SetId(SLIDER_Postroll);
	OnRollChange(event);

	Fit(); //Makes the height correct but the width too small
	SetSize(wxDefaultCoord, wxDefaultCoord, 500, wxDefaultCoord); //makes the width sensible
}

/// Responds to a slider event by updating the relevant text.
/// @param event The slider event.
void SetRollsDlg::OnRollChange( wxScrollEvent& event )
{
	if (SLIDER_Preroll == event.GetId()) {
		GetPreroll();
	}
	else if (SLIDER_Postroll == event.GetId()) {
		GetPostroll();
	}
}

/// Sets the preroll box label to the current preroll slider value.
/// @return The preroll value.
const ProdAuto::MxfDuration SetRollsDlg::GetPreroll() {
	return SetRoll(wxT("Preroll"), mPrerollCtrl->GetValue(), mMaxPreroll, mPrerollBox);
}

/// Sets the postroll box label to the current postroll slider value.
/// @return The postroll value.
const ProdAuto::MxfDuration SetRollsDlg::GetPostroll() {
	return SetRoll(wxT("Postroll"), mPostrollCtrl->GetValue(), mMaxPostroll, mPostrollBox);
}

/// Sets a control's text.
/// @param label The displayed name of the parameter.
/// @param proportion The slider's position from 0-1000.
/// @param max The maximum value for the parameter.
/// @param control The control to be modified.
ProdAuto::MxfDuration SetRollsDlg::SetRoll(const wxChar * label, int proportion, const ProdAuto::MxfDuration & max, wxStaticBoxSizer * control)
{
	ProdAuto::MxfDuration roll = max; //for edit rates and defined flag
	roll.samples = max.samples * proportion / 1000;
	control->GetStaticBox()->SetLabel(wxString::Format(wxT("%s: %d fr (%3.1f s)"), label, roll.samples, roll.samples * roll.edit_rate.denominator / (float) roll.edit_rate.numerator));
	return roll;
}

BEGIN_EVENT_TABLE( SetProjectDlg, wxDialog )
	EVT_BUTTON(ADD, SetProjectDlg::OnAdd)
	EVT_BUTTON(DELETE, SetProjectDlg::OnDelete)
	EVT_BUTTON(EDIT, SetProjectDlg::OnEdit)
	EVT_CHOICE(wxID_ANY, SetProjectDlg::OnChoice)
END_EVENT_TABLE()

/// Reads project names from the XML document and displays a dialogue to manipulate them if there are none or if forced.
/// Updates the document if user changes data.
/// @param parent The parent window.
/// @param doc The XML document to read and modify.
/// @param force Set true to display the dialogue regardless.
SetProjectDlg::SetProjectDlg(wxWindow * parent, wxXmlDocument & doc, bool force) : wxDialog(parent, wxID_ANY, (const wxString &) wxT("Select a project name"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), mUpdated(false)
{
	wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mainSizer);
	wxStaticBoxSizer * mListSizer = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Project name for new recordings"));
	mainSizer->Add(mListSizer, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	mProjectList = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, NULL); //ListBox is nicer but can't scroll it to show a new project name
	mListSizer->Add(mProjectList, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	wxBoxSizer * buttonSizer1 = new wxBoxSizer(wxHORIZONTAL);
	mainSizer->Add(buttonSizer1, 0, wxEXPAND);
	wxButton * addButton = new wxButton(this, ADD, wxT("Add"));
	buttonSizer1->Add(addButton, 0, wxALL, CONTROL_BORDER);
	mDeleteButton = new wxButton(this, DELETE, wxT("Delete"));
	buttonSizer1->AddStretchSpacer();
	buttonSizer1->Add(mDeleteButton, 0, wxALL, CONTROL_BORDER);
	mEditButton = new wxButton(this, EDIT, wxT("Edit"));
	buttonSizer1->AddStretchSpacer();
	buttonSizer1->Add(mEditButton, 0, wxALL, CONTROL_BORDER);
	wxBoxSizer * buttonSizer2 = new wxBoxSizer(wxHORIZONTAL);
	mainSizer->Add(buttonSizer2, 1, wxEXPAND);
	mOKButton = new wxButton(this, wxID_OK, wxT("OK"));
	buttonSizer2->Add(mOKButton, 1, wxALL, CONTROL_BORDER);
	wxButton * CancelButton = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
	buttonSizer2->Add(CancelButton, 1, wxALL, CONTROL_BORDER);

	//read/create/recreate state from XML doc
	wxXmlNode * projectsNode = doc.GetRoot()->GetChildren();
	while (projectsNode && projectsNode->GetName() != wxT("Projects")) {
		projectsNode = projectsNode->GetNext();
	}
	if (!projectsNode) {
		projectsNode = new wxXmlNode(doc.GetRoot(), wxXML_ELEMENT_NODE, wxT("Projects"));
	}
	else {
		mSelectedProject = projectsNode->GetPropVal(wxT("selected"), wxT(""));
		wxXmlNode * projectNode = projectsNode->GetChildren();
		while (projectNode) {
			if (wxT("Project") == projectNode->GetName() && !IsEmpty(projectNode->GetNodeContent())) {
				mProjectList->Append(projectNode->GetNodeContent());
			}
			projectNode = projectNode->GetNext();
		}
	}
	if (!mProjectList->GetCount()) { //no projects
		//create default project
		wxDateTime now;
		now.SetToCurrent();
		mProjectList->Append(now.Format());
		new wxXmlNode(new wxXmlNode(projectsNode, wxXML_ELEMENT_NODE, wxT("Project")), wxXML_TEXT_NODE, wxT(""), mProjectList->GetString(0));
	}
	Fit();
	SetMinSize(GetSize());
	if (!mSelectedProject.IsEmpty()) {
		mProjectList->Select(mProjectList->FindString(mSelectedProject));
		if (wxNOT_FOUND == mProjectList->GetSelection()) {
			//duff selected project
			mSelectedProject.Clear();
		}
	}
	//selection and update
	if (mSelectedProject.IsEmpty() || force) {
		if (mSelectedProject.IsEmpty()) {
			//make default selection
			mProjectList->Select(mProjectList->GetCount() - 1);
			mSelectedProject = mProjectList->GetStringSelection();
			projectsNode->SetProperties(new wxXmlProperty(wxT("selected"), mSelectedProject)); //don't worry about any existing properties - this will only happen once when the app is run
			mUpdated = true;
		}
		if (wxID_OK == ShowModal()) {
			//delete existing projects
			doc.GetRoot()->RemoveChild(projectsNode); //also gets rid of any rubbish in the file
			delete projectsNode;
			//create new list of projects
			mSelectedProject = mProjectList->GetStringSelection();
			projectsNode = new wxXmlNode(doc.GetRoot(), wxXML_ELEMENT_NODE, wxT("Projects"), wxT(""), new wxXmlProperty(wxT("selected"), mSelectedProject));
			//add new projects
			for (unsigned int i = mProjectList->GetCount(); i > 0;) { //backwards to preserve order
				new wxXmlNode(new wxXmlNode(projectsNode, wxXML_ELEMENT_NODE, wxT("Project")), wxXML_TEXT_NODE, wxT(""), mProjectList->GetString(--i));
			}
			mUpdated = true;
		}
	}
}

/// Responds to the "Add" button.
/// @param event The button event.
void SetProjectDlg::OnAdd( wxCommandEvent& WXUNUSED( event ) )
{
	EnterName(wxT("Enter a new project name"), wxT("New Project"));
}

/// Responds to the "Edit" button.
/// @param event The button event.
void SetProjectDlg::OnEdit( wxCommandEvent& WXUNUSED( event ) )
{
	EnterName(wxT("Edit project name"), wxT("Edit Project"), mProjectList->GetSelection());
}

/// Adds or edits a project name via a sub-dialogue, checking for duplicate names.
/// Adjusts button enable states accordingly.
/// @param msg The instruction to the user.
/// @param caption The dialogue title.
/// @param item The item to edit, if editing.
void SetProjectDlg::EnterName(const wxString & msg, const wxString & caption, int item) //item defaults to wxNOT_FOUND
{
	while (true) {
		wxString name = wxGetTextFromUser(msg, caption, mProjectList->GetString(item));
		mProjectList->Delete(item); //so that searching for duplicates is accurate
		name = name.Trim(false);
		name = name.Trim(true);
		if (name.IsEmpty()) {
			break;
		}
		else if (wxNOT_FOUND == mProjectList->FindString(name)) { //case-insensitive match
			if (wxNOT_FOUND == item) { //adding a new name
				mProjectList->Append(name);
				mProjectList->Select(mProjectList->GetCount() - 1);
			}
			else {
				mProjectList->Insert(name, item);
				mProjectList->Select(item);
			}
			Fit();
			SetMinSize(GetSize());
			break;
		}
		else {
			wxMessageBox(wxT("There is already a project named \"") + mProjectList->GetString(mProjectList->FindString( name)) + wxT("\"."), wxT("Name Clash"), wxICON_HAND); //prints existing project name (may have case differences)
		}
	}
	EnableButtons(wxNOT_FOUND != mProjectList->GetSelection());
}

/// Responds to the "Delete" button.
/// Adjusts button enable states accordingly.
/// @param event The button event.
void SetProjectDlg::OnDelete( wxCommandEvent& WXUNUSED( event ) )
{
	mProjectList->Delete(mProjectList->GetSelection());
	//select the latest remaining project if possible
	if (mProjectList->GetCount()) {
		mProjectList->SetSelection(mProjectList->GetCount() - 1);
	}
	else {
		EnableButtons(false);
	}
}

/// Responds to a project name being chosen.
/// Adjusts button enable states accordingly.
/// @param event The choice event.
void SetProjectDlg::OnChoice( wxCommandEvent& WXUNUSED( event ) )
{
	//always a valid choice
	EnableButtons(true);
}

/// @return True if the user updated information while the dialogue was shown.
bool SetProjectDlg::IsUpdated()
{
	return mUpdated;
}

/// @return The name of the project that was selected when the dialogue was closed.
const wxString SetProjectDlg::GetSelectedProject()
{
	return mSelectedProject;
}

/// Enable or disable all dialogue's "Add", "Delete" and "Edit" buttons.
/// @param state True to enable.
void SetProjectDlg::EnableButtons(bool state)
{
	mDeleteButton->Enable(state);
	mEditButton->Enable(state);
	mOKButton->Enable(state);
}

BEGIN_EVENT_TABLE(SetTapeIdsDlg, wxDialog)
	EVT_GRID_EDITOR_HIDDEN(SetTapeIdsDlg::OnEditorHidden)
	EVT_GRID_EDITOR_SHOWN(SetTapeIdsDlg::OnEditorShown)
	EVT_GRID_RANGE_SELECT(SetTapeIdsDlg::OnCellRangeSelected) //for an area being dragged to select several cells
	EVT_BUTTON(wxID_HIGHEST + 1, SetTapeIdsDlg::OnAutoNumber)
	EVT_BUTTON(wxID_HIGHEST + 2, SetTapeIdsDlg::OnHelp)
END_EVENT_TABLE()
WX_DECLARE_STRING_HASH_MAP(wxString, StringHash);
WX_DECLARE_STRING_HASH_MAP(bool, BoolHash);

/// Reads tape IDs names from the XML document and displays a dialogue to manipulate them.
/// Displays in package name order within three groups: enabled for recording and no tape ID; not enabled and no tape ID; the rest.
/// Updates the document if user changes data.
/// @param parent The parent window.
/// @param doc The XML document to read and modify.
/// @param currentPackages Package names of connected recorders, in display order
/// @param enabled Value corresponding to each package name, which is true if any sources within that package are enabled for recording
SetTapeIdsDlg::SetTapeIdsDlg(wxWindow * parent, wxXmlDocument & doc, wxArrayString & currentPackages, std::vector<bool> & enabled) : wxDialog(parent, wxID_ANY, (const wxString &) wxT("Set tape IDs"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), mEditing(false), mUpdated(false)
{
	wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mainSizer);
//	wxStaticBoxSizer * gridBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Tape IDs"));
	mGrid = new wxGrid(this, wxID_ANY);
//	gridBox->Add(mGrid, 1, wxEXPAND);
//	gridBox->Add(mGrid, 2);
	mainSizer->Add(mGrid, 2, wxEXPAND);
//	mainSizer->Add(gridBox, 2, wxEXPAND);
//	mainSizer->Add(gridBox, 1, wxEXPAND);
	mGrid->CreateGrid(currentPackages.GetCount(), 2);
	mGrid->SetColLabelValue(0, wxT("Source Name"));
	mGrid->SetColLabelValue(1, wxT("Tape ID"));
	mGrid->SetRowLabelSize(0); //can't autoadjust this (?) so don't use it
	mGrid->SetSelectionMode(wxGrid::wxGridSelectRows);
	wxBoxSizer * buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	mainSizer->Add(buttonSizer, 0, wxEXPAND);
	wxButton * OKButton = new wxButton(this, wxID_OK, wxT("OK"));
	buttonSizer->Add(OKButton, 0, wxALL, CONTROL_BORDER);
	buttonSizer->AddStretchSpacer();
	mAutoButton = new wxButton(this, wxID_HIGHEST + 1, wxT("Auto Number"));
	mAutoButton->Disable();
	buttonSizer->Add(mAutoButton, 0, wxALL, CONTROL_BORDER);
	buttonSizer->AddStretchSpacer();
	wxButton * helpButton = new wxButton(this, wxID_HIGHEST + 2, wxT("Help"));
	buttonSizer->Add(helpButton, 0, wxALL, CONTROL_BORDER);
	buttonSizer->AddStretchSpacer();
	wxButton * cancelButton = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
	buttonSizer->Add(cancelButton, 0, wxALL, CONTROL_BORDER);

	wxXmlNode * tapeIdsNode = doc.GetRoot()->GetChildren();
	StringHash tapeIds;
	wxXmlNode * tapeIdNode;
	while (tapeIdsNode && tapeIdsNode->GetName() != wxT("TapeIds")) {
		tapeIdsNode = tapeIdsNode->GetNext();
	}
	if (!tapeIdsNode) {
		tapeIdsNode = new wxXmlNode(doc.GetRoot(), wxXML_ELEMENT_NODE, wxT("TapeIds"));
	}
	//make a map of package names to IDs
	else {
		tapeIdNode = tapeIdsNode->GetChildren();
		wxString packageName;
		while (tapeIdNode) {
			packageName = tapeIdNode->GetPropVal(wxT("PackageName"), wxT(""));
			if (wxT("TapeId") == tapeIdNode->GetName() && !packageName.IsEmpty() && !tapeIdNode->GetNodeContent().IsEmpty()) {
				tapeIds[packageName] = tapeIdNode->GetNodeContent();
			}
			tapeIdNode = tapeIdNode->GetNext();
		}
	}
	//list package names enabled for recording but with no tape ID
	int nextRow = 0;
	mEnabledPackages = new BoolHash;
	for (size_t index = 0; index < currentPackages.GetCount(); index++) {
		if (tapeIds.end() == tapeIds.find(currentPackages[index]) && enabled.at(index)) { //no tape ID and enabled for recording
			mGrid->SetCellBackgroundColour(nextRow, 1, wxColour(wxT("RED")));
			mGrid->SetCellValue(nextRow, 0, currentPackages[index]);
			mGrid->SetReadOnly(nextRow++, 0);
		}
		(*mEnabledPackages)[currentPackages[index]] = enabled.at(index);
	}
	//list package names with no tape ID but not enabled for recording
	for (size_t index = 0; index < currentPackages.GetCount(); index++) {
		if (tapeIds.end() == tapeIds.find(currentPackages[index]) && !enabled.at(index)) { //no tape ID but not enabled for recording
			mGrid->SetCellBackgroundColour(nextRow, 1, wxColour(wxT("YELLOW")));
			mGrid->SetCellValue(nextRow, 0, currentPackages[index]);
			mGrid->SetReadOnly(nextRow++, 0);
		}
	}
	//list package names with tape ID
	for (size_t index = 0; index < currentPackages.GetCount(); index++) {
		if (tapeIds.end() != tapeIds.find(currentPackages[index])) { //tape ID present
			mGrid->SetCellValue(nextRow, 0, currentPackages[index]);
			mGrid->SetCellValue(nextRow, 1, tapeIds[currentPackages[index]]);
			mGrid->SetReadOnly(nextRow++, 0);
			tapeIds.erase(currentPackages[index]); //don't pass straight back into XML file
		}
	}
	mGrid->AutoSizeColumns();

	if (wxID_OK == ShowModal()) {
		//delete existing projects
		doc.GetRoot()->RemoveChild(tapeIdsNode); //also gets rid of any rubbish in the file
		delete tapeIdsNode;
		//create new list of IDs
		tapeIdsNode = new wxXmlNode(doc.GetRoot(), wxXML_ELEMENT_NODE, wxT("TapeIds"));
		wxString tapeId;
		bool someIds = false;
		for (unsigned int i = 0; i < currentPackages.GetCount(); i++) {
			if (!mGrid->GetCellValue(i, 1).IsEmpty()) { //there's a tape ID
				//a new element node "TapeId" with property "PackageName", containing a content node with the tape ID
				new wxXmlNode(new wxXmlNode(tapeIdsNode, wxXML_ELEMENT_NODE, wxT("TapeId"), wxT(""), new wxXmlProperty(wxT("PackageName"), mGrid->GetCellValue(i, 0))), wxXML_TEXT_NODE, wxT(""), mGrid->GetCellValue(i, 1));
				someIds = true;
			}
		}
		//Add IDs not being used
		for (StringHash::iterator it = tapeIds.begin(); it != tapeIds.end(); it++) {
			new wxXmlNode(new wxXmlNode(tapeIdsNode, wxXML_ELEMENT_NODE, wxT("TapeId"), wxT(""), new wxXmlProperty(wxT("PackageName"), it->first)), wxXML_TEXT_NODE, wxT(""), it->second);
			someIds = true;
		}
		if (!someIds) {
			doc.GetRoot()->RemoveChild(tapeIdsNode);
			delete tapeIdsNode;
		}
		mUpdated = true;
	}
}

/// Deletes enabled packages hash to avoid memory leaks.
SetTapeIdsDlg::~SetTapeIdsDlg()
{
	delete mEnabledPackages;
}

/// Responds to a cell starting to be edited.
/// @param event The grid event.
void SetTapeIdsDlg::OnEditorShown(wxGridEvent & WXUNUSED(event))
{
	mEditing = true; //gets round a bug (?) which causes two wxEVT_GRID_EDITOR_HIDDEN events to be emitted when editing stops
}

/// Responds to completion of cell editing.
/// Works round an apparent bug in the grid control which results in two events being issued when this happens.
/// Warns of duplicate tape IDs.
/// Sets cell background colour according to contents.
/// @param event The grid event.
void SetTapeIdsDlg::OnEditorHidden(wxGridEvent & event)
{
	if (mEditing) {
		mEditing = false;
		//reformat
		mGrid->AutoSizeColumn(1, true);
		mGrid->ForceRefresh(); //sometimes makes a mess if you don't
		//check for duplicate IDs
		for (size_t i = 0; i < mEnabledPackages->size(); i++) {
				if (event.GetRow() != (int) i && !mGrid->GetCellValue(event.GetRow(), 1).IsEmpty() && mGrid->GetCellValue(event.GetRow(), 1) == mGrid->GetCellValue(i, 1)) {
				wxMessageBox(wxT("Duplicate tape ID"), wxT("Warning"), wxICON_HAND);
				break;
			}
		}
		//set correct cell colour
		if (mGrid->GetCellValue(event.GetRow(), 1).IsEmpty()) { //no tape ID
			if ((*mEnabledPackages)[mGrid->GetCellValue(event.GetRow(), 0)]) { //enabled for recording
				mGrid->SetCellBackgroundColour(event.GetRow(), 1, wxColour(wxT("RED")));
			}
			else {
				mGrid->SetCellBackgroundColour(event.GetRow(), 1, wxColour(wxT("YELLOW")));
			}
		}
		else { //tape ID
			mGrid->SetCellBackgroundColour(event.GetRow(), 1, wxColour(wxT("WHITE")));
		}
		mAutoButton->Enable(AutoNumber(false));
	}
}

/// Responds to a range of cells being selected.
/// Enables the "Auto Number" button if automatic numbering is possible.
/// @param event The grid event.
void SetTapeIdsDlg::OnCellRangeSelected(wxGridRangeSelectEvent & event)
{
	mAutoButton->Enable(AutoNumber(false));
}

/// Responds to the "Auto Number" button being pressed.
/// Auto-numbers and disables the button.
/// @param event The button event.
void SetTapeIdsDlg::OnAutoNumber(wxCommandEvent & WXUNUSED(event))
{
	AutoNumber(true);
	mAutoButton->Enable(false);
}

/// Responds to the "Help" button being pressed.
/// Opens a modal help dialogue.
/// @param event The button event.
void SetTapeIdsDlg::OnHelp(wxCommandEvent & WXUNUSED(event))
{
	wxMessageBox(wxT("The source names of the recorders to which you are connected are displayed in three groups.  "
		"Within these groups, the sources are ordered firstly by recorder name (alphabetically), and then by the order in which they are connected to each recorder.\n\n"
		"The first group of sources comprises those which are currently at least partially enabled for recording but which have no tape ID.  "
		"As a result, these are preventing recordings being made.  Their (empty) tape ID fields have a red background.\n\n"
		"Below these are any non-enabled sources with no tape ID (which will prevent recording should you enable them).  Their tape ID fields have a yellow background.\n\n"
		"Finally, there are the sources which already have tape IDs (white background).\n\n"
		"To enter or edit a tape ID, double-click in the corresponding field.  To finish editing, press ENTER or click in another cell.  You will be warned if there are duplicate IDs but you are not prevented from using them.  Editing a tape ID will not change its position in the list (you have to press OK and re-open the dialogue for that).\n\n"
		"Automatic numbering allows you to apply an incrementing tape ID to several sources which you select using single click, shift-click, control-click or dragging over an area.  "
		"This will enable the Auto Number button if the uppermost selected source has a suitable ID to increment (i.e. something ending in a number), and subsequent cells would be modified by an automatic numbering operation.  "
		"Pressing the button will give all the other selected sources incremented IDs based on the uppermost source, ordered by their position in the list.  "
		"All the sources you want to affect will therefore probably have to be in the same state (enabled or disabled) for them to be ordered correctly."), wxT("Set Tape IDs dialogue"));
}


/// Responds to the "Auto Number" button being pressed.
/// Automatically increments tape IDs of selected cells based on the value in the uppermost selected cell.
/// @param commit True to make changes
/// @return True if auto numbering would/did change anything
bool SetTapeIdsDlg::AutoNumber(bool commit)
{
	bool changing;
	//get the original tape ID to copy
	int row = mGrid->GetNumberRows();
	int block;
	for (size_t i = 0; i < mGrid->GetSelectionBlockTopLeft().GetCount(); i++) {
		if (mGrid->GetSelectionBlockTopLeft()[i].GetRow() < row) { //found a higher block
 			row = mGrid->GetSelectionBlockTopLeft()[i].GetRow();
			block = i;
		}
	}
	changing = (row != mGrid->GetNumberRows());
	//split the original tape ID into leading text and trailing digits
	wxString characters;
	wxULongLong_t number;
	size_t nDigits = 1;
	if (changing) { //something's selected
		characters = mGrid->GetCellValue(row, 1);
		while (nDigits <= characters.Length() //haven't got the whole string
		 && wxT("-") != characters.Right(nDigits).Left(1) //don't want a dash being interpreted as a negative number!
		 && wxT(" ") != characters.Right(nDigits).Left(1) //detect a space because ToULongLong() doesn't mind them
		 && characters.Right(nDigits).ToULongLong(&number)) //what we have so far is a valid number
		{ 
			nDigits++;
		}
		changing = --nDigits;
	}
	//change the values
	if (changing) { //the source cell contents ends in a digit
		characters.Right(nDigits).ToULongLong(&number); //starting number
		characters = characters.Left(characters.Length() - nDigits); //leading text
		wxString numberFormat = wxString::Format(wxT("%%0%d"), nDigits); //handle leading zeros by printing the number in a field with leading zeros at least the size of the starting number
		//find other selected cells in order and replace contents
		int nextRow;
		row++; //start looking one below the source row
		changing = false; //not found anything to change yet
		wxString newValue;
		while (-1 != block) {
			//go through current block
			for (; row <= mGrid->GetSelectionBlockBottomRight()[block].GetRow(); row++) {
				newValue = characters + wxString::Format(numberFormat + wxT(wxLongLongFmtSpec "u"), ++number);
				if (mGrid->GetCellValue(row, 1) != newValue) {
					changing = true;
					if (commit) { //actually autonumbering
						mGrid->SetCellValue(row, 1, newValue);
						mGrid->SetCellBackgroundColour(row, 1, wxColour(wxT("WHITE"))); //it's got a tape ID
					}
					else {
						break; //we know all we need to know
					}
				}
			}
			if (changing && !commit) {
				break; //we know all we need to know
			}
			//find next block
			block = -1;
			nextRow = mGrid->GetNumberRows();
			for (size_t i = 0; i < mGrid->GetSelectionBlockTopLeft().GetCount(); i++) { //block counter
				if (mGrid->GetSelectionBlockTopLeft()[i].GetRow() < nextRow && mGrid->GetSelectionBlockTopLeft()[i].GetRow() >= row) {
					//found a lower row which isn't lower than one we've already used
					nextRow = mGrid->GetSelectionBlockTopLeft()[i].GetRow();
					block = i;
				}
			}
			row = nextRow;
		}
	}
	return changing;
}

/// @return True if the user updated information while the dialogue was shown.
bool SetTapeIdsDlg::IsUpdated()
{
	return mUpdated;
}
