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
#include "ticktree.h"
#include "small_disabled.xpm"
#include "small_enabled.xpm"
#include "small_partial_enabled.xpm"
#include "small_rec.xpm"
#include "small_partial_rec.xpm"
#include "small_exclamation.xpm"
#include "small_question.xpm"
#include <wx/imaglist.h>
#include "wx/xml/xml.h"

WX_DECLARE_STRING_HASH_MAP(wxTreeItemId, TreeItemHash);
WX_DECLARE_STRING_HASH_MAP(wxString, StringHash);
WX_DECLARE_HASH_MAP(int, wxString, wxIntegerHash, wxIntegerEqual, IntStringHash);

DEFINE_EVENT_TYPE(wxEVT_TREE_MESSAGE)

BEGIN_EVENT_TABLE(TickTreeCtrl, wxTreeCtrl)
	EVT_LEFT_DOWN(TickTreeCtrl::OnLMouseDown)
END_EVENT_TABLE()

/// Displays empty tree, disabled.
/// @param parent The parent window.
/// @param id The control's window ID.
/// @param pos The control's position.
/// @param size The control's size.
/// @param name The displayed name of the root node.
TickTreeCtrl::TickTreeCtrl(wxWindow * parent, wxWindowID id, const wxPoint& pos, const wxSize& size, const wxString & name) :
	wxTreeCtrl(parent, id, pos, size), mEnableChanges(false)
{
	SetNextHandler(parent);
	wxImageList * mImages = new wxImageList();
	mImages->Create(16, 16);
	//image loading order must correspond to enum order
	mImages->Add((const wxIcon &) small_disabled); //cast needed for cygwin
	mImages->Add((const wxIcon &) small_partial_enabled); //cast needed for cygwin
	mImages->Add((const wxIcon &) small_enabled); //cast needed for cygwin
	mImages->Add((const wxIcon &) small_rec); //cast needed for cygwin
	mImages->Add((const wxIcon &) small_partial_rec); //cast needed for cygwin
	mImages->Add((const wxIcon &) small_exclamation); //cast needed for cygwin
	mImages->Add((const wxIcon &) small_question); //cast needed for cygwin

	AssignImageList(mImages);

	SetItemData(AddRoot(name, DISABLED), new ItemData());
	Disable();
//	Unselect();
}

/// Adds a recorder to the bottom of the tree.
/// Arranges sources by package name, and displays tape IDs for packages names if found.
/// Assumes sources that are recording are enabled for recording and all others aren't.
/// Updates overall tree state and sends a notification event.
/// @param name The recorder name.
/// @param trackList Information about the sources and package names.
/// @param trackStatusList Whether each source is recording or not.
/// @param routerRecorder True if this recorder is a router recorder - all sources (should be only one anyway) will be regarded as router sources and tape IDs will not be used.
/// @param doc XML document object for tape ID information.
/// @return True if there are any packages names anywhere which can have tape IDs (i.e. not router recorder package names).
bool TickTreeCtrl::AddRecorder(const wxString & name, ProdAuto::TrackList_var trackList, ProdAuto::TrackStatusList_var trackStatusList, bool routerRecorder, wxXmlDocument & doc)
{
	Enable();
	//make recorder root node
	wxTreeItemId recorderRoot = AppendItem(GetRootItem(), name, DISABLED);
	//make recorder package (branch) and source (terminal) nodes
	wxXmlNode * firstTapeIdNode = GetFirstTapeIdNode(doc);
	TreeItemHash packageNameTreeNodes;
	for (unsigned int i = 0; i < trackList->length(); i++) {
		if (trackList[i].has_source) { //something's plugged into this input
			//group tracks by package name
			wxString packageName = wxString(trackList[i].src.package_name, *wxConvCurrent);
			if (packageNameTreeNodes.end() == packageNameTreeNodes.find(packageName)) {
				//haven't come across this package name: does it have a tape ID?
				wxXmlNode * tapeIdNode = firstTapeIdNode;
				while (tapeIdNode && packageName != tapeIdNode->GetPropVal(wxT("PackageName"), wxT(""))) {
					tapeIdNode = tapeIdNode->GetNext();
				}
				wxString tapeId;
				if (tapeIdNode && tapeIdNode->GetChildren()) {
					tapeId = tapeIdNode->GetChildren()->GetContent();
				}
				//create a branch node and remember its ID
				if (tapeId.IsEmpty() || routerRecorder) {
					packageNameTreeNodes[packageName] = AppendItem(recorderRoot, packageName, DISABLED); //default: no sources recording
				}
				else {
					packageNameTreeNodes[packageName] = AppendItem(recorderRoot, packageName + wxT(" (") + tapeId + wxT(")"), DISABLED); //default: no sources recording. WARNING: this formatting is unpicked elsewhere to retrieve the tape ID
				}
				ItemData * data = new ItemData(packageName, routerRecorder, !tapeId.IsEmpty() || routerRecorder);
				SetItemData(packageNameTreeNodes[packageName], data); //remember package name for when messing about with tape IDs
			}
			//create a node for the track name (source) on the appropriate branch
			wxTreeItemId id = AppendItem(packageNameTreeNodes[packageName], wxString(trackList[i].src.track_name, *wxConvCurrent) + wxString(wxT(" via ")) + wxString(trackList[i].name, *wxConvCurrent), trackStatusList[i].rec ? RECORDING : DISABLED);
			ItemData * data = new ItemData(i, trackStatusList[i].rec); //If recording initially, assume it's enabled to record
			SetItemData(id, data); //tree deletes this
			if (routerRecorder) {
				SetItemBackgroundColour(id, ROUTER_SOURCE_COLOUR);
			}
			else {
				SetItemBackgroundColour(id, trackList[i].type == ProdAuto::VIDEO ? VIDEO_SOURCE_COLOUR : AUDIO_SOURCE_COLOUR);
			}
		}
	}
	ItemData * data = new ItemData(trackList->length());
	SetItemData(recorderRoot, data);
	//reflect status of source nodes in package nodes, and sort source nodes
	TreeItemHash::iterator it;
	for (it = packageNameTreeNodes.begin(); it != packageNameTreeNodes.end(); ++it) {
		wxTreeItemIdValue cookie;
		wxTreeItemId source = GetFirstChild(it->second, cookie);
		bool recording = false;
		bool allRecording = true;
		while (source.IsOk()) {
			recording |= (RECORDING == GetItemImage(source));
			allRecording &= (RECORDING == GetItemImage(source));
			source = GetNextChild(it->second, cookie);
		}
		if (recording) {
			SetItemImage(it->second, allRecording ? RECORDING : PARTIALLY_RECORDING);
		}
//		SortChildren(it->second); don't do this as the order they arrive in is useful
	}
	//sort package name nodes
//	SortChildren(recorderRoot); don't do this as the order they arrive in is useful
	//sort recorder nodes
	SortChildren(GetRootItem());
	//reflect status of package nodes in recorder and root, and tell frame the status
	ReportState(packageNameTreeNodes.begin()->second); //any package-level ID will do
	//find out if there are any non router-recorder package names
	wxArrayString names;
	std::vector<bool> enabled;
	ScanPackageNames(&names, &enabled);
	return names.GetCount();
}

/// Removes a recorder from the tree.
/// Updates overall tree state and sends a notification event.
/// @param recorderName The name of the recorder to remove.
/// @return True if there are any packages names anywhere which can have tape IDs (i.e. not router recorder package names).
bool TickTreeCtrl::RemoveRecorder(const wxString & recorderName)
{
	wxTreeItemId recorder = FindRecorder(recorderName);
	if (recorder.IsOk()) {
		Delete(recorder);
		if (GetCount() < 2) {
			//No recorders
			Disable();
			ReportState(GetRootItem());
		}
		else {
			wxTreeItemIdValue dummyCookie;
			ReportState(GetFirstChild(GetRootItem(), dummyCookie)); //any recorder-level ID will do
		}
	}
	//find out if there are any non router-recorder package names
	wxArrayString names;
	std::vector<bool> enabled;
	ScanPackageNames(&names, &enabled);
	return names.GetCount();
}

/// Finds the root node of a recorder by recorder name.
/// @param recorderName The name to search for.
/// @return The root node of the recorder (may not be valid).
wxTreeItemId TickTreeCtrl::FindRecorder(const wxString & recorderName)
{
	wxTreeItemIdValue cookie;
	wxTreeItemId recorder = GetFirstChild(GetRootItem(), cookie);
	while (recorder.IsOk() && GetItemText(recorder) != recorderName) {
		recorder = GetNextChild(GetRootItem(), cookie);
	}
	return recorder;
}

/// Returns the tree to the initial state.
void TickTreeCtrl::Clear()
{
	CollapseAndReset(GetRootItem());
	SetItemImage(GetRootItem(), DISABLED);
	Disable();
}

/// Responds to a left mouse down event by toggling the record state of the clicked node and all nodes below, if enabled.
/// Updates overall tree state and sends a notification event.
/// @param event The mouse event.
void TickTreeCtrl::OnLMouseDown(wxMouseEvent & event)
{
	int flags;
	wxTreeItemId id = HitTest(event.GetPosition(), flags);
	if (mEnableChanges && (flags & wxTREE_HITTEST_ONITEMICON)) {
		//apply selection state to this and all children recursively
		if (DISABLED == GetItemImage(id) || PARTIALLY_ENABLED == GetItemImage(id)) {
			//record enable
			SelectRecursively(id);
		}
		else {
			//record disable
			SelectRecursively(id, false);
		}
		//report selection state to all parents recursively
		ReportState(id);
	}
	event.Skip(); //to allow event to be used for other things, i.e. item selection
}

/// Indicates whether or not any sources are enabled for recording.
/// @return True if any sources enabled.
bool TickTreeCtrl::SomeEnabled()
{
	return DISABLED != GetItemImage(GetRootItem());
}

/// Indicates whether all enabled sources' packages have tape IDs.
/// @return True if all tape IDs present.
bool TickTreeCtrl::TapeIdsOK()
{
	return ((ItemData *) GetItemData(GetRootItem()))->GetBool();
}

/// Indicates whether any recorders have a problem.
/// @return True if any problems.
bool TickTreeCtrl::Problem()
{
	return PROBLEM == GetItemImage(GetRootItem());
}

/// Indicates whether at least one source is recording and there are no problems.
/// @return True if recording with no problems.
bool TickTreeCtrl::RecordingSuccessfully()
{
	return RECORDING == GetItemImage(GetRootItem()) || PARTIALLY_RECORDING == GetItemImage(GetRootItem());
}

/// Travels from the given point in the tree away from the root, changing the state of all items to the state given.
/// @param id Starting node.
/// @param state Enabled for recording if true.
/// @param unknown Unknown state if true.  Overrides state parameter.
/// @param problem Problem if true.  Overrides state parameter.
/// @return True if there are tape IDs for every enabled source.
bool TickTreeCtrl::SelectRecursively(wxTreeItemId id, bool state, bool unknown, bool problem)
{
	//apply selection state to this node
	if (problem) {
		SetItemImage(id, PROBLEM);
	}
	else if (unknown) {
		SetItemImage(id, UNKNOWN);
	}
	else if (state) {
		SetItemImage(id, ENABLED);
	}
	else {
		SetItemImage(id, DISABLED);
	}
	//apply selection state to all children recursively
	wxTreeItemId child;
	wxTreeItemIdValue cookie;
	bool tapeIdsOK = true;
	child = GetFirstChild(id, cookie);
	while (child.IsOk()) {
		//set child's state and note tape ID status
		tapeIdsOK &= SelectRecursively(child, state, unknown, problem);
		child = GetNextChild(id, cookie);
	}
	if (!GetFirstChild(id, cookie).IsOk()) { //this node doesn't have children so represents a source
		if (!unknown && !problem) {
			//set record enable state
			((ItemData *) GetItemData(id))->SetBool(state);
		}
	}
	else if (!((ItemData *) GetItemData(id))->GetString().IsEmpty()) { //this is a package level node
		//ignore tape ID result from lower level because tape ID is at this level
		tapeIdsOK = (DISABLED == GetItemImage(id) || ((ItemData *) GetItemData(id))->GetBool());
	}
	else { //this is a higher level node
		//reflect tape ID result from lower nodes
		((ItemData *) GetItemData(id))->SetBool(tapeIdsOK);
	}
	return tapeIdsOK;
}

/// Changes the state of the supplied node's parent to reflect the state of supplied node and its siblings.
/// If the parent is the root node, sends a notification event
/// @param id Starting node.
/// @param recurse True to recurse to the root (and always send a notification event).
void TickTreeCtrl::ReportState(wxTreeItemId id, bool recurse) {
	wxTreeItemId parent = GetItemParent(id);
	if (parent.IsOk()) { //below root level
		bool allSelected = true;
		bool allUnselected = true;
		bool problem = false;
		bool unknown = false;
		bool recording = false;
		bool tapeIdsOK = true;
		wxTreeItemIdValue cookie;
		wxTreeItemId sibling = GetFirstChild(parent, cookie);
		//determine state from all siblings
		while (sibling.IsOk()) {
			if (PROBLEM == GetItemImage(sibling)) {
				problem = true; //takes priority over everything
				break; //no need to look any further
			}
			else if (UNKNOWN == GetItemImage(sibling)) {
				unknown = true; //takes priority over everything
				break; //no need to look any further
			}
			else {
				if (DISABLED != GetItemImage(sibling)) {
					allUnselected = false;
					//if this is a package name node, we need a tape ID if it's enabled for recording and it hasn't got one; if it's a higher level node this will just pass the value on
					tapeIdsOK &= ((ItemData *) GetItemData(sibling))->GetBool();
				}
				if (ENABLED != GetItemImage(sibling) && RECORDING != GetItemImage(sibling)) {
					allSelected = false;
				}
				if (RECORDING == GetItemImage(sibling) || PARTIALLY_RECORDING == GetItemImage(sibling)) {
					recording = true; //takes priority over non-recording images
				}
			}
			sibling = GetNextChild(parent, cookie);
		}
		if (problem) {
			SetItemImage(parent, PROBLEM);
		}
		else if (unknown) {
			SetItemImage(parent, UNKNOWN);
		}
		else if (allUnselected) {
			SetItemImage(parent, DISABLED);
		}
		else if (allSelected) {
			if (recording) {
				SetItemImage(parent, RECORDING);
			}
			else {
				SetItemImage(parent, ENABLED);
			}
		}
		else {
			if (recording) {
				SetItemImage(parent, PARTIALLY_RECORDING);
			}
			else {
				SetItemImage(parent, PARTIALLY_ENABLED);
			}
		}
		wxTreeItemIdValue dummyCookie;
		if (GetFirstChild(id, dummyCookie).IsOk()) { //this node has children so is not a source node
			//set tape ID status for parent
			((ItemData *) GetItemData(parent))->SetBool(tapeIdsOK);
		}
		if (recurse) {
			ReportState(parent);
		}
	}
	else { //the root item
		//tell the frame
		wxCommandEvent guiEvent(wxEVT_TREE_MESSAGE, wxID_ANY);
		AddPendingEvent(guiEvent);
	}
}

/// Allow or disallow record enables to be changed with the mouse.
/// @param enable True to allow changes.
void TickTreeCtrl::EnableChanges(bool enable)
{
	mEnableChanges = enable;
}

/// Returns true if any sources enabled, and not already recording
/// Provides record enable and tape ID data for a recorder.
/// @param recorderName The recorder.
/// @param enableList Returns enable/disable state for each source, in recorder source order.
/// @param tapeIDs Returns the tape ID for each video source, in recorder source order.
/// @return True if any sources are enabled but not recording.
bool TickTreeCtrl::GetEnableStates(const wxString & recorderName, CORBA::BooleanSeq & enableList, CORBA::StringSeq & tapeIds)
{
	wxTreeItemId recorder = FindRecorder(recorderName);
	bool someEnabled = false;
	IntStringHash tapeIdHash;
	if (recorder.IsOk()) {
		//get the number of sources
		enableList.length(((ItemData *) GetItemData(recorder))->GetInt());
		//default enable values in case there are unused sources
		for (unsigned int i = 0; i < enableList.length(); i++) {
			enableList[i] = false;
		}
		//fill in the values
		wxTreeItemIdValue packageCookie, sourceCookie;
		wxTreeItemId package = GetFirstChild(recorder, packageCookie);
		while (package.IsOk()) {
			wxTreeItemId source = GetFirstChild(package, sourceCookie);
			while (source.IsOk()) {
				ItemData * itemData = (ItemData*) GetItemData(source);
				enableList[itemData->GetInt()] = itemData->GetBool();
				someEnabled |= (itemData->GetBool() && RECORDING != GetItemImage(source));
				if (VIDEO_SOURCE_COLOUR == GetItemBackgroundColour(source)) { //video source - has an associated tape ID
					//store the tape ID keyed by the source index to sort them correctly (whether enabled or not is irrelevant)
					tapeIdHash[itemData->GetInt()] = GetItemText(package).Mid(((ItemData*) GetItemData(package))->GetString().Len() + 2); //remove characters up to start of tape Id
					tapeIdHash[itemData->GetInt()].Truncate(tapeIdHash[itemData->GetInt()].Len() - 1); //remove trailing bracket
				}
				source = GetNextChild(package, sourceCookie);
			}
			package = GetNextChild(recorder, packageCookie);
		}
		//put the tape IDs in the list in order
		tapeIds.length(tapeIdHash.size());
		int index = 0;
		for (IntStringHash::iterator it = tapeIdHash.begin(); it != tapeIdHash.end(); it++) {
			tapeIds[index++] = it->second.mb_str(*wxConvCurrent); //mb_str() returns const so will definitely be copied
		}
	}
	return someEnabled;
}

/// @param recorderName The recorder.
/// Reports to the root, and generates a notification event.
void TickTreeCtrl::SetRecorderStateUnknown(const wxString & recorderName)
{
	SetRecorderState(recorderName, false);
}

/// @param recorderName The recorder.
/// Reports to the root, and generates a notification event.
void TickTreeCtrl::SetRecorderStateProblem(const wxString & recorderName)
{
	SetRecorderState(recorderName, true);
}

/// Sets given recorder's state to problem or unknown.
/// Reports to the root, and generates a notification event.
/// @param recorderName The recorder.
/// @param problem True for problem; false for unknown.
void TickTreeCtrl::SetRecorderState(const wxString & recorderName, bool problem)
{
	wxTreeItemId recorderRoot = FindRecorder(recorderName);
	if (recorderRoot.IsOk()) { //prob unnecessary
		SelectRecursively(recorderRoot, false, true, problem);
	}
	ReportState(recorderRoot);
}

/// Sets state of sources for given recorder.
/// Reports to the root, and generates a notification event.
/// @param recorderName The recorder.
/// @param recording True if the recorder is recording.
/// @param trackStatus enabled/recording or disabled for each of the recorder's sources.
void TickTreeCtrl::SetTrackStatus(const wxString & recorderName, bool recording, ProdAuto::TrackStatusList_var trackStatus)
{
	wxTreeItemId recorder = FindRecorder(recorderName);
	wxTreeItemIdValue packageCookie, sourceCookie;
	wxTreeItemId package;
	if (recorder.IsOk()) {
		for (unsigned int index = 0; index < trackStatus->length(); index++) {
			//find the source
			package = GetFirstChild(recorder, packageCookie);
			bool found = false;
			while (!found && package.IsOk()) {
				wxTreeItemId source = GetFirstChild(package, sourceCookie);
				while (source.IsOk() && !(found = ((ItemData *) GetItemData(source))->GetInt() == (int) index)) {
					source = GetNextChild(package, sourceCookie);
				}
				if (found) {
					//set the state
					if (recording && trackStatus[index].rec && ((ItemData *) GetItemData(source))->GetBool()) {
						//recording and supposed to be
						SetItemImage(source, RECORDING);
					}
					else if (!trackStatus[index].rec && !((ItemData *) GetItemData(source))->GetBool()) {
						//not recording and not supposed to be
						SetItemImage(source, DISABLED);
					}
					else if (!recording && !trackStatus[index].rec && ((ItemData *) GetItemData(source))->GetBool()) {
						//not recording and not supposed to be, but enabled
						SetItemImage(source, ENABLED);
					}
					else {
						//anomalous situation
						SetItemImage(source, PROBLEM);
					}
				}
				else {
					package = GetNextChild(recorder, packageCookie);
				}
			}
		}
		//determine state of each package of sources
		package = GetFirstChild(recorder, packageCookie);
		while (package.IsOk()) {
			ReportState(GetFirstChild(package, sourceCookie), false); //don't recurse because it just adds overhead at this stage
			package = GetNextChild(recorder, packageCookie);
		}
		//report all source packages to root
		package = GetFirstChild(recorder, packageCookie);
		ReportState(package);
	}
}

/// Obtains all (non router-recorder) package names and corresponding enable status.
/// @param names Returns package names.
/// @param enabled Returns true for each corresponding package in names that's enabled or partially enabled; false otherwise.
void TickTreeCtrl::GetPackageNames(wxArrayString & names, std::vector<bool> & enabled)
{
	ScanPackageNames(&names, &enabled);
}

/// Sets all (non router-recorder) package names' corresponding tape IDs (which may not exist).
/// Reports to the root, and generates a notification event.
/// @param doc XML document object for tape ID information
void TickTreeCtrl::UpdateTapeIds(wxXmlDocument & doc)
{
	//make a hash of all tape IDs
	StringHash tapeIds;
	wxString packageName;
	wxXmlNode * tapeIdNode = GetFirstTapeIdNode(doc);
	while (tapeIdNode) {
		packageName = tapeIdNode->GetPropVal(wxT("PackageName"), wxT(""));
		if (wxT("TapeId") == tapeIdNode->GetName() && !packageName.IsEmpty() && !tapeIdNode->GetNodeContent().IsEmpty()) {
			tapeIds[packageName] = tapeIdNode->GetNodeContent();
		}
		tapeIdNode = tapeIdNode->GetNext();
	}
	//update the tree
	ScanPackageNames(0, 0, &tapeIds);
}

/// Either returns all package names, with corresponding enabled status, or updates all tape IDs.
/// Skips package names that belong to router recorders.
/// If updating, reports to the root, and generates a notification event.
/// @param names If 0, updates; if not, returns package names.
/// @param enabled If returning package names, returns true for each corresponding package in names that's enabled or partially enabled; false otherwise.  Ignored if not returning package names.
/// @param tapeIDs If updating, provides a set of mappings from package name to tape ID.
void TickTreeCtrl::ScanPackageNames(wxArrayString * names, std::vector<bool> * enabled, StringHash * tapeIds)
{
	wxString packageName;
	wxTreeItemIdValue recordersCookie;
	wxTreeItemId recorder = GetFirstChild(GetRootItem(), recordersCookie);
	while (recorder.IsOk()) {
		wxTreeItemIdValue packagesCookie;
		wxTreeItemId package = GetFirstChild(recorder, packagesCookie);
		while (package.IsOk()) {
			packageName = ((ItemData *) GetItemData(package))->GetString();
			if (((ItemData *) GetItemData(package))->GetInt()) { //a router recorder
				//doesn't have a tape ID so ignore
			}
			else if (names) { //scanning package names
				names->Add(packageName);
				enabled->push_back(ENABLED == GetItemImage(package) || PARTIALLY_ENABLED == GetItemImage(package));
			}
			else if (tapeIds->end() == tapeIds->find(packageName)) { //updating: no tape ID
				SetItemText(package, packageName);
				((ItemData *) GetItemData(package))->SetBool(false);
			}
			else { //updating: tape ID
				SetItemText(package, packageName + wxT(" (") + (*tapeIds)[packageName] + wxT(")")); //WARNING: this formatting is unpicked elsewhere to retrieve the tape ID
				((ItemData *) GetItemData(package))->SetBool(true);
			}
			package = GetNextChild(recorder, packagesCookie);
		}
		if (!names) { //updating
			//report tape ID state upwards for this recorder
			ReportState(GetFirstChild(recorder, packagesCookie), false); //don't recurse because it just adds overhead at this stage
		}
		recorder = GetNextChild(GetRootItem(), recordersCookie);
	}
	if (!names) { //updating
		//report all recorder states upwards
		ReportState(GetFirstChild(GetRootItem(), recordersCookie));
	}
}

/// Provides the first tape ID node from the XML document.
/// @param doc XML document object for tape ID information.
/// @return The first node, or 0 if no such node.
wxXmlNode * TickTreeCtrl::GetFirstTapeIdNode(wxXmlDocument & doc)
{
	wxXmlNode * mTapeIdsNode = doc.GetRoot()->GetChildren();
	while (mTapeIdsNode && mTapeIdsNode->GetName() != wxT("TapeIds")) {
		mTapeIdsNode = mTapeIdsNode->GetNext();
	}
	if (mTapeIdsNode) {
		return mTapeIdsNode->GetChildren();
	}
	else {
		return 0;
	}
}
