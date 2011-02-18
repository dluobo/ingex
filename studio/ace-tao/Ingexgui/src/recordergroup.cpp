/***************************************************************************
 *   $Id: recordergroup.cpp,v 1.23 2011/02/18 16:31:15 john_f Exp $       *
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

#include "recordergroup.h"
#include "controller.h"
#include "comms.h"
#include "timepos.h"
#include "ticktree.h"
#include "ingexgui.h" //for consts
#include "savedstate.h"

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
RecorderGroupCtrl::RecorderGroupCtrl(wxWindow * parent, wxWindowID id, const wxPoint & pos, const wxSize & size, int& argc, char** argv) : wxListView(parent, id, pos, size, wxLC_REPORT | wxLC_NO_HEADER), mEnabledForInput(true),  mMaxPreroll(InvalidMxfDuration), mMaxPostroll(InvalidMxfDuration), mPreroll(InvalidMxfDuration), mMode(STOPPED)
{
    InsertColumn(0, wxEmptyString);
    InsertColumn(1, wxEmptyString);
    mComms = new Comms(this, argc, argv);
}

RecorderGroupCtrl::~RecorderGroupCtrl()
{
    delete mComms;
}

/// Starts the process of obtaining a list of recorders from the name server, in another thread.
/// Stops clicks on the control to prevent recorders being selected which might disappear.
void RecorderGroupCtrl::StartGettingRecorders()
{
    if (mComms->GetStatus()) { //started properly
        if (0 == GetItemCount()) {
            //show that something's happening
            InsertItem(0, wxT("Getting list...")); //this will be removed when the list has been obtained
            SetItemBackgroundColour(0, DESELECTED_BACKGROUND_COLOUR); //just in case GetNumberRecordersConnected() is called
            SetItemTextColour(0, DESELECTED_TEXT_COLOUR);
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
        if (0 == GetItemData(i)) {
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
            for (size_t j = 0; j < names.GetCount(); j++) {
                int k = 0;
                do {
                    if (k == GetItemCount() || GetItemText(k).Cmp(names[j]) > 0) { //Item text should always contain the recorder name in this situation; if reached end of list, or gone past this recorder, so recorder not listed
                        //add to list
                        InsertItem(k, names[j]);
                        SetItemTextColour(k, DESELECTED_TEXT_COLOUR);
                        SetItemBackgroundColour(k, DESELECTED_BACKGROUND_COLOUR); //just in case GetNumberRecordersConnected() is called
                        SetColumnWidth(0, wxLIST_AUTOSIZE);
                        break;
                    }
                    if (GetName(k) == names[j]) { //recorder already listed
                        break;
                    }
                    k++;
                } while (true);
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
/// @return The recorder name; empty if no recorder at given position
const wxString RecorderGroupCtrl::GetName(int index)
{
    wxString name;
    if (GetItemData(index)) name = ((Controller *) GetItemData(index))->GetName();
    return name;
}

/// Starts the process of connecting to a recorder, in another thread, creating a controller.
/// @param index The position in the list of the recorder to connect to.
void RecorderGroupCtrl::BeginConnecting(const int index)
{
    SetItemPtrData(index, (wxUIntPtr) new Controller(GetItemText(index), mComms, this));
    SetItemText(index, wxT("Connecting..."));
    SetColumnWidth(0, wxLIST_AUTOSIZE);
}

/// Returns the number of connected recorders
unsigned int RecorderGroupCtrl::GetNumberRecordersConnected()
{
    unsigned int nConnected = 0;
    for (int i = 0; i < GetItemCount(); i++) {
        if (GetItemBackgroundColour(i) != DESELECTED_BACKGROUND_COLOUR) nConnected++;
    }
    return nConnected;
}

/// Sets the label for the time available for recording.
/// @param index The position in the list of the recorder.
/// @param mins The number of minutes available; if -1, clears the message.
void RecorderGroupCtrl::SetRecTimeAvailable(const int index, const long mins)
{
    wxListItem item;
    item.SetId(index);
    item.SetColumn(1);
    item.SetText(-1 == mins ? wxT("") : wxString::Format(wxT("%d min"), mins));
    SetItem(item);
    SetColumnWidth(1, wxLIST_AUTOSIZE);
}

/// Deselects the given recorder and disconnects from it.
/// Informs the frame of the disconnection.
/// Re-calculates preroll and postroll limits.
/// Looks for another recorder to get timecode from if this was the one being used.
/// @param index The position in the list of the recorder to disconnect from.
void RecorderGroupCtrl::Deselect(const int index)
{
    SetItemBackgroundColour(index, DESELECTED_BACKGROUND_COLOUR);
    SetItemTextColour(index, DESELECTED_TEXT_COLOUR);
    SetRecTimeAvailable(index, -1);
    Disconnect(index);
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
        if (i != index && GetItemData(i) && ((Controller *) GetItemData(i))->IsOK()) {
            mMaxPreroll = ((Controller *) GetItemData(i))->GetMaxPreroll();
            mMaxPostroll = ((Controller *) GetItemData(i))->GetMaxPostroll();
            break;
        }
    }
    //update with the rest of the recorders
    for (; i < GetItemCount(); i++) {
        if (i != index && GetItemData(i) && ((Controller *) GetItemData(i))->IsOK()) {
            mMaxPreroll.samples = ((Controller *) GetItemData(i))->GetMaxPreroll().samples < mMaxPreroll.samples ? ((Controller *) GetItemData(i))->GetMaxPreroll().samples : mMaxPreroll.samples;
            mMaxPostroll.samples = ((Controller *) GetItemData(i))->GetMaxPostroll().samples < mMaxPostroll.samples ? ((Controller *) GetItemData(i))->GetMaxPostroll().samples : mMaxPostroll.samples;
        }
    }
}

/// Disconnects from a recorder, by telling its controller object to get ready for deletion.
/// Looks for another recorder to get timecode from if this was the one being used.
/// @param index The position in the list of the recorder to disconnect from.
void RecorderGroupCtrl::Disconnect(int index)
{
    if (GetName(index) == mTimecodeRecorder) { //removing the recorder we're using to display timecode
        //start looking for a new one
        SetTimecodeRecorder();
    }
    if (GetItemData(index)) ((Controller *) GetItemData(index))->Destroy();
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
    if (mEnabledForInput && wxNOT_FOUND != (clickedItem = HitTest(event.GetPosition(), flags, 0))) {
        if (GetItemData(clickedItem)) { //disconnect from a recorder
            wxMessageDialog dlg(this, wxT("Are you sure you want to disconnect from ") + GetName(clickedItem) + wxT("?"), wxT("Confirmation of Disconnect"), wxYES_NO | wxICON_QUESTION);
            if (wxID_YES == dlg.ShowModal()) {
                Deselect(clickedItem);
            }
        }
        else if (!GetItemData(clickedItem)) { //connect to a recorder
            BeginConnecting(clickedItem);
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
    if (mEnabledForInput && wxNOT_FOUND != (clickedItem = HitTest(event.GetPosition(), flags, 0)) && GetItemData(clickedItem)) {
        Deselect(clickedItem);
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
        if (event.GetName() == GetName(pos)) {
            break;
        }
    }
    Controller * controller = 0;
    if (pos < GetItemCount()) controller = (Controller *) GetItemData(pos); //controller must be present if name found in list, as names are guaranteed to be unique and not blank
    //deal with connection and disconnection
    if (Controller::DIE == event.GetCommand()) {
        if (controller) { //sanity check 
            delete controller;
            SetItemData(pos, 0);
            SetItemText(pos, event.GetName()); //could be displaying connecting message
            SetColumnWidth(0, wxLIST_AUTOSIZE);
        }
    }
    else if (Controller::CONNECT == event.GetCommand()) {
        //complete connection process if successful, or reset and report failure
        if (controller) { //sanity check
            if (Controller::SUCCESS == event.GetResult()) {
                Controller * controller = (Controller *) GetItemData(pos);
                //check edit rate compatibility
                if (0 == GetNumberRecordersConnected() || (controller->GetMaxPreroll().edit_rate.numerator == mMaxPreroll.edit_rate.numerator && controller->GetMaxPreroll().edit_rate.denominator == mMaxPreroll.edit_rate.denominator)) { //the only connected recorder, or compatible edit rate (assume MaxPostroll has same edit rate as MaxPreroll)
                    //connect
                    SetItemBackgroundColour(pos, SELECTED_BACKGROUND_COLOUR); //indicates connected to user and program - can't use select capability of list, as the colour can't be changed
                    SetItemTextColour(pos, SELECTED_TEXT_COLOUR);
                    wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, NEW_RECORDER);
                    SetRecTimeAvailable(pos, event.GetRecordTimeAvailable());
                    //populate the source tree
                    frameEvent.SetExtraLong(mTree->AddRecorder(event.GetName(), event.GetTrackList(), event.GetTrackStatusList(), controller->IsRouterRecorder()));
                    //tell the frame
                    frameEvent.SetString(event.GetName());
                    frameEvent.SetInt(pos); //to allow quick disconnection
                    AddPendingEvent(frameEvent);
                    //preroll and postroll
                    if (1 == GetNumberRecordersConnected()) { //the only connected recorder
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
                    //synchronise project names
                    CORBA::StringSeq_var strings = event.GetStrings();
                    wxSortedArrayString namesToSend = mProjectNames;
                    for (size_t i = 0; i < strings->length(); i++) {
                        wxString projectName = wxString((*strings)[i], *wxConvCurrent);
                        int index = namesToSend.Index(projectName);
                        if (wxNOT_FOUND == index) { //new name for the controller
                            mProjectNames.Add(projectName);
                        }
                        else { //not a new name for the recorder
                            namesToSend.RemoveAt(index);
                        }
                    }
                    if (namesToSend.GetCount()) {
                        CORBA::StringSeq CorbaNames;
                        CorbaNames.length(namesToSend.GetCount());
                        // Assignment to the CORBA::StringSeq element must be from a const char *
                        // and should use ISO Latin-1 character set.
                        for (size_t i = 0; i < namesToSend.GetCount(); i++) {
                            CorbaNames[i] = (const char *) namesToSend[i].mb_str(wxConvISO8859_1);
                        }
                        controller->AddProjectNames(CorbaNames);
                    }
                }
                else { //edit rate incompatibility
                    wxMessageDialog dlg(this, wxT("Recorder \"") + event.GetName() + wxString::Format(wxT("\" has an edit rate incompatible with the existing recorder%s.  Deselecting "), GetNumberRecordersConnected() == 2 ? wxEmptyString : wxT("s")) + event.GetName() + wxString::Format(wxT(".\n\nEdit rate numerator: %d ("), controller->GetMaxPreroll().edit_rate.numerator) + event.GetName() + wxString::Format(wxT("); %d (existing)\nEdit rate denominator: %d ("), mMaxPreroll.edit_rate.numerator, controller->GetMaxPreroll().edit_rate.denominator) + event.GetName() + wxString::Format(wxT("); %d (existing)"), mMaxPreroll.edit_rate.denominator), wxT("Edit rate incompatibility"), wxICON_EXCLAMATION | wxOK);
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
    if (GetItemBackgroundColour(pos) != DESELECTED_BACKGROUND_COLOUR) { //not been disconnected earlier in this function due to a reconnect failure, or has been destroyed (which can still result in an event being sent); guarantees there's a controller
        if (Controller::COMM_FAILURE == event.GetResult()) {
            wxCommandEvent frameEvent(EVT_RECORDERGROUP_MESSAGE, COMM_FAILURE);
            frameEvent.SetString(event.GetName());
            AddPendingEvent(frameEvent);
        }
        else {
            switch (event.GetCommand()) {
                case Controller::RECONNECT :
                    if (Controller::FAILURE == event.GetResult()) {
                        Deselect(pos); //informs frame
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
        if (
         (controller = (Controller *) GetItemData(i)) && controller->IsOK()
#ifdef ALLOW_OVERLAPPED_RECORDINGS
         && mTree->GetRecordEnables(GetName(i), enableList, true)) { //something's enabled for recording
#else
         && mTree->GetRecordEnables(GetName(i), enableList, CHUNK_RECORD_WAIT == mMode)) { //something's enabled for recording, and not already recording (unless third argument is non-zero)
#endif
               ProdAuto::MxfDuration preroll = mPreroll;
               if (CHUNK_RECORD_WAIT == mMode) preroll.samples = 0; //want to record exactly at the previously given timecode, and we should be later than this so can do it
               controller->Record(startTimecode, preroll, mCurrentProject, enableList);
               mTree->SetRecorderStateUnknown(GetName(i), wxT("Awaiting response..."));
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

/// Replace the project names list, sending any new names to the recorders.
/// @param names The project names.
void RecorderGroupCtrl::SetProjectNames(const wxSortedArrayString & names)
{
    CORBA::StringSeq CorbaNames;
    for (size_t i = 0; i < names.GetCount(); i++) {
        if (wxNOT_FOUND == mProjectNames.Index(names[i])) { //a new name
            CorbaNames.length(CorbaNames.length() + 1);
            // Assignment to the CORBA::StringSeq element must be from a const char *
            // and should use ISO Latin-1 character set.
            CorbaNames[CorbaNames.length() - 1] = (const char *) names[i].mb_str(wxConvISO8859_1);
        }
    }

    //tell all the recorders we're connected to
    for (int i = 0; i < GetItemCount(); i++) { //the recorders in the list
        Controller * controller;
        if ((controller = (Controller *) GetItemData(i)) && controller->IsOK()) {
            controller->AddProjectNames(CorbaNames);
        }
    }

    //update the list
    mProjectNames = names;
}

/// Get the array of project names.
/// @return The project names (sorted).
const wxSortedArrayString & RecorderGroupCtrl::GetProjectNames()
{
    return mProjectNames;
}

/// Set the project name sent at each recording.  If it is not in the list of project names, add it
void RecorderGroupCtrl::SetCurrentProjectName(const wxString & name)
{
    mCurrentProject = name;
    if (wxNOT_FOUND == mProjectNames.Index(mCurrentProject)) {
        mProjectNames.Add(mCurrentProject);
    }
}

/// Get the project name sent at each recording.
const wxString & RecorderGroupCtrl::GetCurrentProjectName()
{
    return mCurrentProject;
}

/// Get the description sent at the end of each recording.
const wxString & RecorderGroupCtrl::GetCurrentDescription()
{
    return mCurrentDescription;
}
