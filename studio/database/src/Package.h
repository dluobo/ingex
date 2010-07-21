/*
 * $Id: Package.h,v 1.10 2010/07/21 16:29:34 john_f Exp $
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
 
#ifndef __PRODAUTO_PACKAGE_H__
#define __PRODAUTO_PACKAGE_H__


#include <string>
#include <vector>
#include <set>

#include "DatabaseObject.h"
#include "DataTypes.h"
#include "Track.h"
#include "DatabaseEnums.h"
#include "PackageXMLWriter.h"
#include "PackageXMLReader.h"


// standard names for Avid user comments (tagged values)
#define AVID_UC_COMMENTS_NAME       "Comments"
#define AVID_UC_DESCRIPTION_NAME    "Descript"
#define AVID_UC_SHOOT_DATE_NAME     "Shoot Date"
#define AVID_UC_LOCATION_NAME       "Location"
#define AVID_UC_ORGANISATION_NAME   "Organization"
#define AVID_UC_SOURCE_NAME         "Source"

#define STATIC_COMMENT_POSITION     (-1)
#define POSITIONED_COMMENT_NAME     "Comment" 


namespace prodauto
{

typedef enum 
{ 
    MATERIAL_PACKAGE, 
    SOURCE_PACKAGE 
} PackageType;


class ProjectName : public DatabaseObject
{
public:
    ProjectName();
    ProjectName(std::string name_);
    ProjectName(const ProjectName& projectName);
    ~ProjectName();

    ProjectName& operator=(const std::string& nm);
    ProjectName& operator=(const ProjectName& nm);
    
    bool operator==(const ProjectName& pn) const;
    bool operator<(const ProjectName& pn) const;
    
    std::string name;
};

class UserComment : public DatabaseObject
{
public:
    UserComment();
    UserComment(std::string name, std::string value, int64_t position, int colour);
    UserComment(const UserComment& tv);
    virtual ~UserComment();
    
    std::string name;
    std::string value;
    int64_t position;
    int colour;
};

class Package : public DatabaseObject, protected PackageXMLChildParser
{
public:
    friend class Database; // allow access to _taggedValueLinks
    
public:
    Package();
    virtual ~Package();
    
    virtual std::string toString();

    Track* getTrack(uint32_t id);
    
    virtual PackageType getType() = 0;

    void addUserComment(std::string, std::string value, int64_t position, int colour);
    void addUserComments(std::vector<UserComment> &userComments);
    std::vector<UserComment> getUserComments(std::string name); 
    std::vector<UserComment> getUserComments();
    void clearUserComments();

    virtual void cloneInPlace(UMID newUID, bool resetLengths);
    virtual Package* clone() = 0;
    
    virtual void toXML(PackageXMLWriter *xml_writer);
    virtual void fromXML(PackageXMLReader *xml_reader);


    UMID uid;
    std::string name;
    Timestamp creationDate;
    ProjectName projectName;
    std::vector<Track*> tracks;
    
protected:
    void clone(Package *clone);
    
protected:
    // from PackageXMLChildParser
    virtual void ParseXMLChild(PackageXMLReader *xml_reader, std::string name);

private:
    std::vector<UserComment> _userComments;
};



class MaterialPackage : public Package
{
public:
    MaterialPackage();
    virtual ~MaterialPackage();
    
    virtual PackageType getType() { return MATERIAL_PACKAGE; }

    virtual std::string toString();

    virtual Package* clone();
    
    virtual void toXML(PackageXMLWriter *xml_writer);
    virtual void fromXML(PackageXMLReader *xml_reader);


    int op;
};



class EssenceDescriptor : public DatabaseObject
{
public:
    EssenceDescriptor();
    virtual ~EssenceDescriptor() {};
    
    virtual int getType() = 0;
    virtual EssenceDescriptor* clone() = 0;
    virtual void toXML(PackageXMLWriter *xml_writer) = 0;
    virtual void fromXML(PackageXMLReader *xml_reader) = 0;
};

class SourcePackage : public Package
{
public:
    SourcePackage();
    virtual ~SourcePackage();
    
    virtual PackageType getType() { return SOURCE_PACKAGE; }

    virtual std::string toString();

    virtual void cloneInPlace(UMID newUID, bool resetLengths);
    virtual Package* clone();

    virtual void toXML(PackageXMLWriter *xml_writer);
    virtual void fromXML(PackageXMLReader *xml_reader);
    
    
    EssenceDescriptor* descriptor;
    std::string sourceConfigName; // file SourcePackage only
    bool dropFrameFlag; // only relevant for tape source package
    
protected:
    // from PackageXMLChildParser
    virtual void ParseXMLChild(PackageXMLReader *xml_reader, std::string name);
};



class FileEssenceDescriptor : public EssenceDescriptor
{
public:
    FileEssenceDescriptor();
    
    virtual int getType() { return FILE_ESSENCE_DESC_TYPE; };
    virtual EssenceDescriptor* clone();
    virtual void toXML(PackageXMLWriter *xml_writer);
    virtual void fromXML(PackageXMLReader *xml_reader);
    
    std::string fileLocation;
    int fileFormat;
    int videoResolutionID;
    Rational imageAspectRatio;
    uint32_t storedWidth;
    uint32_t storedHeight; // equals field height for separate field video
    uint32_t audioQuantizationBits;
};

class TapeEssenceDescriptor : public EssenceDescriptor
{
public:
    TapeEssenceDescriptor();
    
    virtual int getType() { return TAPE_ESSENCE_DESC_TYPE; };
    virtual EssenceDescriptor* clone();
    virtual void toXML(PackageXMLWriter *xml_writer);
    virtual void fromXML(PackageXMLReader *xml_reader);
    
    std::string spoolNumber;
};

class LiveEssenceDescriptor : public EssenceDescriptor
{
public:
    LiveEssenceDescriptor();
    
    virtual int getType() { return LIVE_ESSENCE_DESC_TYPE; };
    virtual EssenceDescriptor* clone();
    virtual void toXML(PackageXMLWriter *xml_writer);
    virtual void fromXML(PackageXMLReader *xml_reader);
    
    long recordingLocation;
};


// typedefs for sets of pointers to Package, MaterialPackage and SourcePackage

struct PackageComp
{
    bool operator()(const Package* left, const Package* right) const
    {
        return memcmp(&left->uid, &right->uid, sizeof(UMID)) < 0;
    }
};

typedef std::set<Package*, PackageComp> PackageSet;

struct MaterialPackageComp
{
    bool operator()(const MaterialPackage* left, const MaterialPackage* right) const
    {
        return memcmp(&left->uid, &right->uid, sizeof(UMID)) < 0;
    }
};

typedef std::set<MaterialPackage*, MaterialPackageComp> MaterialPackageSet;

struct SourcePackageComp
{
    bool operator()(const SourcePackage* left, const SourcePackage* right) const
    {
        return memcmp(&left->uid, &right->uid, sizeof(UMID)) < 0;
    }
};

typedef std::set<SourcePackage*, SourcePackageComp> SourcePackageSet;


};



#endif


