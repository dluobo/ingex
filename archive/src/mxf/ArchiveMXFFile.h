/*
 * $Id: ArchiveMXFFile.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * Archive MXF file reader
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

#ifndef __RECORDER_ARCHIVE_MXF_FILE_H__
#define __RECORDER_ARCHIVE_MXF_FILE_H__


#include <string>

#include <mxf_reader.h>
#include <mxf/mxf_page_file.h>

#include "MXFFileReader.h"
#include "ArchiveMXFContentPackage.h"


namespace rec
{


class ArchiveMXFFile : public MXFFileReader
{
public:
    ArchiveMXFFile(std::string filename);
    ArchiveMXFFile(std::string filename, int64_t pageSize);
    virtual ~ArchiveMXFFile();
    
    uint32_t getComponentDepth() const { return _componentDepth; }
    
public:
    virtual long getPSEFailures(PSEFailure **failures) const
        { *failures = _pseFailures; return _numPSEFailures; }
    virtual long getVTRErrors(VTRErrorAtPos **errors) const
        { *errors = _vtrErrors; return _numVTRErrors; }
    virtual long getDigiBetaDropouts(DigiBetaDropout **digiBetaDropouts) const
        { *digiBetaDropouts = _digiBetaDropouts; return _numDigiBetaDropouts; }
    virtual long getTimecodeBreaks(TimecodeBreak **timecodeBreaks) const
        { *timecodeBreaks = _timecodeBreaks; return _numTimecodeBreaks; }
    
    virtual bool nextFrame(bool ignoreAVEssenceData, MXFContentPackage *&contentPackage);
    virtual bool skipFrames(int64_t count);
    virtual bool readFrame(int64_t position, bool ignoreAVEssenceData, MXFContentPackage *&contentPackage);
    virtual void forwardTruncate();

private:
    void initialize(std::string filename, int64_t pageSize);

private:
    // MXFReader callbacks
    static int accept_frame(MXFReaderListener *mxfListener, int trackIndex);
    static int allocate_buffer(MXFReaderListener *mxfListener, int trackIndex, uint8_t **buffer, uint32_t bufferSize);
    static void deallocate_buffer(MXFReaderListener *mxfListener, int trackIndex, uint8_t **buffer);
    static int receive_frame(MXFReaderListener *mxfListener, int trackIndex, uint8_t* buffer, uint32_t bufferSize);

private:
    MXFFile *_mxfFile;
    MXFPageFile *_mxfPageFile;
    MXFReader *_mxfReader;
    MXFDataModel *_dataModel;
    
    uint32_t _componentDepth;
    
    PSEFailure *_pseFailures;
    long _numPSEFailures;
    VTRErrorAtPos *_vtrErrors;
    long _numVTRErrors;
    DigiBetaDropout *_digiBetaDropouts;
    long _numDigiBetaDropouts;
    TimecodeBreak *_timecodeBreaks;
    long _numTimecodeBreaks;

    ArchiveMXFContentPackage _contentPackage;
    int _videoTrack;
    int _audioTrack[8];

    // used in MXFReader callbacks
    bool _ignoreAVEssenceData;
};


};



#endif

