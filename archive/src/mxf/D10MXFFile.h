/*
 * $Id: D10MXFFile.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * SMPTE D-10 (S386M) MXF file reader
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

#ifndef __RECORDER_D10_MXF_FILE_H__
#define __RECORDER_D10_MXF_FILE_H__


#include <mxf_reader.h>
#include <mxf/mxf_page_file.h>

#include "MXFFileReader.h"
#include "D10MXFContentPackage.h"
#include "MXFEventFile.h"
#include "FFmpegDecoder.h"



namespace rec
{


class D10MXFFile : public MXFFileReader
{
public:
    D10MXFFile(std::string filename, std::string eventFilename, bool decodeVideo, bool decodeToYUV420);
    D10MXFFile(std::string filename, int64_t pageSize, std::string eventFilename, bool decodeVideo, bool decodeToYUV420);
    virtual ~D10MXFFile();
    
public:
    virtual long getPSEFailures(PSEFailure **failures) const;
    virtual long getVTRErrors(VTRErrorAtPos **errors) const;
    virtual long getDigiBetaDropouts(DigiBetaDropout **digiBetaDropouts) const;
    virtual long getTimecodeBreaks(TimecodeBreak **timecodeBreaks) const;
    
    virtual bool nextFrame(bool ignoreAVEssenceData, MXFContentPackage *&contentPackage);
    virtual bool skipFrames(int64_t count);
    virtual bool readFrame(int64_t position, bool ignoreAVEssenceData, MXFContentPackage *&contentPackage);
    virtual void forwardTruncate();

private:
    void initialize(std::string filename, int64_t pageSize, std::string eventFilename, bool decodeVideo, bool decodeToYUV420);

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
    
    MXFEventFile _eventFile;

    FFmpegDecoder *_decoder;

    D10MXFContentPackage _contentPackage;
    int _videoTrack;
    int _audioTrack;
    
    uint8_t *_audioBuffer;
    uint32_t _audioBufferSize;

    // used in MXFReader callbacks
    bool _ignoreAVEssenceData;
};


};



#endif

