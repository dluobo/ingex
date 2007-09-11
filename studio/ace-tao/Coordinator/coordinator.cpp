// coordinator.cpp

//#include "ace/OS_NS_unistd.h"
//#include "ace/Log_Msg.h"
//#include "ace/Process_Manager.h"
#include <ace/Signal.h>
#include <iostream>

#include "CoordinatorApp.h"

extern "C" void handler (int)
{
	CoordinatorApp::Instance()->Stop();
}

int ACE_TMAIN (int argc, ACE_TCHAR *argv[])
{
	// Setup handler for ctrl-c
	ACE_Sig_Action sa ((ACE_SignalHandler) handler, SIGINT);

	// Get instance of app
	CoordinatorApp * app = CoordinatorApp::Instance();

	// Run the app
	if(	app->Init(argc, argv) )
	{
		std::cerr << "Coordinator Running...\n";
		app->Run();
		std::cerr << "Coordinator Stopped\n";
	}
	app->Clean();

	return 0;
}
