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

const char * const USAGE =
    "Usage: Routerlogger.exe [-r <router port>] [-s] [-t <timecode port>] [-u]"
    " [-n <name>] [-f <db file>]"
    " [-a <VT1 dest>]  [-b <VT2 dest>] [-c <VT1 dest>]  [-d <VT2 dest>] [-m <MixerOut dest>"
    " <CORBA options>\n"
    "    -s  router on TCP socket, router port in format host:port\n"
    "    -u  timecode reader on TCP socket, timecode port in format host:port\n"
    "    example CORBA options: -ORBDefaultInitRef corbaloc:iiop:192.168.1.1:8888\n";

const char * const OPTS = "r:st:un:f:a:b:c:d:m:";


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
    ACE_Get_Opt cmd_opts (argc, argv, OPTS);


    std::string routerlogger_name = "RouterLog"; // default name
    std::string router_port; // default blank, router will be discovered
    bool router_tcp = false;
    std::string tc_port;
    bool tc_tcp = false;
    unsigned int mix_dest = 1; // default destination to record
    unsigned int vt1_dest = 1;
    unsigned int vt2_dest = 2;
    unsigned int vt3_dest = 3;
    unsigned int vt4_dest = 4;
#ifdef WIN32
    std::string db_file = "C:\\TEMP\\RouterLogs\\database.txt";
#else
    std::string db_file = "/var/tmp/routerlogdb.txt";
#endif

	int option;
	while ((option = cmd_opts ()) != EOF)
    {
		//ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("option %d\n"), option ));
		switch (option)
        {
		case 'r':
            // router port
            router_port = ACE_TEXT_ALWAYS_CHAR( cmd_opts.opt_arg() );
			break;
				
        case 's':
            router_tcp = true;
            break;
		
		case 't':
            // timecode reader port
            tc_port = ACE_TEXT_ALWAYS_CHAR( cmd_opts.opt_arg() );
			break;

        case 'u':
            tc_tcp = true;
            break;
		
		case 'n':
          routerlogger_name = ACE_TEXT_ALWAYS_CHAR( cmd_opts.opt_arg() );
			break;

        case 'f':
            // cuts databse filename
            db_file = ACE_TEXT_ALWAYS_CHAR( cmd_opts.opt_arg() );
            break;
	
        case 'a':
            // Router destination for VT1
            vt1_dest = ACE_OS::atoi( cmd_opts.opt_arg() );
            break;

        case 'b':
            // Router destination for VT2
            vt2_dest = ACE_OS::atoi( cmd_opts.opt_arg() );
            break;

        case 'c':
            // Router destination for VT3
            vt3_dest = ACE_OS::atoi( cmd_opts.opt_arg() );
            break;

        case 'd':
            // Router destination for VT4
            vt4_dest = ACE_OS::atoi( cmd_opts.opt_arg() );
            break;

        case 'm':
            // Router destination to record
            mix_dest = ACE_OS::atoi( cmd_opts.opt_arg() );
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
    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("router on   \"%C\"%C\n"), router_port.c_str(), (router_tcp ? " (TCP)" : "") ));
	ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("timecode on \"%C\"%C\n"), tc_port.c_str(), (tc_tcp ? " (TCP)" : "") ));


// apply timeout for CORBA operations
	const int timeoutsecs = 5;
	CorbaUtil::Instance()->SetTimeout(timeoutsecs);

// activate POA manager
	CorbaUtil::Instance()->ActivatePoaMgr();

// Create the servant object.
	mpServant = new SimplerouterloggerImpl();

// and initialise
    mpServant->Name(routerlogger_name);
	bool ok = mpServant->Init(router_port, router_tcp, tc_port, tc_tcp, db_file, mix_dest, vt1_dest, vt2_dest, vt3_dest, vt4_dest);

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

