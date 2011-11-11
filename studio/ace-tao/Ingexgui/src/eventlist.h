/***************************************************************************
 *   $Id: eventlist.h,v 1.17 2011/11/11 11:21:23 john_f Exp $             *
 *                                                                         *
 *   Copyright (C) 2009-2011 British Broadcasting Corporation              *
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

WX_DEFINE_ARRAY_INT(ProdAuto::TrackType, ArrayOfTrackTypes);
WX_DECLARE_OBJARRAY(ProdAuto::TrackList_var, ArrayOfTrackList_var);
WX_DECLARE_OBJARRAY(CORBA::StringSeq_var, ArrayOfStringSeq_var);

class RecorderData;
class wxSocketEvent;
class SavedState;

/// Class to display and store the list of recording events
class EventList : public wxListView, wxThread //used wxListCtrl for a while because wxListView crashed when you added an item - seems ok with 2.8
{
    public:
        EventList(wxWindow *, wxWindowID, const wxPoint & = wxDefaultPosition, const wxSize & = wxDefaultSize, bool = true, const wxString & = wxEmptyString);
        ~EventList();
        enum EventType //NB order must match TypeLabels[] initialisation
        {
            NONE = 0,
            START,
            CUE,
            CHUNK,
            STOP,
            PROBLEM
        };
        void AddEvent(EventType, ProdAuto::MxfTimecode *, const int64_t = 0, const wxString & = wxEmptyString, const size_t = 0, const bool = true, const wxString & = wxEmptyString);
        void AddRecorderData(RecorderData * data, bool = true);
        void DeleteCuePoint();
        void Select(const long, const bool = false);
        void SelectPrevTake(const bool);
        void SelectAdjacentEvent(const bool);
        void SelectNextTake();
        void SelectLastTake();
        void SetSavedState(SavedState * savedState);
        void CanEdit(const bool);
        bool LatestChunkCuePointIsSelected();
        bool AtTop();
        bool AtStartOfTake();
        bool InLastTake();
        bool AtBottom();
        bool ChunkBeforeSelectedChunk();
        bool ChunkAfterSelectedChunk();
        bool ListIsEmpty();
        ProdAuto::MxfTimecode GetEditRate() {return mEditRate;};
        ProdAuto::LocatorSeq GetLocators();
        int64_t GetSelectedCuePointOffset();
        unsigned long GetSelectedChunkStartIndex();
        std::vector<unsigned long long> GetSelectedChunkCuePointOffsets();
        int64_t GetSelectedChunkStartPosition();
        void Clear();
        bool JumpToTimecode(const ProdAuto::MxfTimecode &, int64_t &);
        bool FilesHaveChanged();
        const wxString GetSelectedProjectName();
        const wxArrayString GetSelectedFiles(ArrayOfTrackTypes * = 0, wxArrayString * = 0);
        void SetMaxChunks(const unsigned long);
        unsigned long GetMaxChunks();
    private:
        void OnEventBeginEdit(wxListEvent&);
        void OnEventEndEdit(wxListEvent&);
        void OnRestoreListLabel(wxCommandEvent&);
        void OnEventSelection(wxListEvent&);
        void OnSocketEvent(wxSocketEvent&);
        ExitCode Entry();
        void LoadDocument();
        void SaveDocument();
        wxXmlNode * FindChildNodeByName(wxXmlNode *, const wxString &, bool = false);
        unsigned long long GetNumericalPropVal(const wxXmlNode *, const wxString &, const unsigned long long);
        const wxString GetCdata(const wxXmlNode *);
        void SetItemAppearance(wxListItem &, const EventType, const size_t = 0);
        wxXmlNode * SetNextChild(wxXmlNode *, wxXmlNode *, wxXmlNode *);
        wxXmlNode *  NewChunk(ProdAuto::MxfTimecode *, int64_t, const wxString & = wxEmptyString);
        void LimitListSize();
        ProdAuto::MxfTimecode GetStartTimecode();
        bool RecordingIsSelected();
        wxXmlNode * GetNode(const long);
        wxXmlNode * GetSelectedNode();
        wxXmlNode * GetRecordingNode(wxXmlNode * = 0);
        wxXmlNode * GetCreatingChunkNode();

        long mBlockEventItem;
        wxMutex mMutex;
        //vbls which need mutex protection to access
        wxCondition * mCondition;
        wxXmlNode * mRootNode;
        bool mRunThread;
        bool mSyncThread;
        wxString mFilename;
        ProdAuto::MxfTimecode mEditRate;
        wxString mSocketData;
        wxXmlNode * mSelectedChunkNode;
        bool mFilesHaveChanged;
        unsigned int mTotalChunks;
        SavedState * mSavedState;
    DECLARE_EVENT_TABLE()
};

#endif
