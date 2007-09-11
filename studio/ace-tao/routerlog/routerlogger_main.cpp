// routerlogger_main.cpp

// Copyright (c) 2006. British Broadcasting Corporation. All Rights Reserved.

#include <ace/Signal.h>
#include <iostream>

#include "routerloggerApp.h"

extern "C" void handler (int)
{
	routerloggerApp::Instance()->Stop();
}

int main(int argc, char * argv[])
{
	// Setup handler for ctrl-c
	ACE_Sig_Action sa ((ACE_SignalHandler) handler, SIGINT);

	// Get instance of app
	App * app = routerloggerApp::Instance();

	// Run the app
	if(	app->Init(argc, argv) )
	{
		std::cerr << "routerlogger Running (stop with ctrl-C)...\n";
		app->Run();
		std::cerr << "routerlogger Stopped\n";
	}
	app->Clean();

	return 0;
}
