/*
 * $Id: D10MXFFile.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#include <cstdlib>
#include <cstring>

#include "D10MXFFile.h"
#include "Logging.h"
#include "RecorderException.h"

using namespace std;
using namespace rec;



struct _MXFReaderListenerData
{
    D10MXFFile *mxfFile;
};



D10MXFFile::D10MXFFile(string filename, string eventFilename, bool decodeVideo, bool decodeToYUV420)
: MXFFileReader(filename)
{
    initialize(filename, 0, eventFilename, decodeVideo, decodeToYUV420);
}

D10MXFFile::D10MXFFile(string filename, int64_t pageSize, string eventFilename, bool decodeVideo, bool decodeToYUV420)
: MXFFileReader(filename)
{
    initialize(filename, pageSize, eventFilename, decodeVideo, decodeToYUV420);
}

D10MXFFile::~D10MXFFile()
{
    close_mxf_reader(&_mxfReader);
    mxf_free_data_model(&_dataModel);
    
    delete _decoder;
    
    delete [] _audioBuffer;
}

long D10MXFFile::getPSEFailures(PSEFailure **failures) const
{
    return _eventFile.GetPSEFailures(failures);
}

long D10MXFFile::getVTRErrors(VTRErrorAtPos **errors) const
{
    return _eventFile.GetVTRErrors(errors);
}

long D10MXFFile::getDigiBetaDropouts(DigiBetaDropout **digiBetaDropouts) const
{
    return _eventFile.GetDigiBetaDropouts(digiBetaDropouts);
}

long D10MXFFile::getTimecodeBreaks(TimecodeBreak **timecodeBreaks) const
{
    return _eventFile.GetTimecodeBreaks(timecodeBreaks);
}

void D10MXFFile::initialize(string filename, int64_t pageSize, string eventFilename, bool decodeVideo, bool decodeToYUV420)
{
    REC_CHECK(pageSize == 0 || filename.find("%d") != string::npos);
    
    _mxfFile = 0;
    _mxfPageFile = 0;
    _mxfReader = 0;
    _dataModel = 0;
    _decoder = 0;
    _videoTrack = -1;
    _audioTrack = -1;
    _audioBuffer = 0;
    _audioBufferSize = 0;
    
    
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
        
        
        // init reader
        
        REC_CHECK(mxf_load_data_model(&_dataModel));
        REC_CHECK(mxf_finalise_data_model(_dataModel));
        REC_CHECK(init_mxf_reader_2(&_mxfFile, _dataModel, &_mxfReader));
        
        
        // get metadata
        
        _duration = get_duration(_mxfReader);
        int numTracks = get_num_tracks(_mxfReader);
        int i;
        for (i = 0; i < numTracks; i++) {
            MXFTrack *track = get_mxf_track(_mxfReader, i);
            if (track->isVideo) {
                _aspectRatio.numerator = track->video.aspectRatio.numerator;
                _aspectRatio.denominator = track->video.aspectRatio.denominator;
                break;
            }
        }
        
        
        // allocate buffers
        
        for (i = 0; i < numTracks; i++) {
            MXFTrack *track = get_mxf_track(_mxfReader, i);
            if (track->isVideo)
                _videoTrack = i;
            else
                _audioTrack = i;
        }
        REC_CHECK(_videoTrack != -1);
        REC_CHECK(_audioTrack != -1);
        
        MXFTrack *audioTrackInfo = get_mxf_track(_mxfReader, _audioTrack);
        
        _contentPackage.setNumAudioTracks(audioTrackInfo->audio.channelCount);
        _contentPackage.setAudioSize(1920 * 3);
        _contentPackage.allocateAudio(_contentPackage.getAudioSize());
        
        
        // read event file
        
        if (!_eventFile.Read(eventFilename))
        {
            Logging::warning("Failed to read events from file '%s'\n", eventFilename.c_str());
        }
        
        
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
        
        throw;
    }
    
    if (decodeVideo)
        _decoder = new FFmpegDecoder(MaterialResolution::IMX50_MXF_1A, Ingex::VideoRaster::PAL, decodeToYUV420, 0);
}

bool D10MXFFile::nextFrame(bool ignoreAVEssenceData, MXFContentPackage *&contentPackage)
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

    
    // get LTC from system item pack's user timecode
    
    MXFTimecode mxfTimecode;
    int mxfTimecodeType;
    Timecode timecode;
    int count;
    int numSourceTimecodes = get_num_source_timecodes(_mxfReader);
    int i;
    for (i = 0; i < numSourceTimecodes; i++) {
        if (get_source_timecode(_mxfReader, i, &mxfTimecode, &mxfTimecodeType, &count) == 1) {
            if (mxfTimecodeType == SYSTEM_ITEM_SDTI_USER_TIMECODE) {
                timecode.hour = mxfTimecode.hour;
                timecode.min = mxfTimecode.min;
                timecode.sec = mxfTimecode.sec;
                timecode.frame = mxfTimecode.frame;
                _contentPackage.setLTC(timecode);
                break;
            }
        }
    }
    if (i >= numSourceTimecodes)
        _contentPackage.setMissingLTC();
    
    
    // decode video and VITC
    
    if (_decoder && !ignoreAVEssenceData) {
        const unsigned char *decoded_data;
        unsigned int decoded_size;
        bool have_vitc = false;
        rec::Timecode vitc;
        if (!_decoder->DecodeFrame(_contentPackage.getVideo(), _contentPackage.getVideoSize(),
                                   &decoded_data, &decoded_size, &have_vitc, &vitc))
        {
            Logging::error("Failed to decode D10 video frame\n");
            return false;
        }
        
        _contentPackage.setDecodedVideo(decoded_data, decoded_size);
        
        if (have_vitc)
            _contentPackage.setVITC(vitc);
        else
            _contentPackage.setMissingVITC();
    } else {
        _contentPackage.setMissingVITC();
    }
    
    
    contentPackage = &_contentPackage;
    return true;
}

bool D10MXFFile::skipFrames(int64_t count)
{
    REC_ASSERT(_isComplete);
    
    int64_t lastFrameNumber = get_frame_number(_mxfReader);

    int result = position_at_frame(_mxfReader, lastFrameNumber + 1 + count);

    return result == 1;
}

bool D10MXFFile::readFrame(int64_t position, bool ignoreAVEssenceData, MXFContentPackage *&contentPackage)
{
    REC_ASSERT(_isComplete);
    
    int result = position_at_frame(_mxfReader, position);
    if (result != 1)
        return false;

    return nextFrame(ignoreAVEssenceData, contentPackage);
}

void D10MXFFile::forwardTruncate()
{
    REC_ASSERT(_isComplete);
    REC_ASSERT(_mxfPageFile != 0);
    
    REC_CHECK(mxf_page_file_forward_truncate(_mxfPageFile));
}


int D10MXFFile::accept_frame(MXFReaderListener* mxfListener, int trackIndex)
{
    D10MXFFile *mxfFile = mxfListener->data->mxfFile;
    
    if (mxfFile->_ignoreAVEssenceData)
        return 0;
    
    return trackIndex == mxfFile->_videoTrack || trackIndex == mxfFile->_audioTrack;
}

int D10MXFFile::allocate_buffer(MXFReaderListener *mxfListener, int trackIndex, uint8_t **buffer, uint32_t bufferSize)
{
    D10MXFFile *mxfFile = mxfListener->data->mxfFile;
    
    if (mxfFile->_ignoreAVEssenceData)
        return 0;
    
    if (trackIndex == mxfFile->_videoTrack) {
        if (mxfFile->_contentPackage.getVideoSize() == 0) {
            mxfFile->_contentPackage.setVideoSize(bufferSize);
            mxfFile->_contentPackage.allocateVideo(bufferSize);
        } else {
            REC_CHECK(bufferSize == mxfFile->_contentPackage.getVideoSize());
        }
        *buffer = mxfFile->_contentPackage.getVideoBuffer();
        return 1;
    } else if (trackIndex == mxfFile->_audioTrack) {
        if (mxfFile->_audioBufferSize < bufferSize) {
            delete [] mxfFile->_audioBuffer;
            mxfFile->_audioBuffer = new uint8_t[bufferSize];
            mxfFile->_audioBufferSize = bufferSize;
        }
        *buffer = mxfFile->_audioBuffer;
        return 1;
    }

    return 0;
}

void D10MXFFile::deallocate_buffer(MXFReaderListener *mxfListener, int trackIndex, uint8_t **buffer)
{
    (void)mxfListener;
    (void)trackIndex;
    (void)buffer;
    
    // do nothing
}

int D10MXFFile::receive_frame(MXFReaderListener *mxfListener, int trackIndex, uint8_t *buffer, uint32_t bufferSize)
{
    (void)buffer;
    
    D10MXFFile *mxfFile = mxfListener->data->mxfFile;
    
    if (trackIndex == mxfFile->_videoTrack) {
        REC_CHECK(bufferSize == mxfFile->_contentPackage.getVideoSize());
    } else {
        // deinterleave audio channels into tracks
        
        REC_CHECK(bufferSize == mxfFile->_contentPackage.getNumAudioTracks() * mxfFile->_contentPackage.getAudioSize());
        
        // input is 24-bit audio where samples of mxfFile->_contentPackage.getNumAudioTracks() channels are interleaved
        int c, s;
        uint8_t *input;
        unsigned char *output;
        for (c = 0; c < mxfFile->_contentPackage.getNumAudioTracks(); c++) {
            input = buffer + c * 3;
            output = mxfFile->_contentPackage.getAudioBuffer(c);
            
            for (s = 0; s < 1920; s++) {
                *output++ = *input++;
                *output++ = *input++;
                *output++ = *input++;
                input += (mxfFile->_contentPackage.getNumAudioTracks() - 1) * 3;
            }
        }
    }
    
    return 1;
}

