/***************************************************************************
 *   $Id: player.cpp,v 1.17 2010/03/30 07:47:52 john_f Exp $              *
 *                                                                         *
 *   Copyright (C) 2006-2009 British Broadcasting Corporation              *
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

#include "player.h"
#include "wx/file.h"
#include "wx/tglbtn.h"
#include "ingexgui.h"
#include "dialogues.h"
#include "dragbuttonlist.h"
#include "eventlist.h"
#include "ingexgui.h"

DEFINE_EVENT_TYPE(EVT_PLAYER_MESSAGE)

using namespace prodauto; //IngexPlayer class is in this namespace

/***************************************************************************
 *   PLAYER                                                                *
 ***************************************************************************/


BEGIN_EVENT_TABLE(Player, wxPanel)
	EVT_COMMAND(FRAME_DISPLAYED, EVT_PLAYER_MESSAGE, Player::OnFrameDisplayed)
	EVT_COMMAND(STATE_CHANGE, EVT_PLAYER_MESSAGE, Player::OnStateChange)
	EVT_COMMAND(SPEED_CHANGE, EVT_PLAYER_MESSAGE, Player::OnSpeedChange)
	EVT_COMMAND(PROGRESS_BAR_DRAG, EVT_PLAYER_MESSAGE, Player::OnProgressBarDrag)
	EVT_COMMAND(KEYPRESS, EVT_PLAYER_MESSAGE, Player::OnKeyPress)
	EVT_TIMER(wxID_ANY, Player::OnFilePollTimer)
	EVT_SOCKET(wxID_ANY, Player::OnSocketEvent)
	EVT_TOGGLEBUTTON(wxID_ANY, Player::OnModeButton)
	EVT_RADIOBUTTON(wxID_ANY, Player::OnPlaybackTrackSelect)
	EVT_UPDATE_UI(wxID_ANY, Player::OnUpdateUI)
END_EVENT_TABLE()

/// Creates the player, but does not display it.
/// Player must be deleted explicitly or a traffic control notification will be missed.
/// @param parent Parent object.
/// @param enabled True to enable player.
/// @param outputType The output type - accelerated or unaccelerated; SDI or not.
/// @param displayType The on screen display type.
Player::Player(wxWindow* parent, const wxWindowID id, const bool enabled, const PlayerOutputType outputType, const PlayerOSDtype displayType) :
wxPanel(parent, id), LocalIngexPlayer(&mListenerRegistry), mTrackSelector(0), //to check before accessing as it's not created automatically
mOSDtype(displayType), mEnabled(enabled), mOK(false), mState(PlayerState::STOPPED), mSpeed(0), mMuted(false), mOpeningSocket(false),
mPrevTrafficControl(true), //so that it can be switched off
mMode(RECORDINGS), mPreviousMode(RECORDINGS),
mCurrentChunkInfo(0), //to check before accessing in case it hasn't been set
mDivertKeyPresses(false)
{
	//Controls
	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(sizer);
	sizer->Add(new wxToggleButton(this, RECORDINGS, wxT("Recordings")), 0, wxEXPAND); //wxEXPAND allows all buttons to be the same size
	mModesAllowed[RECORDINGS] = true;
#ifndef DISABLE_SHARED_MEM_SOURCE
	sizer->Add(new wxToggleButton(this, ETOE, wxT("E to E")), 0, wxEXPAND); //wxEXPAND allows all buttons to be the same size
	mModesAllowed[ETOE] = true;
#endif
	sizer->Add(new wxToggleButton(this, FILES, wxT("Opened Files")), 0, wxEXPAND); //wxEXPAND allows all buttons to be the same size
	mModesAllowed[FILES] = false; //not opened any files yet
	Fit(); //or buttons shrink (as a result of wxEXPAND)
	Layout(); //or buttons are superimposed!

	//set up LocalIngexPlayer
	setOutputType(outputType);
	setNumAudioLevelMonitors(4);
	Rational unity = {1, 1};
	setPixelAspectRatio(&unity);

	mSelectRecDlg = new SelectRecDlg(this); //persistent so that it can remember what was the last recording played
	mListener = new Listener(this, &mListenerRegistry); //registers with the player
	mFilePollTimer = new wxTimer(this, wxID_ANY);
	mDesiredTrackName = ""; //display the quad split by default
	mSocket = new wxSocketClient();
	mSocket->SetEventHandler(*this);
	mSocket->SetNotify(wxSOCKET_CONNECTION_FLAG);
	TrafficControl(false); //in case it has just been restarted after crashing and leaving traffic control on
}

///Calls SetMode() if a button is being pressed (as opposed to released)
void Player::OnModeButton(wxCommandEvent& event)
{
	if (event.GetInt()) { //pressing in a button
		SetMode((PlayerMode) event.GetId());
	}
}

/// Updates the toggle button enable and selected states via periodic WX pseudo-events
void Player::OnUpdateUI(wxUpdateUIEvent& event)
{
	switch (event.GetId()) {
		case RECORDINGS:
#ifndef DISABLE_SHARED_MEM_SOURCE
		case ETOE:
#endif
		case FILES:
			event.Enable(mModesAllowed[event.GetId()]);
			((wxToggleButton*) event.GetEventObject())->SetValue(event.GetId() == mMode && mEnabled);
			break;
		default:
			break;
	}
}

/// Responds to a track select radio button being pressed (the event being passed on from the track select buttons object).
/// Informs the player of the new track, and updates the enable states of the previous/next track shortcuts
/// @param event The command event.
void Player::OnPlaybackTrackSelect(wxCommandEvent & event)
{
	if (FILES == mMode) { //File Mode
		mFilesModeSelectedTrack = event.GetId();
		SelectTrack(event.GetId(), false); //don't remember the selection
	}
	else {
		SelectTrack(event.GetId(), true); //remember the selection
	}
}

/// Determines whether there are selectable tracks before the current one, or selects the next earlier track.
/// @param select True to select (if possible).
/// @return If select is false, true if there are earlier selectable tracks than that currently selected.  If select is true, true if a new track was selected.
bool Player::EarlierTrack(const bool state)
{
	bool available = false;
	if (mTrackSelector) available = mTrackSelector->EarlierTrack(state);
	return available;
}

/// Determines whether there are selectable tracks after the current one, or selects the next later track.
/// @param select True to select (if possible).
/// @return If select is false, true if there are later selectable tracks than that currently selected.  If select is true, true if a new track was selected.
bool Player::LaterTrack(const bool state)
{
	bool available = false;
	if (mTrackSelector) available = mTrackSelector->LaterTrack(state);
	return available;
}

/// If the quad split is displayed, switches source to that corresponding to the quadrant value given (if it is available).
/// If an individual source is displayed, switches to the quad split (thus providing toggling behaviour).
/// @param source The quadrant (1-4).
void Player::SelectQuadrant(const unsigned int source)
{
	if (mTrackSelector) mTrackSelector->SelectQuadrant(source);
}

/// Creates track selector if not already created.
/// @param parent Parent for the track selector (if not already created).
/// @return The track selector.
DragButtonList* Player::GetTrackSelector(wxWindow * parent)
{
	if (!mTrackSelector) {
		mTrackSelector = new DragButtonList(parent, this);
		SetMode(mMode, true); //SetMode will now work
//if no setting of event handler, crashes when GetNextHandler() is called
//		mTrackSelector->PushEventHandler(this); //so that events go here - crashes when button pressed
//		mTrackSelector->SetNextHandler(this); //so that events go here
	}
	return mTrackSelector;
}

/// Responds to a keypress in the player window by passing it through, executing a player command, generating a frame menu event or ignoring it.
/// @param event The command event containing the keypress and modifier.
void Player::OnKeyPress(wxCommandEvent& event)
{
//std::cerr << "key: " << event.GetInt() << std::endl;
//std::cerr << "modifier: " << event.GetExtraLong() << std::endl;
	if (mDivertKeyPresses) {
		event.Skip(); //let the parent deal with it
	}
	else {
		int frameCommand = wxID_ANY;
		switch (event.GetInt()) {
			case 65470: //F1
				frameCommand = IngexguiFrame::BUTTON_MENU_Record;
				break;
			case 65471: //F2
				frameCommand = IngexguiFrame::MENU_MarkCue;
				break;
			case 65474: //F5 (NB shift is detected below)
				frameCommand = IngexguiFrame::BUTTON_MENU_Stop;
				break;
			case 65362 : //Up
				//Prev Event
				frameCommand = IngexguiFrame::MENU_Up;
				break;
			case 65364 : //Down
				//Next Event
				frameCommand = IngexguiFrame::MENU_Down;
				break;
			case 65365 : //PgUp
				frameCommand = IngexguiFrame::BUTTON_MENU_PrevTake;
				break;
			case 65366 : //PgDn
				frameCommand = IngexguiFrame::BUTTON_MENU_NextTake;
				break;
			case 65360 : //Home
				//First Take
				frameCommand = IngexguiFrame::MENU_FirstTake;
				break;
			case 65367 : //End
				//Last Take
				frameCommand = IngexguiFrame::MENU_LastTake;
				break;
			case 116 : //t
				frameCommand = IngexguiFrame::MENU_JumpToTimecode;
				break;
			case 32 : //space
				if (mSpeed) {
					Pause();
				}
				else {
					Play();
				}
				break;
			case 51 : case 65361 : //3 and left
				Step(false);
				break;
			case 52 : case 65363 : //4 and right
				Step(true);
				break;
			case 109 : //m
				MuteAudio(!mMuted);
				break;
			case 65476 : //F7
				EarlierTrack(true);
				break;
			case 65477 : //F8
				LaterTrack(true);
				break;
			case 65479 : //F10
				SetMode(RECORDINGS);
				break;
#ifndef DISABLE_SHARED_MEM_SOURCE
			case 65480 : //F11
				SetMode(ETOE);
				break;
#endif
			case 65481 : //F12
				SetMode(FILES);
				break;
			case 106 : //j
				Play(true, true);
				break;
			case 107 : //k
				Pause();
				break;
			case 108 : //l
				Play(true, false);
				break;
		}
		if (wxID_ANY != frameCommand &&	((IngexguiFrame::BUTTON_MENU_Stop == frameCommand && event.GetExtraLong() != 1) || (IngexguiFrame::BUTTON_MENU_Stop != frameCommand && event.GetExtraLong()))) { //stop has been pressed with shift, or any other keypress doesn't have a modifier
			wxCommandEvent menuEvent(wxEVT_COMMAND_MENU_SELECTED, frameCommand);
			GetParent()->AddPendingEvent(menuEvent);
		}
	}
}

///If player is enabled, puts in safe (non disk-accessing) "recording" mode, or returns it to the mode it was in before recording commenced.
///@param recording True to put into recording mode.
void Player::Record(bool recording)
{
	if (recording) {
		mPreviousMode = mMode; //so that previous mode will be restored on stop, even if the player is disabled at the moment
		mModesAllowed[RECORDINGS] = false;
		mModesAllowed[FILES] = false;
		if (mEnabled) {
#ifndef DISABLE_SHARED_MEM_SOURCE //E to E mode available
 			if (ETOE == mMode) { //don't need to change mode
 				if (PlayerState::PAUSED == mState) {
 					//start playback as button to do this is not available during recording
 					Play();
 				}
 			}
 			else {
 				//switch to E to E mode
 				SetMode(ETOE);
 			}
#else
			Reset();
#endif
		}
	}
	else {
		mModesAllowed[RECORDINGS] = true;
		mModesAllowed[FILES] = mFileModeMxfFiles.GetCount() || mFileModeMovFile.Length();
		SetPreviousMode();
	}
}

/// Switches player to the mode last selected
void Player::SetPreviousMode()
{
	if (mEnabled) {
		SetMode(mPreviousMode);
	}
}

/// Returns the label for the player project name
const wxString Player::GetProjectName()
{
	wxString name;
	if (FILES == mMode) {
		if (mFileModeMovFile.IsEmpty()) { //MXF files
			if (mTrackSelector) name = mTrackSelector->GetProjectName();
		}
		else {
			name = mFileModeMovFile; //no project so show file name instead
		}
	}
	else if (mCurrentChunkInfo) {
		name = mCurrentChunkInfo->GetProjectName();
	}
	return name;
}

/// Returns the label for the player project static box; if blank, box should be hidden.
const wxString Player::GetProjectType()
{
	wxString type;
	if (FILES == mMode && !mFileModeMovFile.IsEmpty()) { //showing a MOV file
		type = wxT("Filename");
	}
#ifndef DISABLE_SHARED_MEM_SOURCE
	else if (ETOE != mMode) {
		type = wxT("Project");
	}
#endif
	return type;
}

/// Updates the details for recordings mode. If player is enabled and in recordings mode, responds to the changes.
/// @param chunkInfo Recording details - can be zero which will cause player to be reset if in recordings mode.  Player reloads if this has changed.
/// @param cuePoint A cue point to jump to (0 is the start of the recording; > number of cue points is the end), if this value has changed.
/// @param forceReload Reload the recording even if the chunk info hasn't changed
void Player::SelectRecording(ChunkInfo * chunkInfo, const int cuePoint, const bool forceReload)
{
	bool reload  = forceReload || mCurrentChunkInfo != chunkInfo;
	mCurrentChunkInfo = chunkInfo;
	mLastPlayingBackwards = false; //no point continuing the default play direction of the previous set of files
	if (mCurrentChunkInfo) {
		if (cuePoint > (int) mCurrentChunkInfo->GetCuePointFrames().size()) {
			//the end
			mRecordingModeFrameOffset = -1;
		}
		else if (cuePoint) {
			//somewhere in the middle
			mRecordingModeFrameOffset = mCurrentChunkInfo->GetCuePointFrames()[cuePoint - 1];
		}
		else {
			//the beginning
			mRecordingModeFrameOffset = 0;
		}
	}
	if (mEnabled && RECORDINGS == mMode) { //showing recordings
		if (reload) {
			LoadRecording();
		}
		else {
			JumpToFrame(mRecordingModeFrameOffset);
		}
	}
}

/// Loads the player with the current recording details, or resets if these are absent
void Player::LoadRecording()
{
	if (mCurrentChunkInfo && mTrackSelector) {
		mInputType = mTrackSelector->SetTracks(mCurrentChunkInfo, mFileNames, mTrackNames);
		Load();
	}
	else {
		Reset();
	}
}

/// If this mode is allowed, changes the mode of the player, enabling if disabled.
/// @param mode The desired mode.  No reloading occurs unless this is different from the current mode or force is true.
/// @param force If true, always reloads even if mode isn't changing.
void Player::SetMode(const PlayerMode mode, const bool force)
{
	if (mModesAllowed[mode] && mTrackSelector) {
		if (!mEnabled) Enable();
		if (mode != mMode || force) {
			mPreviousMode = mMode;
			mMode = mode;
			switch (mode) {
				case RECORDINGS:
					LoadRecording();
					break;
#ifndef DISABLE_SHARED_MEM_SOURCE //E to E mode available
				case ETOE: {
						mInputType = mTrackSelector->SetEtoE(mFileNames, mTrackNames);
						Load();
						//clear the position display (even if loading failed) - no more FRAME_DISPLAYED events will be sent in this mode
						wxCommandEvent event(EVT_PLAYER_MESSAGE, FRAME_DISPLAYED);
						event.SetInt(false); //invalid position
						GetParent()->AddPendingEvent(event); //don't add it to this handler or it will be blocked
						break;
					}
#endif
				case FILES:
					{
						if (mFileModeMovFile.IsEmpty()) { //MXF files
							ProdAuto::MxfTimecode* editRate = new ProdAuto::MxfTimecode; //event handler must delete this!
							mInputType = mTrackSelector->SetMXFFiles(mFileModeMxfFiles, mFileNames, mTrackNames, *editRate);
							//Notify the edit rate to allow position display to work if it hasn't got it from anywhere else
							wxCommandEvent event(EVT_PLAYER_MESSAGE, EDIT_RATE);
							event.SetClientData(editRate); //invalid position
							GetParent()->AddPendingEvent(event); //don't add it to this handler or it will be blocked
						}
						else { //MOV file
							mFileNames.clear();
							mFileNames.push_back((const char*) mFileModeMovFile.mb_str(*wxConvCurrent));
							mTrackNames.clear();
							mTrackNames.push_back((const char*) mFileModeMovFile.mb_str(*wxConvCurrent));
							mTrackSelector->Clear();
							mInputType = prodauto::FFMPEG_INPUT;
							mFilesModeSelectedTrack = 1; //first source rather than quad split
						}
						Load();
						break;
					}
			}
		}
	}
}

/// Opens a file/recording select dialogue, loading player if selections are made, and enabling player if disabled.
void Player::Open(const PlayerOpenType type)
{
	bool opened;
	switch (type) {
		case OPEN_MOV: {
			wxFileDialog dlg(this, wxT("Choose a MOV file"), wxT(""), wxT(""), wxT("MOV files |*.mov;*.MOV|All files|*.*"), wxFD_FILE_MUST_EXIST | wxFD_CHANGE_DIR); //select single file only
			if ((opened = (wxID_OK == dlg.ShowModal()))) {
				mFileModeMovFile = dlg.GetPath();
				mFileModeMxfFiles.Clear();
			}
			break;
		}
		case OPEN_MXF: {
			wxFileDialog dlg(this, wxT("Choose one or more MXF files"), wxT(""), wxT(""), wxT("MXF files|*.mxf;*.MXF|All files|*.*"), wxFD_FILE_MUST_EXIST | wxFD_CHANGE_DIR | wxFD_MULTIPLE); //select multiple files
			if ((opened = (wxID_OK == dlg.ShowModal()))) { //at least one file has been chosen
				dlg.GetPaths(mFileModeMxfFiles);
				mFileModeMovFile.Clear();
			}
			break;
		}
		case OPEN_RECORDINGS:
			if (wxDirExists(RECORDING_SERVER_ROOT)) {
				if ((opened = (wxID_OK == mSelectRecDlg->ShowModal()))) {
					mSelectRecDlg->GetPaths(mFileModeMxfFiles);
					mFileModeMovFile.Clear();
				}
			}
			else {
				wxMessageDialog dlg(this, wxString(RECORDING_SERVER_ROOT) + wxT(" is not a directory: no recordings can be chosen\n"), wxT("Cannot access recordings area"));
				dlg.ShowModal();
				opened = false;
			}
			break;
	}
	if (opened) {
		mModesAllowed[FILES] = true;
		mFileModeFrameOffset = 0;
		SetMode(FILES, true); //enables player if disabled; always reload even if mode isn't changing
 	}
}

Player::~Player()
{
	TrafficControl(false, true); //must do this synchronously or the socket will be opened but the data will never be sent
	delete mListener;
	delete mSocket;
}

/// Enables or disables the player.
/// Enabling opens the window if player is displaying on-screen and there are files loaded.  Disabling closes the window (and the SDI output if used).
/// @param state True to enable.
/// @return The enable state.
bool Player::Enable(bool state)
{
//std::cerr << "Player Enable" << std::endl;
	if (state != mEnabled) {
		mEnabled = state;
		if (mEnabled) {
			if (mFileNames.size()) {
				//a clip was already registered: reload
				Load();
			}
		}
		else {
			mFilePollTimer->Stop(); //suspend any current attempts to load
			close(); //stops and closes window
			if (mSpeed) {
				mSpeed = 0;
				TrafficControl(false);
			}
			//clear the position display
			wxCommandEvent guiFrameEvent(EVT_PLAYER_MESSAGE, FRAME_DISPLAYED);
			guiFrameEvent.SetInt(false); //invalid position
			AddPendingEvent(guiFrameEvent);
			mOK = false;
		}
	}
return mEnabled;
}

/// If player is enabled, tries to load the current filenames or re-load previously given filenames.  Starts polling if it can't open them all, unless in E to E mode.
void Player::Load()
{
//std::cerr << "Player Load" << std::endl;
	if (mFileNames.size()) { //not a router recorder recording only, for instance
		if (mEnabled) {
			mFilePollTimer->Stop(); //may be running from another take
#ifndef DISABLE_SHARED_MEM_SOURCE
			if (ETOE != mMode) {
#endif
				//see how many files we have before the player loads them, so we can start polling even if more appear just after loading
				mNFilesExisting = 0;
				for (size_t i = 0; i < mFileNames.size(); i++) {
					if (wxFile::Exists(wxString(mFileNames[i].c_str(), *wxConvCurrent))) {
						mNFilesExisting++;
					}
				}
#ifndef DISABLE_SHARED_MEM_SOURCE
			}
			else {
				//can't poll for shared memory sources
				mNFilesExisting = mFileNames.size();
			}
#endif
		}
#ifndef DISABLE_SHARED_MEM_SOURCE
		if (!mNFilesExisting || ETOE == mMode) {
#else
		if (!mNFilesExisting) {
#endif
			TrafficControl(false);
			mOK = reset(); //otherwise it keeps showing what was there before even if it can't open any files/shared memory
		}
#ifndef DISABLE_SHARED_MEM_SOURCE
		if (ETOE == mMode) {
			mState = PlayerState::PLAYING;
		}
		else
#endif
		if (STATE_CHANGE == mChunkLinking) { //no chunk linking so don't carry on playing
			mState = PlayerState::PAUSED;
		}
		//load the player (this will merely store the parameters if the player is not enabled)
		if (!Start() && mEnabled && mFileNames.size() != mNFilesExisting) { //player isn't happy, and (probably) not all files were there when the player opened
			//start polling for files
			mFilePollTimer->Start(FILE_POLL_TIMER_INTERVAL);
		}
	}
}

/// If player is enabled, tries to load.
/// Tries to select the file position previously selected by the user.  If this wasn't opened, selects the only file open or a quad split otherwise.
/// This method is the same as Load() except that it does not manipulate the polling timer or change the playing state.
/// @return True if all files were opened.
bool Player::Start()
{
//std::cerr << "Player Start" << std::endl;
	bool allFilesOpen = false;
	if (mEnabled) {
		mOpened.clear();
		mAtChunkStart = false; //set when a frame is displayed
		mAtChunkEnd = false; //set when a frame is displayed
		std::vector<PlayerInput> inputs;
		for (size_t i = 0; i < mFileNames.size(); i++) {
			PlayerInput input;
			input.name = mFileNames[i];
			input.type = mInputType;
			inputs.push_back(input);
		}
		int64_t frameOffset;
		switch (mMode) {
			case RECORDINGS:
				if (LOAD_PREV_CHUNK == mChunkLinking) {
					frameOffset = -1; //end
				}
				else {
					frameOffset = mRecordingModeFrameOffset;
				}
				break;
			case FILES:
				frameOffset = mFileModeFrameOffset;
				break;
			default:
				frameOffset = 0;
		}	
#ifndef DISABLE_SHARED_MEM_SOURCE
		mOK = start(inputs, mOpened, SHM_INPUT != mInputType && LOAD_FIRST_CHUNK != mChunkLinking && (PlayerState::PAUSED == mState || PlayerState::STOPPED == mState), frameOffset > -1 ? frameOffset : 0); //play forwards or paused
#else
		mOK = start(inputs, mOpened, LOAD_FIRST_CHUNK != mChunkLinking && (PlayerState::PAUSED == mState || PlayerState::STOPPED == mState), frameOffset > -1 ? frameOffset : 0); //play forwards or paused
#endif
		if (-1 == frameOffset) seek(0, SEEK_END, FRAME_PLAY_UNIT);
		int trackToSelect = (1 == mFileNames.size() ? 1 : 0); //display quad split by default unless only one file
		if (mOK) {
			if (RECORDINGS == mMode && mCurrentChunkInfo) {
				for (size_t i = 0; i < mCurrentChunkInfo->GetCuePointFrames().size(); i++) {
					markPosition(mCurrentChunkInfo->GetCuePointFrames()[i], 0); //so that the pointer changes colour at each cue point
				}
			}
#ifndef DISABLE_SHARED_MEM_SOURCE
			if (ETOE != mMode) {
#endif
				//(re)load stored cue points
				if (STATE_CHANGE != mChunkLinking && mSpeed) { //latter a sanity check
					playSpeed(mSpeed);
				}
				else if (PlayerState::PLAYING_BACKWARDS == mState) {
					playSpeed(-1);
				}
				SetOSD(mOSDtype);
#ifndef DISABLE_SHARED_MEM_SOURCE
			}
			else {
				showProgressBar(false); //irrelevant
//				setOSDTimecode(-1, SOURCE_TIMECODE_TYPE, LTC_SOURCE_TIMECODE_SUBTYPE); //timecode from shared memory
				if (mSpeed) {
					mSpeed = 0;
					TrafficControl(false); //not reading from disk
				}
			}
#endif
			mChunkLinking = STATE_CHANGE; //next recording loaded is not linked to this one
			// work out which track to select
			unsigned int nFilesOpen = 0;
			int aWorkingTrack = 0; //initialisation prevents compiler warning
			for (size_t i = 0; i < mOpened.size(); i++) {
				if (mOpened[i]) {
					nFilesOpen++;
					aWorkingTrack = i + 1; // +1 because track 0 is quad split
				}
			}
			if (mDesiredTrackName.size()) { //want something other than quad split
				size_t i;
				for (i = 0; i < mTrackNames.size(); i++) {
					if (mTrackNames[i] == mDesiredTrackName && mOpened[i]) { //located the desired track and it's available
						trackToSelect = i + 1; // + 1 to offset for quad split
						break;
					}
				}
				if (mTrackNames.size() == i && 1 == nFilesOpen) { //can't use the desired track and only one file open
					//display the only track, full screen
					trackToSelect = aWorkingTrack;
				}
			}
			SelectTrack(trackToSelect, false);
			allFilesOpen = mOpened.size() == nFilesOpen;
			muteAudio(mMuted);
		}
		else {
			SetWindowName(wxT("Ingex Player - no files"));
		}
		//tell the track selection list the situation
		if (mTrackSelector) mTrackSelector->EnableAndSelectTracks(&mOpened, trackToSelect);
	}
	return allFilesOpen;
}

/// Displays the file corresponding to the given track (which is assumed to have been loaded) and titles the window appropriately.
/// @param id The track ID - 0 for quad split.
/// @param remember Save the track name (or the fact that it's a quad split) in order to try to select it when new filesets are loaded
void Player::SelectTrack(const int id, const bool remember)
{
//std::cerr << "Player Select Track" << std::endl;
	if (mOK) {
		wxString title;
		if (id) { //individual track
			if (remember) {
				mDesiredTrackName = mTrackNames[id - 1]; // -1 to offset for quad split
			}
			title = wxString(mTrackNames[id - 1].c_str(), *wxConvCurrent);
		}
		else { //quad split
			if (remember) {
				mDesiredTrackName = "";
			}
			unsigned int nTracks = 0;
			for (size_t i = 0; i < mTrackNames.size(); i++) { //only go through video files
				if (mOpened[i]) {
					title += wxString(mTrackNames[i].c_str(), *wxConvCurrent) + wxT("; ");
					if (4 == ++nTracks) {
						//Quad Split displays the first four successfully opened files
						break;
					}
				}
			}
			if (!title.IsEmpty()) { //trap for only audio files
				title.resize(title.size() - 2); //remove trailing semicolon and space
			}
		}
#ifndef DISABLE_SHARED_MEM_SOURCE
		if (SHM_INPUT == mInputType && !mSpeed && remember) { //player doesn't respond to switchVideo when paused and showing shared memory
			Start();
		}
		else {
#endif
			switchVideo(id);
			if (id) { //not quad split
				mCurrentFileName = mFileNames[id - 1]; //-1 to offset for quad split
			}
			else {
				mCurrentFileName.clear(); //don't handle quad split for now
			}
#ifndef DISABLE_SHARED_MEM_SOURCE
		}
#endif
		SetWindowName(title);
	}
}

/// Plays at an absolute speed, forwards or backwards, or pauses.
/// @param rate: 0 to pause; positive to play forwards; speed = 2 ^ (absolute value - 1)
void Player::PlayAbsolute(const int rate)
{
//std::cerr << "Player PlayAbsolute" << std::endl;
	if (mOK) {
#ifndef DISABLE_SHARED_MEM_SOURCE
		if (rate && SHM_INPUT == mInputType) {
#else
		if (rate) {
#endif
			//can only play forwards at normal speed from shared memory!
			play();
		}
		else if (rate > 0 && !AtRecordingEnd()) { //latter check prevents brief displays of different speeds as shuttle wheel is twiddled clockwise at the end of a recording
			playSpeed(1 << (rate - 1));
		}
		else if (rate < 0 && WithinRecording()) { //latter check prevents brief displays of different speeds as shuttle wheel is twiddled anticlockwise at the beginning of a file
			playSpeed(-1 << (rate * -1 - 1));
		}
		else if (!rate) {
			pause();
		}
	}
}

/// Starts playing, or doubles in play speed (up to max. limit) if already playing.
/// If starting to play forwards at the end of the recording, jumps to the beginning.
/// @param setDirection True to play in the direction specified by the next param; false to play in the same direction as currently playing, or previously playing if paused.
/// @param backwards True to play backwards if setDirection is true.
void Player::Play(const bool setDirection, bool backwards)
{
//std::cerr << "Player Play\n";
	if (mOK) {
		if ((setDirection && !backwards && 0 >= mSpeed) //going backwards, or paused, and want to go forwards
		 || (!setDirection && 0 == mSpeed && !mLastPlayingBackwards)) //paused after playing forwards
		{
			if (AtRecordingEnd() && HasChunkBefore()) {
				//replay from first chunk
				mChunkLinking = LOAD_FIRST_CHUNK; //so we know what to do when the player is reloaded
				//ask for the first chunk
				wxCommandEvent guiFrameEvent(EVT_PLAYER_MESSAGE, mChunkLinking);
				AddPendingEvent(guiFrameEvent);
			}
			else if (AtRecordingEnd()) {
				//replay this chunk
				seek(0, SEEK_SET, FRAME_PLAY_UNIT);
				play();
			}
			else {
				play();
			}
			mLastPlayingBackwards = false;
		}
		else if ((setDirection && backwards && 0 <= mSpeed) //going forwards, or paused and want to go backwards
		 || (!setDirection && 0 == mSpeed && mLastPlayingBackwards)) //paused after playing backwards
		{
			//play backwards at normal speed
			playSpeed(-1);
			mLastPlayingBackwards = true;
		}
		else if (0 <= mSpeed) { //playing forwards
			//double the speed
			if (MAX_SPEED > mSpeed) {
				playSpeed(mSpeed * 2);
			}
		}
		else { //playing backwards
			//double the speed
			if (-MAX_SPEED < mSpeed) {
				playSpeed(mSpeed * 2);
			}
		}
	}
}

/// Indicates if can't go any faster forwards.
/// @return True if at max speed.
bool Player::AtMaxForwardSpeed()
{
#ifndef DISABLE_SHARED_MEM_SOURCE
	if (SHM_INPUT != mInputType) {
#endif
		return mSpeed == MAX_SPEED;
#ifndef DISABLE_SHARED_MEM_SOURCE
	}
	else {
		//can only play at normal speed
		return mSpeed;
	}
#endif
}

/// Indicates if can't go any faster backwards.
/// @return True if at max speed.
bool Player::AtMaxReverseSpeed()
{
#ifndef DISABLE_SHARED_MEM_SOURCE
	if (SHM_INPUT != mInputType) {
#endif
		return mSpeed == -MAX_SPEED;
#ifndef DISABLE_SHARED_MEM_SOURCE
	}
	else {
		//can only play forwards!
		return true;
	}
#endif
}

/// Pauses playback.
void Player::Pause()
{
//std::cerr << "Player Pause" << std::endl;
	if (mOK) {
		pause();
	}
}

/// Steps by one frame.
/// @param direction True to step forwards.
void Player::Step(bool direction)
{
//std::cerr << "Player Step" << std::endl;
	if (mOK) {
		//change chunk if necessary - this code is needed because the player generates spurious frame displayed events so chunk changes as a result of frames displayed must be validated by the player not being paused
		if (direction //forwards
		&& mAtChunkEnd //already displayed the last frame
		&& HasChunkAfter()) { //another chunk follows this one
			mChunkLinking = LOAD_NEXT_CHUNK; //so we know what to do when the player is reloaded
			//ask for the next chunk
			wxCommandEvent guiFrameEvent(EVT_PLAYER_MESSAGE, mChunkLinking);
			AddPendingEvent(guiFrameEvent);
		}
		else if (!direction //backwards!
		&& mAtChunkStart //already displayed the first frame
		&& HasChunkBefore()) { //another chunk precedes this one
			mChunkLinking = LOAD_PREV_CHUNK; //so we know what to do when the player is reloaded
			//ask for the previous chunk
			wxCommandEvent guiFrameEvent(EVT_PLAYER_MESSAGE, mChunkLinking);
			AddPendingEvent(guiFrameEvent);
		}
		else {
			step(direction);
		}
	}
}

/// Stops the player and displays a blank window.
/// Keeping the window stops it re-appearing in a different place.
void Player::Reset()
{
//std::cerr << "Player Reset" << std::endl;
	mFilePollTimer->Stop();
//	mFileNames.clear(); //to indicate that nothing's loaded if player is disabled
	mOK = reset();
	if (mOK) {
		SetWindowName(wxT("Ingex Player - no files"));
	}
	if (mSpeed) {
		mSpeed = 0;
		TrafficControl(false);
	}
	mChunkLinking = STATE_CHANGE; //next recording loaded will be treated as isolated
	if (mTrackSelector) mTrackSelector->Clear();
	wxCommandEvent event(EVT_PLAYER_MESSAGE, FRAME_DISPLAYED);
	event.SetInt(false); //invalid position
	GetParent()->AddPendingEvent(event); //don't add it to this handler or it will be blocked
}

/// Moves to the given frame; -1 for the end
/// @param frame The frame offset.
void Player::JumpToFrame(const int64_t frame)
{
	if (mOK) {
		if (-1 == frame) {
			seek(0, SEEK_END, FRAME_PLAY_UNIT);
		}
		else {
			seek(frame, SEEK_SET, FRAME_PLAY_UNIT);
		}
	}
}

/// Sets the type of on screen display.
/// @param OSDtype :
///	OSD_OFF : none
///	CONTROL_TIMECODE : timecode based on file position
///	SOURCE_TIMECODE : timecode read from file
void Player::SetOSD(const PlayerOSDtype type)
{
//std::cerr << "Player SetOSD" << std::endl;
	mOSDtype = type;
	switch (mOSDtype) {
		case OSD_OFF:
			setOSDScreen(OSD_EMPTY_SCREEN);
			break;
		case CONTROL_TIMECODE:
			setOSDTimecode(-1, CONTROL_TIMECODE_TYPE, -1);
			setOSDScreen(OSD_PLAY_STATE_SCREEN);
			break;
		case SOURCE_TIMECODE:
			setOSDTimecode(-1, SOURCE_TIMECODE_TYPE, -1);
			setOSDScreen(OSD_PLAY_STATE_SCREEN);
			break;
	}
}

/// Enable/disable OSD on the SDI output
/// @param enabled True to enable.
void Player::EnableSDIOSD(bool enabled)
{
	setSDIOSDEnable(enabled);
	Load(); //takes effect on reload
}

/// Sets how the player appears.
/// @param outputType Whether external, on-screen or both, and whether on-screen is accelerated.
void Player::SetOutputType(const PlayerOutputType outputType)
{
//std::cerr << "Player SetOutputType" << std::endl;
	setOutputType(outputType);
	if (mOK) {
		//above call has stopped the player so start it again
		Start();
	}
}

/// Returns how the player is appearing
PlayerOutputType Player::GetOutputType()
{
	return getOutputType();
}

/// Responds to a frame displayed event from the listener. If not playing from shared memory:
/// - Detects reaching the start, the end, a chunk start/end while playing, or leaving the start/end of a file and generates a player event accordingly.
/// - Detects reaching a cue point and generates a player event accordingly.
/// - Pauses if the player is playing and hits a recording end stop.
/// - Notes the frame number in case of reloading.
/// - Skips the event so it passes to the parent handler for position display.
/// NB a cue point is reached if the position is between it and the next cue point in the sense of playing forwards - regardless of the playing direction.
/// @param event The command event.
void Player::OnFrameDisplayed(wxCommandEvent& event) {
	//remember the position to jump to if re-loading occurs
#ifndef DISABLE_SHARED_MEM_SOURCE
	if (SHM_INPUT != mInputType) {
#endif
		if (event.GetInt()) { //a valid frame value (i,e, not the zero that's sent when the player is disabled)
			mAtChunkStart = !event.GetExtraLong();
			mAtChunkEnd = (bool) event.GetClientData();
			mPreviousFrameDisplayed = event.GetExtraLong();
			if (!event.GetExtraLong() && 0 > mSpeed && HasChunkBefore()) { //just reached the start of a chunk, playing backwards (this may disrupt the first frame playing but academic as there will be a disturbance anyway as new files are loaded)
				mChunkLinking = LOAD_PREV_CHUNK; //so we know what to do when the player is reloaded
				//ask for the previous chunk
				wxCommandEvent guiFrameEvent(EVT_PLAYER_MESSAGE, mChunkLinking);
				AddPendingEvent(guiFrameEvent);
			}
			else if (mAtChunkEnd && 0 < mSpeed && HasChunkAfter()) { //just reached the end of a chunk, playing forwards (this may disrupt the last frame playing but academic as there will be a disturbance anyway as new files are loaded)
				mChunkLinking = LOAD_NEXT_CHUNK; //so we know what to do when the player is reloaded
				//ask for the next chunk
				wxCommandEvent guiFrameEvent(EVT_PLAYER_MESSAGE, mChunkLinking);
				AddPendingEvent(guiFrameEvent);
			}
			else if (!event.GetExtraLong() && !HasChunkBefore()) { //at the start of a recording
				wxCommandEvent guiFrameEvent(EVT_PLAYER_MESSAGE, AT_START);
				AddPendingEvent(guiFrameEvent);
				if (0 > mSpeed) { //playing forwards
					Pause();
				}
			}
			else if ((bool) event.GetClientData() && !HasChunkAfter()) { //at the end of a recording
				wxCommandEvent guiFrameEvent(EVT_PLAYER_MESSAGE, AT_END);
				AddPendingEvent(guiFrameEvent);
				Pause();
			}

			if (!mPreviousFrameDisplayed || mAtChunkEnd) { //have just moved into a take
				wxCommandEvent guiFrameEvent(EVT_PLAYER_MESSAGE, WITHIN);
				AddPendingEvent(guiFrameEvent);
			}
			//remember offset for mode changes
			if (FILES == mMode) {
				mFileModeFrameOffset = mPreviousFrameDisplayed; //so that we can reinstate the correct position if we come out of file mode
			}
			else if (RECORDINGS == mMode && mCurrentChunkInfo) {
				mRecordingModeFrameOffset = mPreviousFrameDisplayed; //so that we can reinstate the correct position if we come out of take mode
				//work out which cue point area we're in
				unsigned int mark = 0; //assume before first cue point/end of file
				if (mAtChunkEnd) {
					mark = mCurrentChunkInfo->GetCuePointFrames().size() + 1;
				}
				else {
					for (mark = 0; mark < mCurrentChunkInfo->GetCuePointFrames().size(); mark++) {
						if (mCurrentChunkInfo->GetCuePointFrames()[mark] > event.GetExtraLong()) { //not reached this mark yet
							break;
						}
					}
				}
				mark += mCurrentChunkInfo->GetStartIndex();
				//Tell gui if we've moved into a new area
				if (mLastCuePointNotified != mark) {
					if (!mAtChunkEnd || !HasChunkAfter()) { //not at the last frame with a chunk following, where there is no stop event
//std::cerr << "cue point event" << std::endl;
						wxCommandEvent guiEvent(EVT_PLAYER_MESSAGE, CUE_POINT);
						guiEvent.SetInt(mark);
						AddPendingEvent(guiEvent);
					}
					mLastCuePointNotified = mark;
				}
			}
		}
		//tell the gui so it can update the position display
		event.Skip();
#ifndef DISABLE_SHARED_MEM_SOURCE
	}
#endif
}

/// Responds to a state change event from the listener.
/// Notes the state change to restore the state if the player is re-loaded.
/// Skips the event so it passes to the parent handler.
/// @param event The command event.
void Player::OnStateChange(wxCommandEvent& event)
{
	//remember the mode to set if re-loading occurs
//std::cerr << "State Change " << event.GetInt() << std::endl;
	if (PlayerState::CLOSED != (PlayerState::PlayerState) event.GetInt()) {
		mState = (PlayerState::PlayerState) event.GetInt();
	}
	//tell the gui so it can update state
	event.Skip();
}

/// Responds to a speed change event from the listener.
/// Notes the speed to enable new speed to be calculated for repeated play or play backwards requests.
/// Skips the event so it passes to the parent handler.
/// @param event The command event.
void Player::OnSpeedChange(wxCommandEvent& event)
{
	//inform any copying server
#ifndef DISABLE_SHARED_MEM_SOURCE
	if (event.GetInt() && !mSpeed && SHM_INPUT != mInputType) { //just started playing
#else
	if (event.GetInt() && !mSpeed) { //just started playing
#endif
		//traffic control on, to ensure smooth playback
		TrafficControl(true);
	}
	else if (!event.GetInt() && mSpeed) { //just stopped playing
		//traffic control off, to resume fast copying
		TrafficControl(false);
	}
	//remember the current speed to set if re-loading occurs and if changing speed
	mSpeed = event.GetInt();
//std::cerr << "Speed: " << mSpeed << std::endl;
}

/// Responds to a progress bar drag event from the listener.
/// Seeks to the given position.
/// @param event The command event.
void Player::OnProgressBarDrag(wxCommandEvent& event)
{
	seek(event.GetInt(), SEEK_SET, PERCENTAGE_PLAY_UNIT);
}

/// Responds to the file poll timer.
/// Checks to see if any more files have appeared, and if so, restarts the player.
/// If all files have appeared, kills the timer.
/// @param event The timer event.
void Player::OnFilePollTimer(wxTimerEvent& WXUNUSED(event))
{
//std::cerr << "Player OnFilePollTimer" << std::endl;
	//Have any more files appeared?
	unsigned int nFilesExisting = 0;
	for (size_t i = 0; i < mFileNames.size(); i++) {
		if (wxFile::Exists(wxString(mFileNames[i].c_str(), *wxConvCurrent))) {
			nFilesExisting++;
		}
	}
	if (nFilesExisting > mNFilesExisting) {
		mNFilesExisting = nFilesExisting; //prevents restarting continually unless more files appear
		Start(); //existing parameters
		if (nFilesExisting == mFileNames.size()) {
			//all the files are there - even if the player is unhappy there's nothing else we can do
 			mFilePollTimer->Stop();
		}
	}	
}

/// Responds to successful socket connection.
/// Sends traffic control message and disconnects.
void Player::OnSocketEvent(wxSocketEvent& WXUNUSED(event))
{
//std::cerr << "Socket Connection made; setting traffic control " << (mTrafficControl ? "ON\n" : "OFF\n");
	//Socket is now open so best send a message even if traffic control status hasn't changed
	mSocket->Write(mTrafficControl ? "ingexgui\n0\n" : "ingexgui\n", mTrafficControl ? 11 : 9);
	mPrevTrafficControl = mTrafficControl;
	mSocket->Close();
	mOpeningSocket = false;
}

/// Indicates whether player is at the very start of a recording or not.
/// @return True if within the recording.
bool Player::WithinRecording()
{
	return mOK && (!mAtChunkStart || HasChunkBefore());
}

/// Indicates whether player is at the end of a recording or not.
/// @return True if at the recording end.
bool Player::AtRecordingEnd()
{
	return mOK && mAtChunkEnd && !HasChunkAfter();
}

/// Mutes or unmutes audio
/// @param state true to mute
void Player::MuteAudio(const bool state)
{
	mMuted = state;
	muteAudio(state ? 1 : 0);
	SetWindowName();
}

/// Sets audio to follow video or stick to the first audio files
/// @param state true to follow video
void Player::AudioFollowsVideo(const bool state)
{
	switchAudioGroup(state ? 0 : 1);
}

/// Sets the name of the player window if the player is displayed
/// @param name The name to display, which will have a note appended if audio is muted.  If empty, the previous name will be used (useful for updating mute status)
void Player::SetWindowName(const wxString & name)
{
	if (!name.IsEmpty()) {
		mName = name;
	}
//	if (mOK) {
		wxString title = mName;
		if (X11_OUTPUT == getActualOutputType() || DUAL_DVS_X11_OUTPUT == getActualOutputType()) {
			title += wxT(" (unaccelerated)");
		}
		if (mMuted) {
			title += wxT(" (Muted)");
		}
		setX11WindowName((const char*) title.mb_str(*wxConvCurrent));
//	}
}

/// Sends a traffic control message to a server socket.
/// @param state True to switch traffic control on; false to switch it off.
/// @param synchronous Don't use events - block until the message has been sent (with a timeout)
void Player::TrafficControl(const bool state, const bool synchronous)
{
	mTrafficControl = state; //allows state to be changed while waiting for socket to open
	if (!mOpeningSocket //without this check, app can hang with 100% usage in gettimeofday() if it manages to open a socket
	 && mTrafficControl != mPrevTrafficControl) { //avoid unnecessary socket connections
		wxIPV4address addr;
		addr.Hostname(wxT("localhost"));
		addr.Service(TRAFFIC_CONTROL_PORT);
		mSocket->Notify(!synchronous);
		mSocket->Connect(addr, false); //mustn't block the GUI; instead generates an event on connect (don't bother handling the connection fail situation)
		if (synchronous) {
			mSocket->WaitOnConnect(3);
			wxSocketEvent dummyEvent;
			OnSocketEvent(dummyEvent);
		}
		else {
			mOpeningSocket = true;
		}
	}
}

/// Returns true if there is another chunk before the currently loaded clip
bool Player::HasChunkBefore()
{
	return RECORDINGS == mMode && mCurrentChunkInfo && mCurrentChunkInfo->HasChunkBefore();
}

/// Returns true if there is another chunk after the currently loaded clip
bool Player::HasChunkAfter()
{
	return RECORDINGS == mMode && mCurrentChunkInfo && mCurrentChunkInfo->HasChunkAfter();
}

/// Returns true if there is a selectable track earlier in the list than the current one
bool Player::HasEarlierTrack()
{
	return mTrackSelector->EarlierTrack(false);
}

/// Returns true if there is a selectable track later in the list than the current one
bool Player::HasLaterTrack()
{
	return mTrackSelector->LaterTrack(false);
}

/// Selects the previous selectable track in the list, if any
void Player::SelectEarlierTrack()
{
	mTrackSelector->EarlierTrack(true);
}

/// Selects the next selectable track in the list, if any
void Player::SelectLaterTrack()
{
	mTrackSelector->LaterTrack(true);
}

/***************************************************************************
 *   LISTENER                                                              *
 ***************************************************************************/

/// @param player The player associated with this listener.
Listener::Listener(Player * player, prodauto::IngexPlayerListenerRegistry * registry) : prodauto::IngexPlayerListener(registry), mPlayer(player)
{
}

/// Callback for each frame displayed.
/// Sends a Frame Displayed event to the player (after the Cue Point event so that the gui stops the player at the end of the file).
/// @param frameInfo Structure with information about the frame being displayed.
/// NB Called in another thread context.
void Listener::frameDisplayedEvent(const FrameInfo* frameInfo)
{
	wxCommandEvent guiFrameEvent(EVT_PLAYER_MESSAGE, FRAME_DISPLAYED);
	guiFrameEvent.SetExtraLong(frameInfo->position);
	guiFrameEvent.SetInt(true); //valid position
	if (frameInfo->position == frameInfo->sourceLength - 1) { //at the end of the file
		guiFrameEvent.SetClientData((void *) 1);
	}
	mPlayer->AddPendingEvent(guiFrameEvent);
}

/// Callback for a frame being repeated from the SDI card because the player didn't supply the next one in time.
/// Currently does nothing
/// NB Called in another thread context
void Listener::frameDroppedEvent(const FrameInfo*)
{
//std::cerr << "frame dropped event" << std::endl;
}

/// Callback for the player changing state.
/// Sends a state change event to the player in the case of a change in direction or between stop, play and pause.
/// Sends a speed change event for the above and a speed change.
/// @param playerEvent Structure with information about state changes.
/// NB Called in another thread context
void Listener::stateChangeEvent(const MediaPlayerStateEvent* playerEvent)
{
//std::cerr << "state change event" << std::endl;
	if (playerEvent->stopChanged) {
		if (playerEvent->stop) {
			wxCommandEvent guiEvent(EVT_PLAYER_MESSAGE, STATE_CHANGE);
			guiEvent.SetInt(PlayerState::STOPPED);
			mPlayer->AddPendingEvent(guiEvent);
		}
	}
	if (playerEvent->playChanged) {
		//play/pause
		wxCommandEvent stateEvent(EVT_PLAYER_MESSAGE, STATE_CHANGE);
		wxCommandEvent speedEvent(EVT_PLAYER_MESSAGE, SPEED_CHANGE);
		if (playerEvent->play) {
//std::cerr << "state change PLAY: speed: " << playerEvent->speed << std::endl;
			stateEvent.SetInt(playerEvent->speed < 0 ? PlayerState::PLAYING_BACKWARDS : PlayerState::PLAYING);
			speedEvent.SetInt(playerEvent->speed); //always need this as mSpeed will be 0 and speedChanged will be false if speed is 1
		}
		else {
//std::cerr << "state change PAUSE: speed: " << playerEvent->speed << std::endl;
			stateEvent.SetInt(PlayerState::PAUSED);
			speedEvent.SetInt(0); //Pause reports speed as being 1
		}	
		mPlayer->AddPendingEvent(stateEvent);
		mPlayer->AddPendingEvent(speedEvent);
	}
	else if (playerEvent->speedChanged) {
		wxCommandEvent speedEvent(EVT_PLAYER_MESSAGE, SPEED_CHANGE);
		speedEvent.SetInt(playerEvent->speed);
		mPlayer->AddPendingEvent(speedEvent);
//std::cerr << "speed change: " << playerEvent->speed << std::endl;
	}
}

/// Callback for the player reaching the end of the file (while playing forwards).
/// Does nothing (situation detected in frameDisplayedEvent() instead)
/// NB Called in another thread context.
void Listener::endOfSourceEvent(const FrameInfo*)
{
}

/// Callback for the player reaching the start of the file (while playing backwards).
/// Does nothing (situation detected in frameDisplayedEvent() instead).
/// NB Called in another thread context.
/// @param ypos The vertical position of the click within the player window (relative to its default height)
void Listener::startOfSourceEvent(const FrameInfo*)
{
}

/// Callback for the player being closed.
/// Sends a State Change event to the player.
/// NB Called in another thread context.
void Listener::playerClosed()
{
//std::cerr << "player close event" << std::endl;
	wxCommandEvent guiEvent(EVT_PLAYER_MESSAGE, STATE_CHANGE);
	guiEvent.SetInt(PlayerState::CLOSED);
	mPlayer->AddPendingEvent(guiEvent);
}

/// Callback for the user clicking the close box on the player window.
/// Sends a Close Request event to the player.
/// NB Called in another thread context.
void Listener::playerCloseRequested()
{
//std::cerr << "player close requested" << std::endl;
	wxCommandEvent guiEvent(EVT_PLAYER_MESSAGE, CLOSE_REQ);
	mPlayer->AddPendingEvent(guiEvent);
}

/// Callback for the user pressing a key when the player window has focus.
/// Sends a Keypress event to the player.
/// @param code The X11 key code
/// NB Called in another thread context.
//NB Called in another thread context
void Listener::keyPressed(int code, int modifier)
{
//std::cerr << "key pressed" << std::endl;
	wxCommandEvent event(EVT_PLAYER_MESSAGE, KEYPRESS);
	event.SetInt(code);
	event.SetExtraLong(modifier);
	mPlayer->AddPendingEvent(event);
}

/// Callback for the user releasing a key when the player window has focus.
/// Does nothing.
/// NB Called in another thread context.
void Listener::keyReleased(int, int)
{
//std::cerr << "key released" << std::endl;
}

/// Callback for the progress bar being dragged with the mouse.
/// Sends the position to the player.
/// @param position The progress bar position, from 0 to 100.
/// NB Called in another thread context.
void Listener::progressBarPositionSet(float position)
{
	wxCommandEvent event(EVT_PLAYER_MESSAGE, PROGRESS_BAR_DRAG);
	event.SetInt((int) (position * 1000.)); //this is the resolution the player seek command works to
	mPlayer->AddPendingEvent(event);
}

/// Callback for the mouse being clicked.
/// Decodes which quadrant has been clicked on (if any) and sends the value to the player.
/// @param imageWidth The width of the entire image in the player window.
/// @param imageHeight The height of the entire image in the player window.
/// @param xpos The horizontal position of the click within the player window (relative to its default width).
/// @param ypos The vertical position of the click within the player window (relative to its default height).
void Listener::mouseClicked(int imageWidth, int imageHeight, int xpos, int ypos)
{
	if (xpos <= imageWidth && ypos <= imageHeight) { //clicked inside the image (rather than the window)
		wxCommandEvent event(EVT_PLAYER_MESSAGE, QUADRANT_CLICK);
		event.SetInt((xpos < imageWidth / 2 ? 1 : 2) + (ypos < imageHeight / 2 ? 0 : 2));
		mPlayer->AddPendingEvent(event);
	}
}
