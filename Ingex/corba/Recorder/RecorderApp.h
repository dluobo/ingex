/*
 * $Id: RecorderApp.h,v 1.1 2006/12/20 12:28:26 john_f Exp $
 *
 * Encapsulation of the recorder application.
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

#ifndef RecorderApp_h
#define RecorderApp_h

#include <orbsvcs/CosNamingC.h>

#include "App.h"

#include "IngexRecorderImpl.h"

class RecorderApp : public App
{
public:
// singleton access
    static RecorderApp * Instance()
    {
        if(0 == mInstance)
        {
            mInstance = new RecorderApp();
        }
        return mInstance;
    }
// singleton destroy
    void Destroy() { mInstance = 0; delete this; }

// control methods
    bool Init(int argc, char * argv[]);
    void Run();
    void Stop();
    void Clean();

protected:
// constructor protected as this is a singleton
    RecorderApp() {};

private:
// singleton instance pointer
    static RecorderApp * mInstance;

// CORBA servant
    IngexRecorderImpl * mpServant;
    ProdAuto::Recorder_var mRef;
    CosNaming::Name mName;
    bool mIsAdvertised;

// flag for terminating main loop
    bool mTerminated;
};

#endif //#ifndef RecorderApp_h
