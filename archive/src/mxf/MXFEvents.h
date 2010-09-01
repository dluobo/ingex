/*
 * $Id: MXFEvents.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#ifndef __RECORDER_MXF_EVENTS_H__
#define __RECORDER_MXF_EVENTS_H__


#include <libMXF++/MXF.h>

#include "MXFEventFileWriter.h"



namespace rec
{


class MXFEvent : public mxfpp::DMFramework
{
public:
    static void Register(MXFEventFileWriter *event_file);

public:
    MXFEvent(mxfpp::HeaderMetadata *header_metadata, ::MXFMetadataSet *c_metadata_set);
    virtual ~MXFEvent();

    virtual void CreateNewInstance();
};



class PSEAnalysisFrameworkEvent : public MXFEvent
{
public:
    friend class mxfpp::MetadataSetFactory<PSEAnalysisFrameworkEvent>;

public:
    static void Register(MXFEventFileWriter *event_file);
    static PSEAnalysisFrameworkEvent* Create(MXFEventFileWriter *event_file);
    static std::string GetTrackName();
    static uint32_t GetTrackId();
    
public:
    virtual ~PSEAnalysisFrameworkEvent();
    
public:
    void SetRedFlash(int16_t value);
    void SetSpatialPattern(int16_t value);
    void SetLuminanceFlash(int16_t value);
    void SetExtendedFailure(bool value);
    
private:
    PSEAnalysisFrameworkEvent(mxfpp::HeaderMetadata *header_metadata, ::MXFMetadataSet *c_metadata_set);
};



class VTRReplayErrorFrameworkEvent : public MXFEvent
{
public:
    friend class mxfpp::MetadataSetFactory<VTRReplayErrorFrameworkEvent>;

public:
    static void Register(MXFEventFileWriter *event_file);
    static VTRReplayErrorFrameworkEvent* Create(MXFEventFileWriter *event_file);
    static std::string GetTrackName();
    static uint32_t GetTrackId();
    
public:
    virtual ~VTRReplayErrorFrameworkEvent();
    
public:
    void SetErrorCode(uint8_t code);
    
private:
    VTRReplayErrorFrameworkEvent(mxfpp::HeaderMetadata *header_metadata, ::MXFMetadataSet *c_metadata_set);
};



class DigiBetaDropoutFrameworkEvent : public MXFEvent
{
public:
    friend class mxfpp::MetadataSetFactory<DigiBetaDropoutFrameworkEvent>;

public:
    static void Register(MXFEventFileWriter *event_file);
    static DigiBetaDropoutFrameworkEvent* Create(MXFEventFileWriter *event_file);
    static std::string GetTrackName();
    static uint32_t GetTrackId();
    
public:
    virtual ~DigiBetaDropoutFrameworkEvent();
    
public:
    void SetStrength(int32_t strength);
    
private:
    DigiBetaDropoutFrameworkEvent(mxfpp::HeaderMetadata *header_metadata, ::MXFMetadataSet *c_metadata_set);
};



class TimecodeBreakFrameworkEvent : public MXFEvent
{
public:
    friend class mxfpp::MetadataSetFactory<TimecodeBreakFrameworkEvent>;

public:
    static void Register(MXFEventFileWriter *event_file);
    static TimecodeBreakFrameworkEvent* Create(MXFEventFileWriter *event_file);
    static std::string GetTrackName();
    static uint32_t GetTrackId();
    
public:
    virtual ~TimecodeBreakFrameworkEvent();
    
public:
    void SetTimecodeType(uint16_t timecode_type);
    
private:
    TimecodeBreakFrameworkEvent(mxfpp::HeaderMetadata *header_metadata, ::MXFMetadataSet *c_metadata_set);
};



};


#endif
