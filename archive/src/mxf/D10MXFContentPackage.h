/*
 * $Id: D10MXFContentPackage.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#ifndef __RECORDER_D10_MXF_CONTENT_PACKAGE_H__
#define __RECORDER_D10_MXF_CONTENT_PACKAGE_H__

#include "MXFContentPackage.h"
#include "Types.h"

#include <D10ContentPackage.h>


namespace rec
{


class D10MXFContentPackage : public MXFContentPackage
{
public:
    D10MXFContentPackage();
    virtual ~D10MXFContentPackage();

public:
    // reading
    
    int64_t getPosition() const { return _position; }
    
    bool haveVITC() const { return _haveVITC; }
    Timecode getVITC() const { return _vitc; }
    bool haveLTC() const { return _haveLTC; }
    Timecode getLTC() const { return _ltc; }
    
    const unsigned char* getVideo() const;
    unsigned int getVideoSize() const { return _videoSize; }

    const unsigned char* getDecodedVideo() const { return _decodedVideo; }
    unsigned int getDecodedVideoSize() const { return _decodedVideoSize; }

    int getNumAudioTracks() const { return _numAudioTracks; }
    const unsigned char* getAudio(int index) const;
    unsigned int getAudioSize() const { return _audioSize; }
    
public:
    // writing

    void setVideoSize(unsigned int size);
    void setNumAudioTracks(int count);
    void setAudioSize(unsigned int size);
    
    void setPosition(int64_t position);
    
    void setMissingVITC();
    void setVITC(rec::Timecode vitc);
    void setMissingLTC();
    void setLTC(rec::Timecode ltc);
    
    void allocateVideo(unsigned int video_size);
    void setVideo(const unsigned char *video, unsigned int video_size, bool create_copy);
    unsigned char* getVideoBuffer();

    void setDecodedVideo(const unsigned char *decoded_video, unsigned int decoded_video_size);
    
    void allocateAudio(unsigned int audio_buffer_size);
    void setAudio(int index, const unsigned char *audio, unsigned int audio_size, bool create_copy);
    unsigned char* getAudioBuffer(int index);
    unsigned int getAudioBufferSize() const { return _audioBufferSize; }
    
private:
    int64_t _position;
    
    bool _haveVITC;
    Timecode _vitc;
    bool _haveLTC;
    Timecode _ltc;
    
    unsigned char *_video;
    const unsigned char *_cvideo;
    unsigned int _videoSize;
    
    const unsigned char *_decodedVideo;
    unsigned int _decodedVideoSize;
    
    unsigned char *_audio[8];
    const unsigned char *_caudio[8];
    unsigned int _audioSize;
    unsigned int _audioBufferSize;
    int _numAudioTracks;
};



};


#endif

