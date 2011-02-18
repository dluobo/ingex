/***************************************************************************
 *   $Id: recordergroup.h,v 1.16 2011/02/18 16:31:15 john_f Exp $         *
 *                                                                         *
 *   Copyright (C) 2006-2010 British Broadcasting Corporation              *
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

#ifndef _RECORDERGROUP_H_
#define _RECORDERGROUP_H_
#include <wx/wx.h>
#include "controller.h"
#include "ingexgui.h"

#define DESELECTED_BACKGROUND_COLOUR wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)
#define DESELECTED_TEXT_COLOUR wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)
#define SELECTED_BACKGROUND_COLOUR wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)
#define SELECTED_TEXT_COLOUR wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT)

DECLARE_EVENT_TYPE(EVT_RECORDERGROUP_MESSAGE, -1)

class Comms;
class wxXmlDocument;
class SavedState;

/// A control derived from a list box containing a list of recorders, connected and disconnected.
class RecorderGroupCtrl : public wxListView
{
    public:
        RecorderGroupCtrl(wxWindow *, wxWindowID id, const wxPoint &, const wxSize &, int&, char**);
        ~RecorderGroupCtrl();
        void SetTree(TickTreeCtrl * tree) {mTree = tree;};
        void SetSavedState(SavedState * savedState) { mSavedState = savedState; };
        void StartGettingRecorders();
        void SetPreroll(const ProdAuto::MxfDuration);
        void SetPostroll(const ProdAuto::MxfDuration);
        const ProdAuto::MxfDuration GetPreroll();
        const ProdAuto::MxfDuration GetPostroll();
        const ProdAuto::MxfDuration GetMaxPreroll();
        const ProdAuto::MxfDuration GetMaxPostroll();
        const ProdAuto::MxfDuration GetChunkingPostroll();
        void SetTapeIds(const wxString &, const CORBA::StringSeq &, const CORBA::StringSeq &);
        void Record(const ProdAuto::MxfTimecode);
        void Stop(const bool, const ProdAuto::MxfTimecode &, const wxString &, const ProdAuto::LocatorSeq &);
        void ChunkStop(const ProdAuto::MxfTimecode &, const wxString &, const ProdAuto::LocatorSeq &);
        void EnableForInput(bool state = true) { mEnabledForInput = state; };
        bool IsEnabledForInput() { return mEnabledForInput; };
        void SetProjectNames(const wxSortedArrayString &);
        const wxSortedArrayString & GetProjectNames();
        void SetCurrentProjectName(const wxString &);
        const wxString & GetCurrentProjectName();
        const wxString & GetCurrentDescription();
        void Deselect(const int);
        const wxString& GetTimecodeRecorder() { return mTimecodeRecorder; };
        enum RecorderGroupCtrlEventType {
            ENABLE_REFRESH,
            NEW_RECORDER,
            REMOVE_RECORDER,
            RECORD,
            RECORDER_STARTED,
            STOP,
            RECORDER_STOPPED,
            DISPLAY_TIMECODE_SOURCE,
            DISPLAY_TIMECODE,
            DISPLAY_TIMECODE_STUCK,
            DISPLAY_TIMECODE_MISSING,
            TIMECODE_RUNNING,
            TIMECODE_STUCK,
            TIMECODE_MISSING,
            COMM_FAILURE,
            CHUNK_START,
            CHUNK_END,
            SET_TRIGGER,
        };
    private:
        void OnListRefreshed(wxCommandEvent &);
        void OnLMouseDown(wxMouseEvent &);
        void OnRightMouseDown(wxMouseEvent &);
        void OnUnwantedMouseDown(wxMouseEvent &);
        void OnControllerEvent(ControllerThreadEvent &);
        void OnTimeposEvent(wxCommandEvent &);
        const wxString GetName(const int);
        void BeginConnecting(const int);
        unsigned int GetNumberRecordersConnected();
        void Disconnect(const int);
        void SetTimecodeRecorder(wxString name = wxEmptyString);
        void SetRecTimeAvailable(const int, const long);
        Comms * mComms;
        bool mEnabledForInput;
        ProdAuto::MxfDuration mMaxPreroll, mMaxPostroll, mPreroll, mPostroll;
        wxString mTimecodeRecorder;
        bool mTimecodeRecorderStuck;
        ProdAuto::MxfTimecode mChunkStartTimecode;
        SavedState * mSavedState;
        wxSortedArrayString mProjectNames;
        wxString mCurrentProject;
        wxString mCurrentDescription;
        enum Mode {
            STOPPED,
            STOP_WAIT,
            RECORD_WAIT,
            RECORDING,
            CHUNK_STOP_WAIT,
            CHUNK_WAIT,
            CHUNK_RECORD_WAIT,
            CHUNK_RECORDING,
        };
        Mode mMode;
        bool mHaveNonRouterRecorders;
        TickTreeCtrl * mTree;
        DECLARE_EVENT_TABLE()
};

/// Class storing various information about a recorder, to be attached to events from the recorder group.
/// Various constructors for different information; all data constant.
class RecorderData
{
    public:
        RecorderData(const ProdAuto::TrackStatusList_var & trackStatusList, const ProdAuto::TrackList_var & trackList) : mTrackStatusList(trackStatusList), mTrackList(trackList) {};
        RecorderData(const ProdAuto::TrackStatusList_var & trackStatusList) : mTrackStatusList(trackStatusList) {};
        RecorderData(ProdAuto::MxfTimecode tc) : mTimecode(tc) {};
        RecorderData(ProdAuto::TrackList_var trackList, CORBA::StringSeq_var fileList, ProdAuto::MxfTimecode tc = InvalidMxfTimecode) : mTrackList(trackList), mFileList(fileList), mTimecode(tc) {};
        const ProdAuto::TrackList_var & GetTrackList() {return mTrackList;};
        const ProdAuto::TrackStatusList_var & GetTrackStatusList() {return mTrackStatusList;};
        CORBA::StringSeq_var GetFileList() {return mFileList;};
        ProdAuto::MxfTimecode GetTimecode() {return mTimecode;};
    private:
        ProdAuto::TrackStatusList_var mTrackStatusList;
        ProdAuto::TrackList_var mTrackList;
        CORBA::StringSeq_var mFileList;
        ProdAuto::MxfTimecode mTimecode;
};

#endif
