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

#include "player.h"
#include <wx/file.h>

DEFINE_EVENT_TYPE(wxEVT_PLAYER_MESSAGE)

using namespace prodauto; //IngexPlayer class is in this namespace

BEGIN_EVENT_TABLE( Player, wxEvtHandler )
	EVT_COMMAND( FRAME_DISPLAYED, wxEVT_PLAYER_MESSAGE, Player::OnFrameDisplayed )
	EVT_COMMAND( STATE_CHANGE, wxEVT_PLAYER_MESSAGE, Player::OnStateChange )
	EVT_TIMER( wxID_ANY, Player::OnFilePollTimer )
END_EVENT_TABLE()

/// Creates the player, but does not display it.
/// @param handler The handler where events will be sent.
/// @param enabled True to enable player.
/// @param outputType The output type - accelerated or unaccelerated; SDI or not.
/// @param displayType The on screen display type.
Player::Player(wxEvtHandler * handler, const bool enabled, const PlayerOutputType outputType, const OSDtype displayType) : LocalIngexPlayer(outputType), mOSDtype(displayType), mEnabled(enabled), mOK(false) //simple LocalIngexPlayer constructor with defaults
{
	mListener = new Listener(this);
	mFilePollTimer = new wxTimer(this, wxID_ANY);
	SetNextHandler(handler);
	mSelectedSource = 0; //display the quad split by default
	registerListener(mListener);
}

/// Indicates whether the player has successfully loaded anything.
/// @return True if at least one file loaded.
bool Player::IsOK()
{
	return mOK;
}

/// Indicates that an SDI card with a free output is available.
/// @return True if SDI available.
bool Player::ExtOutputIsAvailable()
{
	return dvsCardIsAvailable();
}

/// Enables or disables the player.
/// Enabling opens the window if player is displaying on-screen and there are files loaded.  Disabling closes the window (and the SDI output if used).
/// @param state True to enable.
void Player::Enable(bool state)
{
//std::cerr << "Player Enable" << std::endl;
	if (state != mEnabled) {
		mEnabled = state;
		if (mEnabled) {
			if (mFileNames.size()) {
				//a clip was already registered: reload
				Load(0);
			}
		}
		else {
			mFilePollTimer->Stop(); //suspend any current attempts to load
			stop(); //closes window
			//clear the position display
			wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, FRAME_DISPLAYED);
			guiFrameEvent.SetInt(false); //invalid position
			AddPendingEvent(guiFrameEvent);
			mOK = false;
		}
	}
}

/// If player is enabled, tries to load the given filenames or re-load previously given filenames.  Starts polling if it can't open them all.
/// If player is disabled, stores the given filenames for later.
/// @param fileNames List of file paths, or if zero, use previous supplied list and ignore all other parameters.
/// @param sourceNames Corresponding list of source names, for display as the player window title.
/// @param cuePoints Frame numbers of cue points.
/// @param startIndex Event list index for entry corresponding to start of file.
/// @param cuePoint Index in cuePoints to jump to.
void Player::Load(std::vector<std::string> * fileNames, std::vector<std::string> * sourceNames, std::vector<int64_t> * cuePoints, int startIndex, unsigned int cuePoint)
{
//std::cerr << "Player Load" << std::endl;
	if (mEnabled) {
		mFilePollTimer->Stop(); //may be running from another take
		//see how many files we have before the player loads them, so we can start polling even if more appear just after loading
		mNFilesExisting = 0;
		std::vector<std::string> * fNames = fileNames ? fileNames : &mFileNames;
		for (size_t i = 0; i < fNames->size(); i++) {
			if (wxFile::Exists(wxString(fNames->at(i).c_str(), *wxConvCurrent))) {
				mNFilesExisting++;
			}
		}
	}
	//load the player (this will merely store the parameters if the player is not enabled
	if (!Start(fileNames, sourceNames, cuePoints, startIndex, cuePoint) && mFileNames.size() != mNFilesExisting && mEnabled) {
		//player isn't happy, and (probably) not all files were there when the player opened, so start polling for them
		mFilePollTimer->Start(FILE_POLL_TIMER_INTERVAL);
	}
}


//Starts player (if enabled), either with a new set of files/cue points or with the previously stored values

/// If player is enabled, tries to load the given filenames or re-load previously given filenames.
/// If player is disabled, stores the given filenames for later.
/// This method is the same as Load() except that it does not manipulate the polling timer.
/// @param fileNames List of file paths, or if zero, use previous supplied list and ignore all other parameters.
/// @param sourceNames Corresponding list of source names, for display as the player window title.
/// @param cuePoints Frame numbers of cue points.
/// @param startIndex Event list index for entry corresponding to start of file.
/// @param cuePoint Index in cuePoints to jump to.
bool Player::Start(std::vector<std::string> * fileNames, std::vector<std::string> * sourceNames, std::vector<int64_t> * cuePoints, int startIndex, unsigned int cuePoint)
{
//std::cerr << "Player Start" << std::endl;
	if (fileNames) {
		//a new set of files; assume all parameters are supplied
		mFileNames.clear();
		mSourceNames.clear();
		for (size_t i = 0; i < fileNames->size(); i++) {
			mFileNames.push_back(fileNames->at(i));
			mSourceNames.push_back(sourceNames->at(i));
		}
		mCuePoints.clear();
		mListener->ClearCuePoints();
		mStartIndex = startIndex;
		mListener->SetStartIndex(mStartIndex); //an offset for all the cue point event values
		for (size_t i = 0; i < cuePoints->size(); i++) {
			mCuePoints.push_back(cuePoints->at(i)); //so that we know where to jump to for a given cue point
			mListener->AddCuePoint(cuePoints->at(i)); //so we can work out which cue point has been reached
		}
		mLastFrameDisplayed = 0; //new clip, so previous position not relevant
		mLastRequestedCuePoint = cuePoint;
		mMode = PAUSE;
	}
	bool allFilesOpen = false;
	if (mEnabled) {
		mOpened.clear();
		mOK = start(mFileNames, mOpened);
		if (mOK) {
			//(re)loading stored cue points
			for (size_t i = 0; i < mCuePoints.size(); i++) {
				markPosition(mCuePoints.at(i), 0); //so that an event is generated at each cue point
			}
			//player starts off in play forwards mode
			if (PAUSE == mMode || STOP == mMode) {
				pause();
			}
			else if (PLAY_BACKWARDS == mMode) {
				playSpeed(-1);
			}
			SetOSD(mOSDtype);
			if (mLastFrameDisplayed) {
				//player was already some way into a clip and has been reloaded
				seek(mLastFrameDisplayed, SEEK_SET); //restore position
			}
			else {
				//if mLastRequestedCuePoint isn't zero (start), player was requested to go to a cue point in this clip while no files were available
				JumpToCue(mLastRequestedCuePoint);
			}
			allFilesOpen = true;
			int nFilesOpen = 0;
			int aWorkingSource = 0; //default to quad split if no working sources
			for (size_t i = 0; i < mOpened.size(); i++) {
				allFilesOpen &= mOpened.at(i);
				if (mOpened.at(i)) {
					nFilesOpen++;
					aWorkingSource = i + 1;
				}
			}
			if (1 == nFilesOpen) {
				//display the only source, full screen
				mSelectedSource = aWorkingSource;
			}
			else if (nFilesOpen > 1 && mSelectedSource && !mOpened.at(mSelectedSource - 1)) {
				//display quad split, as several sources available but the requested one isn't
				mSelectedSource = 0;
			}
			//else, display the source last displayed
			SelectSource(mSelectedSource);
		}
		else {
			setX11WindowName("Ingex Player - no files");
		}
		//tell the source selection list the situation
		wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, NEW_FILESET);
		guiEvent.SetClientData(&mOpened);
		guiEvent.SetInt(mSelectedSource);
		AddPendingEvent(guiEvent);
	}
	return allFilesOpen;
}

/// Displays the given source and titles the window appropriately.
/// @param id The source ID - 0 for quad split.
void Player::SelectSource(const int id)
{
//std::cerr << "Player Select Source" << std::endl;
	if (mOK) {
		switchVideo(id);
		mSelectedSource = id;
		if (mSelectedSource) {
			//individual source
			setX11WindowName(mSourceNames[mSelectedSource - 1]);
		}
		else {
			std::string title;
			unsigned int nSources = 0;
			for (size_t i = 0; i < mOpened.size(); i++) {
				if (mOpened.at(i)) {
					title += mSourceNames[i] + "; ";
					if (4 == ++nSources) {
						//Quad Split displays the first four successfully opened files
						break;
					}
				}
			}
			title.resize(title.size() - 2); //remove trailing semicolon and space
			setX11WindowName(title.c_str());
		}
	}
}

/// Starts playing forwards.
void Player::Play()
{
//std::cerr << "Player Play" << std::endl;
	if (mOK) {
		play();
	}
}

/// Starts playing backwards.
void Player::PlayBackwards()
{
//std::cerr << "Player Play backwards" << std::endl;
	if (Within()) {
		playSpeed(-1);
	}
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
/// @param forwards True to step forwards.
void Player::Step(bool forwards)
{
//std::cerr << "Player Step" << std::endl;
	if (mOK) {
		step(forwards);
	}
}

/// Stops the player and displays a blank window.
/// Keeping the window stops it re-appearing in a different place.
void Player::Reset()
{
//std::cerr << "Player Reset" << std::endl;
	mFilePollTimer->Stop();
	mFileNames.clear(); //to indicate that nothing's loaded if player is disabled
	if (mOK) {
		reset();
		setX11WindowName("Ingex Player - no files");
		//clear the position display
		wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, FRAME_DISPLAYED);
		guiFrameEvent.SetInt(false); //invalid position
		AddPendingEvent(guiFrameEvent);
	}
}

/// Moves to the given cue point.
/// @param cuePoint 0 for the start; 1+ for cue points supplied in Load(); the end for anything greater than the number of cue points supplied.
void Player::JumpToCue(unsigned int cuePoint)
{
//std::cerr << "Player JumpToCue" << std::endl;
	if (mOK) {
		if (cuePoint > mCuePoints.size()) {
			//the end
			seek(0, SEEK_END);
		}
		else if (cuePoint) {
			//somewhere in the middle
			seek(mCuePoints.at(cuePoint - 1), SEEK_SET);
		}
		else {
			//the start
			seek(0, SEEK_SET);
		}
	}
	mLastRequestedCuePoint = cuePoint;
	mLastFrameDisplayed = 0; //so that the cue point takes priority when the player is reloaded if it is currently disabled
}

/// Sets the type of on screen display.
/// @param OSDtype :
///	OSD_OFF : none
///	CONTROL_TIMECODE : timecode based on file position
///	SOURCE_TIMECODE : timecode read from file
void Player::SetOSD(const OSDtype type)
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

/// Sets how the player appears and what size it is.
/// @param outputType Whether external, on-screen or both, and whether on-screen is accelerated.
/// @param scale <= 1 for on-screen accelerated player size.
void Player::SetDisplayParams(const PlayerOutputType outputType, const float scale)
{
//std::cerr << "Player SetDisplayParams" << std::endl;
	setOutputType(outputType, scale);
	if (mOK) {
		//we're already seeing something, so make changes take effect immediately
		Start();
	}
}

/// Responds to a frame displayed event from the listener.
/// Detects moving into a take and generates a player event when this happens.
/// Skips the event so it passes to the parent handler
/// @param event The command event.
void Player::OnFrameDisplayed(wxCommandEvent& event) {
	//remember the position to jump to if re-loading occurs
	if (event.GetInt()) {
		//a valid frame value (i,e, not the zero that's sent when the player is disabled)
		if (!mLastFrameDisplayed && event.GetExtraLong()) {
			//have just moved into a take: tell the player so it can update the previous take button
			wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, WITHIN_TAKE);
			AddPendingEvent(guiFrameEvent);
		}
		mLastFrameDisplayed = event.GetExtraLong();
	}
	//tell the gui
	event.Skip();
}

/// Responds to a state change event from the listener.
/// Notes the state change to restore the state if the player is re-loaded.
/// Skips the event so it passes to the parent handler.
/// @param event The command event.
void Player::OnStateChange(wxCommandEvent& event) {
	//remember the mode to set if re-loading occurs
//std::cerr << "State Change " << event.GetInt() << std::endl;
	if (CLOSE != event.GetInt()) {
		mMode = (prodauto::PlayerMode) event.GetInt();
	}
	//tell the gui
	event.Skip();
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
		if (wxFile::Exists(wxString(mFileNames.at(i).c_str(), *wxConvCurrent))) {
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

/// Indicates whether player is at the very start of the file or not.
/// @return True if within the file.
bool Player::Within()
{
	return mOK && mLastFrameDisplayed;
}

/// @param player The player associated with this listener.
Listener::Listener(Player * player) : IngexPlayerListener((IngexPlayerListenerRegistry *) player), mPlayer(player), mStartIndex(0)
{
}

/// Sets the offset added to cue point events returned from the player.
/// @param startIndex The offset.
void Listener::SetStartIndex(const int startIndex)
{
	wxMutexLocker lock(mMutex);
	mStartIndex = startIndex;
}

/// Erases all cue points previously set.
void Listener::ClearCuePoints()
{
	wxMutexLocker lock(mMutex);
	mCuePointMap.clear();
}

/// Adds a cue point to detect a position within the file.
/// @param cuePoint The frame number of the cue point.
void Listener::AddCuePoint(const int64_t cuePoint)
{
	wxMutexLocker lock(mMutex);
	int index = mCuePointMap.size() + 1 + mStartIndex; //do this as a separate line or size() seems to be increased before the assignment!  Can't rely on this being consistent behaviour
	mCuePointMap[cuePoint] = index;
}

/// Callback for each frame displayed.
/// Sends a Frame Displayed event to the player.
/// Sends a Cue Point event to the player if a cue point has been reached.
/// @param frameInfo Structure with information about the frame being displayed.
/// NB Called in another thread context.
void Listener::frameDisplayedEvent(const FrameInfo* frameInfo)
{
	wxCommandEvent guiFrameEvent(wxEVT_PLAYER_MESSAGE, FRAME_DISPLAYED);
	guiFrameEvent.SetInt(true); //valid position
	guiFrameEvent.SetExtraLong(frameInfo->position);
	mPlayer->AddPendingEvent(guiFrameEvent);
	if (frameInfo->isMarked) {
//std::cerr << "Player cue point reached" << std::endl;
		wxCommandEvent guiCueEvent(wxEVT_PLAYER_MESSAGE, CUE_POINT);
		wxMutexLocker lock(mMutex);
		guiCueEvent.SetInt(mCuePointMap[frameInfo->position]);
		guiFrameEvent.SetExtraLong(frameInfo->position);
		mPlayer->AddPendingEvent(guiCueEvent);
	}
}

/// Callback for a frame being repeated from the SDI card because the player didn't supply the next one in time.
/// Currently does nothing
/// @param frameInfo Structure with information about the repeated frame
/// NB Called in another thread context
void Listener::frameDroppedEvent(const FrameInfo* lastFrameInfo)
{
//std::cerr << "frame dropped event" << std::endl;
}

/// Callback for the player changing state.
/// Sends a state change event to the player in the case of a change in direction or between stop, play and pause.
/// @param playerEvent Structure with information about state changes.
/// NB Called in another thread context
void Listener::stateChangeEvent(const MediaPlayerStateEvent* playerEvent)
{
//std::cerr << "state change event" << std::endl;
	if (playerEvent->stopChanged) {
		if (playerEvent->stop) {
			wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, STATE_CHANGE);
			guiEvent.SetInt(STOP);
			mPlayer->AddPendingEvent(guiEvent);
		}
	}
	if (playerEvent->playChanged) {
		//play/pause
		wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, STATE_CHANGE);
		if (playerEvent->play) {
//std::cerr << "speed factor: " << playerEvent->speedFactor << std::endl;
			guiEvent.SetInt(playerEvent->speed < 0 ? PLAY_BACKWARDS : PLAY);
		}
		else {
			guiEvent.SetInt(PAUSE);
		}	
		mPlayer->AddPendingEvent(guiEvent);
	}
	if (playerEvent->speedChanged && playerEvent->play) {
		//forward/reverse
		wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, STATE_CHANGE);
		guiEvent.SetInt(playerEvent->speed < 0 ? PLAY_BACKWARDS : PLAY);
		mPlayer->AddPendingEvent(guiEvent);
//std::cerr << "speed factor changed: " << playerEvent->speedFactor << std::endl;
	}
}

/// Callback for the player reaching the end of the file (while playing forwards).
/// Sends a Cue Point event to the player.
/// @param lastReadFrameInfo Sructure containing information about the last frame read from the file.
/// NB Called in another thread context.
void Listener::endOfSourceEvent(const FrameInfo* lastReadFrameInfo)
{
//std::cerr << "end of source event" << std::endl;
	wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, CUE_POINT);
	guiEvent.SetExtraLong(lastReadFrameInfo->position);
	wxMutexLocker lock(mMutex);
	guiEvent.SetInt(mCuePointMap.size() + 1 + mStartIndex);
	mPlayer->AddPendingEvent(guiEvent);
}

/// Callback for the player reaching the start of the file (while playing backwards).
/// Sends a Cue Point event to the player.
/// @param lastReadFrameInfo Sructure containing information about the last frame read from the file.
/// NB Called in another thread context.
void Listener::startOfSourceEvent(const FrameInfo* lastReadFrameInfo)
{
//std::cerr << "start of source event" << std::endl;
	wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, CUE_POINT);
	guiEvent.SetExtraLong(lastReadFrameInfo->position); //should be 0!
	wxMutexLocker lock(mMutex);
	guiEvent.SetInt(mStartIndex);
	mPlayer->AddPendingEvent(guiEvent);
}

/// Callback for the player being closed.
/// Sends a State Change event to the player.
/// NB Called in another thread context.
void Listener::playerClosed()
{
//std::cerr << "player close event" << std::endl;
	wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, STATE_CHANGE);
	guiEvent.SetInt(CLOSE);
	mPlayer->AddPendingEvent(guiEvent);
}

/// Callback for the user clicking the close box on the player window.
/// Sends a Close Request event to the player.
/// NB Called in another thread context.
void Listener::playerCloseRequested()
{
//std::cerr << "player close requested" << std::endl;
	wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, CLOSE_REQ);
	mPlayer->AddPendingEvent(guiEvent);
}

/// Callback for the user pressing a key when the player window has focus.
/// Sends a Keypress event to the player.
/// @param code The X11 key code
/// NB Called in another thread context.
//NB Called in another thread context
void Listener::keyPressed(int code)
{
//std::cerr << "key pressed" << std::endl;
	wxCommandEvent guiEvent(wxEVT_PLAYER_MESSAGE, KEYPRESS);
	guiEvent.SetInt(code);
	mPlayer->AddPendingEvent(guiEvent);
}

/// Callback for the user releasing a key when the player window has focus.
/// Currently does nothing.
/// @param code The X11 key code
/// NB Called in another thread context.
void Listener::keyReleased(int code)
{
//std::cerr << "key released" << std::endl;
}
