/*
 * $Id: LocalIngexPlayer.h,v 1.23 2011/11/10 10:56:18 philipn Exp $
 *
 * Copyright (C) 2008-2010 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
 * Modifications: Matthew Marks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __PRODAUTO_LOCAL_INGEX_PLAYER_H__
#define __PRODAUTO_LOCAL_INGEX_PLAYER_H__


#include <pthread.h>

#include <map>

#include "IngexPlayer.h"



namespace prodauto
{


class LocalIngexPlayerState;

class LocalIngexPlayerListenerData;

/* options for each PlayerInputType are as follows:

    MXF_INPUT:
        "fallback_blank": "true", "false" (default "false")

    RAW_INPUT:
        "stream_type": "picture", "sound", "timecode" or "event"  (default "picture")
        "stream_format": "uyvy", "yuv422", "yuv420", "yuv411", "pcm", "timecode"  (default "uyvy")
        "frame_rate": "<numerator>/<denominator>"  (default 25/1)
        "fallback_blank": "true", "false" (default "false")
      video only:
        "width": <image width> (default 720)
        "height": <image height> (default 576)
        "aspect_ratio": "<numerator>/<denominator>" (default 4/3)
      audio only:
        "sampling rate": "<numerator>/<denominator>" (default 48000/1)
        "num_channels": <num>  (default 1)
        "bits_per_sample": <bps>  (default 16)

    DV_INPUT:
        "fallback_blank": "true", "false" (default "false")

    FFMPEG_INPUT:
        "num_ffmpeg_threads" <num> (default 0)
        "fallback_blank": "true", "false" (default "false")

    SHM_INPUT:
        "fallback_blank": "true", "false" (default "false")

    UDP_INPUT:
        "fallback_blank": "true", "false" (default "false")

    BBALLS_INPUT:
        same as RAW_INPUT
        "num_balls": <num> (default 5)

    BLANK_INPUT:
        same as RAW_INPUT

    CLAPPER_INPUT:
        same as RAW_INPUT, but video only
*/


/* IngexPlayer that runs locally on the machine */
class LocalIngexPlayer : public IngexPlayer
{
public:
    LocalIngexPlayer(IngexPlayerListenerRegistry* listenerRegistry);
    virtual ~LocalIngexPlayer();

    std::string getVersion();
    std::string getBuildTimestamp();


    /* returns true if a DVS card is available for output */
    bool dvsCardIsAvailable();
    bool dvsCardIsAvailable(int card, int channel);

    /* returns true if an X11 XV output is available */
    bool x11XVIsAvailable();


    /* configuration - call these methods before calling start() */
    void setWindowInfo(const X11WindowInfo* windowInfo); /* default: not set */
    void setInitiallyLocked(bool locked); /* default: false */
    void setNumFFMPEGThreads(int num); /* default: 4 */
    void setUseWorkerThreads(bool enable); /* default: true */
    void setApplyScaleFilter(bool enable); /* default: true */
    void setSourceBufferSize(int size); /* default: 0 */
    void setEnableX11OSD(bool enable); /* default: true */
    void setSourceAspectRatio(Rational *aspect); /* default: 0 */
    void setPixelAspectRatio(Rational *aspect); /* default: 1/1 */
    void setMonitorAspectRatio(Rational *aspect); /* default: 4/3 */
    void setEnablePCAudio(bool enable); /* default: true */
    void setAudioDevice(int device); /* default: 0 */
    void setNumAudioLevelMonitors(int num); /* default: 2 */
    void setAudioLineupLevel(float level); /* default: -18.0 */
    void setEnableAudioSwitch(bool enable); /* default: true */
    void setOutputType(PlayerOutputType outputType); /* default: X11_AUTO_OUTPUT */
    void setScale(float scale); /* default: 1.0 */
    void setDVSTarget(int card, int channel); /* default: -1, -1 */
    void setVideoSplit(VideoSwitchSplit videoSplit); /* default: QUAD_SPLIT_VIDEO_SWITCH */
    void setSDIOSDEnable(bool enable); /* default: true */
    void setUseDisplayDimensions(bool enable); /* default: false */


    /* will reset the player and display blank video on the output - returns false if a reset fails and
       the player will be closed */
    bool reset();

    /* will stop the player and close the X11 window */
    bool close();


    /* prepares files/sources and start playing.
       inputs:         input files / sources
       opened:         indicates for each input whether it was successfully opened or not
       startPaused:    if true the player will start in paused state
       startPosition:  first frame to play. A negative value is equivalent to seeking to
                       (last frame + (startPosition + 1)), i.e. startPosition = -1 means the player seeks to
                       the last frame
    */
    virtual bool start(std::vector<PlayerInput> inputs, std::vector<bool>& opened, bool startPaused, int64_t startPosition);

    // same as start above, except it allows setting player settings after calling prepare() and before start()
    // e.g. start above calls seek() after prepare() to seek to startPosition 
    bool prepare(std::vector<PlayerInput> inputs, std::vector<bool>& opened);
    void start(bool startPaused);


    /* returns the output type used */
    PlayerOutputType getOutputType();
    /* same as getOutputType(), except it returns the actual output type if an auto type is used */
    /* eg. if X11_AUTO_OUTPUT is set then returns either X11_XV_OUTPUT or X11_OUTPUT */
    PlayerOutputType getActualOutputType();


    /* functions inherited from IngexPlayer */
    virtual bool setX11WindowName(std::string name);
    virtual bool fitX11Window();
    virtual bool stop();
    virtual bool toggleLock();
    virtual bool play();
    virtual bool pause();
    virtual bool togglePlayPause();
    virtual bool seek(int64_t offset, int whence, PlayUnit unit);
    virtual bool playSpeed(int speed);
    virtual bool step(bool forward);
    virtual bool muteAudio(int mute);
    virtual bool mark(int type);
    virtual bool markPosition(int64_t position, int type);
    virtual bool clearMark();
    virtual bool clearAllMarks();
    virtual bool seekNextMark();
    virtual bool seekPrevMark();
    virtual bool setOSDScreen(OSDScreen screen);
    virtual bool nextOSDScreen();
    virtual bool setOSDTimecode(int index, int type, int subType);
    virtual bool setOSDTimecodeDefaultSHM();
    virtual bool showProgressBar(bool show);
    virtual bool nextOSDTimecode();
    virtual bool setOSDPlayStatePosition(OSDPlayStatePosition position);
    virtual bool switchNextVideo();
    virtual bool switchPrevVideo();
    virtual bool switchVideo(int index);
    virtual bool switchNextAudioGroup();
    virtual bool switchPrevAudioGroup();
    virtual bool switchAudioGroup(int index); /* index == 0 is equiv. to snapAudioToVideo */
    virtual bool snapAudioToVideo(int enable); /* -1=toggle, 0=disable, 1=enable*/
    virtual bool reviewStart(int64_t duration);
    virtual bool reviewEnd(int64_t duration);
    virtual bool review(int64_t duration);


private:
    typedef struct
    {
        X11WindowInfo externalWindowInfo;
        PlayerOutputType outputType;
        int dvsCard;
        int dvsChannel;
        VideoSwitchSplit videoSplit;
        int numFFMPEGThreads;
        bool initiallyLocked;
        bool useWorkerThreads;
        bool applyScaleFilter;
        int srcBufferSize;
        bool disableSDIOSD;
        bool disableX11OSD;
        Rational sourceAspectRatio;
        Rational pixelAspectRatio;
        Rational monitorAspectRatio;
        float scale;
        bool disablePCAudio;
        int audioDevice;
        int numAudioLevelMonitors;
        float audioLineupLevel;
        bool enableAudioSwitch;
        bool useDisplayDimensions;
    } Configuration;

private:
    bool setOrCreateX11Window(const X11WindowInfo* externalWindowInfo);
    void closeLocalX11Window();

    bool createFallbackBlankSource(MediaSource** mediaSource);

private:
    pthread_mutex_t _configMutex;
    Configuration _config;
    Configuration _nextConfig;

    PlayerOutputType _actualOutputType;

    TimecodeType _shmDefaultTCType;
    TimecodeSubType _shmDefaultTCSubType;

    X11WindowInfo _windowInfo;

    std::string _x11WindowName;

    pthread_rwlock_t _playStateRWLock;
    LocalIngexPlayerState* _playState;

    MediaPlayerListener _mediaPlayerListener;
    X11WindowListener _x11WindowListener;
    KeyboardInputListener _x11KeyListener;
    ProgressBarInputListener _x11ProgressBarListener;
    MouseInputListener _x11MouseListener;

    StreamInfo _videoStreamInfo;

    OSDPlayStatePosition _osdPlayStatePosition;

public:
    // for listener callbacks
    void updateListenerData(LocalIngexPlayerListenerData* listenerData);
    IngexPlayerListenerRegistry* _listenerRegistry;

private:
    int _sourceIdToIndexVersion;
    std::map<int, int> _sourceIdToIndex;
};


};



#endif
