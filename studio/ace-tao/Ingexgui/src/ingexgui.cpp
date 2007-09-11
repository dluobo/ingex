#include <wx/wx.h>
#include "wx/tooltip.h" //for SetDelay()
#include "wx/splitter.h"
#include "wx/xml/xml.h"
#include "wx/file.h"
#include "help.h"
//#include "source.h"
#include "ingexgui.xpm"
//#include "dialogues.h"
#include "stop.xpm"
#include "record.xpm"
#include "play.xpm"
#include "play_backwards.xpm"
#include "paused.xpm"
#include "happy.xpm"
#include "exclamation.xpm"
#include "question.xpm"
#include "blank.xpm"

#include "ticktree.h"
#include "timepos.h"
#include "dialogues.h"
#include "dragbuttonlist.h"
#include "player.h"
#include "recordbutton.h"
#include "recordergroup.h"

//#include <wx/tooltip.h>

#include <wx/arrimpl.cpp>
WX_DEFINE_OBJARRAY(TakeInfoArray);
WX_DEFINE_OBJARRAY(ArrayOfTrackList_var);
WX_DEFINE_OBJARRAY(ArrayOfStringSeq_var);

//DEFINE_EVENT_TYPE (wxEVT_CONTROLLER_THREAD);
//IMPLEMENT_DYNAMIC_CLASS(wxControllerThreadEvent, wxEvent)


BEGIN_EVENT_TABLE( ingexguiFrame, wxFrame )
	EVT_CLOSE( ingexguiFrame::OnClose )
	EVT_MENU( MENU_Help, ingexguiFrame::OnHelp )
	EVT_MENU( MENU_About, ingexguiFrame::OnAbout )
	EVT_MENU( MENU_Record, ingexguiFrame::OnRecord )
	EVT_MENU( MENU_MarkCue, ingexguiFrame::OnCue )
	EVT_MENU( MENU_Stop, ingexguiFrame::OnStop )
	EVT_MENU( MENU_PrevSource, ingexguiFrame::OnShortcut )
	EVT_MENU( MENU_NextSource, ingexguiFrame::OnShortcut )
	EVT_MENU( MENU_3, ingexguiFrame::OnShortcut )
	EVT_MENU( MENU_4, ingexguiFrame::OnShortcut )
	EVT_MENU( MENU_Up, ingexguiFrame::OnShortcut )
	EVT_MENU( MENU_Down, ingexguiFrame::OnShortcut )
	EVT_MENU( MENU_PageUp, ingexguiFrame::OnPrevTake )
	EVT_MENU( MENU_PageDown, ingexguiFrame::OnNextTake )
	EVT_MENU( MENU_Home, ingexguiFrame::OnShortcut )
	EVT_MENU( MENU_End, ingexguiFrame::OnShortcut )
	EVT_MENU( MENU_J, ingexguiFrame::OnShortcut )
	EVT_MENU( MENU_K, ingexguiFrame::OnShortcut )
	EVT_MENU( MENU_L, ingexguiFrame::OnShortcut )
//	EVT_MENU( MENU_Space, ingexguiFrame::OnSpace ) prevents spaces being put in the recording tag!
	EVT_MENU( MENU_ClearLog, ingexguiFrame::OnClearLog )
	EVT_MENU( MENU_DisablePlayer, ingexguiFrame::OnDisablePlayer )
	EVT_MENU( MENU_AbsoluteTimecode, ingexguiFrame::OnPlayerOSDTypeChange )
	EVT_MENU( MENU_RelativeTimecode, ingexguiFrame::OnPlayerOSDTypeChange )
	EVT_MENU( MENU_NoOSD, ingexguiFrame::OnPlayerOSDTypeChange )
	EVT_MENU( MENU_ExtOutput, ingexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( MENU_AccelOutput, ingexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( MENU_ExtAccelOutput, ingexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( MENU_ExtUnaccelOutput, ingexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( MENU_UnaccelOutput, ingexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( MENU_NormalDisplay, ingexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( MENU_SmallerDisplay, ingexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( MENU_SmallestDisplay, ingexguiFrame::OnPlayerOutputTypeChange )
	EVT_MENU( wxID_CLOSE, ingexguiFrame::OnQuit )
	EVT_MENU( MENU_SetProjectName, ingexguiFrame::OnSetProjectName )
	EVT_MENU( MENU_SetRolls, ingexguiFrame::OnSetRolls )
	EVT_BUTTON( BUTTON_RecorderListRefresh, ingexguiFrame::OnRecorderListRefresh )
	EVT_BUTTON( BUTTON_TapeId, ingexguiFrame::OnSetTapeIds )
	EVT_BUTTON( BUTTON_Record, ingexguiFrame::OnRecord )
	EVT_BUTTON( BUTTON_Stop, ingexguiFrame::OnStop )
	EVT_BUTTON( BUTTON_Cue, ingexguiFrame::OnCue )
	EVT_LIST_ITEM_SELECTED( wxID_ANY, ingexguiFrame::OnEventSelection )
	EVT_LIST_ITEM_ACTIVATED( wxID_ANY, ingexguiFrame::OnEventActivated )
	EVT_BUTTON( BUTTON_PrevTake, ingexguiFrame::OnPrevTake )
	EVT_BUTTON( BUTTON_NextTake, ingexguiFrame::OnNextTake )
	EVT_COMMAND( wxID_ANY, wxEVT_PLAYER_MESSAGE, ingexguiFrame::OnPlayerEvent )
	EVT_COMMAND( wxID_ANY, wxEVT_TREE_MESSAGE, ingexguiFrame::OnTreeEvent )
	EVT_COMMAND( wxID_ANY, wxEVT_RECORDERGROUP_MESSAGE, ingexguiFrame::OnRecorderGroupEvent )
	EVT_RADIOBUTTON( wxID_ANY, ingexguiFrame::OnPlaybackSourceSelect )
	EVT_NOTEBOOK_PAGE_CHANGED( wxID_ANY, ingexguiFrame::OnNotebookPageChange )

//	EVT_NOTEBOOK_PAGE_CHANGED(wxID_ANY, ingexguiFrame::OnNotebookPageChange)
//	EVT_SPINCTRL( -1, ingexguiFrame::OnSpinCtrl )
END_EVENT_TABLE()

IMPLEMENT_APP(ingexguiapp)

/// Application initialisation: creates and displays the main frame.
/// @return Always true.
bool ingexguiapp::OnInit()
{
	ingexguiFrame *frame = new ingexguiFrame(argc, argv);

	frame->Show(true);
	SetTopWindow(frame);
	return true;
} 

//Need to do this because accelerator/menu shortcuts don't always work
//Even so, sometimes (when control that takes cursor keys doesn't have focus?), no key events are generated for cursor keys
int ingexguiapp::FilterEvent(wxEvent& event)
{
//return -1;
	bool intercepted = false;
	if (wxEVT_CHAR == event.GetEventType()) {
		wxKeyEvent * keyEvent = (wxKeyEvent *) &event;
		intercepted = true;
		wxCommandEvent guiMenuEvent(wxEVT_COMMAND_MENU_SELECTED);
		switch (keyEvent->GetKeyCode()) {
			case WXK_UP : //controls get this before menu (at least in GTK)
				guiMenuEvent.SetId(ingexguiFrame::MENU_Up);
				break;
			case WXK_DOWN : //controls get this before menu (at least in GTK)
				guiMenuEvent.SetId(ingexguiFrame::MENU_Down);
				break;
/*			case WXK_PAGEUP : //menus don't recognise this as a shortcut (at least in GTK)
				guiMenuEvent.SetId(ingexguiFrame::MENU_PageUp);
				GetTopWindow()->AddPendingEvent(guiMenuEvent);
//				guiButtonEvent.SetId(ingexguiFrame::BUTTON_PrevTake);
//				GetTopWindow()->AddPendingEvent(guiButtonEvent);
				break;
			case WXK_PAGEDOWN : //menus don't recognise this as a shortcut (at least in GTK)
				guiButtonEvent.SetId(ingexguiFrame::BUTTON_NextTake);
				GetTopWindow()->AddPendingEvent(guiButtonEvent);
				break; */

//			case ' ' :
//				break;
			default:
				intercepted = false;
		}
		if (intercepted) {
			GetTopWindow()->AddPendingEvent(guiMenuEvent);
		}
	}


	if (intercepted) {
		return true; //if false is returned, the event is processed again
	}
	else {
		return -1; //process as normal
	}
}

/// Lays out all controls and menus, and creates persistent objects.
/// @param argc Command line argument count.
/// @param argv Command line argument vector.
ingexguiFrame::ingexguiFrame(int argc, wxChar** argv)
	: wxFrame((wxFrame *)0, wxID_ANY, wxT("ingexgui")/*, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxWANTS_CHARS* - this doesn't prevent cusor keys being lost, as hoped */), mStatus(STOPPED), mEndEvent(-1), mCurrentTakeInfo(0) //, mPreroll.undefined(true), mPostroll.undefined(true)/*, mRecording(false), mRecordEnable(false)*/
{
//	SetWindowStyle(GetWindowStyle() | wxWANTS_CHARS);
	mHelpDlg = new HelpDlg(this); //modeless to allow it to remain visible while using the app
	//icon

	wxIcon icon(ingexgui_xpm);
	SetIcon(icon);
	//menu
	wxMenuBar * menuBar = new wxMenuBar;

	//Misc menu
	wxMenu * menuMisc = new wxMenu;
	menuMisc->Append(MENU_SetProjectName, wxT("Set project name..."));
	menuMisc->Append(MENU_SetRolls, wxT("Set pre- and post-roll..."));
	wxMenuItem * clearLogItem = menuMisc->Append(MENU_ClearLog, wxT("Clear recording log"));
	clearLogItem->Enable(false);
	menuMisc->AppendCheckItem(MENU_AutoClear, wxT("Don't log multiple recordings"));
	menuMisc->Append(wxID_CLOSE, wxT("Quit"));
	menuBar->Append(menuMisc, wxT("&Misc"));

	//Player menu
	wxMenu * menuPlayer = new wxMenu;
	menuPlayer->AppendCheckItem(MENU_DisablePlayer, wxT("Disable player"));
	wxMenu * menuPlayerType = new wxMenu;
	wxMenu * menuPlayerSize = new wxMenu;
	wxMenu * menuPlayerOSD = new wxMenu;
	wxMenuItem * extAccelOutput = menuPlayerType->AppendRadioItem(MENU_ExtAccelOutput, wxT("External monitor and On-screen (accelerated)")); //first item is selected by default
	wxMenuItem * extOutput = menuPlayerType->AppendRadioItem(MENU_ExtOutput, wxT("External monitor"));
	menuPlayerSize->AppendRadioItem(MENU_NormalDisplay, wxT("Full size")); //first item is selected by default
	menuPlayerOSD->AppendRadioItem(MENU_AbsoluteTimecode, wxT("&Absolute timecode")); //first item is selected by default
	mPlayer = new prodauto::Player(this, true, prodauto::DUAL_DVS_X11_XV_OUTPUT, prodauto::SOURCE_TIMECODE); //must delete this explicitly on app exit
	wxMenuItem * accelOutput = menuPlayerType->AppendRadioItem(MENU_AccelOutput, wxT("On-screen (accelerated)"));
	wxMenuItem * extUnaccelOutput = menuPlayerType->AppendRadioItem(MENU_ExtUnaccelOutput, wxT("External monitor and On-screen unaccelerated (use if accelerated fails)"));
	menuPlayerType->AppendRadioItem(MENU_UnaccelOutput, wxT("On-screen unaccelerated (use if accelerated fails)"));
	menuPlayerSize->AppendRadioItem(MENU_SmallerDisplay, wxT("Smaller"));
	menuPlayerSize->AppendRadioItem(MENU_SmallestDisplay, wxT("Smaller still"));
	menuPlayerOSD->AppendRadioItem(MENU_RelativeTimecode, wxT("&Relative timecode"));
	menuPlayerOSD->AppendRadioItem(MENU_NoOSD, wxT("&OSD Off"));
	menuPlayer->Append(MENU_PlayerType, wxT("Player type"), menuPlayerType);
	menuPlayer->Append(MENU_PlayerSize, wxT("Player size"), menuPlayerSize)->Enable(false); //Default: external player only, so size irrelevant
	menuPlayer->Append(MENU_PlayerOSD, wxT("Player On Screen Display"), menuPlayerOSD);
	menuBar->Append(menuPlayer, wxT("&Player"));

	//Help menu
	wxMenu *menuHelp = new wxMenu;
	menuHelp->Append(MENU_Help, wxT("&Help..."));
	menuHelp->Append(MENU_About, wxT("&About..."));
	menuBar->Append(menuHelp, wxT("&Help"));

	//Shortcuts menu
	wxMenu *menuShortcuts = new wxMenu;
	menuShortcuts->Append(MENU_Record, wxT("Record\tF1"));
	menuShortcuts->Append(MENU_MarkCue, wxT("Mark Cue\tF2"));
	menuShortcuts->Append(MENU_Stop, wxT("Stop\tF5"));
	menuShortcuts->Append(MENU_PrevSource, wxT("Select previous playback source\tF9"));
	menuShortcuts->Append(MENU_NextSource, wxT("Select next playback source\tF10"));
	menuShortcuts->Append(MENU_Up, wxT("Move up cue point list\tUP"));
	menuShortcuts->Append(MENU_Down, wxT("Move down cue point list\tDOWN"));
	menuShortcuts->Append(MENU_PageUp, wxT("Move to previous take\tPGUP"));
	menuShortcuts->Append(MENU_PageDown, wxT("Move to next take\tPGDN"));
	menuShortcuts->Append(MENU_Home, wxT("Move to start of first take\tHOME"));
	menuShortcuts->Append(MENU_End, wxT("Move to start of last take\tEND"));
	menuShortcuts->Append(MENU_J, wxT("Play backwards\tJ"));
	menuShortcuts->Append(MENU_K, wxT("Pause\tK"));
	menuShortcuts->Append(MENU_L, wxT("Play\tL"));
	menuShortcuts->Append(MENU_3, wxT("Step backwards\t3"));
	menuShortcuts->Append(MENU_4, wxT("Step forwards\t4"));
//	menuShortcuts->Append(MENU_Left, wxT("Move left through sources\t["));
//	menuShortcuts->Append(MENU_Right, wxT("Move right through sources\t]"));
	menuHelp->Append(-1, wxT("Shortcuts"), menuShortcuts); //add this after adding menu items to it

	SetMenuBar(menuBar);
	//disable SDI playback ability if card not available
	if (!mPlayer->ExtOutputIsAvailable()) {
		extOutput->Enable(false);
		accelOutput->Check();
		extAccelOutput->Enable(false);
		extUnaccelOutput->Enable(false);
		wxCommandEvent event; //dummy
		OnPlayerOutputTypeChange(event);
	}

	CreateStatusBar();

	//saved state
	if (!wxFile::Exists(SAVED_STATE_FILENAME)) {
		wxMessageBox(wxT("No saved preferences file found.  Default values will be used."), wxT("No saved preferences"));
		wxXmlNode * rootNode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Ingexgui"));
		mSavedState.SetRoot(rootNode);
	}
	else if (!mSavedState.Load(SAVED_STATE_FILENAME)) { //already produces a warning message box
		wxXmlNode * rootNode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Ingexgui"));
		mSavedState.SetRoot(rootNode);
	}
	else if (wxT("Ingexgui") != mSavedState.GetRoot()->GetName()) {
		wxMessageBox(wxString::Format(wxT("%s%s%s"), wxT("Saved preferences file \""), SAVED_STATE_FILENAME, wxT("\": has unrecofnised data: recreating with default values.")), wxT("Unrecognised saved preferences"), wxICON_EXCLAMATION);
		wxXmlNode * rootNode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Ingexgui"));
		mSavedState.SetRoot(rootNode); //dialogue will detect this as updated
	}
	SetProjectDlg dlg(this, mSavedState, false);
	if (dlg.IsUpdated()) {
		mSavedState.Save(SAVED_STATE_FILENAME);
	}
	mProjectName = dlg.GetSelectedProject();
	SetTitle(wxT("Ingex Control: ") + mProjectName);

	//window
	wxBoxSizer * sizer1V = new wxBoxSizer(wxVERTICAL);
	SetSizer(sizer1V);

	//recorder selection row
	wxBoxSizer * sizer2aH = new wxBoxSizer(wxHORIZONTAL);
	sizer1V->Add(sizer2aH, 0, wxEXPAND);
	wxStaticBoxSizer * recorderSelectionBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Recorder Selection"));
	sizer2aH->Add(recorderSelectionBox, 1, wxALL, CONTROL_BORDER);
	mRecorderGroup = new RecorderGroupCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, argc, argv);
	recorderSelectionBox->Add(mRecorderGroup, 1, wxALL | wxALIGN_CENTRE_VERTICAL, CONTROL_BORDER);
	mRecorderListRefreshButton = new wxButton(this, BUTTON_RecorderListRefresh, wxT("Refresh list"));
	recorderSelectionBox->Add(mRecorderListRefreshButton, 0, wxALL | wxALIGN_CENTRE_VERTICAL, CONTROL_BORDER);
	mTapeIdButton = new wxButton(this, BUTTON_TapeId, wxT("Set tape IDs"));
	sizer2aH->Add(mTapeIdButton, 0, wxALL | wxALIGN_CENTRE_VERTICAL, CONTROL_BORDER);

	//indicator row
	wxBoxSizer * sizer2bH = new wxBoxSizer(wxHORIZONTAL);
	sizer1V->Add(sizer2bH);
	mStatusCtrl = new wxStaticBitmap(this, -1, wxBitmap(blank));
	sizer2bH->Add(mStatusCtrl, 0, wxALL | wxALIGN_CENTRE, CONTROL_BORDER);

	mTimecodeBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Studio Timecode"));
	sizer2bH->Add(mTimecodeBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	wxStaticText * timecodeDisplay = new wxStaticText(this, -1, UNKNOWN_TIMECODE); //need to set the contents to read the correct size for the position box and display
	wxFont * font = wxFont::New(TIME_FONT_SIZE, wxFONTFAMILY_MODERN); //this way works under GTK
	timecodeDisplay->SetFont(*font);
	timecodeDisplay->SetToolTip(wxT("Time-of-day timecode"));
	mTimecodeBox->Add(timecodeDisplay, 0, wxALL | wxALIGN_CENTRE, CONTROL_BORDER);
	mAlertCtrl = new wxStaticBitmap(this, -1, wxBitmap(blank));
	mAlertCtrl->SetToolTip(wxT(""));
	sizer2bH->Add(mAlertCtrl, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE, CONTROL_BORDER);
	wxStaticBoxSizer * positionBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Position"));
	sizer2bH->Add(positionBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	wxStaticText * positionDisplay = new wxStaticText(this, -1, NO_POSITION, wxDefaultPosition, timecodeDisplay->GetSize());
	positionDisplay->SetFont(*font);
	delete font;
	positionDisplay->SetToolTip(wxT("Current recording's position"));
	positionBox->Add(positionDisplay, 0, wxALIGN_CENTRE);

	mTimepos = new Timepos(timecodeDisplay, positionDisplay);

	//transport controls
	wxBoxSizer * sizer2cH = new wxBoxSizer(wxHORIZONTAL);
	sizer1V->Add(sizer2cH);
	mRecordButton = new RecordButton(this, BUTTON_Record);
	sizer2cH->Add(mRecordButton, 0, wxALL, CONTROL_BORDER);
	mStopButton = new wxButton(this, BUTTON_Stop, wxT("Stop"));
	sizer2cH->Add(mStopButton, 0, wxALL, CONTROL_BORDER);
	mCueButton = new wxButton(this, BUTTON_Cue, wxT("Mark Cue"));
	sizer2cH->Add(mCueButton, 0, wxALL, CONTROL_BORDER);

	//splitter window containing everything else
	wxSplitterWindow * splitterWindow = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D);
	sizer1V->Add(splitterWindow, 1, wxEXPAND | wxALL);

	//notebook panel and notebook
	wxPanel * notebookPanel = new wxPanel(splitterWindow); //need a panel so we can have a sizer so we can have a control border...
	notebookPanel->SetNextHandler(this);
	wxBoxSizer * notebookPanelSizer = new wxBoxSizer(wxVERTICAL);
	notebookPanel->SetSizer(notebookPanelSizer);
//	mNotebook = new wxNotebook(splitterWindow, wxID_ANY, wxDefaultPosition, wxSize(splitterWindow->GetClientSize().x, NOTEBOOK_HEIGHT));
	mNotebook = new wxNotebook(notebookPanel, wxID_ANY);
	notebookPanelSizer->Add(mNotebook, 1, wxEXPAND + wxALL, CONTROL_BORDER);
//	sizer1V->Add(mNotebook, 1, wxEXPAND | wxALL, CONTROL_BORDER);

	//notebook record page
	wxPanel * recordPage = new wxPanel(mNotebook); //need this as can't add a sizer directly to a notebook
	recordPage->SetNextHandler(this);
	mNotebook->AddPage(recordPage, wxT("Record"));
	wxBoxSizer * recordPageSizer = new wxBoxSizer(wxVERTICAL);
	recordPage->SetSizer(recordPageSizer);
	wxStaticBoxSizer * recordTagBox = new wxStaticBoxSizer(wxHORIZONTAL, recordPage, wxT("Project"));
	recordPageSizer->Add(recordTagBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	mRecordTagCtrl = new wxStaticText(recordPage, -1, mProjectName);
	recordTagBox->Add(mRecordTagCtrl, 1, wxEXPAND);
	mTree = new TickTreeCtrl(recordPage, TREE, wxDefaultPosition, wxDefaultSize, wxT("Sources"));
	recordPageSizer->Add(mTree, 1, wxEXPAND | wxALL, CONTROL_BORDER);

	//display a sensible amount (i.e total initial size of record tab) in the upper part of the splitter window 
	splitterWindow->SetMinimumPaneSize(mNotebook->GetSize().GetHeight() + recordPage->GetSize().GetHeight() + recordTagBox->GetSize().GetHeight() + mRecordTagCtrl->GetSize().GetHeight() + mTree->GetSize().GetHeight()); //SetSashPosition doesn't work - probably because it doesn't affect the frame layout

	//notebook playback page
	wxPanel * playbackPage = new wxPanel(mNotebook); //need this as can't add a sizer directly to a notebook
	playbackPage->SetNextHandler(this); //allow source select radio button events to propagate here
	mNotebook->AddPage(playbackPage, wxT("Playback"));
	wxBoxSizer * playbackPageSizer = new wxBoxSizer(wxVERTICAL);
	playbackPage->SetSizer(playbackPageSizer);
	wxStaticBoxSizer * playbackTagBox = new wxStaticBoxSizer(wxHORIZONTAL, playbackPage, wxT("Project"));
	playbackPageSizer->Add(playbackTagBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
	mPlaybackTagCtrl = new wxStaticText(playbackPage, -1, wxT(""));
	playbackTagBox->Add(mPlaybackTagCtrl, 1, wxEXPAND);
	wxStaticBoxSizer * playbackSourcesBox = new wxStaticBoxSizer(wxHORIZONTAL, playbackPage, wxT("Sources"));
	playbackPageSizer->Add(playbackSourcesBox, 1, wxEXPAND | wxALL, CONTROL_BORDER);
	mPlaybackSourceSelector = new DragButtonList(playbackPage);
	playbackSourcesBox->Add(mPlaybackSourceSelector, 1, wxEXPAND);

	//event panel
	wxPanel * eventPanel = new wxPanel(splitterWindow);
	splitterWindow->SplitHorizontally(notebookPanel, eventPanel); //default 50% split
	eventPanel->SetNextHandler(this);
	wxBoxSizer * eventPanelSizer = new wxBoxSizer(wxVERTICAL);
	eventPanel->SetSizer(eventPanelSizer);

	//take navigation buttons
	wxBoxSizer * sizer2dH = new wxBoxSizer(wxHORIZONTAL);
	eventPanelSizer->Add(sizer2dH);
	mPrevTakeButton = new wxButton(eventPanel, BUTTON_PrevTake, wxT("Start of Take")); //init with longest string to make button big enough
	sizer2dH->Add(mPrevTakeButton, 0, wxALL, CONTROL_BORDER);
	mNextTakeButton = new wxButton(eventPanel, BUTTON_NextTake, wxT("End of Take"));
	sizer2dH->Add(mNextTakeButton, 0, wxALL, CONTROL_BORDER);

	//event list
	mEventList = new wxListView(eventPanel, -1, wxDefaultPosition, wxSize(splitterWindow->GetClientSize().x, EVENT_LIST_HEIGHT), wxLC_REPORT|wxLC_SINGLE_SEL|wxSUNKEN_BORDER/*|wxALWAYS_SHOW_SB*/); //ALWAYS_SHOW_SB results in a disabled scrollbar on GTK (wx 2.8)
	//set up the columns
	wxListItem itemCol;
	itemCol.SetText(wxT("Event"));
	itemCol.SetWidth(EVENT_COL_WIDTH);
	mEventList->InsertColumn(0, itemCol);
#ifdef __WIN32__
	mEventList->SetColumnWidth(0, 140);
#endif
	itemCol.SetText(wxT("Timecode"));
	mEventList->InsertColumn(1, itemCol); //will set width of this automatically later
#ifdef __WIN32__
	mEventList->SetColumnWidth(1, 125);
#endif
	itemCol.SetText(wxT("Position"));
	mEventList->InsertColumn(2, itemCol); //will set width of this automatically later
#ifdef __WIN32__
	mEventList->SetColumnWidth(2, 125);
#endif
	eventPanelSizer->Add(mEventList, 1, wxEXPAND + wxALL, CONTROL_BORDER);

	wxToolTip::SetDelay(TOOLTIP_DELAY);
	Fit();
	splitterWindow->SetMinimumPaneSize(1); //to allow it to be almost but not quite closed.  Do this after Fit() or the earlier value will not be reflected in the window layout.
	SetMinSize(GetSize()); //prevents scrollbar of event list disappearing oddly

	mRecorderGroup->StartGettingRecorders();
	Connect(false);
	SetStatus(STOPPED);
}

/// Responds to an application close event by closing, preventing closure or seeking confirmation, depending on the circumstances.
/// @param event The close event.
void ingexguiFrame::OnClose(wxCloseEvent & event)
{
	if (event.CanVeto() && RECORDING == mStatus) {
		wxMessageBox(wxT("You cannot close the application while recording: stop the recording first"), wxT("Cannot close while recording"), wxICON_EXCLAMATION);
		event.Veto();
	}	
	else if (!event.CanVeto() || !mConnected || wxYES == wxMessageBox(wxT("Are you sure?  You are not disconnected"), wxT("Confirmation of Quit"), wxYES_NO | wxICON_QUESTION)) {
		if (mConnected) {
			delete mRecorderGroup; //or doesn't exit
		}
		Destroy();
	}
	else {
		event.Veto();
	}
}

/// Responds to an About menu request by showing the appropriate dialogue.
/// @param event The command event.
void ingexguiFrame::OnAbout( wxCommandEvent& WXUNUSED( event ) )
{
	AboutDlg dlg(this);
	dlg.ShowModal();
}

/// Responds to a Help menu request by displaying the help dialogue modelessly.
/// @param event The command event.
void ingexguiFrame::OnHelp( wxCommandEvent& WXUNUSED( event ) )
{
	//modeless help dialog
	mHelpDlg->Show();
}

/// Responds to a Project Names menu request by showing the appropriate dialogue and updating state if user makes changes.
/// @param event The command event.
void ingexguiFrame::OnSetProjectName( wxCommandEvent& WXUNUSED( event ) )
{
	SetProjectDlg dlg(this, mSavedState, true); //force
	if (dlg.IsUpdated()) {
		mSavedState.Save(SAVED_STATE_FILENAME);
		mProjectName = dlg.GetSelectedProject();
		SetTitle(wxT("Ingex Control: ") + mProjectName);
		mRecordTagCtrl->SetLabel(mProjectName);
	}
}

/// Responds to a Set Rolls menu request by showing the appropriate dialogue and informing the recorder group if user makes changes.
/// @param event The command event.
void ingexguiFrame::OnSetRolls( wxCommandEvent& WXUNUSED( event ) )
{
	SetRollsDlg dlg(this, mRecorderGroup->GetPreroll(), mRecorderGroup->GetMaxPreroll(), mRecorderGroup->GetPostroll(), mRecorderGroup->GetMaxPostroll());
	if (wxID_OK == dlg.ShowModal()) {
		mRecorderGroup->SetPreroll(dlg.GetPreroll());
		mRecorderGroup->SetPostroll(dlg.GetPostroll());
	}
}

/// Responds to a Refresh Recorder List request by initiating the process of obtaining a list of recorders.
/// @param event The command event.
void ingexguiFrame::OnRecorderListRefresh( wxCommandEvent& WXUNUSED( event ) )
{
	mRecorderGroup->StartGettingRecorders();
}

/// Responds to the tape IDs button being pressed by showing the appropriate dialogue and updating state if user makes changes.
/// @param event The button event.
void ingexguiFrame::OnSetTapeIds( wxCommandEvent& WXUNUSED( event ) )
{
	wxArrayString names;
	std::vector<bool> enabled;
	mTree->GetPackageNames(names, enabled);
	SetTapeIdsDlg dlg(this, mSavedState, names, enabled);
	if (dlg.IsUpdated()) {
		mSavedState.Save(SAVED_STATE_FILENAME);
		mTree->UpdateTapeIds(mSavedState); //will issue an event to update the record and tape ID buttons
	}
}

/// Responds to a Clear Log menu request.
/// @param event The command event.
void ingexguiFrame::OnClearLog( wxCommandEvent& WXUNUSED(event) )
{
	ClearLog();
}

/// Responds to OSD type menu requests by communicating the OSD type to the player.
/// @param event The command event.
void ingexguiFrame::OnPlayerOSDTypeChange( wxCommandEvent& event )
{
	switch (event.GetId()) {
		case MENU_AbsoluteTimecode :
			mPlayer->SetOSD(prodauto::SOURCE_TIMECODE);
			break;
		case MENU_RelativeTimecode :
			mPlayer->SetOSD(prodauto::CONTROL_TIMECODE);
			break;
		default :
			mPlayer->SetOSD(prodauto::OSD_OFF);
			break;
	}
}

/// Responds to a player type/size menu requests by communicating with the player and enabling/disabling other menu items as appropriate.
/// @param event The command event.
void ingexguiFrame::OnPlayerOutputTypeChange(wxCommandEvent& WXUNUSED(event))
{
	float scale = 1.0;
	if (GetMenuBar()->FindItem(MENU_UnaccelOutput)->IsChecked() || GetMenuBar()->FindItem(MENU_ExtUnaccelOutput)->IsChecked()) {
		//we can only have normal display when non-accelerated
		GetMenuBar()->FindItem(MENU_NormalDisplay)->Check();
		GetMenuBar()->FindItem(MENU_SmallerDisplay)->Enable(false);
		GetMenuBar()->FindItem(MENU_SmallestDisplay)->Enable(false);
	}
	else {
		GetMenuBar()->FindItem(MENU_SmallerDisplay)->Enable(true);
		GetMenuBar()->FindItem(MENU_SmallestDisplay)->Enable(true);
		if (GetMenuBar()->FindItem(MENU_SmallerDisplay)->IsChecked()) {
			scale = 0.8;
		}
		else if (GetMenuBar()->FindItem(MENU_SmallestDisplay)->IsChecked()) {
			scale = 0.5;
		}
	}
	GetMenuBar()->FindItem(MENU_PlayerSize)->Enable(!GetMenuBar()->FindItem(MENU_ExtOutput)->IsChecked());
	if (GetMenuBar()->FindItem(MENU_ExtOutput)->IsChecked()) {
		mPlayer->SetDisplayParams(prodauto::DVS_OUTPUT, scale);
	}
	else if (GetMenuBar()->FindItem(MENU_AccelOutput)->IsChecked()) {
		mPlayer->SetDisplayParams(prodauto::X11_XV_OUTPUT, scale);
	}
	else if (GetMenuBar()->FindItem(MENU_ExtAccelOutput)->IsChecked()) {
		mPlayer->SetDisplayParams(prodauto::DUAL_DVS_X11_XV_OUTPUT, scale);
	}
	else if (GetMenuBar()->FindItem(MENU_ExtUnaccelOutput)->IsChecked()) {
		mPlayer->SetDisplayParams(prodauto::DUAL_DVS_X11_OUTPUT, scale);
	}
	else {
		//on-screen unaccelerated
		mPlayer->SetDisplayParams(prodauto::X11_OUTPUT, scale);
	}
}

/// Responds to a menu quit event by attempting to close the app.
/// @param event The command event.
void ingexguiFrame::OnQuit( wxCommandEvent& WXUNUSED( event ) )
{
	Close();
}

/// Responds to various shortcut menu events.
/// @param event The command event.
void ingexguiFrame::OnShortcut( wxCommandEvent& event )
{
	switch (event.GetId()) {
		case MENU_PrevSource :
			//previous source
			GetMenuBar()->FindItem(MENU_PrevSource)->Enable(mPlaybackSourceSelector->EarlierSource(true));
			break;
		case MENU_NextSource :
			//next source
			GetMenuBar()->FindItem(MENU_NextSource)->Enable(mPlaybackSourceSelector->LaterSource(true));
			break;
		case MENU_Up :
			//up one event
			SelectAdjacentEvent(false);
			break;
		case MENU_Down :
			//down one event
			SelectAdjacentEvent(true);
			break;
		case MENU_Home :
			//start of first take
			mEventList->Select(0);
			break;
		case MENU_End :
			//start of last take
			mEventList->Select(mTakeInfo.Item(mTakeInfo.GetCount() - 1).GetStartIndex()); //Menu item is disabled if no takes!!
			break;
		case MENU_J :
			//play backwards
//			if (mPlayer->Within()) {
				mPlayer->PlayBackwards();
//			}
			break;
		case MENU_K :
			//pause
			mPlayer->Pause();
			break;
		case MENU_L :
			//play
			if (AtStopEvent()) {
				mPlayer->JumpToCue(0); //the start - replay
			}
			mPlayer->Play();
			break;
		case MENU_3 :
			mPlayer->Step(false);
			break;
		case MENU_4 :
			mPlayer->Step(true);
			break;
	}
}

/// Responds to record/playback selected tab changing.
/// @param event The notebook event.
void ingexguiFrame::OnNotebookPageChange(wxNotebookEvent & event)
{
	//want to call UpdatePlayerAndEventControls(), but depending on the platform. wxNotebook::GetSelection() will return "before change" or "after change" if called in the event handler, so instead, send an unrelated event which will trigger the same thing
	wxListEvent listEvent(wxEVT_COMMAND_LIST_ITEM_SELECTED);
	AddPendingEvent(listEvent);
	//will call UpdatePlayerAndEventControls
}

/// Moves up or down the event list by one item, if possible
/// @param down True to move downwards; false to move upwards.
void ingexguiFrame::SelectAdjacentEvent(bool down) {
	long item = mEventList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (-1 == item) {
		//nothing selected
		mEventList->Select(down ? mEventList->GetItemCount() - 1 : 0); //select first item
	}
	else if (!down && 0 != item) {
		//not the first item
		mEventList->Select(--item); //select next item up
	}
	else if (down && mEventList->GetItemCount() - 1 != item) {
		//not the last item
		mEventList->Select(++item); //select next item up
	}
	mEventList->EnsureVisible(item); //scroll if necessary
}

/// Responds to record button being pressed.
/// @param event The button event.
void ingexguiFrame::OnRecord( wxCommandEvent& WXUNUSED( event ) )
{
	if (GetMenuBar()->FindItem(MENU_AutoClear)->IsChecked()) {
		//clear everything away because we're starting again
		ClearLog(); //resets player
	}
	else {
		ResetPlayer();
	}
	ProdAuto::MxfTimecode now;
	mTimepos->GetTimecode(&now);
	mRecorderGroup->RecordAll(mProjectName, now);

	SetStatus(UNKNOWN);
}

/// Responds to the Previous Take button/menu item by moving to the start of the take or the start of the previous.
/// @param event The command event.
void ingexguiFrame::OnPrevTake( wxCommandEvent& event )
{
	//get the index of the take info corresponding to the current selection
	wxListItem item;
	item.SetId(mEventList->GetFirstSelected());
	mEventList->GetItem(item); //the take info index is in this item's data

	//set the selection
	if (AtStartEvent() && !mPlayer->Within()) {
		//the start of this take is selected: select the start of the previous
		if (item.GetData()) {
			//not the first take
			mEventList->Select(mTakeInfo.Item(item.GetData() - 1).GetStartIndex());
		}
	}
	else {
		//somewhere within a take: go to the start of it
		mEventList->Select(mTakeInfo.Item(item.GetData()).GetStartIndex());
	}
	mEventList->EnsureVisible(mEventList->GetFirstSelected());

	UpdatePlayerAndEventControls(true); //force player to jump to cue, as this won't happen if the selected event hasn't changed
}

/// Responds to the Next Take button/menu item by moving to the start of the next take
/// @param event The command event.
void ingexguiFrame::OnNextTake( wxCommandEvent& event )
{
	//get the index of the take info corresponding to the current selection
	wxListItem item;
	item.SetId(mEventList->GetFirstSelected());
	mEventList->GetItem(item); //the take info index is in this item's data

	//set the selection
	if (item.GetData() == mTakeInfo.GetCount() - 1) {
		//In the last take: move to the end
		mEventList->Select(mEventList->GetItemCount() - 1);
	}
	else {
		//Not in the last take: move to the start of the next
		mEventList->Select(mTakeInfo.Item(item.GetData() + 1).GetStartIndex());
	}
	mEventList->EnsureVisible(mEventList->GetFirstSelected());

	UpdatePlayerAndEventControls();
}

/// Responds to the an event from the player.
/// @param event The command event.
/// The following event IDs are recognised, with corresponding data values (all enumerations being in the prodauto namespace):
///	NEW_FILESET: Set up a new column of source select buttons.
///		Event int: the selected source.
///	STATE_CHANGE: Reflect the player's status.
///		Event int: PLAY, PLAY_BACKWARDS, PAUSE, STOP, CLOSE.
///	CUE_POINT: Select indicated point in the event list and pause player if at end/start of file depending on playback direction
///		Event int: Event list index for cue point.
///	FRAME_DISPLAYED: Update time counter.
///		Event int: Non-zero for valid frame number.
///		Event extra long: Frame number.
///	WITHIN_TAKE: Update "Previous Take" button because it will now cause a jump to the start of the current take.
///	CLOSE_REQ: Switch the player to external output only, or disable completely if external output is not being used.
///	KEYPRESS: Respond to the user pressing a shortcut key in the player window.
///		Event int: X-windows key value.
void ingexguiFrame::OnPlayerEvent(wxCommandEvent& event) {
	switch ((prodauto::PlayerEventType) event.GetId()) {
		case prodauto::NEW_FILESET :
			mPlaybackSourceSelector->EnableAndSelectSources((std::vector<bool> *) event.GetClientData(), event.GetInt());
			break;
		case prodauto::STATE_CHANGE :
			switch (event.GetInt()) {
				case prodauto::PLAY :
					SetStatus(PLAYING);
					break;
				case prodauto::PLAY_BACKWARDS :
					SetStatus(PLAYING_BACKWARDS);
					break;
				case prodauto::PAUSE :
					SetStatus(PAUSED);
					break;
				case prodauto::STOP :
					SetStatus(STOPPED);
					break;
				case prodauto::CLOSE :
					//Issued on Enable(false) _and_ Reset()
					UpdatePlayerAndEventControls();
					break;
			}
			break;
		case prodauto::CUE_POINT :
			//move to the right position in the list
			//As it is the player selecting this event, stop the player being told to jump to it
			mCurrentSelectedEvent = event.GetInt();
			mEventList->Select(event.GetInt()); //generates and processes a selection event immediately
			if ((AtStopEvent() && PLAYING == mStatus) || (AtStartEvent() && PLAYING_BACKWARDS == mStatus)) {
				mPlayer->Pause(); //player is already banging its head against the end stop, but this will issue a state change event
			}
			break;
		case prodauto::FRAME_DISPLAYED :
			if (event.GetInt()) {
				//valid position
				mTimepos->SetPosition(event.GetExtraLong());
			}
			else {
				mTimepos->SetPositionUnknown(true); //display "no position"
			}
			break;
		case prodauto::WITHIN_TAKE :
			//have just moved into a take: update the "previous take" button
			UpdatePlayerAndEventControls();
			break;
		case prodauto::CLOSE_REQ :
			if (GetMenuBar()->FindItem(MENU_ExtAccelOutput)->IsChecked() || GetMenuBar()->FindItem(MENU_ExtUnaccelOutput)->IsChecked()) {
				//external output is active so don't close the player, just switch to external only
				GetMenuBar()->FindItem(MENU_ExtOutput)->Check(); //doesn't generate an event
				wxCommandEvent dummyEvent;
				OnPlayerOutputTypeChange(dummyEvent);
			}
			else {
				//close the player completely
				GetMenuBar()->FindItem(MENU_DisablePlayer)->Check(); //doesn't generate an event
				wxCommandEvent dummyEvent;
				OnDisablePlayer(dummyEvent);
			}
			break;
		case prodauto::KEYPRESS :
//std::cerr << event.GetInt() << std::endl;
			wxCommandEvent menuEvent(wxEVT_COMMAND_MENU_SELECTED);
			event.SetId(wxID_ANY);
			//menu events are not blocked even if the menu is disabled
			switch (event.GetInt()) {
				case 65470: //F1
					//Record
					if (GetMenuBar()->FindItem(MENU_Record)->IsEnabled()) {
						menuEvent.SetId(MENU_Record);
					}
					break;
				case 65471: //F2
					//Mark cue
					if (GetMenuBar()->FindItem(MENU_MarkCue)->IsEnabled()) {
						menuEvent.SetId(MENU_MarkCue);
					}
					break;
				case 65474: //F5
					//Stop
					if (GetMenuBar()->FindItem(MENU_Stop)->IsEnabled()) {
						menuEvent.SetId(MENU_Stop);
					}
					break;
				case 65478: //F9
					//Prev Source
					menuEvent.SetId(MENU_PrevSource);
					break;
				case 65479: //F10
					//Next Source
					menuEvent.SetId(MENU_NextSource);
					break;
				case 65362 : //Up
					//Prev Event
					menuEvent.SetId(MENU_Up);
					break;
				case 65364 : //Down
					//Next Event
					menuEvent.SetId(MENU_Down);
					break;
				case 65365 : //PgUp
					//Prev Take
					menuEvent.SetId(MENU_PageUp);
					break;
				case 65366 : //PgDn
					//Next Take
					menuEvent.SetId(MENU_PageDown);
					break;
				case 65360 : //Home
					//First Take
					menuEvent.SetId(MENU_Home);
					break;
				case 65367 : //Home
					//Last Take
					menuEvent.SetId(MENU_End);
					break;
				case 106 : //j
					//play backwards
					menuEvent.SetId(MENU_J);
					break;
				case 107 : //k
					//pause
					menuEvent.SetId(MENU_K);
					break;
				case 108 : //l
					//play forwards
					menuEvent.SetId(MENU_L);
					break;
				case 51 : case 65361 : //3 and left
					//step backwards
					menuEvent.SetId(MENU_3);
					break;
				case 52 : case 65363 : //4 and right
					//step forwards
					menuEvent.SetId(MENU_4);
					break;
			}
			if (wxID_ANY != menuEvent.GetId()) {
				AddPendingEvent(menuEvent);
			}
			break;
	}
}

/// Responds to a source select radio button being pressed (the event being passed on from the source select buttons object).
/// Informs the player of the new source.
/// @param event The command event.
void ingexguiFrame::OnPlaybackSourceSelect(wxCommandEvent & event)
{
//wxMessageBox(wxString::Format(wxT("src %d"), event.GetId()));
	mPlayer->SelectSource(event.GetId());
}

/// Responds to the stop button being pressed.
/// @param event The command event.
void ingexguiFrame::OnStop( wxCommandEvent& WXUNUSED( event ) )
{
	if (RECORDING == mStatus) {
		ProdAuto::MxfTimecode now;
		mTimepos->GetTimecode(&now);
		mRecorderGroup->Stop(now);
		SetStatus(UNKNOWN);
	}
	else if (PLAYING == mStatus || PLAYING_BACKWARDS == mStatus || PAUSED == mStatus) {
		SetStatus(STOPPED);
	}
}

/// Responds to the Play/Pause/Cue button being pressed.
/// Adds a cue point, or tells the player.
/// @param event The command event.
void ingexguiFrame::OnCue( wxCommandEvent& WXUNUSED( event ) )
{
	switch (mStatus) {
		case RECORDING :
			AddEvent(CUE);
			break;
		case PLAYING : case PLAYING_BACKWARDS :
			mPlayer->Pause();
			break;
		case PAUSED :
			if (AtStopEvent()) {
				mPlayer->JumpToCue(0); //the start - replay
			}
			//resume playing
			mPlayer->Play();
			break;
		default :
			break;
	}
}


/// Responds to an event (cue point) being selected.
/// Updates controls and informs player.
/// @param event The command event.
void ingexguiFrame::OnEventSelection(wxListEvent& WXUNUSED(event))
{
	//NB a list event is generated by OnNotebookPageChange() so this must be changed if this function is modified to do other things than call UpdatePlayerAndEventControls()
	UpdatePlayerAndEventControls();
}

//When double-clicked, jumps to the selected event, and starts playing if paused; replaying if at stop point

/// Responds to an event (cue point) being double-clicked.
/// Updates controls and informs player, and starts playback if paused.
/// @param event The command event.
void ingexguiFrame::OnEventActivated(wxListEvent& WXUNUSED(event))
{
	UpdatePlayerAndEventControls();
	if (PAUSED == mStatus) {
		if (AtStopEvent()) {
			mPlayer->JumpToCue(0); //the start - replay
		}
		mPlayer->Play();
	}
}

/// Responds to the an event from the group of recorders.
/// @param event The command event.
/// The following event IDs are recognised, the enumerations being in the RecorderGroupCtrl namespace:
/// 	GETTING_RECORDERS: Disables the list refresh button.
///	GOT_RECORDERS: Enables the list refresh button.
///	NEW_RECORDER: Adds the recorder to the source tree, and sets mode to connected.  Sets status to recording if the new recorder is recording.
///		Event string: The recorder name.
///		Event client data: Ptr to a RecorderData object, containing a track list and a track status list.  Deletes this.
///	REQUEST_RECORD: Asks the source tree for a list of enable states and tape IDs and passes it to the recorder group.
///		Event string: The recorder name (an event will be received for each recorder).
///	STATUS_UNKNOWN: Updates tree (and status indicator).
///		Event string: The recorder name.
///	RECORDING: If successful, set status to recording and set timecode counter to given timecode if not already recording.
///		Otherwise, report failure.
///		Event string: The recorder name.
///		Event int: Non-zero for success.
///		Event client data: Ptr to a RecorderData object, containing a timecode.  Deletes this.
///	STOPPED: If successful, set status to stopped and set timecode counter to given timecode if not already stopped.
///		Otherwise, report failure.
///		Event string: The recorder name.
///		Event int: Non-zero for success.
///		Event client data: Ptr to a RecorderData object, containing a timecode.  Deletes this.
///	TRACK_STATUS: Informs source tree of tracks' status (recording or not) for given recorder.
///		Event string: The recorder name.
///		Event client data: Ptr to a RecorderData object, containing a track status list.  Deletes this.
///	REMOVE_RECORDER: Removes the given recorder from the tree, and sets mode to disconnected if no recorders remaining.
///		Event string: The recorder name.
///	DISPLAY_TIMECODE: Displays the given timecode, enabling auto-increment.
///		Event client data: Ptr to a RecorderData object, containing a timecode.  Deletes this.
///	DISPLAY_TIMECODE_STUCK: Displays the given timecode, disabling auto-increment.
///		Event client data: Ptr to a RecorderData object, containing a timecode.  Deletes this.
///	DISPLAY_TIMECODE_MISSING: Displays timecode as being missing.
///	DISPLAY_TIMECODE_SOURCE: Labels the timecode source as the given recorder name (which may be empty).
///		Event string: The recorder name.
void ingexguiFrame::OnRecorderGroupEvent(wxCommandEvent& event) {
	switch ((RecorderGroupCtrl::RecorderGroupCtrlEventType) event.GetId()) {
		case RecorderGroupCtrl::GETTING_RECORDERS :
			mRecorderListRefreshButton->Disable();
			break;
		case RecorderGroupCtrl::GOT_RECORDERS :
			mRecorderListRefreshButton->Enable();
			break;
		case RecorderGroupCtrl::NEW_RECORDER :
			//event contains all the details about the recorder
			mTapeIdButton->Enable(mTree->AddRecorder(event.GetString(), ((RecorderData *) event.GetClientData())->GetTrackList(), ((RecorderData *) event.GetClientData())->GetTrackStatusList(), event.GetInt(), mSavedState));
			Connect(true);
			if (mTree->RecordingSuccessfully()) {
				//this recorder is already recording
				ResetPlayer();
				SetStatus(RECORDING); //will prevent any more recorders being added
				AddEvent(START);
				mTimepos->SetPositionUnknown(); //don't know when recording started
			}
			delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::REQUEST_RECORD :
			//record enable list required
			{
				CORBA::BooleanSeq enableList;
				CORBA::StringSeq tapeIds;
				if (mTree->GetEnableStates(event.GetString(), enableList, tapeIds)) { //something's enabled for recording, and not already recording
					mRecorderGroup->Record(event.GetString(), enableList, tapeIds);
				}
			}
			break;
		case RecorderGroupCtrl::STATUS_UNKNOWN :
			//event contains name of recorder with unknown status
			mTree->SetRecorderStateUnknown(event.GetString());
			break;
		case RecorderGroupCtrl::RECORDING :
			if (event.GetInt()) { //successful
				if (RECORDING != mStatus) {
					//use the first returned timecode as the start position
					mTimepos->Record(((RecorderData *) event.GetClientData())->GetTimecode());
					AddEvent(START);
				}
				SetStatus(RECORDING); //do this always, to allow record button to reflect overall record status
			}
			else {
				wxMessageBox(wxT("Record command failed for \"") + event.GetString() + wxT("\"."), wxT("Recorder problem"), wxICON_EXCLAMATION);
				mTree->SetRecorderStateProblem(event.GetString());
			}
			delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::STOPPED :
			if (event.GetInt()) { //successful
				//Add the recorded files to the take info
				mTakeInfo.Item(mTakeInfo.GetCount() - 1).AddRecorder(((RecorderData *) event.GetClientData())->GetTrackList(), ((RecorderData *) event.GetClientData())->GetFileList());
				if (STOPPED != mStatus) {
					//use the first returned timecode as the start position
					mTimepos->Stop(((RecorderData *) event.GetClientData())->GetTimecode());
					AddEvent(STOP);
					SetStatus(STOPPED);
				}
				else {
					//need to reload the player as more files have appeared
					mCurrentTakeInfo = 0;
					UpdatePlayerAndEventControls();
				}
			}
			else {
				wxMessageBox(wxT("Stop command failed for \"") + event.GetString() + wxT("\"."), wxT("Recorder problem"), wxICON_EXCLAMATION);
				mTree->SetRecorderStateProblem(event.GetString());
			}
			delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::TRACK_STATUS :
			mTree->SetTrackStatus(event.GetString(), RECORDING == mStatus, ((RecorderData *) event.GetClientData())->GetTrackStatusList()); //will set record button and status indicator
			delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::REMOVE_RECORDER :
			//event contains name of recorder being removed
			mTapeIdButton->Enable(mTree->RemoveRecorder(event.GetString()));
			if (mRecorderGroup->IsEmpty()) {
				Connect(false);
			}
			break;
		case RecorderGroupCtrl::DISPLAY_TIMECODE :
			mTimepos->SetTimecode(((RecorderData *) event.GetClientData())->GetTimecode(), false);
			delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::DISPLAY_TIMECODE_STUCK :
			mTimepos->SetTimecode(((RecorderData *) event.GetClientData())->GetTimecode(), true);
			delete (RecorderData *) event.GetClientData();
			break;
		case RecorderGroupCtrl::DISPLAY_TIMECODE_MISSING :
			mTimepos->DisableTimecode(UNKNOWN_TIMECODE);
			break;
		case RecorderGroupCtrl::DISPLAY_TIMECODE_SOURCE :
			if (event.GetString().IsEmpty()) {
				mTimecodeBox->GetStaticBox()->SetLabel(wxT("Studio Timecode"));
			}
			else {
				mTimecodeBox->GetStaticBox()->SetLabel(wxT("Studio Timecode (") + event.GetString() + wxT(")"));
			}
			break;
	}
}

/// Responds to an event from the source tree.
/// Updates various controls.
/// @param event The command event.
void ingexguiFrame::OnTreeEvent(wxCommandEvent& WXUNUSED( event ))
{
	bool enableRecordButton = mTree->SomeEnabled() && mTree->TapeIdsOK() && (RECORDING != mStatus || mTree->Problem()); //allow trying again if problem
	mRecordButton->Enable(enableRecordButton);
	GetMenuBar()->FindItem(MENU_Record)->Enable(enableRecordButton);
	mStopButton->Enable(STOPPED != mStatus || mTree->Problem());
	GetMenuBar()->FindItem(MENU_Stop)->Enable(STOPPED != mStatus || mTree->Problem());
	mAlertCtrl->SetBitmap(mTree->Problem() ? exclamation : happy);
	//Highlight the tape ID button if we can't record because of missing tape IDs
	mTapeIdButton->SetBackgroundColour(mTree->SomeEnabled() && !mTree->TapeIdsOK() ? wxColour(0xFF, 0x80, 0x00) : wxNullColour);
}

/// Responds to the player enable/disable menu requests by telling the player and updating menu items.
/// @param event The command event.
void ingexguiFrame::OnDisablePlayer(wxCommandEvent& WXUNUSED( event ))
{
	bool enabled = !GetMenuBar()->FindItem(MENU_DisablePlayer)->IsChecked();
	mPlayer->Enable(enabled);
	GetMenuBar()->FindItem(MENU_PlayerType)->Enable(enabled);
	GetMenuBar()->FindItem(MENU_PlayerSize)->Enable(enabled);
	GetMenuBar()->FindItem(MENU_PlayerOSD)->Enable(enabled);
}

/// Adds a stop/start/cue point event to the event list.
/// For start events, creates a new TakeInfo object.
/// Updates the other controls and the player.
/// @param type The event type: START, CUE, STOP or PROBLEM.
void ingexguiFrame::AddEvent(EventType type)
{
	wxListItem item;
	wxFont font(EVENT_FONT_SIZE, wxFONTFAMILY_DEFAULT, wxFONTFLAG_UNDERLINED, wxFONTFLAG_UNDERLINED);
//	font.SetUnderlined(true);
//	font.SetPointSize(FONT_SIZE); //Default rather small in MSW
	switch (type) {
		case START :
			item.SetText(wxT("Start"));
			item.SetTextColour(wxColour(0xFF, 0x20, 0x00));
//			item.SetBackgroundColour(wxColour(wxT("GREEN")));
//			mStartEvent = mEventList->GetItemCount();
			{
				TakeInfo * info = new TakeInfo(mEventList->GetItemCount(), mProjectName); //this will be the index of the current event
				mTakeInfo.Add(info); //deleted by array (object array)
			}
//			font.SetWeight(wxFONTWEIGHT_BOLD); breaks auto col width
//			font.SetStyle(wxFONTSTYLE_ITALIC); breaks auto col width
			break;
		case CUE :
			item.SetText(wxT("Cue"));
			item.SetTextColour(wxColour(wxT("CORNFLOWER BLUE")));
			break;
//		case REVIEW :
//			item.SetText(wxString::Format(wxT("Last %d s"), REVIEW_THRESHOLD));
//			item.SetTextColour(wxColour(wxT("CORNFLOWER BLUE")));
//			break;
		case STOP :
			item.SetText(wxT("Stop"));
			item.SetTextColour(wxColour(wxT("BLACK"))); //needed with cygwin for text to appear
			mEndEvent = mEventList->GetItemCount();
			break;
		case PROBLEM :
			item.SetText(wxT("Problem"));
			item.SetTextColour(wxColour(wxT("RED")));
			break;
	}

	item.SetId(mEventList->GetItemCount()); //will insert at end
	item.SetData(mTakeInfo.GetCount() - 1); //the index of the take info for this take
	mEventList->InsertItem(item); //insert (at end)
	mEventList->EnsureVisible(item.GetId()); //scroll down if necessary
	if (START == type) {
		mEventList->Select(item.GetId());
	}
	else if (STOP == type) {
		GetMenuBar()->FindItem(MENU_ClearLog)->Enable();
	}
#ifndef __WIN32__
	mEventList->SetColumnWidth(0, wxLIST_AUTOSIZE);
#endif
	item.SetFont(font); //point size ignored on GTK and row height doesn't adjust on Win32
	//timecode
	item.SetColumn(1);
	if (START == type) {
		item.SetText(mTimepos->GetStartTimecode()); //started before now, because of preroll etc
	}
	else {
		item.SetText(mTimepos->GetTimecode());
	}
	font.SetFamily(wxFONTFAMILY_MODERN); //fixed pitch so fields don't move about
	mEventList->SetItem(item);
#ifndef __WIN32__
	mEventList->SetColumnWidth(1, wxLIST_AUTOSIZE);
#endif
	//position
	item.SetColumn(2);
	if (START == type) {
		item.SetText(mTimepos->GetStartPosition());
	}
	else {
		item.SetText(mTimepos->GetPosition());
	}
	if (START != type && STOP != type) {
		mTakeInfo.Item(mTakeInfo.GetCount() - 1).AddCuePoint(mTimepos->GetFrameCount());
	}

	mEventList->SetItem(item); //additional to previous data
#ifndef __WIN32__
	mEventList->SetColumnWidth(2, wxLIST_AUTOSIZE);
#endif

	if (-1 == mEventList->GetFirstSelected()) {
		//First item in the list
		mEventList->Select(0);
	}
	UpdatePlayerAndEventControls();
}

/// Sets the mode to connected or disconnected.
/// Updates various controls and sets status to STOPPED if connecting.
/// @param state True to connect.
void ingexguiFrame::Connect(bool state)
{
	mConnected = state;
	if (mConnected) {
		SetStatus(mStatus); //refresh things
		GetMenuBar()->FindItem(MENU_SetRolls)->Enable();
		mAlertCtrl->SetBitmap(happy);
		mAlertCtrl->SetToolTip(wxT("Status OK"));
		//record button must be enabled separately because we may already be recording when we connect
	}
	else {
		mTimepos->DisableTimecode();
		mAlertCtrl->SetBitmap(blank);
		mAlertCtrl->SetToolTip(wxT(""));
		mTree->Clear();
		mRecordButton->Disable();
		GetMenuBar()->FindItem(MENU_SetRolls)->Enable(false);
		SetStatus(STOPPED);
		mTapeIdButton->Disable();
	}
}

/// Sets the gui status.
/// @param stat The new status:
/// 	STOPPED: Not recording and not playing.
///	RECORDING
///	PLAYING
///	PLAYING_BACKWARDS
///	PAUSED
///	UNKNOWN: Waiting for a response from a recorder.
void ingexguiFrame::SetStatus(Stat status)
{
	mStatus = status;
	switch (mStatus) {
		case STOPPED:
			mStatusCtrl->SetBitmap(stop);
			mStatusCtrl->SetToolTip(wxT("Stopped"));
			SetStatusText(wxT("Stopped."));
			mRecordButton->Enable(mTree->SomeEnabled() && mTree->TapeIdsOK());
			GetMenuBar()->FindItem(MENU_Record)->Enable(mTree->SomeEnabled());
			mRecordButton->SetToolTip(wxT("Start a new recording"));
			mStopButton->Disable();
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable(false);
			GetMenuBar()->FindItem(MENU_Stop)->Enable(false);
			mTree->EnableChanges();
			mRecorderGroup->Enable();
			break;
		case RECORDING:
			mStatusCtrl->SetBitmap(record);
			mStatusCtrl->SetToolTip(wxT("Recording"));
			SetStatusText(wxT("Recording."));
			if (mTree->RecordingSuccessfully()) {
				mRecordButton->Record();
				mRecordButton->SetToolTip(wxT("Recording"));
			}
			else {
				mRecordButton->Enable();
				mRecordButton->SetToolTip(wxT("Re-send record command"));
			}
			GetMenuBar()->FindItem(MENU_Record)->Enable(false);
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable();
			GetMenuBar()->FindItem(MENU_Stop)->Enable();
			mStopButton->Enable();
			mStopButton->SetToolTip(wxT("Stop recording after postroll"));
			mTree->EnableChanges(false);
			mRecorderGroup->Disable();
			break;
		case PLAYING:
			mStatusCtrl->SetBitmap(play);
			mStatusCtrl->SetToolTip(wxT("Playing"));
			SetStatusText(wxT("Playing."));
			mRecordButton->Enable(mTree->SomeEnabled());
			GetMenuBar()->FindItem(MENU_Record)->Enable(mTree->SomeEnabled());
			mRecordButton->SetToolTip(wxT("Start a new recording"));
			mStopButton->Disable();
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable(false);
			GetMenuBar()->FindItem(MENU_Stop)->Enable(false);
			mStopButton->SetToolTip(wxT(""));
			mTree->EnableChanges();
			mRecorderGroup->Enable();
			break;
		case PLAYING_BACKWARDS:
			mStatusCtrl->SetBitmap(play_backwards);
			mStatusCtrl->SetToolTip(wxT("Playing backwards"));
			SetStatusText(wxT("Playing backwards."));
			mRecordButton->Enable(mTree->SomeEnabled());
			GetMenuBar()->FindItem(MENU_Record)->Enable(mTree->SomeEnabled());
			mRecordButton->SetToolTip(wxT("Start a new recording"));
			mStopButton->Disable();
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable(false);
			GetMenuBar()->FindItem(MENU_Stop)->Enable(false);
			mStopButton->SetToolTip(wxT(""));
			mTree->EnableChanges();
			mRecorderGroup->Enable();
			break;
		case PAUSED:
			mStatusCtrl->SetBitmap(paused);
			mStatusCtrl->SetToolTip(wxT("Paused"));
			SetStatusText(wxT("Paused."));
			mRecordButton->Enable(mTree->SomeEnabled());
			GetMenuBar()->FindItem(MENU_Record)->Enable(mTree->SomeEnabled());
			mRecordButton->SetToolTip(wxT("Start a new recording"));
			mStopButton->Disable();
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable(false);
			GetMenuBar()->FindItem(MENU_Stop)->Enable(false);
			mStopButton->SetToolTip(wxT(""));
			mTree->EnableChanges();
			mRecorderGroup->Enable();
			break;
		case UNKNOWN:
			mStatusCtrl->SetBitmap(question);
			mStatusCtrl->SetToolTip(wxT("Awaiting response"));
			SetStatusText(wxT("Awaiting response."));
			mRecordButton->Disable();
			GetMenuBar()->FindItem(MENU_Record)->Enable(false);
			mRecordButton->SetToolTip(wxT(""));
			mStopButton->Disable();
			GetMenuBar()->FindItem(MENU_MarkCue)->Enable(false);
			GetMenuBar()->FindItem(MENU_Stop)->Enable(false);
			mTree->EnableChanges(true); //we're stuck if a record command fails
			mRecorderGroup->Enable();
			break;
	}
	UpdatePlayerAndEventControls();
}

/// Updates player and various controls.
/// @param force Force player to jump to current cue point.
/// If a new valid take has been selected in the event list, updates the playback notebook page and loads the player with the new files.
/// If a new cue point has been selected in the event list, or if force is true, instruct player to jump to this cue point.
void ingexguiFrame::UpdatePlayerAndEventControls(bool force)
{
	if (mTakeInfo.GetCount()) {
		//work out the current take info
		wxListItem item;
		item.SetId(mEventList->GetFirstSelected());
		mEventList->GetItem(item);
		TakeInfo * currentTakeInfo = &(mTakeInfo.Item(item.GetData()));
		//player control
		if (currentTakeInfo->GetFiles()->GetCount()) { //the take is complete and there are at least some files available
			if (currentTakeInfo != mCurrentTakeInfo) { //a new take
				mCurrentTakeInfo = currentTakeInfo;
				//update playback sources notebook page
				mPlaybackTagCtrl->SetLabel(currentTakeInfo->GetTag());
				std::vector<std::string> fileNames;
				std::vector<std::string> sourceNames;
				mPlaybackSourceSelector->SetSources(currentTakeInfo, &fileNames, &sourceNames);
				//load files and jump to current position
				mPlayer->Load(&fileNames, &sourceNames, mCurrentTakeInfo->GetCuePoints(), mCurrentTakeInfo->GetStartIndex(), mEventList->GetFirstSelected() - mCurrentTakeInfo->GetStartIndex());
			}
			else if ((unsigned long) mEventList->GetFirstSelected() != mCurrentSelectedEvent || force) { //only the selected event has changed
				mPlayer->JumpToCue(mEventList->GetFirstSelected() - mCurrentTakeInfo->GetStartIndex());
			}
			mCurrentSelectedEvent = mEventList->GetFirstSelected();
		}
		//"previous" button/"previous", "up" and "first" menu items
		if (mCurrentSelectedEvent || mPlayer->Within()) {
			//not at the beginning of the first take
			mPrevTakeButton->Enable();
			GetMenuBar()->FindItem(MENU_PageUp)->Enable();
			GetMenuBar()->FindItem(MENU_Up)->Enable();
			GetMenuBar()->FindItem(MENU_Home)->Enable();
			if (mCurrentSelectedEvent == mCurrentTakeInfo->GetStartIndex() && !mPlayer->Within()) {
				//the start of this take is selected
				mPrevTakeButton->SetLabel(wxT("Previous Take"));
				GetMenuBar()->FindItem(MENU_PageUp)->SetText(wxT("Move to start of previous take\tPGUP"));
			}
			else {
				//somewhere within a take
				mPrevTakeButton->SetLabel(wxT("Start of Take"));
				GetMenuBar()->FindItem(MENU_PageUp)->SetText(wxT("Move to start of take\tPGUP"));
			}
		}
		else {
			mPrevTakeButton->Disable();
			GetMenuBar()->FindItem(MENU_PageUp)->Enable(false);
			GetMenuBar()->FindItem(MENU_Up)->Enable(false);
			GetMenuBar()->FindItem(MENU_Home)->Enable(false);
		}
		//"next" button/"next", "down" and "last" menu items
		if (mCurrentSelectedEvent != (unsigned long) mEventList->GetItemCount() - 1) {
			//not at the end of the last take
			mNextTakeButton->Enable();
			GetMenuBar()->FindItem(MENU_PageDown)->Enable();
			GetMenuBar()->FindItem(MENU_Down)->Enable();
			GetMenuBar()->FindItem(MENU_End)->Enable();
			if (item.GetData() != mTakeInfo.GetCount() - 1) {
				//not in the last take
				mNextTakeButton->SetLabel(wxT("Next Take"));
				GetMenuBar()->FindItem(MENU_PageDown)->SetText(wxT("Move to start of next take\tPGDN"));
			}
			else {
				//somewhere in the last take
				mNextTakeButton->SetLabel(wxT("End of Take"));
				GetMenuBar()->FindItem(MENU_PageDown)->SetText(wxT("Move to end of take\tPGDN"));
			}
		}
		else {
			mNextTakeButton->Disable();
			GetMenuBar()->FindItem(MENU_PageDown)->Enable(false);
			GetMenuBar()->FindItem(MENU_Down)->Enable(false);
			GetMenuBar()->FindItem(MENU_End)->Enable(false);
		}
	}
	else {
		//empty list
		mPrevTakeButton->Disable();
		GetMenuBar()->FindItem(MENU_PageUp)->Enable(false);
		mNextTakeButton->Disable();
		GetMenuBar()->FindItem(MENU_PageDown)->Enable(false);
		GetMenuBar()->FindItem(MENU_Up)->Enable(false);
		GetMenuBar()->FindItem(MENU_Down)->Enable(false);
		GetMenuBar()->FindItem(MENU_Home)->Enable(false);
		GetMenuBar()->FindItem(MENU_End)->Enable(false);
		mPlaybackTagCtrl->SetLabel(wxT(""));
		mPlaybackSourceSelector->Clear();
		mCurrentTakeInfo = 0; //no takes
		mCurrentSelectedEvent = 0;
	}
	//player shortcuts
	GetMenuBar()->FindItem(MENU_J)->Enable(mNotebook->GetSelection() && mPlayer->Within() && PLAYING_BACKWARDS != mStatus);
	GetMenuBar()->FindItem(MENU_K)->Enable(mNotebook->GetSelection() && mPlayer->IsOK() && PAUSED != mStatus);
	GetMenuBar()->FindItem(MENU_L)->Enable(mNotebook->GetSelection() && mPlayer->IsOK() && PLAYING != mStatus);
	GetMenuBar()->FindItem(MENU_3)->Enable(mNotebook->GetSelection() && PAUSED == mStatus && mPlayer->Within());
	GetMenuBar()->FindItem(MENU_4)->Enable(mNotebook->GetSelection() && PAUSED == mStatus && !AtStopEvent());
	GetMenuBar()->FindItem(MENU_PrevSource)->Enable(mPlaybackSourceSelector->EarlierSource(false));
	GetMenuBar()->FindItem(MENU_NextSource)->Enable(mPlaybackSourceSelector->LaterSource(false));
	//cue button
	switch (mStatus) {
		case UNKNOWN:
			mCueButton->Disable();
			mCueButton->SetToolTip(wxT(""));
			break;
		case STOPPED:
			mCueButton->SetLabel(wxT("Play"));
			mCueButton->SetToolTip(wxT("Play from current position"));
			mCueButton->Disable();
			break;
		case RECORDING:
			mCueButton->Enable();
			mCueButton->SetLabel(wxT("Mark cue"));
			mCueButton->SetToolTip(wxT("Store a cue point for subsequent review"));
			break;
		case PLAYING: case PLAYING_BACKWARDS:
			mCueButton->Enable();
			mCueButton->SetLabel(wxT("Pause"));
			mCueButton->SetToolTip(wxT("Freeze playback"));
			break;
		case PAUSED:
			if (AtStopEvent()) {
				mCueButton->SetLabel(wxT("Replay"));
				mCueButton->SetToolTip(wxT("Playback from start"));
			}
			else {
				mCueButton->SetLabel(wxT("Play"));
				mCueButton->SetToolTip(wxT("Play from current position"));
			}
			mCueButton->Enable(mPlayer->IsOK());
			break;
	}
}

/// Determine whether the currently selected event is a stop point (which also means that the player must be at the end of the file)
/// @return True if at a stop point.
bool ingexguiFrame::AtStopEvent()
{
	return (mCurrentTakeInfo && (unsigned long) mEventList->GetFirstSelected() == mCurrentTakeInfo->GetStartIndex() + mCurrentTakeInfo->GetCuePoints()->size() + 1);
}

/// Determine whether the currently selected event is a start point (which means that the player is between the start and the first cue point inclusive)
/// @return True if at a start point
bool ingexguiFrame::AtStartEvent()
{
	return (mCurrentTakeInfo && (unsigned long) mEventList->GetFirstSelected() == mCurrentTakeInfo->GetStartIndex());
}

/// Clears the list of events, the source buttons and anything being displayed by the player
void ingexguiFrame::ClearLog()
{
	ResetPlayer();
	mEventList->DeleteAllItems();
	mTakeInfo.Empty();
	mCurrentTakeInfo = 0;
	GetMenuBar()->FindItem(MENU_ClearLog)->Enable(false);
}

/// Clears the source buttons and anything being displayed by the player
void ingexguiFrame::ResetPlayer()
{
	mPlaybackSourceSelector->Clear();
	mPlayer->Reset();
}
