/*
 * $Id: ArchiveMXFWriter.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * Archive MXF file writer
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

#ifndef __RECORDER_ARCHIVE_MXF_WRITER_H__
#define __RECORDER_ARCHIVE_MXF_WRITER_H__

#include "MXFWriter.h"
#include "TimecodeBreakHelper.h"

#include <write_archive_mxf.h>


namespace rec
{


class ArchiveMXFWriter : public MXFWriter
{
public:
    static uint32_t getContentPackageSize(bool component_depth_8bit, int num_audio_tracks, bool include_crc32);
    
public:
    ArchiveMXFWriter();
    virtual ~ArchiveMXFWriter();
    
    void setComponentDepth(uint32_t depth);     // either 8 or 10. Default 8
    void setAspectRatio(Rational aspect_ratio); // either 4:3 or 16:9. Default 4:3
    void setNumAudioTracks(int count);          // Default 4
    void setIncludeCRC32(bool enable);          // Default true
    void setStartPosition(int64_t position);    // Default 0
    
    bool createFile(std::string filename);
    bool createFile(MXFFile **mxfFile, std::string filename);

    int getNumAudioTracks() const { return _numAudioTracks; }
    uint32_t getComponentDepth() const { return _componentDepth; }
    bool includeCRC32() const { return _includeCRC32; }
    
public:
    // from MXFWriter

    virtual bool writeContentPackage(const MXFContentPackage *content_package);
    
    virtual bool abort();
    virtual bool complete(InfaxData *infax_data,
                          const PSEFailure *pse_failures, long pse_failure_count,
                          const VTRError *vtr_errors, long vtr_error_count,
                          const DigiBetaDropout *digibeta_dropouts, long digibeta_dropout_count);
    
    virtual int64_t getFileSize();
    
    virtual UMID getMaterialPackageUID();
    virtual UMID getFileSourcePackageUID();
    virtual UMID getTapeSourcePackageUID();
    
private:
    uint32_t _componentDepth;
    Rational _aspectRatio;
    int _numAudioTracks;
    bool _includeCRC32;
    int64_t _startPosition;
    
    TimecodeBreakHelper _timecodeBreakHelper;
    
    ::ArchiveMXFWriter *_writer;
};



};


#endif
