/*
 * $Id: D10MXFWriter.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * SMPTE D-10 (S386M) MXF file writer
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

#ifndef __RECORDER_D10_MXF_WRITER_H__
#define __RECORDER_D10_MXF_WRITER_H__

#include "MXFWriter.h"
#include "MXFEventFileWriter.h"
#include "TimecodeBreakHelper.h"

#include <mxf/mxf_file.h>
#include <timecode_index.h>


class D10MXFOP1AWriter;



namespace rec
{


class D10MXFWriter : public MXFWriter
{
private:
    typedef struct
    {
        int64_t duration;
        int64_t start_timecode;
        int64_t last_timecode;
        bool broken_timecodes;
    } TimecodeBreak;

public:
    static uint32_t getContentPackageSize();
    
public:
    D10MXFWriter();
    virtual ~D10MXFWriter();
    
    void setAspectRatio(Rational aspect_ratio);                 // either 4:3 or 16:9. Default 4:3
    void setNumAudioTracks(int count);                          // Default 4
    void setPrimaryTimecode(PrimaryTimecode primary_timecode);  // Default PRIMARY_TIMECODE_AUTO
    
    void setEventFilename(std::string filename);
    
    bool createFile(std::string filename);
    bool createFile(MXFFile **mxfFile, std::string filename);

    int getNumAudioTracks() const { return _numAudioTracks; }
    
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
    void createWriter();

    void processPrimaryTimecode(bool have_timecode, int64_t timecode);
    void processPrimaryTimecode(bool have_ltc, int64_t ltc, bool have_vitc, int64_t vitc);

    void writePSEFailureEvents(MXFEventFileWriter *event_file, const PSEFailure *pse_failures, long pse_failure_count);
    void writeVTRErrorEvents(MXFEventFileWriter *event_file, const VTRError *vtr_errors, long vtr_error_count);
    void writeDigiBetaDropoutEvents(MXFEventFileWriter *event_file, const DigiBetaDropout *digibeta_dropouts,
                                    long digibeta_dropout_count);
    
    bool findVTRErrorPosition(TimecodeIndexSearcher *vitc_index_searcher,
                              TimecodeIndexSearcher *ltc_index_searcher,
                              const VTRError *vtr_error,
                              int64_t *error_position, bool *matched_ltc);
    
private:
    Rational _aspectRatio;
    int _numAudioTracks;
    int64_t _startPosition;
    PrimaryTimecode _primaryTimecode;
    
    std::string _eventFilename;
    
    D10MXFOP1AWriter *_writer;
    
    TimecodeIndex _vitcTimecodeIndex;
    TimecodeIndex _ltcTimecodeIndex;
    
    TimecodeBreakHelper _timecodeBreakHelper;
};



};


#endif
