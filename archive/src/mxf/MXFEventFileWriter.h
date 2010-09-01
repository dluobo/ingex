/*
 * $Id: MXFEventFileWriter.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#ifndef __RECORDER_MXF_EVENT_FILE_WRITER_H__
#define __RECORDER_MXF_EVENT_FILE_WRITER_H__

#include <string>
#include <vector>



namespace mxfpp
{
    class File;
    class DataModel;
    class HeaderMetadata;
    class MaterialPackage;
    class Sequence;
    class DMFramework;
};


namespace rec
{


class MXFEventFileWriter
{
public:
    static MXFEventFileWriter* Open(std::string filename);

public:
    ~MXFEventFileWriter();

    mxfpp::DataModel* GetDataModel() const { return mDataModel; }
    mxfpp::HeaderMetadata* GetHeaderMetadata() const { return mHeaderMetadata; }
    
    void SetDMScheme(mxfUL dm_scheme);
    
    void StartNewEventTrack(uint32_t track_id, std::string name);
    mxfpp::DMFramework* AddEvent(int64_t position, int64_t duration, mxfpp::DMFramework *dm_framework);
    
    void Save();
    
    
private:
    MXFEventFileWriter(mxfpp::File *mxf_file);

    void CreateHeaderMetadata();
    
private:
    mxfpp::File *mFile;
    mxfpp::DataModel *mDataModel;
    mxfpp::HeaderMetadata *mHeaderMetadata;

    mxfpp::MaterialPackage *mMaterialPackage;
    mxfUMID mMaterialPackageUID;
    
    uint32_t mNextEventTrackId;
    std::string mEventTrackName;
    mxfpp::Sequence *mEventTrackSequence;
    uint32_t mNumEventTracks;
    uint32_t mEventTrackEventCount;
};



};


#endif
