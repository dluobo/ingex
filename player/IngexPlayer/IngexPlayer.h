/*
 * $Id: IngexPlayer.h,v 1.7 2011/04/19 10:15:18 philipn Exp $
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

#ifndef __PRODAUTO_INGEX_PLAYER_H__
#define __PRODAUTO_INGEX_PLAYER_H__


#include <string>
#include <vector>

#include <media_player.h>

#include "IngexPlayerListener.h"
#include "IngexPlayerException.h"


namespace prodauto
{

typedef enum
{
    UNKNOWN_OUTPUT = 0,
    /* playout to SDI via DVS card */
    DVS_OUTPUT,
    /* auto-detect whether X11 X video extension is available, if not use plain X11 */
    X11_AUTO_OUTPUT,
    /* playout to X11 window using X video extension*/
    X11_XV_OUTPUT,
    /* playout to X11 window */
    X11_OUTPUT,
    /* playout to both SDI and X11 window (auto-detect availability of Xv) */
    DUAL_DVS_AUTO_OUTPUT,
    /* playout to both SDI and X11 window */
    DUAL_DVS_X11_OUTPUT,
    /* playout to both SDI and X11 X video extension window */
    DUAL_DVS_X11_XV_OUTPUT
} PlayerOutputType;

typedef enum
{
    MXF_INPUT = 1,
    RAW_INPUT,
    DV_INPUT,
    FFMPEG_INPUT,
#ifndef DISABLE_SHARED_MEM_SOURCE
    SHM_INPUT,
#endif
    UDP_INPUT,
    BALLS_INPUT,
    BLANK_INPUT,
    CLAPPER_INPUT
} PlayerInputType;

typedef struct
{
    prodauto::PlayerInputType type;
    std::string name;
    std::map<std::string, std::string> options;
} PlayerInput;

class IngexPlayer
{
public:

    virtual ~IngexPlayer() {};


    /* X11 display */

    virtual bool setX11WindowName(std::string name) = 0;


    /* stop the player and free resources */

    virtual bool stop() = 0;


    /* toggle locking out player control functions below, eg. to prevent accidents during playback */

    virtual bool toggleLock() = 0;


    /* audio/video stream control */

    virtual bool play() = 0;
    virtual bool pause() = 0;
    virtual bool togglePlayPause() = 0;
    /* whence = SEEK_SET, SEEK_CUR or SEEK_END */
    /* offset is in 1/1000% if unit equals PERCENTAGE_PLAY_UNIT,
    ie. multiple a percentage position by 1000 to get offset */
    virtual bool seek(int64_t offset, int whence, PlayUnit unit) = 0;
    virtual bool playSpeed(int speed) = 0; /* -1 <= speed >=1 */
    virtual bool step(bool forward) = 0;
    virtual bool muteAudio(int mute /* -1=toggle, 0=not mute, 1=mute */) = 0;


    /* mark-in and mark-out controls */

    /* TODO: add toggle parameters */
    virtual bool mark(int type) = 0;
    virtual bool markPosition(int64_t position, int type) = 0;
    virtual bool clearMark() = 0;
    virtual bool clearAllMarks() = 0;
    virtual bool seekNextMark() = 0;
    virtual bool seekPrevMark() = 0;


    /* on screen display */

    virtual bool setOSDScreen(OSDScreen screen) = 0;
    virtual bool nextOSDScreen() = 0;
    virtual bool setOSDTimecode(int index, int type, int subType) = 0;
    virtual bool nextOSDTimecode() = 0;
    virtual bool setOSDPlayStatePosition(OSDPlayStatePosition position) = 0;


    /* video switch control */

    virtual bool switchNextVideo() = 0;
    virtual bool switchPrevVideo() = 0;
    virtual bool switchVideo(int index) = 0;


    /* audio switch control */

    virtual bool switchNextAudioGroup() = 0;
    virtual bool switchPrevAudioGroup() = 0;
    virtual bool switchAudioGroup(int index) = 0; /* index == 0 is equiv. to snapAudioToVideo */
    virtual bool snapAudioToVideo(int enable) = 0; /* -1=toggle, 0=disable, 1=enable*/


    /* review */

    virtual bool reviewStart(int64_t duration) = 0;
    virtual bool reviewEnd(int64_t duration) = 0;
    virtual bool review(int64_t duration) = 0;

};


};



#endif
