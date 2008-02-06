// routerloggerApp.cpp

// Copyright (c) 2006. British Broadcasting Corporation. All Rights Reserved.

#include "ace/DEV_Addr.h"
#include "ace/DEV_Connector.h"
#include "ace/DEV_IO.h"
#include "ace/TTY_IO.h"
#include "ace/OS_NS_unistd.h"
#include "ace/Get_Opt.h"
#include "ace/Log_Msg.h"


#include "CorbaUtil.h"

#include "routerloggerApp.h"
#include "SimplerouterloggerImpl.h"
#include "quartzRouter.h"

#include <string>

const char * const USAGE = "Usage: Routerlogger.exe [-r <router port>] [-t <timecode port>] [-n <name>] [-d <dest>] [-b <db file>] <CORBA options>\n"
    "    example CORBA options: -ORBDefaultInitRef corbaloc:iiop:192.168.1.1:8888\n";


// Static member
routerloggerApp * routerloggerApp::mInstance = 0;

// Constructor
routerloggerApp::routerloggerApp()
: mpServant(0)
{
}

bool routerloggerApp::Init(int argc, char * argv[])
{
// Initialise ORB
    int initial_argc = argc;
	CorbaUtil::Instance()->InitOrb(argc, argv);

// and check whether ORB options were supplied
    if (argc == initial_argc)
    {
        // No CORBA options supplied
		ACE_ERROR_RETURN((LM_ERROR, ACE_TEXT (USAGE)), 0);
    }

	// get command line args
    static const ACE_TCHAR options[] = ACE_TEXT (":t:r:n:d:b:h");
	ACE_Get_Opt cmd_opts (argc, argv, options);


    std::string routerlogger_name = "RouterLog"; // default name
    std::string router_port; // default blank, router will be discovered
    std::string tc_port;
    unsigned int dest = 1; // default destination to watch
    std::string db_file = "C:\\TEMP\\RouterLogs\\database.txt";

	int option;
	while ((option = cmd_opts ()) != EOF)
    {
		//ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("option %d\n"), option ));
		switch (option)
        {
		case 't':
            // timecode reader port
			//ACE_OS_String::strncpy (tcport,
            //            cmd_opts.opt_arg (),
            //            MAXPATHLEN);           //timecode port
            tc_port = ACE_TEXT_ALWAYS_CHAR( cmd_opts.opt_arg() );
			break;
		
		case 'r':
            // router port
			//ACE_OS_String::strncpy (rport,
            //            cmd_opts.opt_arg (),
            //            MAXPATHLEN);			
            router_port = ACE_TEXT_ALWAYS_CHAR( cmd_opts.opt_arg() );
			break;
				
		case 'n':
			//ACE_OS_String::strncpy (routerlogger_name,
            //            cmd_opts.opt_arg (),
            //            MAXPATHLEN);
            routerlogger_name = ACE_TEXT_ALWAYS_CHAR( cmd_opts.opt_arg() );
			break;

        case 'd':
            // Router destination to record
            dest = ACE_OS::atoi( cmd_opts.opt_arg() );
            break;

        case 'b':
            // cuts databse filename
            db_file = ACE_TEXT_ALWAYS_CHAR( cmd_opts.opt_arg() );
            break;
	
		case 'h':
			ACE_ERROR_RETURN
    			((LM_ERROR, ACE_TEXT (USAGE)), 0);	// help 
			break;

		case ':':
			ACE_ERROR_RETURN
				((LM_ERROR, ACE_TEXT ("-%c requires an argument\n"), cmd_opts.opt_opt()), 0);
            break;

		default:
			ACE_ERROR_RETURN
				((LM_ERROR, ACE_TEXT ("Parse Error.\n")), 0);
			//ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("other args\n") ));
            break;

		}
    }

	ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("routerlogger name is \"%C\"\n"), routerlogger_name.c_str() ));
	ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("router on   \"%C\"\n"), router_port.c_str() ));
	ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("timecode on \"%C\"\n"), tc_port.c_str() ));


// apply timeout for CORBA operations
	const int timeoutsecs = 5;
	CorbaUtil::Instance()->SetTimeout(timeoutsecs);

// activate POA manager
	CorbaUtil::Instance()->ActivatePoaMgr();

// Create the servant object.
	mpServant = new SimplerouterloggerImpl();

// and initialise
    mpServant->Name(routerlogger_name);
	bool ok = mpServant->Init(router_port, tc_port, dest, db_file);

// incarnate servant object
	mRef = mpServant->_this();

// get the NameService object reference(s)
// which were passed to Orb from command line arguments
	CorbaUtil::Instance()->InitNs();

// Set the name to be used in Naming Service
	mName.length(3);
	mName[0].id = CORBA::string_dup("ProductionAutomation");
	mName[1].id = CORBA::string_dup("RecordingDevices");
	mName[2].id = CORBA::string_dup(routerlogger_name.c_str());


// Try to advertise in naming services
	if (ok)
	{
		ok = CorbaUtil::Instance()->Advertise(mRef, mName);
	}

// other initialisation
	mTerminated = false;

	return ok; 
}

void routerloggerApp::Run()
{
    //Router quartz;
    //quartz.Init(mRouterPort);

	//if (quartz.isConnected())
    //{
	//	quartz.setObserver(mpServant);

	//	int result = quartz.activate(); // start router watching thread
	//	ACE_ASSERT (result == 0);

		// Accept CORBA requests until told to stop
		ACE_Time_Value timeout(1,0);  // seconds, microseconds
		while (!mTerminated)
		{
			CorbaUtil::Instance()->OrbRun(timeout);
		}
	//}// exit if router not connected
}

void routerloggerApp::Stop()
{
	// Cause Run loop to exit
	mTerminated = true;
}

void routerloggerApp::Clean()
{
// remove from naming services
	CorbaUtil::Instance()->Unadvertise(mName);
// Deactivate the CORBA object and
// relinquish our reference to the servant.
// The POA will delete it at an appropriate time.
    if (mpServant)
    {
	    mpServant->Destroy();
    }
}

