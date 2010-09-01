/*
 * $Id: ArchiveMXFContentPackage.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#ifndef __RECORDER_ARCHIVE_MXF_CONTENT_PACKAGE_H__
#define __RECORDER_ARCHIVE_MXF_CONTENT_PACKAGE_H__

#include "MXFContentPackage.h"

#include "Types.h"


namespace rec
{


class ArchiveMXFContentPackage : public MXFContentPackage
{
public:
    ArchiveMXFContentPackage();
    virtual ~ArchiveMXFContentPackage();

public:
    // reading
    
    int64_t getPosition() const { return _position; }
    
    Timecode getCTC() const { return _ctc; }
    bool haveLTC() const { return _haveLTC; }
    Timecode getLTC() const { return _ltc; }
    bool haveVITC() const { return _haveVITC; }
    Timecode getVITC() const { return _vitc; }
    
    bool videoIs8Bit() const { return _videoIs8Bit; }
    const unsigned char* getVideo() const;
    unsigned int getVideoSize() const;
    const unsigned char* getVideo8Bit() const;
    unsigned int getVideo8BitSize() const;

    int getNumAudioTracks() const { return _numAudioTracks; }
    const unsigned char* getAudio(int index) const;
    unsigned int getAudioSize() const { return _audioSize; }
    
    int getNumCRC32() const { return _numCRC32; }
    uint32_t getCRC32(int index) const { return _crc32[index]; }
    const uint32_t* getCRC32() const { return _crc32; }
    
public:
    // writing

    void setVideoIs8Bit(bool is_8bit);
    void setVideoSize(unsigned int size);
    void setVideo8BitSize(unsigned int size);
    void setNumAudioTracks(int count);
    void setAudioSize(unsigned int size);
    void setNumCRC32(int count);
    
    void setPosition(int64_t position);
    
    void setMissingVITC();
    void setVITC(Timecode vitc);
    void setMissingLTC();
    void setLTC(Timecode ltc);
    
    void allocateVideo(unsigned int video_size);
    void setVideo(const unsigned char *video, unsigned int video_size, bool create_copy);
    unsigned char* getVideoBuffer();
    void allocateVideo8Bit(unsigned int video_8bit_size);
    void setVideo8Bit(const unsigned char *video_8bit, unsigned int video_8bit_size, bool create_copy);
    unsigned char* getVideo8BitBuffer();

    void allocateAudio(unsigned int audio_size);
    void setAudio(int index, const unsigned char *audio, unsigned int audio_size, bool create_copy);
    unsigned char* getAudioBuffer(int index);
    
    void setCRC32(int index, uint32_t crc32);
    
private:
    int64_t _position;
    
    Timecode _ctc;
    bool _haveVITC;
    Timecode _vitc;
    bool _haveLTC;
    Timecode _ltc;
    
    bool _videoIs8Bit;
    unsigned char *_video;
    const unsigned char *_cvideo;
    unsigned int _videoSize;
    unsigned char *_video8Bit;
    const unsigned char *_cvideo8Bit;
    unsigned int _video8BitSize;
    
    unsigned char *_audio[8];
    const unsigned char *_caudio[8];
    unsigned int _audioSize;
    int _numAudioTracks;
    
    uint32_t _crc32[9];
    int _numCRC32;
};



};


#endif

