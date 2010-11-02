/*
 * $Id: IngexShm.h,v 1.7 2010/11/02 16:45:19 john_f Exp $
 *
 * Interface for reading audio/video data from shared memory.
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
 * All Rights Reserved.
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

#ifndef IngexShm_h
#define IngexShm_h

#include <ace/Thread.h>
#include <string>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

//#include <ace/Log_Msg.h>
//#include "integer_types.h"

#include "nexus_control.h"

/**
C++ API for the shared memory used to pass video/audio
data into the recorder.
*/
class IngexShm
{
public:
  // Singelton accessor
    static IngexShm * Instance()
    {
        if (mInstance == 0)  // is it the first call?
        {  
            mInstance = new IngexShm; // create sole instance
        }
        return mInstance; // address of sole instance
    }
  // Singleton destroy
    static void Destroy() { delete mInstance; mInstance = 0; }

    ~IngexShm();

    void Attach();

    unsigned int Channels();
    unsigned int AudioTracksPerChannel();
    int RingLength();
    int LastFrame(unsigned int channel);

    Ingex::VideoRaster::EnumType PrimaryVideoRaster();
    Ingex::VideoRaster::EnumType SecondaryVideoRaster();
    Ingex::PixelFormat::EnumType PrimaryPixelFormat();
    Ingex::PixelFormat::EnumType SecondaryPixelFormat();

    unsigned int PrimaryWidth();
    unsigned int PrimaryHeight();
    unsigned int SecondaryWidth();
    unsigned int SecondaryHeight();

    int FrameRateNumerator();
    int FrameRateDenominator();
    void GetFrameRate(int & numerator, int & denominator);
    void GetFrameRate(int & fps, bool & df);

    Ingex::Timecode Timecode(unsigned int channel, unsigned int frame);
    Ingex::Timecode CurrentTimecode(unsigned int channel);

    bool SignalPresent(unsigned int channel);
    int HwDrop(unsigned int channel);

    std::string SourceName(unsigned int channel);
    void SourceName(unsigned int channel, const std::string & name);

    int FrameNumber(unsigned int channel, unsigned int frame);

    uint8_t * pVideoPri(int channel, int frame);
    uint8_t * pVideoSec(unsigned int channel, unsigned int frame);
    int32_t * pPrimaryAudio(unsigned int channel, unsigned int frame, int track);
    int16_t * pSecondaryAudio(unsigned int channel, unsigned int frame, int track);
    NexusFrameData * pFrameData(unsigned int channel, unsigned int frame);

    int NumAudioSamples(unsigned int channel, unsigned int frame);

    void GetHeartbeat(struct timeval * tv);

    // Store recorder name ready for informational update when connecting to shared memory
    void RecorderName(const std::string & name);

    // Informational updates from Recorder to shared memory
    void InfoSetup();
    int InfoGetRecIdx(void);
    void InfoResetChannels(void);
    void InfoReset(unsigned int channel, int index, bool quad_video);
    void InfoSetEnabled(unsigned int channel, int index, bool quad_video, bool enabled);
    void InfoSetRecording(unsigned int channel, int index, bool quad_video, bool recording);
    void InfoSetDesc(unsigned int channel, int index, bool quad_video, const char *fmt, ...);
    void InfoSetRecordError(unsigned int channel, int index, bool quad_video, const char *fmt, ...);
    void InfoSetFramesWritten(unsigned int channel, int index, bool quad_video, int frames_written);
    void InfoSetFramesDropped(unsigned int channel, int index, bool quad_video, int frames_dropped);
    void InfoSetBacklog(unsigned int channel, int index, bool quad_video, int frames_in_backlog);


protected:
    // Protect constructors to force use of Instance()
    IngexShm();
    IngexShm(const IngexShm &);
    IngexShm & operator= (const IngexShm &);

private:
    // methods
    void Detach();

    // data
    std::string mRecorderName;
    unsigned int mChannels;
    unsigned int mAudioTracksPerChannel;
    uint8_t * mRing[MAX_CHANNELS];
    NexusControl * mpControl;
    // static instance pointer
    static IngexShm * mInstance;
    // thread management
    bool mActivated;
    ACE_thread_t mThreadId;

// friends
    friend ACE_THR_FUNC_RETURN monitor_shm(void * p);
};

// Inline method definitions

inline uint8_t * IngexShm::pVideoPri(int channel, int frame)
{
    return (uint8_t *)nexus_primary_video(mpControl, mRing, channel, frame);
}

inline uint8_t * IngexShm::pVideoSec(unsigned int channel, unsigned int frame)
{
    return (uint8_t *)nexus_secondary_video(mpControl, mRing, channel, frame);
}

inline int32_t * IngexShm::pPrimaryAudio(unsigned int channel, unsigned int frame, int track)
{
    return (int32_t *)nexus_primary_audio(mpControl, mRing, channel, frame, track);
}

inline int16_t * IngexShm::pSecondaryAudio(unsigned int channel, unsigned int frame, int track)
{
    return (int16_t *)nexus_secondary_audio(mpControl, mRing, channel, frame, track);
}

inline NexusFrameData * IngexShm::pFrameData(unsigned int channel, unsigned int frame)
{
    return nexus_frame_data(mpControl, mRing, channel, frame);
}

inline int IngexShm::LastFrame(unsigned int channel)
{
    int frame = 0;
    if (channel < mChannels)
    {
        frame = nexus_lastframe(mpControl, channel);
    }
    if (frame < 0)
    {
        frame = 0;
    }
    return frame;
}

inline bool IngexShm::SignalPresent(unsigned int channel)
{
    return nexus_signal_ok(mpControl, mRing, channel, LastFrame(channel));
}

inline int IngexShm::FrameNumber(unsigned int channel, unsigned int frame)
{
    return nexus_frame_number(mpControl, mRing, channel, frame);
}

inline int IngexShm::FrameRateNumerator()
{
    if (mpControl)
    {
        return mpControl->frame_rate_numer;
    }
    else
    {
        return 0;
    }
}

inline int IngexShm::FrameRateDenominator()
{
    if (mpControl)
    {
        return mpControl->frame_rate_denom;
    }
    else
    {
        return 0;
    }
}

inline unsigned int IngexShm::PrimaryWidth() { return mpControl->width; }
inline unsigned int IngexShm::PrimaryHeight() { return mpControl->height; }
inline unsigned int IngexShm::SecondaryWidth() { return mpControl->sec_width; }
inline unsigned int IngexShm::SecondaryHeight() { return mpControl->sec_height; }

inline unsigned int IngexShm::Channels() { return mChannels; }
inline unsigned int IngexShm::AudioTracksPerChannel() { return mAudioTracksPerChannel; }
inline int IngexShm::RingLength() { if (mpControl) return mpControl->ringlen; else return 0; }

inline int IngexShm::NumAudioSamples(unsigned int channel, unsigned int frame)
{
    int audio_samples_per_frame = nexus_num_aud_samp(mpControl, mRing, channel, frame);
    // Guard against garbage data
    if (audio_samples_per_frame < 0 || audio_samples_per_frame > 1920)
    {
        audio_samples_per_frame = 1920;
    }
    return audio_samples_per_frame;
}

inline void IngexShm::GetHeartbeat(struct timeval * tv)
{
    if (mpControl)
    {
        *tv = mpControl->owner_heartbeat;
    }
    else
    {
        tv->tv_sec = 0;
        tv->tv_usec = 0;
    }
}


#endif //#ifndef IngexShm_h

