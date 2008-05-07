/*
 * $Id: Package.cpp,v 1.3 2008/05/07 17:13:26 philipn Exp $
 *
 * A MXF/AAF Package
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

#include "Package.h"
#include "Database.h"
#include "Utilities.h"
#include "Logging.h"
#include "DBException.h"


using namespace std;
using namespace prodauto;


ProjectName::ProjectName()
: DatabaseObject()
{}

ProjectName::ProjectName(string name_)
: DatabaseObject(), name(name_)
{}

ProjectName::ProjectName(const ProjectName& projectName)
: DatabaseObject(projectName), name(projectName.name)
{}

ProjectName::~ProjectName()
{}

ProjectName& ProjectName::operator=(const std::string& nm)
{
    name = nm;
    wasLoaded(0); // reset to no persistent
    
    return *this;
}

ProjectName& ProjectName::operator=(const ProjectName& nm)
{
    name = nm.name;
    wasLoaded(nm.isPersistent() ? nm.getDatabaseID() : 0);
    
    return *this;
}

bool ProjectName::operator==(const ProjectName& pn) const
{
    return name == pn.name;
}

bool ProjectName::operator<(const ProjectName& pn) const
{
    return name < pn.name;
}


UserComment::UserComment()
: DatabaseObject()
{}

UserComment::UserComment(string n, string v)
: DatabaseObject(), name(n), value(v)
{}

UserComment::UserComment(const UserComment& userComment)
: DatabaseObject(userComment), name(userComment.name), value(userComment.value)
{}

UserComment::~UserComment()
{}



Package::Package()
: DatabaseObject(), uid(g_nullUMID), creationDate(g_nullTimestamp)
{}

Package::~Package()
{
    vector<Track*>::const_iterator iter1;
    for (iter1 = tracks.begin(); iter1 != tracks.end(); iter1++)
    {
        delete *iter1;
    }
}

string Package::toString()
{
    stringstream packageStr;
    
    packageStr << "Name = " << name << endl;
    packageStr << "UID = " << getUMIDString(uid) << endl;
    packageStr << "Creation date = " << getTimestampString(creationDate) << endl;
    packageStr << "Project name = " << projectName.name << endl;
    packageStr << "Tracks:" << endl;
    
    vector<Track*>::const_iterator iter;
    for (iter = tracks.begin(); iter != tracks.end(); iter++)
    {
        packageStr << "Track:" << endl;
        packageStr << (*iter)->toString();
    }
    
    return packageStr.str();
}

Track* Package::getTrack(uint32_t id)
{
    vector<Track*>::const_iterator iter;
    for (iter = tracks.begin(); iter != tracks.end(); iter++)
    {
        if ((*iter)->id == id)
        {
            return *iter;
        }
    }
    
    return 0;
}
    
void Package::addUserComment(string name, string value)
{
    vector<UserComment>::iterator iter;
    for (iter = _userComments.begin(); iter != _userComments.end(); iter++)
    {
        UserComment userComment = *iter;
        if (userComment.name.compare(name) == 0 && userComment.value.compare(value) == 0)
        {
            return; // already have tagged value
        }
    }
    
    _userComments.push_back(UserComment(name, value));
}

vector<UserComment> Package::getUserComments(string name)
{
    vector<UserComment> result;
    
    vector<UserComment>::iterator iter;
    for (iter = _userComments.begin(); iter != _userComments.end(); iter++)
    {
        UserComment& userComment = *iter;
        if (name.compare(userComment.name) == 0)
        {
            result.push_back(userComment);
        }
    }
    
    return result;
}

vector<UserComment> Package::getUserComments()
{
    return _userComments;
}

void Package::clearUserComments()
{
    _userComments.clear();
}

void Package::cloneInPlace(UMID newUID, bool resetLengths)
{
    DatabaseObject::cloneInPlace();
    
    uid = newUID;

    vector<UserComment>::iterator iter1;
    for (iter1 = _userComments.begin(); iter1 != _userComments.end(); iter1++)
    {
        UserComment& userComment = *iter1;
        userComment.cloneInPlace();
    }

    vector<Track*>::const_iterator iter2;
    for (iter2 = tracks.begin(); iter2 != tracks.end(); iter2++)
    {
        (*iter2)->cloneInPlace(resetLengths);
    }
}


MaterialPackage::MaterialPackage()
: Package() 
{}

MaterialPackage::~MaterialPackage()
{}

string MaterialPackage::toString()
{
    stringstream packageStr;
    
    packageStr << "Material Package" << endl;
    packageStr << Package::toString() << endl;

    return packageStr.str();
}



SourcePackage::SourcePackage()
: Package(), descriptor(0)
{}

SourcePackage::~SourcePackage()
{
    delete descriptor;
}

string SourcePackage::toString()
{
    stringstream packageStr;
    
    packageStr << "Source Package" << endl;
    if (descriptor->getType() == FILE_ESSENCE_DESC_TYPE)
    {
        packageStr << "Source config name:" << sourceConfigName << endl;
        FileEssenceDescriptor* fileDesc = dynamic_cast<FileEssenceDescriptor*>(descriptor);
        packageStr << "File essence descriptor:" << endl;
        packageStr << "File location = " << fileDesc->fileLocation << endl;
        packageStr << "File format = " << fileDesc->fileFormat << endl;
        if (fileDesc->videoResolutionID != 0)
        {
            packageStr << "Video resolution ID = " << fileDesc->videoResolutionID << endl;
            packageStr << "Image aspect ratio = " << fileDesc->imageAspectRatio.numerator 
                << ":" << fileDesc->imageAspectRatio.denominator << endl;
        }
        if (fileDesc->audioQuantizationBits != 0)
        {
            packageStr << "Audio bits per sample = " << fileDesc->audioQuantizationBits << endl;
        }
    }
    else if (descriptor->getType() == TAPE_ESSENCE_DESC_TYPE)
    {
        TapeEssenceDescriptor* tapeDesc = dynamic_cast<TapeEssenceDescriptor*>(descriptor);
        packageStr << "Tape essence descriptor:" << endl;
        packageStr << "Spool number = " << tapeDesc->spoolNumber << endl;
    }
    else // LIVE_ESSENCE_DESC_TYPE
    {
        LiveEssenceDescriptor* liveDesc = dynamic_cast<LiveEssenceDescriptor*>(descriptor);
        assert(liveDesc != 0);
        packageStr << "Live essence descriptor:" << endl;
        packageStr << "Recording location = '" << liveDesc->recordingLocation << "'" << endl;
    }

    packageStr << Package::toString() << endl;

    return packageStr.str();
}

void SourcePackage::cloneInPlace(UMID newUID, bool resetLengths)
{
    Package::cloneInPlace(newUID, resetLengths);
    
    if (descriptor != 0)
    {
        descriptor->cloneInPlace();
    }
}


EssenceDescriptor::EssenceDescriptor()
: DatabaseObject()
{}


FileEssenceDescriptor::FileEssenceDescriptor()
: EssenceDescriptor(), fileFormat(0), videoResolutionID(0), imageAspectRatio(g_nullRational),
  audioQuantizationBits(0)
{}

TapeEssenceDescriptor::TapeEssenceDescriptor()
: EssenceDescriptor()
{}

LiveEssenceDescriptor::LiveEssenceDescriptor()
: EssenceDescriptor(), recordingLocation(0)
{}




