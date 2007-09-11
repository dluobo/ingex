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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <wx/wx.h>
#include "RecorderC.h"

#define DEFAULT_MAX_POSTROLL 250 //frames
//Watchdog timer needs to check twice in quick succession to detect stuck timecode immediately
#define WATCHDOG_TIMER_LONG_INTERVAL 10000 //ms
#define WATCHDOG_TIMER_SHORT_INTERVAL 250 //ms


class Comms;
class wxControllerThreadEvent;

/// Handles communication with a recorder in a threaded manner.
class Controller : public wxEvtHandler, wxThread //each controller handles its own messages
{

public:
	Controller(const wxString &, Comms *, wxEvtHandler *);
	~Controller();
//	ProdAuto::TrackStatusList * Create(wxString, Comms *, wxEvtHandler *); //not 0 indicates success, in which case pointer deletion required
	bool IsOK();
//	void RecEnableChange(); this is needed if there is a reason for sources to handle these events
//	const bool SomeEnabled();
	void Record(const ProdAuto::MxfTimecode &, const ProdAuto::MxfDuration &, const CORBA::BooleanSeq &, const wxString &, const CORBA::StringSeq &);
	void Stop(const ProdAuto::MxfTimecode &, const ProdAuto::MxfDuration &);
//	const bool IsRecording();
//	const bool SetRolls(const bool = false);
//	void AddPackageName(wxString);
//	void NewPackageName(wxString);
//	void PackageNameSelected(wxString);
	const ProdAuto::MxfDuration GetMaxPreroll();
	const ProdAuto::MxfDuration GetMaxPostroll();
	void EnableTrack(unsigned int, bool = true);

	enum Command
	{
		NONE,
		CONNECT,
		STATUS,
		RECORD,
		STOP,
		DIE,
	};

	enum Result
	{
		SUCCESS,
		FAILURE,
		COMM_FAILURE,
	};

private:
	ExitCode Entry();
	void Signal(Command);
	void OnThreadEvent(wxControllerThreadEvent &);
//	void SetRoll(const wxChar*, ProdAuto::MxfDuration&, const long, const unsigned int, wxString&, const bool);
//	void SetRoll(const wchar_t*, ProdAuto::MxfDuration&, const long, const unsigned int, wxString&, //const bool); works only in unicode
//	void OnCheckBox(wxCommandEvent& WXUNUSED(event));
	void OnTimer(wxTimerEvent& WXUNUSED(event));
//	void SetSources();

//	wxStaticBoxSizer * mControllerSizer;
//	wxFlexGridSizer * mSourceWidgetsSizer; //One sizer for all the sources' individual widgets allows them all to be aligned
	ProdAuto::Recorder_var mRecorder;
//	ingexguiFrame * mFrame;
//	wxStaticText * mStatusBox;
//	ControllerGroup * mControllerGroup;
	wxMutex mMutex;
//	long mMaxPrerollSamples, mMaxPostrollSamples;
//	ArrayOfSources mSourceArray;
	wxTimer * mWatchdogTimer;
	//vbls accessed in both contexts while thread is running
	wxCondition* mCondition;
	Command mCommand;
	CORBA::BooleanSeq mEnableList;
	CORBA::StringSeq mTapeIds;
	wxString mName;
	Comms * mComms;
	wxString mTag;
	ProdAuto::MxfDuration mPreroll, mPostroll;
	ProdAuto::MxfDuration mMaxPreroll, mMaxPostroll;
	ProdAuto::SourceList mSourceList;
	ProdAuto::TrackList_var mTrackList;
	ProdAuto::MxfTimecode mLastTimecodeReceived;
	ProdAuto::MxfTimecode mStartTimecode;
	ProdAuto::MxfTimecode mStopTimecode;
	bool mTimecodeRunning;
	DECLARE_EVENT_TABLE()
};

/// Event to accommodate the output from a controller thread.
/// The method is from http://www.xmule.ws/phpnuke/modules.php?name=News&file=article&sid=76 - without the typedef, and making the last cast
/// in the macro wxNotifyEventFunction (i.e. not following the instructions completely), it works with gcc but doesn't compile under Win32.
/// Members are all very simple.
typedef void (wxEvtHandler::*wxControllerThreadEventFunction)(wxControllerThreadEvent &);
DECLARE_EVENT_TYPE(wxEVT_CONTROLLER_THREAD, -1)

#define EVT_CONTROLLER_THREAD(fn) \
	DECLARE_EVENT_TABLE_ENTRY( wxEVT_CONTROLLER_THREAD, wxID_ANY, wxID_ANY, \
	(wxObjectEventFunction) (wxEventFunction) (wxControllerThreadEventFunction) &fn, \
	(wxObject*) NULL \
	),

//Custom event to carry a TrackStatusList etc.  Each instance must only be cloned once!
class wxControllerThreadEvent : public wxNotifyEvent //Needs to be a Notify type event
{
public:
	wxControllerThreadEvent() : mTrackStatusList(0), mTimecodeRunning(false), mTimecodeStateChanged(false) {};
	wxControllerThreadEvent(const wxEventType & type) : wxNotifyEvent(type), mTrackStatusList(0) {};
	~wxControllerThreadEvent() {};
// 	void Destroy() { if (mTrackStatusList) {delete mTrackStatusList; mTrackStatusList = 0;} };
	virtual wxEvent * Clone() const { return new wxControllerThreadEvent(*this); }; //Called when posted to the event queue - must have exactly this prototype or base class one will be called which won't copy extra stuff
	const CORBA::ULong GetNTracks() { if (mTrackStatusList.operator->()) return mTrackStatusList->length(); else return 0; };
	void SetName(const wxString & name) { mName = name; };
	void SetMessage(const wxString & msg) { mMessage = msg; };
	void SetTrackList(ProdAuto::TrackList_var trackList) { mTrackList = trackList; };
	void SetTrackStatusList(ProdAuto::TrackStatusList_var trackStatusList) { mTrackStatusList = trackStatusList; };
	void SetCommand(Controller::Command command) { mCommand = command; };
	void SetResult(Controller::Result result) { mResult = result; };
	void SetFileList(CORBA::StringSeq_var files) { mFiles = files; };
	void SetTimecode(ProdAuto::MxfTimecode_var timecode) { mTimecode = timecode; };
	void SetTimecodeRunning(bool running) { mTimecodeRunning = running; };
	void SetTimecodeStateChanged(bool changed) { mTimecodeStateChanged = changed; };
	const wxString GetName() { return mName; };
	const wxString GetMessage() { return mMessage; };
	ProdAuto::TrackList_var GetTrackList() {return mTrackList; };
	ProdAuto::TrackStatusList_var GetTrackStatusList() {return mTrackStatusList; };
	const Controller::Command GetCommand() { return mCommand; };
	const Controller::Result GetResult() { return mResult; };
	const CORBA::StringSeq_var GetFileList() {return mFiles; };
	const ProdAuto::MxfTimecode GetTimecode() {return mTimecode; };
	bool GetTimecodeRunning() { return mTimecodeRunning; };
	bool GetTimecodeStateChanged() { return mTimecodeStateChanged; };
	DECLARE_DYNAMIC_CLASS(wxControllerThreadEvent)
private:
	wxString mName;
	wxString mMessage;
	ProdAuto::TrackList_var mTrackList; //deletes itself
	ProdAuto::TrackStatusList_var mTrackStatusList; //deletes itself
	Controller::Command mCommand;
	Controller::Result mResult;
	CORBA::StringSeq_var mFiles; //deletes itself
	ProdAuto::MxfTimecode mTimecode; //deletes itself
	bool mTimecodeRunning;
	bool mTimecodeStateChanged;
};
#endif
