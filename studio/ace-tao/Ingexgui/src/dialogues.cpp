/***************************************************************************
 *   $Id: dialogues.cpp,v 1.31 2012/02/10 15:12:55 john_f Exp $           *
 *                                                                         *
 *   Copyright (C) 2006-2012 British Broadcasting Corporation              *
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

#include "dialogues.h"
#include "help.h" //for StyleAndWrite()
#include "savedstate.h"
#include <wx/spinctrl.h>
#include <wx/tglbtn.h>
#include <wx/colordlg.h>
#include <wx/filename.h>
#include <wx/dir.h>
#include "colours.h"

#include "up.xpm"
#include "down.xpm"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(SetRollsDlg, wxDialog)
    EVT_SCROLL(SetRollsDlg::OnRollChange)
END_EVENT_TABLE()

/// Sets up dialogue.
/// @param parent The parent window.
/// @param preroll The current preroll value.
/// @param maxPreroll The maximum allowed preroll.
/// @param postroll The current postroll value.
/// @param maxPostroll The maximum allowed postroll.
SetRollsDlg::SetRollsDlg(wxWindow * parent, const ProdAuto::MxfDuration preroll, const ProdAuto::MxfDuration maxPreroll, const ProdAuto::MxfDuration postroll, const ProdAuto::MxfDuration maxPostroll, SavedState * savedState)
: wxDialog(parent, wxID_ANY, (const wxString &) wxT("Pre-roll and Post-roll")), mMaxPreroll(maxPreroll), mMaxPostroll(maxPostroll), mSavedState(savedState)
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
    return SetRoll(wxT("Pre-roll"), mPrerollCtrl->GetValue(), mMaxPreroll, mPrerollBox);
}

/// Sets the postroll box label to the current postroll slider value.
/// @return The postroll value.
const ProdAuto::MxfDuration SetRollsDlg::GetPostroll() {
    return SetRoll(wxT("Post-roll"), mPostrollCtrl->GetValue(), mMaxPostroll, mPostrollBox);
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

/// Overloads Show Modal to update the XML document
int SetRollsDlg::ShowModal()
{
    int rc = wxDialog::ShowModal();
    if (wxID_OK == rc) {
        mSavedState->SetUnsignedLongValue(wxT("Preroll"), GetPreroll().samples);
        mSavedState->SetUnsignedLongValue(wxT("Postroll"), GetPostroll().samples);
    }
    return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE( SetProjectDlg, wxDialog )
    EVT_BUTTON(ADD, SetProjectDlg::OnAdd)
    EVT_BUTTON(DELETE, SetProjectDlg::OnDelete)
    EVT_BUTTON(EDIT, SetProjectDlg::OnEdit)
    EVT_CHOICE(wxID_ANY, SetProjectDlg::OnChoice)
END_EVENT_TABLE()

/// Waits for list of project names from the recorders, then displays it and the selected project from the XML doc and allows the list to be added to.
/// Updates the document if user changes the selected project.
/// @param parent The parent window.
SetProjectDlg::SetProjectDlg(wxWindow * parent) : wxDialog(parent, wxID_ANY, (const wxString &) wxT("Select a project name"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(mainSizer);
    wxStaticBoxSizer * mListSizer = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Project name for new recordings"));
    mainSizer->Add(mListSizer, 0, wxEXPAND | wxALL, CONTROL_BORDER);
    mProjectList = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize); //ListBox is nicer but can't scroll it to show a new project name
    mListSizer->Add(mProjectList, 1, wxEXPAND | wxALL, CONTROL_BORDER);
    wxBoxSizer * buttonSizer1 = new wxBoxSizer(wxHORIZONTAL);
    mainSizer->Add(buttonSizer1, 0, wxEXPAND);
    mAddButton = new wxButton(this, ADD, wxT("Add"));
    buttonSizer1->Add(mAddButton, 0, wxALL, CONTROL_BORDER);
//  buttonSizer1->AddStretchSpacer();
//  mDeleteButton = new wxButton(this, DELETE, wxT("Delete"));
//  buttonSizer1->AddStretchSpacer();
//  buttonSizer1->Add(mDeleteButton, 0, wxALL, CONTROL_BORDER);
//  mEditButton = new wxButton(this, EDIT, wxT("Edit"));
//  buttonSizer1->AddStretchSpacer();
//  buttonSizer1->Add(mEditButton, 0, wxALL, CONTROL_BORDER);
    wxBoxSizer * buttonSizer2 = new wxBoxSizer(wxHORIZONTAL);
    mainSizer->Add(buttonSizer2, 1, wxEXPAND);
    mOKButton = new wxButton(this, wxID_OK, wxT("OK"));
    buttonSizer2->Add(mOKButton, 1, wxALL, CONTROL_BORDER);
    wxButton * CancelButton = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
    buttonSizer2->Add(CancelButton, 1, wxALL, CONTROL_BORDER);
    mProjectList->Append(wxT("Getting project names..."));
    mProjectList->Select(0);
    mProjectList->Disable();
    EnableButtons(false); //wait until SetProjectNames() is called
}

/// If list of names supplied, removes holding message and populates the list of project names.  If list not empty, highlights the current project name (if any), and makes dialogue fully active; if list empty, asks user for a project name and ends modal with cancel if one isn't entered.
/// If list not supplied, warns the user and (if dialogue showing modally) ends modal with cancel.
/// If dialogue is left active, this method will do nothing if called again.
/// @param projectNames The current list of project names (can be empty).
/// @param currentProjectName The current project name (can be empty).
void SetProjectDlg::SetProjectNames(CORBA::StringSeq_var projectNames, const wxString & currentProjectName)
{
    if (!projectNames) {
        wxMessageDialog dlg(this, wxT("Cannot get list of project names from recorder.  It may work if you try again."), wxT("Communication error"), wxOK | wxICON_ERROR);
        dlg.ShowModal();
        if (IsModal()) {
            EndModal(wxID_CANCEL);
        }
    }
    else if (!mProjectList->IsEnabled()) {
        mProjectList->Enable();
        mProjectList->Clear(); //remove holding message
        for (size_t i = 0; i < projectNames->length(); i++) {
            mProjectNames.Add(wxString((*projectNames)[i], wxConvLibc));
        }
        mProjectNames.Sort();
        mProjectList->Append(mProjectNames);
        if (currentProjectName.IsEmpty() && mProjectList->GetCount()) { //recorder has returned project names but no selected project from the saved state
            mProjectList->SetSelection(0); //just select something as a default project
        }
        else {
            mProjectList->SetSelection(mProjectNames.Index(currentProjectName)); //could be nothing if no match
        }
        Fit();
        SetMinSize(GetSize());
        mAddButton->Enable();
        mOKButton->Enable();
        mOKButton->SetFocus(); //especially useful to get through the dialogue quickly when it immediately brings up an add project box when there are no project names

        if (!mProjectList->GetCount()) { //must enter at least one project name
            EnterName(wxT("No project names stored: enter a project name"), wxT("New Project"));
        }
        if (!mProjectList->GetCount()) { //no existing names and user has refused to enter one
            EndModal(wxID_CANCEL);
        }
    }
}

/// Returns the currently selected project name.
const wxString SetProjectDlg::GetCurrentProjectName()
{
    return mProjectList->GetStringSelection();
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
/// @param item The item to edit, if editing.  NOT FULLY IMPLEMENTED
void SetProjectDlg::EnterName(const wxString & msg, const wxString & caption, int item)
{
    while (true) {
        wxString name = wxGetTextFromUser(msg, caption, mProjectList->GetString(item), this);
        name = name.Trim(true).Trim(false);
        if (name.IsEmpty()) {
            break;
        }
        mProjectList->Delete(item); //so that searching for duplicates is accurate
        if (wxNOT_FOUND == mProjectList->FindString(name)) { //case-insensitive match
            if (wxNOT_FOUND == item) { //adding a new name
                mProjectNames.Add(name);
                mProjectNames.Sort();
                int index = mProjectNames.Index(name);
                mProjectList->Insert(name, index);
                mProjectList->Select(index);
                mNewProjectNames.Add(name);
            }
            else { //Not fully implemented for editing
                mProjectList->Insert(name, item);
                mProjectList->Select(item);
            }
            Fit();
            SetMinSize(GetSize());
            break;
        }
        else {
            wxMessageDialog dlg(this, wxT("There is already a project named \"") + mProjectList->GetString(mProjectList->FindString( name)) + wxT("\"."), wxT("Name Clash"), wxICON_HAND | wxOK); //prints existing project name (may have case differences)
            dlg.ShowModal();
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

/// @return The name of the project that was selected when the dialogue was closed.
const wxString SetProjectDlg::GetSelectedProject()
{
    return mProjectList->GetStringSelection();
}

/// @return Sorted list of project names.
const CORBA::StringSeq SetProjectDlg::GetNewProjectNames()
{
    CORBA::StringSeq corbaNames;
    for (size_t i = 0; i < mNewProjectNames.GetCount(); i++) {
            corbaNames.length(corbaNames.length() + 1);
            // Assignment to the CORBA::StringSeq element must be from a const char *
            // and should use ISO Latin-1 character set.
            corbaNames[corbaNames.length() - 1] = (const char *) mNewProjectNames[i].mb_str(wxConvLibc);
    }
    return corbaNames;
}

/// Enable or disable all dialogue's "Add", "Delete" and "Edit" buttons.
/// @param state True to enable.
void SetProjectDlg::EnableButtons(bool state)
{
    mAddButton->Enable(state);
//  mDeleteButton->Enable(state);
//  mEditButton->Enable(state);
    mOKButton->Enable(state);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(MyGrid, wxGrid)
    EVT_CHAR(MyGrid::OnChar)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(SetTapeIdsDlg, wxDialog)
    EVT_GRID_EDITOR_HIDDEN(SetTapeIdsDlg::OnEditorHidden)
    EVT_GRID_EDITOR_SHOWN(SetTapeIdsDlg::OnEditorShown)
    EVT_GRID_CELL_CHANGE(SetTapeIdsDlg::OnCellChange)
    EVT_GRID_CELL_LEFT_CLICK(SetTapeIdsDlg::OnCellLeftClick)
    EVT_GRID_LABEL_LEFT_CLICK(SetTapeIdsDlg::OnLabelLeftClick)
    EVT_GRID_RANGE_SELECT(SetTapeIdsDlg::OnCellRangeSelected) //for an area being dragged to select several cells
    EVT_BUTTON(FILLCOPY, SetTapeIdsDlg::OnFillCopy)
    EVT_BUTTON(FILLINC, SetTapeIdsDlg::OnFillInc)
    EVT_BUTTON(INCREMENT, SetTapeIdsDlg::OnIncrement)
    EVT_BUTTON(GROUPINC, SetTapeIdsDlg::OnGroupIncrement)
    EVT_BUTTON(HELP, SetTapeIdsDlg::OnHelp)
    EVT_BUTTON(CLEAR, SetTapeIdsDlg::OnClear)
    EVT_INIT_DIALOG(SetTapeIdsDlg::OnInitDlg)
END_EVENT_TABLE()

WX_DECLARE_STRING_HASH_MAP(bool, BoolHash2);

/// Reads tape IDs names from the XML document and displays a dialogue to manipulate them.
/// Displays in package name order with background colours corresponding to enabled for recording and no tape ID; not enabled and no tape ID; the rest.
/// Duplicated names are combined, with the background colour reflecting the most abnormal state.
/// Updates the saved state if user changes data.
/// @param parent The parent window.
/// @param savedState Persistent data to read and modify.
/// @param currentPackages Package names of connected recorders, in display order.
/// @param enabled Value corresponding to each package name, which is true if any sources within that package are enabled for recording.
SetTapeIdsDlg::SetTapeIdsDlg(wxWindow * parent, SavedState * savedState, wxArrayString & currentPackages, std::vector<bool> & enabled) : wxDialog(parent, wxID_ANY, (const wxString &) wxT("Set tape IDs"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), mEditing(false), mUpdated(false)
{
    //controls
    wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(mainSizer);
    wxBoxSizer * upperSizer = new wxBoxSizer(wxHORIZONTAL);
    mainSizer->Add(upperSizer, 0, wxEXPAND);
    mGrid = new MyGrid(this, wxID_ANY);
    upperSizer->Add(mGrid, 0, wxALL, CONTROL_BORDER);
    mGrid->CreateGrid(currentPackages.GetCount(), 7);
    mGrid->SetColLabelValue(0, wxT("Source Name"));
    mGrid->SetColLabelValue(1, wxT("Tape ID:"));
    mGrid->SetColLabelValue(2, wxT("Part 1"));
    mGrid->SetColLabelValue(3, wxT("Part 2"));
    mGrid->SetColLabelValue(4, wxT("Part 3"));
    mGrid->SetColLabelValue(5, wxT("Part 4"));
    mGrid->SetColLabelValue(6, wxT("Part 5"));
    mGrid->SetRowLabelSize(0); //can't autoadjust this (?) so don't use it
    mGrid->SetGridCursor(0, 2); //first two columns can't be edited so don't leave the cursor in the top left hand corner
    wxBoxSizer * rightHandSizer = new wxBoxSizer(wxVERTICAL);
    upperSizer->Add(rightHandSizer, 0, wxALL, CONTROL_BORDER);
    mDupWarning = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH | wxTE_CENTRE | wxBORDER_NONE); //don't think you can set background colour of static text - it would be much easier...
    mDupWarning->SetDefaultStyle(wxTextAttr(*wxRED, wxColour(wxT("YELLOW"))));
    mDupWarning->AppendText(wxT("Duplicate tape IDs"));
    mDupWarning->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BACKGROUND));
    rightHandSizer->Add(mDupWarning, 0, wxALL | wxALIGN_CENTRE_VERTICAL, CONTROL_BORDER);
    mIncrementButton = new wxButton(this, INCREMENT, wxT("Increment selected"));
    mIncrementButton->Disable();
    rightHandSizer->Add(mIncrementButton, 0, wxEXPAND);
    rightHandSizer->AddSpacer(CONTROL_BORDER);
    mGroupIncButton = new wxButton(this, GROUPINC, wxT("Increment as group"));
    mGroupIncButton->Disable();
    rightHandSizer->Add(mGroupIncButton, 0, wxEXPAND);
    wxGridSizer * buttonSizer = new wxGridSizer(2, 3, CONTROL_BORDER, CONTROL_BORDER);
    mainSizer->Add(buttonSizer, 0, wxALL, CONTROL_BORDER);
    wxButton * cancelButton = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
    buttonSizer->Add(cancelButton, 0, wxEXPAND);
    wxButton * helpButton = new wxButton(this, HELP, wxT("Help"));
    buttonSizer->Add(helpButton, 0, wxEXPAND);
    wxButton * OKButton = new wxButton(this, wxID_OK, wxT("OK"));
    buttonSizer->Add(OKButton, 0, wxEXPAND);
    mFillCopyButton = new wxButton(this, FILLCOPY, wxT("Fill column with copies"));
    mFillCopyButton->Disable();
    buttonSizer->Add(mFillCopyButton, 0, wxEXPAND);
    mFillIncButton = new wxButton(this, FILLINC, wxT("Fill column by incrementing"));
    mFillIncButton->Disable();
    buttonSizer->Add(mFillIncButton, 0, wxEXPAND);
    mClearButton = new wxButton(this, CLEAR, wxT("Clear selected"));
    mClearButton->Disable();
    buttonSizer->Add(mClearButton, 0, wxEXPAND);

    //aggregate enable values for duplicated package names, and allow sorting
    BoolHash2 enables;
    for (size_t i = 0; i < currentPackages.GetCount(); i++) {
        enables[currentPackages[i]] = (enables.end() == enables.find(currentPackages[i]) ? enabled.at(i) : (enables[currentPackages[i]] || enabled.at(i))); //if package name already encountered, set value to true if any enables for this package name are true
    }

    //populate the table with package names in alphabetical order and not duplicated
    currentPackages.Sort();
    size_t row = 0;
    for (size_t index = 0; index < currentPackages.GetCount(); index++) {
        if (0 == index || currentPackages[index] != currentPackages[index - 1]) { //first instance of this name
            mGrid->SetCellValue(row, 0, currentPackages[index]); //source name
            mGrid->SetReadOnly(row, 0);
            wxArrayString sections;
            sections.Insert(wxEmptyString, 0, mGrid->GetNumberCols() - 2); //set the number of sections
            mGrid->SetCellValue(row, 1, GetTapeId(savedState, currentPackages[index], &sections));
            mGrid->SetReadOnly(row, 1);
            mGrid->SetCellBackgroundColour(row, 1, wxColour(wxT("GREY"))); //updated in a mo
            for (size_t section = 0; section < sections.GetCount(); section++) {
                mGrid->SetCellValue(row, section + 2, sections[section]);
            }
            mEnabled.push_back(enables[currentPackages[index]]);
            SetBackgroundColour(row++);
        }
    }

    mGrid->AutoSizeColumns();
    Fit();
    SetMinSize(GetSize());

    //show dialogue and save results
    if (wxID_OK == ShowModal()) {
        for (int i = 0; i < mGrid->GetNumberRows(); i++) {
            //remove any existing node(s) for this source
            wxXmlNode * tapeIdsNode = savedState->GetTopLevelNode(TAPE_IDS_NODE_NAME, true);
            wxXmlNode * tapeIdNode = tapeIdsNode->GetChildren();
            while (tapeIdNode) {
                if (mGrid->GetCellValue(i, 0) == tapeIdNode->GetPropVal(wxT("PackageName"), wxEmptyString)) {
                    wxXmlNode * deadNode = tapeIdNode;
                    tapeIdNode = tapeIdNode->GetNext();
                    tapeIdsNode->RemoveChild(deadNode);
                    delete deadNode;
                }
                else {
                    tapeIdNode = tapeIdNode->GetNext();
                }
            }
            if (!mGrid->GetCellValue(i, 1).IsEmpty()) {
                //a new element node "TapeId" with property "PackageName"
                tapeIdNode = new wxXmlNode(tapeIdsNode, wxXML_ELEMENT_NODE, wxT("TapeId"), wxEmptyString, new wxXmlProperty(wxT("PackageName"), mGrid->GetCellValue(i, 0)));
                for (int sectionCol = 2; sectionCol < mGrid->GetNumberCols(); sectionCol++) {
                    //a new element node "Section" containing a new text node with the section of the tape ID
                    new wxXmlNode(new wxXmlNode(tapeIdNode, wxXML_ELEMENT_NODE, wxT("Section"), wxEmptyString, new wxXmlProperty(wxT("Number"), wxString::Format(wxT("%d"), sectionCol - 1))), wxXML_TEXT_NODE, wxEmptyString, mGrid->GetCellValue(i, sectionCol));
                }
            }
        }
        mUpdated = true;
        savedState->Save();
    }
}

/// Returns the enabled state of tape IDs.  Default true if state not stored.
/// @param savedState The persistent data containing the setting.  Not checked for existence.
bool SetTapeIdsDlg::AreTapeIdsEnabled(SavedState * savedState)
{
    return savedState->GetBoolValue(TAPE_IDS_NODE_NAME, true, wxT("Enabled"));
}

/// Saves the enabled state of tape IDs.
/// @param savedState The persistent data to which to save the setting.
void SetTapeIdsDlg::EnableTapeIds(SavedState * savedState, bool enabled)
{
    savedState->SetBoolValue(TAPE_IDS_NODE_NAME, enabled, wxT("Enabled"));
}

/// Gets the tape ID for the given package name from the children of the given node.
/// @param savedState The persistent data containing the setting.  Not checked for existence.
/// @param packageName The package name to search for.
/// @param sections Array to be populated with the sections of the tape ID.  If there are more sections than the size of the array, the extras will be placed in the last element.
/// @return The complete tape ID (empty string if not found).
const wxString SetTapeIdsDlg::GetTapeId(SavedState * savedState, const wxString & packageName, wxArrayString * sections)
{
    wxString fullName;
    wxArrayString internal;
    size_t nSections;
    if (sections) { //array supplied
        nSections = sections->GetCount();
    }
    else {
        //use our own array
        sections = &internal;
        nSections = 0; //indicates unlimited
    }
    wxXmlNode * tapeIdsNode = savedState->GetTopLevelNode(TAPE_IDS_NODE_NAME, true);
    wxXmlNode * tapeIdNode = tapeIdsNode->GetChildren();
    while (tapeIdNode) { //assumes all nodes at this level are called "TapeId"
        if (packageName == tapeIdNode->GetPropVal(wxT("PackageName"), wxEmptyString)) { //found it!
            wxXmlNode * sectionNode = tapeIdNode->GetChildren();
            while (sectionNode) {
                unsigned long sectionNumber;
                if (wxT("Section") == sectionNode->GetName() && sectionNode->GetPropVal(wxT("Number"), wxT("X")).ToULong(&sectionNumber) && sectionNumber > 0) {
                    if (sections->GetCount() < sectionNumber) sections->SetCount(sectionNumber); //make sure the array's big enough
                    (*sections)[sectionNumber - 1] = sectionNode->GetNodeContent().Trim(false).Trim(true);
                }
                sectionNode = sectionNode->GetNext();
            }
            for (size_t i = 0; i < sections->GetCount(); i++) {
                fullName += (*sections)[i];
            }
            if (!fullName.IsEmpty()) {
                if (nSections) { //a section limit
                    //put any extra sections in the last section
                    while (sections->GetCount() > nSections) {
                        (*sections)[nSections - 1] += (*sections)[nSections];
                        sections->RemoveAt(nSections);
                    }
                }
                break; //no need to look further for the XML tape ID
            }
            else { //duff data for this node
                //remove the node
                wxXmlNode * deadNode = tapeIdNode;
                tapeIdNode = tapeIdNode->GetNext();
                tapeIdsNode->RemoveChild(deadNode);
                delete deadNode;
            }
        }
        else { //not found it yet
            tapeIdNode = tapeIdNode->GetNext();
        }
    }
    return fullName;
}

/// Sets the background colour of the full tape ID cell of the given row, depending on presence/absence of content, and enabled status.
/// @param row The row to affect.
void SetTapeIdsDlg::SetBackgroundColour(int row)
{
    if (mGrid->GetCellValue(row, 1).IsEmpty() && mEnabled.at(row)) { //no tape ID and enabled for recording, so preventing a recording
            mGrid->SetCellBackgroundColour(row, 0, wxColour(wxT("RED")));
    }
    else if (mGrid->GetCellValue(row, 1).IsEmpty()) { //no tape ID but not enabled for recording
        mGrid->SetCellBackgroundColour(row, 0, wxColour(wxT("YELLOW")));
    }
    else { //all OK
        mGrid->SetCellBackgroundColour(row, 0, wxColour(wxT("GREY")));
    }
}

/// Responds to the dialogue intialising by showing or hiding the duplicate tape warning.
/// Need to call it here because it doesn't subsequently display in the right place if hidden before dialogue initialisation.
/// @param event The dialog init event.
void SetTapeIdsDlg::OnInitDlg(wxInitDialogEvent & WXUNUSED(event))
{
    CheckForDuplicates();
}

/// Responds to a cell starting to be edited.
/// @param event The grid event.
void SetTapeIdsDlg::OnEditorShown(wxGridEvent & WXUNUSED(event))
{
    mEditing = true; //gets round a bug (?) which causes two wxEVT_GRID_EDITOR_HIDDEN events to be emitted when editing stops
}

/// Responds to cell content changing (only called when cell loses focus)
/// Removes leading and trailing spaces, updates the corresponding complete tape ID cell and the source name background colour, and checks for duplicate tape Ids
/// @param event The grid event.
void SetTapeIdsDlg::OnCellChange(wxGridEvent & event)
{
    mGrid->SetCellValue(event.GetRow(), event.GetCol(), mGrid->GetCellValue(event.GetRow(), event.GetCol()).Trim(false).Trim(true));
    UpdateRow(event.GetRow());
    CheckForDuplicates();
}

/// Updates the complete name cell and background colour from its row values etc.
/// @param row The grid row to update.
void SetTapeIdsDlg::UpdateRow(const int row) {
    wxString fullName;
    for (int i = 2; i < mGrid->GetNumberCols(); i++) {
        fullName += mGrid->GetCellValue(row, i);
    }
    if (mGrid->GetCellValue(row, 1) != fullName) {
        mGrid->SetCellValue(row, 1, fullName);
        SetBackgroundColour(row);
    }
}

/// Checks grid for duplicate tape IDs and shows or hides warning accordingly
void SetTapeIdsDlg::CheckForDuplicates()
{
    for (int i = 0; i < mGrid->GetNumberRows() - 1; i++) {
        if (!mGrid->GetCellValue(i, 1).IsEmpty()) {
            for (int j = i + 1; j < mGrid->GetNumberRows(); j++) {
                if (mGrid->GetCellValue(i, 1) == mGrid->GetCellValue(j, 1)) {
                    mDupWarning->Show();
                    return;
                }
            }
        }
    }
    mDupWarning->Hide();
}


/// Responds to completion of cell editing.
/// Works round an apparent bug in the grid control which results in two events being issued when this happens.
/// Warns of duplicate tape IDs.
/// Sets cell background colour according to contents.
/// @param event The grid event.
void SetTapeIdsDlg::OnEditorHidden(wxGridEvent & WXUNUSED(event))
{
    mGrid->AutoSizeColumns();
//  mGrid->ForceRefresh(); //sometimes makes a mess if you don't
    Fit();
}

/// Responds to a left click in a cell
/// Prevents the cursor moving to the first two columns (which aren't editable), moving it to the third column instead.
/// @param event The grid event.
void SetTapeIdsDlg::OnCellLeftClick(wxGridEvent & event)
{
    if (event.GetCol() < 2) {
        mGrid->SetGridCursor(event.GetRow(), 2);
    }
    else {
        event.Skip();
    }
}

/// Responds to a left click in a label
/// Traps the click if it's in the first two columns, as these can't be selected.
/// @param event The grid event.
void SetTapeIdsDlg::OnLabelLeftClick(wxGridEvent & event)
{
    if (event.GetCol() >= 2) {
        event.Skip();
    }
}

/// Responds to the fill copy button being pressed, by filling empty cells in the selected column with copies of its top cell
void SetTapeIdsDlg::OnFillCopy(wxCommandEvent & WXUNUSED(event))
{
    FillCol(true, false);
    CheckForDuplicates();
}

/// Responds to the fill increment button being pressed, by filling empty cells in the selected column with copies of its top cell, incrementing each time
void SetTapeIdsDlg::OnFillInc(wxCommandEvent & WXUNUSED(event))
{
    FillCol(false, true);
    CheckForDuplicates();
}

/// Responds to the increment button being pressed, by incrementing all suitable selected cells.
void SetTapeIdsDlg::OnIncrement(wxCommandEvent & WXUNUSED(event))
{
    ManipulateCells(true, true); //do changes
    CheckForDuplicates();
}

/// Responds to the group increment button being pressed, by incrementing all non-blank cells in selected column as a group.
void SetTapeIdsDlg::OnGroupIncrement(wxCommandEvent & WXUNUSED(event))
{
    IncrementAsGroup(true); //do changes
    CheckForDuplicates();
}

/// Responds to the clear button being pressed, by clearing all selected cells.
void SetTapeIdsDlg::OnClear(wxCommandEvent & WXUNUSED(event))
{
    ManipulateCells(false, true); //do changes
    mClearButton->Disable();
    CheckForDuplicates();
}

/// Responds to a range of cells being selected.
/// Sets enable states of the buttons.
/// @param event The grid event.
void SetTapeIdsDlg::OnCellRangeSelected(wxGridRangeSelectEvent & WXUNUSED(event))
{
    mGrid->HideCellEditControl(); //so that when you initially enter a value in the cell you don't have to press enter or click in another cell before you clicking a column heading enables the fill buttons
    mIncrementButton->Enable(ManipulateCells(true, false)); //just check
    mClearButton->Enable(ManipulateCells(false, false)); //just check
    FillCol(false, false); //just check
    IncrementAsGroup(false); //just check
}

/// Checks to see if a column can be filled with copies or filled with incremented copies of the top cell, setting the enable states of the corresponding buttons accordingly.
/// Filling with copies is possible if the only selection on the grid is one column, the top cell of the column is not empty, and at least one other cell in the column is empty.  Additional requirements for increment filling are that the top cell contains a non-negative number (leading zeros allowed and maintained), or a single character 'A'..'Z' or 'a'..'z'.
/// Optionally fills with copies or incremented copies, working downwards and jumping over non-empty cells.  Incrementing occurs as a cell is filled.  Alphabetical increments wrap, maintaining the case.
/// @param copy True to fill empty cells with a copy of the top cell, if possible
/// @param inc True to fill empty cells with incremented copies of the top cell, if possible (ignored if copy is true)
void SetTapeIdsDlg::FillCol(const bool copy, const bool inc) {
    bool canCopy = false;
    bool canInc = false;
    //Find out whether we can copy or increment-copy, where to copy from, and whether this cell is numerical
    bool numerical;
    int sourceRow;
    if (mGrid->GetSelectionBlockTopLeft().IsEmpty() //no blocks selected
     && mGrid->GetSelectedCells().IsEmpty() //no individual cells selected
     && 1 == mGrid->GetSelectedCols().GetCount()) { //exactly 1 column selected
        for (sourceRow = 0; sourceRow < mGrid->GetNumberRows(); sourceRow++) {
            if (!mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).IsEmpty()) { //we've found the row to copy from
                break;
            }
        }
        if (sourceRow < mGrid->GetNumberRows()) { //there are some cells below the source row
            for (int row = sourceRow; row < mGrid->GetNumberRows(); row++) {
                if (mGrid->GetCellValue(row, mGrid->GetSelectedCols()[0]).IsEmpty()) { //at least one empty cell in the column
                    canCopy = true;
                    if ((numerical = ManipulateCell(sourceRow, mGrid->GetSelectedCols()[0], true, false)) //cell contains a number
                     ||
                     (1 == mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).Len()
                      && mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).CmpNoCase(wxT("A")) >= 0
                      && mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).CmpNoCase(wxT("Z")) < 0)) { //cell contains a single letter
                        canInc = true;
                    }
                    break; //we can fill at least one cell
                }
            }
        }
    }
    //copy or increment if possible
    if (canCopy && copy) {
        //fill up the empty cells with the initial contents
        for (int row = sourceRow + 1; row < mGrid->GetNumberRows(); row++) {
            if (mGrid->GetCellValue(row, mGrid->GetSelectedCols()[0]).IsEmpty()) { //destination cell is empty
                //copy contents of source cell to dest cell
                mGrid->SetCellValue(row, mGrid->GetSelectedCols()[0], mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]));
                UpdateRow(row);
            }
        }
        canCopy = false;
        canInc = false;
    }
    else if (canInc && inc) {
        //fill up the empty cells with an incremented value
        if (numerical) {
#if USING_ULONGLONG
            wxULongLong_t number;
            mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).ToULongLong(&number);
            wxString format = wxString::Format(wxT("%%0%d"), mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).Len()) + wxLongLongFmtSpec + wxT("u"); //handles leading zeros
#else
            unsigned long number;
            mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).ToULong(&number);
            wxString format = wxString::Format(wxT("%%0%du"), mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0]).Len()); //handles leading zeros
#endif
            for (int row = sourceRow + 1; row < mGrid->GetNumberRows(); row++) {
                if (mGrid->GetCellValue(row, mGrid->GetSelectedCols()[0]).IsEmpty()) { //destination cell is empty
                    mGrid->SetCellValue(row, mGrid->GetSelectedCols()[0], wxString::Format(format, ++number));
                    UpdateRow(row);
                }
            }
        }
        else { //alphabetical
            wxChar value = mGrid->GetCellValue(sourceRow, mGrid->GetSelectedCols()[0])[0];
            for (int row = sourceRow + 1; row < mGrid->GetNumberRows(); row++) {
                if (mGrid->GetCellValue(row, mGrid->GetSelectedCols()[0]).IsEmpty()) { //destination cell is empty
                    if (wxT('Z') == value) {
                        //upper case wrap
                        value = wxT('A');
                    }
                    else if (wxT('z') == value) {
                        //lower case wrap
                        value = wxT('a');
                    }
                    else {
                        value++;
                    }
                    mGrid->SetCellValue(row, mGrid->GetSelectedCols()[0], wxString(value));
                    UpdateRow(row);
                }
            }
        }
        canCopy = false;
        canInc = false;
    }
    mFillCopyButton->Enable(canCopy);
    mFillIncButton->Enable(canInc);
}

/// Checks for exactly one column being selected, and its (numerical) incrementability as a group (highest non-blank cell becomes lowest non-blank cell + 1, etc),
/// and sets button state accordingly.  Optionally does the increment.
/// @param commit True to make changes
void SetTapeIdsDlg::IncrementAsGroup(const bool commit)
{
    //Check whether column can be incremented by group
    int groupSize = 0;
#if USING_ULONGLONG
    wxULongLong_t number = 0; //Initialisation prevents compiler warning
    wxULongLong_t newNumber;
#else
    unsigned long number = 0; //Initialisation prevents compiler warning
    unsigned long newNumber;
#endif
    int sourceRow = 0; //Initialisation prevents compiler warning
    int col; //for convenience
    if (mGrid->GetSelectionBlockTopLeft().IsEmpty() //no blocks selected
     && mGrid->GetSelectedCells().IsEmpty() //no individual cells selected
     && 1 == mGrid->GetSelectedCols().GetCount()) { //exactly 1 column selected
        col = mGrid->GetSelectedCols()[0];
        for (int row = 0; row < mGrid->GetNumberRows(); row++) { //check whole column
            if (!mGrid->GetCellValue(row, col).IsEmpty()) { //not empty, or we ignore it
                if (ManipulateCell(row, col, true, false, &newNumber) && (0 == groupSize || number + 1 == newNumber)) { //can increment this cell
                    sourceRow = row; //so far it's the lowest cell we can increment
                    number = newNumber;
                    groupSize++;
                }
                else {
                    //don't want to group increment a column with non-incrementable or non-sequential cells
                    groupSize = 0; //indicates no go
                    break;
                }
            }
        }
    }
    //Increment as a group
    if (groupSize > 1 && commit) { //can do something useful
        for (int row = 0; row <= sourceRow; row++) {
            if (!mGrid->GetCellValue(row, col).IsEmpty()) { //not empty, or we ignore it
#if USING_ULONGLONG
                mGrid->SetCellValue(row, col, wxString::Format(wxString::Format(wxT("%%0%d"), mGrid->GetCellValue(row,col).Len()) + wxLongLongFmtSpec + wxT("u"), ++number));
#else
                mGrid->SetCellValue(row, col, wxString::Format(wxString::Format(wxT("%%0%du"), mGrid->GetCellValue(row,col).Len()), ++number));
#endif
                UpdateRow(row);
            }
        }
    }
    mGroupIncButton->Enable(groupSize > 1);
}

/// Checks for (numerical) incrementability or presence of content in all selected cells, and optionally increments or clears.
/// @param inc True to increment; false to clear
/// @param commit True to make changes
/// @return True if the operation would/did change anything
bool SetTapeIdsDlg::ManipulateCells(const bool inc, const bool commit)
{
    bool changing = false;
    //blocks
    for (size_t block = 0; block < mGrid->GetSelectionBlockTopLeft().GetCount(); block++) {
        for (int row = mGrid->GetSelectionBlockTopLeft()[block].GetRow(); row <= mGrid->GetSelectionBlockBottomRight()[block].GetRow(); row++) {
            for (int col = mGrid->GetSelectionBlockTopLeft()[block].GetCol(); col <= mGrid->GetSelectionBlockBottomRight()[block].GetCol(); col++) {
                if (col > 1) { //an editable cell
                    changing |= ManipulateCell(row, col, inc, commit);
                }
            }
        }
    }
    //columns
    for (size_t colCounter = 0; colCounter < mGrid->GetSelectedCols().GetCount(); colCounter++) {
        for (int row = 0; row < mGrid->GetNumberRows(); row++) {
            changing |= ManipulateCell(row, mGrid->GetSelectedCols()[colCounter], inc, commit);
        }
    }
    //individual cells
    for (size_t cell = 0; cell < mGrid->GetSelectedCells().GetCount(); cell++) {
        changing |= ManipulateCell(mGrid->GetSelectedCells()[cell].GetRow(), mGrid->GetSelectedCells()[cell].GetCol(), inc, commit);
    }
    return changing;
}

/// Checks for (numerical) incrementability or presence of content, and optionally increments or clears, the given cell.
/// If changing, updates the row the cell is in.
/// @param row The row of the cell to increment or clear.
/// @param col The column of the cell to increment or clear.
/// @param inc True to increment; false to clear
/// @param commit Increment or clear if possible to do so.
/// @param retunNumber Optional cell numerical value to return
/// @return True if cell can be or has been changed.
#if USING_ULONGLONG
bool SetTapeIdsDlg::ManipulateCell(const int row, const int col, const bool inc, const bool commit, wxULongLong_t * returnNumber)
#else
bool SetTapeIdsDlg::ManipulateCell(const int row, const int col, const bool inc, const bool commit, unsigned long * returnNumber)
#endif
{
    bool changeable = false;
#if USING_ULONGLONG
    wxULongLong_t number;
    if (inc && wxNOT_FOUND == mGrid->GetCellValue(row, col).Find(wxT("-")) //not negative number
     && mGrid->GetCellValue(row, col).ToULongLong(&number)) { //expressable as a number
#else
    unsigned long number;
    if (inc && wxNOT_FOUND == mGrid->GetCellValue(row, col).Find(wxT("-")) //not negative number
     && mGrid->GetCellValue(row, col).ToULong(&number)) { //expressable as a number
#endif
        changeable = true;
        if (commit) {
            //set cell to incremented number, including any leading zeros
#if USING_ULONGLONG
            mGrid->SetCellValue(row, col, wxString::Format(wxString::Format(wxT("%%0%d"), mGrid->GetCellValue(row,col).Len()) + wxLongLongFmtSpec + wxT("u"), ++number));
#else
            mGrid->SetCellValue(row, col, wxString::Format(wxString::Format(wxT("%%0%du"), mGrid->GetCellValue(row,col).Len()), ++number));
#endif
            UpdateRow(row);
        }
        if (returnNumber) {
            *returnNumber = number;
        }
    }
    else if (!inc && !mGrid->GetCellValue(row, col).IsEmpty()) {
        changeable = true;
        if (commit) {
            mGrid->SetCellValue(row, col, wxEmptyString);
            UpdateRow(row);
        }
    }
    return changeable;
}

/// Responds to the "Help" button being pressed.
/// Opens a modal help dialogue.
void SetTapeIdsDlg::OnHelp(wxCommandEvent & WXUNUSED(event))
{
    wxDialog helpDlg(this, wxID_ANY, wxT("Set Tape IDs help"), wxDefaultPosition, wxDefaultSize, wxRESIZE_BORDER | wxDEFAULT_DIALOG_STYLE);
    wxBoxSizer * sizer = new wxBoxSizer(wxVERTICAL);
    helpDlg.SetSizer(sizer);
    wxTextCtrl * text = new wxTextCtrl(&helpDlg, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
    wxString message = wxT("The first column in the table shows the names of the sources connected to the the recorders you are using.  Any that have a red background are preventing you from recording because they are enabled to record but have no corresponding tape ID.  Any that have a yellow background are also missing a tape ID but are not preventing you from recording because they are not enabled to record.\n\n"
     "The second column shows the tape IDs, if any, corresponding to the sources.  You do not edit the cells in this column because each is created by concatenating the row of editable cells to its right.  Splitting the tape IDs into parts like this allows you to specify and increment them easily.  If there are any duplicate IDs, a warning is shown to the right of the table, but the situation is permitted.\n\n"
     "You can use as many as, and whichever of, the editable cells you like to create a tape ID, but you must separate the ID into parts which are incremented and parts which are not, and put these into different cells.\n\n"
     "For example, you may have a tape ID #1001A# which will become #1002A#, #1003A#, etc., when the tape is changed.  To do this, you can put #1001# into the top row of the Part 1 column, and #A# into the cell to its right.  You have now separated the incrementing part from the non-incrementing part, and the number #1001# can be incremented automatically without the #A# interfering with it.\n\n"
     "There are two buttons to help you fill in the table quickly.  The first of these, \"Fill column with copies\", copies the contents of the highest non-empty cell to all the empty cells below it.  It is only available if you select a column (by clicking on one of the \"Part\" headings), there is at least one non-empty cell (so there is something to copy from), and there is at least one empty cell below it (so there is something to copy to).  In the example above, if you want all your tape IDs to start with #1001#, you can quickly fill in the rest of the column by clicking on the \"Part 1\" heading and pressing \"Fill column with copies\".  (As this will make the second and subsequent rows identical, you will temporarily get a duplicate tape ID warning.)  If you don't want to fill the whole column, you will need to remove unwanted cell contents manually.  You can do this quickly by selecting them and pressing \"Clear selected\".\n\n"
     "The other button to help you fill in the table quickly is \"Fill column by incrementing\".  Again, this is only available if you select a column where there is an empty cell with empty cells below it, but also the uppermost non-empty cell must be suitable for incrementing.  This will be the case if it contains a set of digits or a single character from #a# to #z# or #A# to #Z#.  Going back to the example (#1001A#), the top cell in the Part 2 column contains #A#, so selecting this column and pressing \"Fill column by incrementing\" will put #B#, #C#, #D# etc. into the cells below.  (If the value reaches #Z# or #z#, it will wrap back round to #A# or #a# respectively.)\n\n"
     "So now you have been able to fill the tape ID table more efficiently than by manual typing.\n\n"
     "When tapes are changed, there are two buttons to enable you to update their IDs quickly.  The first is the \"Increment Selected\" button, which will increment any cells containing digits (not letters) that have been selected.  You can select a column as before, or you can select an area of cells by dragging the mouse across them, or you can select or deselect individual cells by control-clicking them.\n\n"
     "The second button you can use is \"Increment as group\".  This is only available if you select a column containing a continuous sequence of increasing numbers (plus blank cells in any position).  When you press this button, the whole group will increment by the length of the sequence.  So, for instance, if the column contained #5#, #6#, #7# and #8#, pressing the button would change the cells to #9#, #10#, #11# and #12#.\n\n"
     "A useful tip is that if IDs tend to be incremented in known groupings, rather than as a whole or randomly, it may be quicker to use different columns to specify the different groups.  This will allow you to increment some tape IDs very quickly without affecting the others, by clicking on the appropriate column heading and pressing one of the increment buttons.  For example, you could use the Part 1 column to enter the number part of some of the IDs and the Part 2 column to enter the rest, while using Part 3 for all the letters.\n\n"
     "You cannot undo individual changes you make to the tape IDs, but you can press Cancel instead of OK to return to the situation where you started.");
    HelpDlg::StyleAndWrite(text, message);
    sizer->Add(text, 1, wxEXPAND | wxALL, CONTROL_BORDER);
    wxButton * okButton = new wxButton(&helpDlg, wxID_OK);
    sizer->Add(okButton, 0, wxALIGN_CENTRE | wxBOTTOM, CONTROL_BORDER);
    helpDlg.ShowModal();
}

/// @return True if the user updated information while the dialogue was shown.
bool SetTapeIdsDlg::IsUpdated()
{
    return mUpdated;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(JumpToTimecodeDlg, wxDialog)
    EVT_TEXT(wxID_ANY, JumpToTimecodeDlg::OnTextChange)
    EVT_SET_FOCUS(JumpToTimecodeDlg::OnFocus)
    EVT_KILL_FOCUS(JumpToTimecodeDlg::OnFocus)
    EVT_BUTTON(wxID_ANY, JumpToTimecodeDlg::OnButton)
END_EVENT_TABLE()

DEFINE_EVENT_TYPE(EVT_JUMP_TO_TIMECODE)

/// @param tc A timecode for the edit rate.
JumpToTimecodeDlg::JumpToTimecodeDlg(wxWindow * parent) : wxDialog(parent, wxID_ANY, (const wxString &) wxT("Jump to Timecode"))
{
    wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(mainSizer);
    wxBoxSizer * timecodeSizer = new wxBoxSizer(wxHORIZONTAL);
    mainSizer->Add(timecodeSizer, 0, wxALL | wxEXPAND, CONTROL_BORDER);
    timecodeSizer->AddStretchSpacer();
    wxFont * font = wxFont::New(TIME_FONT_SIZE, wxFONTFAMILY_MODERN); //this way works under GTK
    mHours = new MyTextCtrl(this, HRS, wxT("00"));
    mHours->SetFont(*font);
    wxWindowDC DC(mHours);
    wxSize size = DC.GetTextExtent(wxT("00"));
    size.x += 8; //Ugh.  But it doesn't work otherwise...
    mHours->SetClientSize(size);
    mHours->SetMaxLength(2);
    timecodeSizer->Add(mHours, 0, wxFIXED_MINSIZE);
    mHours->SetFocus();
    wxStaticText * hoursSep = new wxStaticText(this, wxID_ANY, wxT(":"));
    hoursSep->SetFont(*font);
    timecodeSizer->Add(hoursSep);
    mMins = new MyTextCtrl(this, MINS, wxT("00"));
    mMins->SetFont(*font);
    mMins->SetClientSize(size);
    mMins->SetMaxLength(2);
    timecodeSizer->Add(mMins, 0, wxFIXED_MINSIZE);
    wxStaticText * minsSep = new wxStaticText(this, wxID_ANY, wxT(":"));
    minsSep->SetFont(*font);
    timecodeSizer->Add(minsSep);
    mSecs = new MyTextCtrl(this, SECS, wxT("00"));
    mSecs->SetFont(*font);
    mSecs->SetClientSize(size);
    mSecs->SetMaxLength(2);
    timecodeSizer->Add(mSecs, 0, wxFIXED_MINSIZE);
    wxStaticText * secsSep = new wxStaticText(this, wxID_ANY, wxT(":"));
    secsSep->SetFont(*font);
    timecodeSizer->Add(secsSep);
    mFrames = new MyTextCtrl(this, FRAMES, wxT("00"));
    mFrames->SetFont(*font);
    mFrames->SetClientSize(size);
    mFrames->SetMaxLength(2);
    timecodeSizer->Add(mFrames, 0, wxFIXED_MINSIZE);
    timecodeSizer->AddStretchSpacer();
    mNotFoundMessage = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE | wxST_NO_AUTORESIZE); //should centre text but doesn't
    mainSizer->Add(mNotFoundMessage, 0, wxALL | wxEXPAND, CONTROL_BORDER);
    wxBoxSizer * buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    mainSizer->Add(buttonSizer, 0, wxALL, CONTROL_BORDER);
    mApplyButton = new wxButton(this, wxID_ANY, wxT("Apply"));
    buttonSizer->Add(mApplyButton, 0, wxALL, CONTROL_BORDER);
    buttonSizer->Add(new wxButton(this, wxID_OK), 0, wxALL, CONTROL_BORDER);
    buttonSizer->Add(new wxButton(this, wxID_CANCEL), 0, wxALL, CONTROL_BORDER);
//  timecodeSizer->Add(jumpButton, 0, wxALL, CONTROL_BORDER);
//  hours->SetClientSize(size);
//  hours->Layout();
//  hours->SetInitialSize(wxSize(hours->GetCharWidth()*2 + 10, hours->GetCharHeight()));
//  SetFont(*font);
//  hours->SetInitialSize(size);
//  hours->SetClientSize(size);
//  hours->SetSize(size);
    delete font;
    Fit();
}

/// Checks a defined timecode value (for edit rate) has been supplied before allowing dialogue to be shown
void JumpToTimecodeDlg::Show(ProdAuto::MxfTimecode tc)
{
    mTimecode = tc;
    if (!mTimecode.undefined) {
        mNotFoundMessage->SetLabel(wxEmptyString);
        wxDialog::Show(); //sanity check - avoids divide by zero if timecode not defined
        FindWindow(HRS)->SetFocus();
    }
}

/// Responds to character entry in text entry field by checking, changing focus or deleting character just entered as appropriate
void JumpToTimecodeDlg::OnTextChange(wxCommandEvent & event)
{
    mNotFoundMessage->SetLabel(wxEmptyString);
    MyTextCtrl * ctrl = (MyTextCtrl *) event.GetEventObject();
    wxString value = ctrl->GetValue();
    bool ok;
    switch (event.GetId()) {
        case HRS:
            ok = CheckValue(value, wxT("23"));
            break;
        case MINS: case SECS:
            ok = CheckValue(value, wxT("59"));
            break;
        default:
            ok = CheckValue(value, wxString::Format(wxT("%02d"), mTimecode.edit_rate.numerator / mTimecode.edit_rate.denominator - 1));
            break;
    }
    if (!ok) { //illegal value
        //remove the last character entered
        ctrl->SetValue(value.Left(ctrl->GetInsertionPoint()));
    }
    else if (2 == value.Len()) { //full field
        //move to the next field
//      ctrl->Navigate();
        if (FRAMES == event.GetId()) {
            mApplyButton->SetFocus();
        }
        else {
            FindWindow(event.GetId() + 1)->SetFocus();
        }
    }
}

/// Returns true if a control has an allowed value
/// @param value value expressed as a string
/// @param limit Max value (expressed as a string) that is allowed for this control
bool JumpToTimecodeDlg::CheckValue(const wxString & value, const wxString & limit)
{
    const wxString digits(wxT("0123456789"));
    int digit;
    if (!value.Len() || ((wxNOT_FOUND != (digit = digits.Find(value[0]))) && digit <= digits.Find(limit[0]))) { //either empty or first char is a digit, within the limit for that digit
        if (value.Len() < 2 || (wxNOT_FOUND != digits.Find(value[1]))) { //either <2 chars or second char is a digit, within the limit for that digit
            long total, limitVal;
            value.ToLong(&total);
            limit.ToLong(&limitVal);
            if (total <= limitVal) {
                return true;
            }
        }
    }
    return false;
}

/// Puts leading zeros into text entry fields that are losing focus
void JumpToTimecodeDlg::OnFocus(wxFocusEvent & event)
{
    if (wxEVT_KILL_FOCUS == event.GetEventType()) { //this is a control losing focus
        switch (event.GetId()) {
            case HRS: case MINS: case SECS: case FRAMES: {
                MyTextCtrl * ctrl = (MyTextCtrl *) event.GetEventObject();
                if (2 != ctrl->GetValue().Len()) { //this check prevents the value being changed unnecessarily, which causes the control focus to be changed which breaks shift-tabbing
                    //put in leading zeros
                    ctrl->SetValue(wxString(wxT("00") + ctrl->GetValue()).Right(2));
                }
                break;
            }
            default:
                break;
        }
    }
    event.Skip();
}

/// Sends an event to the frame if OK or Apply have been pressed.  Indirectly closes the window if OK or Cancel have been pressed.
void JumpToTimecodeDlg::OnButton(wxCommandEvent & event)
{
    if (wxID_CANCEL != event.GetId()) { //must the OK or the apply button
        //tell the frame there's a timecode to jump to
        wxCommandEvent guiEvent(EVT_JUMP_TO_TIMECODE, wxID_ANY);
        GetParent()->AddPendingEvent(guiEvent);
        FindWindow(HRS)->SetFocus(); //ready for next tc
    }
    if (wxID_OK == event.GetId() || wxID_CANCEL == event.GetId()) {
        event.Skip();
    }
}

/// Returns the timecode corresponding to the values in the boxes
const ProdAuto::MxfTimecode JumpToTimecodeDlg::GetTimecode()
{
    long field;
    (dynamic_cast<MyTextCtrl *>(FindWindow(HRS)))->GetValue().ToLong(&field);
    mTimecode.samples = field * 60;
    (dynamic_cast<MyTextCtrl *>(FindWindow(MINS)))->GetValue().ToLong(&field);
    mTimecode.samples = (mTimecode.samples + field) * 60;
    (dynamic_cast<MyTextCtrl *>(FindWindow(SECS)))->GetValue().ToLong(&field);
    mTimecode.samples += field;
    mTimecode.samples *= mTimecode.edit_rate.numerator / mTimecode.edit_rate.denominator;
    (dynamic_cast<MyTextCtrl *>(FindWindow(FRAMES)))->GetValue().ToLong(&field);
    mTimecode.samples += field;
    return mTimecode;
}

/// Displays a message indicating the timecode has not been found.
void JumpToTimecodeDlg::NotFoundMessage()
{
    if (wxEmptyString == mNotFoundMessage->GetLabel()) { //not already showing message, preventing an endless loop
        mNotFoundMessage->SetLabel(wxT("Timecode not found"));
        wxDialog::Show(); //in case OK had been pressed
    }
}

/// Hides the dialogue
void JumpToTimecodeDlg::Hide()
{
    wxDialog::Show(false);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(TestModeDlg, wxDialog)
    EVT_SPINCTRL(MIN_REC, TestModeDlg::OnChangeMinRecTime)
    EVT_SPINCTRL(MAX_REC, TestModeDlg::OnChangeMaxRecTime)
    EVT_SPINCTRL(MIN_GAP, TestModeDlg::OnChangeMinGapTime)
    EVT_SPINCTRL(MAX_GAP, TestModeDlg::OnChangeMaxGapTime)
    EVT_SPINCTRL(ERASE_THRESH, TestModeDlg::OnChangeEraseThreshold)
    EVT_TOGGLEBUTTON(RUN, TestModeDlg::OnRun)
    EVT_RADIOBUTTON(wxID_ANY, TestModeDlg::OnModeChange)
    EVT_TIMER(wxID_ANY, TestModeDlg::OnTimer)
    EVT_CHECKBOX(wxID_ANY, TestModeDlg::OnEraseEnable)
END_EVENT_TABLE()

#define COUNTDOWN_FORMAT wxT("%H:%M:%S")

/// Sets up dialogue.
/// @param parent The parent window.
/// @param recordId ID of a menu event to send to the parent to start recording.
/// @param stopId ID of a menu event to send to the parent to stop recording.
TestModeDlg::TestModeDlg(wxWindow * parent, const int recordId, const int stopId) : wxDialog(parent, wxID_ANY, (const wxString &) wxT("Test Mode")), mRecording(false),
mRecordId(recordId), mStopId(stopId)
{
    wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL); //for border
    SetSizer(mainSizer);
    wxFlexGridSizer * gridSizer = new wxFlexGridSizer(5, 5, CONTROL_BORDER, CONTROL_BORDER);
    mainSizer->Add(gridSizer, 0, wxALL, CONTROL_BORDER);
    //mode row
    gridSizer->Add(new wxRadioButton(this, RANDOM, wxT("Random"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP), 0, wxALL, CONTROL_BORDER);
    gridSizer->Add(new wxRadioButton(this, wxID_ANY, wxT("Continuous")), 0, wxALL, CONTROL_BORDER);
    gridSizer->Add(new wxStaticText(this, wxID_ANY, wxEmptyString)); //dummy
    gridSizer->Add(new wxStaticText(this, wxID_ANY, wxEmptyString)); //dummy
    gridSizer->Add(new wxStaticText(this, wxID_ANY, wxEmptyString)); //dummy
    //record time row
    gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Record time minimum"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT), 0, wxALIGN_CENTRE);
#define DEFAULT_MAX_REC_TIME 1
    mMinRecTime = new wxSpinCtrl(this, MIN_REC, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, DEFAULT_MAX_REC_TIME, 1);
    gridSizer->Add(mMinRecTime, 1, wxALIGN_CENTRE);
    gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("min; maximum")), 0, wxALIGN_CENTRE);
    mMaxRecTime = new wxSpinCtrl(this, MAX_REC, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, mMinRecTime->GetValue(), TEST_MAX_REC, DEFAULT_MAX_REC_TIME);
    gridSizer->Add(mMaxRecTime, 0, wxALIGN_CENTRE);
    gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("min."), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT), 0, wxALIGN_CENTRE);
    //gap time row
    gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Gap time minimum"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT), 0, wxALIGN_CENTRE);
#define DEFAULT_MAX_GAP_TIME 5
    mMinGapTime = new wxSpinCtrl(this, MIN_GAP, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, DEFAULT_MAX_GAP_TIME, 5);
    gridSizer->Add(mMinGapTime, 0, wxALIGN_CENTRE);
    gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("sec; maximum")), 0, wxALIGN_CENTRE);
    mMaxGapTime = new wxSpinCtrl(this, MAX_GAP, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, mMinGapTime->GetValue(), TEST_MAX_GAP, DEFAULT_MAX_GAP_TIME);
    gridSizer->Add(mMaxGapTime, 0, wxALIGN_CENTRE);
    gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("sec."), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT), 0, wxALIGN_CENTRE);
    //erase row
    mEraseEnable = new wxCheckBox(this, wxID_ANY, wxT("Keep disks below"));
    gridSizer->Add(mEraseEnable, 0, wxALIGN_RIGHT);
    mEraseThreshold = new wxSpinCtrl(this, ERASE_THRESH, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 100, 50);
    gridSizer->Add(mEraseThreshold, 0, wxALIGN_CENTRE);
    gridSizer->Add(new wxStaticText(this, wxID_ANY, wxT("% full")), 0, wxALIGN_LEFT | wxALIGN_CENTRE_VERTICAL);
    mEraseMessage = new wxStaticText(this, wxID_ANY, wxEmptyString);
    gridSizer->Add(mEraseMessage, 0, wxALIGN_CENTRE_VERTICAL);
    gridSizer->Add(new wxStaticText(this, wxID_ANY, wxEmptyString)); //dummy
    //button row
    mRunButton = new wxToggleButton(this, RUN, wxT("Run"));
    gridSizer->Add(mRunButton);
    mRunStopMessage = new wxStaticText(this, wxID_ANY, wxEmptyString);
    gridSizer->Add(mRunStopMessage, 0, wxALIGN_CENTRE_VERTICAL);
    mRunStopCountdown = new wxStaticText(this, wxID_ANY, wxEmptyString);
    gridSizer->Add(mRunStopCountdown, 0, wxALIGN_CENTRE_VERTICAL);
    mCancelButton = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
    gridSizer->Add(mCancelButton);

    Fit();
    mCountdownTimer = new wxTimer(this);
    mEraseTimer = new wxTimer(this);
    srand(wxDateTime::Now().GetMillisecond());
}

/// Stops timers when dialogue deleted - otherwise there's a segfault when the timers complete...
TestModeDlg::~TestModeDlg()
{
    mCountdownTimer->Stop();
    mEraseTimer->Stop();
}

/// Stops testing when dialogue closed but leaves any recording happening, amongst other things
int TestModeDlg::ShowModal()
{
    //remove all info about files to erase, as the project name may have been changed since recordings were made and so different directories may be in use
    HashOfDirContents::iterator dirIt = mDirInfo.begin();
    while (dirIt != mDirInfo.end()) {
        std::list<time_t>::iterator mtimeIt = mDirInfo[dirIt->first]->mtimes.begin();
        while (mDirInfo[dirIt->first]->mtimes.end() != mtimeIt) {
            delete mDirInfo[dirIt->first]->files[*mtimeIt++]; //the file arrays
        }
        delete dirIt++->second; //the DirContents structure
    }
    mDirInfo.clear(); //the hash elements pointing to the deleted DirContent structures
    int rc = wxDialog::ShowModal();
    mCountdownTimer->Stop();
    mRunButton->SetValue(false);
    mRecording = false;
    UpdateCountdownDisplay();
    mEraseMessage->SetLabel(wxEmptyString);
    EnableModeButtons();
    return rc;
}

/// Changes between continuous and random modes.
/// Should only be called when test mode not running.
void TestModeDlg::OnModeChange(wxCommandEvent & WXUNUSED(event))
{
    bool random = ((wxRadioButton *) FindWindow(RANDOM))->GetValue();
    wxSizer * gridSizer = GetSizer()->GetItem((size_t) 0)->GetSizer();
    for (int i = 5; i < 15; i++) {
        gridSizer->GetItem(i)->GetWindow()->Enable(random);
    }
}

/// Responds to the minimum record time being changed.
/// Adjusts the minimum value of the maximum record time to be the same as the minimum record time.
void TestModeDlg::OnChangeMinRecTime(wxSpinEvent & WXUNUSED(event))
{
    mMaxRecTime->SetRange(mMinRecTime->GetValue(), mMaxRecTime->GetMax());
}

/// Responds to the maximum record time being changed.
/// Adjusts the maximum value of the minimum record time to be the same as the maximum record time.
void TestModeDlg::OnChangeMaxRecTime(wxSpinEvent & WXUNUSED(event))
{
    mMinRecTime->SetRange(mMinRecTime->GetMin(), mMaxRecTime->GetValue());
}

/// Responds to the minimum gap time being changed.
/// Adjusts the minimum value of the maximum gap time to be the same as the minimum gap time.
void TestModeDlg::OnChangeMinGapTime(wxSpinEvent & WXUNUSED(event))
{
    mMaxGapTime->SetRange(mMinGapTime->GetValue(), mMaxGapTime->GetMax());
}

/// Responds to the maximum gap time being changed.
/// Adjusts the maximum value of the minimum gap time to be the same as the maximum gap time.
void TestModeDlg::OnChangeMaxGapTime(wxSpinEvent & WXUNUSED(event))
{
    mMinGapTime->SetRange(mMinGapTime->GetMin(), mMaxGapTime->GetValue());
}

/// Responds to the erase threshold being changed.
/// Checks if disk is now above erase threshold.
void TestModeDlg::OnChangeEraseThreshold(wxSpinEvent & WXUNUSED(event))
{
        Erase(); //will only do anything if recording, etc
}

/// Responds to the run button being pressed.
void TestModeDlg::OnRun(wxCommandEvent & WXUNUSED(event))
{
    if (mRunButton->GetValue()) {
        //run
        Record();
    }
    else {
        //stop
        Record(false);
        mCountdownTimer->Stop();
    }
    EnableModeButtons(!mRunButton->GetValue());
}

/// Switches on and off erasing of old files
void TestModeDlg::OnEraseEnable(wxCommandEvent & WXUNUSED(event))
{
    if (mEraseEnable->IsChecked()) {
        Erase(); //will only do anything if recording, etc
    }
    else {
        //no need to stop erase timer as it will be ignored when it times out
        mEraseMessage->SetLabel(wxEmptyString);
    }
}

/// Enables or disables the mode buttons.
void TestModeDlg::EnableModeButtons(bool enable)
{
    wxSizer * gridSizer = GetSizer()->GetItem((size_t) 0)->GetSizer();
    gridSizer->GetItem((size_t) 0)->GetWindow()->Enable(enable);
    gridSizer->GetItem(1)->GetWindow()->Enable(enable);
}

/// Responds to the countdown timer by updating the countdown message and toggling the recording state if it reaches zero, and the erase timer by starting another erase cycle.
void TestModeDlg::OnTimer(wxTimerEvent & event)
{
    if (mCountdownTimer == event.GetEventObject()) {
        mCountdown -= wxTimeSpan(0, 0, 1, 0); //one second
        if (mCountdown.IsEqualTo(wxTimeSpan())) { //empty
            Record(!mRecording);
        }
        else {
            mCountdownTimer->Start(1000, wxTIMER_ONE_SHOT); //one second
        }
        UpdateCountdownDisplay();
    }
    else {
        Erase();
    }
}

/// Sends a record or stop command as a menu event to the parent, checks for files to erase, and starts the timer for the next command.
/// @param rec True to record.
void TestModeDlg::Record(bool rec)
{
    if (mRecording != rec) {
        //send command
        mRecording = rec;
        wxCommandEvent menuEvent(wxEVT_COMMAND_MENU_SELECTED, mRecording ? mRecordId : mStopId);
        GetParent()->AddPendingEvent(menuEvent);
        //work out duration of next action and send comment
        if (((wxRadioButton *) FindWindow(RANDOM))->GetValue()) {
            int dur;
            if (mRecording) {
                int range = mMaxRecTime->GetValue() - mMinRecTime->GetValue() + 1;
                while ((dur = rand()/(RAND_MAX/range)) > range) {}; //avoid occasional truncation to range
                dur += mMinRecTime->GetValue();
                mCountdown = wxTimeSpan::Minutes(dur);
            }
            else {
                int range = mMaxGapTime->GetValue() - mMinGapTime->GetValue() + 1;
                while ((dur = rand()/(RAND_MAX/range)) > range) {}; //avoid occasional truncation to range
                dur += mMinGapTime->GetValue();
                mCountdown = wxTimeSpan::Seconds(dur);
            }
            mCountdownTimer->Start(1000, wxTIMER_ONE_SHOT); //one second
        }
        UpdateCountdownDisplay();
    }
}

/// Sets the run/stop message and countdown display according to the current state.
void TestModeDlg::UpdateCountdownDisplay()
{
    if (!mRunButton->GetValue()) {
        mRunStopMessage->SetLabel(wxT("Stopped"));
        mRunStopCountdown->SetLabel(wxEmptyString);
    }
    else if (!((wxRadioButton *) FindWindow(RANDOM))->GetValue()) {
        mRunStopMessage->SetLabel(wxT("Recording continuously"));
        mRunStopCountdown->SetLabel(wxEmptyString);
    }
    else {
        mRunStopMessage->SetLabel(mRecording ? wxT("Stopping in") : wxT("Recording in"));
        mRunStopCountdown->SetLabel(mCountdown.Format(COUNTDOWN_FORMAT));
    }
}

/// Adds paths to be scanned for erasing files to stop the disk(s) filling up.
/// Scans each path and if not known about, inserts it into a hash pointing to a structure which contains a sorted list of file ages, and a hash mapping the ages to lists of filenames.  This only happens if the path is not known to avoid pointless re-scans as the files in known paths will be older than any more found in a re-scan.  As files are deleted in decreasing order of age, their corresponding records will then be removed from the structure, and the structure and hash entry removed when all files have been deleted, prompting a re-scan the next time this method is called with that path.  Starts the erasing procedure if the right criteria are met.
/// @param fullNames The full path and filenames of paths to be scanned; filenames ignored; duplicate paths allowed; can be on more than one disk.
void TestModeDlg::AddPaths(const wxArrayString & fullNames)
{
    if (IsModal()) { //no point in wasting memory otherwise
        wxString dirName;
        for (size_t i = 0; i < fullNames.GetCount(); i++) {
            wxFileName::SplitPath(fullNames[i], &dirName, 0, 0);
            if (!dirName.IsEmpty() && mDirInfo.find(dirName) == mDirInfo.end()) { //avoid pointless scan when we already have records of files at this location
                //scan the directory
                wxLongLong total, free;
                wxDir dir;
                wxString fileName, fullName;
                if (
                 dir.Open(dirName) //we can open the material directory
                 && dir.GetFirst(&fileName, wxEmptyString, wxDIR_FILES) //there is at least one file in it (ignore subdirectories)
                ) {
                    //Make a sorted list of file ages and a hash mapping those file ages to arrays of filenames
                    DirContents* contents = new DirContents; //deleted here or in Erase()
                    time_t mtime;
                    bool failed = false;
                    do {
                        fullName = dirName + wxFileName::GetPathSeparator() + fileName;
                        if (-1 == (mtime = wxFileModificationTime(fullName))) { //couldn't get file modification time
                            failed = true;
                            break; //don't continue or could end up deleting newer files than necessary
                        }
                        if (contents->files.find(mtime) == contents->files.end()) { //not got an array of files of this age
                            contents->mtimes.push_back(mtime);
                            contents->files[mtime] = new wxArrayString; //deleted here or in Erase()
                        }
                        contents->files[mtime]->Add(fileName);
                    } while (dir.GetNext(&fileName));
                    if (failed) {
                        while (contents->files.size()) {
                            delete contents->files.begin()->second; //the string arrays
                            contents->files.erase(contents->files.begin()->first);
                        }
                        delete contents;
                    }
                    else {
                        contents->mtimes.sort();
                        mDirInfo[dirName] = contents;
                    }
                }
            }
        }
    }
    Erase(); //makes sure files get erased during continuous chunking where otherwise Erase() wouldn't be called
}

///Attempts to erase the oldest set of files of the same age that reside in a directory that is on a disk more full than the erase threshold.
///If files are found to erase, start a timer to call the method again after a pause, thus avoiding saturating disks with space checks and delete operations.
///Otherwise, erasing will stop until the method is called again by other means, thus avoiding a continuous cycle of disk space checks when no disks are above the erase threshold.
void TestModeDlg::Erase()
{
    if (mRecording && mEraseEnable->IsChecked() && mDirInfo.size() && !mEraseTimer->IsRunning()) { //don't do an out-of-sequence erase if already erasing
        //find and delete the oldest files overall: this caters for more than one record directory on the same disk, where otherwise files might be deleted unevenly from the directories
        wxString dirName;
        wxLongLong total, free;
        time_t mtime = 0; //dummy initialiser
        HashOfDirContents::iterator dirIt = mDirInfo.begin();
        //find the oldest files that need to be deleted
        do {
            if (
             wxGetDiskSpace(dirIt->first, &total, &free) //we know how full the disk is
             && (int) (free * 100 / total).GetLo() <= 100 - mEraseThreshold->GetValue() //the occupancy is over the erase threshold
             && (dirName.IsEmpty() || *dirIt->second->mtimes.begin() < mtime) //the oldest files are the oldest we've come across
             && (dirName.IsEmpty() || *dirIt->second->mtimes.begin() < mtime) //the oldest files are the oldest we've come across
            ) {
                dirName = dirIt->first;
                mtime = *dirIt->second->mtimes.begin();
            }
        } while (mDirInfo.end() != ++dirIt);
        if (!dirName.IsEmpty()) { //found something to delete
            //erase all files of this age from this dir (because it's likely to correspond to a complete recording and it reduces number of disk space checks)
            mEraseMessage->SetLabel(wxEmptyString == mEraseMessage->GetLabel() ? wxT("Erasing...") : wxEmptyString);
            for (size_t i = 0; i < mDirInfo[dirName]->files[mtime]->GetCount(); i++) {
                wxRemoveFile(dirName + wxFileName::GetPathSeparator() + mDirInfo[dirName]->files[mtime]->Item(i)); //can't do anything if can't erase the file, so ignore return value and the file will be abandoned until all more recent files in this directory are erased
            }
            //remove the array of files and the mtime entry corresponding to it
            delete mDirInfo[dirName]->files[mtime];
            mDirInfo[dirName]->mtimes.erase(mDirInfo[dirName]->mtimes.begin());
            //remove the DirContents structure and hash entry pointing to it if an attempt has been made to remove all the files from this directory
            if (!mDirInfo[dirName]->mtimes.size()) {
                delete mDirInfo[dirName];
                mDirInfo.erase(dirName);
            }
            //set up repeat call if more files known about
            if (mDirInfo.size()) { //more to erase
                //call ourselves again
                mEraseTimer->Start(500, wxTIMER_ONE_SHOT);
            }
            else {
                mEraseMessage->SetLabel(wxEmptyString);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const struct {char colour[8]; char labelColour[8]; char label[8]; ProdAuto::LocatorColour::EnumType code;} Colours[N_CUE_POINT_COLOURS] = {
    { "#FF0000", "#000000", "DEFAULT", ProdAuto::LocatorColour::DEFAULT_COLOUR }, //this must come first
    { "#FFFFFF", "#000000", "White", ProdAuto::LocatorColour::WHITE },
    { "#FF0000", "#000000", "Red", ProdAuto::LocatorColour::RED },
    { "#FFFF00", "#000000", "Yellow", ProdAuto::LocatorColour::YELLOW },
    { "#00FF00", "#000000", "Green", ProdAuto::LocatorColour::GREEN },
    { "#00FFFF", "#000000", "Cyan", ProdAuto::LocatorColour::CYAN },
    { "#0000FF", "#FFFFFF", "Blue", ProdAuto::LocatorColour::BLUE },
    { "#FF00FF", "#000000", "Magenta", ProdAuto::LocatorColour::MAGENTA },
    { "#000000", "#FFFFFF", "Black", ProdAuto::LocatorColour::BLACK }
};

DEFINE_EVENT_TYPE (EVT_SET_GRID_ROW);

BEGIN_EVENT_TABLE(CuePointsDlg, wxDialog)
    EVT_GRID_EDITOR_SHOWN(CuePointsDlg::OnEditorShown)
    EVT_GRID_EDITOR_HIDDEN(CuePointsDlg::OnEditorHidden)
    EVT_MENU(wxID_ANY, CuePointsDlg::OnMenu)
    EVT_BUTTON(wxID_OK, CuePointsDlg::OnOK)
    EVT_RADIOBUTTON(wxID_ANY, CuePointsDlg::OnRadioButton)
    EVT_GRID_CELL_LEFT_CLICK(CuePointsDlg::OnCellLeftClick)
    EVT_GRID_LABEL_LEFT_CLICK(CuePointsDlg::OnLabelLeftClick)
    EVT_COMMAND(wxID_ANY, EVT_SET_GRID_ROW, CuePointsDlg::OnSetGridRow)
//  EVT_GRID_CELL_RIGHT_CLICK(CuePointsDlg::OnCellRightClick) //don't use right click because it ignores the mouse pointer position
END_EVENT_TABLE()

/// Sets up dialogue.
/// TODO: prevent selection of ranges - have tried selecting a row (or generating an event to do so) in an EVT_GRID_RANGE_SELECT handler but it gets upset.  Really wxGrid should have a style to do this.
/// @param parent The parent window.
/// @param savedState Persistent state
CuePointsDlg::CuePointsDlg(wxWindow * parent, SavedState * savedState) : wxDialog(parent, wxID_ANY, wxEmptyString,  wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), mSavedState(savedState), mCurrentRow(0)
{
    //accelerator table
    wxAcceleratorEntry entries[11]; //NB number used below and later IDs used for colour menu
    for (int entry = 0; entry < 10; entry++) {
        entries[entry].Set(wxACCEL_NORMAL, (int) '0' + entry, wxID_HIGHEST + N_CUE_POINT_COLOURS + 2 + entry); //ID corresponds to key so we can find out which key is pressed
    }
    entries[10].Set(wxACCEL_NORMAL, WXK_F2, wxID_HIGHEST + N_CUE_POINT_COLOURS + 1);
    wxAcceleratorTable accel(11, entries); //NB number used above
    SetAcceleratorTable(accel);

    //controls
    wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(mainSizer);

    mTimecodeDisplay = new wxStaticText(this, wxID_ANY, wxEmptyString);
    wxFont * font = wxFont::New(TIME_FONT_SIZE, wxFONTFAMILY_MODERN); //this way works under GTK
    mTimecodeDisplay->SetFont(*font);
    mainSizer->Add(mTimecodeDisplay, 0, wxALL | wxALIGN_CENTRE, CONTROL_BORDER);

    mSingleTypeRadioButton = new wxRadioButton(this, wxID_ANY, wxT("Use a single cue point type"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    mainSizer->Add(mSingleTypeRadioButton, 0, wxALL, CONTROL_BORDER);
    mMultipleTypeRadioButton = new wxRadioButton(this, wxID_ANY, wxT("Use multiple cue point types"));
    mainSizer->Add(mMultipleTypeRadioButton, 0, wxALL, CONTROL_BORDER);

    mGrid = new MyGrid(this, wxID_ANY);
    mainSizer->Add(mGrid, 0 , wxEXPAND | wxALL, CONTROL_BORDER);

    mMessage = new wxStaticText(this, wxID_ANY, wxT("To add a cue point:\n- Press a number key,\n- click on a number, or\n- press ENTER or the jog/shuttle cue point button\nto select the highlighted row.\nThe highlighted row can be changed with the jog wheel.\nClick the highlighted description cell to edit it\nor a colour cell to change the colour."), wxDefaultPosition, wxDefaultSize);
    mTextColour = mMessage->GetForegroundColour();
    mBackgroundColour = mMessage->GetBackgroundColour();
    mainSizer->Add(mMessage, 0 , wxEXPAND | wxALL, CONTROL_BORDER);

    wxBoxSizer * buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    mainSizer->Add(buttonSizer);

    mOkButton = new wxButton(this, wxID_OK, wxT("OK"));
    buttonSizer->Add(mOkButton, 1, wxEXPAND | wxALL, CONTROL_BORDER);

    buttonSizer->AddStretchSpacer();

    wxButton * cancelButton = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
    buttonSizer->Add(cancelButton, 1, wxEXPAND | wxALL, CONTROL_BORDER);

    //set up the grid
    mGrid->CreateGrid(11, 2);
    for (int row = 0; row < 10; row++) {
        mGrid->SetRowLabelValue(row, wxString::Format(wxT("%d"), (row + 1) % 10));
    }
    mGrid->SetRowLabelValue(10, wxT("None"));
    mGrid->SetReadOnly(10, 0);
    mGrid->SetCellBackgroundColour(10, 0, wxSystemSettings::GetColour(wxSYS_COLOUR_BACKGROUND));
    mGrid->SetReadOnly(10, 1);
    mGrid->SetCellBackgroundColour(10, 1, wxSystemSettings::GetColour(wxSYS_COLOUR_BACKGROUND));
    mGrid->SetColLabelValue(0, wxT("Description"));
    mGrid->SetColLabelValue(1, wxT("Colour"));
    wxGridCellAttr * readOnly = new wxGridCellAttr;
    readOnly->SetReadOnly();
    mGrid->SetColAttr(1, readOnly); //prevent editing of this column, which is possible if you get to it by tabbing
    Load();
}

/// Loads the grid contents and mode from the saved state.
void CuePointsDlg::Load()
{
    //load default values as may not be overwriting everything
    mGrid->ClearGrid();
    for (int i = 0; i < 10; i++) {
        SetColour(i, 0); //default
    }
    //load saved values
    wxXmlNode * cuePointNode = mSavedState->GetTopLevelNode(wxT("CuePoints"), true)->GetChildren(); //create if not existing
    while (cuePointNode) {
        wxString shortcut, colour;
        unsigned long shortcutVal, colourVal;
        if (wxT("CuePoint") == cuePointNode->GetName() //the right sort of node
         && cuePointNode->GetPropVal(wxT("Shortcut"), &shortcut) //has a required attribute...
         && shortcut.ToULong(&shortcutVal) //... which is a number...
         && shortcutVal < 10 //...in the right range
         && cuePointNode->GetPropVal(wxT("Colour"), &colour) //has a required attribute...
         && colour.ToULong(&colourVal) //... which is a number...
         && colourVal < N_CUE_POINT_COLOURS) { //...in the right range
            shortcutVal = (shortcutVal == 0) ? 9 : shortcutVal - 1; //row number
            mGrid->SetCellValue(shortcutVal, 0, cuePointNode->GetNodeContent().Trim(false).Trim(true));
            SetColour(shortcutVal, colourVal);
        }
        cuePointNode = cuePointNode->GetNext();
    }
    mGrid->AutoSizeColumns();
    Fit();
    mSavedState->GetBoolValue(wxT("CuePoints"), false, wxT("SingleType")) ? mSingleTypeRadioButton->SetValue(true) : mMultipleTypeRadioButton->SetValue(true);
}

/// Saves the grid contents and mode in the XML document.
void CuePointsDlg::Save()
{
    //remove old state
    wxXmlNode * cuePointsNode = mSavedState->GetTopLevelNode(wxT("CuePoints"), true, true); //delete children
    //store new state
    for (int row = 0; row < 10; row++) {
        wxString description = mGrid->GetCellValue(row, 0).Trim(false).Trim(true);
        if (!description.IsEmpty() || mDescrColours[row]) { //something to store
            //create a new element node "CuePoint" containing a new text node with the description
            new wxXmlNode(
                new wxXmlNode(
                    cuePointsNode,
                    wxXML_ELEMENT_NODE,
                    wxT("CuePoint"),
                    wxEmptyString,
                    new wxXmlProperty(
                        wxT("Shortcut"),
                        wxString::Format(wxT("%d"), (row + 1) % 10),
                        new wxXmlProperty(wxT("Colour"), wxString::Format(wxT("%d"), mDescrColours[row]))
                    )
                ),
                wxXML_TEXT_NODE,
                wxEmptyString,
                description
            );
        }
    }
    mSavedState->SetBoolValue(wxT("CuePoints"), mSingleTypeRadioButton->GetValue(), wxT("SingleType")); //also saves the document
}

/// If dialogue not already visible, shows the dialogue and returns when the dialogue is dismissed, saving the grid and mode if OK is pressed.
/// If dialogue already visible, this will be a Mark Cue keystroke from an external device, so act as if the default event had been selected
/// @param timecode If present, displays this at the top of the window and goes into add cue mode - shortcut keys enabled.
int CuePointsDlg::ShowModal(const wxString timecode)
{
    int rc = wxID_CANCEL; //to stop compiler warning
    if (IsModal()) {
        Save();
        EndModal(wxID_OK);
    }
    else {
        if (!timecode.IsEmpty() && mSingleTypeRadioButton->GetValue()) { //entering a cue point and there is only one type
            //No need to show the dialogue
            rc = wxID_OK;
        }
        else {
            Raise(); //Attempt to raise window to the top, which might be useful when opened by the Shuttle Pro, but window manager policy might prevent it happening
            mTimecodeDisplay->SetLabel(timecode);
            mTimecodeDisplay->Show(!timecode.IsEmpty());
            mMessage->Show(!timecode.IsEmpty());
            Fit();

            if (timecode.IsEmpty()) {
                SetTitle(wxT("Edit Cue Point Descriptions"));
                mGrid->SetFocus(); //typing will immediately start editing
                mGrid->ClearSelection(); //in case there's one hanging around from entering cue points, which looks silly
            }
            else {
                SetTitle(wxT("Choose/edit Cue Point"));
                mOkButton->SetFocus(); //stops shortcut keys disappearing into the grid
                mMessage->SetForegroundColour(mTextColour); //as it's shown we need to see it...
                mGrid->SelectRow(mCurrentRow); //always highlight one row
            }
            if (wxID_OK == (rc = wxDialog::ShowModal())) {
                Save();
            }
            else {
                //reload old state
                Load();
            }
        }
    }
    return rc;
}

/// Disables shortcut keys so that they can be used for text entry
void CuePointsDlg::OnEditorShown(wxGridEvent & WXUNUSED(event))
{
    mMessage->SetForegroundColour(mBackgroundColour); //will also disable shortcuts
}

/// Adjusts the grid and window sizes to reflect any editing changes.
void CuePointsDlg::OnEditorHidden(wxGridEvent & WXUNUSED(event))
{
    mGrid->AutoSizeColumns();
//  mGrid->ForceRefresh(); //sometimes makes a mess if you don't
    Fit();
    if (mTimecodeDisplay->IsShown()) {
        mGrid->SelectRow(mCurrentRow);
        mOkButton->SetFocus(); //stops shortcut keys disappearing into the grid
        mMessage->SetForegroundColour(mTextColour); //as it's shown we need to see it...
        //Prevent it jumping down a cell because it's likely you want to submit the just-edited value as the cue point
        wxCommandEvent event(EVT_SET_GRID_ROW, mCurrentRow);
        AddPendingEvent(event);
    }
    else {
        mGrid->SetFocus(); //typing will immediately start editing
        mCurrentRow = mGrid->GetCursorRow(); //this is needed if you press ENTER, and then return later to press ENTER to create a cue point, as the cursor moves down
    }
}

/// Sets the grid cursor to the row specified by the event ID - this has to be done this way because it doesn't like you doing it in a grid event handler
void CuePointsDlg::OnSetGridRow(wxCommandEvent & event)
{
    mGrid->SetGridCursor(event.GetId(), 0); //or it doesn't highlight
    mGrid->SelectRow(mCurrentRow); //or the selection goes away
}

/// Depending on the ID, sets a new locator colour (i.e. called from the pop-up colour menu), or, if enabled to do so, closes the dialogue (saving the grid state), setting the selected locator accordingly (i.e. called with a shortcut key).
/// @param event Contains the menu ID.
void CuePointsDlg::OnMenu(wxCommandEvent & event)
{
    if (event.GetId() < wxID_HIGHEST + 1 + N_CUE_POINT_COLOURS) { //a colour menu selection
        SetColour(mCurrentRow, event.GetId() - wxID_HIGHEST - 1);
        //Move the grid cursor away from the right hand column as it can't be "edited" as such
        wxCommandEvent event(EVT_SET_GRID_ROW, mCurrentRow);
        AddPendingEvent(event);
    }
    else if (mTextColour == mMessage->GetForegroundColour() && mTimecodeDisplay->IsShown()) { //shortcuts enabled
        if (wxID_HIGHEST + N_CUE_POINT_COLOURS + 1 != event.GetId()) { //number key pressed
            //change cue point to correspond to number key
            mCurrentRow = event.GetId() == wxID_HIGHEST + N_CUE_POINT_COLOURS + 2 ? 9 : event.GetId() - N_CUE_POINT_COLOURS - wxID_HIGHEST - 3;
        }
        Save();
        EndModal(wxID_OK);
    }
    else { //editing
        event.Skip();
    }
}

/// If dialogue is accepting cue point selection shortcuts, acts as if the given cue point number had been entered
/// @param shortcut The cue point number (0-9) or -1 to cancel the dialogue
void CuePointsDlg::Shortcut(const int shortcut)
{
    if (mTextColour == mMessage->GetForegroundColour() && mTimecodeDisplay->IsShown() && !mSingleTypeRadioButton->GetValue()) { //shortcuts enabled and this action isn't pointless
        if (-1 == shortcut) { //cancel
            Load(); //reload old state (to make it consistent with pressing ESC in the dialogue)
            EndModal(wxID_CANCEL);
        }
        else {
            mCurrentRow = shortcut ? shortcut - 1 : 9;
            Save();
            EndModal(wxID_OK);
        }
    }
}

/// If shortcuts are enabled, scrolls the selected row up or down, without wrapping.
/// @param down True to scroll downwards.
void CuePointsDlg::Scroll(const bool down)
{
    if (mTextColour == mMessage->GetForegroundColour() && mTimecodeDisplay->IsShown()) { //shortcuts enabled
        if (down) {
            if (mCurrentRow < 10) {
                mCurrentRow++;
                mGrid->SelectRow(mCurrentRow);
            }
        }
        else {
            if (mCurrentRow > 0) {
                mCurrentRow--;
                mGrid->SelectRow(mCurrentRow);
            }
        }
    }
}

/// Saves the grid before ending modal.
void CuePointsDlg::OnOK(wxCommandEvent & event)
{
    Save();
    event.Skip();
}

/// Shows/hides the instruction message if the mode is changed during entry of cue points
void CuePointsDlg::OnRadioButton(wxCommandEvent & WXUNUSED(event))
{
    if (mTimecodeDisplay->IsShown()) mMessage->Show(!mSingleTypeRadioButton->GetValue());
}


/// Produces a pop-up colour menu if the correct column has been clicked.
/// @param event Indicates the column of the grid which was clicked.
void CuePointsDlg::OnCellLeftClick(wxGridEvent & event)
{
    mCurrentRow = event.GetRow(); //Note the row for ENTER being pressed in add cue mode, or if a colour is going to be set
    if (mTimecodeDisplay->IsShown()) { //Entering a cue point
        wxCommandEvent rowEvent(EVT_SET_GRID_ROW, mCurrentRow); //undo the "jump to the edited cell" from OnEditorHidden()
        AddPendingEvent(rowEvent);
        mOkButton->SetFocus(); //makes sure ENTER can still be pressed after clicking on a different event
    }
    if (1 == event.GetCol() && event.GetRow() < 10) { //clicked in the colour column
        wxMenu colourMenu(wxT("Choose colour"));
        for (unsigned int i = 0; i < N_CUE_POINT_COLOURS; i++) {
            colourMenu.Append(wxID_HIGHEST + 1 + i, wxString(Colours[i].label, *wxConvCurrent));
        }
        PopupMenu(&colourMenu);
    }
    event.Skip();
}

/// Sets the selected event and closes the dialogue if in cue point setting mode
void CuePointsDlg::OnLabelLeftClick(wxGridEvent & event)
{
    if (mTimecodeDisplay->IsShown() && mTextColour == mMessage->GetForegroundColour() && !mSingleTypeRadioButton->GetValue() && event.GetRow() > -1) { //make sure it isn't a click in a column heading
        mCurrentRow = event.GetRow();
        Save();
        EndModal(wxID_OK);
    }
}

/// Sets text, text colour and background colour to correspond to an event colour index
/// @param row The row to set
/// @param colour The colour index to be used
void CuePointsDlg::SetColour(const int row, const int colour)
{
    mGrid->SetCellValue(row, 1, wxString(Colours[colour].label, *wxConvCurrent));
    mGrid->SetCellBackgroundColour(row, 1, wxString(Colours[colour].colour, *wxConvCurrent));
    mGrid->SetCellTextColour(row, 1, wxString(Colours[colour].labelColour, *wxConvCurrent));
    mDescrColours[row] = colour;
}

/// returns true if the selected cue point is not "none".
bool CuePointsDlg::ValidCuePointSelected()
{
    return mCurrentRow < 10;
}

/// returns true if all cue points are the same, end entered with a single keystroke
bool CuePointsDlg::DefaultCueMode()
{
    return mSavedState->GetBoolValue(wxT("CuePoints"), false, wxT("SingleType"));
}

/// Returns the currently selected locator's description
const wxString CuePointsDlg::GetDescription()
{
    return mSingleTypeRadioButton->GetValue() ? wxT("") : mGrid->GetCellValue(mCurrentRow, 0).Trim(false).Trim(true);
}

/// Returns colour index of the currently selected locator
size_t CuePointsDlg::GetColourIndex()
{
    return mSingleTypeRadioButton->GetValue() ? 0 : mDescrColours[mCurrentRow];
}

/// Returns the locator colour code corresponding to an index
ProdAuto::LocatorColour::EnumType CuePointsDlg::GetColourCode(const size_t index)
{
    return Colours[index].code;
}

/// Returns the displayed colour corresponding to an index
const wxColour CuePointsDlg::GetColour(const size_t index)
{
    return wxColour(wxString(Colours[index].colour, *wxConvCurrent));
}

/// Returns the label colour corresponding to an index.
const wxColour CuePointsDlg::GetLabelColour(const size_t index)
{
    return wxColour(wxString(Colours[index].labelColour, *wxConvCurrent));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(ChunkingDlg, wxDialog)
    EVT_SPINCTRL(wxID_ANY, ChunkingDlg::OnChangeChunkSize)
    EVT_CHOICE(wxID_ANY, ChunkingDlg::OnChangeChunkAlignment)
    EVT_CHECKBOX(wxID_ANY, ChunkingDlg::OnEnable)
    EVT_TIMER(wxID_ANY, ChunkingDlg::OnTimer)
END_EVENT_TABLE()

#define MAX_CHUNK_SIZE 120 //minutes
#define N_ALIGNMENTS 10

static const int Alignments[N_ALIGNMENTS] = {
    0,
    1,
    2,
    5,
    10,
    15,
    20,
    30,
    40,
    60,
};

/// Chunking occurs in two ways: manually, when the "chunk now" button in the main frame is pressed, or automatically, when enabled in this dialogue.
/// The "chunk now" button should only be enabled while recording.  When pressed, it calls IngexguiFrame::OnChunk(), which calls ChunkingDlg::RunFrom() with no arguments.  This stops any pending automatic chunking trigger, resets the button label, and disables the button to prevent another chunk being requested while the previous chunking cycle is still in progress.  A manual chunking cycle is then initiated by calling RecorderGroupCtrl::Stop(true, ...) with the current timecode and recording details.  See later in this description for what happens when this method is called.  Realign() is also called, which results in the next chunk length being calculated to conform to the alignment (if any) chosen in the chunking dialogue.
/// Automatic chunking is set up at the start of recording, when ChunkingDlg::RunFrom() is called, with the start timecode, and a chunking postroll value determined by the RecorderGroup object (about half a second if all recorders can manage this).  If automatic chunking is enabled in the dialogue, the stop timecode for the current chunk is worked out.
/// This is either the start timecode plus the chunk length, or, if alignment is enabled in the dialogue and the function call, the next alignment point (or the one after that, if it's less than half a minute away).
/// If an alignment was calculated, the "chunk now" button label is set to the time difference (otherwise it's already showing the chunk length, which is the correct value), and a repeating countdown tick timer of a second's duration is started, which decrements the countdown value on the button until it reaches zero.  (The button countdown mechanism is not used to calculate the stop timecode or to trigger the stop command, because it has neither the resolution nor the accuracy.)
/// The postroll value is subtracted from the stop timecode to give the trigger timecode, which allows recorders to be contacted sufficiently in advance of the stop timecode to avoid recording overruns.  It also ensures that the command will be sent in time despite it being generated asynchronously from a refresh timer.
/// Timepos::SetTrigger() is called with the trigger timecode, which is wrapped at midnight, but will always be assumed to be in the future.  The call includes an event handler pointer to send an event to (the main frame).
/// At the first Timepos::OnRefreshTimer() call after the trigger point, the event is generated and is picked up by IngexguiFrame::OnTimeposEvent(), which calls RecorderGroupCtrl::Stop(true, ...) with the trigger timecode, the current contents of the description field, and cue point data.  This sets variable RecorderGroupCtrl::mMode from (initially) RecorderGroupCtrl::RECORDING to CHUNK_STOP_WAIT and initiates a chunking cycle by sending an event to the frame for each recorder asking if tracks are enabled to record, just as if the stop button had been pressed.  This results in stop commands being sent to the appropriate recorders via Enables(), with the postroll set to the chunking postroll value, resulting in stopping happening exactly at the chunk end.
/// The first recorder (or non-router recorder if they are present) that reports that it has successfully stopped causes the mode to be updated to CHUNK_WAIT, and generates a RecorderGroupCtrl::CHUNK_END event with the timecode of the next chunk start, which is the timecode returned by the recorder that has just stopped (as this is the first frame not recorded).  This event is picked up by the frame, which calls EventList::AddEvent() to append the chunk boundary entry to the recording list.  The CHUNK_WAIT mode prevents subsequent recorders generating more events, and also causes RecorderGroupCtrl::TRACK_STATUS events to have a flag set which prevents the recorder source tree briefly reporting an error due to a mismatch between the recorder mode (stopped) and the main frame mode (recording).
/// The timecode returned by the stopped recorder has a safety margin added to it (half the maximum preroll allowed, limited to half a second), to make sure that recorders can never be told to start in the future, and is sent to the frame in a SET_TRIGGER event.  This causes Timepos::SetTrigger to be called again, but this time the trigger mode is set to allow timecodes in the past (which is fine so long as it is within the recorders' preroll abilities), and these will cause an immediate trigger.  The event is targeted at the RecorderGroup.
/// Timepos waits for the trigger point (unless it was in the past), ensuring that recorders are not asked to start recording in the future (which will fail as the requested frame will not be in the capture buffer).  The event it generates is picked up by RecorderGroupCtrl::OnTimeposEvent() which checks that the mode is still RecorderGroupCtrl::CHUNK_WAIT (preventing race states due to user intervention while an event is in the queue).  If so, it sets the mode to CHUNK_RECORD_WAIT and starts recording again by calling Record() as if the user had pressed the record button, but at the chunk start timecode (stored in RecorderGroup::mTimecode since a recorder stop).  When the lists of record enables are requested from the frame, the recording flags are ignored (because these flags are still set at this point).  Also, each controller's record command will have a preroll of zero (which is safe to do as we know the start time is in the past), rather than the normal value.
/// The first successful record command changes the mode from CHUNK_RECORD_WAIT to CHUNK_RECORDING and a RecorderGroupCtrl::CHUNK_START event is issued, with the recorder start timecode.  This causes the frame to call ChunkingDlg::RunFrom() with the timecode from the event and the align argument false, to start counting down exactly one chunk length to the next chunk point if automatic chunking is enabled, regardless of whether the chunk had been intiated manually or automatically.  The chunking button is also enabled in case it had been pressed by the user, which would have been disabled it.  The cycle thus begins again.
/// The RecorderGroup mode is set back to STOPPED (via STOP_WAIT) when RecorderGroupCtrl::Stop(false, ...) is called as a result of the user pressing the stop button.  The STOP event sent to the frame when the first recorder successfully stops causes the frame status to be changed to STOPPED, which calls ChunkingDlg::RunFrom() with no arguments, to stop any pending automatic chunking trigger and reset the countdown display on the "chunk now" button.

/// Sets up dialogue.
/// @param parent Parent window.
/// @param savedState The XML document for retrieving and saving settings.
ChunkingDlg::ChunkingDlg(wxWindow * parent, Timepos * timepos, SavedState * savedState) : wxDialog(parent, wxID_ANY, wxT("Chunking")), mTimepos(timepos), mSavedState(savedState), mCanChunk(false), mPostroll(InvalidMxfDuration), mRealign(false)
{
    const wxChar* alignmentLabels[N_ALIGNMENTS] = {
        wxT("None"),
        wxT("minute"),
        wxT("even minutes"),
        wxT("5 minutes"),
        wxT("10 minutes"),
        wxT("15 minutes"),
        wxT("20 minutes"),
        wxT("30 minutes"),
        wxT("40 minutes"),
        wxT("60 minutes"),
    };
    //controls
    wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(mainSizer);
    wxStaticBoxSizer * sizeBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Chunk Size"));
    mainSizer->Add(sizeBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
    mChunkSizeCtrl = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, MAX_CHUNK_SIZE, 10);
    sizeBox->Add(mChunkSizeCtrl, 0, wxALIGN_CENTRE);
    sizeBox->Add(new wxStaticText(this, wxID_ANY, wxT(" min")), 0, wxALIGN_CENTRE);
    wxStaticBoxSizer * alignBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Chunk Alignment"));
    mainSizer->Add(alignBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
    mChunkAlignCtrl = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxArrayString(N_ALIGNMENTS, alignmentLabels));
    alignBox->Add(mChunkAlignCtrl, 0, wxALIGN_CENTRE);
    mEnableCheckBox = new wxCheckBox(this, wxID_ANY, wxT("Enable"));
    mainSizer->Add(mEnableCheckBox, 0, wxALIGN_CENTRE);
    mainSizer->Add(new wxButton(this, wxID_OK, wxT("Close")), 1, wxEXPAND | wxALL, CONTROL_BORDER);
    Fit();
    mCountdownTimer = new wxTimer(this);

    //saved state
    unsigned long value = mSavedState->GetUnsignedLongValue(wxT("Chunking"), (unsigned long) 10);
    if (value < MAX_CHUNK_SIZE) mChunkSizeCtrl->SetValue((int) value);
    mEnableCheckBox->SetValue(mSavedState->GetBoolValue(wxT("Chunking"), false, wxT("Enabled")));
    value = mSavedState->GetUnsignedLongValue(wxT("Chunking"), 0, wxT("Alignment"));
    if (value < N_ALIGNMENTS) mChunkAlignCtrl->SetSelection((int) value);
    SetNextHandler(parent);
    Reset();
}

/// Overloads Show Modal to update the XML document with chunking settings.
int ChunkingDlg::ShowModal()
{
    wxDialog::ShowModal();
    //create new node - whatever ShowModal returned as changes apply immediately with this dialogue
    wxXmlNode * node = mSavedState->GetTopLevelNode(wxT("Chunking"), true, true); //replace existing node
    //add properties for enabled and chunk alignment
    node->AddProperty(new wxXmlProperty(wxT("Alignment"), wxString::Format(wxT("%d"), mChunkAlignCtrl->GetSelection())));
    mSavedState->SetBoolValue(wxT("Chunking"), mEnableCheckBox->IsChecked(), wxT("Enabled"));
    //add text node containing chunk size
    new wxXmlNode(node, wxXML_TEXT_NODE, wxEmptyString, wxString::Format(wxT("%d"), mChunkSizeCtrl->GetValue()));
    mSavedState->Save();
    return wxID_OK;
}

/// Responds to chunking enable state being toggled by starting/stopping chunking if recording, and updating the button label.
void ChunkingDlg::OnEnable(wxCommandEvent & WXUNUSED(event))
{
    if (mCanChunk) {
        if (mEnableCheckBox->IsChecked()) {
            //start chunk countdown from now
            ProdAuto::MxfTimecode currentTimecode;
            mTimepos->GetTimecode(&currentTimecode);
            mRealign = true;
            RunFrom(currentTimecode);
        }
        else {
            //stop chunk countdown
            RunFrom();
            mCanChunk = true; //calling RunFrom() will have cleared mCanChunk
        }
    }
}

/// Responds to chunking size being changed by resetting the countdown counter and updating the label on the chunking button if not currently counting down.
void ChunkingDlg::OnChangeChunkSize(wxSpinEvent & WXUNUSED(event))
{
    mChunkLength = (unsigned long) mChunkSizeCtrl->GetValue() * 60; //in seconds
    mRealign = true;
    if (!mCountdownTimer->IsRunning()) {
        Reset();
    }
}

/// Responds to chunking size being changed by resetting the countdown counter and updating the label on the chunking button if not currently counting down.
void ChunkingDlg::OnChangeChunkAlignment(wxCommandEvent & WXUNUSED(event))
{
    mChunkAlignment = Alignments[mChunkAlignCtrl->GetSelection()] * 60; //in seconds
    mRealign = true;
    if (!mCountdownTimer->IsRunning()) {
        Reset();
    }
}

/// Re-aligns the next chunk (if aligning is enabled) even if RunFrom() is called with its align argument false
void ChunkingDlg::Realign()
{
    mRealign = true;
}

/// Starts or stops the countdown.
/// @param startTimecode Timecode from which to calculate when to trigger a chunk.  Stops the countdown if undefined flag set.
/// @param postroll Subtracted from the time to trigger a chunk.  If undefined flag set, previous stored value is used.
/// @param align True to start next chunk at the next alignment point (or the one after, if the next one is too soon), if aligning is enabled.
void ChunkingDlg::RunFrom(const ProdAuto::MxfTimecode & startTimecode, const ProdAuto::MxfDuration & postroll, const bool align)
{
    Reset();
    if (!postroll.undefined) {
        mPostroll = postroll;
    }
    mCanChunk = !startTimecode.undefined && !mPostroll.undefined && startTimecode.edit_rate.denominator; //latter a sanity check
    if (mCanChunk) {
        if (mEnableCheckBox->IsChecked()) {
            //calculate when to chunk to frame accuracy
            ProdAuto::MxfTimecode triggerTimecode = startTimecode;
            if ((align || mRealign) && mChunkAlignment) {
                triggerTimecode.samples /= ((int64_t) mChunkAlignment) * triggerTimecode.edit_rate.numerator / triggerTimecode.edit_rate.denominator; //number of alignment points passed since midnight
                ++triggerTimecode.samples; //not samples yet but alignment points since midnight
                triggerTimecode.samples = ((int64_t) triggerTimecode.samples) * mChunkAlignment * triggerTimecode.edit_rate.numerator / triggerTimecode.edit_rate.denominator; //the next alignment point
                if (triggerTimecode.samples - startTimecode.samples < ((int64_t) triggerTimecode.edit_rate.numerator) * 30 / triggerTimecode.edit_rate.denominator) { //less than half a minute to the next alignment point, so producing a very small chunk
                    //don't chunk until the alignment point after this
                    triggerTimecode.samples += ((int64_t) mChunkAlignment) * triggerTimecode.edit_rate.numerator / triggerTimecode.edit_rate.denominator;
                }
                //we now know the time to the next chunk
                mCountdown = ((int64_t) (triggerTimecode.samples - startTimecode.samples)) / (triggerTimecode.edit_rate.numerator / triggerTimecode.edit_rate.denominator);
            }
            else {
                triggerTimecode.samples += ((int64_t) mChunkLength) * triggerTimecode.edit_rate.numerator / triggerTimecode.edit_rate.denominator;
            }
            triggerTimecode.samples -= mPostroll.samples; //trigger the stop command before a postroll period to make sure recorders get it in time
            triggerTimecode.samples %= 24LL * 3600 * triggerTimecode.edit_rate.numerator / triggerTimecode.edit_rate.denominator; //wrap (either way)
            mCountdownTimer->Start(1000); //countdown in seconds - this is only for the button label
            //set the trigger to chunk
            mTimepos->SetTrigger(&triggerTimecode, dynamic_cast<wxEvtHandler *>(GetParent()), true); //will inform main frame when trigger occurs; always in the future
        }
    }
    else {
        //stop chunking
        mCountdownTimer->Stop();
        mTimepos->SetTrigger(&InvalidMxfTimecode);
    }
    mRealign = false;
}

/// Responds to the countdown timer by updating the label on the chunking button.
void ChunkingDlg::OnTimer(wxTimerEvent & WXUNUSED(event))
{
    if (mCountdown) {
        mCountdown--;
    }
}

/// Resets the countdown to the full chunk size.
void ChunkingDlg::Reset()
{
    mChunkLength = (unsigned long) mChunkSizeCtrl->GetValue() * 60; //in seconds
    mChunkAlignment = Alignments[mChunkAlignCtrl->GetSelection()] * 60; //in seconds
    mCountdown = mChunkLength;
    if (mCountdownTimer->IsRunning()) {
        //nicer if it's accurate to the second
        mCountdownTimer->Stop();
        mCountdownTimer->Start();
    }
}

/// Returns the label for the chunking button.
const wxString ChunkingDlg::GetChunkButtonLabel()
{
    wxString label;
    if (mEnableCheckBox->IsChecked()) {
        if (mChunkAlignment && !mCountdownTimer->IsRunning()) { //don't know when the next chunk is going to be because that can only be determined when we start recording
            label = wxT("Chunk ???:??");
        }
        else {
            label = wxString::Format(wxT("Chunk %d:%02d"), mCountdown / 60, mCountdown % 60);
        }
    }
    else {
        label = wxT("Chunk ---:--");
    }
    return label;
}

/// Returns the colour of the chunking button
const wxColour ChunkingDlg::GetChunkButtonColour()
{
    return mEnableCheckBox->IsChecked() ? BUTTON_WARNING_COLOUR : wxNullColour;
}


/// Returns the tooltip for the chunking button.
const wxString ChunkingDlg::GetChunkButtonToolTip()
{
    wxString tooltip;
    if (mEnableCheckBox->IsChecked()) {
        if (mChunkAlignment) { //don't know when the next chunk is going to be because that can only be determined when we start recording
            tooltip = wxT("Automatic chunking is enabled, aligned with timecode");
        }
        else {
            tooltip = wxT("Automatic chunking is enabled");
        }
    }
    else {
        tooltip = wxT("Automatic chunking is disabled");
    }
    return tooltip;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(SelectRecDlg, wxDialog)
    EVT_CHOICE(PROJECT, SelectRecDlg::OnSelectProject)
    EVT_CHOICE(DATE, SelectRecDlg::OnSelectDate)
    EVT_LISTBOX(RECORDING, SelectRecDlg::OnSelectRecording)
    EVT_LISTBOX_DCLICK(RECORDING, SelectRecDlg::OnDoubleClick)
    EVT_BUTTON(UP, SelectRecDlg::OnNavigateRecordings)
    EVT_BUTTON(DOWN, SelectRecDlg::OnNavigateRecordings)
    EVT_TOGGLEBUTTON(PREFER_ONLINE, SelectRecDlg::OnPreferOnline)
END_EVENT_TABLE()

#define NO_RECORDINGS wxT("NO RECORDINGS")

/// @param root Root path of recordings
SelectRecDlg::SelectRecDlg(wxWindow * parent, const wxString & root) : wxDialog(parent, wxID_ANY, wxT("Select a recording"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), mRoot(root)
{
    //controls
    wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(mainSizer);

    wxStaticBoxSizer * projectBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Project"));
    mainSizer->Add(projectBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
    mProjectSelector = new wxChoice(this, PROJECT);
    projectBox->Add(mProjectSelector, 1, wxEXPAND);

    wxStaticBoxSizer * dateBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Date"));
    mainSizer->Add(dateBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
    mDateSelector = new wxChoice(this, DATE);
    dateBox->Add(mDateSelector, 1, wxEXPAND);

    wxStaticBoxSizer * recordingBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Recording"));
    mainSizer->Add(recordingBox, 1, wxEXPAND | wxALL, CONTROL_BORDER);
    wxBoxSizer * recSizer = new wxBoxSizer(wxHORIZONTAL);
    recordingBox->Add(recSizer, 1, wxEXPAND);
    wxSize size = wxDefaultSize;
    size.SetHeight(100);
    mRecordingSelector = new wxListBox(this, RECORDING, wxDefaultPosition, size);
    recSizer->Add(mRecordingSelector, 1, wxEXPAND);
    wxBoxSizer * arrowsSizer = new wxBoxSizer(wxVERTICAL);
    recSizer->Add(arrowsSizer, 0, wxEXPAND);
    mUpButton = new wxBitmapButton(this, UP, wxBitmap(up));
    arrowsSizer->Add(mUpButton);
    arrowsSizer->AddStretchSpacer();
    mDownButton = new wxBitmapButton(this, DOWN, wxBitmap(down));
    arrowsSizer->Add(mDownButton);

    wxStaticBoxSizer * qualityBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Quality"));
    mainSizer->Add(qualityBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
    mPreferOnline = new wxToggleButton(this, PREFER_ONLINE, wxT("Prefer Online"));
    qualityBox->Add(mPreferOnline, 0, wxEXPAND | wxALL, CONTROL_BORDER);
    mOnlineMessage = new wxStaticText(this, wxID_ANY, wxEmptyString);
    qualityBox->Add(mOnlineMessage, 0, wxEXPAND | wxALL | wxALIGN_CENTRE_VERTICAL, CONTROL_BORDER);

    mainSizer->Add(CreateButtonSizer(wxOK|wxCANCEL), 0, wxALL, CONTROL_BORDER);

    //accelerator keys
    wxAcceleratorEntry accelerators[2];
    accelerators[0].Set(wxACCEL_NORMAL, WXK_UP, UP);
    accelerators[1].Set(wxACCEL_NORMAL, WXK_DOWN, DOWN);
    wxAcceleratorTable table(2, accelerators);
    SetAcceleratorTable(table);

    //window
    Fit(); //needed as size of recording selector
    SetMinSize(GetSize()); //stops window being shrunk far enough for controls to overlap
}

/// Populates the project selector with available projects.
/// If previously-selected project is found, or only one date is found, selects this and generates a project select event.
/// Displays dialogue.
int SelectRecDlg::ShowModal()
{
    //reset all controls, as available material may have changed
    mProjectSelector->Clear();
    mProjectSelector->Disable();
    mDateSelector->Clear();
    mDateSelector->Disable();
    mRecordingSelector->Clear();
    mUpButton->Disable();
    mDownButton->Disable();
    mPreferOnline->SetBackgroundColour(wxNullColour);
    mOnlineMessage->Hide();
    FindWindow(wxID_OK)->Disable();
    //choose category directory
    mCategoryDir = mRoot + wxFileName::GetPathSeparator() + OFFLINE_SUBDIR; //prioritise offline dir as smaller files and more likely to be there
    wxDir categoryDir(mCategoryDir);
    mOfflineDir = categoryDir.IsOpened();
    if (!mOfflineDir) {
        //look for online directory
        mCategoryDir = mRoot + wxFileName::GetPathSeparator() + ONLINE_SUBDIR;
        categoryDir.Open(mCategoryDir);
    }
    if (categoryDir.IsOpened()) {
        if (mOfflineDir) {
            mOnlineMessage->SetLabel(wxT("Online available")); //will be shown/hidden as necessary
        }
        else {
            mOnlineMessage->SetLabel(wxT("Online only"));
            mOnlineMessage->Show(); //this message will be shown permanently
        }
        wxString project;
        if (categoryDir.GetFirst(&project, wxEmptyString, wxDIR_DIRS)) { //GetAllFiles() doesn't return directory names so use this method instead
            //populate project selector
            wxArrayString projects; //to allow sorting
            do {
                projects.Add(project);
            } while (categoryDir.GetNext(&project));
            projects.Sort();
            mProjectSelector->Append(projects);
            //if only a single project, select it for convenience
            if (1 == projects.GetCount()) {
                mProjectSelector->SetSelection(0);
                wxCommandEvent event(wxEVT_COMMAND_CHOICE_SELECTED, PROJECT);
                AddPendingEvent(event); //trigger repopulating of date selector
            }
            else {
                mProjectSelector->Enable();
                //maintain the selection if this was the previously selected project
                int index;
                if (wxNOT_FOUND != (index = projects.Index(mSelectedProject))) {
                    mProjectSelector->SetSelection(index);
                    wxCommandEvent event(wxEVT_COMMAND_CHOICE_SELECTED, PROJECT);
                    AddPendingEvent(event); //trigger repopulating of date selector
                }
            }
        }
        else { //no projects
            mProjectSelector->Append(wxT("NO PROJECTS"));
            mProjectSelector->Select(0);
        }
    }
    else {
        mProjectSelector->Append(wxT("NO MATERIAL DIRECTORIES"));
        mProjectSelector->Select(0);
    }
    Fit(); //make sure all text can all be displayed
    return wxDialog::ShowModal();
}

/// Populates the date selector with available dates for currently selected project.
/// If previously-selected date is found, or only one date is found, selects this and generates a date select event.
void SelectRecDlg::OnSelectProject(wxCommandEvent & WXUNUSED(event))
{
    //reset date selector and controls depending on it
    mDateSelector->Clear();
    mDateSelector->Disable();
    mRecordingSelector->Clear();
    mUpButton->Disable();
    mDownButton->Disable();
    mPreferOnline->SetBackgroundColour(wxNullColour);
    if (mOfflineDir) mOnlineMessage->Hide();
    FindWindow(wxID_OK)->Disable();
    //look at project directory
    mSelectedProject = mProjectSelector->GetStringSelection();
    wxDir projDir(mCategoryDir + wxFileName::GetPathSeparator() + mSelectedProject);
    wxString date;
    wxArrayString dates; //to allow audio files to be found first, by sorting
    if (projDir.GetFirst(&date, wxEmptyString, wxDIR_DIRS)) { //GetAllFiles() doesn't return directory names so use this method instead
        //populate date selector
        long value;
        do {
            //check for valid directory name
            if (8 == date.Len() && wxNOT_FOUND == date.Find(wxT("-")) && date.ToLong(&value)) { //it's an 8-digit date directory
                dates.Add(date);
            }
        } while (projDir.GetNext(&date));
        dates.Sort();
        wxDateTime dateTime;
        for (size_t i = 0; i < dates.GetCount(); i++) {
            date = dates[i];
            //show a formatted date as the choice
            date.Left(4).ToLong(&value);
            dateTime.SetYear((int) value);
            date.Mid(4, 2).ToLong(&value);
            dateTime.SetMonth((wxDateTime::Month) (value - 1));
            date.Right(2).ToLong(&value);
            dateTime.SetDay((wxDateTime::wxDateTime_t) value);
//            mDateSelector->Append(dateTime.Format(wxT("%a %d %b %g"))); //%g seems to be broken near the beginning of the year, showing a year behind
            mDateSelector->Append(dateTime.Format(wxT("%a %d %b ")) + date.Left(4)); //see above
            //store the date directory name for later use
            mDateSelector->SetClientObject(mDateSelector->GetCount() - 1, new StringContainer(date)); //deleted by control
        }
        //if only a single date, select it for convenience
        if (1 == dates.GetCount()) {
            mDateSelector->SetSelection(0);
            wxCommandEvent event(wxEVT_COMMAND_CHOICE_SELECTED, DATE);
            AddPendingEvent(event); //trigger repopulating of recording selector
        }
        else if (dates.GetCount()) {
            mDateSelector->Enable();
            //maintain the selection if this was the previously selected date
            int index;
            if (wxNOT_FOUND != (index = dates.Index(mSelectedDate))) {
                mDateSelector->SetSelection(index);
                wxCommandEvent event(wxEVT_COMMAND_CHOICE_SELECTED, DATE);
                AddPendingEvent(event); //trigger repopulating of recording selector
            }
        }
    }
    if (!dates.GetCount()) { //no dates for selected project
        mDateSelector->Append(wxT("NO DATES"));
        mDateSelector->Select(0);
    }
}

/// Populates the recording selector with available recordings for currently selected project and date.
/// If previously-selected recording is found, or only one recording is found, selects this and enables the OK button.
void SelectRecDlg::OnSelectDate(wxCommandEvent & WXUNUSED(event))
{
    //reset recording selector and associated controls
    mRecordingSelector->Clear();
    mUpButton->Disable();
    mDownButton->Disable();
    mPreferOnline->SetBackgroundColour(wxNullColour);
    if (mOfflineDir) mOnlineMessage->Hide();
    FindWindow(wxID_OK)->Disable();
    //look at date directory
    mSelectedDate = dynamic_cast<StringContainer *>(mDateSelector->GetClientObject(mDateSelector->GetSelection()))->GetString();
    wxArrayString paths; //to allow sorting
    wxDir::GetAllFiles(mCategoryDir + wxFileName::GetPathSeparator() + mSelectedProject + wxFileName::GetPathSeparator() + mSelectedDate, &paths, wxEmptyString, wxDIR_FILES); //likely to be several files per recording
    //populate recording selector
    paths.Sort(); //into time order
    long value;
    for (size_t i = 0; i < paths.GetCount(); i++) {
        //check for valid filename
        wxString file = paths[i].Mid(paths[i].Find(wxFileName::GetPathSeparator(), true) + 1); //remove path
        if (file.Left(8) == mSelectedDate //first 8 digits (date)
         && file[8] == wxT('_') //following underscore
         && wxNOT_FOUND == file.Mid(9, 8).Find(wxT("-")) && file.Mid(9, 8).ToLong(&value) //next 8 digits (time)
         && file[17] == wxT('_') //following underscore
         && 0 == file.Mid(18).Find(mSelectedProject + wxT("_")) //project name and following underscore
         && !file.Right(4).CmpNoCase(wxT(".mxf")) //extension
        ) { //it's a valid filename
            wxString startTime = file.Mid(9, 2) + wxT(":") + file.Mid(11, 2) + wxT(":") + file.Mid(13, 2) + wxT(":") + file.Mid(15, 2);
            if (!mRecordingSelector->GetCount() || mRecordingSelector->GetString(mRecordingSelector->GetCount() - 1).Left(11) != startTime) { //new recording
                mRecordingSelector->Append(startTime + (file.Right(6)[0] == wxT('a') ? wxT(" (Audio only)") : wxEmptyString)); //subsequently found video tracks will overwrite the "audio only" indication
            }
            else if (file.Right(6)[0] != wxT('a')) { //already added this recording, and this is a video track
                mRecordingSelector->SetString(mRecordingSelector->GetCount() - 1, startTime); //it might have been marked audio only
            }
            //maintain the selection if this was the previously selected recording
            if (mSelectedRecording == startTime && wxNOT_FOUND == mRecordingSelector->GetSelection()) { //not already selected as a previous file
                //maintain the selection
                mRecordingSelector->SetSelection(mRecordingSelector->GetCount() - 1);
                wxCommandEvent event(wxEVT_COMMAND_LISTBOX_SELECTED, RECORDING);
                AddPendingEvent(event); //trigger updating of other controls
            }
        }
    }
    //if only a single recording, select it for convenience
    if (1 == mRecordingSelector->GetCount() && wxNOT_FOUND == mRecordingSelector->GetSelection()) { //not already selected by being the previously selected recording
        mRecordingSelector->SetSelection(0);
        wxCommandEvent event(wxEVT_COMMAND_LISTBOX_SELECTED, RECORDING);
        AddPendingEvent(event); //trigger updating of other controls
    }
    else if (!mRecordingSelector->GetCount()) {
        mRecordingSelector->Append(NO_RECORDINGS); //control has to be enabled in order to display this.  Selection of it won't be allowed.
    }
}

/// Sets state of up and down buttons, OK button, background colour of PreferOnline button and visibility of online message depending on the recording selected.
void SelectRecDlg::OnSelectRecording(wxCommandEvent & WXUNUSED(event))
{
    if (wxNOT_FOUND != mRecordingSelector->GetSelection()) { //sometimes spurious events appear
        if (NO_RECORDINGS != mRecordingSelector->GetString(0)) {
            mSelectedRecording = mRecordingSelector->GetString(mRecordingSelector->GetSelection()).Left(11); //remove any "audio only" indication
            FindWindow(wxID_OK)->Enable();
            mUpButton->Enable(mRecordingSelector->GetSelection());
            mDownButton->Enable((unsigned int) mRecordingSelector->GetSelection() != mRecordingSelector->GetCount() - 1);
            FindWindow(wxID_OK)->SetFocus(); //If this is not done, the OK button ends up with or without focus depending on whether the arrow button just pressed remains enabled or not
        //see if online is available
            if (mOfflineDir) {
                wxArrayString dummy;
                GetPaths(dummy, true); //get online paths
                if (dummy.GetCount()) {
                    mOnlineMessage->Show(dummy.GetCount());
                    Layout(); //needed to put message in the right place
                }
                mPreferOnline->SetBackgroundColour((mPreferOnline->GetValue() && mOnlineMessage->IsShown()) ? BUTTON_WARNING_COLOUR : wxNullColour); //highlight button if prefer online and online file(s) are present
            }
        }
        else { //no recordings
            mRecordingSelector->SetSelection(wxNOT_FOUND); //not allowed to select this item
        }
    }
}

/// Closes the dialogue as if the OK button had been pressed.
void SelectRecDlg::OnDoubleClick(wxCommandEvent & WXUNUSED(event))
{
    if (NO_RECORDINGS != mRecordingSelector->GetString(0)) {
        EndModal(wxID_OK);
    }
}

/// Selects previous or next recording in the list, if possible.
void SelectRecDlg::OnNavigateRecordings(wxCommandEvent & event)
{
    //check that we can move in the requested direction (as this might be called from an accelerator key, which is always enabled)
    if (mRecordingSelector->GetCount()
     && ((UP == event.GetId() && mRecordingSelector->GetSelection() > 0) //want to go up and not at the top
      || (DOWN  == event.GetId() && mRecordingSelector->GetSelection() != (int) mRecordingSelector->GetCount() - 1) //want to go down and not at the bottom
    )) {
        mRecordingSelector->Select(mRecordingSelector->GetSelection() + (UP == event.GetId() ? -1 : 1));
        wxCommandEvent newEvent(wxEVT_COMMAND_LISTBOX_SELECTED, RECORDING);
        AddPendingEvent(newEvent);
    }
}

/// Sets the background colour of the PreferOnline button depending on its state and if online material is available.
void SelectRecDlg::OnPreferOnline(wxCommandEvent & WXUNUSED(event))
{
    mPreferOnline->SetBackgroundColour(mOfflineDir && (mPreferOnline->GetValue() && mOnlineMessage->IsShown()) ? BUTTON_WARNING_COLOUR : wxNullColour); //highlight button if there is a choice of types, prefer online and online file(s) are present
}

/// Gets the paths of the files of the selected recording.
/// @param paths Returns the full path of each file, sorted alphabetically.
/// @param selectOnline Forces online files to be returned; otherwise, online files are returned if they are the only ones available or PreferOnline button is pressed and at least one is available.
void SelectRecDlg::GetPaths(wxArrayString & paths, bool selectOnline)
{
    paths.Clear();
    wxString stem = wxString(mRoot) + wxFileName::GetPathSeparator() + ((!mOfflineDir || selectOnline || BUTTON_WARNING_COLOUR == mPreferOnline->GetBackgroundColour()) ? ONLINE_SUBDIR : OFFLINE_SUBDIR) + wxFileName::GetPathSeparator() + mSelectedProject + wxFileName::GetPathSeparator() + mSelectedDate;
    wxDir::GetAllFiles(stem, &paths, wxEmptyString, wxDIR_FILES); //all files in the recording's directory
    stem += wxFileName::GetPathSeparator() + mSelectedDate + wxT("_") + mSelectedRecording.Left(2) + mSelectedRecording.Mid(3, 2) + mSelectedRecording(6, 2) + mSelectedRecording(9, 2) + wxT("_") + mSelectedProject + wxT("_"); //common stem for all files in the wanted recording
    for (size_t i = 0; i < paths.GetCount(); i++) {
        if (paths[i].Find(stem) || paths[i].Right(4).CmpNoCase(wxT(".mxf"))) { //doesn't match
            paths.RemoveAt(i);
            i--; //next value moved to this position
        }
    }
    paths.Sort();
}

SetMaxChunksDlg::SetMaxChunksDlg(wxWindow * parent, unsigned int currentValue) : wxDialog(parent, wxID_ANY, wxT("Maximum number of chunks to display"))
{
    wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(mainSizer);
    wxBoxSizer * hSizer = new wxBoxSizer(wxHORIZONTAL);
    mainSizer->Add(hSizer);
    hSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Maximum chunks remembered:")), 0, wxALIGN_CENTRE_VERTICAL | wxALL, CONTROL_BORDER);
    mSpinCtrl = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, 10, INT_MAX, currentValue);
    hSizer->Add(mSpinCtrl, 0, wxALIGN_CENTRE | wxALL, CONTROL_BORDER);
    mainSizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_CENTRE | wxALL, CONTROL_BORDER);
    Fit();
}

unsigned int SetMaxChunksDlg::GetMaxChunks()
{
    return mSpinCtrl->GetValue();
}

