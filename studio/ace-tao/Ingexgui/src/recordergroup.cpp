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

#include "recordergroup.h"
#include "controller.h"
#include "comms.h"
#include "ingexgui.h" //for consts

DEFINE_EVENT_TYPE(wxEVT_RECORDERGROUP_MESSAGE)

BEGIN_EVENT_TABLE( RecorderGroupCtrl, wxListBox )
	EVT_LEFT_DOWN(RecorderGroupCtrl::OnLMouseDown)
	EVT_MIDDLE_DOWN(RecorderGroupCtrl::OnUnwantedMouseDown)
	EVT_RIGHT_DOWN(RecorderGroupCtrl::OnUnwantedMouseDown)
	EVT_COMMAND(GOT_RECORDERS, wxEVT_RECORDERGROUP_MESSAGE, RecorderGroupCtrl::OnListRefreshed)
	EVT_CONTROLLER_THREAD(RecorderGroupCtrl::OnControllerEvent)
END_EVENT_TABLE()

/// @param parent The parent window.
/// @param id The control's window ID.
/// @param pos The control's position.
/// @param size The control's size.
/// @param argc Argument count, for ORB initialisation.
/// @param argv Argument vector, for ORB initialisation.
/// Initialises list with no recorders and creates Comms object for communication with the name server.
RecorderGroupCtrl::RecorderGroupCtrl(wxWindow * parent, wxWindowID id, const wxPoint & pos, const wxSize & size, int argc, wxChar** argv) : wxListBox(parent, id, pos, size, 0, 0, wxLB_MULTIPLE), mEnabledForInput(true)
{
	mComms = new Comms(this, argc, argv);
	SetNextHandler(parent);
	mPreroll.undefined = true;
}

RecorderGroupCtrl::~RecorderGroupCtrl()
{
	delete mComms;
}

/// Starts the process of obtaining a list of recorders from the name server, in another thread.
/// Stops clicks on the control to prevent recorders being selected which might disappear.
void RecorderGroupCtrl::StartGettingRecorders() {
	if (!GetCount()) { //empty list
		//show that something's happening
		Insert(wxT("Getting list..."), 0); //this will be removed when the list has been obtained
	}
	mEnabledForInput = false;
	wxCommandEvent event(wxEVT_RECORDERGROUP_MESSAGE, GETTING_RECORDERS);
	AddPendingEvent(event);
	mComms->StartGettingRecorders(wxEVT_RECORDERGROUP_MESSAGE, GOT_RECORDERS); //threaded, so won't hang app
}

/// Responds to Comms finishing obtaining a list of recorders.
/// Updates the list, removing recorders which weren't found and which aren't connected, and adding new recorders.
/// @param event The command event.
void RecorderGroupCtrl::OnListRefreshed(wxCommandEvent & event)
{
	mEnabledForInput = true;
	//Remove all the recorders which aren't connected/connecting (including the dummy message if present)
	for (unsigned int i = 0; i < GetCount(); i++) {
		if (!GetController(i)) {
			Delete(i--);
		}
	}
	wxString errMsg;
	if (mComms->GetStatus(errMsg)) { //OK
		wxArrayString names;
		mComms->GetRecorderList(names);
		//Put in any recorders not listed, in alphabetical order
		for (size_t j = 0; j < names.GetCount(); j++) {
			unsigned int k = 0;
			do {
				if (k == GetCount() || GetName(k).Cmp(names[j]) > 0) { //reached end of list, or gone past this recorder, so recorder not listed
					//add to list
					Insert(names[j], k);
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
		wxMessageBox(wxT("Failed to get list of recorders:\n") + errMsg, wxT("Comms problem"), wxICON_ERROR);
	}
	//Tell the parent to re-enable the select button
	event.Skip();
}

/// Inserts an item in the list.
/// Creates a corresponding ControllerContainer (but not a controller).
/// @param name The recorder name.
/// @param pos The position to insert the recorder.
void RecorderGroupCtrl::Insert(const wxString & name, unsigned int pos)
{
	//At least one item must have client data or GetClientData will fail, so give the item client data even if there is no controller (disconnected)
	ControllerContainer * container = new ControllerContainer(name); //deleted when list item deleted
	wxListBox::Insert(name, pos, container);
}

/// Gets the name of the recorder at the given position.
/// @param index The position of the recorder name to return.
/// @return The recorder name.
const wxString RecorderGroupCtrl::GetName(unsigned int index)
{
	return ((ControllerContainer *) GetClientObject(index))->GetName();
}

/// Starts the process of connecting to a recorder, in another thread, creating a controller.
/// @param index The position in the list of the recorder to connect to.
void RecorderGroupCtrl::Connect(unsigned int index)
{
	((ControllerContainer *) GetClientObject(index))->Connect(mComms, this);
}

/// Disconnects from a recorder, by deleting its controller object.
/// Looks for another recorder to get timecode from if this was the one being used.
/// @param index The position in the list of the recorder to disconnect from.
void RecorderGroupCtrl::Disconnect(unsigned int index)
{
	if (GetString(index) == mTimecodeRecorder) { //removing the recorder we're using to display timecode
		//start looking for a new one
		SetTimecodeRecorder();
	}
	((ControllerContainer *) GetClientObject(index))->Disconnect();
}

/// Gets the controller of the given recorder - 0 if it doesn't exist.
/// @param index The position in the list of the wanted recorder.
Controller * RecorderGroupCtrl::GetController(unsigned int index)
{
	return ((ControllerContainer *) GetClientObject(index))->GetController();
}

/// Responds to a left mouse click on the control to select or deselect a recorder.
/// Have to intercept the mouse click event in order to allow an unmodified click to deselect the recorder.
/// If selecting, start the connection process in another thread and put a holding message in the list.
/// If deselecting, send an event to the parent and adjust maximum preroll and postroll values to the constraints imposed by the remaining recorders.
/// @param event The mouse event.
void RecorderGroupCtrl::OnLMouseDown(wxMouseEvent & event)
{
	int clickedItem;
	if (mEnabledForInput && wxNOT_FOUND != (clickedItem = HitTest(event.GetPosition()))) {
		if (IsSelected(clickedItem)) { //disconnect from a recorder
			Deselect(clickedItem);
			Disconnect(clickedItem);
			//depopulate the source tree
			wxCommandEvent event(wxEVT_RECORDERGROUP_MESSAGE, REMOVE_RECORDER);
			event.SetString(GetString(clickedItem));
			AddPendingEvent(event);
			//preroll and postroll limits might have been relaxed
			unsigned int i;
			for (i = 0; i < GetCount(); i++) {
				if (GetController(i) && GetController(i)->IsOK()) {
					mMaxPreroll = GetController(i)->GetMaxPreroll();
					mMaxPostroll = GetController(i)->GetMaxPostroll();
					break;
				}
			}
			for (; i < GetCount(); i++) {
				if (GetController(i) && GetController(i)->IsOK()) {
					mMaxPreroll.samples = GetController(i)->GetMaxPreroll().samples < mMaxPreroll.samples ? GetController(i)->GetMaxPreroll().samples : mMaxPreroll.samples;
					mMaxPostroll.samples = GetController(i)->GetMaxPostroll().samples < mMaxPostroll.samples ? GetController(i)->GetMaxPostroll().samples : mMaxPostroll.samples;
				}
			}
		}
		else if (!GetController(clickedItem)) { //connect to a recorder
			SetString(clickedItem, wxT("Connecting..."));
			Connect(clickedItem);
		}
	}
}

/// Responds to unwanted mouse clicks on the control by preventing them doing anything
/// @param event The mouse event.
void RecorderGroupCtrl::OnUnwantedMouseDown(wxMouseEvent & event)
{
}

/// Responds to an event from one of the recorder controllers.
/// If just connected to a recorder, do a sanity check on the recorder parameters, send an event to the parent and adjust max preroll and postroll to accommodate this recorder's values.
/// If recording or stopping, send an event to the parent.
/// If status information, send events if timecode status has changed, and try to use this recorder to supply displayed timecode if there currently isn't one.
/// If this recorder is supplying displayed timecode, send an event with the timecode.
/// Send a track status event.
/// @param event The controller thread event.
void RecorderGroupCtrl::OnControllerEvent(wxControllerThreadEvent & event)
{
	switch (event.GetCommand()) {
		case Controller::CONNECT :
			unsigned int i;
			//find the entry in the list
			for (i = 0; i < GetCount(); i++) {
				if (event.GetName() == GetName(i)) {
					break;
				}
			}
			//connect if successful
			if (i < GetCount()) { //sanity check
				if (Controller::SUCCESS == event.GetResult()) {
					//check edit rate compatibility
					wxArrayInt selectedItems;
					if (!GetSelections(selectedItems) || (GetController(i)->GetMaxPreroll().edit_rate.numerator == mMaxPreroll.edit_rate.numerator && GetController(i)->GetMaxPreroll().edit_rate.denominator == mMaxPreroll.edit_rate.denominator)) { //the only recorder, or compatible edit rate (assume MaxPostroll has same edit rate)
						//everything about the recorder is now checked
						Select(i);
						//populate the source tree
						wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, NEW_RECORDER);
						frameEvent.SetString(event.GetName());
						RecorderData * recorderData = new RecorderData(event.GetTrackStatusList(), event.GetTrackList()); //must be deleted by event handler
						frameEvent.SetClientData(recorderData);
						frameEvent.SetInt(wxString(event.GetMessage()).MakeUpper().Matches(wxT("*ROUTER*"))); //non-zero if a router recorder
						AddPendingEvent(frameEvent);
						//preroll and postroll
						if (!selectedItems.GetCount()) { //the only recorder
							mMaxPreroll = GetController(i)->GetMaxPreroll();
							mPreroll = mMaxPreroll; //for the edit rate values
							mPreroll.samples = DEFAULT_PREROLL;
							mMaxPostroll = GetController(i)->GetMaxPostroll();
							mPostroll = mMaxPostroll; //for the edit rate values
							mPostroll.samples = DEFAULT_POSTROLL;
						}
						if (GetController(i)->GetMaxPreroll().samples < mMaxPreroll.samples) {
							//limit mMaxPreroll (and possibly mPreroll) to new maximum
							mMaxPreroll.samples = GetController(i)->GetMaxPreroll().samples;
							mPreroll.samples = mPreroll.samples > GetController(i)->GetMaxPreroll().samples ? GetController(i)->GetMaxPreroll().samples : mPreroll.samples;
						}
						if (GetController(i)->GetMaxPostroll().samples < mMaxPostroll.samples) {
							//limit mMaxPostroll (and possibly mPostroll) to new maximum
							mMaxPostroll.samples = GetController(i)->GetMaxPostroll().samples;
							mPostroll.samples = mPostroll.samples > GetController(i)->GetMaxPostroll().samples ? GetController(i)->GetMaxPostroll().samples : mPostroll.samples;
						}
					}
					else { //edit rate incompatibility
						wxMessageBox(wxT("Recorder \"") + event.GetName() + wxString::Format(wxT("\" has an edit rate incompatible with the existing recorder%s.  Deselecting "), selectedItems.GetCount() == 1 ? wxT("") : wxT("s")) + event.GetName() + wxString::Format(wxT(".\n\nEdit rate numerator: %d ("), GetController(i)->GetMaxPreroll().edit_rate.numerator) + event.GetName() + wxString::Format(wxT("); %d (existing)\nEdit rate denominator: %d ("), mMaxPreroll.edit_rate.numerator, GetController(i)->GetMaxPreroll().edit_rate.denominator) + event.GetName() + wxString::Format(wxT("); %d (existing)"), mMaxPreroll.edit_rate.denominator), wxT("Edit rate incompatibility"), wxICON_EXCLAMATION);
						Disconnect(i);
					}
				}
				else {
					Disconnect(i);
					wxMessageBox(wxT("Couldn't connect to recorder \"") + event.GetName() + wxT("\": ") + event.GetMessage(), wxT("Initialisation failure"), wxICON_EXCLAMATION);
				}
				SetString(i, event.GetName());
			}
			break;
		case Controller::RECORD : {
				wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, RECORDING);
				frameEvent.SetString(event.GetName());
				frameEvent.SetInt (Controller::SUCCESS == event.GetResult());
				RecorderData * recorderData = new RecorderData(event.GetTimecode()); //must be deleted by event handler
				frameEvent.SetClientData(recorderData);
				AddPendingEvent(frameEvent);
			}
			break;
		case Controller::STOP : {
				wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, STOPPED);
				frameEvent.SetString(event.GetName());
				frameEvent.SetInt (Controller::SUCCESS == event.GetResult());
				RecorderData * recorderData = new RecorderData(event.GetTrackList(), event.GetFileList(), event.GetTimecode()); //must be deleted by event handler
				frameEvent.SetClientData(recorderData);
				AddPendingEvent(frameEvent);
			}
			break;
	}
	//timecode
	if (event.GetNTracks()) { //have status
		if (event.GetTimecodeStateChanged() || mTimecodeRecorder.IsEmpty()) {
			//timecode display
			if (event.GetTimecodeRunning() && mTimecodeRecorder.IsEmpty()) { //found a recorder to get timecode from
				SetTimecodeRecorder(event.GetName());
			}
			else if (!event.GetTimecodeRunning() && event.GetName() == mTimecodeRecorder) { //just lost running timecode from the recorder we're using
				SetTimecodeRecorder();
				ProdAuto::TrackStatus status = event.GetTrackStatusList()[0];
				wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, DISPLAY_TIMECODE_STUCK);
				RecorderData * recorderData = new RecorderData(status.timecode); //must be deleted by event handler
				frameEvent.SetClientData(recorderData);
				AddPendingEvent(frameEvent);
			}
			//recorder status
			wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, event.GetTimecodeRunning() ? TIMECODE_RUNNING : TIMECODE_STUCK);
			frameEvent.SetString(event.GetName());
			AddPendingEvent(frameEvent);
		}
		if (event.GetTimecodeRunning() && event.GetName() == mTimecodeRecorder) { //timecode for the display
			ProdAuto::TrackStatus status = event.GetTrackStatusList()[0];
			wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, DISPLAY_TIMECODE);
			RecorderData * recorderData = new RecorderData(status.timecode); //must be deleted by event handler
			frameEvent.SetClientData(recorderData);
			AddPendingEvent(frameEvent);
		}
		//track status
		wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, TRACK_STATUS);
		frameEvent.SetString(event.GetName());
		RecorderData * recorderData = new RecorderData(event.GetTrackStatusList()); //must be deleted by event handler
		frameEvent.SetClientData(recorderData);
		AddPendingEvent(frameEvent);
	}
	else { //no status
		if (event.GetName() == mTimecodeRecorder) { //just lost timecode from the recorder we're using
			SetTimecodeRecorder();
			wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, DISPLAY_TIMECODE_MISSING);
			AddPendingEvent(frameEvent);
		}
		wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, TIMECODE_MISSING);
		frameEvent.SetString(event.GetName());
		AddPendingEvent(frameEvent);
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

/// Initiate a recording by sending an event for each recorder asking for its enable states and tape IDs.
/// @param tag Project name to be used for all recorders.
/// @param startTimecode First frame in recording after preroll period
void RecorderGroupCtrl::RecordAll(const wxString tag, const ProdAuto::MxfTimecode startTimecode)
{
	mEnabledForInput = false; //don't want recorders being removed/added while recording
	mTag = tag;
	mStartTimecode = startTimecode;
	//Ask for all the enable states
	for (unsigned int i = 0; i < GetCount(); i++) {
		if (GetController(i) && GetController(i)->IsOK()) {
			wxCommandEvent event(wxEVT_RECORDERGROUP_MESSAGE, REQUEST_RECORD);
			event.SetString(GetName(i));
			AddPendingEvent(event);
		}
	}
}

/// Issue a record command and an event indicating status unknown.
/// @param recorderName The recorder in question.
/// @param enableList Record enable states.
/// @param tapeIds Tape IDs for video sources.
void RecorderGroupCtrl::Record(const wxString & recorderName, const CORBA::BooleanSeq & enableList, const CORBA::StringSeq & tapeIds)
{
	GetController(FindString(recorderName))->Record(mStartTimecode, mPreroll, enableList, mTag, tapeIds);
	wxCommandEvent event(wxEVT_RECORDERGROUP_MESSAGE, STATUS_UNKNOWN);
	event.SetString(recorderName);
	AddPendingEvent(event);
}

/// Issue a stop command to all recorders.
/// @param stopTimecode First frame in recording of postroll period
void RecorderGroupCtrl::Stop(const ProdAuto::MxfTimecode & stopTimecode)
{
	//It doesn't matter if a non-recording recorder gets a stop command
	for (unsigned int i = 0; i < GetCount(); i++) {
		if (GetController(i) && GetController(i)->IsOK()) {
			GetController(i)->Stop(stopTimecode, mPostroll);
		}
	}
	mEnabledForInput = true; //do it here for safety (in case we get no response)

}

/// Determine whether there are any connected recorders.
/// @return True if any connected.
bool RecorderGroupCtrl::IsEmpty()
{
	for (unsigned int i = 0; i < GetCount(); i++) {
		if (GetController(i)) {
			return false;
		}
	}
	return true;
}

/// Set the recorder being used for displayed timecode and issue an event to this effect.
/// @param name The recorder to use.
void RecorderGroupCtrl::SetTimecodeRecorder(wxString name)
{
	mTimecodeRecorder = name;
	wxCommandEvent frameEvent(wxEVT_RECORDERGROUP_MESSAGE, DISPLAY_TIMECODE_SOURCE);
	frameEvent.SetString(mTimecodeRecorder);
	AddPendingEvent(frameEvent);
}

//MAKE SURE ALL FUNCTIONS CHECK FOR EXISTENCE OF CONTROLLER FIRST - IF THERE ARE ANY THAT TAKE A CONTROLLER NAME ARG
