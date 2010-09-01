/*
 * $Id: JogShuttleControl.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * Recorder jog-shuttle control
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
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

#include "JogShuttleControl.h"
#include "Recorder.h"
#include "Logging.h"

using namespace rec;


static const int g_reverseSpeed[7] = {-1, -2, -5, -10, -20, -50, -100}; 
static const int g_forwardSpeed[7] = {2, 5, 10, 20, 50, 75, 100}; 


static long get_elapsed_time(struct timeval* from)
{
    struct timeval now;
    gettimeofday(&now, 0);
    
    return (now.tv_sec - from->tv_sec) * 1000 + (now.tv_usec - from->tv_usec) / 1000;
}



JogShuttleControl::JogShuttleControl(Recorder* recorder)
{
    _recorder = recorder;
    _button10Pressed = false;
    _button14PressTime = (struct timeval){0, 0};
    _button15PressTime = (struct timeval){0, 0};
    
    _jogShuttle = new ingex::JogShuttle();
    _jogShuttle->addListener(this);
    _jogShuttle->start();
}

JogShuttleControl::~JogShuttleControl()
{
    _jogShuttle->removeListener(this);
    delete _jogShuttle;
}

void JogShuttleControl::connected(ingex::JogShuttle* jogShuttle, ingex::JogShuttleDevice device)
{
    Logging::info("Connected to jog-shuttle device '%s'\n", jogShuttle->getDeviceName(device).c_str());
}

void JogShuttleControl::disconnected(ingex::JogShuttle* jogShuttle, ingex::JogShuttleDevice device)
{
    Logging::warning("Disconnected from jog-shuttle device '%s'\n", jogShuttle->getDeviceName(device).c_str());
}

void JogShuttleControl::buttonPressed(ingex::JogShuttle* jogShuttle, int number)
{
    (void)jogShuttle;
    
    switch (number)
    {
        case 1:
            _recorder->forwardConfidenceReplayControl("next-osd-screen");
            break;
        case 2:
            _recorder->forwardConfidenceReplayControl("next-osd-timecode");
            break;
        case 3:
            _recorder->forwardConfidenceReplayControl("next-marks-selection");
            break;
        case 5:
            _recorder->markItemStart();
            break;
        case 6:
            _recorder->clearItem();
            break;
        case 7:
            _recorder->forwardConfidenceReplayControl("play");
            break;
        case 8:
            _recorder->forwardConfidenceReplayControl("pause");
            break;
        case 9:
            _recorder->markItemStart();
            break;
        case 10:
            _button10Pressed = true;
            break;
        case 14:
            gettimeofday(&_button14PressTime, 0);
            break;
        case 15:
            gettimeofday(&_button15PressTime, 0);
            break;
        default:
            break;
    }
}

void JogShuttleControl::buttonReleased(ingex::JogShuttle* jogShuttle, int number)
{
    (void)jogShuttle;
    
    switch (number)
    {
        case 10:
            _button10Pressed = false;
            break;
        case 14:
            if (get_elapsed_time(&_button14PressTime) >= 1000)
            {
                _recorder->forwardConfidenceReplayControl("home?pause=true");
            }
            else
            {
                _recorder->forwardConfidenceReplayControl("prev-mark");
            }
            break;
        case 15:
            if (get_elapsed_time(&_button15PressTime) >= 1000)
            {
                _recorder->forwardConfidenceReplayControl("end?pause=true");
            }
            else
            {
                _recorder->forwardConfidenceReplayControl("next-mark");
            }
            break;
        default:
            break;
    }
}

void JogShuttleControl::jog(ingex::JogShuttle* jogShuttle, bool clockwise, int position)
{
    (void)jogShuttle;
    (void)position;
    
    if (clockwise)
    {
        if (_button10Pressed)
        {
            _recorder->forwardConfidenceReplayControl("step?forward=true&unit=PERCENTAGE_PLAY_UNIT");
        }
        else
        {
            _recorder->forwardConfidenceReplayControl("step?forward=true&unit=FRAME_PLAY_UNIT");
        }
    }
    else
    {
        if (_button10Pressed)
        {
            _recorder->forwardConfidenceReplayControl("step?forward=false&unit=PERCENTAGE_PLAY_UNIT");
        }
        else
        {
            _recorder->forwardConfidenceReplayControl("step?forward=false&unit=FRAME_PLAY_UNIT");
        }
    }
}

void JogShuttleControl::shuttle(ingex::JogShuttle* jogShuttle, bool clockwise, int position)
{
    (void)jogShuttle;
    
    if (position < 0 || position - 1 >= (int)(sizeof(g_reverseSpeed) / sizeof(int)))
    {
        // invalid position
        return;
    }
    
    if (position == 0)
    {
        _recorder->forwardConfidenceReplayControl("pause");
        return;        
    }
    
    char command[64];
    if (clockwise)
    {
        sprintf(command, "play-speed?speed=%d&unit=FRAME_PLAY_UNIT", g_forwardSpeed[position - 1]);
    }
    else
    {
        sprintf(command, "play-speed?speed=%d&unit=FRAME_PLAY_UNIT", g_reverseSpeed[position - 1]);
    }
    
    _recorder->forwardConfidenceReplayControl(command);
}

void JogShuttleControl::ping(ingex::JogShuttle* jogShuttle)
{
    (void)jogShuttle;
    // ignore
}

