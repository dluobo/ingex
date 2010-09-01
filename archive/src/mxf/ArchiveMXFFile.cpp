/*
 * $Id: ArchiveMXFFile.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#include <cstdlib>
#include <cstring>

#include "ArchiveMXFFile.h"
#include "Logging.h"
#include "RecorderException.h"

#include <video_conversion_10bits.h>

#include <archive_mxf_info_lib.h>

using namespace std;
using namespace rec;



struct _MXFReaderListenerData
{
    ArchiveMXFFile *mxfFile;
};



ArchiveMXFFile::ArchiveMXFFile(string filename)
: MXFFileReader(filename)
{
    initialize(filename, 0);
}

ArchiveMXFFile::ArchiveMXFFile(string filename, int64_t pageSize)
: MXFFileReader(filename)
{
    initialize(filename, pageSize);
}

ArchiveMXFFile::~ArchiveMXFFile()
{
    close_mxf_reader(&_mxfReader);
    mxf_free_data_model(&_dataModel);
    
    if (_pseFailures)
        free(_pseFailures);
    if (_vtrErrors)
        free(_vtrErrors);
    if (_digiBetaDropouts)
        free(_digiBetaDropouts);
    if (_timecodeBreaks)
        free(_timecodeBreaks);
}

void ArchiveMXFFile::initialize(string filename, int64_t pageSize)
{
    REC_CHECK(pageSize == 0 || filename.find("%d") != string::npos);
    
    _mxfFile = 0;
    _mxfPageFile = 0;
    _mxfReader = 0;
    _dataModel = 0;
    _componentDepth = 8;
    _pseFailures = 0;
    _numPSEFailures = 0;
    _vtrErrors = 0;
    _numVTRErrors = 0;
    _digiBetaDropouts = 0;
    _numDigiBetaDropouts = 0;
    _timecodeBreaks = 0;
    _numTimecodeBreaks = 0;
    _videoTrack = 0;
    memset(_audioTrack, 0, sizeof(_audioTrack));
    
    
    // extract properties
    
    try
    {
        // open the mxf file
        
        if (pageSize != 0) {
            // open the page file for modification because we might want to forward truncate it
            if (!mxf_page_file_open_modify(filename.c_str(), pageSize, &_mxfPageFile))
                REC_LOGTHROW(("Failed to open MXF page file '%s'", filename.c_str()));
            _mxfFile = mxf_page_file_get_file(_mxfPageFile);
        } else {
            if (!mxf_disk_file_open_read(filename.c_str(), &_mxfFile))
                REC_LOGTHROW(("Failed to open MXF file '%s'", filename.c_str()));
        }
        
        
        // load archive extensions and init reader
        
        REC_CHECK(mxf_load_data_model(&_dataModel));
        REC_CHECK(archive_mxf_load_extensions(_dataModel));
        REC_CHECK(mxf_finalise_data_model(_dataModel));
        REC_CHECK(init_mxf_reader_2(&_mxfFile, _dataModel, &_mxfReader));
        
        
        // without the footer the file is incomplete
        
        if (!have_footer_metadata(_mxfReader)) {
            _isComplete = false;
            return;
        }

        
        // get metadata
        
        _duration = get_duration(_mxfReader);
        int numTracks = get_num_tracks(_mxfReader);
        int i;
        for (i = 0; i < numTracks; i++) {
            MXFTrack *track = get_mxf_track(_mxfReader, i);
            if (track->isVideo) {
                _aspectRatio.numerator = track->video.aspectRatio.numerator;
                _aspectRatio.denominator = track->video.aspectRatio.denominator;
                _componentDepth = track->video.componentDepth;
                break;
            }
        }

        ArchiveMXFInfo archiveMXFInfo;
        memset(&archiveMXFInfo, 0, sizeof(ArchiveMXFInfo));
        REC_CHECK(archive_mxf_get_info(get_header_metadata(_mxfReader), &archiveMXFInfo));
        _sourceInfaxData = archiveMXFInfo.sourceInfaxData;
        _ltoInfaxData = archiveMXFInfo.ltoInfaxData;

        REC_CHECK(archive_mxf_get_pse_failures(get_header_metadata(_mxfReader), &_pseFailures, &_numPSEFailures));
        REC_CHECK(archive_mxf_get_vtr_errors(get_header_metadata(_mxfReader), &_vtrErrors, &_numVTRErrors));
        REC_CHECK(archive_mxf_get_digibeta_dropouts(get_header_metadata(_mxfReader), &_digiBetaDropouts, &_numDigiBetaDropouts));
        REC_CHECK(archive_mxf_get_timecode_breaks(get_header_metadata(_mxfReader), &_timecodeBreaks, &_numTimecodeBreaks));
        
        
        // allocate buffers
        
        if (_componentDepth == 8) {
            _contentPackage.setVideoIs8Bit(true);
            _contentPackage.setVideoSize(720 * 576 * 2);
            _contentPackage.allocateVideo(_contentPackage.getVideoSize());
        } else {
            _contentPackage.setVideoIs8Bit(false);
            _contentPackage.setVideoSize((720 + 5) / 6 * 16 * 576);
            _contentPackage.allocateVideo(_contentPackage.getVideoSize());
            _contentPackage.setVideo8BitSize(720 * 576 * 2);
            _contentPackage.allocateVideo8Bit(_contentPackage.getVideo8BitSize());
        }

        int numAudioTracks = 0;
        for (i = 0; i < numTracks; i++) {
            MXFTrack *track = get_mxf_track(_mxfReader, i);
            if (track->isVideo) {
                _videoTrack = i;
            } else {
                _audioTrack[numAudioTracks] = i;
                numAudioTracks++;
            }
        }
        _contentPackage.setNumAudioTracks(numAudioTracks);
        _contentPackage.setAudioSize(1920 * 3);
        _contentPackage.allocateAudio(_contentPackage.getAudioSize());
        
        _isComplete = true;
    }
    catch (...)
    {
        if (_mxfReader) {
            close_mxf_reader(&_mxfReader);
            _mxfReader = 0;
        }
        else if (_mxfFile) {
            mxf_file_close(&_mxfFile);
            _mxfFile = 0;
        }
        if (_dataModel) {
            mxf_free_data_model(&_dataModel);
            _dataModel = 0;
        }
        if (_pseFailures) {
            free(_pseFailures);
            _pseFailures = 0;
        }
        if (_vtrErrors) {
            free(_vtrErrors);
            _vtrErrors = 0;
        }
        if (_digiBetaDropouts) {
            free(_digiBetaDropouts);
            _digiBetaDropouts = 0;
        }
        if (_timecodeBreaks) {
            free(_timecodeBreaks);
            _timecodeBreaks = 0;
        }
        
        throw;
    }
}

bool ArchiveMXFFile::nextFrame(bool ignoreAVEssenceData, MXFContentPackage *&contentPackage)
{
    REC_ASSERT(_isComplete);
    
    MXFReaderListener mxfListener;
    MXFReaderListenerData mxfListenerData;
    
    mxfListenerData.mxfFile = this;
    
    mxfListener.data = &mxfListenerData;
    mxfListener.accept_frame = accept_frame;
    mxfListener.allocate_buffer = allocate_buffer;
    mxfListener.deallocate_buffer = deallocate_buffer;
    mxfListener.receive_frame = receive_frame;

    _ignoreAVEssenceData = ignoreAVEssenceData;
    
    
    // read next frame
    
    int result = read_next_frame(_mxfReader, &mxfListener);
    if (result != 1)
        return false;
    
    _contentPackage.setPosition(get_frame_number(_mxfReader));

    
    // convert 10-bit video to 8-bit
    
    if (!ignoreAVEssenceData && !_contentPackage.videoIs8Bit())
        DitherFrame(_contentPackage.getVideo8BitBuffer(), _contentPackage.getVideo(),
                    720 * 2, (720 + 5) / 6 * 16,
                    720, 576);


    // get the VITC and LTC
    
    MXFTimecode mxfTimecode;
    int mxfTimecodeType;
    Timecode timecode;
    int count;
    int numSourceTimecodes = get_num_source_timecodes(_mxfReader);
    int i;
    for (i = 0; i < numSourceTimecodes; i++) {
        if (get_source_timecode(_mxfReader, i, &mxfTimecode, &mxfTimecodeType, &count) == 1) {
            if (mxfTimecodeType == SYSTEM_ITEM_TC_ARRAY_TIMECODE) {
                timecode.hour = mxfTimecode.hour;
                timecode.min = mxfTimecode.min;
                timecode.sec = mxfTimecode.sec;
                timecode.frame = mxfTimecode.frame;
                if (count == 0)
                    _contentPackage.setVITC(timecode);
                else if (count == 1)
                    _contentPackage.setLTC(timecode);
            }
        }
    }
    
    
    // get the CRC-32 array if present
    
    _contentPackage.setNumCRC32(get_num_archive_crc32(_mxfReader));
    uint32_t crc32;
    for (i = 0; i < _contentPackage.getNumCRC32(); i++) {
        get_archive_crc32(_mxfReader, i, &crc32);
        _contentPackage.setCRC32(i, crc32);
    }
    
    
    contentPackage = &_contentPackage;
    return true;
}

bool ArchiveMXFFile::skipFrames(int64_t count)
{
    REC_ASSERT(_isComplete);
    
    int64_t lastFrameNumber = get_frame_number(_mxfReader);

    int result = position_at_frame(_mxfReader, lastFrameNumber + 1 + count);

    return result == 1;
}

bool ArchiveMXFFile::readFrame(int64_t position, bool ignoreAVEssenceData, MXFContentPackage *&contentPackage)
{
    REC_ASSERT(_isComplete);
    
    int result = position_at_frame(_mxfReader, position);
    if (result != 1)
        return false;

    return nextFrame(ignoreAVEssenceData, contentPackage);
}

void ArchiveMXFFile::forwardTruncate()
{
    REC_ASSERT(_isComplete);
    REC_ASSERT(_mxfPageFile != 0);
    
    REC_CHECK(mxf_page_file_forward_truncate(_mxfPageFile));
}


int ArchiveMXFFile::accept_frame(MXFReaderListener* mxfListener, int trackIndex)
{
    ArchiveMXFFile *mxfFile = mxfListener->data->mxfFile;
    
    if (mxfFile->_ignoreAVEssenceData)
        return 0;
    
    if (trackIndex == mxfFile->_videoTrack) {
        return 1;
    } else {
        int i;
        for (i = 0; i < mxfFile->_contentPackage.getNumAudioTracks(); i++) {
            if (trackIndex == mxfFile->_audioTrack[i])
                return 1;
        }
    }
    
    return 0;
}

int ArchiveMXFFile::allocate_buffer(MXFReaderListener *mxfListener, int trackIndex, uint8_t **buffer, uint32_t bufferSize)
{
    ArchiveMXFFile *mxfFile = mxfListener->data->mxfFile;
    
    if (mxfFile->_ignoreAVEssenceData)
        return 0;
    
    if (trackIndex == mxfFile->_videoTrack) {
        *buffer = mxfFile->_contentPackage.getVideoBuffer();
        return 1;
    } else {
        int i;
        for (i = 0; i < mxfFile->_contentPackage.getNumAudioTracks(); i++) {
            if (trackIndex == mxfFile->_audioTrack[i]) {
                *buffer = mxfFile->_contentPackage.getAudioBuffer(i);
                return 1;
            }
        }
    }

    return 0;
}

void ArchiveMXFFile::deallocate_buffer(MXFReaderListener *mxfListener, int trackIndex, uint8_t **buffer)
{
    (void)mxfListener;
    (void)trackIndex;
    (void)buffer;
    
    // do nothing
}

int ArchiveMXFFile::receive_frame(MXFReaderListener *mxfListener, int trackIndex, uint8_t *buffer, uint32_t bufferSize)
{
    (void)mxfListener;
    (void)trackIndex;
    (void)buffer;
    (void)bufferSize;
    
    // do nothing
    
    return 1;
}

