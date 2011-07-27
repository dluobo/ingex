/***************************************************************************
 *   $Id: eventlist.h,v 1.14 2011/07/27 17:08:36 john_f Exp $             *
 *                                                                         *
 *   Copyright (C) 2009-2011 British Broadcasting Corporation                   *
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

#ifndef EVENTLIST_H_
#define EVENTLIST_H_
#include <vector>
#include "RecorderC.h"
#include "ingexgui.h"
#include <wx/xml/xml.h>

WX_DECLARE_OBJARRAY(ProdAuto::TrackList_var, ArrayOfTrackList_var);
WX_DECLARE_OBJARRAY(CORBA::StringSeq_var, ArrayOfStringSeq_var);

/// Class holding information about a chunk, for later replay.
/// Holds the position of the chunk within the displayed list of events, the project name, an array of lists of files
/// (one for each recorder) and an array of lists of track names (ditto).
/// Also arrays of cue point colour indeces, timecodes and frame numbers.
class ChunkInfo
{
    public:
        ChunkInfo(const unsigned long startIndex, const wxString & projectName, const ProdAuto::MxfTimecode & startTimecode, const int64_t startPosition, const bool hasChunkBefore) : mStartIndex(startIndex), mProjectName(projectName), mStartTimecode(startTimecode), mLastTimecode(InvalidMxfTimecode), mStartPosition(startPosition), mHasChunkBefore(hasChunkBefore), mHasChunkAfter(false) {};
        void SetHasChunkAfter() {mHasChunkAfter = true;};
        void AddRecorder(ProdAuto::TrackList_var trackList, CORBA::StringSeq_var fileList) {mFiles.Add(fileList); mTracks.Add(trackList);}; //adds a set of tracks provided by a recorder at the end of a recording
        unsigned long AddCuePoint(const int64_t frame, size_t colourIndex) { //no text here because it's stored in the event list (which means it can be edited)
            unsigned int i = mCuePointFrames.size();
            while (i > 0 && mCuePointFrames[i - 1] > frame) i--; //cue point is not necessarily added to the end
            std::vector<int64_t>::iterator it = mCuePointFrames.begin() + i;
            mCuePointFrames.insert(it, frame);
            std::vector<size_t>::iterator it2 = mCueColourIndeces.begin() + i;
            mCueColourIndeces.insert(it2, colourIndex);
            return mStartIndex + i + 1; //+1 for start event
        };
        void SetLastTimecode(const ProdAuto::MxfTimecode & lastTimecode) {mLastTimecode = lastTimecode;};
        bool DeleteCuePoint(const unsigned long index) {
            if (mCuePointFrames.size() > index) {
                mCuePointFrames.erase(mCuePointFrames.begin() + index); mCueColourIndeces.erase(mCueColourIndeces.begin() + index);
                return true;
            }
            else {
                return false;
            }
        };
        unsigned long GetStartIndex() {return mStartIndex;};
        ProdAuto::MxfTimecode GetStartTimecode() {return mStartTimecode;};
        ProdAuto::MxfTimecode GetLastTimecode() {return mLastTimecode;};
        ArrayOfStringSeq_var * GetFiles() {return &mFiles;};
        const wxString GetProjectName() {return mProjectName;};
        ArrayOfTrackList_var & GetTracks() {return mTracks;};
        std::vector<int64_t> & GetCuePointFrames() {return mCuePointFrames;};
        std::vector<size_t> & GetCueColourIndeces() {return mCueColourIndeces;};
        int64_t GetStartPosition() {return mStartPosition;};
        bool HasChunkBefore() {return mHasChunkBefore;};
        bool HasChunkAfter() {return mHasChunkAfter;};
    private:
        const unsigned long mStartIndex; //the index in the event list for the start of this chunk
        ArrayOfStringSeq_var mFiles;
        const wxString mProjectName;
        const ProdAuto::MxfTimecode mStartTimecode; //of this chunk
        ProdAuto::MxfTimecode mLastTimecode; //of this chunk
        const int64_t mStartPosition; //relative to start of recording
        ArrayOfTrackList_var mTracks; //deletes itself
        std::vector<int64_t> mCuePointFrames; //relative to start of recording (i.e. not necessarily this chunk)
        std::vector<size_t> mCueColourIndeces; //of the cue points
        const bool mHasChunkBefore;
        bool mHasChunkAfter;
};

WX_DECLARE_OBJARRAY(ChunkInfo, ChunkInfoArray);

class RecorderData;
class wxSocketEvent;

/// Class to display and store the list of recording events
class EventList : public wxListView, wxThread //used wxListCtrl for a while because wxListView crashed when you added an item - seems ok with 2.8
{
    public:
        EventList(wxWindow *, wxWindowID, const wxPoint & = wxDefaultPosition, const wxSize & = wxDefaultSize, bool = true);
        enum EventType //NB order must match TypeLabels[] initialisation
        {
            NONE = 0,
            START,
            CUE,
            CHUNK,
            STOP,
            PROBLEM
        };
        void AddEvent(EventType, ProdAuto::MxfTimecode *, const wxString &, const int64_t = 0, const size_t = 0, const bool = true);
        void AddRecorderData(RecorderData * data, bool = true);
        void DeleteCuePoint();
        void Select(const long, const bool = false);
        void SelectPrevTake(const bool);
        void SelectAdjacentEvent(const bool);
        void SelectNextTake();
        void SelectLastTake();
        void CanEdit(const bool);
        bool LatestChunkCuePointIsSelected();
        bool AtTop();
        bool AtStartOfTake();
        bool InLastTake();
        bool AtBottom();
        bool SelectedChunkHasChanged();
        void Load();
        ProdAuto::MxfTimecode GetEditRate() {return mEditRate;};
        ProdAuto::LocatorSeq GetLocators();
        ChunkInfo * GetCurrentChunkInfo();
        int GetCurrentCuePoint();
        int64_t GetCurrentChunkStartPosition();
        void Clear();
        bool JumpToTimecode(const ProdAuto::MxfTimecode &, int64_t &);
    private:
        ExitCode Entry();
        void OnEventBeginEdit(wxListEvent&);
        void OnEventEndEdit(wxListEvent&);
        void OnRestoreListLabel(wxCommandEvent&);
        void OnEventSelection(wxListEvent&);
        void OnSocketEvent(wxSocketEvent&);
        void ClearSavedData();
        const wxString GetCdata(wxXmlNode *);
        void NewChunkInfo(ProdAuto::MxfTimecode *, int64_t, const wxString & = wxEmptyString);
        ProdAuto::MxfTimecode GetStartTimecode();

        ChunkInfoArray mChunkInfoArray;
        long mCanEditAfter;
        long mCurrentChunkInfo;
        long mBlockEventItem;
        wxMutex mMutex;
        unsigned int mRecordingNodeCount;
        bool mChunking;
        //vbls which need mutex protection to access
        wxCondition * mCondition;
        wxXmlNode * mRootNode;
        wxXmlNode * mRecordingNode;
        wxXmlNode * mPrevRecordingNode;
        bool mRunThread;
        bool mSyncThread;
        bool mLoadEventFiles;
        wxString mFilename;
        ProdAuto::MxfTimecode mEditRate;
        ProdAuto::MxfTimecode mRecStartTimecode;
        wxString mSocketData;
    DECLARE_EVENT_TABLE()
};

#endif
