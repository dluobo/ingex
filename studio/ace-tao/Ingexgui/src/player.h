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

#ifndef PLAYER_H_
#define PLAYER_H_

#include <vector>
#include <string>
#include <map>
#include "LocalIngexPlayer.h"
#include <wx/wx.h>

//File poll timer checks for files appearing after completion of a recording
#define FILE_POLL_TIMER_INTERVAL 250 //ms

DECLARE_EVENT_TYPE(wxEVT_PLAYER_MESSAGE, -1)

namespace prodauto
{

enum PlayerEventType {
	STATE_CHANGE = 0,
	CUE_POINT,
	FRAME_DISPLAYED,
	NEW_FILESET,
	WITHIN_TAKE,
	CLOSE_REQ,
	KEYPRESS
};

enum PlayerMode {
	PLAY,
	PLAY_BACKWARDS,
	PAUSE,
	STOP,
	CLOSE
};

enum OSDtype {
	OSD_OFF,
	CONTROL_TIMECODE,
	SOURCE_TIMECODE
};

class Player;

/// Class with methods that are called by the player.
class Listener : public IngexPlayerListener
{
	public:
		Listener(Player *);
		void SetStartIndex(const int);
		void ClearCuePoints();
		void AddCuePoint(const int64_t);
		virtual void frameDisplayedEvent(const FrameInfo* frameInfo);
		virtual void frameDroppedEvent(const FrameInfo* lastFrameInfo);
		virtual void stateChangeEvent(const MediaPlayerStateEvent* event);
		virtual void endOfSourceEvent(const FrameInfo* lastReadFrameInfo);
		virtual void startOfSourceEvent(const FrameInfo* lastReadFrameInfo);
		virtual void playerClosed();
		virtual void playerCloseRequested();
		virtual void keyPressed(int);
		virtual void keyReleased(int);
	private:
		Player * mPlayer; //never changed so doesn't need protecting by mutex
		wxMutex mMutex;
		int mStartIndex;
		std::map<int64_t, int> mCuePointMap;
};

/// Class representing the X11/SDI MXF video player.
class Player : public wxEvtHandler, LocalIngexPlayer
{
	public:
		Player(wxEvtHandler *, const bool, const PlayerOutputType, const OSDtype);
		bool IsOK();
		void Enable(bool);
		void Load(std::vector<std::string> *, std::vector<std::string> * = 0, std::vector<int64_t> * = 0, int = 0, unsigned int = 0);
		void SelectSource(const int);
		void SetOSD(const OSDtype);
		void SetDisplayParams(const PlayerOutputType, const float);
		void Play();
		void PlayBackwards();
		void Pause();
		void Step(bool);
		void Reset();
		void JumpToCue(unsigned int cuePoint);
		bool Within();
		bool ExtOutputIsAvailable();
//		std::vector<bool> GetFilesStatus();
	private:
		bool Start(std::vector<std::string> * = 0, std::vector<std::string> * = 0, std::vector<int64_t> * = 0, int = 0, unsigned int = 0);
		void OnFrameDisplayed(wxCommandEvent&);
//		void OnPlayerClosing(wxCommandEvent&);
		void OnFilePollTimer(wxTimerEvent&);
		void OnStateChange(wxCommandEvent& event);
		Listener * mListener;
		OSDtype mOSDtype;
		bool mEnabled;
		bool mOK;
		PlayerMode mMode;
		int mStartIndex;
		int mSelectedSource;
		unsigned int mNFilesExisting;
		std::vector<std::string> mFileNames;
		std::vector<std::string> mSourceNames;
		std::vector<bool> mOpened;
		std::vector<int64_t> mCuePoints;
		long mLastFrameDisplayed;
		wxTimer * mFilePollTimer;
		unsigned int mLastRequestedCuePoint;
		DECLARE_EVENT_TABLE()
};

}
#endif