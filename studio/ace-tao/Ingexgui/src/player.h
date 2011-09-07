/***************************************************************************
 *   $Id: player.h,v 1.23 2011/09/07 15:07:08 john_f Exp $                *
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

#ifndef PLAYER_H_
#define PLAYER_H_

#include <vector>
#include <string>
#include <map>
#ifdef USE_HTTP_PLAYER
#include "HTTPPlayerClient.h"
#else
#include "LocalIngexPlayer.h"
#endif //USE_HTTP_PLAYER
#include <wx/wx.h>
#include <wx/socket.h>

#ifdef USE_HTTP_PLAYER
#define HTTP_PLAYER_HOST "localhost"
#define HTTP_PLAYER_PORT 9008
#define HTTP_PLAYER_RETRY_TIMER_INTERVAL 400 //ms - must be greater than HTTP_PLAYER_STATE_POLL_INTERVAL
#define HTTP_PLAYER_STATE_POLL_INTERVAL 200 //ms - must be less than HTTP_PLAYER_RETRY_TIMER_INTERVAL
#endif //USE_HTTP_PLAYER


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
    SOURCE_NAME,
};

enum PlayerOSDtype {
    OSD_OFF,
    CONTROL_TIMECODE,
    SOURCE_TIMECODE,
};

enum PlayerOSDposition {
    OSD_TOP,
    OSD_MIDDLE,
    OSD_BOTTOM,
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
    OPEN_FROM_SERVER,
    OPEN_MXF,
    OPEN_MOV,
};

class Player;
class SelectRecDlg;
class DragButtonList;
class ChunkInfo;


/// Class with methods that are called by the player.
#ifdef USE_HTTP_PLAYER
class Listener : public ingex::HTTPPlayerClientListener
#else
class Listener : public prodauto::IngexPlayerListener
#endif //USE_HTTP_PLAYER
{
    public:
#ifdef USE_HTTP_PLAYER
        Listener(Player *);
        virtual void stateUpdate(ingex::HTTPPlayerClient*, ingex::HTTPPlayerState);
#else
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
#endif //NOT USE_HTTP_PLAYER
    private:
        Player * mPlayer; //never changed so doesn't need protecting by mutex
        void ProcessFrameDisplayedEvent(const FrameInfo*);
        void ProcessStateChangeEvent(const bool, const bool, const bool, const bool, const bool, const int);
#ifdef USE_HTTP_PLAYER
        ingex::HTTPPlayerState mPreviousState;
#else
#endif //NOT USE_HTTP_PLAYER
};

class SavedState;

/// Class representing the X11/SDI video/audio player.
#ifdef USE_HTTP_PLAYER
class Player : public wxPanel, ingex::HTTPPlayerClient
#else
class Player : public wxPanel, prodauto::LocalIngexPlayer
#endif //NOT USE_HTTP_PLAYER
{
    public:
        Player(wxWindow*, const wxWindowID, const bool, const wxString &);
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
        void ChangeOSDPosition(const PlayerOSDposition);
        PlayerOSDposition GetOSDPosition();
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
        bool ExtOutputIsAvailable();
        bool AtMaxForwardSpeed();
        bool AtMaxReverseSpeed();
        void MuteAudio(const bool);
        void EnableAudioFollowsVideo(const bool = true);
        bool IsAudioFollowingVideo();
        void LimitSplitToQuad(const bool = true);
        bool IsSplitLimitedToQuad();
        void DisableScalingFiltering(const bool = true);
        bool IsScalingFilteringDisabled();
        void EnableSDIOSD(const bool = true);
        bool IsSDIOSDEnabled();
        unsigned long GetLatestFrameDisplayed() { return mPreviousFrameDisplayed; };
        std::string GetCurrentFileName();
        const wxString GetPlaybackName();
        const wxString GetPlaybackType();
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
        void ChangeNumLevelMeters(const unsigned int);
        unsigned int GetNumLevelMeters();
        const wxString GetOutputTypeLabel(const prodauto::PlayerOutputType outputType);
    private:
        void OnModeButton(wxCommandEvent&);
        void OnUpdateUI(wxUpdateUIEvent&);
        void Setup();
        void Load();
        bool Start();
        void SetWindowName(const wxString & name = wxEmptyString);
        void OnFrameDisplayed(wxCommandEvent&);
        void OnTimer(wxTimerEvent&);
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
        void SetSDIOSDEnable();
        void SetOutputType();
        void SetOSDType();
        void SetOSDPosition();
        void SetNumLevelMeters();
        void SetAudioFollowsVideo();
        prodauto::PlayerOutputType Fallback(const prodauto::PlayerOutputType);
        const wxString GetOutputTypeDescription(const prodauto::PlayerOutputType);
        void Connect();
        Listener * mListener;
#ifndef USE_HTTP_PLAYER
        prodauto::IngexPlayerListenerRegistry mListenerRegistry;
#endif // !USE_HTTP_PLAYER
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
        wxTimer * mConnectRetryTimer;
        bool mConnected;
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
        int mNVideoTracks;
        bool mRecording;
        bool mPrematureStart;
        bool mUsingDVSCard;
        SavedState * mSavedState;
        const wxString mRecServerRoot;
    DECLARE_EVENT_TABLE()
};

#endif
