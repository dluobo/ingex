/*
 * $Id: Package.h,v 1.1 2007/09/11 14:08:39 stuart_hc Exp $
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


// standard names for Avid user comments (tagged values)
#define AVID_UC_COMMENTS_NAME       "Comments"
#define AVID_UC_DESCRIPTION_NAME    "Descript"
#define AVID_UC_SHOOT_DATE_NAME     "Shoot Date"




namespace prodauto
{

typedef enum 
{ 
    MATERIAL_PACKAGE, 
    SOURCE_PACKAGE 
} PackageType;


class UserComment : public DatabaseObject
{
public:
    UserComment();
    UserComment(std::string name, std::string value);
    UserComment(const UserComment& tv);
    virtual ~UserComment();
    
    std::string name;
    std::string value;
};

class Package : public DatabaseObject
{
public:
    friend class Database; // allow access to _taggedValueLinks
    
public:
    Package();
    virtual ~Package();
    
    virtual std::string toString();

    Track* getTrack(uint32_t id);
    
    virtual PackageType getType() = 0;

    void addUserComment(std::string, std::string value);
    std::vector<UserComment> getUserComments(std::string name); 
    std::vector<UserComment> getUserComments();

    virtual void cloneInPlace(UMID newUID, bool resetLengths);
    
    UMID uid;
    std::string name;
    Timestamp creationDate;
    std::string avidProjectName;
    std::vector<Track*> tracks;
    
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
};



class EssenceDescriptor : public DatabaseObject
{
public:
    EssenceDescriptor();
    virtual ~EssenceDescriptor() {};
    
    virtual int getType() = 0;
};

class SourcePackage : public Package
{
public:
    SourcePackage();
    virtual ~SourcePackage();
    
    virtual PackageType getType() { return SOURCE_PACKAGE; }

    virtual std::string toString();

    virtual void cloneInPlace(UMID newUID, bool resetLengths);
    
    
    EssenceDescriptor* descriptor;
    std::string sourceConfigName; // file SourcePackage only
};



class FileEssenceDescriptor : public EssenceDescriptor
{
public:
    FileEssenceDescriptor();
    
    virtual int getType() { return FILE_ESSENCE_DESC_TYPE; };
    
    std::string fileLocation;
    int fileFormat;
    int videoResolutionID;
    Rational imageAspectRatio;
    uint32_t audioQuantizationBits;
};

class TapeEssenceDescriptor : public EssenceDescriptor
{
public:
    TapeEssenceDescriptor();
    
    virtual int getType() { return TAPE_ESSENCE_DESC_TYPE; };
    
    std::string spoolNumber;
};

class LiveEssenceDescriptor : public EssenceDescriptor
{
public:
    LiveEssenceDescriptor();
    
    virtual int getType() { return LIVE_ESSENCE_DESC_TYPE; };
    
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


