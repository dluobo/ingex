/***************************************************************************
 *   $Id: savedstate.cpp,v 1.3 2011/01/14 10:03:40 john_f Exp $           *
 *                                                                         *
 *   Copyright (C) 2010 British Broadcasting Corporation                   *
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

#include "savedstate.h"
#include "wx/stdpaths.h"
#include "wx/file.h"
#include "wx/filename.h"

/// Tries to open an existing file; if not, creates a new one.
/// Clears contents if unexpected root node name.
/// Reports problems in a message box.
/// @param parent Window to parent message boxes.
/// @param filename Name only of file - will be created in standard user config directory.
SavedState::SavedState(wxWindow * parent, const wxString & filename)
{
    mFilename = wxStandardPaths::Get().GetUserConfigDir() + wxFileName::GetPathSeparator() + filename;
    bool ok = false;
    if (!wxFile::Exists(mFilename)) {
        wxMessageDialog dlg(parent, wxT("No saved preferences file found.  Default values will be used."), wxT("No saved preferences"));
        dlg.ShowModal();
    }
    else if (!Load(mFilename)) { //already produces a warning message box
    }
    else if (wxT("Ingexgui") != GetRoot()->GetName()) {
        wxMessageDialog dlg(parent, wxT("Saved preferences file \"") + mFilename + wxT("\": has unrecognised data: re-creating with default values."), wxT("Unrecognised saved preferences"), wxICON_EXCLAMATION | wxOK);
        dlg.ShowModal();
    }
    else {
        ok = true;
    }
    if (!ok) {
        SetRoot(new wxXmlNode(wxXML_ELEMENT_NODE, wxT("Ingexgui")));
        Save();
    }
}

void SavedState::Save()
{
    wxXmlDocument::Save(mFilename);
}

/// Sets the content or an attribute of a node just below root level to be "Yes" or "No".
/// @param nodeName The node name.  Will be created if it doesn't already exist.
/// @param value The value to set.
/// @param attrName The optional attribute name to use, instead of using the node content.
void SavedState::SetUnsignedLongValue(const wxString & nodeName, const unsigned long value, const wxString & attrName)
{
    SetStringValue(nodeName, wxString::Format(wxT("%lu"), value), attrName);
}

/// Gets the content or an attribute content of a node just below root level, where the value of that node/attribute is expected to be resolvable to an unsigned long.
/// @param nodeName The node name to search for.
/// @param deflt A default value to return in case the node wasn't found or its value is invalid.
/// @param attrName The optional attribute name to search for.
unsigned long SavedState::GetUnsignedLongValue(const wxString & nodeName, const unsigned long deflt, const wxString & attrName)
{
    unsigned long value = deflt;
    GetStringValue(nodeName, wxString::Format(wxT("%lu"), deflt), attrName).ToULong(&value); //doesn't change value if string not recognised
    return value;
}

/// Sets the content or an attribute of a node just below root level to be "Yes" or "No".
/// @param nodeName The node name.  Will be created if it doesn't already exist.
/// @param value The value to set (true becomes "Yes").
/// @param attrName The optional attribute name to use, instead of using the node content.
void SavedState::SetBoolValue(const wxString & nodeName, const bool value, const wxString & attrName)
{
    SetStringValue(nodeName, value ? wxT("Yes") : wxT("No"), attrName);
}

/// Gets the content or an attribute of a node just below root level, expected to be "Yes" or "No", as a boolean.
/// @param nodeName The node name to search for.
/// @param deflt A default value to return if the node or attribute wasn't found.
/// @param attrName The optional attribute name to search for.
/// @return true if the attribute/content was "Yes"
bool SavedState::GetBoolValue(const wxString & nodeName, const bool deflt, const wxString & attrName)
{
    return wxT("Yes") == GetStringValue(nodeName, deflt ? wxT("Yes") : wxT("No"), attrName);
}

/// Sets the content or an attribute of a node just below root level to be the given string value.
/// @param nodeName The node name.  Will be created if it doesn't already exist.
/// @param value The value to set.
/// @param attrName The optional attribute name to use, instead of using the node content.
void SavedState::SetStringValue(const wxString & nodeName, const wxString & value, const wxString & attrName)
{
    if (wxEmptyString == attrName) {
        wxXmlNode * node = GetTopLevelNode(nodeName, true, true); //remove existing node if present
        new wxXmlNode(node, wxXML_TEXT_NODE, wxEmptyString, value);
    }
    else {
        wxXmlNode * node = GetTopLevelNode(nodeName, true); //create if not present
        node->DeleteProperty(attrName); //could perhaps turn this into a loop to delete all instances, but docn doesn't confirm that the return value from DeleteProperty indicates that a property has been deleted
        node->AddProperty(new wxXmlProperty(attrName, value));
    }
    Save();
}

/// Gets the content or an attribute content of a node just below root level, as a string.
/// @param nodeName The node name to search for.
/// @param deflt A default string to return in case the node or attribute wasn't found.
/// @param attrName The optional attribute name to search for.
wxString SavedState::GetStringValue(const wxString & nodeName, const wxString & deflt, const wxString & attrName)
{
    wxXmlNode * node = GetTopLevelNode(nodeName);
    wxString value;
    if (node) {
        if (wxEmptyString == attrName) {
            value = node->GetNodeContent();
        }
        else {
            value = node->GetPropVal(attrName, deflt);
        }
    }
    else {
        value = deflt;
    }
    return value;
}

/// Gets a node just below the root with the given name.
/// @param create Create the node if not found.
/// @param clear Delete all nodes (and their children) with the given name, and create a new node
/// @return The named node; zero if not found and not created.
wxXmlNode * SavedState::GetTopLevelNode(const wxString & name, bool create, bool clear)
{
    wxXmlNode * node = GetRoot()->GetChildren();
    if (clear) {
        //remove all nodes with this name
        while (node) {
            if (node->GetName() == name) {
                wxXmlNode * deadNode = node;
                node = node->GetNext(); //do this before deleting node to avoid breaking the chain
                GetRoot()->RemoveChild(deadNode);
                delete deadNode;
            }
            else {
                node = node->GetNext();
            }
        }
    }
    else {
        //find the first node with this name
        while (node && node->GetName() != name) {
            node = node->GetNext();
        }
    }
    if (clear || (create && !node)) {
        node = new wxXmlNode(GetRoot(), wxXML_ELEMENT_NODE, name);
    }
    return node;
}
