/***************************************************************************
 *   $Id: eventlist.cpp,v 1.16 2010/08/27 17:44:05 john_f Exp $           *
 *                                                                         *
 *   Copyright (C) 2009-2010 British Broadcasting Corporation                   *
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
END_EVENT_TABLE()

const wxString TypeLabels[] = {wxT(""), wxT("Start"), wxT("Cue"), wxT("Chunk Start"), wxT("Last Frame"), wxT("PROBLEM")}; //must match order of EventType enum

EventList::EventList(wxWindow * parent, wxWindowID id, const wxPoint & pos, const wxSize & size, bool loadEventFiles) :
wxListView(parent, id, pos, size, wxLC_REPORT|wxLC_SINGLE_SEL|wxSUNKEN_BORDER|wxLC_EDIT_LABELS/*|wxALWAYS_SHOW_SB*/), //ALWAYS_SHOW_SB results in a disabled scrollbar on GTK (wx 2.8))
mCanEditAfter(0), mCurrentChunkInfo(-1), mBlockEventItem(-1), mRecordingNodeCount(0), mChunking(false), mRunThread(false), mSyncThread(false), mLoadEventFiles(loadEventFiles), mEditRate(InvalidMxfTimecode)
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
//idea for storing in a standard place  mFilename = wxStandardPaths::Get().GetUserConfigDir() + wxFileName::GetPathSeparator() + SAVED_STATE_FILENAME;
    mFilename = wxDateTime::Now().Format(wxT("ingexgui-%y%m%d.xml"));
    //start the thread (not checking for errors as not crucial to save; not waiting until thread starts as not crucial either)
    mCondition = new wxCondition(mMutex);
    mMutex.Lock(); //that's how the thread expects it
    wxThread::Create();
    wxThread::Run();
//    Load(); segfaults if called here, at GetFirstSelected() in GetCurrentChunkInfo()
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
            break;
        case CUE :
            item.SetText(TypeLabels[CUE]);
            item.SetTextColour(CuePointsDlg::GetLabelColour(colourIndex));
            item.SetBackgroundColour(CuePointsDlg::GetColour(colourIndex));
            mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).AddCuePoint(frameCount, colourIndex);
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
                mRecordingNode->AddProperty(wxT("Linking"), mChunking ? wxT("Continues") : wxT("Finishes"));
                if (!description.IsEmpty()) {
                    new wxXmlNode(new wxXmlNode(mRecordingNode, wxXML_ELEMENT_NODE, wxT("Description")), wxXML_CDATA_SECTION_NODE, wxT(""), description);
                }
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
                    mRecordingNode->AddProperty(wxT("OutSample"), wxString::Format(wxT("%d"), position - mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).GetStartPosition())); //first sample not recorded. Relative to start of chunk
                }
                else if (frameCount) {
                    mRecordingNode->AddProperty(wxT("OutSample"), wxString::Format(wxT("%d"), frameCount - mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).GetStartPosition())); //less accurate than the above. Relative to start of chunk
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
                    tc.samples %= 24LL * 3600 * tc.edit_rate.numerator / tc.edit_rate.denominator;
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
                for (unsigned int i = 0; i < latestChunkInfo.GetCueColourIndeces().size(); i++) {
                    item.SetId(latestChunkInfo.GetStartIndex() + i + 1); //the 1 is to jump over the start event
                    GetItem(item);
                    wxXmlNode * cuePointNode = new wxXmlNode(mRecordingNode, wxXML_ELEMENT_NODE, wxT("CuePoint"), wxT(""), new wxXmlProperty(wxT("Index"), wxString::Format(wxT("%d"), i)));
                    cuePointNode->AddProperty(wxT("Sample"), wxString::Format(wxT("%d"), latestChunkInfo.GetCuePointFrames()[i]));
                    cuePointNode->AddProperty(wxT("Colour"), wxString::Format(wxT("%d"), CuePointsDlg::GetColourCode(latestChunkInfo.GetCueColourIndeces()[i])));
                    if (item.GetText().length()) {
                        new wxXmlNode(cuePointNode, wxXML_CDATA_SECTION_NODE, wxT(""), item.GetText());
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
                break;
            }
        default : //FIXME: putting these in will break the creation of the list of locators; "cue point" editing won't be prevented; probably other issues too!
            item.SetText(TypeLabels[PROBLEM]);
            item.SetTextColour(wxColour(wxT("RED")));
            item.SetBackgroundColour(wxColour(wxT("WHITE")));
            break;
    }
    mMutex.Lock();
    //Try to add edit rate - can't guarantee that the first start event will have a valid timecode
    if (!mRootNode->GetPropVal(wxT("EditRateNumerator"), &dummy) && timecode && !timecode->undefined) {
        mRootNode->AddProperty(wxT("EditRateNumerator"), wxString::Format(wxT("%d"), timecode->edit_rate.numerator));
        mRootNode->AddProperty(wxT("EditRateDenominator"), wxString::Format(wxT("%d"), timecode->edit_rate.denominator));
        mRootNode->AddProperty(wxT("DropFrame"), timecode->drop_frame ? wxT("Yes") : wxT("No"));
    }
    mRunThread = true; //in case thread is busy so misses the signal
    mCondition->Signal();
    mMutex.Unlock();

    //event name
    item.SetColumn(0);
    item.SetId(GetItemCount()); //will insert at end
    item.SetData(mChunkInfoArray.GetCount() - 1); //the index of the chunk info for this chunk
    InsertItem(item); //insert (at end)
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
    mMutex.Lock();
    mPrevRecordingNode = mRecordingNode; //so that in chunking mode, recorder data can be added to the previous recording
    mRecordingNode = new wxXmlNode(mRootNode, wxXML_ELEMENT_NODE, wxT("Recording"), wxT(""), new wxXmlProperty(wxT("Index"), wxString::Format(wxT("%d"), mRecordingNodeCount++)));
    if (!projectName.IsEmpty()) new wxXmlNode(new wxXmlNode(mRecordingNode, wxXML_ELEMENT_NODE, wxT("ProjectName")), wxXML_CDATA_SECTION_NODE, wxT(""), projectName);
    if (!timecode->undefined) mRecordingNode->AddProperty(wxT("StartTime"), wxString::Format(wxT("%d"), timecode->samples));
    mMutex.Unlock();
    if (mChunking && mChunkInfoArray.GetCount()) mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).SetHasChunkAfter(); //latter check a sanity check
    ChunkInfo * info = new ChunkInfo(GetItemCount(), projectName.IsEmpty() && mChunkInfoArray.GetCount() ? mChunkInfoArray.Item(mChunkInfoArray.GetCount() - 1).GetProjectName() : projectName, *timecode, position, mChunking); //this will be the index of the current event; deleted by mChunkInfoArray (object array)
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
        mChunkInfoArray.Item(mChunkInfoArray.GetCount() - (mChunking ? 2 : 1)).AddRecorder(data->GetTrackList(), data->GetFileList());
        unsigned int index = 0;
        mMutex.Lock();
        wxXmlNode * recorderNode = new wxXmlNode(mChunking ? mPrevRecordingNode : mRecordingNode, wxXML_ELEMENT_NODE, wxT("Recorder"), wxT(""), new wxXmlProperty(wxT("Index"), wxString::Format(wxT("%d"), mChunkInfoArray.Item(mChunkInfoArray.GetCount() - (mChunking ? 2 : 1)).GetTracks().GetCount() - 1)));
        for (size_t i = 0; i < data->GetTrackList()->length(); i++) {
            if (data->GetTrackList()[i].has_source && strlen(data->GetFileList()[i].in())) { //tracks not enabled for record have blank filenames
                wxXmlNode * fileNode = new wxXmlNode(recorderNode, wxXML_ELEMENT_NODE, wxT("File"), wxT(""), new wxXmlProperty(wxT("Index"), wxString::Format(wxT("%d"), index++)));
                fileNode->AddProperty(wxT("Type"), ProdAuto::VIDEO == (data->GetTrackList())[i].type ? wxT("Video") : wxT("Audio"));
                new wxXmlNode(new wxXmlNode(fileNode, wxXML_ELEMENT_NODE, wxT("Label")), wxXML_CDATA_SECTION_NODE, wxT(""), wxString((data->GetTrackList())[i].src.package_name, *wxConvCurrent));
                new wxXmlNode(new wxXmlNode(fileNode, wxXML_ELEMENT_NODE, wxT("Path")), wxXML_CDATA_SECTION_NODE, wxT(""), wxString((data->GetFileList())[i].in(), *wxConvCurrent));
            }
        }
        //save updated XML tree
        mRunThread = true; //in case thread is busy so misses the signal
        mCondition->Signal();
        mMutex.Unlock();
    }
    //Send an event to reload the player if this programme is selected
    if (InLastTake() && reload) {
        //simulate selecting the start of the recording - NB no way I can see of setting the selection value of the event
        wxListEvent event(wxEVT_COMMAND_LIST_ITEM_SELECTED);
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
    wxXmlNode * loadedRootNode = 0;
    wxString str;
    long long1, long2;
    mEditRate = InvalidMxfTimecode;
    long index;
    wxXmlNode * node;
    std::vector<wxXmlNode *> recordingNodes;
    if (wxFile::Exists(mFilename)
     && doc.Load(mFilename)
     && (loadedRootNode = doc.GetRoot())
     && ROOT_NODE_NAME == loadedRootNode->GetName()
    ) {
        //get edit rate (if present; if not, no timecodes will be valid)
        if (loadedRootNode->GetPropVal(wxT("EditRateNumerator"), &str)
         && str.ToLong(&long1)
         && 0 < long1
         && loadedRootNode->GetPropVal(wxT("EditRateDenominator"), &str)
         && str.ToLong(&long2)
         && 0 < long2
        ) {
            mEditRate.undefined = false;
            mEditRate.edit_rate.numerator = long1;
            mEditRate.edit_rate.denominator = long2;
            mEditRate.drop_frame = wxT("Yes") == loadedRootNode->GetPropVal(wxT("DropFrame"), wxT("No"));
        }
        //make a list of the recording nodes in order
        node = loadedRootNode->GetChildren();
        while (node) {
            if (wxT("Recording") == node->GetName()
             && node->GetPropVal(wxT("Index"), &str)
             && str.ToLong(&index)
             && 0 <= index
            ) {
                //extend vector if necessary
                if (recordingNodes.size() < (unsigned long) index + 1) {
                    size_t oldSize = recordingNodes.size();
                    recordingNodes.resize(index + 1);
                    for (size_t i = oldSize; i <= (unsigned long) index; i++) {
                        recordingNodes[i] = 0; //to catch missing index values
                    }
                }
                recordingNodes[index] = node;
            }
            node = node->GetNext();
        }
    }
    else {
        loadedRootNode = 0;
    }
    if (loadedRootNode) {
        //iterate over the recording nodes, filling in the event list
        long lastPosition = 0;
        for (size_t i = 0; i < recordingNodes.size(); i++) {
            wxString projectName, description;
            if (recordingNodes[i]) {
                //make a list of the cue points and recorders in this recording, in order, and obtain project name and description
                std::vector<wxXmlNode *> recorderNodes;
                std::vector<wxXmlNode *> cuePointNodes;
                node = recordingNodes[i]->GetChildren();
                ProdAuto::MxfTimecode timecode = mEditRate; //for edit rate and defined values
                while (node) {
                    if (wxT("CuePoint") == node->GetName()
                        && node->GetPropVal(wxT("Index"), &str)
                        && str.ToLong(&index)
                        && 0 <= index
                    ) {
                        //extend vector if necessary
                        if (cuePointNodes.size() < (unsigned long) index + 1) {
                            size_t oldSize = cuePointNodes.size();
                            cuePointNodes.resize(index + 1);
                            for (size_t i = oldSize; i <= (unsigned long) index; i++) {
                                cuePointNodes[i] = 0; //to catch missing index values
                            }
                        }
                        cuePointNodes[index] = node;
                    }
                    else if (wxT("Recorder") == node->GetName()
                        && node->GetPropVal(wxT("Index"), &str)
                        && str.ToLong(&index)
                        && 0 <= index
                    ) {
                        //extend vector if necessary
                        if (recorderNodes.size() < (unsigned long) index + 1) {
                            size_t oldSize = recorderNodes.size();
                            recorderNodes.resize(index + 1);
                            for (size_t i = oldSize; i <= (unsigned long) index; i++) {
                                recorderNodes[i] = 0; //to catch missing index values
                            }
                        }
                        recorderNodes[index] = node;
                    }
                    else if (wxT("ProjectName") == node->GetName()) {
                        projectName = GetCdata(node);
                    }
                    else if (wxT("Description") == node->GetName()) {
                        description = GetCdata(node);
                    }
                    node = node->GetNext();
                }
                //add recording start to the event list
                if (!timecode.undefined) { //have edit rate
                    if (recordingNodes[i]->GetPropVal(wxT("StartTime"), &str)
                    && str.ToLong(&long1)
                    && 0 <= long1
                    ) {
                        timecode.samples = long1;
                    }
                    else {
                        timecode.undefined = true;
                    }
                }
                AddEvent(START, &timecode, projectName);
                //add cue points to the event list
                for (size_t j = 0; j < cuePointNodes.size(); j++) {
                    if (cuePointNodes[j]
                        && cuePointNodes[j]->GetPropVal(wxT("Sample"), &str)
                        && str.ToLong(&long1)
                        && 0 <= long1
                    ) {
                        if (!cuePointNodes[j]->GetPropVal(wxT("Colour"), &str)
                        || !str.ToLong(&long2)
                        || 0 > long1
                        || N_CUE_POINT_COLOURS <= long2
                        ) {
                            long2 = 0; //default colour
                        }
                        AddEvent(CUE, 0, GetCdata(cuePointNodes[j]), long1, (size_t) long2);
                    }
                }
                //add chunk stop to the event list
                long1 = 0; //indicates unknown
                if (!recordingNodes[i]->GetPropVal(wxT("OutSample"), &str)
                    || !str.ToLong(&long1)
                    || 0 > long1
                ) {
                    long1 = 0;
                }
                if (long1) { //valid position
                    lastPosition += long1; //OutSample is relative to start of chunk; make it relative to start of recording
                }
                else {
                    lastPosition = 0; //indicates unknown
                }
                str = recordingNodes[i]->GetPropVal(wxT("Linking"), wxT("Finishes"));
                AddEvent(wxT("Continues") == str ? CHUNK : STOP, 0, description, lastPosition, 0, false); //don't select as will look messy and could take ages if loading lots of recordings
                if (wxT("Continues") != str) {
                    lastPosition = 0;
                }
                //iterate over the recorder nodes, adding file/track details to the recording
                for (size_t i = 0; i < recorderNodes.size(); i++) {
                    if (recorderNodes[i]) {
                        std::vector<wxXmlNode *> fileNodes;
                        node = recorderNodes[i]->GetChildren();
                        while (node) {
                            if (wxT("File") == node->GetName()
                                && node->GetPropVal(wxT("Index"), &str)
                                && str.ToLong(&index)
                                && 0 <= index
                            ) {
                                //extend vector if necessary
                                if (fileNodes.size() < (unsigned long) index + 1) {
                                    size_t oldSize = fileNodes.size();
                                    fileNodes.resize(index + 1);
                                    for (size_t i = oldSize; i <= (unsigned long) index; i++) {
                                        fileNodes[i] = 0; //to catch missing index values
                                    }
                                }
                                fileNodes[index] = node;
                            }
                            node = node->GetNext();
                        }
                        if (fileNodes.size()) {
                            //add tracks/files to the chunk info
                            ProdAuto::TrackList_var trackList = new ProdAuto::TrackList;
                            CORBA::StringSeq_var fileList = new CORBA::StringSeq;
                            trackList->length(fileNodes.size());
                            fileList->length(fileNodes.size());
                            for (size_t j = 0; j < fileNodes.size(); j++) {
                                trackList[j].type = ProdAuto::VIDEO;
                                trackList[j].name = "";
                                trackList[j].has_source = false;
                                fileList[j] = "";
                                if (fileNodes[j]) {
                                    if (fileNodes[j]->GetPropVal(wxT("Type"), &str) && wxT("Audio") == str) {
                                        trackList[j].type = ProdAuto::AUDIO;
                                    }
                                    node = fileNodes[j]->GetChildren();
                                    while (node) {
                                        if (wxT("Path") == node->GetName()) {
                                            fileList[j] = GetCdata(node).mb_str(*wxConvCurrent);
                                            trackList[j].has_source = true;
                                        }
                                        else if (wxT("Label") == node->GetName()) {
                                            trackList[j].src.package_name = GetCdata(node).mb_str(*wxConvCurrent);
                                        }
                                        node = node->GetNext();
                                    }
                                }
                            }
                            RecorderData data(trackList, fileList);
                            AddRecorderData(&data, false);
                        } //there are file nodes
                    } //recorder node present
                } //recorder node loop
            } //recording node present
        } //recording node loop
        SelectLastTake(); //not done earlier while recordings were being added
    } //root node
}

/// If the given node has a CDATA node as a child, returns the value of this; empty string otherwise
/// Assumes mutex is locked or doesn't have to be
const wxString EventList::GetCdata(wxXmlNode * node)
{
    wxString content;
    if (node->GetChildren() && wxXML_CDATA_SECTION_NODE == node->GetChildren()->GetType()) {
        content = node->GetChildren()->GetContent().Trim(); //seems to have appended newline and space!
    }
    return content;
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
    mRecordingNodeCount = 0;
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

