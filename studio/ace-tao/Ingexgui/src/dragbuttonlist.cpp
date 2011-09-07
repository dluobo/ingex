/***************************************************************************
 *   $Id: dragbuttonlist.cpp,v 1.21 2011/09/07 15:07:08 john_f Exp $      *
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

#include "dragbuttonlist.h"
#include "ingexgui.h"
#include "eventlist.h"
#include "avid_mxf_info.h"
#include "mxf_reader.h"

BEGIN_EVENT_TABLE(DragButtonList, wxScrolledWindow)
    EVT_RADIOBUTTON(wxID_ANY, DragButtonList::OnRadioButton)
    EVT_UPDATE_UI(wxID_ANY, DragButtonList::OnUpdateUI)
END_EVENT_TABLE()

/// @param parent The parent window.
/// @param buttonEvtHandler Where to send events for button pushes.
DragButtonList::DragButtonList(wxWindow* parent, wxEvtHandler* buttonEvtHandler)
: wxScrolledWindow(parent), mButtonEvtHandler(buttonEvtHandler)
{
    SetSizer(new wxBoxSizer(wxVERTICAL));
    SetScrollRate(10, 10); //needed for scrollbars to appear
}

/// Updates the radio button enable and selected states
void DragButtonList::OnUpdateUI(wxUpdateUIEvent& event)
{
    if (event.GetId() > wxID_HIGHEST && event.GetId() <= (int) mEnableStates.GetCount() + wxID_HIGHEST) {
        event.Enable(mEnableStates[event.GetId() - wxID_HIGHEST - 1]);
    }
    if (event.GetId() == (int) mSelected + wxID_HIGHEST + 1) { //the selected button
        (dynamic_cast<wxRadioButton *>(event.GetEventObject()))->SetValue(true); //toggles the others automatically
    }
}

/// Notes the selection when a new button is pressed, and passes the event to the handler expecting to receive it
void DragButtonList::OnRadioButton(wxCommandEvent& event)
{
    mSelected = event.GetId() - wxID_HIGHEST - 1;
    event.SetId(mSelected);
    mButtonEvtHandler->AddPendingEvent(event);
}

/// Replaces current state with a new column of video track radio buttons, one for each track with a video file, and with a split view button at the top.
/// Returns information about the new state.
/// Single track buttons are labelled with the track name and have a tool tip showing the associated filename.
/// All buttons are disabled.
/// @param chunkInfo The file names, the track names and the track types.  Gets all info from here rather than examining the files themselves, because they may not be available yet.  Checks for null pointer.
/// @param fileNames Returns the file name associated with each video track which has a file, with the audio filenames at the end.
/// @param trackNames Returns corresponding names of tracks.
/// @param nVideoTracks Returns the number of video tracks (-1 if not known).
/// @return The input type.
prodauto::PlayerInputType DragButtonList::SetTracks(ChunkInfo* chunkInfo, std::vector<std::string> & fileNames, std::vector<std::string> & trackNames, int & nVideoTracks)
{
    Clear();
    fileNames.clear();
    trackNames.clear();
    nVideoTracks = -1; //unknown
    prodauto::PlayerInputType inputType = prodauto::MXF_INPUT;
    if (chunkInfo) {
        nVideoTracks = 0;
        std::vector<std::string> audioFileNames;
        if (chunkInfo->GetFiles()->GetCount()) { //this chunk has files associated
            wxRadioButton * split = new wxRadioButton(this, wxID_HIGHEST + 1, wxT("Split View")); //the split is always the first video track (id = 0)
            split->SetToolTip(wxT("Up to the first nine successfully opened files"));
            GetSizer()->Add(split, -1, wxEXPAND);
            mEnableStates.Add(false); //enable later if any files successfully loaded
            wxString name;
            for (size_t i = 0; i < chunkInfo->GetFiles()->GetCount(); i++) { //recorder loop
                wxArrayString uniqueNames;
                for (size_t j = 0; j < (*chunkInfo->GetFiles())[i]->length(); j++) { //file loop
                    name = wxString((*chunkInfo->GetFiles())[i][j].in(), *wxConvCurrent);
                    bool duplicated = (wxNOT_FOUND != uniqueNames.Index(name));
                    if (!duplicated) {
                        uniqueNames.Add(name);
                    }
                    if (ProdAuto::VIDEO == chunkInfo->GetTracks()[i][j].type && !name.IsEmpty()) {
                        if (!duplicated) {
                            fileNames.push_back((*chunkInfo->GetFiles())[i][j].in());
                            nVideoTracks++;
                        }
                        wxRadioButton * rb = new wxRadioButton(this, fileNames.size() + wxID_HIGHEST + 1, wxString(chunkInfo->GetTracks()[i][j].src.package_name, *wxConvCurrent)); //ID corresponds to file index
                        rb->SetToolTip(name);
                        GetSizer()->Add(rb, -1, wxEXPAND);
                        mEnableStates.Add(false); //we don't know whether the player can open this file yet
                        trackNames.push_back(chunkInfo->GetTracks()[i][j].src.package_name.in());
                    }
                    else if (ProdAuto::AUDIO == chunkInfo->GetTracks()[i][j].type && !name.IsEmpty() && !duplicated) {
                        audioFileNames.push_back((*chunkInfo->GetFiles())[i][j].in());
                    }
                }
            }
            if (fileNames.size()) {
                wxString extn = wxString(fileNames[0].c_str(), *wxConvCurrent).Right(4);
                if (!extn.CmpNoCase(wxT(".mov"))) {
                    inputType = prodauto::FFMPEG_INPUT;
                }
                else if (!extn.CmpNoCase(wxT(".dv"))) {
                    inputType = prodauto::DV_INPUT;
                }
            }

        }
        Layout();
        //add audio files at the end
        for (size_t i = 0; i < audioFileNames.size(); i++) {
            fileNames.push_back(audioFileNames.at(i));
        }
    }
    return inputType;
}

/// Alternative to SetTracks for MXF file mode
/// Replaces current state with a new column of video file radio buttons, and with a split view button at the top.
/// Returns information about the current state.
/// Single track buttons are labelled with the Clip Track String and have a tool tip showing the associated filename.
/// All buttons are disabled.
/// @param paths The file names.
/// @param fileNames Returns the file name associated with each video file, with the audio filenames at the end.
/// @param trackNames Returns corresponding Clip Track Strings.
/// @param nVideoTracks Returns the number of video tracks.
/// @return The input type.
prodauto::PlayerInputType DragButtonList::SetMXFFiles(wxArrayString & paths, std::vector<std::string> & fileNames, std::vector<std::string> & trackNames, int & nVideoTracks)
{
    Clear();
    fileNames.clear();
    trackNames.clear();
    nVideoTracks = 0;
    wxRadioButton * split = new wxRadioButton(this, wxID_HIGHEST + 1, wxT("Split view")); //the split is always the first video track (id = 0)
    split->SetToolTip(mSplitButtonToolTip);
    GetSizer()->Add(split, -1, wxEXPAND);
    mEnableStates.Add(false); //enable later if any files successfully loaded
    std::vector<std::string> audioFileNames;
    mProjectName.Clear();
    AvidMXFInfo info;
    MXFReader *mxfReader;
    mxfRational frameRate;
    for (size_t i = 0; i < paths.GetCount(); i++) {
        std::string path = (const char *) paths[i].mb_str(*wxConvCurrent);
        if (!ami_read_info(path.c_str(), &info, 0)) { //recognises the file
            if (info.isVideo) {
                fileNames.push_back(path);
                wxRadioButton * rb = new wxRadioButton(this, fileNames.size() + wxID_HIGHEST + 1, wxString(info.tracksString, *wxConvCurrent)); //ID corresponds to file index
                rb->SetToolTip(paths[i]);
                GetSizer()->Add(rb, -1, wxEXPAND);
                mEnableStates.Add(false); //we don't know whether the player can open this file yet
                trackNames.push_back(info.tracksString);
                wxString p = wxString(info.projectName, *wxConvCurrent);
                if (info.projectName && p != mProjectName) {
                    if (mProjectName.Length()) {
                        mProjectName = wxT("<Various projects>");
                    }
                    else {
                        mProjectName = p;
                    }
                }
                nVideoTracks++;
            }
            else {
                audioFileNames.push_back(path);
            }
            ami_free_info(&info);
        }
        else if (open_mxf_reader(path.c_str(), &mxfReader)) {
            if (clip_has_video(mxfReader)) {
                fileNames.push_back(path);
                wxRadioButton * rb = new wxRadioButton(this, fileNames.size() + wxID_HIGHEST + 1, wxString(get_tracks_string(mxfReader), *wxConvCurrent)); //ID corresponds to file index
                rb->SetToolTip(paths[i]);
                GetSizer()->Add(rb, -1, wxEXPAND);
                mEnableStates.Add(false); //we don't know whether the player can open this file yet
                trackNames.push_back(get_tracks_string(mxfReader));
                get_frame_rate(mxfReader, &frameRate);
                nVideoTracks++;
            }
            else {
                audioFileNames.push_back(path);
            }
            close_mxf_reader(&mxfReader);
        }
    }
    Layout();
    //add audio files at the end
    for (size_t i = 0; i < audioFileNames.size(); i++) {
        fileNames.push_back(audioFileNames.at(i));
    }
    return prodauto::MXF_INPUT;
}

/// Returns the project name generated from the last call to SetMXFFiles()
const wxString DragButtonList::GetProjectName()
{
    return mProjectName;
}

/// Alternative to SetTracks for E to E mode.
/// Replaces current state with a new column of video source radio buttons, and with a split view button at the top.
/// Sources are given fixed names.
/// All buttons are disabled.
/// @param sources Returns the source name associated with each source.
/// @param names Returns corresponding displayed names.
/// @param nVideoTracks Returns the number of video tracks.
/// @return The input type.

#define N_SOURCES 4

#ifndef DISABLE_SHARED_MEM_SOURCE
prodauto::PlayerInputType DragButtonList::SetEtoE(std::vector<std::string> & sources, std::vector<std::string> & names, int & nVideoTracks)
{
    Clear();
    sources.clear();
    names.clear();
    nVideoTracks = N_SOURCES;
    wxRadioButton * split = new wxRadioButton(this, wxID_HIGHEST + 1, wxT("Split view")); //the split is always the first video source (id = 0)
    GetSizer()->Add(split, -1, wxEXPAND);
    mEnableStates.Add(false); //enable later if any sources successfully opened
    char source[3];
    mEnableStates.SetCount(sources.size() + 1);
    wxString name;
    for (unsigned int i = 0; i < N_SOURCES; i++) {
        sprintf(source, "%dp", i);
        sources.push_back(source);
        name.Printf(wxT("Live %d"), i + 1);
        names.push_back((const char *) name.mb_str(*wxConvCurrent));
        wxRadioButton * rb = new wxRadioButton(this, sources.size() + wxID_HIGHEST + 1, name);
        GetSizer()->Add(rb, -1, wxEXPAND);
        mEnableStates.Add(false); //we don't know whether the player can open this source yet
    }
    Layout();
    return prodauto::SHM_INPUT;
}
#endif

/// Updates the label on one of the buttons.
/// @param id The source ID, starting at 0 for the split.  Checked for validity.
/// @param name The new name.
void DragButtonList::SetSourceName(const unsigned int id, const wxString& name)
{
    wxWindow *button = FindWindow(id + wxID_HIGHEST + 1);
    if (button) button->SetLabel(name);
}

/// Enables/disables and selects the track select buttons.  Does nothing if no buttons.
/// Split button is enabled if any of the other buttons are.
/// COMMENTED OUT: Hides the split view button if there is only one track enabled.
/// @param enables Enable state of each button (excluding the split).
/// @param selected The button to select, 0 being the split.
void DragButtonList::EnableAndSelectTracks(std::vector<bool> * enables, const unsigned int selected)
{
    if (mEnableStates.GetCount()) {
        bool someOK = false;
        for (size_t i = 0; i < enables->size(); i++) {
            if (i >= mEnableStates.GetCount() - 1) { //sanity check
                break;
            }
            mEnableStates[i + 1] = enables->at(i); //shifted down one by split view
            someOK |= enables->at(i);
        }
        if (selected < mEnableStates.GetCount()) { //sanity check
            mSelected = selected;
        }
//      if (enables->size() < 2) {
//          //no point in having a split view if only one track to show
//          GetSizer()->GetItem((size_t) 0)->GetWindow()->Hide(); ... this will need updating to reflect MVC philosophy
//      }
//      else {
            //enable split view if any files OK
            mEnableStates[0] = someOK;
//      }
    }
}

/// Removes all buttons.
void DragButtonList::Clear()
{
    GetSizer()->Clear(true); //delete all buttons
    mEnableStates.Clear();
    mSelected = 0;
}

/// Determines whether there are selectable tracks before the current one, or selects the next earlier track.
/// @param select True to select (if possible).
/// @return If select is false, true if there are earlier selectable tracks than that currently selected.  If select is true, true if a new track was selected.
bool DragButtonList::EarlierTrack(const bool select)
{
    int previous;
    for (previous = mSelected - 1; previous > -1; previous--) {
        if (mEnableStates[previous]) { //found an enabled button
            break;
        }
    }
    if (previous > -1 && select) {
        //select the previous enabled button
        Select(previous);
    }
    return previous > -1;
}

/// Determines whether there are selectable tracks after the current one, or selects the next track.
/// @param select True to select (if possible).
/// @return If select is false, true if there are later selectable tracks than that currently selected.  If select is true, true if a new track was selected.
bool DragButtonList::LaterTrack(const bool select)
{
    unsigned int next;
    for (next = mSelected + 1; next < mEnableStates.GetCount(); next++) {
        if (mEnableStates[next]) { //found an enabled button
            break;
        }
    }
    if (next < mEnableStates.GetCount() && select) {
        //select the next enabled button
        Select(next);
    }
    return next < mEnableStates.GetCount();
}

/// If the split view is displayed, switches source to that corresponding to the value given (if it is available).
/// If an individual source is displayed, switches to the split view (thus providing toggling behaviour).
/// @param source The source (1-9).
void DragButtonList::ToggleSplitView(const unsigned int source)
{
    if (0 == mSelected && source && source < mEnableStates.GetCount() && mEnableStates[source]) { //showing split view; sanity check; can select this source
        //show individual source
        Select(source);
    }
    else if (mSelected) { //showing an individual source
        //show split view
        Select(0);
    }
}

/// Returns the ID of the selected source; 0 for a split or if there are no buttons.
unsigned int DragButtonList::GetSelectedSource()
{
    return mSelected;
}


/// Selects the given source; does not sanity-check the value
void DragButtonList::Select(unsigned int source)
{
    mSelected = source;
    wxCommandEvent event(wxEVT_COMMAND_RADIOBUTTON_SELECTED, mSelected);
    mButtonEvtHandler->AddPendingEvent(event); //Don't want to go through this class's event handler as no point and it will change the ID
}

/// Sets the tooltip on the split view button to reflect the number of sources it displays
/// @param limit True if intending to limit split to four sources
void DragButtonList::LimitSplitToQuad(bool limit)
{
    if (limit) {
        mSplitButtonToolTip = wxT("Up to the first four successfully opened files");
    }
    else {
        mSplitButtonToolTip = wxT("Up to the first nine successfully opened files");
    }
    wxWindow * split = FindWindow(wxID_HIGHEST + 1);
    if (split) split->SetToolTip(mSplitButtonToolTip);
}

//drag drop experiment

/*  wxArrayString names;
    names.Add(wxT("First"));
    names.Add(wxT("Second"));
    names.Add(wxT("Third"));
    names.Add(wxT("Fourth"));

    for (unsigned int i = 0; i < names.GetCount(); i++) {
        wxBoxSizer * bs = new wxBoxSizer(wxHORIZONTAL);
        playbackPageSizer->Add(bs);
        wxRadioButton * rb = new wxRadioButton(playbackPage, -1, wxEmptyString);
        bs->Add(rb);
        wxStaticText * tb = new wxStaticText(playbackPage, -1, names[i]);
        bs->Add(tb);

void DragButtonList::SelectSource(unsigned int source) {
    wxWindow * button = FindWindowById(source, this);
    if (button) { //assumption that button will be NULL if not found!
        //select the button
        ((wxRadioButtonbutton
        //send an event as if the button had been pressed by the user
    }
}

//drag drop experiment

    } */
