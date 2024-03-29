/*
 * $Id: routerloggerApp.cpp,v 1.14 2011/08/19 10:11:12 john_f Exp $
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

#include <ace/Get_Opt.h>
#include <ace/Log_Msg.h>

#include "CorbaUtil.h"
#include "Logfile.h"
#include "DateTime.h"
#include "routerloggerApp.h"
#include "RouterRecorderImpl.h"
#include "quartzRouter.h"
#include "EasyReader.h"
#include "ClockReader.h"
#include "ShmTimecodeReader.h"
#include "Timecode.h"
#include "DatabaseManager.h"
#include "DBException.h"

#include <string>
#include <sstream>

const char * const USAGE =
    "Usage: Routerlogger [-v] \\\n"
    "  [--dbhost <db_host>] [--dbname <db_name>] [--dbuser <db_user>] [--dbpass <db_password>] \\\n"
    "  [-r <router port>] [-s] \\\n"
    "  [[-t <timecode port>] [-u] | [-l]] \\\n"
    "  [-n <name>] [-c <mc_clip_def_name>] [-f <db file>] \\\n"
    "  [-d <name> -p <number>] [-m <MixerOut dest>] \\\n"
    "  [-a <nameserver>] \\\n"
    "  [-o <timecode adjustment in frames>] \\\n"
    "  <CORBA options>\n"
    "Info:\n"
    "  -s  router on TCP socket, router port in format host:port \\\n"
    "  -u  timecode reader on TCP socket, timecode port in format host:port \\\n"
    "  -l  take timecode from shared memory \\\n"
    "Example CORBA options:\n"
    "  -ORBDefaultInitRef corbaloc:iiop:192.168.1.1:8888\n"
    "Example nameserver:\n"
    "  -a corbaloc:iiop:192.168.1.1:8888/NameService\n"
    "\n";

namespace
{
void exit_usage()
{
    fprintf(stderr, USAGE);
    exit(0);
}
}



// Static member
routerloggerApp * routerloggerApp::mInstance = 0;

// Constructor
routerloggerApp::routerloggerApp()
: mpRouter(0), mTcAdjust(0)
{
}

routerloggerApp::~routerloggerApp()
{
    if (mpRouter)
    {
        mpRouter->Stop();
        mpRouter->wait();
        delete mpRouter;
    }
}

bool routerloggerApp::Init(int argc, char * argv[])
{
#if 1
// Set logging to file
	Logfile::Init();
	Logfile::AddPathComponent("var");
	Logfile::AddPathComponent("tmp");
	Logfile::AddPathComponent("IngexLogs");
	std::string dir = "RouterRecorder_";
	dir += DateTime::DateTimeNoSeparators();

    Logfile::AddPathComponent(dir.c_str());
	Logfile::Open("Main", false);
	ACE_DEBUG(( LM_NOTICE, ACE_TEXT("Main thread\n\n") ));
#endif

    bool ok = true;

    // Initialise ORB
    int initial_argc = argc;
    CorbaUtil::Instance()->InitOrb(argc, argv);

    // and check whether ORB options were supplied
    if (argc == initial_argc)
    {
        // No CORBA options supplied
        ACE_ERROR_RETURN((LM_ERROR, ACE_TEXT (USAGE)), 0);
    }


    std::string db_file; // We'll add a default name later if none supplied

    std::string router_port;
    Transport::EnumType  router_transport = Transport::SERIAL;
    std::string tc_port;
    Transport::EnumType  tc_transport = Transport::SERIAL;
    bool tc_from_shm = false;

    unsigned int debug_level = 2; // need level 3 for LM_DEBUG messages

    // Database parameter defaults
    std::string db_host = "localhost";
    std::string db_name = "prodautodb";
    std::string db_username;
    std::string db_password;

// Now that InitOrb has consumed its arguments, check for
// other command line arguments.
    for (int argindex = 1; argindex < argc; ++argindex)
    {
        if (strcmp(argv[argindex], "-h") == 0 ||
            strcmp(argv[argindex], "--help") == 0)
        {
            exit_usage();
        }
        else if (strcmp(argv[argindex], "-v") == 0 ||
            strcmp(argv[argindex], "--verbose") == 0)
        {
            ++debug_level;
        }
        else if (strcmp(argv[argindex], "-r") == 0)
        {
            // router port
            if (++argindex < argc)
            {
                router_port = ACE_TEXT_ALWAYS_CHAR( argv[argindex] );
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "-s") == 0)
        {
            router_transport = Transport::TCP;
        }
        else if (strcmp(argv[argindex], "-t") == 0)
        {
            // timecode reader port
            tc_port = ACE_TEXT_ALWAYS_CHAR( argv[argindex] );
        }
        else if (strcmp(argv[argindex], "-u") == 0)
        {
            tc_transport = Transport::TCP;
        }
        else if (strcmp(argv[argindex], "-l") == 0)
        {
            tc_from_shm = true;
        }
        else if (strcmp(argv[argindex], "-n") == 0)
        {
            if (++argindex < argc)
            {
                ServantInfo * servant_info = new ServantInfo();
                mServantInfo.push_back(servant_info);
                mServantInfo.back()->name = ACE_TEXT_ALWAYS_CHAR( argv[argindex] );
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "-c") == 0)
        {
            // multi-cam clip def name
            if (++argindex < argc && !mServantInfo.empty())
            {
                mServantInfo.back()->mc_clip_name = ACE_TEXT_ALWAYS_CHAR( argv[argindex] );
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "-f") == 0)
        {
            // cuts database filename
            if (++argindex < argc && !mServantInfo.empty())
            {
                mServantInfo.back()->db_file = ACE_TEXT_ALWAYS_CHAR( argv[argindex] );
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "-a") == 0)
        {
            // nameserver
            if (++argindex < argc && !mServantInfo.empty())
            {
                mServantInfo.back()->name_server = argv[argindex];
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "-d") == 0)
        {
            // Router destination name
            if (++argindex < argc && !mServantInfo.empty())
            {
                RouterDestination * rd = new RouterDestination(argv[argindex], 0);
                mServantInfo.back()->destinations.push_back(rd);
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "-p") == 0)
        {
            // Router destination number
            if (++argindex < argc && !mServantInfo.empty() && !mServantInfo.back()->destinations.empty())
            {
                mServantInfo.back()->destinations.back()->output_number = ACE_OS::atoi(argv[argindex]);
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "-m") == 0)
        {
            // Router destination to record
            if (++argindex < argc && !mServantInfo.empty())
            {
                mServantInfo.back()->mix_dest = ACE_OS::atoi(argv[argindex]);
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "-o") == 0)
        {
            // Timecode offset
            if (++argindex < argc)
            {
                mTcAdjust = ACE_OS::atoi(argv[argindex]);
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "--dbhost") == 0)
        {
            if (++argindex < argc)
            {
                db_host = argv[argindex];
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "--dbname") == 0)
        {
            if (++argindex < argc)
            {
                db_name = argv[argindex];
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "--dbuser") == 0)
        {
            if (++argindex < argc)
            {
                db_username = argv[argindex];
            }
            else
            {
                exit_usage();
            }
        }
        else if (strcmp(argv[argindex], "--dbpass") == 0)
        {
            if (++argindex < argc)
            {
                db_password = argv[argindex];
            }
            else
            {
                exit_usage();
            }
        }
    }

// Set verbosity of debug messages
    Logfile::DebugLevel(debug_level);

// Log startup details
    ACE_DEBUG((LM_INFO, ACE_TEXT ("RouterRecorder started %C\n\n"), DateTime::DateTimeWithSeparators().c_str()));

    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("router on   \"%C\"\n"), router_port.c_str() ));
    if (tc_from_shm)
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("timecode from shared memory\n")));
    }
    else
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("timecode on \"%C\"\n"), tc_port.c_str() ));
    }

// apply timeout for CORBA operations
    const int timeoutsecs = 5;
    CorbaUtil::Instance()->SetTimeout(timeoutsecs);

// activate POA manager
    CorbaUtil::Instance()->ActivatePoaMgr();

// Create router monitor
    mpRouter = new Router;
    mpRouter->Init(router_port, router_transport);

// Create timecode reader
    if (tc_from_shm)
    {
        // Timecode from Ingex capture buffer
        mpTcReader = new ShmTimecodeReader;
    }
    else if (tc_port.empty())
    {
        // Use PC clock
        mpTcReader = new ClockReader;
    }
    else
    {
        // Use external timecode reader
        mpTcReader = new EasyReader;
    }

    // Specific initialisation for EasyReader
    EasyReader * er = dynamic_cast<EasyReader *>(mpTcReader);
    if (er)
    {
        if (er->Init(tc_port, tc_transport))
        {
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("EasyReader initialised on port \"%C\"\n"),
                tc_port.c_str()));
        }
        else
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("EasyReader initialisation failed on port \"%C\"\n"),
                tc_port.c_str()));
            ok = false;
        }
    }

    // Specific initialisation for ShmTimecodeReader
    ShmTimecodeReader * sr = dynamic_cast<ShmTimecodeReader *>(mpTcReader);
    if (sr)
    {
        //sr->TcMode(tc_mode);
    }

    // Initialise database connection
    try
    {
        DatabaseManager::Instance()->Initialise(db_host, db_name, db_username, db_password, 4, 12);
    }
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database init failed: %C\n"), dbe.getMessage().c_str()));
    }
    catch (...)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database init failed!\n")));
    }

    // If no NameService args passed, try to get one from
    // general CORBA arguments.
    if (mServantInfo.size() && mServantInfo[0]->name_server.empty())
    {
        // Get the NameService object reference(s)
        // which were passed to Orb from command line arguments
        CorbaUtil::Instance()->InitNs();
    }


    // Set up the servant(s)
    for (std::vector<ServantInfo *>::iterator
        it = mServantInfo.begin(); it != mServantInfo.end(); ++it)
    {
        ServantInfo * servant_info = *it;

        // Create object
        servant_info->servant = new RouterRecorderImpl();

        // Initialise
        if (servant_info->servant->Init(servant_info->name, servant_info->mc_clip_name, servant_info->db_file,
                                        servant_info->mix_dest, servant_info->destinations))
        {
            // Incarnate servant object
            servant_info->ref = servant_info->servant->_this();

            // Set the name to be used in Naming Service
            servant_info->cosname.length(3);
            servant_info->cosname[0].id = CORBA::string_dup("ProductionAutomation");
            servant_info->cosname[1].id = CORBA::string_dup("RecordingDevices");
            servant_info->cosname[2].id = CORBA::string_dup(servant_info->name.c_str());

            // Try to advertise in naming services
            if (!servant_info->name_server.empty())
            {
                CorbaUtil::Instance()->ClearNameServices();
                CorbaUtil::Instance()->AddNameService(servant_info->name_server.c_str());
            }
            CorbaUtil::Instance()->Advertise(servant_info->ref, servant_info->cosname);
        }
        else
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Init of %C failed!\n"),
                servant_info->name.c_str()));
        }
    }

// other initialisation
    mTerminated = false;

    return ok; 
}

void routerloggerApp::Run()
{
    if (mpRouter->Connected())
    {
        // Start monitoring messages from router
        for (std::vector<ServantInfo *>::iterator
            it = mServantInfo.begin(); it != mServantInfo.end(); ++it)
        {
            ServantInfo * servant_info = *it;
            mpRouter->AddObserver(servant_info->servant);
        }
        mpRouter->activate();

        for (std::vector<ServantInfo *>::iterator
            it = mServantInfo.begin(); it != mServantInfo.end(); ++it)
        {
            ServantInfo * svi = *it;
            // Query routing to destinations we're interested in
            for (std::vector<RouterDestination *>::const_iterator
                it = svi->destinations.begin(); it != svi->destinations.end(); ++it)
            {
                mpRouter->QuerySrc((*it)->output_number);
            }
            mpRouter->QuerySrc(svi->mix_dest);
        }
    }

    // Accept CORBA requests until told to stop
    ACE_Time_Value timeout(1,0);  // seconds, microseconds
    while (!mTerminated)
    {
        CorbaUtil::Instance()->OrbRun(timeout);
    }
}

void routerloggerApp::Stop()
{
    // Cause Run loop to exit
    mTerminated = true;
}

void routerloggerApp::Clean()
{
    for (std::vector<ServantInfo *>::iterator
        it = mServantInfo.begin(); it != mServantInfo.end(); ++it)
    {
        ServantInfo * servant_info = *it;

        // remove from naming services
        if (!servant_info->name_server.empty())
        {
            CorbaUtil::Instance()->ClearNameServices();
            CorbaUtil::Instance()->AddNameService(servant_info->name_server.c_str());
        }
        CorbaUtil::Instance()->Unadvertise(servant_info->cosname);

        // Deactivate the CORBA object and
        // relinquish our reference to the servant.
        // The POA will delete it at an appropriate time.
        if (servant_info->servant)
        {
            servant_info->servant->Destroy();
        }

        delete servant_info;
    }
}

std::string routerloggerApp::Timecode()
{
    Ingex::Timecode tc(mpTcReader->Timecode().c_str(), pa_EDIT_RATE.numerator, pa_EDIT_RATE.denominator, DROP_FRAME);
    tc += mTcAdjust;
    return std::string(tc.Text());
}


