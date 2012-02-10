/***************************************************************************
 *   $Id: controller.cpp,v 1.21 2012/02/10 15:12:55 john_f Exp $          *
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

#define __STDC_LIMIT_MACROS
#include <stdint.h>

#include "controller.h"
#include "comms.h"
#include "timepos.h"
#include "ingexgui.h"

DEFINE_EVENT_TYPE (EVT_CONTROLLER_THREAD);
IMPLEMENT_DYNAMIC_CLASS(ControllerThreadEvent, wxEvent)

BEGIN_EVENT_TABLE( Controller, wxEvtHandler )
    EVT_CONTROLLER_THREAD(Controller::OnThreadEvent)
    EVT_TIMER( wxID_ANY, Controller::OnTimer )
END_EVENT_TABLE()

/// Starts the thread, and resolves and connects to a recorder.
/// Sends an EVT_CONTROLLER_THREAD FAILURE event if unsuccessful
/// @param name The name of the recorder.
/// @param comms The comms object, to allow the recorder object to be resolved.
/// @param handler The handler where events will be sent.
Controller::Controller(const wxString & name, Comms * comms, wxEvtHandler * handler)
: wxThread(wxTHREAD_JOINABLE), mComms(comms), mTimecodeRunning(false), mReconnecting(false), mPendingCommand(NONE), mPendingCommandSent(false), mName(name), mPrevCommand(NONE), mOK(false) //making the thread joinable means that we can start the deletion process without waiting for it to end, which might take a long time if there is a CORBA command waiting for a response.  Instead, we issue an event to signal when the thread has exited and hence the object can be deleted.
{
    mCondition = new wxCondition(mMutex);
    SetNextHandler(handler); //allow thread ControllerThreadEvents to propagate to the frame
    wxString msg;
    mPollingTimer = new wxTimer(this);
    mMutex.Lock(); //to allow a test for the thread being ready, when it unlocks the mutex with Wait();
    if (!mCondition->IsOk()) {
        msg = wxT("Could not create condition object for recorder.");
    }
    else if (wxTHREAD_NO_ERROR != wxThread::Create()) {
        msg = wxT("Could not create thread for recorder.");
    }
    else if (wxTHREAD_NO_ERROR != wxThread::Run()) {
        msg = wxT("Could not run thread for recorder.");
    }
    else { //thread is starting
        mOK = true;
        mMutex.Lock(); //wait, if necessary, for thread to be ready to accept signals (which is indicated by it unlocking the mutex, locked before the thread started)
        mMutex.Unlock(); //undo the above to return to normal
        Signal(CONNECT);
    }
    if (!msg.IsEmpty()) {
        //whine
        ControllerThreadEvent event(EVT_CONTROLLER_THREAD);
        event.SetName(mName);
        event.SetCommand(CONNECT);
        event.SetResult(FAILURE);
        event.SetMessage(msg);
        handler->AddPendingEvent(event);
    }
}

Controller::~Controller()
{
    wxDELETE(mPollingTimer); //may not exist
    delete mCondition;
}

/// Returns status.
/// @return True if OK.
bool Controller::IsOK()
{
    return mOK;
}

/// @return The recorder name.
const wxString Controller::GetName()
{
    wxMutexLocker lock(mMutex); //prevent concurrent access
    return mName;
}

/// @return The maximum preroll the recorder allows.
const ProdAuto::MxfDuration Controller::GetMaxPreroll()
{
    wxMutexLocker lock(mMutex); //prevent concurrent access
    return mMaxPreroll;
}

/// @return The maximum postroll the recorder allows.
const ProdAuto::MxfDuration Controller::GetMaxPostroll()
{
    wxMutexLocker lock(mMutex); //prevent concurrent access
    return mMaxPostroll;
}

/// Signals to the thread to tell the recorder the tape IDs, or stores the command for later execution.
/// @param sourceNames The names of the video sources.
/// @param tapeIDs The Tape ID of each source, as identified in sourceNames.
void Controller::SetTapeIds(const CORBA::StringSeq & sourceNames, const CORBA::StringSeq & tapeIds)
{
    //Set up the parameters
    mMutex.Lock();
    mSourceNames = sourceNames;
    mTapeIds = tapeIds;
    mMutex.Unlock();
    if (!mReconnecting) { //can send a command now
        Signal(SET_TAPE_IDS);
    }
    //SET_TAPE_IDs is always sent after reconnection
}

/// Signals to the thread to tell the recorder a list of project names, or stores the command for later execution if it can, or does nothing.
/// @param name The project name.
void Controller::AddProjectNames(const CORBA::StringSeq & projectNames)
{
    mMutex.Lock();
    mProjectNames = projectNames;
    mMutex.Unlock();
    if (NONE == mPendingCommand) { //nothing more important is happening - adding a project name is not a vital thing to do
        mPendingCommand = ADD_PROJECT_NAMES; //act on it later or detect if it is superceded while retrying
    }
    if (!mReconnecting) { //can send a command now
        Signal(ADD_PROJECT_NAMES);
        mPendingCommandSent = true;
    }
}

/// Signals to the thread to ask the recorder to return how much record time is available, stores the command for later execution if it can, or does nothing.
/// Does nothing if this is a router recorder.
void Controller::RequestRecordTimeAvailable()
{
    if (!mRouterRecorder) {
        if (NONE == mPendingCommand) { //nothing more important is happening - requesting record time is not a vital thing to do
            mPendingCommand = REC_TIME_AVAILABLE; //act on it later or detect if it is superceded while retrying
        }
        if (!mReconnecting) { //can send a command now
            Signal(REC_TIME_AVAILABLE);
            mPendingCommandSent = true;
        }
    }
}

/// Signals to the thread to ask the recorder to return a list of project names, stores the command for later execution if it can, or does nothing.
/// Does nothing if this is a router recorder.
/// @return False if unable to do anything (so an event will not be generated).
bool Controller::RequestProjectNames()
{
    bool rc = false;
    if (!mRouterRecorder) {
        if (NONE == mPendingCommand) { //nothing more important is happening - requesting project names is not a vital thing to do
            mPendingCommand = GET_PROJECT_NAMES; //act on it later or detect if it is superceded while retrying
            rc = true;
        }
        if (!mReconnecting) { //can send a command now
            Signal(GET_PROJECT_NAMES);
            mPendingCommandSent = true;
        }
    }
    return rc;
}

/// Signals to the thread to tell the recorder to start recording, or stores the command for later execution.
/// @param startTimecode The first frame to be recorded after preroll.
/// @param preroll The preroll to be applied to the recording.
/// @param project The project name to be applied to the recording.
/// @param enableList True for each source that should record (in order of source index).
void Controller::Record(const ProdAuto::MxfTimecode & startTimecode, const ProdAuto::MxfDuration & preroll, const wxString & project, const CORBA::BooleanSeq & enableList)
{
    //Set up the parameters
    mMutex.Lock();
    mPreroll = preroll;
    mProject = project;
    mEnableList = enableList;
    if (mReconnecting) { //can't send a command now
//std::cerr << "Record while reconnecting: setting timecode undefined" << std::endl;
        mPendingCommand = RECORD; //act on it later or detect if it is superceded while retrying
        mStartTimecode.undefined = true; //the requested frame might not be available by the time we reconnect
        mMutex.Unlock();
    }
    else {
        mStartTimecode = startTimecode;
        mMutex.Unlock();
        Signal(RECORD);
        mPendingCommandSent = true;
    }
}

/// Signals to the thread to tell the recorder to stop recording, or stores the command for later execution.
/// @param stopTimecode The first frame of the postroll period.
/// @param postroll The postroll to be applied to the recording.
/// @param description The description to be applied to the recording.
/// @param locators Locator (timecode, description, colour) information for the recording.
void Controller::Stop(const ProdAuto::MxfTimecode & stopTimecode, const ProdAuto::MxfDuration & postroll, const wxString & description, const ProdAuto::LocatorSeq & locators)
{
    //Set up the parameters
    mMutex.Lock();
    mPostroll = postroll;
    mDescription = description;
    mLocators = locators; 
    if (mReconnecting) { //can't send a command now
//std::cerr << "Stop while reconnecting: setting timecode undefined" << std::endl;
        mPendingCommand = STOP; //act on it later or detect if it is superceded while retrying
        mStopTimecode.undefined = true; //the requested frame might not be available by the time we reconnect
        mMutex.Unlock();
    }
    else {
        mStopTimecode = stopTimecode;
        mMutex.Unlock();
        Signal(STOP);
        mPendingCommandSent = true;
    }
}

/// @return True if this is a router recorder
bool Controller::IsRouterRecorder()
{
    return mRouterRecorder;
}

/// If thread isn't running, immediately sends an event confirming this controller can be deleted.
/// Otherwise, signals to the thread to tell it to die, which will (at its leisure) generate the above event and exit, and the event will be passed on once the thread has exited.
/// This allows the controller to be deleted without waiting for a pending CORBA command.
/// Once this function has been called, the controller should be considered unusable.
void Controller::Destroy()
{
    wxDELETE(mPollingTimer); //stops it too!
    mOK = false;
    if (IsAlive()) { //initialisation succeeded
        Signal(DIE);
    }
    else {
        //write a suicide note now
        ControllerThreadEvent event(EVT_CONTROLLER_THREAD);
        mMutex.Lock();
        event.SetName(mName);
        mMutex.Unlock();
        event.SetCommand(DIE);
        GetNextHandler()->AddPendingEvent(event);
    }
}

/// Kicks the thread into life
/// @param command The command to execute.
void Controller::Signal(Command command)
{
    if (mPollingTimer) {
        mPollingTimer->Stop(); //about to execute a command, so timer is irrelevant
    }
    wxMutexLocker lock(mMutex); //prevent concurrent access to mCommand
    mCommand = command; //this will ensure the thread runs again even if it's currently busy (so not listening for a signal)
    mCondition->Signal(); //this will set the thread going if it's currently waiting
}

/// Respond to a thread event.
/// Passes event on if appropriate and starts the timer or sends another command depending on the circumstances.
/// @param event The thread event.
void Controller::OnThreadEvent(ControllerThreadEvent & event)
{
    mReconnecting = COMM_FAILURE == event.GetResult();
    if (DIE == event.GetCommand()) { //the thread is on its last legs
        Wait(); //on the offchance the thread is still running
//std::cerr << "die notification" << std::endl;
        GetNextHandler()->AddPendingEvent(event); //NB don't use Skip() because the next event handler can delete this controller, leading to a hang
    }
    else if (mPollingTimer) { //not waiting to die
        if ((CONNECT == event.GetCommand() && SUCCESS != event.GetResult()) //failure to connect
           || (RECONNECT == event.GetCommand() && FAILURE == event.GetResult())) { //failure to reconnect
            //let the parent know - parent will delete this controller
//std::cerr << "connect/reconnect failure notification" << std::endl;
            GetNextHandler()->AddPendingEvent(event); //NB don't use Skip() because the next event handler can delete this controller, leading to a hang
        }
        else if (FAILURE == event.GetResult()) {
            if (event.GetCommand() != mPrevCommand) {
                mMutex.Lock();
                mStartTimecode.undefined = true; //in case we're recording and the start time requested is no longer in the buffer
                mStopTimecode.undefined = true; //in case we're stopping and the stop time requested is no longer in the buffer
                mMutex.Unlock();
                //let the parent know
//std::cerr << "command failure notification" << std::endl;
                GetNextHandler()->AddPendingEvent(event);
            }
            if (event.GetCommand() == mPendingCommand || NONE == mPendingCommand) { //this isn't the result of a previous command which has been superceded
                //try again
//std::cerr << "retrying failed command" << std::endl;
                mCommandOnTimer = event.GetCommand();
                mPollingTimer->Start(POLLING_INTERVAL, wxTIMER_ONE_SHOT);
            }
        }
        else if (COMM_FAILURE == event.GetResult()) {
            if (RECONNECT != event.GetCommand()) { //not already reconnecting
                //start reconnecting
//std::cerr << "reconnect starting notification: setting timecodes undefined" << std::endl;
                //if trying to record or stop, the requested frame might not be available by the time we succeed so record/stop "now"
                mMutex.Lock();
                mStartTimecode.undefined = true; //in case we're recording and the start time requested is no longer in the buffer
                mStopTimecode.undefined = true; //in case we're stopping and the stop time requested is no longer in the buffer
                mMutex.Unlock();
                //let the parent know
                GetNextHandler()->AddPendingEvent(event);
                //buffer the failed command
                if ((RECORD == event.GetCommand() || STOP == event.GetCommand() || ADD_PROJECT_NAMES == event.GetCommand() || REC_TIME_AVAILABLE == event.GetCommand() || GET_PROJECT_NAMES == event.GetCommand()) && NONE == mPendingCommand) { //make sure the event doesn't override a command received while waiting for a response
//std::cerr << "new pending command" << std::endl;
                    //execute command when reconnected
                    mPendingCommand = event.GetCommand();
                    mPendingCommandSent = false; //needs to be sent again when communication re-established
                }
            }
            mCommandOnTimer = RECONNECT;
            mPollingTimer->Start(POLLING_INTERVAL, wxTIMER_ONE_SHOT);
        }
        else if (RECONNECT == event.GetCommand()) { //just reconnected
            //set tape IDs as may be a restarted recorder which won't have this information
//std::cerr << "successful reconnect notification" << std::endl;
            GetNextHandler()->AddPendingEvent(event);
            Signal(SET_TAPE_IDS);
        }
        else if (event.GetCommand() == mPendingCommand) { //pending command successfully sent
//std::cerr << "pending command successfully sent" << std::endl;
            mPendingCommand = NONE;
            mPendingCommandSent = false; //default state
            GetNextHandler()->AddPendingEvent(event);
            mPollingTimer->Start(POLLING_INTERVAL, wxTIMER_ONE_SHOT); //back to normal
        }
        else if (NONE != mPendingCommand) { //not in idle state
            if (!mPendingCommandSent) {
                //Send the pending command again immediately, with updated parameters
//std::cerr << "pending command being sent" << std::endl;
                Signal(mPendingCommand);
                mPendingCommandSent = true;
            }
        }
        else { //idle state
//std::cerr << "ordinary notification" << std::endl;
            GetNextHandler()->AddPendingEvent(event);
            mCommandOnTimer = STATUS;
            mPollingTimer->Start(POLLING_INTERVAL, wxTIMER_ONE_SHOT);
        }
    }
    mPrevCommand = event.GetCommand();
}

/// Called by the polling timer
/// Sends the stored command.
void Controller::OnTimer(wxTimerEvent& WXUNUSED(event))
{
    //timer is one-shot so has stopped: it will be re-started if necessary when the thread returns an event in response to Signal() or the parent issues a command
    Signal(mCommandOnTimer);
}

/// Thread entry point, called by wxWidgets.
/// @return Always 0.
wxThread::ExitCode Controller::Entry()
{
    mCommand = NONE;
    //assumes mutex already locked here, so we can signal to the main thread that we are ready to accept signals by unlocking it when Wait() is called
    while (1) {
        if (NONE == mCommand) { //not been told to do anything new during previous interation
            mCondition->Wait(); //unlock mutex and wait for a signal, then relock
        }
        //Now we're going
        ControllerThreadEvent event(EVT_CONTROLLER_THREAD);
        event.SetName(mName);
        event.SetCommand(mCommand);
        mCommand = NONE; //can now detect if another command is issued while busy (more frequently during abnormal situations) 
        mMutex.Unlock();
        if (DIE == event.GetCommand()) {
            AddPendingEvent(event);
            break;
        }
        //do the work...
        ProdAuto::Recorder::ReturnCode rc = ProdAuto::Recorder::SUCCESS;
        wxString msg;
        CORBA::StringSeq_var strings; //deletes itself
        ProdAuto::MxfTimecode timecode = InvalidMxfTimecode; //structure so deletes itself
        //send a recorder command, if any
        for (int i = 0; i < 2; i++) { //always good to try things twice with CORBA
            event.SetResult(SUCCESS); //default
            switch (event.GetCommand())
            {
                case CONNECT: case RECONNECT: {
//std::cerr << "thread CONNECT/RECONNECT" << std::endl;
                    ProdAuto::MxfDuration maxPreroll = InvalidMxfDuration; //initialisation prevents compiler warning
                    ProdAuto::MxfDuration maxPostroll = InvalidMxfDuration; //initialisation prevents compiler warning
                    ProdAuto::TrackList_var trackList;
                    bool routerRecorder = false; //initialisation prevents compiler warning
                    unsigned long recordTimeAvailable = -1;
                    event.SetTimecodeStateChanged(); //always trigger action
                    mLastTimecodeReceived.undefined = true; //guarantee that a valid timecode will be assumed to be running timecode
                    mMutex.Lock();
                    wxString name = mName;
                    mMutex.Unlock();
                    msg = mComms->SelectRecorder(name, mRecorder);
                    if (msg.IsEmpty()) { //OK so far
                        try {
                            trackList = mRecorder->Tracks();
                            maxPreroll = mRecorder->MaxPreRoll();
                            maxPostroll = mRecorder->MaxPostRoll();
                            event.SetTrackStatusList(mRecorder->TracksStatus());
                            routerRecorder = wxString(mRecorder->RecordingFormat(), wxConvLibc).MakeUpper().Matches(wxT("*ROUTER*"));
                            strings = mRecorder->ProjectNames();
                        }
                        catch (const CORBA::Exception & e) {
//std::cerr << "connect/reconnect exception: " << e._name() << std::endl;
                            msg = wxT("Error communicating with this recorder: ") + wxString(e._name(), wxConvLibc) + wxT(".  Is it operational and connected to the network?  The problem may resolve itself if you try again.");
                        }
                    }
                    if (!msg.IsEmpty()) { //failed to select
                        event.SetResult(COMM_FAILURE);
                    }
                    else { //OK so far
                        try {
                            recordTimeAvailable = mRecorder->RecordTimeAvailable();
                        }
                        catch (const CORBA::Exception & e) {
                            //do nothing - ignore old recorders that don't understand this command, and other errors are likely to have been detected by the previous calls
                        }
                        mMutex.Lock();
                        if (CONNECT == event.GetCommand()) {
                            //various checks on the data we've acquired
                            if (!trackList->length() || !event.GetNTracks()) {
                                msg = wxT("This recorder has no tracks.");
                            }
                            else if (maxPreroll.undefined) {
                                msg = wxT("This recorder has no maximum preroll.");
                            }
                            else if (!maxPreroll.edit_rate.numerator || !maxPreroll.edit_rate.denominator) {
                                msg = wxT("This recorder has invalid maximum preroll."); //don't want divide by zero errors
                            }
                            else if (maxPostroll.undefined) { //quite likely as postroll isn't dependent on buffer length
                                mMaxPostroll = maxPreroll; //for the edit rate values
                                mMaxPostroll.undefined = false;
                                mMaxPostroll.samples = DEFAULT_MAX_POSTROLL;
                            }
                            else if (!maxPostroll.edit_rate.numerator || !maxPostroll.edit_rate.denominator) {
                                msg = wxT("This recorder has invalid maximum postroll."); //don't want divide by zero errors
                            }
                            else {
                                mMaxPostroll = maxPostroll;
                            }
                            mTrackList = trackList;
                            mMaxPreroll = maxPreroll;
                            mSourceList.length(mTrackList->length());
                            mRouterRecorder = routerRecorder;
                        }
                        else { //reconnecting
                            //check nothing's changed after reconnection
                            if (maxPreroll.undefined
                             || maxPreroll.edit_rate.numerator != mMaxPreroll.edit_rate.numerator
                             || maxPreroll.edit_rate.denominator != mMaxPreroll.edit_rate.denominator
                             || maxPreroll.samples != mMaxPreroll.samples) {
                                msg = wxT("the maximum preroll has changed");
                            }
                            else if ((maxPostroll.undefined && mMaxPostroll.samples != DEFAULT_MAX_POSTROLL) ||
                             (!maxPostroll.undefined &&
                              (maxPostroll.edit_rate.numerator != mMaxPostroll.edit_rate.numerator
                              || maxPostroll.edit_rate.denominator != mMaxPostroll.edit_rate.denominator
                              || maxPostroll.samples != mMaxPostroll.samples))) {
                                msg = wxT("the maximum postroll has changed");
                            }
                            else if (trackList->length() != mTrackList->length()) {
                                msg = wxT("the number of tracks has changed");
                            }
                            else if (mRouterRecorder && !routerRecorder) {
                                msg = wxT("it is no longer a router recorder");
                            }
                            else if (!mRouterRecorder && routerRecorder) {
                                msg = wxT("it now presents itself as a router recorder");
                            }
                            else {
                                for (unsigned int i = 0; i < trackList->length(); i++) {
                                    if (strcmp(trackList[i].name, mTrackList[i].name)) {
                                        msg = wxT("name");
                                    }
                                    else if (trackList[i].type != mTrackList[i].type) {
                                        msg = wxT("type");
                                    }
                                    else if (trackList[i].id != mTrackList[i].id) {
                                        msg = wxT("ID");
                                    }
                                    else if (trackList[i].has_source != mTrackList[i].has_source) {
                                        msg = wxT("\"has_source\" status");
                                    }
                                    else if (trackList[i].has_source && strcmp(trackList[i].src.package_name, mTrackList[i].src.package_name)) {
                                        msg = wxT("package name");
                                    }
                                    else if (trackList[i].has_source && strcmp(trackList[i].src.track_name, mTrackList[i].src.track_name)) {
                                        msg = wxT("track name");
                                    }
                                    if (msg.Length()) {
                                        msg = wxT("track \"") + wxString(mTrackList[i].name, wxConvLibc) + wxT("\" has changed ") + msg;
                                        break;
                                    }
                                }
                            }
                        }
                        mMutex.Unlock();
                    }
                    if (msg.IsEmpty()) {
                        rc = ProdAuto::Recorder::SUCCESS;
                        event.SetRecordTimeAvailable(mRouterRecorder ? -1 : recordTimeAvailable);
                    }
                    else {
                        rc = ProdAuto::Recorder::FAILURE;
                        event.SetMessage(msg);
                        event.SetTrackStatusList(0); //track status not valid
                    }
                    break;
                }
                case SET_TAPE_IDS: {
//std::cerr << "thread SET_TAPE_IDS" << std::endl;
                    mMutex.Lock();
                    CORBA::StringSeq sourceNames = mSourceNames;
                    CORBA::StringSeq tapeIds = mTapeIds;
                    mMutex.Unlock();
                    try {
                        mRecorder->SetTapeNames(sourceNames, tapeIds);
                    }
                    catch (const CORBA::Exception & e) {
//std::cerr << "set tape IDs exception: " << e._name() << std::endl;
                        event.SetResult(COMM_FAILURE);
                    }
                    rc = ProdAuto::Recorder::SUCCESS; //no return code suppled by SetTapeNames()
                    break;
                }
                case ADD_PROJECT_NAMES: {
//std::cerr << "thread ADD_PROJECT_NAMES" << std::endl;
                    mMutex.Lock();
                    CORBA::StringSeq projectNames = mProjectNames;
                    mMutex.Unlock();
                    try {
                        mRecorder->AddProjectNames(projectNames);
                    }
                    catch (const CORBA::Exception & e) {
//std::cerr << "AddProjectNames exception: " << e._name() << std::endl;
                        event.SetResult(COMM_FAILURE);
                    }
                    rc = ProdAuto::Recorder::SUCCESS; //no return code suppled by AddProject()
                    break;
                }
                case REC_TIME_AVAILABLE: { //already checked that this isn't a router recorder
//std::cerr << "thread REC_TIME_AVAILABLE" << std::endl;
                    long recordTimeAvailable = -1;
                    try {
                        recordTimeAvailable = mRecorder->RecordTimeAvailable();
                    }
                    catch (const CORBA::BAD_OPERATION & e) {
                        //do nothing - ignore old recorders that don't understand this command
                    }
                    catch (const CORBA::Exception & e) {
//std::cerr << "RecordTimeAvailable exception: " << e._name() << std::endl;
                        event.SetResult(COMM_FAILURE);
                    }
                    event.SetRecordTimeAvailable(recordTimeAvailable);
                    rc = ProdAuto::Recorder::SUCCESS; //no return code available
                    break;
                }
                case GET_PROJECT_NAMES: { //assumed not to be a router recorder
//std::cerr << "thread GET_PROJECT_NAMES" << std::endl;
                    try {
                        strings = mRecorder->ProjectNames();
                    }
                    catch (const CORBA::Exception & e) {
//std::cerr << "GetProjectNames exception: " << e._name() << std::endl;
                        event.SetResult(COMM_FAILURE);
                    }
                    rc = ProdAuto::Recorder::SUCCESS; //no return code available
                    break;
                }
                case RECORD: {
//std::cerr << "thread RECORD" << std::endl;
                    wxCommandEvent loggingEvent(EVT_LOGGING_MESSAGE);
                    mMutex.Lock();
                    timecode = mStartTimecode;
                    ProdAuto::MxfDuration preroll = mPreroll;
                    CORBA::BooleanSeq rec_enable = mEnableList;
                    std::string project =  (const char *) mProject.mb_str(wxConvLibc);
                    loggingEvent.SetString(wxT("Start() being called for \"") + mName + wxT("\" @ ") + Timepos::FormatTimecode(timecode));
                    mMutex.Unlock();
                    AddPendingEvent(loggingEvent);
                    try {
                        rc = mRecorder->Start(timecode, preroll, rec_enable, project.c_str(), false);
                    }
                    catch (const CORBA::Exception & e) {
//std::cerr << "start exception: " << e._name() << std::endl;
                        event.SetResult(COMM_FAILURE);
                    }
                    break;
                }
                case STOP: {
//std::cerr << "thread STOP" << std::endl;
                    wxCommandEvent loggingEvent(EVT_LOGGING_MESSAGE);
                    mMutex.Lock();
                    timecode = mStopTimecode;
                    ProdAuto::MxfDuration postroll = mPostroll;
                    char * description = CORBA::string_dup(mDescription.mb_str(wxConvLibc));
                    ProdAuto::LocatorSeq locators = mLocators; 
                    loggingEvent.SetString(wxT("Stop() being called for \"") + mName + wxT("\" @ ") + Timepos::FormatTimecode(timecode));
                    mMutex.Unlock();
                    AddPendingEvent(loggingEvent);
                    try {
                        rc = mRecorder->Stop(timecode, postroll, description, locators, strings.out());
                    }
                    catch (const CORBA::Exception & e) {
//std::cerr << "stop exception: " << e._name() << std::endl;
                        event.SetResult(COMM_FAILURE);
                    }
                    event.SetTimecodeStateChanged(); //so that a timecode that was stuck during the recording will be reported as such
                    break;
                }
                default:
                    break;
            }
            if (COMM_FAILURE != event.GetResult()) {
                if (ProdAuto::Recorder::SUCCESS == rc) {
                    event.SetStrings(strings); //only relevant for STOP (list of files), and CONNECT or GET_PROJECT_NAMES (list of project names)
                    if (!timecode.edit_rate.numerator || !timecode.edit_rate.denominator) {
                        timecode.undefined = true;
                    }
                    event.SetTimecode(timecode); //only relevant for START and STOP
                }
                else {
                    event.SetResult(FAILURE);
                }
                break; //comms successful or no call made: no need to try again
            }
        }
        event.SetTrackList(mTrackList);
        //get status
        if (COMM_FAILURE != event.GetResult()) {
            for (int j = 0; j < 2; j++) { //always good to try things twice with CORBA
                try {
                    if (CONNECT != event.GetCommand() && RECONNECT != event.GetCommand()) { //haven't already got track status list
                        event.SetTrackStatusList(mRecorder->TracksStatus());
                    }
                    //timecode situation
                    if (!event.GetNTracks() || event.GetTrackStatusList()->operator[](0).timecode.undefined || !event.GetTrackStatusList()->operator[](0).timecode.edit_rate.numerator || !event.GetTrackStatusList()->operator[](0).timecode.edit_rate.denominator) { //no or invalid timecode
                        if (!mLastTimecodeReceived.undefined) { //newly in this state
                            mLastTimecodeReceived.undefined = true; //to allow future detection of state change
                            event.SetTimecodeStateChanged();
                            //ABSENT is the default timecode state in the event
                        }
                    }
                    else { //timecode value is OK
                        ProdAuto::MxfTimecode timecode = event.GetTrackStatusList()->operator[](0).timecode;
                        if (mLastTimecodeReceived.undefined) { //timecode has just appeared
                            event.SetTimecodeState(UNCONFIRMED);
                            if (!mTimecodeRunning) { //just started
                                mTimecodeRunning = true; //to allow future detection of state change
                                event.SetTimecodeStateChanged();
                            }
                        }
                        else if (!wxDateTime::Now().IsEqualUpTo(mLastTimecodeRequest, wxTimeSpan::Milliseconds(100))) { //long enough since the last stored value was received to assume that the timecode has changed
                            if (timecode.samples != mLastTimecodeReceived.samples) { //timecode is running
                                event.SetTimecodeState(RUNNING);
                                if (!mTimecodeRunning) { //just started
                                    mTimecodeRunning = true; //to allow future detection of state change
                                    event.SetTimecodeStateChanged();
                                }
                            }
                            else { //timecode is stuck
                                event.SetTimecodeState(STUCK);
                                if (mTimecodeRunning) { //just stopped
                                    mTimecodeRunning = false; //to allow future detection of state change
                                    event.SetTimecodeStateChanged();
                                }
                            }
                        }
                        else {
                            //repeat previous status
                            event.SetTimecodeState(mTimecodeRunning ? UNCONFIRMED : STUCK);
                        }
                        mLastTimecodeReceived = timecode;
                        mLastTimecodeRequest = wxDateTime::Now(); //now or sometime before!
                    }
                    break; //successful: no need to try again
                }
                catch (const CORBA::Exception & e) {
//std::cerr << "status exception: " << e._name() << std::endl;
                    event.SetResult(COMM_FAILURE);
                //indicated by list size being zero
                }
            }
        }
        //communicate the results
        AddPendingEvent(event);
        mMutex.Lock();
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ControllerThreadEvent::ControllerThreadEvent() : mTrackStatusList(0), mTimecodeState(Controller::ABSENT), mTimecodeStateChanged(false)
{
}

ControllerThreadEvent::ControllerThreadEvent(const wxEventType & type) : wxNotifyEvent(type), mTrackStatusList(0), mTimecodeState(Controller::ABSENT), mTimecodeStateChanged(false)
{
}

///Called when posted to the event queue - must have exactly this prototype or base class one will be called which won't copy extra stuff
wxEvent * ControllerThreadEvent::Clone() const
{
    return new ControllerThreadEvent(*this);
}

CORBA::ULong ControllerThreadEvent::GetNTracks()
{
    if (mTrackStatusList.operator->())
        return mTrackStatusList->length();
    else
        return 0;
}

void ControllerThreadEvent::SetName(const wxString & name)
{
    mName = name;
}

void ControllerThreadEvent::SetMessage(const wxString & msg)
{
    mMessage = msg;
}

void ControllerThreadEvent::SetTrackList(ProdAuto::TrackList_var trackList)
{
    mTrackList = trackList;
}

void ControllerThreadEvent::SetTrackStatusList(ProdAuto::TrackStatusList_var trackStatusList)
{
    mTrackStatusList = trackStatusList;
}

void ControllerThreadEvent::SetCommand(Controller::Command command)
{
    mCommand = command;
}

void ControllerThreadEvent::SetResult(Controller::Result result)
{
    mResult = result;
}

void ControllerThreadEvent::SetStrings(CORBA::StringSeq_var strings)
{
    mStrings = strings;
}

void ControllerThreadEvent::SetTimecode(ProdAuto::MxfTimecode_var timecode)
{
    mTimecode = timecode;
}

void ControllerThreadEvent::SetTimecodeState(Controller::TimecodeState state)
{
    mTimecodeState = state;
}

void ControllerThreadEvent::SetTimecodeStateChanged(bool changed)
{
    mTimecodeStateChanged = changed;
}

void ControllerThreadEvent::SetRecordTimeAvailable(long timeAvailable)
{
    mRecordTimeAvailable = (UINT32_MAX == timeAvailable ? -1 : timeAvailable);
}

const wxString ControllerThreadEvent::GetName()
{
    return mName;
}

const wxString ControllerThreadEvent::GetMessage()
{
    return mMessage;
}

const ProdAuto::TrackList_var & ControllerThreadEvent::GetTrackList()
{
    return mTrackList;
}

const ProdAuto::TrackStatusList_var & ControllerThreadEvent::GetTrackStatusList()
{
    return mTrackStatusList;
}

Controller::Command ControllerThreadEvent::GetCommand() {
    return mCommand;
}

Controller::Result ControllerThreadEvent::GetResult()
{
    return mResult;
}

CORBA::StringSeq_var ControllerThreadEvent::GetStrings()
{
    return mStrings;
}

const ProdAuto::MxfTimecode ControllerThreadEvent::GetTimecode()
{
    return mTimecode;
}

Controller::TimecodeState ControllerThreadEvent::GetTimecodeState()
{
    return mTimecodeState;
}

bool ControllerThreadEvent::TimecodeStateHasChanged()
{
    return mTimecodeStateChanged;
}

long ControllerThreadEvent::GetRecordTimeAvailable()
{
    return mRecordTimeAvailable;
}

