/***************************************************************************
 *   $Id: savedstate.h,v 1.3 2011/01/14 10:03:40 john_f Exp $             *
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

#ifndef SAVEDSTATE_H
#define SAVEDSTATE_H

#include <wx/wx.h>
#include <wx/xml/xml.h>

/// Information which is saved by the application.
class SavedState : public wxXmlDocument
{
    public:
        SavedState(wxWindow *, const wxString &);
        void Save();
        void SetUnsignedLongValue(const wxString &, const unsigned long, const wxString & = wxEmptyString);
        unsigned long GetUnsignedLongValue(const wxString &, const unsigned long, const wxString & = wxEmptyString);
        void SetStringValue(const wxString &, const wxString &, const wxString & = wxEmptyString);
        wxString GetStringValue(const wxString &, const wxString &, const wxString & = wxEmptyString);
        void SetBoolValue(const wxString &, const bool, const wxString & = wxEmptyString);
        bool GetBoolValue(const wxString &, const bool, const wxString & = wxEmptyString);
        wxXmlNode * GetTopLevelNode(const wxString &, bool = false, bool = false);

    private:
        wxString mFilename;
};

#endif
