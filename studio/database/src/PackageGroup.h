/*
 * $Id: PackageGroup.h,v 1.7 2010/10/12 17:44:12 john_f Exp $
 *
 * Package group
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __PRODAUTO_PACKAGE_GROUP_H__
#define __PRODAUTO_PACKAGE_GROUP_H__

#include <vector>

#include "Package.h"
#include "MaterialResolution.h"
#include "PackageXMLReader.h"
#include "Transaction.h"



namespace prodauto
{

class PackageGroup : protected PackageXMLChildParser
{
public:
    PackageGroup();
    PackageGroup(Rational edit_rate, int op);
    virtual ~PackageGroup();

    int GetOP() { return mOP; }
    Rational GetProjectEditRate() { return mProjectEditRate; }
    bool Is25FPSProject() { return mProjectEditRate.numerator == 25 && mProjectEditRate.denominator == 1; }
    bool Is29FPSProject() { return mProjectEditRate.numerator == 30000 && mProjectEditRate.denominator == 1001; }
    bool Is50FPSProject() { return mProjectEditRate.numerator == 50 && mProjectEditRate.denominator == 1; }
    bool Is59FPSProject() { return mProjectEditRate.numerator == 60000 && mProjectEditRate.denominator == 1001; }
    
    void UpdateUserComments(std::vector<UserComment> &user_comments);
    void UpdateProjectName(ProjectName project_name);

    // duration is at project edit rate
    void UpdateDuration(int64_t duration); // all tracks have equal duration
    void UpdateDuration(uint32_t mp_track_id, int64_t duration);

    void UpdateFileLocation(uint32_t mp_track_id, std::string file_location);
    void UpdateFileLocation(std::string file_location); // use for OP-1A only
    void UpdateAllFileLocations(std::string prefix);

    void UpdateStoredDimensions(uint32_t mp_track_id, uint32_t stored_width, uint32_t stored_height);
    void UpdateStoredDimensions(uint32_t stored_width, uint32_t stored_height); // use for OP-1A only

    MaterialPackage* GetMaterialPackage() { return mMaterialPackage; }
    std::vector<SourcePackage*>& GetFileSourcePackages() { return mFileSourcePackages; }
    SourcePackage* GetFileSourcePackage(); // use for OP-1A only
    SourcePackage* GetTapeSourcePackage() { return mTapeSourcePackage; }
    
    MaterialResolution::EnumType GetMaterialResolution();
    
    bool HaveFileSourcePackage(uint32_t mp_track_id);
    SourcePackage* GetFileSourcePackage(uint32_t mp_track_id);
    Track* GetFileSourceTrack(uint32_t mp_track_id);
    
    std::string GetFileLocation(uint32_t mp_track_id);
    std::string GetFileLocation(); // use for OP-1A only
    
    std::string CreateFileLocation(std::string directory, std::string file_path);
    void RelocateFile(uint32_t mp_track_id, std::string target_directory);
    void RelocateFile(std::string target_directory); // use for OP-1A only
    void DeleteFile(uint32_t mp_track_id);
    void DeleteFile(); // use for OP-1A only
    
    PackageGroup* Clone();
    
    void SaveToDatabase(Transaction *transaction = 0);
    
    void SaveToFile(std::string filename, DatabaseCache *db_cache);
    void RestoreFromFile(std::string filename, DatabaseCache *db_cache);
    
public:
    void SetMaterialPackage(MaterialPackage *material_package);
    void AppendFileSourcePackage(SourcePackage *file_source_package);
    void ClearFileSourcePackages();
    void SetTapeSourcePackage(SourcePackage *tape_source_package, bool take_ownership);

public:
    virtual void ClearPackages();

protected:
    // from PackageXMLChildParser
    virtual void ParseXMLChild(PackageXMLReader *reader, std::string name);

protected:
    std::string CreatePrefixFileLocation(std::string prefix, std::string file_path);

protected:
    int mOP;
    Rational mProjectEditRate;
    
    MaterialPackage *mMaterialPackage;
    std::vector<SourcePackage*> mFileSourcePackages;
    SourcePackage *mTapeSourcePackage;
    bool mOwnTapeSourcePackage;
};


};


#endif

