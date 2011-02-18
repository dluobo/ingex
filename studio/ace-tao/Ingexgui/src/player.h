/***************************************************************************
 *   $Id: player.h,v 1.21 2011/02/18 16:31:15 john_f Exp $                *
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

#ifndef PLAYER_H_
#define PLAYER_H_

#include <vector>
#include <string>
#include <map>
#include "LocalIngexPlayer.h"
#include <wx/wx.h>
#include <wx/socket.h>

//File poll timer checks for files appearing after completion of a recording
#define FILE_POLL_TIMER_INTERVAL 250 //ms
#define MAX_SPEED 64 //times normal; must be power of 2
#define TRAFFIC_CONTROL_PORT 2000

DECLARE_EVENT_TYPE(EVT_PLAYER_MESSAGE, -1)
WX_DECLARE_HASH_MAP(int, bool, wxIntegerHash, wxIntegerEqual, BoolHash);

enum PlayerEventType {
    STATE_CHANGE = 0,
    CUE_POINT,
    FRAME_DISPLAYED,
    NEW_MODE,
    NEW_FILESET,
    AT_START,
    WITHIN,
    AT_END,
    CLOSE_REQ,
    KEYPRESS,
    PROGRESS_BAR_DRAG,
    SPEED_CHANGE,
    IMAGE_CLICK,
    LOAD_NEXT_CHUNK,
    LOAD_PREV_CHUNK,
    LOAD_FIRST_CHUNK,
    EDIT_RATE,
    SOURCE_NAME,
};

enum PlayerOSDtype {
    OSD_OFF,
    CONTROL_TIMECODE,
    SOURCE_TIMECODE,
};

namespace PlayerState {
    enum PlayerState {
        PLAYING,
        PLAYING_BACKWARDS,
        PAUSED,
        STOPPED,
        CLOSED,
    };
}

enum PlayerMode {
    RECORDINGS = wxID_HIGHEST + 1, //used as control IDs
#ifndef DISABLE_SHARED_MEM_SOURCE
    ETOE,
#endif
    FILES,
};

enum PlayerOpenType {
    OPEN_RECORDINGS,
    OPEN_MXF,
    OPEN_MOV,
};

class Player;
class SelectRecDlg;
class DragButtonList;
class ChunkInfo;

/// Class with methods that are called by the player.
class Listener : public prodauto::IngexPlayerListener
{
    public:
        Listener(Player *, prodauto::IngexPlayerListenerRegistry *);
        virtual void frameDisplayedEvent(const FrameInfo* frameInfo);
        virtual void frameDroppedEvent(const FrameInfo* lastFrameInfo);
        virtual void stateChangeEvent(const MediaPlayerStateEvent* event);
        virtual void endOfSourceEvent(const FrameInfo* lastReadFrameInfo);
        virtual void startOfSourceEvent(const FrameInfo* lastReadFrameInfo);
        virtual void playerClosed();
        virtual void playerCloseRequested();
        virtual void keyPressed(int, int);
        virtual void keyReleased(int, int);
        virtual void progressBarPositionSet(float);
        virtual void mouseClicked(int, int, int, int);
        virtual void sourceNameChangeEvent(int, const char*);
    private:
        Player * mPlayer; //never changed so doesn't need protecting by mutex
};

class SavedState;

/// Class representing the X11/SDI video/audio player.
class Player : public wxPanel, prodauto::LocalIngexPlayer
{
    public:
        Player(wxWindow*, const wxWindowID, const bool);
        ~Player();
        void SetSavedState(SavedState * savedState);
        void OnPlaybackTrackSelect(wxCommandEvent&);
        void Record(const bool = true);
        void SetMode(const PlayerMode, const bool = false);
        bool IsOK() { return mOK; };
        bool Enable(bool = true);
        bool IsEnabled() { return mEnabled; };
        void Open(const PlayerOpenType);
        std::vector<std::string>* SelectRecording(ChunkInfo*, const int = 0, const bool = false);
        bool EarlierTrack(const bool);
        bool LaterTrack(const bool);
        void SelectTrack(const int, const bool);
        void ChangeOSDType(const PlayerOSDtype);
        PlayerOSDtype GetOSDType();
        void Play(const bool = false, const bool = false);
        void PlayAbsolute(const int);
        void Pause();
        void Step(bool);
        void Reset();
        void JumpToFrame(const int64_t);
        void DivertKeyPresses(const bool state = true) { mDivertKeyPresses = state; };
        bool WithinRecording();
        bool AtRecordingEnd();
        bool LastPlayingBackwards() { return mLastPlayingBackwards; };
#ifdef HAVE_DVS
        bool ExtOutputIsAvailable();
#endif
        bool AtMaxForwardSpeed();
        bool AtMaxReverseSpeed();
        void MuteAudio(const bool);
        void EnableAudioFollowsVideo(const bool = true);
        bool IsAudioFollowingVideo();
        void LimitSplitToQuad(const bool = true);
        bool IsSplitLimitedToQuad();
        void DisableScalingFiltering(const bool = true);
        bool IsScalingFilteringDisabled();
#ifdef HAVE_DVS
        void EnableSDIOSD(const bool = true);
        bool IsSDIOSDEnabled();
#endif
        unsigned long GetLatestFrameDisplayed() { return mPreviousFrameDisplayed; };
        std::string GetCurrentFileName();
        const wxString GetProjectName();
        const wxString GetProjectType();
        DragButtonList* GetTrackSelector(wxWindow*);
        bool HasEarlierTrack();
        bool HasLaterTrack();
        void SelectEarlierTrack();
        void SelectLaterTrack();
        PlayerMode GetMode() { return mMode; };
        void SetPreviousMode();
        int GetSpeed() { return mSpeed; };
        bool ModeAllowed(PlayerMode mode) {return mModesAllowed[mode]; };
        bool IsMuted() { return mMuted; };
        void ChangeOutputType(const prodauto::PlayerOutputType);
        prodauto::PlayerOutputType GetOutputType(const bool = true);
        bool IsShowingSplit();
        void ChangeNumLevelMeters(const unsigned int);
        unsigned int GetNumLevelMeters();
        const wxString GetOutputTypeLabel(const prodauto::PlayerOutputType outputType);
    private:
        void OnModeButton(wxCommandEvent&);
        void OnUpdateUI(wxUpdateUIEvent&);
        void Load();
        bool Start();
        void SetWindowName(const wxString & name = wxEmptyString);
        void OnFrameDisplayed(wxCommandEvent&);
        void OnFilePollTimer(wxTimerEvent&);
        void OnStateChange(wxCommandEvent&);
        void OnSpeedChange(wxCommandEvent&);
        void OnProgressBarDrag(wxCommandEvent&);
        void OnKeyPress(wxCommandEvent&);
        void OnImageClick(wxCommandEvent&);
        void OnSocketEvent(wxSocketEvent&);
        void OnSourceNameChange(wxCommandEvent&);
        void TrafficControl(const bool, const bool = false);
        bool HasChunkBefore();
        bool HasChunkAfter();
        void LoadRecording();
        void SetVideoSplit(const bool = true);
        void SetApplyScaleFilter();
#ifdef HAVE_DVS
        void SetSDIOSDEnable();
#endif
        void SetOutputType();
        void SetOSDType();
        void SetNumLevelMeters();
        void SetAudioFollowsVideo();
        prodauto::PlayerOutputType Fallback(const prodauto::PlayerOutputType);
        const wxString GetOutputTypeDescription(const prodauto::PlayerOutputType);
        Listener * mListener;
        prodauto::IngexPlayerListenerRegistry mListenerRegistry;
        DragButtonList* mTrackSelector;
        wxWindow* mParent;
        bool mEnabled;
        bool mOK;
        PlayerState::PlayerState mState;
        std::string mDesiredTrackName;
        unsigned int mNFilesExisting;
        std::vector<std::string> mFileNames;
        std::vector<std::string> mTrackNames;
        std::vector<bool> mOpened;
        unsigned int mLastCuePointNotified;
        long mPreviousFrameDisplayed;
        bool mAtChunkStart;
        bool mAtChunkEnd;
        wxTimer * mFilePollTimer;
        int mSpeed;
        bool mMuted;
        bool mLastPlayingBackwards;
        wxSocketClient * mSocket;
        bool mOpeningSocket;
        bool mTrafficControl;
        bool mPrevTrafficControl;
        PlayerEventType mChunkLinking;
        PlayerMode mMode, mPreviousMode;
        wxArrayString mFileModeMxfFiles;
        unsigned int mFilesModeSelectedTrack;
        wxString mFileModeMovFile;
        int64_t mFileModeFrameOffset;
        int64_t mRecordingModeFrameOffset;
        SelectRecDlg * mSelectRecDlg;
        BoolHash mModesAllowed;
        ChunkInfo* mCurrentChunkInfo;
        prodauto::PlayerInputType mInputType;
        bool mDivertKeyPresses;
        unsigned int mNVideoTracks;
        bool mRecording;
        bool mPrematureStart;
#ifdef HAVE_DVS
        bool mUsingDVSCard;
#endif
        SavedState * mSavedState;
    DECLARE_EVENT_TABLE()
};

#endif
