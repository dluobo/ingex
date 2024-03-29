/*
 * $Id: routerloggerApp.h,v 1.8 2010/07/14 13:06:36 john_f Exp $
 *
 * Router recorder application class.
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

#ifndef routerloggerApp_h
#define routerloggerApp_h

#include "App.h"

#include "RouterRecorderImpl.h"
#include "RecorderC.h"

#include <ace/Thread_Mutex.h>
#include <string>
#include <vector>

class TimecodeReader;
class Router;

// Details of a CORBA servant
class ServantInfo
{
public:
    ServantInfo() : servant(0) {}

    std::string name;
    std::string mc_clip_name;
    unsigned int mix_dest;
    std::vector<RouterDestination *> destinations;
    std::string db_file;
    std::string name_server;
    CosNaming::Name cosname;
	RouterRecorderImpl * servant;
	ProdAuto::Recorder_var ref;
};

class routerloggerApp : public App
{
public:
// singleton access
	static routerloggerApp * Instance()
	{
		if (0 == mInstance)
		{
			mInstance = new routerloggerApp();
		}
		return mInstance;
	}
// singleton destroy
	void Destroy() { mInstance = 0; delete this; }

// destructor
	~routerloggerApp();

// control methods
	bool Init(int argc, char * argv[]);
	void Run();
	void Stop();
	void Clean();

// utility methods
    std::string Timecode();

protected:
// constructor protected as this is a singleton
	routerloggerApp();

private:
// singleton instance pointer
	static routerloggerApp * mInstance;

    // Router monitor
    Router * mpRouter;

    // Timecode reader
    TimecodeReader * mpTcReader;

    // Timecode adjustment
    int mTcAdjust;

    // CORBA servants
    std::vector<ServantInfo *> mServantInfo;
	//RouterRecorderImpl * mpServant;
	//ProdAuto::Recorder_var mRef;
	//CosNaming::Name mName;

// flag for terminating main loop
	bool mTerminated;

};

#endif //#ifndef routerloggerApp_h

