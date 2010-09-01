/*
 * $Id: JogShuttleControl.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#ifndef __RECORDER_JOG_SHUTTLE_CONTROL_H__
#define __RECORDER_JOG_SHUTTLE_CONTROL_H__

#include <sys/time.h>

#include <JogShuttle.h>


namespace rec
{

class Recorder;

class JogShuttleControl : protected ingex::JogShuttleListener
{
public:
    JogShuttleControl(Recorder* recorder);
    virtual ~JogShuttleControl();
    
protected:
    // from JogShuttleListener
    virtual void connected(ingex::JogShuttle* jogShuttle, ingex::JogShuttleDevice device);
    virtual void disconnected(ingex::JogShuttle* jogShuttle, ingex::JogShuttleDevice device);
    virtual void buttonPressed(ingex::JogShuttle* jogShuttle, int number);
    virtual void buttonReleased(ingex::JogShuttle* jogShuttle, int number);
    virtual void jog(ingex::JogShuttle* jogShuttle, bool clockwise, int position);
    virtual void shuttle(ingex::JogShuttle* jogShuttle, bool clockwise, int position);
    virtual void ping(ingex::JogShuttle* jogShuttle);

private:
    Recorder* _recorder;
    ingex::JogShuttle* _jogShuttle;

    bool _button10Pressed;
    struct timeval _button14PressTime;
    struct timeval _button15PressTime;
};


};



#endif

