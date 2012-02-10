/***************************************************************************
 *   $Id: dialogues.h,v 1.20 2012/02/10 15:12:55 john_f Exp $             *
 *                                                                         *
 *   Copyright (C) 2006-2012 British Broadcasting Corporation              *
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

#ifndef _DIALOGUES_H_
#define _DIALOGUES_H_
#include <wx/wx.h>
#include <wx/grid.h>
#include <vector>
#include "ingexgui.h"
#include "timepos.h"
#include <list>
#include <wx/hashset.h>

/// Set preroll and postroll
class SetRollsDlg : public wxDialog
{
    public:
        SetRollsDlg(wxWindow *, const ProdAuto::MxfDuration, const ProdAuto::MxfDuration, const ProdAuto::MxfDuration, const ProdAuto::MxfDuration, SavedState *);
        int ShowModal();
        const ProdAuto::MxfDuration GetPreroll();
        const ProdAuto::MxfDuration GetPostroll();
    private:
        ProdAuto::MxfDuration SetRoll(const wxChar *, int, const ProdAuto::MxfDuration &, wxStaticBoxSizer *);
        void OnRollChange( wxScrollEvent& event );
        wxSlider * mPrerollCtrl;
        wxSlider * mPostrollCtrl;
        wxStaticBoxSizer * mPrerollBox;
        wxStaticBoxSizer * mPostrollBox;
        ProdAuto::MxfDuration mMaxPreroll;
        ProdAuto::MxfDuration mMaxPostroll;
        SavedState * mSavedState;

    enum
    {
        SLIDER_Preroll,
        SLIDER_Postroll,
    };

    DECLARE_EVENT_TABLE()
};

class wxGrid;
class wxGridEvent;
class SavedState;

/// Set project names.
class SetProjectDlg : public wxDialog
{
    public:
        SetProjectDlg(wxWindow *);
        void SetProjectNames(CORBA::StringSeq_var = 0, const wxString & = wxEmptyString);
        const wxString GetSelectedProject();
        const CORBA::StringSeq GetNewProjectNames();
        const wxString GetCurrentProjectName();
    private:
        enum {
            ADD = wxID_HIGHEST + 1,
            DELETE,
            EDIT,
        };
        void OnAdd(wxCommandEvent&);
        void OnDelete(wxCommandEvent&);
        void OnEdit(wxCommandEvent&);
        void OnChoice(wxCommandEvent&);
        void EnterName(const wxString &, const wxString &, int = wxNOT_FOUND);
        void EnableButtons(bool);

//      wxListBox * mProjectList;
        wxButton * mAddButton;
        wxButton * mDeleteButton;
        wxButton * mEditButton;
        wxButton * mOKButton;
        wxChoice * mProjectList;
        wxArrayString mProjectNames; //needed because there is no sort style for a wxChoice; was using wxSortedArrayString but can't add this to a wxControlWithItems in 2.8.11.
        wxArrayString mNewProjectNames;
    DECLARE_EVENT_TABLE()
};

/// A wxGrid which passes on character events - currently these aren't being used because they don't seem to be passed on.
class MyGrid : public wxGrid
{
    public:
        MyGrid(wxWindow * parent, wxWindowID id, const wxPoint & pos = wxDefaultPosition, const wxSize & size = wxDefaultSize, long style = wxWANTS_CHARS, const wxString & name = wxPanelNameStr) : wxGrid(parent, id, pos, size, style, name) {};
    private:
        void OnChar(wxKeyEvent & event) { /* std::cerr << "grid char" << std::endl; ---this works*/
event.Skip(); };
    DECLARE_EVENT_TABLE()
};

class wxXmlNode;

/// Set tape IDs.

#define TAPE_IDS_NODE_NAME wxT("TapeIds")

class SetTapeIdsDlg : public wxDialog
{
    public:
        SetTapeIdsDlg(wxWindow *, SavedState *, wxArrayString &, std::vector<bool> &);
        static const wxString GetTapeId(SavedState *, const wxString &, wxArrayString * = 0);
        static void EnableTapeIds(SavedState *, bool);
        static bool AreTapeIdsEnabled(SavedState *);
        bool IsUpdated();
    private:
        enum {
            FILLINC = wxID_HIGHEST + 1,
            FILLCOPY,
            INCREMENT,
            GROUPINC,
            HELP,
            CLEAR,
        };
        void OnInitDlg(wxInitDialogEvent &);
        void OnEditorShown(wxGridEvent &);
        void OnEditorHidden(wxGridEvent &);
        void OnCellChange(wxGridEvent &);
        void OnCellRangeSelected(wxGridRangeSelectEvent &);
        void OnCellLeftClick(wxGridEvent &);
        void OnLabelLeftClick(wxGridEvent &);
        void OnIncrement(wxCommandEvent &);
        void OnGroupIncrement(wxCommandEvent &);
        void OnFillCopy(wxCommandEvent &);
        void OnFillInc(wxCommandEvent &);
        void OnHelp(wxCommandEvent &);
        void OnClear(wxCommandEvent &);
        bool ManipulateCells(const bool, const bool);
#if USING_ULONGLONG
        bool ManipulateCell(const int, const int, const bool, const bool, wxULongLong_t * = 0);
#else
        bool ManipulateCell(const int, const int, const bool, const bool, unsigned long * = 0);
#endif
        void FillCol(const bool, const bool);
        void UpdateRow(const int);
        void IncrementAsGroup(const bool commit);
        void SetBackgroundColour(int);
        void CheckForDuplicates();

        MyGrid * mGrid;
        wxButton * mIncrementButton;
        wxButton * mFillCopyButton;
        wxButton * mFillIncButton;
        wxButton * mGroupIncButton;
        wxButton * mClearButton;
        wxTextCtrl * mDupWarning;

        std::vector<bool> mEnabled;

        bool mEditing;
        bool mUpdated;

    DECLARE_EVENT_TABLE()
};

/// Set a timecode to jump to

DECLARE_EVENT_TYPE(EVT_JUMP_TO_TIMECODE, -1)

class JumpToTimecodeDlg : public wxDialog
{
    public:
        JumpToTimecodeDlg(wxWindow *);
        void Show(ProdAuto::MxfTimecode);
        const ProdAuto::MxfTimecode GetTimecode();
        void Hide();
        void NotFoundMessage();
    private:
        enum //these must be in order
        {
            HRS = wxID_HIGHEST + 1,
            MINS,
            SECS,
            FRAMES,
        };
        void OnTextChange(wxCommandEvent &);
        void OnFocus(wxFocusEvent &);
        void OnButton(wxCommandEvent &);
        bool CheckValue(const wxString &, const wxString &);
        MyTextCtrl * mHours, * mMins, * mSecs, * mFrames;
        wxButton * mApplyButton;
        wxStaticText * mNotFoundMessage;
        ProdAuto::MxfTimecode mTimecode;
    DECLARE_EVENT_TABLE()
};

#define TEST_MAX_REC 180 //minutes
#define TEST_MAX_GAP 900 //seconds

WX_DECLARE_HASH_MAP(time_t, wxArrayString*, wxIntegerHash, wxIntegerEqual, HashOfArrayStrings);

struct DirContents {
    std::list<time_t> mtimes;
    HashOfArrayStrings files;
};

WX_DECLARE_HASH_MAP(wxString, DirContents*, wxStringHash, wxStringEqual, HashOfDirContents);

class wxSpinCtrl;
class wxSpinEvent;
class wxToggleButton;

/// Enable and set parameters for test mode
class TestModeDlg : public wxDialog
{
    public:
        TestModeDlg(wxWindow *, const int, const int);
        int ShowModal();
        ~TestModeDlg();
        void AddPaths(const wxArrayString &);
        enum
        {
            RECORD,
            STOP,
        };
    private:
        enum
        {
            MIN_REC = wxID_HIGHEST + 1,
            MAX_REC,
            REC_RANDOM,
            MIN_GAP,
            MAX_GAP,
            ERASE_THRESH,
            GAP_RANDOM,
            RUN,
            RANDOM
        };
        void OnModeChange(wxCommandEvent &);
        void OnChangeMinRecTime(wxSpinEvent &);
        void OnChangeMaxRecTime(wxSpinEvent &);
        void OnChangeMinGapTime(wxSpinEvent &);
        void OnChangeMaxGapTime(wxSpinEvent &);
        void OnChangeEraseThreshold(wxSpinEvent &);
        void OnRun(wxCommandEvent &);
        void OnTimer(wxTimerEvent &);
        void OnEraseEnable(wxCommandEvent &);
        void Record(bool rec = true);
        void EnableModeButtons(bool enable = true);
        void UpdateCountdownDisplay();
        void Erase();
        wxSpinCtrl * mMinRecTime;
        wxSpinCtrl * mMaxRecTime;
        wxSpinCtrl * mMinGapTime;
        wxSpinCtrl * mMaxGapTime;
        wxCheckBox * mEraseEnable;
        wxSpinCtrl * mEraseThreshold;
        wxToggleButton * mRunButton;
        wxStaticText * mRunStopMessage;
        wxStaticText * mRunStopCountdown;
        wxStaticText * mEraseMessage;
        wxButton * mCancelButton;
        wxTimer * mCountdownTimer;
        wxTimer * mEraseTimer;
        wxTimeSpan mCountdown;
        bool mRecording;
        const int mRecordId;
        const int mStopId;
        HashOfDirContents mDirInfo;
    DECLARE_EVENT_TABLE()
};

#define N_CUE_POINT_COLOURS 9 //including default

/// Set cue point descriptions
class CuePointsDlg : public wxDialog
{
    public:
        CuePointsDlg(wxWindow *, SavedState *);
        int ShowModal(const wxString = wxEmptyString);
        void Shortcut(const int);
        void Scroll(const bool);
        const wxString GetDescription();
        size_t GetColourIndex();
        static ProdAuto::LocatorColour::EnumType GetColourCode(const size_t);
        bool ValidCuePointSelected();
        static const wxColour GetColour(const size_t);
        static const wxColour GetLabelColour(const size_t);
        bool DefaultCueMode();
    private:
        void OnEditorShown(wxGridEvent&);
        void OnEditorHidden(wxGridEvent&);
        void Load();
        void Save();
        void OnMenu(wxCommandEvent&);
        void OnOK(wxCommandEvent&);
        void OnCellLeftClick(wxGridEvent&);
        void OnSetGridRow(wxCommandEvent&);
        void OnLabelLeftClick(wxGridEvent&);
        void OnRadioButton(wxCommandEvent&);
        void SetColour(const int, const int);

        wxRadioButton * mSingleTypeRadioButton;
        wxRadioButton * mMultipleTypeRadioButton;
        wxGrid * mGrid;
        wxButton * mOkButton;
        wxStaticText * mTimecodeDisplay;
        wxStaticText * mMessage;

        SavedState * mSavedState;
        wxColour mTextColour;
        wxColour mBackgroundColour;
        int mCurrentRow;
        int mDescrColours[10];
    DECLARE_EVENT_TABLE()
};

#endif

/// Controls automatic chunking.  Active when not visible.  Changes when visible take immediate effect (i.e. no possibility to cancel)
class ChunkingDlg : public wxDialog
{
    public:
        ChunkingDlg(wxWindow *, Timepos *, SavedState *);
        int ShowModal();
        void RunFrom(const ProdAuto::MxfTimecode & = InvalidMxfTimecode, const ProdAuto::MxfDuration & = InvalidMxfDuration, const bool = true);
        void Realign();
        bool CanChunk() { return mCanChunk; };
        const wxString GetChunkButtonLabel();
        const wxString GetChunkButtonToolTip();
        const wxColour GetChunkButtonColour();
    private:
        void OnChangeChunkSize(wxSpinEvent &);
        void OnChangeChunkAlignment(wxCommandEvent &);
        void OnEnable(wxCommandEvent &);
        void OnTimer(wxTimerEvent &);
        void SetCountdownLabel();
        void Reset();

        wxSpinCtrl * mChunkSizeCtrl;
        wxChoice * mChunkAlignCtrl;
        wxCheckBox * mEnableCheckBox;
        wxButton * mChunkButton;
        wxTimer * mCountdownTimer;
        Timepos * mTimepos;
        SavedState * mSavedState;
        unsigned int mCountdown;
        unsigned long mChunkLength;
        int mChunkAlignment;
        bool mCanChunk;
        ProdAuto::MxfDuration mPostroll;
        bool mRealign;
    DECLARE_EVENT_TABLE()
};

class SelectRecDlg : public wxDialog
{
    public:
        SelectRecDlg(wxWindow *, const wxString &);
        int ShowModal();
        void GetPaths(wxArrayString &, bool = false);
    private:
        void OnSelectProject(wxCommandEvent &);
        void OnSelectDate(wxCommandEvent &);
        void OnSelectRecording(wxCommandEvent &);
        void OnDoubleClick(wxCommandEvent &);
        void OnPreferOnline(wxCommandEvent &);
        void OnNavigateRecordings(wxCommandEvent &);
        wxChoice * mProjectSelector;
        wxChoice * mDateSelector;
        wxListBox * mRecordingSelector;
        wxBitmapButton * mUpButton;
        wxBitmapButton * mDownButton;
        wxStaticText * mOnlineMessage;
        wxToggleButton * mPreferOnline;
        wxString mCategoryDir;
        wxString mSelectedProject;
        wxString mSelectedDate;
        wxString mSelectedRecording;
        const wxString mRoot;
        bool mOfflineDir;
        enum {
            PROJECT = wxID_HIGHEST + 1,
            DATE,
            RECORDING,
            UP,
            DOWN,
            PREFER_ONLINE
        };
    DECLARE_EVENT_TABLE()
};

class SetMaxChunksDlg : public wxDialog
{
    public:
        SetMaxChunksDlg(wxWindow *, unsigned int);
        unsigned int GetMaxChunks();
    private:
        wxSpinCtrl * mSpinCtrl;
};

/// Class storing a constant string, to be attached to each entry in a choice control.
class StringContainer : public wxClientData
{
    public:
        StringContainer(const wxString & data) : mData(data) {};
        const wxString GetString() { return mData; };
    private:
        const wxString mData;
};
