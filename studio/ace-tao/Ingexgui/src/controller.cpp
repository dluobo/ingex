/***************************************************************************
 *   Copyright (C) 2006 by Matthew Marks   *
 *   matthew@pcx208   *
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

#include "controller.h"
#include "comms.h"
//#include "source.h"
//#include "controllergroup.h"

DEFINE_EVENT_TYPE (wxEVT_CONTROLLER_THREAD);
IMPLEMENT_DYNAMIC_CLASS(wxControllerThreadEvent, wxEvent)

BEGIN_EVENT_TABLE( Controller, wxEvtHandler )
	EVT_CONTROLLER_THREAD(Controller::OnThreadEvent)
//	EVT_CHECKBOX( -1, Controller::OnCheckBox )
	EVT_TIMER( -1, Controller::OnTimer )
END_EVENT_TABLE()

/// Starts the thread, and resolves and connects to a recorder.
/// Sends a wxEVT_CONTROLLER_THREAD FAILURE event if unsuccessful
/// @param name The name of the recorder.
/// @param comms The comms object, to allow the recorder object to be resolved.
/// @param handler The handler where events will be sent.
Controller::Controller(const wxString & name, Comms * comms, wxEvtHandler * handler)
: wxThread(wxTHREAD_JOINABLE), mWatchdogTimer(NULL), mCommand(NONE), mName(name), mComms(comms) //joinable means we can wait until the thread terminates, and this object doesn't delete itself when that happens
{
	mCondition = new wxCondition(mMutex);
	SetNextHandler(handler); //allow thread wxControllerThreadEvents to propagate to the frame
	wxString msg;
	if (wxTHREAD_NO_ERROR != wxThread::Create()) {
		msg = wxT("Could not create thread for recorder.");
		Delete();
	}
	else if (wxTHREAD_NO_ERROR != wxThread::Run()) {
		msg = wxT("Could not run thread for recorder.");
		Delete();
	}
	else { //thread is starting
		mMutex.Lock(); //wait for thread to be ready to accept signals
		mMutex.Unlock();
		Signal(CONNECT);
	}
	if (!msg.IsEmpty()) {
		//whine
		wxControllerThreadEvent event(wxEVT_CONTROLLER_THREAD);
		event.SetName(mName);
		event.SetCommand(CONNECT);
		event.SetResult(FAILURE);
		event.SetMessage(msg);
		handler->AddPendingEvent(event);
	}
}

/// Shuts down thread (if running) to avoid memory leaks.
Controller::~Controller()
{
	Signal(DIE);
	if (IsAlive()) { //won't be if initialisation failed
		Wait();
	}
	delete mCondition;
	if (NULL != mWatchdogTimer) {
		delete mWatchdogTimer;
	}
}

/// Returns status.
/// @return True if OK.
bool Controller::IsOK()
{
	return IsAlive();
}

/// @return The maximum preroll the recorder allows.
const ProdAuto::MxfDuration Controller::GetMaxPreroll()
{
	return mMaxPreroll;
}

/// @return The maximum postroll the recorder allows.
const ProdAuto::MxfDuration Controller::GetMaxPostroll()
{
	return mMaxPostroll;
}


/// Called by the watchdog timer.
void Controller::OnTimer(wxTimerEvent& WXUNUSED(event))
{
	//timer is one-shot so has stopped
	Signal(STATUS); //re-starts timer once recorder responds or times out
}

/// Signals to the thread to tell the recorder to start recording.
/// @param startTimecode The first frame to be recorded after preroll.
/// @param preroll The preroll to be applied to the recording.
/// @param enableList True for each source that should record (in order of source index).
/// @param tag The label to be applied to the recording.
/// @param tapeIds The tape ID of each video source (in order of source index), whether enabled for recording or not.
void Controller::Record(const ProdAuto::MxfTimecode & startTimecode, const ProdAuto::MxfDuration & preroll, const CORBA::BooleanSeq & enableList, const wxString & tag, const CORBA::StringSeq & tapeIds)
{
	mMutex.Lock();
	mStartTimecode = startTimecode;
	mPreroll = preroll;
	mEnableList = enableList;
	mTag = tag;
	mTapeIds = tapeIds;
	mMutex.Unlock();
	Signal(RECORD);
}

/// Signals to the thread to tell the recorder to stop recording.
/// @param stopTimecode The first frame of the postroll period.
/// @param postroll The postroll to be applied to the recording.
void Controller::Stop(const ProdAuto::MxfTimecode & stopTimecode, const ProdAuto::MxfDuration & postroll)
{
	mMutex.Lock();
	mStopTimecode = stopTimecode;
	mPostroll = postroll;
	mMutex.Unlock();
	Signal(STOP);
}

/// Kicks the thread into life.
/// @param command The command to execute.
void Controller::Signal(Command command)
{
	wxMutexLocker lock(mMutex); //prevent concurrent access to mCommand
	mCommand = command; //this will ensure the thread runs again even if it's currently busy (so not listening for a signal)
	mCondition->Signal(); //this will set the thread going if it's currently waiting
}

/// Respond to a thread event.
/// Triggers the watchdog timer and passes the event on.
/// @param event The thread event.
void Controller::OnThreadEvent(wxControllerThreadEvent & event)
{
	if (CONNECT != event.GetCommand() || SUCCESS == event.GetResult()) { //this is not a report of failure to connect
		//Send the event to the parent
		if (NULL == mWatchdogTimer) {
			//first time
			mWatchdogTimer = new wxTimer(this);
			mWatchdogTimer->Start(WATCHDOG_TIMER_SHORT_INTERVAL, wxTIMER_ONE_SHOT); //set as a one-shot so that CORBA response delays are accounted for; initial interval is short so that stuck timecode is quickly detected
			//Set sources' status
		}
		else if (WATCHDOG_TIMER_SHORT_INTERVAL == mWatchdogTimer->GetInterval()) {
			mWatchdogTimer->Start(WATCHDOG_TIMER_LONG_INTERVAL, wxTIMER_ONE_SHOT);
		}
		else {
			mWatchdogTimer->Start(WATCHDOG_TIMER_SHORT_INTERVAL, wxTIMER_ONE_SHOT);
		}
	}
	GetNextHandler()->AddPendingEvent(event); //don't use Skip() because the next event handler can delete this controller, leading to a hang
}

/// Thread entry point, called by wxWidgets.
/// @return Always 0.
wxThread::ExitCode Controller::Entry()
{
	//assumes mutex already locked here, so can signal that thread has started by unlocking it when Wait() is called
	while (1) {
		if (NONE == mCommand) { //not been told to do anything new during previous interation
			mCondition->Wait(); //unlock mutex and wait for a signal, then relock
		}
		//Now we're going
		wxControllerThreadEvent event(wxEVT_CONTROLLER_THREAD);
		event.SetName(mName);
		event.SetCommand(mCommand);
		mCommand = NONE; //can now detect if another command is issued while busy
		mMutex.Unlock();
		if (DIE == event.GetCommand()) {
			break;
		}
		//do the work
		ProdAuto::Recorder::ReturnCode rc;
		wxString msg;
		CORBA::StringSeq_var files; //deletes itself
		ProdAuto::MxfTimecode timecode; //structure so deletes itself
		for (int i = 0; i < 2; i++) { //always good to try things twice with CORBA
			try {
				switch (event.GetCommand())
				{
					case CONNECT:
						mMutex.Lock(); //this will hold up any calls to the controller, but there shouldn't be any until we've initialised anyway
						msg = mComms->SelectRecorder(mName, mRecorder);
						if (msg.IsEmpty()) {
							try {
								mTrackList = mRecorder->Tracks();
								mMaxPreroll = mRecorder->MaxPreRoll();
								mMaxPostroll = mRecorder->MaxPostRoll();
								event.SetTrackStatusList(mRecorder->TracksStatus());
								event.SetMessage(wxString(mRecorder->RecordingFormat(), *wxConvCurrent));
							}
							catch (const CORBA::Exception & e) {
								msg = wxT("Error communicating with this recorder: ") + wxString(e._name(), *wxConvCurrent) + wxT(".  Is it operational and connected to the network?  The problem may resolve itself if you try again.");
							}
						}
						if (msg.IsEmpty()) {
							//various checks on the data we've acquired
							if (!mTrackList->length() || !event.GetNTracks()) {
								msg = wxT("This recorder has no tracks.");
							}
							else if (mMaxPreroll.undefined) {
								msg = wxT("This recorder has no maximum preroll.");
							}
							else if (!mMaxPreroll.edit_rate.numerator || !mMaxPreroll.edit_rate.denominator) {
								msg = wxT("This recorder has invalid maximum preroll."); //don't want divide by zero errors
							}
							else if (mMaxPostroll.undefined) { //quite likely as postroll isn't dependent on buffer length
								mMaxPostroll = mMaxPreroll; //for the edit rate values
								mMaxPostroll.undefined = false;
								mMaxPostroll.samples = DEFAULT_MAX_POSTROLL;
							}
							else if (!mMaxPostroll.edit_rate.numerator || !mMaxPostroll.edit_rate.denominator) {
								msg = wxT("This recorder has invalid maximum postroll."); //don't want divide by zero errors
							}
						}
						mMutex.Unlock();
						if (msg.IsEmpty()) {
							mSourceList.length(mTrackList->length());
							//check timecode
							ProdAuto::TrackStatus status = event.GetTrackStatusList()[0]; //already checked there are some tracks
							if (status.timecode.undefined) {
								msg = wxT("This recorder has no timecode.");
							}
							else if (!status.timecode.edit_rate.numerator || !status.timecode.edit_rate.denominator) { //invalid/no timecode
								msg = wxT("This recorder has invalid timecode.");
							}
							else {
								mLastTimecodeReceived = status.timecode;
								event.SetTimecodeRunning(true); //as far as we know at the moment!
								event.SetTimecodeStateChanged(true);
								mTimecodeRunning = true;
							}
						}
						rc = msg.IsEmpty() ? ProdAuto::Recorder::SUCCESS : ProdAuto::Recorder::FAILURE;
						break;
					case RECORD: {
						mMutex.Lock();
						timecode = mStartTimecode;
						ProdAuto::MxfDuration preroll = mPreroll;
						char * tag = CORBA::string_dup(mTag.mb_str(*wxConvCurrent));
						CORBA::BooleanSeq rec_enable = mEnableList;
						CORBA::StringSeq tapeIds = mTapeIds;
						mMutex.Unlock();
						rc = mRecorder->Start(timecode, preroll, rec_enable, tag, tapeIds);
						break;
					}
					case STOP: {
						mMutex.Lock();
						timecode = mStopTimecode;
						ProdAuto::MxfDuration postroll = mPostroll;
						mMutex.Unlock();
						rc = mRecorder->Stop(timecode, postroll, files.out());
						break;
					}
					default:
						break;
				}
				if (ProdAuto::Recorder::SUCCESS == rc) {
					event.SetResult(SUCCESS);
					event.SetFileList(files); //only relevant for STOP
					event.SetTimecode(timecode); //only relevant for START and STOP
				}
				else {
					event.SetResult(FAILURE);
					event.SetMessage(msg);
				}
				i++; //successful or no call made: no need to try again
			}
			catch (const CORBA::Exception &) {
				event.SetResult(COMM_FAILURE);
			}
		}
		event.SetTrackList(mTrackList);
		if (SUCCESS == event.GetResult() && CONNECT != event.GetCommand()) {
			for (int j = 0; j < 2; j++) { //always good to try things twice with CORBA
				try {
					event.SetTrackStatusList(mRecorder->TracksStatus());
					//stuck timecode?
					ProdAuto::TrackStatus status = event.GetTrackStatusList()[0];
					if (status.timecode.samples != mLastTimecodeReceived.samples && !mTimecodeRunning) { //just started
						mTimecodeRunning = true;
						event.SetTimecodeStateChanged(true);
					}
					else if (status.timecode.samples == mLastTimecodeReceived.samples && mTimecodeRunning) { //just stopped
						mTimecodeRunning = false;
						event.SetTimecodeStateChanged(true);
					}
					event.SetTimecodeRunning(mTimecodeRunning);
					mLastTimecodeReceived = status.timecode;
					break; //successful: no need to try again
				}
				catch (const CORBA::Exception &) {
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
