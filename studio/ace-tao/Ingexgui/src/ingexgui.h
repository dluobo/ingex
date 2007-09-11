
#ifndef _INGEXGUI_H_
#define _INGEXGUI_H_
#include <vector>
#include "comms.h"
#include "controller.h"
//#include <wx/spinctrl.h>
//#include "source.h"
#include <wx/listctrl.h> //for Win32
#include <wx/datetime.h> //for Win32
#include <wx/xml/xml.h>
//#include "ticktree.h"
#include <wx/notebook.h> //for wx 2.7+

#define DEFAULT_PREROLL 10 //frames
#define DEFAULT_POSTROLL 10 //frames
//#define REVIEW_THRESHOLD 5 //seconds

#define CONTROL_BORDER 5 //pixels to space controls from edge, each other, etc
#define EVENT_COL_WIDTH 100
#define EVENT_LIST_HEIGHT 150
#define NOTEBOOK_HEIGHT 300
#define SOURCE_TREE_HEIGHT 150
#define TIME_FONT_SIZE 20 //point
#define EVENT_FONT_SIZE 10 //point - if >10 then text is cut off in the list ctrl

#define TOOLTIP_DELAY 1000 //ms

#define UNKNOWN_TIMECODE wxT("??:??:??:??")

#define SAVED_STATE_FILENAME wxT(".ingexguirc")

//class Source;
//class ControllerGroup;
//class ingexguiFrame;
class TickTreeCtrl;
class DragButtonList;

class 
ingexguiapp : public wxApp
{
	public:
		virtual bool OnInit();
		int FilterEvent(wxEvent&);
};

WX_DECLARE_OBJARRAY(ProdAuto::TrackList_var, ArrayOfTrackList_var);
WX_DECLARE_OBJARRAY(CORBA::StringSeq_var, ArrayOfStringSeq_var);

/// Class holding information about a take, for later replay.
/// Holds the position of the take within the displayed list of events, the project name (recording tag), an array of lists of video files
/// (one for each recorder) and an array of lists of track names (ditto).
class TakeInfo
{
	public:
		TakeInfo(unsigned long startIndex, const wxString tag) : mStartIndex(startIndex), mTag(tag) {};
//		void SetFiles(ProdAuto::StringList_var files) {mFiles = files;};
		void AddRecorder(ProdAuto::TrackList_var trackList, CORBA::StringSeq_var fileList) {mFiles.Add(fileList); mTracks.Add(trackList);};
		const unsigned long GetStartIndex() {return mStartIndex;};
//		ProdAuto::StringList_var GetFiles() {return mFiles;};
		ArrayOfStringSeq_var * GetFiles() {return &mFiles;};
		const wxString GetTag() {return mTag;};
//		ProdAuto::TrackList_var GetTrackList() {return mTrackList;};
		ArrayOfTrackList_var * GetTracks() {return &mTracks;};
		void AddCuePoint(int64_t cuePoint) {mCuePoints.push_back(cuePoint);};
		std::vector<int64_t> * GetCuePoints() {return &mCuePoints;};
	private:
		const unsigned long mStartIndex; //the index in the event list for the start of this take
		ArrayOfStringSeq_var mFiles;
//		ProdAuto::StringList_var mFiles; //deletes itself
		const wxString mTag;
		ArrayOfTrackList_var mTracks; //deletes itself
//		ProdAuto::TrackList_var mTrackList; //deletes itself
		std::vector<int64_t> mCuePoints;

};

WX_DECLARE_OBJARRAY(TakeInfo, TakeInfoArray);

class Timepos;
namespace prodauto {
	class Player;
}
class RecordButton;
class HelpDlg;
class RecorderGroupCtrl;

/// The main displayed frame, at the heart of the application
class ingexguiFrame : public wxFrame
{
	public:
		ingexguiFrame(int, wxChar**);
//		~ingexguiFrame();
//		void RecordEnable(const bool enable);
//		const unsigned int GetPreroll();
//		const unsigned int GetPostroll();
//		const wxString GetDir();
//		const bool IsRecording();

	enum
	{
		MENU_About = wxID_HIGHEST + 1,
		MENU_Help,
		MENU_SetRolls,
		MENU_SetProjectName,
		MENU_Record,
		MENU_MarkCue,
		MENU_Stop,
		MENU_PrevSource,
		MENU_NextSource,
		MENU_3,
		MENU_4,
		MENU_Up,
		MENU_Down,
		MENU_PageUp,
		MENU_PageDown,
		MENU_Home,
		MENU_End,
		MENU_J,
		MENU_K,
		MENU_L,
		MENU_AutoClear,
		MENU_ClearLog,
		MENU_DisablePlayer,
		MENU_PlayerType,
		MENU_ExtOutput,
		MENU_AccelOutput,
		MENU_ExtAccelOutput,
		MENU_ExtUnaccelOutput,
		MENU_UnaccelOutput,
		MENU_PlayerSize,
		MENU_NormalDisplay,
		MENU_SmallerDisplay,
		MENU_SmallestDisplay,
		MENU_PlayerOSD,
		MENU_AbsoluteTimecode,
		MENU_RelativeTimecode,
		MENU_NoOSD,
		BUTTON_Record,
		BUTTON_Stop,
		BUTTON_Cue,
		TIMER_Refresh,
		BUTTON_RecorderListRefresh,
		BUTTON_TapeId,
		CHOICE_RecorderSelector,
		CHECKLIST_Sources,
		BUTTON_PrevTake,
		BUTTON_NextTake,
		TREE
	};
	private:
	enum EventType
	{
		START,
		CUE,
//		REVIEW,
		STOP,
		PROBLEM
	};
	enum Stat
	{
		STOPPED,
		RECORDING,
		PLAYING,
		PAUSED,
		PLAYING_BACKWARDS,
		UNKNOWN,
	};
		void OnClose(wxCloseEvent &);
		void OnHelp(wxCommandEvent&);
		void OnAbout(wxCommandEvent&);
		void OnSetRolls(wxCommandEvent&);
		void OnSetProjectName(wxCommandEvent&);
		void OnRecord(wxCommandEvent&);
		void OnRecorderListRefresh(wxCommandEvent&);
		void OnSetTapeIds(wxCommandEvent&);
		void OnStop(wxCommandEvent&);
		void OnCue(wxCommandEvent&);
		void OnClearLog(wxCommandEvent&);
		void OnPlayerOSDTypeChange(wxCommandEvent&);
		void OnPlayerOutputTypeChange(wxCommandEvent&);
		void OnQuit(wxCommandEvent&);
		void OnEventSelection(wxListEvent&);
		void OnEventActivated(wxListEvent&);
		void OnRefreshTimer(wxTimerEvent&);
		void OnPrevTake(wxCommandEvent&);
		void OnNextTake(wxCommandEvent&);
		void OnPlayerEvent(wxCommandEvent&);
		void OnRecorderGroupEvent(wxCommandEvent&);
		void OnTreeEvent(wxCommandEvent& event);
		void OnPlaybackSourceSelect(wxCommandEvent&);
		void OnShortcut(wxCommandEvent &);
		void OnDisablePlayer(wxCommandEvent &);
		void OnNotebookPageChange(wxNotebookEvent &);

		void UpdatePlayerAndEventControls(bool = false);
		void AddEvent(EventType);
		void SetStatus(Stat);
		ProdAuto::MxfDuration SetRoll(const wxChar *, int, const ProdAuto::MxfDuration &, wxStaticBoxSizer *);
		bool AtStopEvent();
		bool AtStartEvent();
		void ClearLog();
		void Connect(bool);
		void SelectAdjacentEvent(bool down);
		void ResetPlayer();

		wxStaticBitmap * mStatusCtrl, * mAlertCtrl;
		RecorderGroupCtrl * mRecorderGroup;
		wxButton * mRecorderListRefreshButton;
		wxButton * mTapeIdButton;
		wxButton * mStopButton, * mCueButton;
		RecordButton * mRecordButton;
		wxListView * mEventList; //used wxListCtrl for a while because wxListView crashed when you added an item - seems ok with 2.8
		wxNotebook * mNotebook;
		TickTreeCtrl * mTree;
		wxStaticText * mPlaybackTagCtrl;
		wxStaticText * mRecordTagCtrl;
		wxButton * mPrevTakeButton, * mNextTakeButton;
		DragButtonList * mPlaybackSourceSelector;
		HelpDlg * mHelpDlg;
		wxStaticBoxSizer * mTimecodeBox;

		bool mConnected;
		Stat mStatus;
		long mStartEvent, mEndEvent;
		long mEditRateNumerator, mEditRateDenominator;
		TakeInfoArray mTakeInfo;
		wxString mProjectName;
		Timepos * mTimepos;
		TakeInfo * mCurrentTakeInfo;
		unsigned long mCurrentSelectedEvent;
		prodauto::Player * mPlayer;
		wxXmlDocument mSavedState;

		DECLARE_EVENT_TABLE()

};

#endif // _INGEXGUI_H_
