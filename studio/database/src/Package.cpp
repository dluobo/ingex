/*
 * $Id: Package.cpp,v 1.9 2010/07/21 16:29:34 john_f Exp $
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
#include "DatabaseCache.h"
#include "MaterialResolution.h"
#include "Utilities.h"
#include "ProdAutoException.h"
#include "Logging.h"


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
: DatabaseObject(), position(-1), colour(0)
{}

UserComment::UserComment(string n, string v, int64_t p, int c)
: DatabaseObject(), name(n), value(v), position(p), colour(c)
{}

UserComment::UserComment(const UserComment& userComment)
: DatabaseObject(userComment), name(userComment.name), value(userComment.value), position(userComment.position),
colour(userComment.colour)
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
 
void Package::addUserComments(vector<UserComment> &userComments)
{
    size_t i;
    for (i = 0; i < userComments.size(); i++)
    {
        addUserComment(userComments[i].name, userComments[i].value, userComments[i].position, userComments[i].colour);
    }
}

void Package::addUserComment(string name, string value, int64_t position, int colour)
{
    vector<UserComment>::iterator iter;
    for (iter = _userComments.begin(); iter != _userComments.end(); iter++)
    {
        UserComment userComment = *iter;
        if (userComment.name == name && userComment.value == value &&
            (position == STATIC_COMMENT_POSITION ||
                (position == userComment.position && colour == userComment.colour)))
        {
            return; // already have tagged value or comment marker
        }
    }
    
    _userComments.push_back(UserComment(name, value, position, colour));
}

vector<UserComment> Package::getUserComments(string name)
{
    vector<UserComment> result;
    
    vector<UserComment>::iterator iter;
    for (iter = _userComments.begin(); iter != _userComments.end(); iter++)
    {
        UserComment& userComment = *iter;
        if (name == userComment.name)
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

void Package::clone(Package *clonedPackage)
{
    DatabaseObject::clone(clonedPackage);
    
    clonedPackage->uid = uid;
    clonedPackage->name = name;
    clonedPackage->creationDate = creationDate;
    clonedPackage->projectName = projectName;
    clonedPackage->_userComments = _userComments;
    
    size_t i;
    for (i = 0; i < tracks.size(); i++)
    {
        clonedPackage->tracks.push_back(tracks[i]->clone());
    }
}

void Package::toXML(PackageXMLWriter *xml_writer)
{
    xml_writer->WriteUMIDAttribute("uid", uid);
    xml_writer->WriteAttribute("name", name);
    xml_writer->WriteTimestampAttribute("creationDate", creationDate);
    if (!projectName.name.empty())
        xml_writer->WriteAttribute("projectName", projectName.name);
    
    if (!_userComments.empty()) {
        xml_writer->WriteElementStart("UserComments");
        
        size_t i;
        for (i = 0; i < _userComments.size(); i++) {
            xml_writer->WriteElementStart("UserComment");
            xml_writer->WriteAttribute("name", _userComments[i].name);
            xml_writer->WriteAttribute("value", _userComments[i].value);
            if (_userComments[i].position != STATIC_COMMENT_POSITION) {
                xml_writer->WriteInt64Attribute("position", _userComments[i].position);
                xml_writer->WriteColourAttribute("colour", _userComments[i].colour);
            }
            xml_writer->WriteElementEnd();
        }
        
        xml_writer->WriteElementEnd();
    }
    
    if (!tracks.empty()) {
        xml_writer->WriteElementStart("Tracks");
        
        size_t i;
        for (i = 0; i < tracks.size(); i++)
            tracks[i]->toXML(xml_writer);

        xml_writer->WriteElementEnd();
    }
}

void Package::fromXML(PackageXMLReader *xml_reader)
{
    uid = xml_reader->ParseUMIDAttr("uid");
    name = xml_reader->ParseStringAttr("name");
    creationDate = xml_reader->ParseTimestampAttr("creationDate");
    if (xml_reader->HaveAttr("projectName", false)) {
        if (xml_reader->HaveDatabaseCache()) {
            projectName = xml_reader->GetDatabaseCache()->LoadOrCreateProjectName(
                    xml_reader->ParseStringAttr("projectName"));
        } else {
            projectName.name = xml_reader->ParseStringAttr("projectName");
            Logging::warning("Parsed Package::projectName from XML but have not persisted it to the database\n");
        }
    }

    // MaterialPackage/SourcePackage subclass will call xml_reader->ParseChildElements()
}

void Package::ParseXMLChild(PackageXMLReader *xml_reader, string name)
{
    if (name == "Tracks") {
        xml_reader->ParseChildElements(this);
    } else if (name == "Track") {
        auto_ptr<Track> track(new Track());
        track->fromXML(xml_reader);
        tracks.push_back(track.release());
    } else if (name == "UserComments") {
        xml_reader->ParseChildElements(this);
    } else if (name == "UserComment") {
        UserComment user_comment;
        user_comment.name = xml_reader->ParseStringAttr("name");
        user_comment.value = xml_reader->ParseStringAttr("value");
        if (xml_reader->HaveAttr("position", false)) {
            user_comment.position = xml_reader->ParseInt64Attr("position");
            user_comment.colour = xml_reader->ParseColourAttr("colour");
        } else {
            user_comment.position = STATIC_COMMENT_POSITION;
        }
        _userComments.push_back(user_comment);
    }
}



MaterialPackage::MaterialPackage()
: Package() 
{
    op = 0;
}

MaterialPackage::~MaterialPackage()
{}

string MaterialPackage::toString()
{
    stringstream packageStr;
    
    packageStr << "Material Package" << endl;
    if (op == OperationalPattern::OP_ATOM)
    {
        packageStr << "OP-Atom" << endl;
    }
    else if (op == OperationalPattern::OP_1A)
    {
        packageStr << "OP-1A" << endl;
    }
    packageStr << Package::toString() << endl;

    return packageStr.str();
}

Package* MaterialPackage::clone()
{
    MaterialPackage *clonedPackage = new MaterialPackage();
    
    Package::clone(clonedPackage);

    clonedPackage->op = op;
    
    return clonedPackage;
}

void MaterialPackage::toXML(PackageXMLWriter *xml_writer)
{
    xml_writer->WriteElementStart("MaterialPackage");
    xml_writer->WriteOPAttribute("op", op);
    
    Package::toXML(xml_writer);
    
    xml_writer->WriteElementEnd();
}

void MaterialPackage::fromXML(PackageXMLReader *xml_reader)
{
    Package::fromXML(xml_reader);

    op = xml_reader->ParseOPAttr("op");

    xml_reader->ParseChildElements(this);
}



SourcePackage::SourcePackage()
: Package(), descriptor(0), dropFrameFlag(false)
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
            packageStr << "Stored dimensions = " << fileDesc->storedWidth << "x" << fileDesc->storedHeight << endl;
        }
        if (fileDesc->audioQuantizationBits != 0)
        {
            packageStr << "Audio bits per sample = " << fileDesc->audioQuantizationBits << endl;
        }
    }
    else if (descriptor->getType() == TAPE_ESSENCE_DESC_TYPE)
    {
        packageStr << "Drop frame = " << (dropFrameFlag ? "true" : "false") << endl;
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

Package* SourcePackage::clone()
{
    SourcePackage *clonedPackage = new SourcePackage();
    
    Package::clone(clonedPackage);

    if (descriptor != 0)
    {
        clonedPackage->descriptor = descriptor->clone();
    }
    clonedPackage->sourceConfigName = sourceConfigName;
    clonedPackage->dropFrameFlag = dropFrameFlag;
    
    return clonedPackage;
}

void SourcePackage::toXML(PackageXMLWriter *xml_writer)
{
    xml_writer->WriteElementStart("SourcePackage");
    switch (descriptor->getType())
    {
        case FILE_ESSENCE_DESC_TYPE:
            xml_writer->WriteAttribute("type", "File");
            xml_writer->WriteAttribute("sourceConfigName", sourceConfigName);
            break;
        case TAPE_ESSENCE_DESC_TYPE:
            xml_writer->WriteAttribute("type", "Tape");
            xml_writer->WriteBoolAttribute("dropFrameFlag", dropFrameFlag);
            break;
        case LIVE_ESSENCE_DESC_TYPE:
            xml_writer->WriteAttribute("type", "Live");
            xml_writer->WriteBoolAttribute("dropFrameFlag", dropFrameFlag);
            break;
        default:
            PA_ASSERT(false);
    }

    Package::toXML(xml_writer);
    
    descriptor->toXML(xml_writer);
    
    xml_writer->WriteElementEnd();
}

void SourcePackage::fromXML(PackageXMLReader *xml_reader)
{
    Package::fromXML(xml_reader);

    if (xml_reader->HaveAttr("sourceConfigName", false))
        sourceConfigName = xml_reader->ParseStringAttr("sourceConfigName");
    if (xml_reader->HaveAttr("dropFrameFlag", false))
        dropFrameFlag = xml_reader->ParseBoolAttr("dropFrameFlag");

    xml_reader->ParseChildElements(this);
}

void SourcePackage::ParseXMLChild(PackageXMLReader *xml_reader, string name)
{
    auto_ptr<EssenceDescriptor> new_descriptor;
    if (name == "FileEssenceDescriptor") {
        new_descriptor = auto_ptr<EssenceDescriptor>(new FileEssenceDescriptor());
    } else if (name == "TapeEssenceDescriptor") {
        new_descriptor = auto_ptr<EssenceDescriptor>(new TapeEssenceDescriptor());
    } else if (name == "LiveEssenceDescriptor") {
        new_descriptor = auto_ptr<EssenceDescriptor>(new LiveEssenceDescriptor());
    } else {
        Package::ParseXMLChild(xml_reader, name);
        return;
    }
    
    new_descriptor->fromXML(xml_reader);
    
    descriptor = new_descriptor.release();
}



EssenceDescriptor::EssenceDescriptor()
: DatabaseObject()
{}


FileEssenceDescriptor::FileEssenceDescriptor()
: EssenceDescriptor(), fileFormat(0), videoResolutionID(0), imageAspectRatio(g_nullRational),
  storedWidth(0), storedHeight(0), audioQuantizationBits(0)
{}

EssenceDescriptor* FileEssenceDescriptor::clone()
{
    FileEssenceDescriptor* clonedDescriptor = new FileEssenceDescriptor();
    
    clonedDescriptor->fileLocation = fileLocation;
    clonedDescriptor->fileFormat = fileFormat;
    clonedDescriptor->videoResolutionID = videoResolutionID;
    clonedDescriptor->imageAspectRatio = imageAspectRatio;
    clonedDescriptor->storedWidth = storedWidth;
    clonedDescriptor->storedHeight = storedHeight;
    clonedDescriptor->audioQuantizationBits = audioQuantizationBits;
    
    return clonedDescriptor;
}

void FileEssenceDescriptor::toXML(PackageXMLWriter *xml_writer)
{
    string name;
    
    xml_writer->WriteElementStart("FileEssenceDescriptor");
    xml_writer->WriteAttribute("fileLocation", fileLocation);
    xml_writer->WriteIntAttribute("fileFormat", fileFormat);
    name = FileFormat::Name((FileFormat::EnumType)fileFormat);
    if (name != "Unknown") {
        xml_writer->WriteAttribute("fileFormatName", name);
    } else {
        Logging::warning("Unknown name for file format %d\n", fileFormat);
    }
    if (videoResolutionID != 0) {
        xml_writer->WriteIntAttribute("videoResolutionID", videoResolutionID);
        name = MaterialResolution::Name((MaterialResolution::EnumType)videoResolutionID);
        if (name != "Unknown") {
            xml_writer->WriteAttribute("videoResolutionName", name);
        } else {
            Logging::warning("Unknown name for video resolution %d\n", videoResolutionID);
        }
        xml_writer->WriteRationalAttribute("imageAspectRatio", imageAspectRatio);
        xml_writer->WriteUInt32Attribute("storedWidth", storedWidth);
        xml_writer->WriteUInt32Attribute("storedHeight", storedHeight);
    } else {
        xml_writer->WriteUInt32Attribute("audioQuantizationBits", audioQuantizationBits);
    }
    xml_writer->WriteElementEnd();
}

void FileEssenceDescriptor::fromXML(PackageXMLReader *xml_reader)
{
    fileLocation = xml_reader->ParseStringAttr("fileLocation");
    fileFormat = xml_reader->ParseIntAttr("fileFormat");
    if (xml_reader->HaveAttr("videoResolutionID", false)) {
        videoResolutionID = xml_reader->ParseIntAttr("videoResolutionID");
        imageAspectRatio = xml_reader->ParseRationalAttr("imageAspectRatio");
        storedWidth = xml_reader->ParseUInt32Attr("storedWidth");
        storedHeight = xml_reader->ParseUInt32Attr("storedHeight");
    } else {
        audioQuantizationBits = xml_reader->ParseUInt32Attr("audioQuantizationBits");
    }
}



TapeEssenceDescriptor::TapeEssenceDescriptor()
: EssenceDescriptor()
{}

EssenceDescriptor* TapeEssenceDescriptor::clone()
{
    TapeEssenceDescriptor* clonedDescriptor = new TapeEssenceDescriptor();
    
    clonedDescriptor->spoolNumber = spoolNumber;
    
    return clonedDescriptor;
}

void TapeEssenceDescriptor::toXML(PackageXMLWriter *xml_writer)
{
    xml_writer->WriteElementStart("TapeEssenceDescriptor");
    xml_writer->WriteAttribute("spoolNumber", spoolNumber);
    xml_writer->WriteElementEnd();
}

void TapeEssenceDescriptor::fromXML(PackageXMLReader *xml_reader)
{
    spoolNumber = xml_reader->ParseStringAttr("spoolNumber");
}


LiveEssenceDescriptor::LiveEssenceDescriptor()
: EssenceDescriptor(), recordingLocation(0)
{}

EssenceDescriptor* LiveEssenceDescriptor::clone()
{
    LiveEssenceDescriptor* clonedDescriptor = new LiveEssenceDescriptor();
    
    clonedDescriptor->recordingLocation = recordingLocation;
    
    return clonedDescriptor;
}

void LiveEssenceDescriptor::toXML(PackageXMLWriter *xml_writer)
{
    xml_writer->WriteElementStart("LiveEssenceDescriptor");
    if (xml_writer->HaveDatabaseCache() && recordingLocation != 0) {
        xml_writer->WriteAttribute("recordingLocation",
                                   xml_writer->GetDatabaseCache()->GetLiveRecordingLocation(recordingLocation));
    }
    xml_writer->WriteElementEnd();
}

void LiveEssenceDescriptor::fromXML(PackageXMLReader *xml_reader)
{
    if (xml_reader->HaveDatabaseCache() && xml_reader->HaveAttr("recordingLocation", false)) {
        recordingLocation = xml_reader->GetDatabaseCache()->LoadOrCreateLiveRecordingLocation(
                xml_reader->ParseStringAttr("recordingLocation"));
    }
}

