/*
 * $Id: Track.cpp,v 1.6 2010/07/21 16:29:34 john_f Exp $
 *
 * A Track in a Package
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

#include "Track.h"
#include "DatabaseEnums.h"
#include "Utilities.h"
#include "Logging.h"
#include "DBException.h"


using namespace std;
using namespace prodauto;


Track::Track()
: DatabaseObject(), id(0), number(0), editRate(g_palEditRate), origin(0), dataDef(0), sourceClip(0)
{}

Track::~Track()
{
    delete sourceClip;
}

string Track::toString()
{
    stringstream trackStr;
    
    trackStr << "ID = " << id << endl;
    trackStr << "Number = " << number << endl;
    trackStr << "Name = " << name << endl;
    switch (dataDef)
    {
        case PICTURE_DATA_DEFINITION:
            trackStr << "Data def = Picture" << endl;
            break;
        
        case SOUND_DATA_DEFINITION:
            trackStr << "Data def = Sound" << endl;
            break;
        
        default:
            assert(false);
            PA_LOGTHROW(DBException, ("Unknown data def"));
    }
    trackStr << "Edit rate = " << editRate.numerator << "/" << editRate.denominator << endl;
    
    if (sourceClip)
    {
        trackStr << "SourceClip:" << endl;
        trackStr << sourceClip->toString();
    }
    
    return trackStr.str();
}

void Track::cloneInPlace(bool resetLengths)
{
    DatabaseObject::cloneInPlace();
    
    if (sourceClip != 0)
    {
        sourceClip->cloneInPlace(resetLengths);
    }
}
    
Track* Track::clone()
{
    Track* clonedTrack = new Track();
    
    DatabaseObject::clone(clonedTrack);
    
    clonedTrack->id = id;
    clonedTrack->number = number;
    clonedTrack->name = name;
    clonedTrack->editRate = editRate;
    clonedTrack->origin = origin;
    clonedTrack->dataDef = dataDef;
    
    if (sourceClip)
    {
        clonedTrack->sourceClip = sourceClip->clone();
    }
    
    return clonedTrack;
}

void Track::toXML(PackageXMLWriter *xml_writer)
{
    xml_writer->WriteElementStart("Track");
    xml_writer->WriteUInt32Attribute("id", id);
    xml_writer->WriteUInt32Attribute("number", number);
    xml_writer->WriteAttribute("name", name);
    xml_writer->WriteRationalAttribute("editRate", editRate);
    xml_writer->WriteInt64Attribute("origin", origin);
    switch (dataDef)
    {
        case PICTURE_DATA_DEFINITION:
            xml_writer->WriteAttribute("dataDef", "Picture");
            break;
        case SOUND_DATA_DEFINITION:
            xml_writer->WriteAttribute("dataDef", "Sound");
            break;
        default:
            PA_ASSERT(false);
    }
    
    if (sourceClip)
        sourceClip->toXML(xml_writer);
    
    xml_writer->WriteElementEnd();
}

void Track::fromXML(PackageXMLReader *xml_reader)
{
    id = xml_reader->ParseUInt32Attr("id");
    number = xml_reader->ParseUInt32Attr("number");
    name = xml_reader->ParseStringAttr("name");
    editRate = xml_reader->ParseRationalAttr("editRate");
    origin = xml_reader->ParseInt64Attr("origin");

    string data_def_str = xml_reader->ParseStringAttr("dataDef");
    if (data_def_str == "Picture")
        dataDef = PICTURE_DATA_DEFINITION;
    else if (data_def_str == "Sound")
        dataDef = SOUND_DATA_DEFINITION;
    else
        throw ProdAutoException("Unknown data definition '%s'\n", data_def_str.c_str());


    xml_reader->ParseChildElements(this);
}

void Track::ParseXMLChild(PackageXMLReader *xml_reader, string name)
{
    if (name == "SourceClip") {
        if (sourceClip)
            throw ProdAutoException("Multiple SourceClips\n");

        auto_ptr<SourceClip> new_source_clip(new SourceClip());
        new_source_clip->fromXML(xml_reader);
        sourceClip = new_source_clip.release();
    }
}

