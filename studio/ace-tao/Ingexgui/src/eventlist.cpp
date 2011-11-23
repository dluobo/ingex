/***************************************************************************
 *   $Id: eventlist.cpp,v 1.26 2011/11/23 13:47:34 john_f Exp $           *
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

#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/file.h>
#include <wx/socket.h>
#include "eventlist.h"
#include "ingexgui.h" //consts
#include "timepos.h"
#include "recordergroup.h"
#include "dialogues.h"
#include "savedstate.h"

#define ROOT_NODE_NAME wxT("IngexguiEvents")
#define DEFAULT_MAX_CHUNKS 1000
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
mBlockEventItem(-1), mRunThread(false), mSyncThread(false), mEditRate(InvalidMxfTimecode), mSelectedChunkNode(0), mFilesHaveChanged(false), mTotalChunks(0), mSavedState(0)
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
    if (loadEventFile) LoadDocument();

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
    delete mRootNode;
}

/// Responds to the selected item being changed, either by the user or programmatically.
/// Notes if a new chunk has been selected.
/// Lets the event through unless the selection matches a value flagged for blocking, in which case the flag is reset.
void EventList::OnEventSelection(wxListEvent& event)
{
    wxMutexLocker lock(mMutex);
    wxXmlNode * node = GetSelectedNode();
    if (node) { //something selected
        if (wxT("Recording") == node->GetName()) node = GetNode(GetFirstSelected() - 1); //move up the list from a stop event (to a cue, chunk or start event)
        if (wxT("Chunk") != node->GetName()) node = node->GetParent()->GetParent(); //move to a chunk node
    }
    if (node != mSelectedChunkNode) {
        mSelectedChunkNode = node;
        mFilesHaveChanged = true;
    }
    if (GetFirstSelected() == mBlockEventItem) {
        mBlockEventItem = -1;
    }
    else {
        event.Skip();
    }
}

/// Returns the node pointed to by a list item.
/// Assumes mutex is locked.
/// @param index The index of the item; must be within the list, or -1.
/// @return The node attached to the item.  Zero if index is -1.
wxXmlNode * EventList::GetNode(const long index)
{
    wxXmlNode * node = 0;
    if (-1 < index) {
        wxListItem item;
        item.SetId(index);
        GetItem(item); //the chunk info index is in this item's data
        node = ((wxXmlNode *) item.GetData());
    }
    return node;
}

/// Returns the node pointed to by the currently selected list item, if any.
/// Assumes mutex is locked.
wxXmlNode * EventList::GetSelectedNode()
{
    return GetNode(GetFirstSelected());
}

/// Returns the recording node corresponding to the currently selected item or the provided node.
/// Assumes mutex is locked.
/// @param node If not provided, uses currently selected node, if any.
wxXmlNode * EventList::GetRecordingNode(wxXmlNode * node)
{
    if (!node) node = GetSelectedNode();
    if (node) {
        if (wxT("CuePoint") == node->GetName()) node = node->GetParent()->GetParent(); //up to Chunk node
        if (wxT("Chunk") == node->GetName()) node = node->GetParent()->GetParent(); //up to Recording node
    }
    return node;
}

/// Returns the chunk node corresponding to the latest chunk.
/// Assumes mutex is locked and that there is a chunk currently being recorded.
wxXmlNode * EventList::GetCreatingChunkNode()
{
    wxXmlNode * node = GetNode(GetItemCount() - 1);
    if (wxT("CuePoint") == node->GetName()) node = node->GetParent()->GetParent();
    return node;
}

/// Returns the currently-selected cue point's frame offset within the chunk (0 if none or at the start, -1 if at the end).
int64_t EventList::GetSelectedCuePointOffset()
{
    int64_t offset = 0;
    wxMutexLocker lock(mMutex);
    wxXmlNode * selectedNode = GetSelectedNode();
    if (selectedNode) {
        if (wxT("Recording") == selectedNode->GetName()) { //stop point
            offset = -1;
        }
        else if (wxT("CuePoint") == selectedNode->GetName()) {
            offset = GetNumericalPropVal(selectedNode, wxT("Frame"), 0);
        }
    }
    return offset;
}

/// Returns the frame offsets of the selected chunk's cue points.
std::vector<unsigned long long> EventList::GetSelectedChunkCuePointOffsets()
{
    std::vector<unsigned long long> offsets;
    wxMutexLocker lock(mMutex);
    wxXmlNode * node = FindChildNodeByName(mSelectedChunkNode, wxT("CuePoints"));
    if (node) { //sanity check
        node = node->GetChildren();
        while (node) {
            offsets.push_back(GetNumericalPropVal(node, wxT("Frame"), 0));
            node = node->GetNext();
        }
    }
    return offsets;
}

/// Returns the index in the list of the start of the selected chunk.
unsigned long EventList::GetSelectedChunkStartIndex()
{
    long index = GetFirstSelected();
    if (index > 0) {
        wxMutexLocker lock(mMutex);
        while (index && GetNode(index) != mSelectedChunkNode) {
            index--;
        }
    }
    else {
        index = 0;
    }
    return (unsigned long) index;
}

/// Returns the position in frames from the start of the selected recording of the start of the selected chunk (or 0 if not applicable).
int64_t EventList::GetSelectedChunkStartPosition()
{
    unsigned long long position;
    wxMutexLocker lock(mMutex);
    position = GetNumericalPropVal(mSelectedChunkNode, wxT("StartFrame"), 0);
    return (int64_t) position;
}

/// Returns a list of the complete paths of the files associated with the currently-selected chunk, if any.
/// @param types Returns track type of each file.
/// @param labels Returns label (source package name) of each file.
const wxArrayString EventList::GetSelectedFiles(ArrayOfTrackTypes * types, wxArrayString * labels)
{
    wxArrayString files;
    wxMutexLocker lock(mMutex);
    wxXmlNode * node = mSelectedChunkNode;
    if (node) {
        node = FindChildNodeByName(node, wxT("Files"));
        if (node) {
            node = node->GetChildren();
            while (node) {
                files.Add(GetCdata(FindChildNodeByName(node, wxT("Path"))));
                if (types) types->Add(wxT("video") == node->GetPropVal(wxT("Type"), wxT("video")) ? ProdAuto::VIDEO : ProdAuto::AUDIO);
                if (labels) labels->Add(GetCdata(FindChildNodeByName(node, wxT("Label"))));
                node = node->GetNext();
            }
        }
    }
    return files;
}

/// Returns the project name corresponding to the currently-selected chunk, if any.
const wxString EventList::GetSelectedProjectName()
{
    wxString projectName;
    wxMutexLocker lock(mMutex);
    if (mSelectedChunkNode) projectName = GetCdata(FindChildNodeByName(mSelectedChunkNode->GetParent()->GetParent(), wxT("ProjectName")));
    return projectName;
}


/// Responds to an attempt to edit in the event list.
/// Allows editing of cue points only on a chunk currently being recorded (as the cue point info hasn't yet been sent to the recorder).
/// Sets the selected label to the text of the description, as you can only edit the labels (left hand column) of a list control.
/// Dispatches an event to restore the label text in case the user presses ENTER without making changes.
/// @param event The list control event.
void EventList::OnEventBeginEdit(wxListEvent& event)
{
    wxXmlNode * currentNode = GetNode(event.GetIndex());
    if (currentNode && currentNode->GetName() == wxT("CuePoint") && !currentNode->GetParent()->GetParent()->GetNext()) { //a cue point in the latest chunk
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
/// If ESCAPE was not pressed, copies the edited text to the description column and the Xml tree, and vetoes the event to prevent the label being overwritten.
/// @param event The list control event.
void EventList::OnEventEndEdit(wxListEvent& event)
{
    if (!event.IsEditCancelled()) { //user hasn't pressed ESCAPE - otherwise the event text is the label text
        wxString description = event.GetText();
        description.Trim(true).Trim(false);
        //put the edited text in the right column
        wxListItem item;
        item.SetId(event.GetIndex());
        GetItem(item);
        item.SetColumn(3);
        item.SetText(description);
        SetItem(item);
        //update the cue point node
        wxMutexLocker lock(mMutex);
        wxXmlNode * cDataNode = ((wxXmlNode *) item.GetData())->GetChildren();
        if (cDataNode) { //cue point already had a description
            if (description.IsEmpty()) { //description removed
                ((wxXmlNode *) item.GetData())->RemoveChild(cDataNode);
                delete cDataNode;
            }
            else { //description (maybe) altered
                cDataNode->SetContent(description);
            }
        }
        else if (!description.IsEmpty()) { //new description
            new wxXmlNode(((wxXmlNode *) item.GetData()), wxXML_CDATA_SECTION_NODE, wxEmptyString, description);
        }
        SaveDocument();
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

/// Clears the list and the XML tree.
/// Returns when the thread is in a waiting state, so shared variables can be manipulated without locking the mutex after calling this function.
void EventList::Clear()
{
    DeleteAllItems();
    mBlockEventItem = -1;
    mSelectedChunkNode = 0;
    mMutex.Lock();
    delete mRootNode; //remove all stored data
    mRootNode = new wxXmlNode(wxXML_ELEMENT_NODE, ROOT_NODE_NAME);
    mRunThread = true; //in case thread is busy so misses the signal
    mSyncThread = true; //tells thread to signal when it's not saving
    mCondition->Signal();
    mCondition->Wait(); //until not saving
    mMutex.Unlock();
    mTotalChunks = 0;
}

/// Selects the given item.
/// Mutex must be unlocked.
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
/// Generates a select event if a change in the recording or recording position results.  Do not use GetSelection() on this event because it might not work.  Use GetFirstSelected() on the object instead.
/// @param withinTake True to move to the start of the current take rather than the previous take
void EventList::SelectPrevTake(const bool withinTake)
{
    long index = GetFirstSelected();
    if (index > 0) { //some takes and not at the start of the first one
        mMutex.Lock();
        wxXmlNode * node = GetNode(index);
        //move to previous recording if at the start of a recording
        if (!withinTake && wxT("Chunk") == node->GetName() && node->GetParent()->GetChildren() == node) {
            index--;
        }
        //find list item corresponding to first chunk
        node = FindChildNodeByName(GetRecordingNode(GetNode(index)), wxT("Chunks"))->GetChildren();
        while (index && GetNode(index) != node) {
            index--;
        }
        //select (which sends event) or send event
        mMutex.Unlock();
        if (index != GetFirstSelected()) { //moving position in the list
            Select(index); //will generate an event
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
        Select(down ? GetItemCount() - 1 : 0); //select first or last item
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
    long index = GetFirstSelected();
    if (index >= 0) { //something's selected
        mMutex.Lock();
        wxXmlNode * node = GetRecordingNode(); //this is the stop node of the current recording
        if (node->GetNext()) { //there are more recordings
            //find start node of next recording
            node = FindChildNodeByName(node->GetNext(), wxT("Chunks"))->GetChildren();
        }
        //find list item corresponding to end of recording
        while (index < GetItemCount() - 1 && GetNode(index) != node) {
            index++;
        }
        mMutex.Unlock();
        Select(index); //will generate an event
        EnsureVisible(GetFirstSelected());
    }
}

/// Moves to the start of the last take, if possible.
/// Generates a select event if new event selected.
void EventList::SelectLastTake()
{
    if (GetItemCount()) {
        mMutex.Lock();
        wxXmlNode * lastStartNode = FindChildNodeByName(GetRecordingNode(GetNode(GetItemCount() - 1)), wxT("Chunks"))->GetChildren();
        long index = GetItemCount() - 1;
        while (index && GetNode(index) != lastStartNode) {
            index--;
        }
        mMutex.Unlock();
        Select(index); //will generate an event
        EnsureVisible(GetFirstSelected());
    }
}

/// Returns true if an event is selected and if it's a cue point in the latest chunk.
bool EventList::LatestChunkCuePointIsSelected()
{
    wxMutexLocker lock(mMutex);
    wxXmlNode * selectedNode = GetSelectedNode();
    return selectedNode //something selected
     && wxT("CuePoint") == selectedNode->GetName() //a cue point is selected
     && !selectedNode->GetParent()->GetParent()->GetNext() //within last chunk
     && !selectedNode->GetParent()->GetParent()->GetParent()->GetParent()->GetNext(); //within last recording
}

/// Returns true if the first or no events are selected
bool EventList::AtTop()
{
    return (1 > GetFirstSelected());
}

/// Returns true if an event is selected event and is the start of a take
bool EventList::AtStartOfTake()
{
    wxMutexLocker lock(mMutex);
    wxXmlNode * currentNode = GetSelectedNode();
    return currentNode //something selected
     && wxT("Chunk") == currentNode->GetName() //at start of chunk
     && currentNode->GetParent()->GetChildren() == currentNode; //first chunk
}

/// Returns true if an event is selected and if it's in the last take.
bool EventList::InLastTake()
{
    bool inLastTake = false;
    wxMutexLocker lock(mMutex);
    wxXmlNode * recordingNode = GetRecordingNode();
    if (recordingNode) {
        //see if it's the last
        inLastTake = !recordingNode->GetNext();
    }
    return inLastTake;
}

/// Returns true if the last or no events are selected
bool EventList::AtBottom()
{
    return GetFirstSelected() == GetItemCount() - 1 || -1 == GetFirstSelected();
}

/// Returns true if the currently selected chunk is not the first in the recording
bool EventList::ChunkBeforeSelectedChunk()
{
    wxMutexLocker lock(mMutex);
    return mSelectedChunkNode && mSelectedChunkNode->GetParent()->GetChildren() != mSelectedChunkNode;
}

/// Returns true if the currently selected chunk is not the last in the recording
bool EventList::ChunkAfterSelectedChunk()
{
    wxMutexLocker lock(mMutex);
    return mSelectedChunkNode && mSelectedChunkNode->GetNext();
}

/// Returns true if the set of files corresponding to the selected chunk has changed since this method was last called.
bool EventList::FilesHaveChanged()
{
    bool filesHaveChanged = mFilesHaveChanged;
    mFilesHaveChanged = false;
    return filesHaveChanged;
}

/// Adds a stop/start/chunk/cue point event to the event list.
/// For start and chunk events, creates a new chunk.
/// Cue points can occur out of order and will be inserted in the correct position in the list.
/// Updates the other controls and the player.
/// @param type The event type: START, CUE, CHUNK, STOP or [PROBLEM]-not implemented.
/// @param timecode Timecode of the event, for START and optionally STOP and CHUNK events (for STOP and CHUNK events, assumed to be frame-accurate, unlike frameCount).
/// @param frameCount The position in frames. (For STOP and CHUNK events, if timecode supplied, frameCount is just used to work out the number of days; otherwise, used as the frame-accurate length unless zero, which indicates unknown.  For START events. if non-zero, assumed to be an orphaned CHUNK event.)
/// @param description For CUE, CHUNK and STOP events.  For CHUNK and STOP events it is shown next to the previous CHUNK or START event, as it is a description added at the end of the corresponding chunk or recording.  For CUE events it is shown next to the event being added.
/// @param colourIndex The colour of a CUE event.
/// @param select If true, selects the start of the last take for a STOP event.
/// @param projectName To display next to STOP events.
void EventList::AddEvent(EventType type, ProdAuto::MxfTimecode * timecode, const int64_t frameCount, const wxString & description, const size_t colourIndex, const bool select, const wxString & projectName)
{
    wxListItem item;
    item.SetId(GetItemCount()); //insert event at end unless a cue point, which can be out of order
    wxString dummy;
    int64_t position = frameCount;
    ProdAuto::MxfTimecode tc = InvalidMxfTimecode;
    wxFont font(EVENT_FONT_SIZE, wxFONTFAMILY_DEFAULT, wxFONTFLAG_UNDERLINED, wxFONTFLAG_UNDERLINED);
    switch (type) {
        case START :
            SetItemAppearance(item, frameCount ? CHUNK : START);
            item.SetData((void *) NewChunk(timecode, 0, description));
            break;
        case CUE :
            {
                SetItemAppearance(item, CUE, colourIndex);
                //new cue point node
                wxXmlNode * cuePointNode = new wxXmlNode(0, wxXML_ELEMENT_NODE, wxT("CuePoint"));
                cuePointNode->AddProperty(wxT("Frame"), wxString::Format(wxT("%d"), frameCount));
                cuePointNode->AddProperty(wxT("Colour"), wxString::Format(wxT("%d"), colourIndex));
                if (!description.IsEmpty()) new wxXmlNode(cuePointNode, wxXML_CDATA_SECTION_NODE, wxEmptyString, description);
                //place node
                mMutex.Lock();
                wxXmlNode * existingNode = GetNode(GetItemCount() - 1);
                if (wxT("Chunk") == existingNode->GetName()) { //no cue points yet
                    FindChildNodeByName(existingNode, wxT("CuePoints"), true)->AddChild(cuePointNode); //create array and add member
                }
                else {
                    //find where in array to put new node (as cue points can appear out of order)
                    long index = GetItemCount() - 1;
                    while (index && wxT("CuePoint") == existingNode->GetName() && (int64_t) GetNumericalPropVal(existingNode, wxT("Frame"), 0) > frameCount) {
                        existingNode = GetNode(--index);
                    }
                    existingNode->GetParent()->InsertChildAfter(cuePointNode, existingNode);
                    item.SetId(index + 1);
                }
                item.SetData((void *) cuePointNode);
                tc = GetStartTimecode();
                mMutex.Unlock();
                tc.samples += frameCount; //wrap is done by FormatTimecode()
                timecode = &tc;
                break;
            }
        case STOP :
        case CHUNK : {
                SetItemAppearance(item, CHUNK == type ? CHUNK : STOP);
                mMutex.Lock();
                wxXmlNode * currentRecordingNode = GetRecordingNode(GetNode(GetItemCount() - 1));
                if (!description.IsEmpty()) new wxXmlNode(new wxXmlNode(GetCreatingChunkNode(), wxXML_ELEMENT_NODE, wxT("Description")), wxXML_CDATA_SECTION_NODE, wxT(""), description);
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
                    if (STOP == type) currentRecordingNode->AddProperty(wxT("OutFrame"), wxString::Format(wxT("%d"), position)); //first sample not recorded. Relative to start of recording.
                }
                else if (frameCount) {
                    if (STOP == type) currentRecordingNode->AddProperty(wxT("OutFrame"), wxString::Format(wxT("%d"), frameCount)); //less accurate than the above
                    tc = mEditRate;
                    tc.samples = GetNumericalPropVal(currentRecordingNode, wxT("StartTime"), 0) + frameCount;
                    if (!tc.undefined) tc.samples %= 24LL * 3600 * tc.edit_rate.numerator / tc.edit_rate.denominator;
                    timecode = &tc;
                    position = frameCount;
                }
                else {
                    timecode = &tc; //invalid timecode
                }
                mMutex.Unlock();
                if (CHUNK == type) {
                    //chunking is like stopping and then immediately starting again, so create a new chunk
                    item.SetData((void *) NewChunk(timecode, position));
                }
                else {
                    if (!timecode->undefined) --(timecode->samples) %= 24LL * 3600 * timecode->edit_rate.numerator / timecode->edit_rate.denominator; //previous frame is the last recorded; wrap-around
                    item.SetData((void *) currentRecordingNode);
                    if (!projectName.IsEmpty()) new wxXmlNode(new wxXmlNode(currentRecordingNode, wxXML_ELEMENT_NODE, wxT("ProjectName")), wxXML_CDATA_SECTION_NODE, wxT(""), projectName);
                }
                //description: add to start event of this chunk
                if (!description.IsEmpty()) { //stops desc column being squashed if all descriptions are empty
                    long index = GetItemCount() - 1;
                    mMutex.Lock();
                    while (index && wxT("Chunk") != GetNode(index)->GetName()) index--; //sanity check
                    mMutex.Unlock();
                    wxListItem chunkStart;
                    chunkStart.SetId(index);
                    GetItem(chunkStart); //for formatting
                    chunkStart.SetColumn(3);
                    chunkStart.SetText(description);
                    SetItem(chunkStart);
#ifndef __WIN32__
                    SetColumnWidth(3, wxLIST_AUTOSIZE);
#endif
                }
                break;
            }
        default : //FIXME: not implemented
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
    SaveDocument();
    mMutex.Unlock();

    //event name
    item.SetColumn(0);
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
    duration.undefined = timecode->undefined;
    duration.edit_rate = mEditRate.edit_rate;
    duration.samples = position;
    if (STOP == type) {
        duration.samples--; //previous frame is the last recorded
    }
    item.SetColumn(2);
    item.SetText(Timepos::FormatPosition(duration));
    SetItem(item); //additional to previous data
#ifndef __WIN32__
    SetColumnWidth(2, wxLIST_AUTOSIZE);
#endif

    //project name for stop events
    if (STOP == type && !projectName.IsEmpty()) {
        item.SetColumn(3);
        item.SetText(projectName);
        SetItem(item);
#ifndef __WIN32__
        SetColumnWidth(3, wxLIST_AUTOSIZE);
#endif
    }
    //description for cue events
    else if (CUE == type && !description.IsEmpty()) {
        item.SetColumn(3);
        item.SetText(description);
        SetItem(item);
#ifndef __WIN32__
        SetColumnWidth(3, wxLIST_AUTOSIZE);
#endif
    }
}

///Sets parameters for a first column list item.
/// @param item The item to manipulate.
/// @param eventType Defines the appearance of the item.
/// @param colourIndex Used to set the item colour for CUE event types.
void EventList::SetItemAppearance(wxListItem & item, const EventType type, const size_t colourIndex)
{
    item.SetText(TypeLabels[type]);
    switch (type) {
        case START:
            item.SetTextColour(wxColour(0xFF, 0x20, 0x00));
            item.SetBackgroundColour(wxColour(wxT("WHITE")));
//          font.SetWeight(wxFONTWEIGHT_BOLD); breaks auto col width
//          font.SetStyle(wxFONTSTYLE_ITALIC); breaks auto col width
            break;
        case CUE:
            item.SetTextColour(CuePointsDlg::GetLabelColour(colourIndex));
            item.SetBackgroundColour(CuePointsDlg::GetColour(colourIndex));
            break;
        case CHUNK:
            item.SetTextColour(wxColour(wxT("GREY")));
            item.SetBackgroundColour(wxColour(wxT("WHITE")));
            break;
        case STOP:
            item.SetTextColour(wxColour(wxT("BLACK")));
            item.SetBackgroundColour(wxColour(wxT("WHITE")));
            break;
        default:
            break;
    }
}

/// Generates a new Recording or Chunk node for a new recording or chunk and adds it to the tree.
/// @param timecode Start time
/// @param position Start position relative to start of recording
/// @param description If not the first chunk in a recording, this becomes the description of the previous chunk.
/// @return The new chunk.
wxXmlNode * EventList::NewChunk(ProdAuto::MxfTimecode * timecode, int64_t position, const wxString & description)
{
    mEditRate = *timecode;
    //Add data to XML tree
    mMutex.Lock();
    wxXmlNode * newChunkNode = new wxXmlNode(0, wxXML_ELEMENT_NODE, wxT("Chunk"), wxT(""), new wxXmlProperty(wxT("StartFrame"), wxString::Format(wxT("%d"), position))); //will be attached to another node so no worries about deletion
    if (!GetItemCount() || wxT("Recording") == GetNode(GetItemCount() - 1)->GetName()) { //first chunk in recording
        //create new recording node
        wxXmlNode * recordingNode = SetNextChild(mRootNode, 0, new wxXmlNode(0, wxXML_ELEMENT_NODE, wxT("Recording"), wxT(""), timecode->undefined ? 0 : new wxXmlProperty(wxT("StartTime"), wxString::Format(wxT("%d"), timecode->samples))));
        //new Chunks node
//        newChunkNode->SetParent(new wxXmlNode(recordingNode, wxXML_ELEMENT_NODE, wxT("Chunks"))); This doesn't work!
        (new wxXmlNode(recordingNode, wxXML_ELEMENT_NODE, wxT("Chunks")))->AddChild(newChunkNode);
    }
    else {
        //New Chunk node is sibling of previous Chunk node
        wxXmlNode * prevChunkNode = GetCreatingChunkNode();
        if (!description.IsEmpty()) new wxXmlNode(new wxXmlNode(prevChunkNode, wxXML_ELEMENT_NODE, wxT("Description")), wxXML_CDATA_SECTION_NODE, wxT(""), description);
//        prevChunkNode->SetNext(newChunkNode); This doesn't work!
        prevChunkNode->GetParent()->AddChild(newChunkNode);
    }
    mMutex.Unlock();
    mTotalChunks++;
    LimitListSize();
    return newChunkNode;
}

/// Returns the timecode of the start of the latest take.
/// Assumes mutex is locked.
ProdAuto::MxfTimecode EventList::GetStartTimecode()
{
    ProdAuto::MxfTimecode startTimecode = mEditRate;
    startTimecode.samples = GetNumericalPropVal(GetRecordingNode(GetNode(GetItemCount() - 1)), wxT("StartTime"), 0);
    return startTimecode;
}

/// Returns true if a recording is selected
bool EventList::RecordingIsSelected()
{
    return -1 != GetFirstSelected();
}

/// Adds track and file list data to the latest completed chunk.
/// Assumes there is a completed chunk.
/// @param data Track and file lists for a recorder.
/// @param reload Reload the player (to update with new recording details) if it is showing the latest chunk.
void EventList::AddRecorderData(RecorderData * data, bool reload)
{
    mMutex.Lock();
    wxXmlNode * chunkNode;
    if (wxT("Recording") == GetNode(GetItemCount() - 1)->GetName()) { //a stop event
        //previous list item points to a node in the latest completed chunk
        chunkNode = GetNode(GetItemCount() - 2);
    }
    else {
        //work backwards past a chunk item
        long index = GetItemCount() - 1;
        while (index && wxT("Chunk") != GetNode(index--)->GetName()) {}; //sanity check
        chunkNode = GetNode(index);
    }
    if (wxT("Chunk") != chunkNode->GetName()) chunkNode = chunkNode->GetParent()->GetParent();
    wxXmlNode * filesNode = FindChildNodeByName(chunkNode, wxT("Files"), true); //create if nonexistent
    wxXmlNode * fileNode = 0; //iterate to find last file node
    for (size_t i = 0; i < data->GetTrackList()->length(); i++) {
        if (data->GetTrackList()[i].has_source && strlen(data->GetStringSeq()[i].in())) { //tracks not enabled for record have blank filenames
            fileNode = SetNextChild(filesNode, fileNode, new wxXmlNode(0, wxXML_ELEMENT_NODE, wxT("File"), wxT(""), new wxXmlProperty(wxT("Type"), ProdAuto::VIDEO == (data->GetTrackList())[i].type ? wxT("video") : wxT("audio"))));
            new wxXmlNode(new wxXmlNode(fileNode, wxXML_ELEMENT_NODE, wxT("Label")), wxXML_CDATA_SECTION_NODE, wxT(""), wxString((data->GetTrackList())[i].src.package_name, wxConvLibc));
            new wxXmlNode(new wxXmlNode(fileNode, wxXML_ELEMENT_NODE, wxT("Path")), wxXML_CDATA_SECTION_NODE, wxT(""), wxString((data->GetStringSeq())[i].in(), wxConvLibc));
        }
    }
    SaveDocument();
    //Send an event to reload the player if this programme is selected
    mMutex.Unlock();
    if (InLastTake() && reload) {
        //simulate selecting the start of the recording - NB no way I can see of setting the selection value of the event
        wxListEvent event(wxEVT_COMMAND_LIST_ITEM_SELECTED);
        event.SetEventObject(this);
        mFilesHaveChanged = true; //signals a forced reload
        GetParent()->AddPendingEvent(event);
    }
}

/// Deletes the cue point corresponding to the currently selected event list item, if possible.
void EventList::DeleteCuePoint()
{
    long selected = GetFirstSelected();
    if (-1 < selected) {
        wxMutexLocker lock(mMutex);
        wxXmlNode * cuePointNode = GetNode(selected);
        if (cuePointNode
         && wxT("CuePoint") == cuePointNode->GetName() //currently selected node is a cue point node
         && GetCreatingChunkNode() == cuePointNode->GetParent()->GetParent() //in the last chunk
         && !GetRecordingNode(cuePointNode)->HasProp(wxT("OutFrame"))) //in last recording, and recording
        {
            //remove displayed cue point
            wxListItem item;
            item.SetId(selected);
            DeleteItem(item);
            //remove node
            wxXmlNode * cuePointsNode = cuePointNode->GetParent();
            cuePointsNode->RemoveChild(cuePointNode);
            delete cuePointNode;
            //remove parent if no cue points left
            if (!cuePointsNode->GetChildren()) {
                cuePointsNode->GetParent()->RemoveChild(cuePointsNode);
                delete cuePointsNode;
            }
            SaveDocument();
            mMutex.Unlock();
            //Selected row has gone so select something sensible (the next row, or the last row if the last row was deleted)
            Select(GetItemCount() == selected ? selected - 1 : selected); //Generates an event which will update the delete cue button state
        }
    }
}

/// Returns information about the locators (cue points) in the latest chunk.
/// Assumes chunk has not been completed yet (i.e. a stop event added or another chunk started).
ProdAuto::LocatorSeq EventList::GetLocators()
{

    ProdAuto::LocatorSeq locators;
    wxMutexLocker lock(mMutex);
    wxXmlNode * node = GetNode(GetItemCount() - 1);
    if (wxT("CuePoint") == node->GetName() && !mEditRate.undefined) { //there are cue points; sanity check
        ProdAuto::MxfTimecode startTimecode = mEditRate;
        node = node->GetParent(); //CuePoints node
        startTimecode.samples = GetNumericalPropVal(node->GetParent()->GetParent()->GetParent(), wxT("startTime"), 0);
        node = node->GetChildren(); //first cue point
        while (node) {
            locators.length(locators.length() + 1);
            locators[locators.length() - 1].comment = GetCdata(node).mb_str(wxConvLibc);
            locators[locators.length() - 1].colour = CuePointsDlg::GetColourCode(GetNumericalPropVal(node, wxT("Colour"), 0));
            locators[locators.length() - 1].timecode = startTimecode;
            locators[locators.length() - 1].timecode.samples += GetNumericalPropVal(node, wxT("Frame"), 0);
            locators[locators.length() - 1].timecode.samples %= 24LL * mEditRate.edit_rate.numerator / mEditRate.edit_rate.denominator;
            node = node->GetNext();
        }
    }
    return locators;
}

/// Tries to find the first chunk in the event list containing the given timecode, and jumps to its start (not any cue points) if found.
/// Does not flag the selected event as being changed, to avoid the frame offset being lost by the resulting reload
/// @param offset Sets this to the frame offset within the chunk, if found
/// @return True if timecode found
bool EventList::JumpToTimecode(const ProdAuto::MxfTimecode & tc, int64_t & offset) {
    wxMutexLocker lock(mMutex);
    wxXmlNode * recordingNode = mRootNode->GetChildren();
    bool found = false;
    while (recordingNode && !found) {
        if ((int64_t) GetNumericalPropVal(recordingNode, wxT("StartTime"), 0) + (int64_t) GetNumericalPropVal(recordingNode, wxT("OutFrame"), 0) >= tc.samples) { //desired timecode is somewhere within this recording
            wxXmlNode * chunkNode = FindChildNodeByName(recordingNode, wxT("Chunks"))->GetChildren();
            while (chunkNode && !found) { //sanity check
                if (!chunkNode->GetNext() || (int64_t) GetNumericalPropVal(recordingNode, wxT("StartTime"), 0) + (int64_t) GetNumericalPropVal(chunkNode->GetNext(), wxT("StartFrame"), 0) >= tc.samples) { //desired timecode is somewhere within this chunk
                    found = true; //desired timecode is somewhere within this chunk
                    offset = tc.samples - GetNumericalPropVal(chunkNode, wxT("StartFrame"), 0) - GetNumericalPropVal(recordingNode, wxT("StartTime"), 0);
                    //find and select start of chunk
                    long index = 0;
                    while (index < GetItemCount() - 1 && GetNode(index) != chunkNode) index++;
                    mMutex.Unlock();
                    Select(index, true); //don't flag the selected event as changed, or it will be reloaded without the offset; instead, it must be reloaded manually with the offset
                }
                else {
                    chunkNode = chunkNode->GetNext();
                }
            }
        }
        recordingNode = recordingNode->GetNext();
    }
    return found;
}

/// Populates the XML tree from a file, which will then be used to save to when new events are added.
/// Calls AddEvent() and AddRecorderData() in the same way a live recording would.
/// Does not clear existing events from the event list.
/// Assumes Clear() has been called since the last save so that mutex doesn't have to be locked on changing shared variables.
void EventList::LoadDocument()
{
    wxXmlDocument doc;
    wxXmlNode * loadedRootNode = 0; //to prevent compiler warning
    mEditRate = InvalidMxfTimecode;
    if (wxFile::Exists(mFilename)
     && doc.Load(mFilename)
     && (loadedRootNode = doc.GetRoot())
     && ROOT_NODE_NAME == loadedRootNode->GetName()
    ) {
        //get edit rate (if present; if not, no timecodes will be valid)
        unsigned long long value = GetNumericalPropVal(loadedRootNode, wxT("EditRateNumerator"), 0);
        if (value > 0 && value < LONG_MAX) {
            mEditRate.edit_rate.numerator = (long) value;
            value = GetNumericalPropVal(loadedRootNode, wxT("EditRateDenominator"), 0);
            if (value > 0 && value <= LONG_MAX) {
                mEditRate.edit_rate.denominator = (long) value;
                mEditRate.undefined = false;
                mEditRate.drop_frame = wxT("yes") == loadedRootNode->GetPropVal(wxT("DropFrame"), wxT("no"));
            }
        }
        //iterate through recording nodes
        wxXmlNode * recordingNode = loadedRootNode->GetChildren();
        while (recordingNode) {
            if (wxT("Recording") == recordingNode->GetName()) {
                //find first valid chunk node
                wxXmlNode * chunkNode = FindChildNodeByName(recordingNode, wxT("Chunks"));
                if (chunkNode) {
                    chunkNode = chunkNode->GetChildren();
                    while (chunkNode && wxT("Chunk") != chunkNode->GetName()) {
                        chunkNode = chunkNode->GetNext();
                    }
                    if (chunkNode) {
                        //issue START (or orphaned CHUNK) event
                        ProdAuto::MxfTimecode startTimecode = mEditRate;
                        if ((value = GetNumericalPropVal(recordingNode, wxT("StartTime"), 0)) < LONG_MAX) {
                            startTimecode.samples = value;
                        }
                        else {
                            startTimecode.undefined = true;
                        }
                        AddEvent(START, &startTimecode, GetNumericalPropVal(chunkNode, wxT("StartFrame"), 0)); //becomes a CHUNK event if StartFrame is not zero
                        //go through all chunks in this recording
                        do {
                            //add cue points
                            wxXmlNode * cuePointNode = FindChildNodeByName(chunkNode, wxT("CuePoints"));
                            if (cuePointNode) {
                                cuePointNode = cuePointNode->GetChildren();
                                while (cuePointNode) {
                                    if (wxT("CuePoint") == cuePointNode->GetName()
                                     && ((value = GetNumericalPropVal(cuePointNode, wxT("Frame"), 0)) < LLONG_MAX)
                                    ) {
                                        long long colour = GetNumericalPropVal(cuePointNode, wxT("Colour"), 0);
                                        if ((long long) N_CUE_POINT_COLOURS <= colour) colour = 0; //default colour
                                        AddEvent(CUE, 0, value, GetCdata(cuePointNode), (size_t) colour); //copes if they're in the wrong order
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
                            value = GetNumericalPropVal(nextChunkNode ? nextChunkNode : recordingNode, nextChunkNode ? wxT("StartFrame") : wxT("OutFrame"), 0); //default unknown
                            AddEvent(nextChunkNode ? CHUNK : STOP, 0, value, GetCdata(FindChildNodeByName(chunkNode, wxT("Description"))), 0, false, GetCdata(FindChildNodeByName(recordingNode, wxT("ProjectName")))); //don't select as will look messy and could take ages if loading lots of recordings
                            //add file data
                            wxXmlNode * fileNode = FindChildNodeByName(chunkNode, wxT("Files"));
                            if (fileNode) {
                                fileNode = fileNode->GetChildren();
                                ProdAuto::TrackList_var trackList = new ProdAuto::TrackList;
                                CORBA::StringSeq_var fileList = new CORBA::StringSeq;
                                while (fileNode) {
                                    wxString type;
                                    if (wxT("File") == fileNode->GetName()
                                     && fileNode->GetPropVal(wxT("Type"), &type)
                                    ) {
                                        trackList->length(trackList->length() + 1);
                                        fileList->length(fileList->length() + 1);
                                        trackList[trackList->length() - 1].type = wxT("video") == type ? ProdAuto::VIDEO : ProdAuto::AUDIO;
                                        trackList[trackList->length() - 1].name = "";
                                        fileList[fileList->length() - 1] = GetCdata(FindChildNodeByName(fileNode, wxT("Path"))).mb_str(wxConvLibc);
                                        trackList[trackList->length() - 1].has_source = strlen(fileList[fileList->length() - 1]);
                                        trackList[trackList->length() - 1].src.package_name = GetCdata(FindChildNodeByName(fileNode, wxT("Label"))).mb_str(wxConvLibc);
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

/// Returns a positive numerical value of a property
/// @param node Looks at this for the property; default value returned if this is zero
/// @param propName If not found or value is not a positive integer, default value is returned
/// @param deflt Value to return on error
unsigned long long EventList::GetNumericalPropVal(const wxXmlNode * node, const wxString & propName, const unsigned long long dflt)
{
    long long value = 0;
    wxString propVal;
    if (!node
     || !node->GetPropVal(propName, &propVal)
     || !propVal.ToLongLong(&value) //don't use ToULongLong as it does inappropriate things with negative numbers
     || value < 0
    ) {
        return dflt;
    }
    else {
        return (unsigned long long) value;
    }
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

/// Adds a node as the last in a list of children.
/// Assumes mutex is locked.
/// If the parent node has no children, sets the new node to be its only child, ignoring the child parameter.
/// If the parent node has children and an existing child node is supplied, assumes this is the current last child and sets the new node to be its next sibling, thus avoiding iterating through the list of children.
/// If the parent node has children and the supplied child node is zero, iterates through the children to find the last, and sets the new node to be its next sibling.
/// @param parent Must not be zero.
/// @param child Optional previous last sibling, to avoid iteration.
/// @param newChild The node to be added as a sibling or a child.
/// @return The new child, which is now the last sibling (and can be passed as the child parameter on subsequent calls).
wxXmlNode * EventList::SetNextChild(wxXmlNode * parent, wxXmlNode * child, wxXmlNode * newChild)
{
    if (parent->GetChildren()) {
        if (!child) {
            //find last child
            child = parent->GetChildren();
            while (child->GetNext()) {
                child = child->GetNext();
            }
        }
        child->SetNext(newChild);
    }
    else {
        parent->AddChild(newChild);
    }
    return newChild;
}

/// Saves the XML document asynchronously.
/// Assumes mutex is locked.
void EventList::SaveDocument()
{
    mRunThread = true; //in case thread is busy so misses the signal
    mCondition->Signal();
}

/// Returns true if there are no items in the list.
bool EventList::ListIsEmpty()
{
    return !GetItemCount();
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
            wxXmlNode * currentRecordingNode = GetRecordingNode(GetNode(GetItemCount() - 1));
            if (
             currentRecordingNode //some recordings
             && !currentRecordingNode->HasProp(wxT("OutFrame")) //recording
             && !mEditRate.undefined //protects against divide by zero
             && socketMessage.Length() > 12 //at least space for a timecode
             && socketMessage.Mid(socketMessage.Length() - 11, 2).ToULong(&value) //hours value a number
             && value < 24 //hours value OK
            ) {
                frameCount = value;
                if (socketMessage.Mid(socketMessage.Length() - 8, 2).ToULong(&value) && value < 60) { //minutes value OK
                    frameCount = frameCount * 60 + value;
                    if (socketMessage.Mid(socketMessage.Length() - 5, 2).ToULong(&value) && value < 60) { //seconds value OK
                        frameCount = frameCount * 60 + value;
                        if (socketMessage.Right(2).ToULong(&value)) { //frames value a number
                            frameCount = frameCount * mEditRate.edit_rate.numerator / mEditRate.edit_rate.denominator + value; //now have cue point timecode as number of frames since midnight
                            frameCount -= GetNumericalPropVal(currentRecordingNode, wxT("StartFrame"), 0); //number of frames since start of recording, mod 1 day FIXME: can't handle recordings more than a day long; doesn't check for stupid values
//                            if (frameCount < 0) frameCount += 24 * 60 * 60 * mEditRate.edit_rate.numerator / mEditRate.edit_rate.denominator; //wrap over midnight
                            if (frameCount < 0) frameCount = 0; //convert cue points with times before the start of the recording to the recording start
                            AddEvent(CUE, 0, frameCount, socketMessage.Left(socketMessage.Length() - 12), 0); //use default cue point colour FIXME: doesn't work over midnight
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

/// Allows access to persistent settings
void EventList::SetSavedState(SavedState * savedState)
{
    mSavedState = savedState;
}

/// Returns the saved value for the maximum number of chunks, or a default if mSavedState not set or value not present in it.
unsigned long EventList::GetMaxChunks()
{
    unsigned long maxChunks = DEFAULT_MAX_CHUNKS;
    if (mSavedState) maxChunks = mSavedState->GetUnsignedLongValue(wxT("MaxChunks"), DEFAULT_MAX_CHUNKS);
    return maxChunks;
}

/// Sets the saved value for the maximum number of chunks if mSavedState has been set, and limits the displayed number to the value given.
void EventList::SetMaxChunks(const unsigned long value)
{
    if (mSavedState) mSavedState->SetUnsignedLongValue(wxT("MaxChunks"), value);
    LimitListSize();
}

/// Limits the size of the list to the saved value, by removing chunks from the earliest onwards.
void EventList::LimitListSize()
{
    if (mTotalChunks > GetMaxChunks()) {
        wxMutexLocker lock(mMutex);
        wxXmlNode * chunksNode = FindChildNodeByName(mRootNode->GetChildren(), wxT("Chunks")); //of earliest recording
        if (chunksNode && chunksNode->GetChildren()) { //sanity check
            do {
                wxXmlNode * deletedNode;
                wxListItem item;
                item.SetId(0);
                if (chunksNode->GetChildren()->GetNext()) { //more than one chunk left in this recording
                    deletedNode = chunksNode->GetChildren(); //earliest chunk
                    //remove everything up to the next chunk start
                    while (GetItemCount() && GetNode(0) != deletedNode->GetNext()) DeleteItem(item); //sanity check
                    chunksNode->RemoveChild(deletedNode);
                }
                else {
                    deletedNode = mRootNode->GetChildren(); //whole recording
                    mRootNode->RemoveChild(deletedNode);
                    //remove everything up to and including recording end
                    while (GetItemCount() && GetNode(0) != deletedNode) DeleteItem(item); //sanity check
                    DeleteItem(item);
                }
                delete deletedNode;
                mTotalChunks--;
            } while (mTotalChunks > GetMaxChunks());
            SaveDocument();
            //select earliest item if selected item has been removed
            mMutex.Unlock();
            if (-1 == GetFirstSelected()) Select(0);
        }
    }
}

