/*
 * $Id: MXFEventFileWriter.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#include <libMXF++/MXF.h>

#include "MXFEventFileWriter.h"
#include "MXFCommon.h"
#include "RecorderException.h"
#include "Logging.h"

using namespace std;
using namespace rec;
using namespace mxfpp;



// number of strong reference is contrained by local set encoding: (2^16 - 8) / 16 = 4095
static const uint32_t MAX_STRONG_REF_ARRAY_SIZE = 4095;



MXFEventFileWriter* MXFEventFileWriter::Open(string filename)
{
    try
    {
        return new MXFEventFileWriter(File::openNew(filename));
    }
    catch (...)
    {
        return 0;
    }
}



MXFEventFileWriter::MXFEventFileWriter(File *mxf_file)
{
    mFile = mxf_file;
    mDataModel = 0;
    mHeaderMetadata = 0;
    mMaterialPackage = 0;
    mNextEventTrackId = 1;
    mEventTrackSequence = 0;
    mEventTrackEventCount = 0;

    mxf_generate_umid(&mMaterialPackageUID);

    try
    {
        CreateHeaderMetadata();
    }
    catch (...)
    {
        // mxf_file became our responsibility
        delete mxf_file;
        throw;
    }
}

MXFEventFileWriter::~MXFEventFileWriter()
{
    delete mFile;
    delete mHeaderMetadata;
    delete mDataModel;
}

void MXFEventFileWriter::SetDMScheme(mxfUL dm_scheme)
{
    Preface *preface = mHeaderMetadata->getPreface();
    
    vector<mxfUL> schemes = preface->getDMSchemes();
    
    size_t i;
    for (i = 0; i < schemes.size(); i++) {
        if (mxf_equals_ul(&dm_scheme, &schemes[i]))
            return;
    }
    
    preface->appendDMSchemes(dm_scheme);
}

void MXFEventFileWriter::StartNewEventTrack(uint32_t track_id, string name)
{
    // Preface - ContentStorage - MaterialPackage - Event Track
    EventTrack *event_track = new EventTrack(mHeaderMetadata);
    mMaterialPackage->appendTracks(event_track);
    event_track->setTrackID(track_id);
    event_track->setTrackNumber(0);
    event_track->setTrackName(name);
    event_track->setEventEditRate((mxfRational){25, 1});
    event_track->setEventOrigin(0);
    
    // Preface - ContentStorage - MaterialPackage - Event Track - Sequence
    mEventTrackSequence = new Sequence(mHeaderMetadata);
    event_track->setSequence(mEventTrackSequence);
    mEventTrackSequence->setDataDefinition(MXF_DDEF_L(DescriptiveMetadata));
    
    mEventTrackEventCount = 0;
    mNextEventTrackId = track_id + 1;
    mEventTrackName = name;
}

DMFramework* MXFEventFileWriter::AddEvent(int64_t position, int64_t duration, DMFramework *dm_framework)
{
    REC_ASSERT(mFile);

    if (!mEventTrackSequence || mEventTrackEventCount >= MAX_STRONG_REF_ARRAY_SIZE) {
        // Preface - ContentStorage - MaterialPackage - Event Track
        EventTrack *event_track = new EventTrack(mHeaderMetadata);
        mMaterialPackage->appendTracks(event_track);
        event_track->setTrackID(mNextEventTrackId);
        event_track->setTrackNumber(0);
        event_track->setTrackName(mEventTrackName);
        event_track->setEventEditRate((mxfRational){25, 1});
        event_track->setEventOrigin(0);
        
        // Preface - ContentStorage - MaterialPackage - Event Track - Sequence
        mEventTrackSequence = new Sequence(mHeaderMetadata);
        event_track->setSequence(mEventTrackSequence);
        mEventTrackSequence->setDataDefinition(MXF_DDEF_L(DescriptiveMetadata));
        
        mEventTrackEventCount = 0;
        mNextEventTrackId++;
    } 
    
    // Preface - ContentStorage - Package - DM Track - Sequence - DMSegment
    DMSegment *dm_segment = new DMSegment(mHeaderMetadata);
    mEventTrackSequence->appendStructuralComponents(dm_segment);
    dm_segment->setDataDefinition(MXF_DDEF_L(DescriptiveMetadata));
    dm_segment->setEventStartPosition(position);
    dm_segment->setDuration(duration);
    
    // Preface - ContentStorage - Package - DM Track - Sequence - DMSegment - DMFramework
    dm_segment->setDMFramework(dm_framework);
    
    
    mEventTrackEventCount++;
    
    return dm_framework;
}

void MXFEventFileWriter::Save()
{
    REC_ASSERT(mFile);
    
    // make sure the required property, MaterialPackage::Tracks, is set
    
    if (!mMaterialPackage->haveItem(&MXF_ITEM_K(GenericPackage, Tracks)))
        mMaterialPackage->setTracks(vector<GenericTrack*>());
    
    
    // set minimum llen
    
    mFile->setMinLLen(4);
    
    
    // write the header partition pack
    
    Partition &header_partition = mFile->createPartition();
    header_partition.setKey(&MXF_PP_K(ClosedComplete, Header));
    header_partition.setOperationalPattern(&MXF_OP_L(1a, qq01));
    header_partition.write(mFile);
    
    
    // write the header metadata
    
    mHeaderMetadata->write(mFile, &header_partition, 0);
    
    
    // update header partition pack
    
    mFile->updatePartitions();
    
    
    delete mFile;
    mFile = 0;
}

void MXFEventFileWriter::CreateHeaderMetadata()
{
    mxfTimestamp now;
    mxf_get_timestamp_now(&now);
    
    
    mDataModel = new DataModel();
    mHeaderMetadata = new HeaderMetadata(mDataModel);
    
    // Preface
    Preface *preface = new Preface(mHeaderMetadata);
    preface->setLastModifiedDate(now);
    preface->setVersion(258);
    preface->setOperationalPattern(MXF_OP_L(1a, qq01));
    preface->setEssenceContainers(vector<mxfUL>());
    preface->setDMSchemes(vector<mxfUL>());
    
    // Preface - Identification
    Identification *ident = new Identification(mHeaderMetadata);
    preface->appendIdentifications(ident);
    ident->initialise(MXF_IDENT_COMPANY_NAME, MXF_IDENT_PRODUCT_NAME, MXF_IDENT_VERSION_STRING, MXF_IDENT_PRODUCT_UID);
    ident->setModificationDate(now);

    // Preface - ContentStorage
    ContentStorage* content_storage = new ContentStorage(mHeaderMetadata);
    preface->setContentStorage(content_storage);
    
    // Preface - ContentStorage - MaterialPackage
    mMaterialPackage = new MaterialPackage(mHeaderMetadata);
    content_storage->appendPackages(mMaterialPackage);
    mMaterialPackage->setPackageUID(mMaterialPackageUID);
    mMaterialPackage->setPackageCreationDate(now);
    mMaterialPackage->setPackageModifiedDate(now);
}

