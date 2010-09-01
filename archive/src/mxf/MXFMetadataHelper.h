/*
 * $Id: MXFMetadataHelper.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * MXF metadata helper functions
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
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

#ifndef __RECORDER_MXF_METADATA_HELPER_H__
#define __RECORDER_MXF_METADATA_HELPER_H__

#include <string>

#include <archive_types.h>


namespace mxfpp
{
    class DataModel;
    class HeaderMetadata;
    class GenericPackage;
    class MaterialPackage;
    class SourcePackage;
};


namespace rec
{

class MXFMetadataHelper
{
public:
    static void RegisterDataModelExtensions(mxfpp::DataModel *data_model);
    
public:
    MXFMetadataHelper(mxfpp::DataModel *data_model, mxfpp::HeaderMetadata *header_metadata);
    ~MXFMetadataHelper();
    
    void SetMaterialPackageName(std::string name);
    mxfpp::SourcePackage* AddTapeSourcePackage(std::string name);
    void SetTapeSourceInfaxMetadata(InfaxData *infax_data);
    void AddInfaxMetadataAsComments(InfaxData *infax_data);
    
private:
    mxfpp::MaterialPackage* GetMaterialPackage();
    mxfpp::SourcePackage* GetFileSourcePackage();
    mxfpp::SourcePackage* GetTapeSourcePackage();
    void SetArchiveDMScheme();
    void SetInfaxMetadata(mxfpp::GenericPackage *package, InfaxData *infax_data);
    void AddInfaxMetadataAsComments(mxfpp::GenericPackage *package, InfaxData *infax_data);
    
private:
    mxfpp::DataModel *mDataModel;
    mxfpp::HeaderMetadata *mHeaderMetadata;
};



};


#endif

