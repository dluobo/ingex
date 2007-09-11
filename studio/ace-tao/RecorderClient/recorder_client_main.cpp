/*
 * $Id: recorder_client_main.cpp,v 1.1 2007/09/11 14:08:31 stuart_hc Exp $
 *
 * Simple test client for Recorder.
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

#include <iostream>

#include <ace/Log_Msg.h>

#include <CorbaUtil.h>
#include <tao/ORB_Core.h>

#include "Logfile.h"
#include "RecorderManager.h"
#include "StatusClientImpl.h"

const bool CLIENT = true;

const ProdAuto::Rational EDIT_RATE = { 25, 1 }; // We are working at 25 frames per second

// Globals
RecorderManager rec_manager;
bool done = false;

// A handler for stdin
class Event_Handler : public ACE_Event_Handler
{
public:
  int handle_input (ACE_HANDLE fd = ACE_INVALID_HANDLE);
};

int Event_Handler::handle_input(ACE_HANDLE h)
{
    char buf[100];
    //std::cout << "Reading..." << std::endl;
    int n = ACE_OS::read(h, buf, sizeof(buf));
    buf[n] = 0;
    //std::cout << "Read: " << buf;

    // remove CR LF etc.
    --n;
    while (buf[n] < 0x20)
    {
        buf[n] = 0;
        --n;
    }
    std::string command = buf;

    if ("q" == command || "quit" == command)
    {
        std::cout << "Quitting" << std::endl;
        done = true;
    }
    else if ("s" == command || "start" == command)
    {
        std::cout << "Starting" << std::endl;
        rec_manager.Start("MyProject");
    }
    else if ("t" == command | "stop" == command)
    {
        std::cout << "Stopping" << std::endl;
        rec_manager.Stop();
    }

    return 0;
}

// Main program
int main(int argc, char * argv[])
{
    Logfile::DebugLevel(2);

// Initialise ORB
    CorbaUtil::Instance()->InitOrb(argc, argv);

    bool interactive = true; // default
    bool start = false;
    bool stop = false;
    bool help = false;
    std::string recorder_name;

// Process non-corba command line args
    for (int i = 1; i <argc; ++i)
    {
        if (0 == strcmp(argv[i], "-h"))
        {
            std::cerr << "Usage: RecorderClient <recorder-name>"
                " [-start|-stop] <ORB options>\n";
            exit(0);
        }
        if (0 == strcmp(argv[i], "-start"))
        {
            interactive = false;
            start = true;
        }
        else if (0 == strcmp(argv[i], "-stop"))
        {
            interactive = false;
            stop = true;
        }
        else if (0 == strcmp(argv[i], "-nop"))
        {
            interactive = false;
        }
        else
        {
            recorder_name = argv[i];
        }
    }
    if (help || recorder_name.empty())
    {
        std::cerr << "Usage: RecorderClient <recorder-name>"
            " [-start|-stop] <ORB options>\n";
        exit(0);
    }

// Apply timeout for CORBA operations
    const int timeoutsecs = 5;
    CorbaUtil::Instance()->SetTimeout(timeoutsecs);

// Get the naming service object reference(s) using initial references
// which were passed to Orb from command line arguments.
    CorbaUtil::Instance()->InitNs();

// Get object reference for recorder from naming service
    CosNaming::Name name;
    name.length(3);
    name[0].id = CORBA::string_dup("ProductionAutomation");
    name[1].id = CORBA::string_dup("RecordingDevices");
    if (argc > 1)
    {
        name[2].id = CORBA::string_dup(recorder_name.c_str());
    }
    else
    {
        name[2].id = CORBA::string_dup("Studio A");
    }

    CORBA::Object_var obj = CorbaUtil::Instance()->ResolveObject(name);

// Setup RecorderManager
    rec_manager.Recorder(obj.in());
    rec_manager.Init();


// Now set up the StatusClient servant
    StatusClientImpl * p_servant;
    ProdAuto::StatusClient_var client_ref;
    if (CLIENT)
    {
    // Create the servant object
        p_servant = new StatusClientImpl();

    // Activate POA manager
        CorbaUtil::Instance()->ActivatePoaMgr();

    // Incarnate servant object
        client_ref = p_servant->_this();

    // Attach client to recorder
        rec_manager.AddStatusClient(client_ref.in());
    }

// Now wait on events
    if (interactive)
    {

    // Register handler for stdin
        Event_Handler handler;
        ACE_Event_Handler::register_stdin_handler(&handler,
            CorbaUtil::Instance()->OrbCore()->reactor(),
            CorbaUtil::Instance()->OrbCore()->thr_mgr());


        std::cout << "[ s(start) | t(stop) | q(quit) ] <return>" << std::endl;

        while (!done)
        {
            ACE_Time_Value corba_timeout(0, 100000);  // seconds, microseconds

            // Check for incoming CORBA requests or other events
            CorbaUtil::Instance()->OrbRun(corba_timeout);
        }
    }
    else if (start)
    {
        rec_manager.Start("MyProject");
    }
    else if (stop)
    {
        rec_manager.Stop();
    }

    if (CLIENT)
    {
        // Check for incoming CORBA requests
        ACE_Time_Value corba_timeout(0, 500000);  // seconds, microseconds
        CorbaUtil::Instance()->OrbRun(corba_timeout);

        // Detach client from recorder
        rec_manager.RemoveStatusClient(client_ref.in());

        // Destroy client servant object
        p_servant->Destroy();
    }

    return 0;
}
