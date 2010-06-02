/*
 * $Id: SourceClip.cpp,v 1.3 2010/06/02 13:04:40 john_f Exp $
 *
 * A Source Clip in a Track referencing a Package or null
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#include <cassert>
#include <sstream>

#include "SourceClip.h"
#include "Utilities.h"
#include "DBException.h"


using namespace std;
using namespace prodauto;


SourceClip::SourceClip()
: DatabaseObject(), sourcePackageUID(g_nullUMID), sourceTrackID(0), length(0), position(0)
{}

SourceClip::~SourceClip()
{}

string SourceClip::toString()
{
    stringstream sourceClipStr;
    
    sourceClipStr << "Source package UID = " << getUMIDString(sourcePackageUID) << endl;
    sourceClipStr << "Source track ID = " << sourceTrackID << endl;
    sourceClipStr << "Length = " << length << endl;
    sourceClipStr << "Position = " << position << endl;
    
    return sourceClipStr.str();
}


void SourceClip::cloneInPlace(bool resetLength)
{
    DatabaseObject::cloneInPlace();
    
    if (resetLength)
    {
        length = 0;
    }
}

SourceClip* SourceClip::clone()
{
    SourceClip* clonedSourceClip = new SourceClip();
    
    DatabaseObject::clone(clonedSourceClip);
    
    clonedSourceClip->sourcePackageUID = sourcePackageUID;
    clonedSourceClip->sourceTrackID = sourceTrackID;
    clonedSourceClip->length = length;
    clonedSourceClip->position = position;
    
    return clonedSourceClip;
}

void SourceClip::toXML(PackageXMLWriter *xml_writer)
{
    xml_writer->WriteElementStart("SourceClip");
    xml_writer->WriteUMIDAttribute("sourcePackageUID", sourcePackageUID);
    xml_writer->WriteUInt32Attribute("sourceTrackID", sourceTrackID);
    xml_writer->WriteInt64Attribute("length", length);
    xml_writer->WriteInt64Attribute("position", position);
    xml_writer->WriteElementEnd();
}

