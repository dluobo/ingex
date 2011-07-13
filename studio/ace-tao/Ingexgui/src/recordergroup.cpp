/***************************************************************************
 *   $Id: recordergroup.cpp,v 1.28 2011/07/13 14:48:21 john_f Exp $       *
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

#include "recordergroup.h"
#include "controller.h"
#include "comms.h"
#include "timepos.h"
#include "ticktree.h"
#include "savedstate.h"
#include "colours.h"

DEFINE_EVENT_TYPE(EVT_RECORDERGROUP_MESSAGE)

BEGIN_EVENT_TABLE(RecorderGroupCtrl, wxListView)
    EVT_LEFT_DOWN(RecorderGroupCtrl::OnLMouseDown)
    EVT_MIDDLE_DOWN(RecorderGroupCtrl::OnUnwantedMouseDown)
    EVT_RIGHT_DOWN(RecorderGroupCtrl::OnRightMouseDown)
    EVT_COMMAND(ENABLE_REFRESH, EVT_RECORDERGROUP_MESSAGE, RecorderGroupCtrl::OnListRefreshed)
    EVT_CONTROLLER_THREAD(RecorderGroupCtrl::OnControllerEvent)
    EVT_COMMAND(wxID_ANY, EVT_TIMEPOS_EVENT, RecorderGroupCtrl::OnTimeposEvent)
END_EVENT_TABLE()

/// Initialises list with no recorders and creates Comms object for communication with the name server.
/// @param parent The parent window.
/// @param id The control's window ID.
/// @param pos The control's position.
/// @param size The control's size.
/// @param argc Argument count, for ORB initialisation.
/// @param argv Argument vector, for ORB initialisation.
RecorderGroupCtrl::RecorderGroupCtrl(wxWindow * parent, wxWindowID id, const wxPoint & pos, const wxSize & size, int& argc, char** argv) : wxListView(parent, id, pos, size, wxLC_REPORT | wxLC_NO_HEADER), mEnabledForInput(true),  mMaxPreroll(InvalidMxfDuration), mMaxPostroll(InvalidMxfDuration), mPreroll(InvalidMxfDuration), mSavedState(0), mMode(STOPPED)
{
    InsertColumn(0, wxEmptyString);
    InsertColumn(1, wxEmptyString);
    mComms = new Comms(this, argc, argv);
}

RecorderGroupCtrl::~RecorderGroupCtrl()
{
    delete mComms;
}

/// Deletes all controllers, and then issues an event indicating that this object is ready for deletion.
void RecorderGroupCtrl::Shutdown()
{
    mMode = DYING; //stops mouse clicks and indicates that we must issue a DIE event when any controllers have finished
    if (!DisconnectAll()) { //haven't got any controllers so not waiting for anything to destroy itself
        //write suicide note now
        wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, DIE);
        AddPendingEvent(frameEvent);
    }
}

/// Starts the process of obtaining a list of recorders from the name server, in another thread.
/// Stops clicks on the control to prevent recorders being selected which might disappear.
void RecorderGroupCtrl::StartGettingRecorders()
{
    if (mComms->GetStatus()) { //started properly
        if (0 == GetItemCount()) {
            //show that something's happening
            InsertItem(0, wxT("Getting list...")); //this will be removed when the list has been obtained
            SetItemBackgroundColour(0, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
            SetItemTextColour(0, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
            SetColumnWidth(0, wxLIST_AUTOSIZE);
            SetItemPtrData(0, (wxUIntPtr) 0); //no controller
        }
        mComms->StartGettingRecorders(EVT_RECORDERGROUP_MESSAGE, ENABLE_REFRESH); //threaded, so won't hang app; sends the given event when it's finished
    }
    mEnabledForInput = false;
}

/// Responds to Comms finishing obtaining a list of recorders.
/// Updates the list, removing recorders which weren't found and which aren't connected, adding new recorders, and requesting an update of available recording time from recorders that are connected.
/// @param event The command event.
void RecorderGroupCtrl::OnListRefreshed(wxCommandEvent & WXUNUSED(event))
{
    //Remove non-connected recorders and request available recording time from connected non-router recorders
    for (int i = 0; i < GetItemCount(); i++) {
        if (0 == GetItemData(i) || !((Controller *) GetItemData(i))->IsOK()) { //controller may just have been told to destroy itself (if we have been called from Disconnect()) so may still exist
            DeleteItem(i--);
        }
        else {
            ((Controller *) GetItemData(i))->RequestRecordTimeAvailable(); //should be connected
        }
    }
    //Put in any recorders not listed, in alphabetical order
    wxString errMsg;
    if (mComms->GetStatus(&errMsg)) { //Comms is alive
        mEnabledForInput = true;
        if (errMsg.IsEmpty()) {
            wxArrayString names;
            mComms->GetRecorderList(names);
            names.Sort();
            int item = -1;
            for (size_t j = 0; j < names.GetCount(); j++) {
                //determine position in the list of this recorder
                while (true) {
                    if (++item >= GetItemCount() || GetItemText(item).Cmp(names[j]) > 0) { //reached the end of the list or beyond where this recorder should be, so it is not in the list (item text should always contain the recorder name in this situation)
                        //add to list before current item (or end of list)
                        InsertItem(item, names[j]);
                        SetItemTextColour(item, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
                        SetItemBackgroundColour(item, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
                        SetColumnWidth(0, wxLIST_AUTOSIZE);
                        break; //original item has moved down by one, so will compare it again with the next recorder in the next iteration (despite it being incremented) in case it needs adding before it
                    }
                    else if (GetName(item) == names[j]) { //recorder already in the list
                        break;
                    }
                }
            }
        }
        else { //CORBA prob
            wxMessageDialog dlg(this, wxT("Failed to get list of recorders:\n") + errMsg, wxT("Comms problem"), wxICON_ERROR | wxOK);
            dlg.ShowModal();
        }
    }
}

/// Gets the name of the recorder at the given position.
/// @param index The position of the recorder name to return.
/// @return The recorder name; empty if no recorder at given position.
const wxString RecorderGroupCtrl::GetName(int index)
{
    wxString name;
    if (GetItemData(index)) name = ((Controller *) GetItemData(index))->GetName();
    return name;
}

/// Sets the label for the time available for recording.  Assumed only to be called for connected recorders.
/// @param index The position in the list of the recorder.
/// @param mins The number of minutes available; if negative, clears the message or if <60, sets warning background colour.
/// @param hide If true, clears the message but does not resize the column
void RecorderGroupCtrl::SetRecTimeAvailable(const unsigned int index, const long mins, const bool hide)
{
    wxListItem item;
    item.SetId(index);
    item.SetColumn(1);
    SetItemBackgroundColour(index, wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT));
    if (mins < 0 || hide) {
        item.SetText(wxEmptyString);
    }
    else if (mins < 60) {
        SetItemBackgroundColour(index, BUTTON_WARNING_COLOUR);
        item.SetText(wxString::Format(wxT("%d min"), mins));
    }
    else {
        item.SetText(wxString::Format(wxT("%d hr %d min"), mins/60, mins %60));
    }
    SetItem(item);
    if (!hide) SetColumnWidth(1, wxLIST_AUTOSIZE);
}

/// Deselects every connected/connecting recorder.
/// @return True if there are any controllers.
bool RecorderGroupCtrl::DisconnectAll()
{
    bool controllers = false;
    for (int i = 0; i < GetItemCount(); i++) {
        controllers |= Disconnect(i);
    }
    return controllers;
}

/// Disconnects a recorder.
/// If the recorder is fully connected, informs the frame of the disconnection, re-calculates preroll and postroll limits and looks for another recorder to get timecode from if this was the one being used.
/// @param index The position in the list of the recorder to disconnect from.  Checked to make sure the controller exists.
/// @return True if the controller exists.
bool RecorderGroupCtrl::Disconnect(const int index)
{
    bool exists = GetItemData(index);
    if (exists) { //sanity check
        if (((Controller *) GetItemData(index))->IsOK()) { //not already disconnecting or broken
            SetItemText(index, wxT("Disconnecting...")); //displaying recorder name or connecting message (highlighted)
            SetColumnWidth(0, wxLIST_AUTOSIZE);
            SetItemBackgroundColour(index, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); //shows the user that something's happening
            SetItemTextColour(index, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
            SetRecTimeAvailable(index, -1);
            if (mTree->IsRecorderPresent(GetName(index))) { //fully connected
                if (GetName(index) == mTimecodeRecorder) { //removing the recorder we're using to display timecode
                    //start looking for a new one
                    SetTimecodeRecorder();
                }
                //depopulate the source tree
                mTree->RemoveRecorder(GetName(index));
                wxCommandEvent event(EVT_RECORDERGROUP_MESSAGE, REMOVE_RECORDER);
                event.SetString(GetItemText(index));
                AddPendingEvent(event);
                //preroll and postroll limits might have been relaxed
                int i;
                mMaxPreroll.undefined = true;
                mMaxPostroll.undefined = true;
                //find a recorder
                for (i = 0; i < GetItemCount(); i++) {
                    if (i != index && GetItemData(i) && mTree->IsRecorderPresent(GetName(i))) {
                        mMaxPreroll = ((Controller *) GetItemData(i))->GetMaxPreroll();
                        mMaxPostroll = ((Controller *) GetItemData(i))->GetMaxPostroll();
                        break;
                    }
                }
                //update with the rest of the recorders
                for (; i < GetItemCount(); i++) {
                    if (i != index && GetItemData(i) && mTree->IsRecorderPresent(GetName(i))) {
                        mMaxPreroll.samples = ((Controller *) GetItemData(i))->GetMaxPreroll().samples < mMaxPreroll.samples ? ((Controller *) GetItemData(i))->GetMaxPreroll().samples : mMaxPreroll.samples;
                        mMaxPostroll.samples = ((Controller *) GetItemData(i))->GetMaxPostroll().samples < mMaxPostroll.samples ? ((Controller *) GetItemData(i))->GetMaxPostroll().samples : mMaxPostroll.samples;
                    }
                }
            }
        }
        ((Controller *) GetItemData(index))->Destroy(); //Destroy even if not OK
        StartGettingRecorders(); //Refresh the list in case this recorder has disappeared since connecting to it.
    }
    return exists;
}

/// Responds to a left mouse click on the control to select or deselect a recorder.
/// Have to intercept the mouse click event in order to allow an unmodified click to deselect the recorder.
/// If selecting, start the connection process in another thread and put a holding message in the list.
/// If deselecting, send an event to the parent and adjust maximum preroll and postroll values to the constraints imposed by the remaining recorders.
/// @param event The mouse event.
void RecorderGroupCtrl::OnLMouseDown(wxMouseEvent & event)
{
    int clickedItem;
    int flags = wxLIST_HITTEST_ONITEM;
    if (mEnabledForInput && DYING != mMode && wxNOT_FOUND != (clickedItem = HitTest(event.GetPosition(), flags, 0))) {
        if (GetItemData(clickedItem)) { //disconnect from a recorder
            wxMessageDialog dlg(this, wxT("Are you sure you want to disconnect from ") + GetName(clickedItem) + wxT("?"), wxT("Confirmation of Disconnect"), wxYES_NO | wxICON_QUESTION);
            if (!mTree->IsRecorderPresent(GetName(clickedItem)) || wxID_YES == dlg.ShowModal()) { //don't bother asking if in connecting phase
                Disconnect(clickedItem);
            }
        }
        else if (!GetItemData(clickedItem)) { //connect to a recorder
            SetItemPtrData(clickedItem, (wxUIntPtr) new Controller(GetItemText(clickedItem), mComms, this));
            SetItemText(clickedItem, wxT("Connecting...")); //displaying recorder name (not highlighted)
            SetColumnWidth(0, wxLIST_AUTOSIZE);
            SetItemBackgroundColour(clickedItem, wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)); //indicates to the user that something is happening - can't use select capability of list for this, as the colour can't be changed
            SetItemTextColour(clickedItem, wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
        }
    }
}

/// Responds to unwanted mouse clicks on the control by preventing them doing anything
/// @param event The mouse event.
void RecorderGroupCtrl::OnUnwantedMouseDown(wxMouseEvent & WXUNUSED(event))
{
}

/// Responds to right mouse click on the control by deselecting the item without confirmation
/// @param event The mouse event.
void RecorderGroupCtrl::OnRightMouseDown(wxMouseEvent & event)
{
    int clickedItem;
    int flags = wxLIST_HITTEST_ONITEM;
    if (mEnabledForInput && DYING != mMode && wxNOT_FOUND != (clickedItem = HitTest(event.GetPosition(), flags, 0)) && GetItemData(clickedItem)) {
        Disconnect(clickedItem);
    }
}

/// Responds to an event from one of the recorder controllers.
/// If a comms failure, send an event to that effect.
/// If just connected to a recorder, do a sanity check on the recorder parameters, send an event to the parent and adjust max preroll and postroll to accommodate this recorder's values.
/// If recording or stopping, send an event to the parent.
/// If status information, send events if timecode status has changed, and try to use this recorder to supply displayed timecode if there currently isn't one.
/// If this recorder is supplying displayed timecode, send an event with the timecode.
/// Send a track status event.
/// @param event The controller thread event.
void RecorderGroupCtrl::OnControllerEvent(ControllerThreadEvent & event)
{
    //find the entry in the list, and the corresponding controller
    int pos;
    for (pos = 0; pos < GetItemCount(); pos++) {
        if (event.GetName() == GetName(pos)) break;
    }
    Controller * controller = 0;
    if (pos < GetItemCount()) controller = (Controller *) GetItemData(pos); //controller must be present if name found in list, as names are guaranteed to be unique and not blank
    //deal with connection and disconnection
    if (Controller::DIE == event.GetCommand()) {
        if (controller) { //sanity check 
            delete controller;
            SetItemData(pos, 0);
            SetItemText(pos, event.GetName()); //displaying connecting (highlighted) or disconnecting (not highlighted) message
            SetColumnWidth(0, wxLIST_AUTOSIZE);
            SetItemBackgroundColour(pos, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); //Shows the user that something's happening
            SetItemTextColour(pos, wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
        }
        if (DYING == mMode) {
            int i;
            for (i = 0; i < GetItemCount(); i++) {
                if (GetItemData(i)) break;
            }
            if (GetItemCount() == i) { //no controllers
                wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, DIE);
                AddPendingEvent(frameEvent);
            }
        }
    }
    else if (Controller::CONNECT == event.GetCommand()) {
        //complete connection process if successful, or reset and report failure
        if (controller) { //sanity check
            if (Controller::SUCCESS == event.GetResult() && controller->IsOK()) { //cope with the event crossing a Destroy() call
                Controller * controller = (Controller *) GetItemData(pos);
                //check edit rate compatibility
                if (0 == mTree->GetRecorderCount() || (controller->GetMaxPreroll().edit_rate.numerator == mMaxPreroll.edit_rate.numerator && controller->GetMaxPreroll().edit_rate.denominator == mMaxPreroll.edit_rate.denominator)) { //the only connected recorder, or compatible edit rate (assume MaxPostroll has same edit rate as MaxPreroll)
                    //display
                    SetItemText(pos, event.GetName()); //displaying connecting message (highlighted)
                    SetColumnWidth(0, wxLIST_AUTOSIZE);
                    SetRecTimeAvailable(pos, event.GetRecordTimeAvailable());
                    //preroll and postroll
                    if (0 == mTree->GetRecorderCount()) { //the only connected recorder
                        mMaxPreroll = controller->GetMaxPreroll();
                        mPreroll = mMaxPreroll; //for the edit rate values
                        mPreroll.samples = mSavedState->GetUnsignedLongValue(wxT("Preroll"), DEFAULT_PREROLL); //limited later
                        mMaxPostroll = controller->GetMaxPostroll();
                        mPostroll = mMaxPostroll; //for the edit rate values
                        mPostroll.samples = mSavedState->GetUnsignedLongValue(wxT("Postroll"), DEFAULT_POSTROLL); //limited later
                    }
                    if (controller->GetMaxPreroll().samples < mMaxPreroll.samples) {
                        //limit mMaxPreroll (and possibly mPreroll) to new maximum
                        mMaxPreroll.samples = controller->GetMaxPreroll().samples;
                        mPreroll.samples = mPreroll.samples > controller->GetMaxPreroll().samples ? controller->GetMaxPreroll().samples : mPreroll.samples;
                    }
                    if (controller->GetMaxPostroll().samples < mMaxPostroll.samples) {
                        //limit mMaxPostroll (and possibly mPostroll) to new maximum
                        mMaxPostroll.samples = controller->GetMaxPostroll().samples;
                        mPostroll.samples = mPostroll.samples > controller->GetMaxPostroll().samples ? controller->GetMaxPostroll().samples : mPostroll.samples;
                    }
                    //check current project name is known
                    wxString currentProjectName = GetCurrentProjectName();
                    if (!currentProjectName.IsEmpty()) { //if it is empty, frame will force one to be selected
                        CORBA::StringSeq_var strings = event.GetStrings();
                        size_t i;
                        for (i = 0; i < strings->length(); i++) {
                            if (wxString((*strings)[i], wxConvISO8859_1) == currentProjectName) break;
                        }
                        if (strings->length() == i) { //project names may have been removed from the database and so the current project name may not be relevant any more
                            wxMessageDialog dlg(this, wxT("The current project name, ") + currentProjectName + wxT(", is not known to this recorder.  Do you wish to keep it?"), wxT("Current project name not found"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
                            if (wxID_YES == dlg.ShowModal()) {
                                //Send current project name to the recorder
                                CORBA::StringSeq newName;
                                newName.length(1);
                                newName[0] = (const char *) currentProjectName.mb_str(wxConvISO8859_1);
                                AddProjectNames(newName, controller);
                            }
                            else {
                                SetCurrentProjectName(wxEmptyString); //frame will force selection of new project name or deselect all recorders
                            }
                        }
                    }
                    //populate the source tree and tell the frame
                    wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, NEW_RECORDER);
                    frameEvent.SetString(event.GetName());
                    frameEvent.SetExtraLong(mTree->AddRecorder(event.GetName(), event.GetTrackList(), event.GetTrackStatusList(), controller->IsRouterRecorder())); //indicate if any tracks are recording
                    AddPendingEvent(frameEvent);
                }
                else { //edit rate incompatibility
                    wxMessageDialog dlg(this, wxT("Recorder \"") + event.GetName() + wxString::Format(wxT("\" has an edit rate incompatible with the existing recorder%s.  Deselecting "), mTree->GetRecorderCount() == 2 ? wxEmptyString : wxT("s")) + event.GetName() + wxString::Format(wxT(".\n\nEdit rate numerator: %d ("), controller->GetMaxPreroll().edit_rate.numerator) + event.GetName() + wxString::Format(wxT("); %d (existing)\nEdit rate denominator: %d ("), mMaxPreroll.edit_rate.numerator, controller->GetMaxPreroll().edit_rate.denominator) + event.GetName() + wxString::Format(wxT("); %d (existing)"), mMaxPreroll.edit_rate.denominator), wxT("Edit rate incompatibility"), wxICON_EXCLAMATION | wxOK);
                    dlg.ShowModal();
                    Disconnect(pos);
                }
            }
            else { //failure or comm failure
                Disconnect(pos);
                wxMessageDialog dlg(this, wxT("Couldn't connect to recorder \"") + event.GetName() + wxT("\": ") + event.GetMessage(), wxT("Initialisation failure"), wxICON_EXCLAMATION | wxOK);
                dlg.ShowModal();
            }
            SetItemText(pos, event.GetName());
            SetColumnWidth(0, wxLIST_AUTOSIZE);
        }
    }
    if (mTree->IsRecorderPresent(GetName(pos))) { //not been disconnected earlier in this function due to a reconnect failure, or has been destroyed (which can still result in an event being sent); guarantees there's a controller
        if (Controller::COMM_FAILURE == event.GetResult()) {
            wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, COMM_FAILURE);
            frameEvent.SetString(event.GetName());
            AddPendingEvent(frameEvent);
        }
        else {
            switch (event.GetCommand()) {
                case Controller::RECONNECT :
                    if (Controller::FAILURE == event.GetResult()) {
                        Disconnect(pos); //informs frame
                        wxMessageDialog dlg(this, wxT("Cannot reconnect automatically to ") + event.GetName() + wxT(" because ") + event.GetMessage() + wxT(".  Other aspects of this recorder may have changed also.  You must connect to it again manually and be aware that it has changed."), wxT("Cannot reconnect"), wxICON_ERROR | wxOK);
                        dlg.ShowModal();
                    }
                    else SetRecTimeAvailable(pos, event.GetRecordTimeAvailable()); //stats might have changed
                    break;
                case Controller::RECORD : {
                    //state change event
                    if (Controller::SUCCESS == event.GetResult()) { //no point in favouring non-router recorders and even if we wanted to, we don't necessarily know yet whether we have any
                        if (RECORD_WAIT == mMode) {
                            mMode = RECORDING;
                            wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, RECORD);
                            frameEvent.SetClientData(new ProdAuto::MxfTimecode(event.GetTimecode())); //deleted by event handler
                            AddPendingEvent(frameEvent);
                        }
                        else if (CHUNK_RECORD_WAIT == mMode) {
                            mMode = CHUNK_RECORDING;
                            wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, CHUNK_START);
                            frameEvent.SetClientData(new ProdAuto::MxfTimecode(event.GetTimecode())); //deleted by event handler
                            AddPendingEvent(frameEvent);
                        }
                    }
                    //update tree
                    if (Controller::SUCCESS == event.GetResult()) {
                        mTree->SetRecorderStateOK(event.GetName());
                    }
                    else if (IngexguiFrame::RUNNING_UP == (dynamic_cast<IngexguiFrame *>(GetParent())->GetStatus())) {
                        mTree->SetRecorderStateProblem(event.GetName(), wxT("Failed to record: retrying"));
                    }
                    //recorder update event
                    wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, RECORDER_STARTED);
                    frameEvent.SetString(event.GetName());
                    frameEvent.SetInt(Controller::SUCCESS == event.GetResult());
                    frameEvent.SetClientData(new ProdAuto::MxfTimecode(event.GetTimecode())); //must be deleted by event handler
                    AddPendingEvent(frameEvent);
                    break;
                }
                case Controller::STOP : {
                    //state change event
                    if (
                     Controller::SUCCESS == event.GetResult()
                     && (
                      !mHaveNonRouterRecorders //use any recorder if we only have router recorders
                      || !controller->IsRouterRecorder() //prefer the timecode returned from a non-router recorder
                     )) {
                        if (STOP_WAIT == mMode) {
                            mMode = STOPPED;
                            wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, STOP);
                            frameEvent.SetClientData(new ProdAuto::MxfTimecode(event.GetTimecode())); //this is the frame after the end of the recording; deleted by event handler
                            AddPendingEvent(frameEvent);
                        }
                        else if (CHUNK_STOP_WAIT == mMode) {
                            mMode = CHUNK_WAIT;
                            //remember the timecode for when the recorders are started
                            mChunkStartTimecode = event.GetTimecode(); //this is the frame after the end of the recording, which is when the new chunk will start
                            //report the chunk end
                            wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, CHUNK_END);
                            ProdAuto::MxfTimecode* chunkStartTimecode = new ProdAuto::MxfTimecode(mChunkStartTimecode); //deleted by event handler
                            frameEvent.SetClientData(chunkStartTimecode);
                            AddPendingEvent(frameEvent);
                            //set trigger for starting the next chunk
                            frameEvent.SetId(SET_TRIGGER);
                            ProdAuto::MxfTimecode* triggerTimecode = new ProdAuto::MxfTimecode(mChunkStartTimecode); //deleted by event handler
                            if (mMaxPreroll.edit_rate.numerator && ((int64_t) mMaxPreroll.samples) * mMaxPreroll.edit_rate.denominator / mMaxPreroll.edit_rate.numerator && mMaxPreroll.edit_rate.denominator) { //max preroll >= 1 second; sanity checks
                                //add half a second delay to the trigger to ensure that a start in the future isn't requested
                                triggerTimecode->samples += mMaxPreroll.edit_rate.numerator / 2 / mMaxPreroll.edit_rate.denominator;
                            }
                            else {
                                //add half the preroll to the trigger to ensure that a start in the future isn't requested
                                triggerTimecode->samples += mMaxPreroll.samples / 2;
                            }
                            frameEvent.SetClientData(triggerTimecode);
                            AddPendingEvent(frameEvent);
                        }
                        if (STOPPED == mMode) controller->RequestRecordTimeAvailable(); //should be connected
                    }
                    //update tree
                    if (Controller::SUCCESS == event.GetResult()) {
                        mTree->SetRecorderStateOK(event.GetName());
                    }
                    else {
                        mTree->SetRecorderStateProblem(event.GetName(), wxT("Failed to stop: retrying"));
                    }
                    //recorder update event (send after any state change event so event list knows which chunk info object to add the recorder data to)
                    wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, RECORDER_STOPPED);
                    frameEvent.SetString(event.GetName());
                    frameEvent.SetInt(Controller::SUCCESS == event.GetResult());
                    frameEvent.SetExtraLong(controller->IsRouterRecorder());
                    if (!event.GetExtraLong()) frameEvent.SetClientData(new RecorderData(event.GetTrackList(), event.GetStrings(), event.GetTimecode())); //must be deleted by event handler
                    AddPendingEvent(frameEvent);
                    break;
                }
                case Controller::REC_TIME_AVAILABLE :
                    SetRecTimeAvailable(pos, event.GetRecordTimeAvailable());
                    break;
                case Controller::GET_PROJECT_NAMES : {
                    wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, PROJECT_NAMES);
                    RecorderData * recorderData = new RecorderData(event.GetStrings()); //must be deleted by event handler
                    frameEvent.SetClientData(recorderData);
                    AddPendingEvent(frameEvent);
                    break;
                }
                case Controller::NONE : //never appears
                case Controller::CONNECT : //handled above
                case Controller::STATUS : //handled below
                case Controller::DIE : //never appears
                case Controller::SET_TAPE_IDS : //never appears
                case Controller::ADD_PROJECT_NAMES : //never appears
                    break;
            }
        }
        if (event.GetNTracks()) { //have status
            //timecode
            if (event.TimecodeStateHasChanged()) {
                wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE);
                frameEvent.SetString(event.GetName());
                switch (event.GetTimecodeState()) {
                    case Controller::RUNNING : case Controller::UNCONFIRMED :
                        frameEvent.SetId(TIMECODE_RUNNING);
                        break;
                    case Controller::STUCK : {
                        frameEvent.SetId(TIMECODE_STUCK);
                        RecorderData * recorderData = new RecorderData(event.GetTrackStatusList()[0].timecode); //must be deleted by event handler
                        frameEvent.SetClientData(recorderData);
                        break;
                    }
                    case Controller::ABSENT :
                        frameEvent.SetId(TIMECODE_MISSING);
                        break;
                }
                AddPendingEvent(frameEvent);
            }
            //timecode display
            Controller * mTimecodeController;
            if (Controller::ABSENT != event.GetTimecodeState() //we have some timecode
             &&
              (mTimecodeRecorder.IsEmpty() //not getting timecode from anywhere at the moment
               || -1 == FindItem(0, mTimecodeRecorder) //timecode recorder unknown
               || 0 == (mTimecodeController = (Controller *) GetItemData(FindItem(0, mTimecodeRecorder))) //timecode recorder not connected/sanity check
               || (mTimecodeController->IsRouterRecorder() && !controller->IsRouterRecorder()))) { //getting timecode from a router recorder at the moment, and this recorder is not a router recorder
                //set the display to this recorder
                SetTimecodeRecorder(event.GetName());
                //set the timecode value, running or stuck
                mTimecodeRecorderStuck = Controller::STUCK == event.GetTimecodeState();
                wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, mTimecodeRecorderStuck ? DISPLAY_TIMECODE_STUCK : DISPLAY_TIMECODE);
                RecorderData * recorderData = new RecorderData(event.GetTrackStatusList()[0].timecode); //must be deleted by event handler
                frameEvent.SetClientData(recorderData);
                AddPendingEvent(frameEvent);
            }
            else if (event.GetName() != mTimecodeRecorder && mTimecodeRecorderStuck && Controller::RUNNING == event.GetTimecodeState()) { //different recorder but timecode's running and current recorder is stuck
                //set the display to this recorder
                SetTimecodeRecorder(event.GetName());
                //set the timecode value, running
                mTimecodeRecorderStuck = false;
                wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, DISPLAY_TIMECODE);
                RecorderData * recorderData = new RecorderData(event.GetTrackStatusList()[0].timecode); //must be deleted by event handler
                frameEvent.SetClientData(recorderData);
                AddPendingEvent(frameEvent);
            }
//          else if (event.GetName() == mTimecodeRecorder && event.TimecodeStateHasChanged()) { //change to status of current display recorder
            else if (event.GetName() == mTimecodeRecorder) { //current display recorder
                if (Controller::ABSENT == event.GetTimecodeState()) {
                    //display no source
                    SetTimecodeRecorder();
                }
                else {
                    //set the timecode value, running or stuck
                    mTimecodeRecorderStuck = Controller::STUCK == event.GetTimecodeState();
                    wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, mTimecodeRecorderStuck ? DISPLAY_TIMECODE_STUCK : DISPLAY_TIMECODE);
                    RecorderData * recorderData = new RecorderData(event.GetTrackStatusList()[0].timecode); //must be deleted by event handler
                    frameEvent.SetClientData(recorderData);
                    AddPendingEvent(frameEvent);
                }
            }
            //track status
            mTree->SetTrackStatus(event.GetName(), dynamic_cast<IngexguiFrame*>(GetParent())->IsRecording(),
             CHUNK_WAIT == mMode //doesn't matter if recorder is not recording during CHUNK_WAIT
              || IngexguiFrame::RUNNING_UP == dynamic_cast<IngexguiFrame*>(GetParent())->GetStatus() //recorders can be in either state during running up
              || IngexguiFrame::RUNNING_DOWN == dynamic_cast<IngexguiFrame*>(GetParent())->GetStatus(), //recorders can be in either state during running down
             event.GetTrackStatusList()); //will set record button and status indicator
        }
        else { //no status
            if (event.GetName() == mTimecodeRecorder) { //just lost timecode from the recorder we're using
                SetTimecodeRecorder();
                wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, DISPLAY_TIMECODE_MISSING);
                AddPendingEvent(frameEvent);
            }
        }
    }
}

/// @param value Preroll for all recorders (expected to be <= GetMaxPreroll()).
void RecorderGroupCtrl::SetPreroll(const ProdAuto::MxfDuration value)
{
    mPreroll = value;
}

/// @param value Postroll for all recorders (expected to be <= GetMaxPostroll()).
void RecorderGroupCtrl::SetPostroll(const ProdAuto::MxfDuration value)
{
    mPostroll = value;
}

/// @return Current value of preroll for all recorders.
const ProdAuto::MxfDuration RecorderGroupCtrl::GetPreroll()
{
    return mPreroll;
}

/// @return Current value of postroll for all recorders.
const ProdAuto::MxfDuration RecorderGroupCtrl::GetPostroll()
{
    return mPostroll;
}

/// @return Value limited by least capable recorder.
const ProdAuto::MxfDuration RecorderGroupCtrl::GetMaxPreroll()
{
    return mMaxPreroll;
}

/// @return Value limited by least capable recorder.
const ProdAuto::MxfDuration RecorderGroupCtrl::GetMaxPostroll()
{
    return mMaxPostroll;
}

/// Issue a Set Tape IDs command to the given recorder.
/// @param recorderName The recorder in question.
/// @param packageNames Packages (with or without tape IDs).
/// @param tapeIds Tape IDs corresponding to packageNames.
void RecorderGroupCtrl::SetTapeIds(const wxString & recorderName, const CORBA::StringSeq & packageNames, const CORBA::StringSeq & tapeIds)
{
    int index = FindItem(0, recorderName);
    if (-1 != index && GetItemData(index)) ((Controller *) GetItemData(index))->SetTapeIds(packageNames, tapeIds);
}

/// Starts a new chunk after a previous one has finished.
/// Must be called late enough that the recorders are not asked to start recording in the future.
/// @param event Contains pointer to trigger timecode as client data, which must be deleted.
void RecorderGroupCtrl::OnTimeposEvent(wxCommandEvent & event)
{
    if (CHUNK_WAIT == mMode) {
        mMode = CHUNK_RECORD_WAIT; //to prevent Record being called more than once
        Record(mChunkStartTimecode); //timecode is the frame after the recordings stopped
    }
    delete (ProdAuto::MxfTimecode *) event.GetClientData();
}

/// Initiate a recording by sending an event for each recorder asking for its enable states.
/// @param startTimecode First frame in recording after preroll period (which depends on whether chunking is happening or not).
void RecorderGroupCtrl::Record(const ProdAuto::MxfTimecode startTimecode)
{
    if (CHUNK_RECORD_WAIT != mMode) mMode = RECORD_WAIT; //so we know what to do when Enables() is called
    mEnabledForInput = false; //don't want recorders being removed/added while recording
    //tell all relevant recorders to record
    for (int i = 0; i < GetItemCount(); i++) {
        CORBA::BooleanSeq enableList;
        Controller * controller;
        if ((controller = (Controller *) GetItemData(i)) && controller->IsOK()) {
            SetRecTimeAvailable(i, 0, true); //static figure will be misleading so hide it
#ifdef ALLOW_OVERLAPPED_RECORDINGS
            if (mTree->GetRecordEnables(GetName(i), enableList, true)) { //something's enabled for recording
#else
            if (mTree->GetRecordEnables(GetName(i), enableList, CHUNK_RECORD_WAIT == mMode)) { //something's enabled for recording, and not already recording (unless third argument is non-zero)
#endif
               ProdAuto::MxfDuration preroll = mPreroll;
               if (CHUNK_RECORD_WAIT == mMode) preroll.samples = 0; //want to record exactly at the previously given timecode, and we should be later than this so can do it
               controller->Record(startTimecode, preroll, GetCurrentProjectName(), enableList);
               mTree->SetRecorderStateUnknown(GetName(i), wxT("Awaiting response..."));
            }
        }
    }
}

/// Stop all recording recorders and optionally initiate a chunking operation.
/// @param chunk True to initiate a new chunk
/// @param stopTimecode First frame in recording of postroll period.
/// @param description Recording description.
/// @param locators Locator (cue point) information.
void RecorderGroupCtrl::Stop(const bool chunk, const ProdAuto::MxfTimecode & stopTimecode, const wxString & description, const ProdAuto::LocatorSeq & locators)
{
    mMode = chunk ? CHUNK_STOP_WAIT : STOP_WAIT; //to know what to do when Enables() is called as the result of the events about to be sent
    if (!chunk) mEnabledForInput = true; //do it here for safety (in case we get no response)
    mCurrentDescription = description;
    mHaveNonRouterRecorders = false; //will be set if we find any
    //tell all relevant recorders to stop
    for (int i = 0; i < GetItemCount(); i++) {
        CORBA::BooleanSeq enableList; //dummy
        Controller * controller;
        if (
         (controller = (Controller *) GetItemData(i)) && controller->IsOK()
         && mTree->GetRecordEnables(GetName(i), enableList, true)) { //ignore current recording state
            controller->Stop(stopTimecode, chunk ? GetChunkingPostroll() : mPostroll, mCurrentDescription, locators); //stop with predefined preroll if chunking
            mTree->SetRecorderStateUnknown(GetName(i), wxT("Awaiting response..."));
            mHaveNonRouterRecorders |= !controller->IsRouterRecorder();
        }
    }
}

/// Returns the duration of the postroll used when chunking
const ProdAuto::MxfDuration RecorderGroupCtrl::GetChunkingPostroll()
{
    ProdAuto::MxfDuration postroll = mMaxPostroll;
    postroll.samples = mMaxPostroll.edit_rate.numerator / mMaxPostroll.edit_rate.denominator / 2; //make stop time half a second or so in the future so that all the recorders get the stop command before this, as they cannot remove material already recorded
    if (postroll.samples > mMaxPostroll.samples) {
        postroll.samples = mMaxPostroll.samples;
    }
    return postroll;
}

/// Set the recorder being used for displayed timecode (may be nothing) and issue an event to this effect.
/// @param name The recorder to use.
void RecorderGroupCtrl::SetTimecodeRecorder(wxString name)
{
    mTimecodeRecorder = name;
    wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, DISPLAY_TIMECODE_SOURCE);
    frameEvent.SetString(mTimecodeRecorder);
    AddPendingEvent(frameEvent);
}

/// Set the project name sent at each recording.
void RecorderGroupCtrl::SetCurrentProjectName(const wxString & name)
{
    if (mSavedState) mSavedState->SetStringValue(wxT("CurrentProject"), name); //sanity check
}

/// Get the project name sent at each recording.
const wxString RecorderGroupCtrl::GetCurrentProjectName()
{
    wxString currentProjectName = wxEmptyString;
    if (mSavedState) currentProjectName = mSavedState->GetStringValue(wxT("CurrentProject"), wxEmptyString); //sanity check
    return currentProjectName;
}

/// Get the description sent at the end of each recording.
const wxString & RecorderGroupCtrl::GetCurrentDescription()
{
    return mCurrentDescription;
}

/// Ask timecode recorder for a list of project names.
/// @return True if successful request, i.e. an event will be sent with the project names.
bool RecorderGroupCtrl::RequestProjectNames()
{
    bool rc = false;
    int index = FindItem(0, mTimecodeRecorder);
    if (-1 != index && GetItemData(index)) rc = ((Controller *) GetItemData(index))->RequestProjectNames(); //sanity checks
    return rc;
}

/// Send the supplied list of project names to the supplied recorder, or the timecode recorder, if not empty.
/// @param controller If supplied, use this instead of the timecode recorder.
void RecorderGroupCtrl::AddProjectNames(const CORBA::StringSeq & names, Controller * controller)
{
    if (names.length()) {
        if (!controller) {
            int index = FindItem(0, mTimecodeRecorder);
            if (-1 != index && GetItemData(index)) controller = (Controller *) GetItemData(index); //sanity checks
        }
        if (controller) controller->AddProjectNames(names);
    }
}
