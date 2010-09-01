/*
 * $Id: ConfidenceReplay.h,v 1.2 2010/09/01 16:05:22 philipn Exp $
 *
 * Runs and provides access to a player running in a separate process
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
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
 
#ifndef __RECORDER_CONF_REPLAY_H__
#define __RECORDER_CONF_REPLAY_H__


#include <string>
#include <vector>

#include "Threads.h"


#define ITEM_START_MARK         0x00000100


namespace rec
{

class ConfidenceReplayStatus
{
public:
    ConfidenceReplayStatus()
    : duration(-1), position(-1), vtrErrorLevel(-1), markFilter(-1)
    {}
    
    int64_t duration;
    int64_t position;
    int vtrErrorLevel;
    unsigned int markFilter;
    std::string filename;
};


class ConfidenceReplay : public Thread
{
public:
    ConfidenceReplay(std::string filename, std::string eventFilename,
                     int httpAccessPort, int64_t startPosition, bool showSecondMarkBar);
    virtual ~ConfidenceReplay();
    
    void forwardControl(std::string command);
    
    void setMark(int64_t position, int type);
    void clearMark(int64_t position, int type);
    void seek(int64_t position);
    void play();
    void seekPositionOrPlay(int64_t position);
    
    ConfidenceReplayStatus getStatus();
    
private:
    std::string _filename;
    Thread* _stateMonitor;
    
};




};




#endif

