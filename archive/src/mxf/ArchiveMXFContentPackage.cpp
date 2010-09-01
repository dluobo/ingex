/*
 * $Id: ArchiveMXFContentPackage.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * Archive MXF content package
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

#include "ArchiveMXFContentPackage.h"
#include "RecorderException.h"
#include "Logging.h"

using namespace rec;



ArchiveMXFContentPackage::ArchiveMXFContentPackage()
{
    setPosition(0);
    _haveVITC = false;
    _haveLTC = false;
    _videoIs8Bit = false;
    _video = 0;
    _cvideo = 0;
    _videoSize = 0;
    _video8Bit = 0;
    _cvideo8Bit = 0;
    _video8BitSize = 0;
    memset(_audio, 0, sizeof(_audio));
    memset(_caudio, 0, sizeof(_caudio));
    _audioSize = 0;
    _numAudioTracks = 0;
    memset(_crc32, 0, sizeof(_crc32));
    _numCRC32 = 0;
}

ArchiveMXFContentPackage::~ArchiveMXFContentPackage()
{
    delete [] _video;
    delete [] _video8Bit;
    
    int i;
    for (i = 0; i < _numAudioTracks; i++)
        delete [] _audio[i];
}

const unsigned char* ArchiveMXFContentPackage::getVideo() const
{
    if (_video)
        return _video;
    else
        return _cvideo;
}

unsigned int ArchiveMXFContentPackage::getVideoSize() const
{
    return _videoSize;
}

const unsigned char* ArchiveMXFContentPackage::getVideo8Bit() const
{
    if (_videoIs8Bit)
        return getVideo();
    else if (_video8Bit)
        return _video8Bit;
    else
        return _cvideo8Bit;
}

unsigned int ArchiveMXFContentPackage::getVideo8BitSize() const
{
    if (_videoIs8Bit)
        return getVideoSize();
    else
        return _video8BitSize;
}
    
const unsigned char* ArchiveMXFContentPackage::getAudio(int index) const
{
    REC_CHECK(index >= 0 && index < _numAudioTracks);
    
    if (_audio[index])
        return _audio[index];
    else
        return _caudio[index];
}

void ArchiveMXFContentPackage::setVideoIs8Bit(bool is_8bit)
{
    _videoIs8Bit = is_8bit;
}

void ArchiveMXFContentPackage::setVideoSize(unsigned int size)
{
    REC_ASSERT(_videoSize == 0 || _videoSize == size);
    _videoSize = size;
}

void ArchiveMXFContentPackage::setVideo8BitSize(unsigned int size)
{
    REC_ASSERT(_video8BitSize == 0 || _video8BitSize == size);
    _video8BitSize = size;
}

void ArchiveMXFContentPackage::setNumAudioTracks(int count)
{
    REC_ASSERT(_numAudioTracks == 0 || _numAudioTracks == count);
    _numAudioTracks = count;
}

void ArchiveMXFContentPackage::setAudioSize(unsigned int size)
{
    REC_ASSERT(_audioSize == 0 || _audioSize == size);
    _audioSize = size;
}

void ArchiveMXFContentPackage::setNumCRC32(int count)
{
    _numCRC32 = count;
}

void ArchiveMXFContentPackage::setPosition(int64_t position)
{
    _position = position;
    
    _ctc.hour = position / (60 * 60 * 25);
    _ctc.min = (position % (60 * 60 * 25)) / (60 * 25);
    _ctc.sec = ((position % (60 * 60 * 25)) % (60 * 25)) / 25;
    _ctc.frame = ((position % (60 * 60 * 25)) % (60 * 25)) % 25;
}

void ArchiveMXFContentPackage::setMissingVITC()
{
    _haveVITC = false;
    _vitc = Timecode();
}

void ArchiveMXFContentPackage::setVITC(Timecode vitc)
{
    _vitc = vitc;
    _haveVITC = true;
}

void ArchiveMXFContentPackage::setMissingLTC()
{
    _haveLTC = false;
    _ltc = Timecode();
}

void ArchiveMXFContentPackage::setLTC(Timecode ltc)
{
    _ltc = ltc;
    _haveLTC = true;
}

void ArchiveMXFContentPackage::allocateVideo(unsigned int video_size)
{
    REC_CHECK(video_size == _videoSize);

    if (!_video)
        _video = new unsigned char[_videoSize];
}

void ArchiveMXFContentPackage::setVideo(const unsigned char *video, unsigned int video_size,
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

unsigned char* ArchiveMXFContentPackage::getVideoBuffer()
{
    REC_CHECK(_video);
    return _video;
}

void ArchiveMXFContentPackage::allocateVideo8Bit(unsigned int video_8bit_size)
{
    REC_CHECK(video_8bit_size == _video8BitSize);
    REC_CHECK(!_videoIs8Bit);

    if (!_video8Bit)
        _video8Bit = new unsigned char[_video8BitSize];
}

void ArchiveMXFContentPackage::setVideo8Bit(const unsigned char *video_8bit, unsigned int video_8bit_size,
                                            bool create_copy)
{
    REC_CHECK(video_8bit_size == _video8BitSize);
    REC_CHECK(!_videoIs8Bit);
    
    if (create_copy) {
        if (!_video8Bit)
            _video8Bit = new unsigned char[_video8BitSize];
        memcpy(_video8Bit, video_8bit, _video8BitSize);
    } else {
        _cvideo8Bit = video_8bit;
    }
}

unsigned char* ArchiveMXFContentPackage::getVideo8BitBuffer()
{
    if (_videoIs8Bit) {
        return getVideoBuffer();
    } else {
        REC_CHECK(_video8Bit);
        return _video8Bit;
    }
}

void ArchiveMXFContentPackage::allocateAudio(unsigned int audio_size)
{
    REC_CHECK(audio_size == _audioSize);
    
    int i;
    for (i = 0; i < _numAudioTracks; i++)
        if (!_audio[i])
            _audio[i] = new unsigned char[_audioSize];
}

void ArchiveMXFContentPackage::setAudio(int index, const unsigned char *audio, unsigned int audio_size,
                                        bool create_copy)
{
    REC_CHECK(audio_size == _audioSize);
    REC_CHECK(index >= 0 && index < _numAudioTracks);
    
    if (create_copy) {
        if (!_audio[index])
            _audio[index] = new unsigned char[_audioSize];
        memcpy(_audio[index], audio, _audioSize);
    } else {
        _caudio[index] = audio;
    }
}

unsigned char* ArchiveMXFContentPackage::getAudioBuffer(int index)
{
    REC_CHECK(index >= 0 && index < _numAudioTracks);
    REC_CHECK(_audio[index]);
    
    return _audio[index];
}

void ArchiveMXFContentPackage::setCRC32(int index, uint32_t crc32)
{
    REC_CHECK(index >= 0 && index < _numCRC32);
    
    _crc32[index] = crc32;
}

