/*
 * $Id: MXFEvents.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#include "MXFEvents.h"
#include "MXFCommon.h"
#include "RecorderException.h"
#include "Logging.h"

using namespace std;
using namespace rec;
using namespace mxfpp;



static const uint32_t PSE_FAILURE_TRACK_ID      = 1000;
static const uint32_t VTR_ERROR_TRACK_ID        = 2000;
static const uint32_t DIGIBETA_DROPOUT_TRACK_ID = 3000;
static const uint32_t TIMECODE_BREAK_TRACK_ID   = 4000;

static const char *PSE_FAILURE_TRACK_NAME       = "PSE Failures";
static const char *VTR_ERROR_TRACK_NAME         = "VTR Errors";
static const char *DIGIBETA_DROPOUT_TRACK_NAME  = "DigiBeta Dropouts";
static const char *TIMECODE_BREAK_TRACK_NAME    = "Timecode Breaks";



void MXFEvent::Register(MXFEventFileWriter *event_file)
{
    event_file->SetDMScheme(MXF_DM_L(APP_PreservationDescriptiveScheme));

    register_archive_extensions(event_file->GetDataModel());
}


MXFEvent::MXFEvent(HeaderMetadata *header_metadata,
                   ::MXFMetadataSet *c_metadata_set)
: DMFramework(header_metadata, c_metadata_set)
{
}

MXFEvent::~MXFEvent()
{
}

void MXFEvent::CreateNewInstance()
{
    _cMetadataSet = _headerMetadata->createCSet(&_cMetadataSet->key);
}




void PSEAnalysisFrameworkEvent::Register(MXFEventFileWriter *event_file)
{
    MXFEvent::Register(event_file);
    
    event_file->GetHeaderMetadata()->registerObjectFactory(&MXF_SET_K(APP_PSEAnalysisFramework),
                                                           new MetadataSetFactory<PSEAnalysisFrameworkEvent>());
}

PSEAnalysisFrameworkEvent* PSEAnalysisFrameworkEvent::Create(MXFEventFileWriter *event_file)
{
    return dynamic_cast<PSEAnalysisFrameworkEvent*>
            (event_file->GetHeaderMetadata()->createAndWrap(&MXF_SET_K(APP_PSEAnalysisFramework)));
}

string PSEAnalysisFrameworkEvent::GetTrackName()
{
    return PSE_FAILURE_TRACK_NAME;
}

uint32_t PSEAnalysisFrameworkEvent::GetTrackId()
{
    return PSE_FAILURE_TRACK_ID;
}


PSEAnalysisFrameworkEvent::PSEAnalysisFrameworkEvent(HeaderMetadata *header_metadata,
                                                     ::MXFMetadataSet *c_metadata_set)
: MXFEvent(header_metadata, c_metadata_set)
{
}

PSEAnalysisFrameworkEvent::~PSEAnalysisFrameworkEvent()
{
}

void PSEAnalysisFrameworkEvent::SetRedFlash(int16_t value)
{
    setInt16Item(&MXF_ITEM_K(APP_PSEAnalysisFramework, APP_RedFlash), value);
}

void PSEAnalysisFrameworkEvent::SetSpatialPattern(int16_t value)
{
    setInt16Item(&MXF_ITEM_K(APP_PSEAnalysisFramework, APP_SpatialPattern), value);
}

void PSEAnalysisFrameworkEvent::SetLuminanceFlash(int16_t value)
{
    setInt16Item(&MXF_ITEM_K(APP_PSEAnalysisFramework, APP_LuminanceFlash), value);
}

void PSEAnalysisFrameworkEvent::SetExtendedFailure(bool value)
{
    setBooleanItem(&MXF_ITEM_K(APP_PSEAnalysisFramework, APP_ExtendedFailure), value);
}



void VTRReplayErrorFrameworkEvent::Register(MXFEventFileWriter *event_file)
{
    MXFEvent::Register(event_file);
    
    event_file->GetHeaderMetadata()->registerObjectFactory(&MXF_SET_K(APP_VTRReplayErrorFramework),
                                                           new MetadataSetFactory<VTRReplayErrorFrameworkEvent>());
}

VTRReplayErrorFrameworkEvent* VTRReplayErrorFrameworkEvent::Create(MXFEventFileWriter *event_file)
{
    return dynamic_cast<VTRReplayErrorFrameworkEvent*>
            (event_file->GetHeaderMetadata()->createAndWrap(&MXF_SET_K(APP_VTRReplayErrorFramework)));
}

string VTRReplayErrorFrameworkEvent::GetTrackName()
{
    return VTR_ERROR_TRACK_NAME;
}

uint32_t VTRReplayErrorFrameworkEvent::GetTrackId()
{
    return VTR_ERROR_TRACK_ID;
}


VTRReplayErrorFrameworkEvent::VTRReplayErrorFrameworkEvent(HeaderMetadata *header_metadata,
                                                           ::MXFMetadataSet *c_metadata_set)
: MXFEvent(header_metadata, c_metadata_set)
{
}

VTRReplayErrorFrameworkEvent::~VTRReplayErrorFrameworkEvent()
{
}

void VTRReplayErrorFrameworkEvent::SetErrorCode(uint8_t code)
{
    setUInt8Item(&MXF_ITEM_K(APP_VTRReplayErrorFramework, APP_VTRErrorCode), code);
}



void DigiBetaDropoutFrameworkEvent::Register(MXFEventFileWriter *event_file)
{
    MXFEvent::Register(event_file);
    
    event_file->GetHeaderMetadata()->registerObjectFactory(&MXF_SET_K(APP_DigiBetaDropoutFramework),
                                                           new MetadataSetFactory<DigiBetaDropoutFrameworkEvent>());
}

DigiBetaDropoutFrameworkEvent* DigiBetaDropoutFrameworkEvent::Create(MXFEventFileWriter *event_file)
{
    return dynamic_cast<DigiBetaDropoutFrameworkEvent*>
            (event_file->GetHeaderMetadata()->createAndWrap(&MXF_SET_K(APP_DigiBetaDropoutFramework)));
}

string DigiBetaDropoutFrameworkEvent::GetTrackName()
{
    return DIGIBETA_DROPOUT_TRACK_NAME;
}

uint32_t DigiBetaDropoutFrameworkEvent::GetTrackId()
{
    return DIGIBETA_DROPOUT_TRACK_ID;
}


DigiBetaDropoutFrameworkEvent::DigiBetaDropoutFrameworkEvent(HeaderMetadata *header_metadata,
                                                           ::MXFMetadataSet *c_metadata_set)
: MXFEvent(header_metadata, c_metadata_set)
{
}

DigiBetaDropoutFrameworkEvent::~DigiBetaDropoutFrameworkEvent()
{
}

void DigiBetaDropoutFrameworkEvent::SetStrength(int32_t strength)
{
    setInt32Item(&MXF_ITEM_K(APP_DigiBetaDropoutFramework, APP_Strength), strength);
}



void TimecodeBreakFrameworkEvent::Register(MXFEventFileWriter *event_file)
{
    MXFEvent::Register(event_file);
    
    event_file->GetHeaderMetadata()->registerObjectFactory(&MXF_SET_K(APP_TimecodeBreakFramework),
                                                           new MetadataSetFactory<TimecodeBreakFrameworkEvent>());
}

TimecodeBreakFrameworkEvent* TimecodeBreakFrameworkEvent::Create(MXFEventFileWriter *event_file)
{
    return dynamic_cast<TimecodeBreakFrameworkEvent*>
            (event_file->GetHeaderMetadata()->createAndWrap(&MXF_SET_K(APP_TimecodeBreakFramework)));
}

string TimecodeBreakFrameworkEvent::GetTrackName()
{
    return TIMECODE_BREAK_TRACK_NAME;
}

uint32_t TimecodeBreakFrameworkEvent::GetTrackId()
{
    return TIMECODE_BREAK_TRACK_ID;
}


TimecodeBreakFrameworkEvent::TimecodeBreakFrameworkEvent(HeaderMetadata *header_metadata,
                                                         ::MXFMetadataSet *c_metadata_set)
: MXFEvent(header_metadata, c_metadata_set)
{
}

TimecodeBreakFrameworkEvent::~TimecodeBreakFrameworkEvent()
{
}

void TimecodeBreakFrameworkEvent::SetTimecodeType(uint16_t timecode_type)
{
    setUInt16Item(&MXF_ITEM_K(APP_TimecodeBreakFramework, APP_TimecodeType), timecode_type);
}

