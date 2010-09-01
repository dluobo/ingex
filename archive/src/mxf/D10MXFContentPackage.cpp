/*
 * $Id: D10MXFContentPackage.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * SMPTE D-10 (S386M) content package
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
#include <D10MXFOP1AWriter.h>

#include "D10MXFContentPackage.h"
#include "RecorderException.h"
#include "Logging.h"

using namespace rec;



D10MXFContentPackage::D10MXFContentPackage()
{
    _haveVITC = false;
    _haveLTC = false;
    _video = 0;
    _cvideo = 0;
    _videoSize = 0;
    _decodedVideo = 0;
    _decodedVideoSize = 0;
    memset(_audio, 0, sizeof(_audio));
    memset(_caudio, 0, sizeof(_caudio));
    _audioSize = 0;
    _numAudioTracks = 0;
}

D10MXFContentPackage::~D10MXFContentPackage()
{
    delete [] _video;
    
    int i;
    for (i = 0; i < _numAudioTracks; i++)
        delete [] _audio[i];
}

const unsigned char* D10MXFContentPackage::getVideo() const
{
    if (_video)
        return _video;
    else
        return _cvideo;
}

const unsigned char* D10MXFContentPackage::getAudio(int index) const
{
    REC_CHECK(index >= 0 && index < _numAudioTracks);
    
    if (_audio[index])
        return _audio[index];
    else
        return _caudio[index];
}

void D10MXFContentPackage::setVideoSize(unsigned int size)
{
    REC_ASSERT(_videoSize == 0 || _videoSize == size);
    _videoSize = size;
}

void D10MXFContentPackage::setNumAudioTracks(int count)
{
    REC_ASSERT(_numAudioTracks == 0 || _numAudioTracks == count);
    _numAudioTracks = count;
}

void D10MXFContentPackage::setAudioSize(unsigned int size)
{
    REC_ASSERT(_audioSize == 0 || _audioSize == size);
    _audioSize = size;
}

void D10MXFContentPackage::setPosition(int64_t position)
{
    _position = position;
}

void D10MXFContentPackage::setMissingVITC()
{
    _haveVITC = false;
    _vitc = Timecode();
}

void D10MXFContentPackage::setVITC(rec::Timecode vitc)
{
    _vitc = vitc;
    _haveVITC = true;
}

void D10MXFContentPackage::setMissingLTC()
{
    _haveLTC = false;
    _ltc = Timecode();
}

void D10MXFContentPackage::setLTC(rec::Timecode ltc)
{
    _ltc = ltc;
    _haveLTC = true;
}

void D10MXFContentPackage::allocateVideo(unsigned int video_size)
{
    REC_CHECK(video_size == _videoSize);

    if (!_video)
        _video = new unsigned char[_videoSize];
}

void D10MXFContentPackage::setVideo(const unsigned char *video, unsigned int video_size,
                                    bool create_copy)
{
    REC_CHECK(video_size == _videoSize);
    
    if (create_copy) {
        if (!_video)
            _video = new unsigned char[_videoSize];
        memcpy(_video, video, _videoSize);
    } else {
        _cvideo = video;
    }
}

unsigned char* D10MXFContentPackage::getVideoBuffer()
{
    REC_CHECK(_video);
    return _video;
}

void D10MXFContentPackage::setDecodedVideo(const unsigned char *decoded_video, unsigned int decoded_video_size)
{
    _decodedVideo = decoded_video;
    _decodedVideoSize = decoded_video_size;
}

void D10MXFContentPackage::allocateAudio(unsigned int audio_buffer_size)
{
    REC_CHECK(audio_buffer_size >= _audioSize);
    
    _audioBufferSize = audio_buffer_size;
    
    int i;
    for (i = 0; i < _numAudioTracks; i++)
        if (!_audio[i])
            _audio[i] = new unsigned char[audio_buffer_size];
}

void D10MXFContentPackage::setAudio(int index, const unsigned char *audio, unsigned int audio_size,
                                    bool create_copy)
{
    REC_CHECK(audio_size == _audioSize);
    REC_CHECK(index >= 0 && index < _numAudioTracks);
    
    if (create_copy) {
        if (!_audio[index]) {
            _audio[index] = new unsigned char[_audioSize];
            _audioBufferSize = _audioSize;
        }
        memcpy(_audio[index], audio, _audioSize);
    } else {
        _caudio[index] = audio;
    }
}

unsigned char* D10MXFContentPackage::getAudioBuffer(int index)
{
    REC_CHECK(index >= 0 && index < _numAudioTracks);
    REC_CHECK(_audio[index]);
    
    return _audio[index];
}

