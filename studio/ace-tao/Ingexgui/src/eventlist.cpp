/***************************************************************************
 *   $Id: eventlist.cpp,v 1.23 2011/09/12 09:41:48 john_f Exp $           *
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

#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/file.h>
#include <wx/socket.h>
#include "eventlist.h"
#include "ingexgui.h" //consts
#include "timepos.h"
#include "recordergroup.h"
#include "dialogues.h"

#define ROOT_NODE_NAME wxT("IngexguiEvents")
DEFINE_EVENT_TYPE (EVT_RESTORE_LIST_LABEL);

BEGIN_EVENT_TABLE( EventList, wxListView )
    EVT_LIST_ITEM_SELECTED( wxID_ANY, EventList::OnEventSelection )
//  EVT_LIST_ITEM_ACTIVATED( wxID_ANY, EventList::OnEventActivated )
    EVT_LIST_BEGIN_LABEL_EDIT( wxID_ANY, EventList::OnEventBeginEdit )
    EVT_LIST_END_LABEL_EDIT( wxID_ANY, EventList::OnEventEndEdit )
    EVT_COMMAND( wxID_ANY, EVT_RESTORE_LIST_LABEL, EventList::OnRestoreListLabel )
    EVT_SOCKET(wxID_ANY, EventList::OnSocketEvent)
END_EVENT_TABLE()

const wxString TypeLabels[] = {wxT(""), wxT("Start"), wxT("Cue"), wxT("Chunk Start"), wxT("Last Frame"), wxT("PROBLEM")}; //must match order of EventType enum

EventList::EventList(wxWindow * parent, wxWindowID id, const wxPoint & pos, const wxSize & size, bool loadEventFile, const wxString & eventFilename) :
wxListView(parent, id, pos, size, wxLC_REPORT|wxLC_SINGLE_SEL|wxSUNKEN_BORDER|wxLC_EDIT_LABELS/*|wxALWAYS_SHOW_SB*/), //ALWAYS_SHOW_SB results in a disabled scrollbar on GTK (wx 2.8))
mCanEditAfter(0), mCurrentChunkInfo(-1), mBlockEventItem(-1), mChunking(false), mRunThread(false), mSyncThread(false), mEditRate(InvalidMxfTimecode), mRecStartTimecode(InvalidMxfTimecode)
{
    //set up the columns
    wxListItem itemCol;
    itemCol.SetText(wxT("Event"));
    itemCol.SetWidth(EVENT_COL_WIDTH);
    InsertColumn(0, itemCol);
#ifdef __WIN32__
    SetColumnWidth(0, 140);
#endif
    itemCol.SetText(wxT("Timecode"));
    InsertColumn(1, itemCol); //will set width of this automatically later
#ifdef __WIN32__
    SetColumnWidth(1, 125);
#endif
    itemCol.SetText(wxT("Position"));
    InsertColumn(2, itemCol); //will set width of this automatically later
#ifdef __WIN32__
    SetColumnWidth(2, 125);
#endif
    itemCol.SetText(wxT("Description"));
    InsertColumn(3, itemCol); //will set width of this automatically later
    mRootNode = new wxXmlNode(wxXML_ELEMENT_NODE, ROOT_NODE_NAME);
    if (eventFilename.IsEmpty()) {
//idea for storing in a standard place:  mFilename = wxStandardPaths::Get().GetUserConfigDir() + wxFileName::GetPathSeparator() + SAVED_STATE_FILENAME;
        mFilename = wxDateTime::Now().Format(wxT("ingexgui-%y%m%d.xml"));
    }
    else {
        mFilename = eventFilename;
    }
    //start the thread (not checking for errors as not crucial to save; not waiting until thread starts as not crucial either)
    mCondition = new wxCondition(mMutex);
    mMutex.Lock(); //that's how the thread expects it
    wxThread::Create();
    wxThread::Run();
    if (loadEventFile) Load();

    //socket for accepting cue points from external processes
    wxIPV4address addr;
    addr.Service(2001); //port
    wxSocketServer * cuePointSocket = new wxSocketServer(addr, wxSOCKET_REUSEADDR); //make sure socket can be used again if app restarted quickly
    if (cuePointSocket->IsOk()) {
        cuePointSocket->SetEventHandler(*this);
        cuePointSocket->SetNotify(wxSOCKET_CONNECTION_FLAG);
        cuePointSocket->Notify(true);
    }
    else {
        wxCommandEvent loggingEvent(EVT_LOGGING_MESSAGE);
        loggingEvent.SetString(wxT("Could not create cue point server socket: will not be able to receive remote cue points."));
        AddPendingEvent(loggingEvent);
        cuePointSocket->Destroy();
    }
}

EventList::~EventList()
{
    delete mCondition;
}

/// Responds to the selected item being changed, either by the user or programmatically.
/// Lets the event through unless the selection matches a value flagged for blocking, in which case the flag is reset.
void EventList::OnEventSelection(wxListEvent& event)
{
    if (GetFirstSelected() == mBlockEventItem) {
        mBlockEventItem = -1;
    }
    else {
        event.Skip();
    }
}

/// Returns the chunk info of the currently selected chunk, if any
ChunkInfo * EventList::GetCurrentChunkInfo()
{
    ChunkInfo * currentChunk = 0;
    long selected = GetFirstSelected();
    if (-1 < selected && mChunkInfoArray.GetCount()) { //something's selected and has associated chunk info
        wxListItem item;
        item.SetId(selected);
        GetItem(item); //the chunk info index is in this item's data
        currentChunk = &mChunkInfoArray.Item(item.GetData());
    }
    return currentChunk;
}

/// Returns the currently-selected cue point of the current chunk (0 if none)
int EventList::GetCurrentCuePoint()
{
    int cuePoint = 0;
    ChunkInfo * currentChunk;
    if ((currentChunk = GetCurrentChunkInfo())) {
        cuePoint = GetFirstSelected() - currentChunk->GetStartIndex();
    }
    return cuePoint;
}

/// Returns the position in frames from the start of the selected recording of the start of the selected chunk
int64_t EventList::GetCurrentChunkStartPosition()
{
    ChunkInfo * currentChunk = GetCurrentChunkInfo();
    if (currentChunk) {
        return currentChunk->GetStartPosition();
    }
    else {
        return 0;
    }
}

/// Responds to an attempt to edit in the event list.
/// Allows editing of cue points only on a chunk currently being recorded (as the cue point info hasn't yet been sent to the recorder).
/// Sets the selected label to the text of the description, as you can only edit the labels (left hand column) of a list control.
/// Dispatches an event to restore the label text in case the user presses ENTER without making changes.
/// @param event The list control event.
void EventList::OnEventBeginEdit(wxListEvent& event)
{
    if (event.GetIndex() > mCanEditAfter) {
        //want to edit the description text (as opposed to the column 0 label text), so transfer it to the label
        wxListItem item;
        item.SetId(event.GetIndex());
        item.SetColumn(3);
        GetItem(item);
        item.SetColumn(0);
        item.SetMask(wxLIST_MASK_TEXT); //so it will update the text property
        SetItem(item);
        //If user presses ENTER without editing, no end editing event will be generated, so restore the label once editing has commenced (and so the modified label text has been grabbed)
        wxCommandEvent restoreLabelEvent(EVT_RESTORE_LIST_LABEL, wxID_ANY);
        restoreLabelEvent.SetExtraLong(event.GetIndex());
        AddPendingEvent(restoreLabelEvent);

        event.Skip();
    }
    else { //can't edit this
        event.Veto();
    }
}

/// Responds to editing finishing in the event list by pressing ENTER after making changes, or ESCAPE.
/// If ESCAPE was not pressed, copies the edited text to the description column and vetoes the event to prevent the label being overwritten.
/// @param event The list control event.
void EventList::OnEventEndEdit(wxListEvent& event)
{
    if (!event.IsEditCancelled()) { //user hasn't pressed ESCAPE - otherwise the event text is the label text
        //put the edited text in the right column
        wxListItem item;
        item.SetId(event.GetIndex());
        item.SetColumn(3);
        item.SetText(event.GetText()); //the newly edited string
        SetItem(item);
        //prevent the label being overwritten
        event.Veto();
#ifndef __WIN32__
        SetColumnWidth(3, wxLIST_AUTOSIZE);
#endif
    }
    //tell the frame so that alphanumeric shortcuts can be re-enabled
    wxListEvent frameEvent(wxEVT_COMMAND_LIST_END_LABEL_EDIT);
    event.SetEventObject(this);
    GetParent()->AddPendingEvent(frameEvent);
}

/// Responds to commencement of editing by restoring the default label.
/// @param event The command event.
void EventList::OnRestoreListLabel(wxCommandEvent& event)
{
    wxListItem item;
    item.SetId(event.GetExtraLong());
    item.SetColumn(0);
    item.SetText(TypeLabels[CUE]);
    SetItem(item);
}

/// Clears the list and the chunk info array.
void EventList::Clear()
{
    DeleteAllItems();
    mChunkInfoArray.Empty();
    mCurrentChunkInfo = -1;
    mBlockEventItem = -1;
    ClearSavedData();
}

/// Returns true if the selected chunk has changed since the last time this function was called.
bool EventList::SelectedChunkHasChanged()
{
    bool hasChanged = true; //err on the safe side
    if (mChunkInfoArray.GetCount()) { //sanity check
        wxListItem item;
        item.SetId(GetFirstSelected());
        GetItem(item); //the take info index is in this item's data
        hasChanged = (long) item.GetData() != mCurrentChunkInfo; //GetData() is supposed to return a long anyway...
        mCurrentChunkInfo = item.GetData();
    }
    return hasChanged;
}

/// Selects the given item
/// @param item The item to select.
/// @param blockEvent Do not generate a selection event if selection changes.
void EventList::Select(const long item, const bool blockEvent)
{
    if (GetFirstSelected() != item) {
        if (blockEvent) mBlockEventItem = item;
        wxListView::Select(item);
    }
}

/// Moves to the start of the current take or the start of the previous take, if at the start of a take.
/// Generates a select event if a change in the recording or recording position results.  Do not use GetSelection() on this event because it might not work.  Use GetFirstSelected() on this object instead.
/// @param withinTake True to move to the start of the current take rather than the previous take; assumed to be at the start event of a take if this is false
void EventList::SelectPrevTake(const bool withinTake)
{
    bool continueBackwards = !withinTake; //if at the start of a take, move back to the previous take
    wxListItem item;
    item.SetId(GetFirstSelected());
    if (item.GetId() >= 0) { //something's selected
        GetItem(item); //the chunk info index is in this item's data
        size_t chunk = item.GetData(); //the currently selected chunk
        while (chunk) { //not the first chunk
            if (!mChunkInfoArray.Item(chunk).HasChunkBefore()) { //the start of a take
                if (continueBackwards) {
                    continueBackwards = false; //have gone back the extra take
                }
                else {
                    break;
                }
            }
            chunk--;
        }
        if ((int) mChunkInfoArray.Item(chunk).GetStartIndex() != GetFirstSelected()) { //moving position in the list
            Select(mChunkInfoArray.Item(chunk).GetStartIndex()); //will generate an event
            EnsureVisible(GetFirstSelected());
        }
        else if (withinTake) { //moving position in the recording but not moving position in the list
            //simulate selecting the start of the recording - NB no way I can see of setting the selection value of the event
            wxListEvent event(wxEVT_COMMAND_LIST_ITEM_SELECTED);
            event.SetEventObject(this);
            GetParent()->AddPendingEvent(event);
        }
    }
}

/// Moves up or down the event list by one item, if possible.
/// Generates a select event if new event selected.
/// @param down True to move downwards; false to move upwards.
void EventList::SelectAdjacentEvent(const bool down) {
    long selected = GetFirstSelected();
    if (-1 == selected) {
        //nothing selected
        Select(down ? GetItemCount() - 1 : 0); //select first item
    }
    else if (!down && 0 != selected) {
        //not the first item
        Select(--selected); //select next item up
    }
    else if (down && GetItemCount() - 1 != selected) {
        //not the last item
        Select(++selected); //select next item down
    }
    EnsureVisible(selected); //scroll if necessary
}

/// Moves to the start of the next take, or the end of the last take, if possible.
/// Generates a select event if new event selected.
void EventList::SelectNextTake()
{
    wxListItem item;
    item.SetId(GetFirstSelected());
    if (item.GetId() >= 0) { //something's selected
        GetItem(item);
        size_t chunk = item.GetData(); //the currently selected chunk
        while (++chunk != mChunkInfoArray.GetCount()) {
            if (!mChunkInfoArray.Item(chunk).HasChunkBefore()) { //we've found a new take
                Select(mChunkInfoArray.Item(chunk).GetStartIndex()); //go to the beginning of it
                break;
            }
        }
        if (chunk == mChunkInfoArray.GetCount()) Select(GetItemCount() - 1); //select end of last chunk
        EnsureVisible(GetFirstSelected());
    }
}

/// Moves to the start of the last take, if possible.
/// Generates a select event if new event selected.
void EventList::SelectLastTake()
{
    size_t chunk = mChunkInfoArray.GetCount();
    if (chunk) {
        while (chunk--) { //not gone past the beginning
            if (!mChunkInfoArray.Item(chunk).HasChunkBefore()) break; //abort at a start event
        }
        Select(mChunkInfoArray.Item(chunk).GetStartIndex());
        EnsureVisible(GetFirstSelected());
    }
}

/// Returns true if an event is selected and if it's a cue point in the latest chunk.
bool EventList::LatestChunkCuePointIsSelected()
{
    bool isCuePoint = false;
    if (-1 < GetFirstSelected()) {
        wxListItem item;
        item.SetId(GetFirstSelected());
        GetItem(item); //the chunk info index is in this item's data
        isCuePoint =
         item.GetData() == mChunkInfoArray.GetCount() - 1 //in the latest chunk
         && GetFirstSelected() > (int) mChunkInfoArray.Item(item.GetData()).GetStartIndex() //not at the start
         && GetFirstSelected() <= (int) mChunkInfoArray.Item(item.GetData()).GetStartIndex() + (int) mChunkInfoArray.Item(item.GetData()).GetCuePointFrames().size(); //not at any stop point
    }
    return isCuePoint;
}

/// Returns true if the first or no events are selected
bool EventList::AtTop()
{
    return (1 > GetFirstSelected());
}

/// Returns true if an event is selected event and is the start of a take
bool EventList::AtStartOfTake()
{
    bool atStartOfTake = false;
    if (-1 < GetFirstSelected()) {
        wxListItem item;
        item.SetId(GetFirstSelected());
        GetItem(item);
        atStartOfTake = (GetFirstSelected() == (int) mChunkInfoArray.Item(item.GetData()).GetStartIndex()) && (!mChunkInfoArray.Item(item.GetData()).HasChunkBefore()); //start of a chunk selected and chunk is the firs in a recording
    }
    return atStartOfTake;
}

/// Returns true if an event is selected and if it's in the last take.
bool EventList::InLastTake()
{
    bool inLastTake = false;
    if (-1 < GetFirstSelected()) {
        inLastTake = true; //unless we find a subsequent take
        wxListItem item;
        item.SetId(GetFirstSelected());
        GetItem(item);
        size_t chunk = item.GetData();
        while (++chunk < mChunkInfoArray.GetCount()) { //not reached the end
            if (!mChunkInfoArray.Item(chunk).HasChunkBefore()) { //start event - there's another event after the selected one
                inLastTake = false;
                break;
            }
        }
    }
    return inLastTake;
}

/// Returns true if the last or no events are selected
bool EventList::AtBottom()
{
    return GetFirstSelected() == GetItemCount() - 1 || -1 == GetFirstSelected();
}

/// Adds a stop/start/chunk/cue point event to the event list.
/// Start and stop events are ignored if already started/stopped
/// For start and chunk events, creates a new ChunkInfo object.
/// Cue points can occur out of order and will be inserted in the correct position in the list.
/// Updates the other controls and the player.
/// @param type The event type: START, CUE, CHUNK, STOP or [PROBLEM]-not fully implemented.
/// @param timecode Timecode of the event, for START and optionally STOP and CHUNK events (for STOP and CHUNK events, assumed to be frame-accurate, unlike frameCount).
/// @param description To show next to each event; this is the project name for START events.
/// @param frameCount The position in frames, for CUE, STOP and CHUNK events. (For STOP and CHUNK events, if timecode supplied, used to work out the number of days; otherwise, used as the frame-accurate length unless zero, which indicates unknown).
/// @param colourIndex The colour of a CUE event.
/// @param select If true, selects the start of the last take for a STOP event.
void EventList::AddEvent(EventType type, ProdAuto::MxfTimecode * timecode, const wxString & description, const int64_t frameCount, const size_t colourIndex, const bool select)
{
    wxListItem item;
    if (NONE == type) { //sanity check
        return;
    }
    //Reject multiple stop or start events
    else if (GetItemCount()) {
        item.SetId(GetItemCount() - 1);
        GetItem(item);
        if ((START == type && TypeLabels[STOP] != item.GetText()) || (STOP == type && TypeLabels[STOP] == item.GetText())) {
            return;
        }
    }
    //Reject adding non-start event as first event (sanity check) - prevents there being list entries with no associated chunk info object
    else if (START != type) {
        return;
    }
    item.SetId(GetItemCount()); //insert event at end unless a cue point, which can be out of order
    wxString dummy;
    int64_t position = frameCount;
    ProdAuto::MxfTimecode tc = InvalidMxfTimecode;
    wxFont font(EVENT_FONT_SIZE, wxFONTFAMILY_DEFAULT, wxFONTFLAG_UNDERLINED, wxFONTFLAG_UNDERLINED);
    switch (type) {
        case START :
            mChunking = false;
            NewChunkInfo(timecode, 0, description);
            item.SetText(TypeLabels[START]);
            item.SetTextColour(wxColour(0xFF, 0x20, 0x00));
            item.SetBackgroundColour(wxColour(wxT("WHITE")));
            mCanEditAfter = GetItemCount(); //don't allow editing of the start event description
//          font.SetWeight(wxFONTWEIGHT_BOLD); breaks auto col width
//          font.SetStyle(wxFONTSTYLE_ITALIC); breaks auto col width
            if (!timecode->undefined) mRecStartTimecode = *timecode;
            break;
        case CUE :
            item.SetText(TypeLabels[CUE]);
            item.SetTextColour(CuePointsDlg::GetLabelColour(colourIndex));
            item.SetBackgroundColour(CuePointsDlg::GetColour(colourIndex));
            item.SetId(mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).AddCuePoint(frameCount, colourIndex)); //put in the cue point and work out its position in the list
            tc = GetStartTimecode();
            tc.samples += frameCount; //wrap is done by FormatTimecode()
            timecode = &tc;
            break;
        case STOP :
        case CHUNK : {
                mChunking = CHUNK == type;
                item.SetText(mChunking ? TypeLabels[CHUNK] : TypeLabels[STOP]);
                item.SetTextColour(mChunking ? wxColour(wxT("GREY")) : wxColour(wxT("BLACK")));
                item.SetBackgroundColour(wxColour(wxT("WHITE")));
                mCanEditAfter = GetItemCount(); //cue point data will be sent to recorder so don't allow editing of existing cue point descriptions (or the stop/chunk description)
                mMutex.Lock();
                if (!description.IsEmpty()) new wxXmlNode(new wxXmlNode(mChunkNode, wxXML_ELEMENT_NODE, wxT("Description")), wxXML_CDATA_SECTION_NODE, wxT(""), description);
                if (timecode && !timecode->undefined) {
                    //work out the exact duration mod 1 day
                    position = timecode->samples - GetStartTimecode().samples;
                    if (position < 0) { //rolled over midnight
                        position += 24LL * 3600 * timecode->edit_rate.numerator / timecode->edit_rate.denominator;
                    }
                    //use the approx. framecount to add whole days
                    int64_t totalMinutes = frameCount / timecode->edit_rate.numerator * timecode->edit_rate.denominator / 60; //this is an approx number of minutes in total
                    if (position < 3600LL * 12 * timecode->edit_rate.numerator / timecode->edit_rate.denominator) { //fractional day is less than half
                        totalMinutes += 1; //nudge upwards to ensure all the days are added if frameCount is a bit low
                    }
                    else {
                        totalMinutes -= 1; //nudge downwards to ensure an extra day is not added if frameCount is a bit high
                    }
                    position += ((int64_t) (totalMinutes / 60 / 24)) * 3600 * 24 * timecode->edit_rate.numerator / timecode->edit_rate.denominator; //add whole days to the fractional day
                    if (!mChunking) mRecordingNode->AddProperty(wxT("OutFrame"), wxString::Format(wxT("%d"), position)); //first sample not recorded. Relative to start of recording.
                }
                else if (frameCount) {
                    if (!mChunking) mRecordingNode->AddProperty(wxT("OutFrame"), wxString::Format(wxT("%d"), frameCount)); //less accurate than the above. Relative to start of chunk
                    wxListItem startItem;
                    startItem.SetId(GetItemCount());
                    while (startItem.GetId()) {
                        startItem.SetId(startItem.GetId() - 1);
                        GetItem(startItem);
                        if (startItem.GetText() == TypeLabels[START]) {
                            tc = mChunkInfoArray.Item(startItem.GetData()).GetStartTimecode();
                            break;
                        }
                    }
                    tc.samples += frameCount;
                    if (!tc.undefined) tc.samples %= 24LL * 3600 * tc.edit_rate.numerator / tc.edit_rate.denominator;
                    timecode = &tc;
                    position = frameCount;
                }
                else {
                    timecode = &tc; //invalid timecode
                }
                //add all cue points to XML doc now, because they could have been edited or deleted up to this point
                ChunkInfo latestChunkInfo = mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1);
                wxListItem item;
                item.SetColumn(3); //description column
                if (latestChunkInfo.GetCueColourIndeces().size()) {
                    wxXmlNode * cuePointsNode = new wxXmlNode(mChunkNode, wxXML_ELEMENT_NODE, wxT("CuePoints"));
                    wxXmlNode * cuePointNode = 0; //to prevent compiler warning
                    for (unsigned int i = 0; i < latestChunkInfo.GetCueColourIndeces().size(); i++) {
                        item.SetId(latestChunkInfo.GetStartIndex() + i + 1); //the 1 is to jump over the start event
                        GetItem(item);
                        cuePointNode = SetNextChild(cuePointsNode, cuePointNode, new wxXmlNode(0, wxXML_ELEMENT_NODE, wxT("CuePoint"), wxT("")));
                        cuePointNode->AddProperty(wxT("Frame"), wxString::Format(wxT("%d"), latestChunkInfo.GetCuePointFrames()[i]));
                        cuePointNode->AddProperty(wxT("Colour"), wxString::Format(wxT("%d"), CuePointsDlg::GetColourCode(latestChunkInfo.GetCueColourIndeces()[i])));
                        if (item.GetText().length()) {
                            new wxXmlNode(cuePointNode, wxXML_CDATA_SECTION_NODE, wxT(""), item.GetText());
                        }
                    }
                }
                mMutex.Unlock();
                mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).SetLastTimecode(*timecode);
                if (mChunking) {
                    //chunking is like stopping and then immediately starting again, so create a new ChunkInfo
                    NewChunkInfo(timecode, position);
                }
                else if (!timecode->undefined) {
                    --(timecode->samples) %= 24LL * 3600 * timecode->edit_rate.numerator / timecode->edit_rate.denominator; //previous frame is the last recorded; wrap-around
                }
                if (STOP == type) mRecStartTimecode.undefined = true; //indicates recording has stopped
                break;
            }
        default : //FIXME: putting these in will break the creation of the list of locators; "cue point" editing won't be prevented; probably other issues too!
            item.SetText(TypeLabels[PROBLEM]);
            item.SetTextColour(wxColour(wxT("RED")));
            item.SetBackgroundColour(wxColour(wxT("WHITE")));
            break;
    }
    mMutex.Lock();
    //Try to add edit rate to xml doc - can't guarantee that the first start event will have a valid timecode
    if (!mRootNode->GetPropVal(wxT("EditRateNumerator"), &dummy) && timecode && !timecode->undefined) {
        mRootNode->AddProperty(wxT("EditRateNumerator"), wxString::Format(wxT("%d"), timecode->edit_rate.numerator));
        mRootNode->AddProperty(wxT("EditRateDenominator"), wxString::Format(wxT("%d"), timecode->edit_rate.denominator));
        mRootNode->AddProperty(wxT("DropFrame"), timecode->drop_frame ? wxT("yes") : wxT("no"));
    }
    mRunThread = true; //in case thread is busy so misses the signal
    mCondition->Signal();
    mMutex.Unlock();

    //event name
    item.SetColumn(0);
    item.SetData(mChunkInfoArray.GetCount() - 1); //the index of the chunk info for this chunk
    InsertItem(item); //insert (normally at end)
    if (STOP == type && select) {
        SelectLastTake();
    }
    else {
        EnsureVisible(item.GetId()); //scroll down if necessary
    }
#ifndef __WIN32__
    SetColumnWidth(0, wxLIST_AUTOSIZE);
#endif
    item.SetFont(font); //point size ignored on GTK and row height doesn't adjust on Win32

    //timecode
    item.SetColumn(1);
    item.SetText(Timepos::FormatTimecode(*timecode));
    font.SetFamily(wxFONTFAMILY_MODERN); //fixed pitch so fields don't move about
    SetItem(item);
#ifndef __WIN32__
    SetColumnWidth(1, wxLIST_AUTOSIZE);
#endif

    //position
    ProdAuto::MxfDuration duration;
    duration.edit_rate.numerator = mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).GetStartTimecode().edit_rate.numerator;
    duration.edit_rate.denominator = mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).GetStartTimecode().edit_rate.denominator;
    duration.undefined = mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).GetStartTimecode().undefined;
    duration.samples = position;
    if (STOP == type) {
        duration.samples--; //previous frame is the last recorded
    }
    if (STOP == type) {
        duration.undefined |= mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).GetLastTimecode().undefined;
    }
    else if (CHUNK == type) {
        duration.undefined |= mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 2).GetLastTimecode().undefined;
    }
    item.SetColumn(2);
    item.SetText(Timepos::FormatPosition(duration));
    SetItem(item); //additional to previous data
#ifndef __WIN32__
    SetColumnWidth(2, wxLIST_AUTOSIZE);
#endif

    //description
    item.SetColumn(3);
    item.SetText(description);
    if (item.GetText().Len()) { //kludge to stop desc field being squashed if all descriptions are empty
        SetItem(item); //additional to previous data
#ifndef __WIN32__
        SetColumnWidth(3, wxLIST_AUTOSIZE);
#endif
    }
}

/// Generates a new ChunkInfo object and Recording XML node for a new recording or chunk
/// @param timecode Start time (can be 0)
/// @param position Start position relative to start of recording
/// @param projectName If empty, project name is copied from previous chunk and no project name node is generated
void EventList::NewChunkInfo(ProdAuto::MxfTimecode * timecode, int64_t position, const wxString & projectName)
{
    //Add data to XML tree
    mMutex.Lock();
    mPrevChunkNode = mChunkNode; //so that in chunking mode, recorder data can be added to the previous chunk
    wxXmlNode * newChunkNode = new wxXmlNode(0, wxXML_ELEMENT_NODE, wxT("Chunk"), wxT(""), new wxXmlProperty(wxT("StartFrame"), wxString::Format(wxT("%d"), position))); //will be attached to another node so no worries about deletion
    if (mChunking) {
        //New Chunk node is sibling of previous Chunk node
        mChunkNode->SetNext(newChunkNode);
    }
    else { //first chunk in recording
        //new Recording node
        mRecordingNode = SetNextChild(mRootNode, mRecordingNode, new wxXmlNode(0, wxXML_ELEMENT_NODE, wxT("Recording"), wxT(""), timecode->undefined ? 0 : new wxXmlProperty(wxT("StartTime"), wxString::Format(wxT("%d"), timecode->samples))));
        //new Chunks node
        wxXmlNode * chunksNode = new wxXmlNode(0, wxXML_ELEMENT_NODE, wxT("Chunks"));
        if (projectName.IsEmpty()) {
            //new Chunks node is only child of new Recording node
            mRecordingNode->AddChild(chunksNode);
        }
        else {
            //new ProjectName node, only child of new Recording node (make first child to make XML file more legible)
            wxXmlNode * projectNameNode = new wxXmlNode(mRecordingNode, wxXML_ELEMENT_NODE, wxT("ProjectName"));
            projectNameNode->AddChild(new wxXmlNode(0, wxXML_CDATA_SECTION_NODE, wxT(""), projectName));
            //new Chunks node is sibling of new ProjectName node
            projectNameNode->SetNext(chunksNode);
        }
        //new Chunk node is first child of new Chunks node
        chunksNode->AddChild(newChunkNode);
    }
    mChunkNode = newChunkNode;
    mMutex.Unlock();

    //Add data to chunk info array
    if (mChunking && mChunkInfoArray.GetCount()) mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).SetHasChunkAfter(); //latter check a sanity check
    ChunkInfo * info = new ChunkInfo(
     GetItemCount(), //where the start of this chunk appears in the displayed list
     projectName.IsEmpty() && mChunkInfoArray.GetCount() ? mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).GetProjectName() : projectName, //project name: use previous (if any) if it's empty
     *timecode, //start timecode
     position, //start frame count relative to start of recording
     mChunking //whether a chunk or an isolated recording
    ); //deleted by mChunkInfoArray (object array)
    mChunkInfoArray.Add(info);
}

/// Returns the timecode of the start of the latest take
ProdAuto::MxfTimecode EventList::GetStartTimecode()
{
    ProdAuto::MxfTimecode startTimecode = InvalidMxfTimecode;

    size_t chunk = mChunkInfoArray.GetCount();
    if (chunk) {
        while (chunk--) { //not gone past the beginning
            if (!mChunkInfoArray.Item(chunk).HasChunkBefore()) { //start event
                startTimecode = mChunkInfoArray.Item(chunk).GetStartTimecode();
                break;
            }
        }
    }
    return startTimecode;
}

/// Adds track and file list data to the latest chunk info.
/// @param data Track and file lists for a recorder.
/// @param reload Reload the player (to update with new recording details) if it is showing the latest chunk
void EventList::AddRecorderData(RecorderData * data, bool reload)
{
    if (mChunkInfoArray.GetCount() > mChunking ? 1 : 0) { //sanity check
        //add data to XML tree
        mMutex.Lock();
        wxXmlNode * filesNode = FindChildNodeByName(mChunking ? mPrevChunkNode : mChunkNode, wxT("Files"), true); //create if nonexistent
        for (size_t i = 0; i < data->GetTrackList()->length(); i++) {
            if (data->GetTrackList()[i].has_source && strlen(data->GetStringSeq()[i].in())) { //tracks not enabled for record have blank filenames
                wxXmlNode * fileNode = new wxXmlNode(filesNode, wxXML_ELEMENT_NODE, wxT("File"), wxT(""), new wxXmlProperty(wxT("Type"), ProdAuto::VIDEO == (data->GetTrackList())[i].type ? wxT("video") : wxT("audio"))); //order of files not important
                new wxXmlNode(new wxXmlNode(fileNode, wxXML_ELEMENT_NODE, wxT("Label")), wxXML_CDATA_SECTION_NODE, wxT(""), wxString((data->GetTrackList())[i].src.package_name, *wxConvCurrent));
                new wxXmlNode(new wxXmlNode(fileNode, wxXML_ELEMENT_NODE, wxT("Path")), wxXML_CDATA_SECTION_NODE, wxT(""), wxString((data->GetStringSeq())[i].in(), *wxConvCurrent));
            }
        }
        //save updated XML tree
        mRunThread = true; //in case thread is busy so misses the signal
        mCondition->Signal();
        mMutex.Unlock();

        //add data to chunk info array
        mChunkInfoArray.Item(mChunkInfoArray.GetCount() - (mChunking ? 2 : 1)).AddRecorder(data->GetTrackList(), data->GetStringSeq());
    }
    //Send an event to reload the player if this programme is selected
    if (InLastTake() && reload) {
        //simulate selecting the start of the recording - NB no way I can see of setting the selection value of the event
        wxListEvent event(wxEVT_COMMAND_LIST_ITEM_SELECTED);
        event.SetEventObject(this);
        event.SetExtraLong(1); //signals a forced reload
        GetParent()->AddPendingEvent(event);
    }
}

/// Deletes the cue point corresponding to the currently selected event list item, if valid.
/// Does not check if recording is still in progress.
void EventList::DeleteCuePoint()
{
    if (mChunkInfoArray.GetCount()) { //sanity check
        //remove cue point from chunk info
        long selected = GetFirstSelected();
        if (mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).DeleteCuePoint(selected - mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).GetStartIndex() - 1)) { //sanity check; the last 1 is to jump over the start event
            //remove displayed cue point
            wxListItem item;
            item.SetId(selected);
            DeleteItem(item);
            //Selected row has gone so select something sensible (the next row, or the last row if the last row was deleted)
            Select(GetItemCount() == selected ? selected - 1 : selected); //Generates an event which will update the delete cue button state
        }
    }
}

/// Returns information about the locators (cue points) in the latest chunk.
ProdAuto::LocatorSeq EventList::GetLocators()
{
    ProdAuto::LocatorSeq locators;
    if (mChunkInfoArray.GetCount()) { //sanity check
        ChunkInfo latestChunkInfo = mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1);
        locators.length(latestChunkInfo.GetCueColourIndeces().size());
        wxListItem item;
        item.SetColumn(3); //description column
        for (unsigned int i = 0; i < locators.length(); i++) {
            item.SetId(latestChunkInfo.GetStartIndex() + i + 1); //the 1 is to jump over the start event
            GetItem(item);
            locators[i].comment = item.GetText().mb_str(*wxConvCurrent);
            locators[i].colour = CuePointsDlg::GetColourCode(latestChunkInfo.GetCueColourIndeces()[i]);
            locators[i].timecode = latestChunkInfo.GetStartTimecode();
            locators[i].timecode.samples += latestChunkInfo.GetCuePointFrames()[i] - latestChunkInfo.GetStartPosition();
        }
    }
    return locators;
}

/// Tries to find the first instance of the given timecode in the event list, and jumps to the selected chunk if found.
/// Does not flag the selected event as being changed, to avoid the frame offset being lost by the resulting reload
/// @param offset Sets this to the frame offset within the chunk, if found
/// @return True if timecode found
bool EventList::JumpToTimecode(const ProdAuto::MxfTimecode & tc, int64_t & offset) {
    for (size_t index = 0; index < mChunkInfoArray.GetCount(); index++) {
        if (mChunkInfoArray.Item(index).GetLastTimecode().samples >= tc.samples) { //desired timecode is somewhere at or before the end of this chunk
            if (mChunkInfoArray.Item(index).GetStartTimecode().samples <= tc.samples) { //desired timecode is somewhere at or after the start of this chunk
                //timecode is within this chunk so select it
                Select(mChunkInfoArray.Item(index).GetStartIndex(), true); //don't flag the selected event as changed, or it will be reloaded without the offset; instead, it must be reloaded manually with the offset
                offset = tc.samples - mChunkInfoArray.Item(index).GetStartTimecode().samples;
                return true;
            }
            else { //desired timecode lies in the gap before this recording
//              break; keep searching in case recordings wrap midnight
            }
        }
    }
    return false;
}

/// Populates the XML tree from a file, which will then be used to save to when new events are added.
/// Calls AddEvent() and AddRecorderData() in the same way a live recording would.
/// Does not clear existing events from the event list.
/// Assumes ClearSavedData() has been called since the last save so that mutex doesn't have to be locked on changing shared variables.
void EventList::Load()
{
    wxXmlDocument doc;
    wxXmlNode * loadedRootNode = 0; //to prevent compiler warning;
    mEditRate = InvalidMxfTimecode;
    if (wxFile::Exists(mFilename)
     && doc.Load(mFilename)
     && (loadedRootNode = doc.GetRoot())
     && ROOT_NODE_NAME == loadedRootNode->GetName()
    ) {
        //get edit rate (if present; if not, no timecodes will be valid)
        long numerator, denominator;
        wxString property;
        if (loadedRootNode->GetPropVal(wxT("EditRateNumerator"), &property)
         && property.ToLong(&numerator)
         && 0 < numerator
         && loadedRootNode->GetPropVal(wxT("EditRateDenominator"), &property)
         && property.ToLong(&denominator)
         && 0 < denominator
        ) {
            mEditRate.undefined = false;
            mEditRate.edit_rate.numerator = numerator;
            mEditRate.edit_rate.denominator = denominator;
            mEditRate.drop_frame = wxT("yes") == loadedRootNode->GetPropVal(wxT("DropFrame"), wxT("no"));
        }
        //iterate through recording nodes
        wxXmlNode * recordingNode = loadedRootNode->GetChildren();
        while (recordingNode) {
            if (wxT("Recording") == recordingNode->GetName()) {
                //iterate through chunk nodes
                wxXmlNode * chunkNode = FindChildNodeByName(recordingNode, wxT("Chunks"));
                if (chunkNode) {
                    chunkNode = chunkNode->GetChildren();
                    while (chunkNode && wxT("Chunk") != chunkNode->GetName()) {
                        chunkNode = chunkNode->GetNext();
                    }
                    if (chunkNode) {
                        //issue START event
                        ProdAuto::MxfTimecode startTimecode = mEditRate;
                        long samples;
                        if (recordingNode->GetPropVal(wxT("StartTime"), &property)
                         && property.ToLong(&samples)
                         && 0 <= startTimecode.samples
                        ) {
                            startTimecode.samples = samples;
                        }
                        else {
                            startTimecode.undefined = true;
                        }
                        AddEvent(START, &startTimecode, GetCdata(FindChildNodeByName(recordingNode, wxT("ProjectName"))));
                        do {
                            //add cue points
                            wxXmlNode * cuePointNode = FindChildNodeByName(chunkNode, wxT("CuePoints"));
                            long frame;
                            if (cuePointNode) {
                                cuePointNode = cuePointNode->GetChildren();
                                while (cuePointNode) {
                                    if (wxT("CuePoint") == cuePointNode->GetName()
                                     && cuePointNode->GetPropVal(wxT("Frame"), &property)
                                     && property.ToLong(&frame)
                                     && 0 <= frame
                                    ) {
                                        long colour;
                                        if (!cuePointNode->GetPropVal(wxT("Colour"), &property)
                                         || !property.ToLong(&colour)
                                         || 0 > colour
                                         || N_CUE_POINT_COLOURS <= colour
                                        ) {
                                            colour = 0; //default colour
                                        }
                                        AddEvent(CUE, 0, GetCdata(cuePointNode), frame, (size_t) colour);
                                    }
                                    cuePointNode = cuePointNode->GetNext();
                                }
                            }
                            //find out if last chunk
                            wxXmlNode * nextChunkNode = chunkNode->GetNext();
                            while (nextChunkNode && wxT("Chunk") != nextChunkNode->GetName()) {
                                nextChunkNode = nextChunkNode->GetNext();
                            }
                            //issue CHUNK event or STOP event as appropriate
                            frame = 0; //indicates unknown
                            if (nextChunkNode) {
                                if (!nextChunkNode->GetPropVal(wxT("StartFrame"), &property)
                                 || !property.ToLong(&frame)
                                 || 0 > frame
                                ) {
                                    frame = 0;
                                }
                            }
                            else {
                                if (!recordingNode->GetPropVal(wxT("OutFrame"), &property)
                                 || !property.ToLong(&frame)
                                 || 0 > frame
                                ) {
                                    frame = 0;
                                }
                            }
                            AddEvent(nextChunkNode ? CHUNK : STOP, 0, GetCdata(FindChildNodeByName(chunkNode, wxT("Description"))), frame, 0, false); //don't select as will look messy and could take ages if loading lots of recordings
                            //add file data
                            wxXmlNode * fileNode = FindChildNodeByName(chunkNode, wxT("Files"));
                            if (fileNode) {
                                fileNode = fileNode->GetChildren();
                                ProdAuto::TrackList_var trackList = new ProdAuto::TrackList;
                                CORBA::StringSeq_var fileList = new CORBA::StringSeq;
                                while (fileNode) {
                                    if (wxT("File") == fileNode->GetName()
                                     && fileNode->GetPropVal(wxT("Type"), &property)
                                    ) {
                                        trackList->length(trackList->length() + 1);
                                        fileList->length(fileList->length() + 1);
                                        trackList[trackList->length() - 1].type = wxT("video") == property ? ProdAuto::VIDEO : ProdAuto::AUDIO;
                                        trackList[trackList->length() - 1].name = "";
                                        fileList[fileList->length() - 1] = GetCdata(FindChildNodeByName(fileNode, wxT("Path"))).mb_str(wxConvISO8859_1);
                                        trackList[trackList->length() - 1].has_source = strlen(fileList[fileList->length() - 1]);
                                        trackList[trackList->length() - 1].src.package_name = GetCdata(FindChildNodeByName(fileNode, wxT("Label"))).mb_str(wxConvISO8859_1);
                                    }
                                    fileNode = fileNode->GetNext();
                                }
                                RecorderData data(trackList, fileList);
                                AddRecorderData(&data, false);
                            }
                            chunkNode = nextChunkNode;
                        } while (chunkNode);
                    } //some chunk nodes
                } //Chunks node present
            } //Recording node present
            recordingNode = recordingNode->GetNext();
        } //recording node loop
        SelectLastTake(); //not done earlier while recordings were being added
    } //root node
}

/// Return a child node with the given name
/// @param node The parent node to search; returns zero if this is zero.
/// @param name Name of the child node to search for.
/// @param create If child node does not exist, create it; otherwise, return zero.
wxXmlNode * EventList::FindChildNodeByName(wxXmlNode * node, const wxString & name, bool create)
{
    wxXmlNode * childNode = 0;
    if (node) {
        childNode = node->GetChildren();
        while (childNode) {
            if (name == childNode->GetName()) break;
            childNode = childNode->GetNext();
        }
        if (!childNode && create) childNode = new wxXmlNode(node, wxXML_ELEMENT_NODE, name);
    }
    return childNode;
}

/// If the given node exists and has a CDATA node has a first/only child, returns the value of this; empty string otherwise
/// Assumes mutex is locked or doesn't have to be
const wxString EventList::GetCdata(const wxXmlNode * node)
{
    wxString content;
    if (node && node->GetChildren() && wxXML_CDATA_SECTION_NODE == node->GetChildren()->GetType()) {
        content = node->GetChildren()->GetContent().Trim(); //seems to have appended newline and space!
    }
    return content;
}

/// Sets a node to be the only child of a parent node, or the next sibling of a child node
/// @param parent If this has no children, newChild becomes its child
/// @param child If parent has children, newChild becomes this parameter's next sibling
/// @param newChild The node to be added as a sibling or a child
/// @return The same as parameter "newChild".
wxXmlNode * EventList::SetNextChild(wxXmlNode * parent, wxXmlNode * child, wxXmlNode * newChild)
{
    if (parent->GetChildren()) {
        child->SetNext(newChild);
    }
    else {
        parent->AddChild(newChild);
    }
    return newChild;
}

/// Clears the XML tree which is used to generate the saved data file.
/// Puts a single node in the tree, containing the project name.
/// Returns when the thread is in a waiting state, so shared variables can be manipulated without locking the mutex after calling this function.
void EventList::ClearSavedData()
{
    mMutex.Lock();
    delete mRootNode; //remove all stored data
    mRootNode = new wxXmlNode(wxXML_ELEMENT_NODE, ROOT_NODE_NAME);
    mRunThread = true; //in case thread is busy so misses the signal
    mSyncThread = true; //tells thread to signal when it's not saving
    mCondition->Signal();
    mCondition->Wait(); //until not saving
    mMutex.Unlock();
}

/// Thread entry point, called by wxWidgets.
/// Saves the XML tree, when signalled to do so, if the root node has children.
/// If mSyncThread is false, copies the tree then saves, so that obtaining a mutex lock to modify the tree is not dependent on the saving operation.
/// If mSyncThread is true, saves without copying or unlocking the mutex, then signals back to the main thread, guaranteeing that shared variable accesses will not occur until the main thread signals again.
/// @return Always 0.
wxThread::ExitCode EventList::Entry()
{
    //mutex should be locked at this point
    while (1) {
        //wait for trigger
        if (!mRunThread) { //not been told to save/release during previous interation
            mCondition->Wait(); //unlock mutex and wait for a signal, then relock
        }
        else {
            mRunThread = false;
        }
        //action
        if (mSyncThread) {
            mSyncThread = false;
            //save synchronously
            wxXmlDocument doc;
            doc.SetRoot(mRootNode);
            doc.Save(mFilename); //zzz...
            doc.DetachRoot();
            mCondition->Signal(); //the foreground is waiting
        }
        else {
            //save asynchronously
            wxXmlNode * rootNodeCopy = new wxXmlNode(*mRootNode); //copy constructor; needs to be on the heap
            wxString fileName = mFilename;
            mMutex.Unlock();
            wxXmlDocument doc;
            doc.SetRoot(rootNodeCopy);
            doc.Save(fileName); //zzz...
            doc.DetachRoot();
            delete rootNodeCopy;
            mMutex.Lock();
        }
    }
    return 0;
}

/// Responds to events from the cue point server socket or sockets created by connections to it.
/// If a connection is made, creates a socket to accept that connection.
/// If a socket is lost, destroys the socket.
/// If data has arrived on a socket, read up to a maximum size and destroy the socket.  If we are recording, and the data contains a valid timecode string, insert a cue point with default colour, and description from the data.
/// @param event The socket event.  Uses this to determine the type of event and to get a handle on the socket involved.
void EventList::OnSocketEvent(wxSocketEvent& event)
{
    switch (event.GetSocketEvent()) {
        case wxSOCKET_CONNECTION: {
            wxSocketBase * connectedSocket = ((wxSocketServer *) event.GetSocket())->Accept();
            if (connectedSocket->IsOk()) {
                connectedSocket->SetEventHandler(*this);
                connectedSocket->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG);
                connectedSocket->Notify(true);
            }
            else {
                wxCommandEvent loggingEvent(EVT_LOGGING_MESSAGE);
                loggingEvent.SetString(wxT("Accepted cue point socket not OK: remote cue point abandoned."));
                AddPendingEvent(loggingEvent);
                connectedSocket->Destroy();
            }
            break;
        }
        case wxSOCKET_INPUT: {
            char buf[1000]; //won't accept messages longer than this
            event.GetSocket()->Read(buf, sizeof buf);
            event.GetSocket()->Destroy();
            wxString socketMessage = wxString(buf, *wxConvCurrent);
            int64_t frameCount;
            unsigned long value;
            if (
             socketMessage.Length() > 12 //at least space for a timecode
             && !mRecStartTimecode.undefined //recording, and protects against divide by zero
             && socketMessage.Mid(socketMessage.Length() - 11, 2).ToULong(&value) //hours value a number
             && value < 24 //hours value OK
            ) {
                frameCount = value;
                if (socketMessage.Mid(socketMessage.Length() - 8, 2).ToULong(&value) && value < 60) { //minutes value OK
                    frameCount = frameCount * 60 + value;
                    if (socketMessage.Mid(socketMessage.Length() - 5, 2).ToULong(&value) && value < 60) { //seconds value OK
                        frameCount = frameCount * 60 + value;
                        if (socketMessage.Right(2).ToULong(&value)) { //frames value a number
                            frameCount = frameCount * mRecStartTimecode.edit_rate.numerator / mRecStartTimecode.edit_rate.denominator + value; //now have cue point timecode as number of frames since midnight
                            frameCount -= mRecStartTimecode.samples; //number of frames since start of recording, mod 1 day FIXME: can't handle recordings more than a day long; doesn't check for stupid values
//                            if (frameCount < 0) frameCount += 24 * 60 * 60 * mRecStartTimecode.edit_rate.numerator / mRecStartTimecode.edit_rate.denominator; //wrap over midnight
                            if (frameCount < 0) frameCount = 0; //convert cue points with times before the start of the recording to the recording start
                            AddEvent(CUE, 0, socketMessage.Left(socketMessage.Length() - 12), frameCount, 0); //use default cue point colour FIXME: doesn't work over midnight
                        }
                    }
                }
            }
            break;
        }
        case wxSOCKET_LOST: //this will happen after a successful data transfer as well as under error conditions
            event.GetSocket()->Destroy(); //prevents memory leaks
            break;
        default:
            break;
    }
}