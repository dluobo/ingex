// routerloggerApp.h

// Copyright (c) 2006. British Broadcasting Corporation. All Rights Reserved.

#ifndef routerloggerApp_h
#define routerloggerApp_h

#include <orbsvcs/CosNamingC.h>

#include "App.h"

#include "SimplerouterloggerImpl.h"
#include "RecorderC.h"

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

// control methods
	bool Init(int argc, char * argv[]);
	void Run();
	void Stop();
	void Clean();

protected:
// constructor protected as this is a singleton
	routerloggerApp();

private:
// singleton instance pointer
	static routerloggerApp * mInstance;

// com port connected to router
    //std::string mRouterPort;

// CORBA servant
	SimplerouterloggerImpl * mpServant;
	ProdAuto::Recorder_var mRef;
	CosNaming::Name mName;

// flag for terminating main loop
	bool mTerminated;
};

#endif //#ifndef routerloggerApp_h
