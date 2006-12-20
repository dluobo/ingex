/*
 * $Id: recorder_client_main.cpp,v 1.1 2006/12/20 12:43:59 john_f Exp $
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

#include "RecorderManager.h"

#include "StatusClientImpl.h"

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
        rec_manager.Start();
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
// Initialise ORB
    CorbaUtil::Instance()->InitOrb(argc, argv);

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
    if(argc > 1)
    {
        name[2].id = CORBA::string_dup(ACE_TEXT_ALWAYS_CHAR(argv[1]));
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

// Create the servant object
    StatusClientImpl * p_servant = new StatusClientImpl();

// Activate POA manager
    CorbaUtil::Instance()->ActivatePoaMgr();

// Incarnate servant object
    ProdAuto::StatusClient_var client_ref = p_servant->_this();

// Attach client to recorder
    rec_manager.AddStatusClient(client_ref.in());

// Now wait on events

// Register handler for stdin
    Event_Handler handler;
    ACE_Event_Handler::register_stdin_handler(&handler,
        CorbaUtil::Instance()->OrbCore()->reactor(),
        CorbaUtil::Instance()->OrbCore()->thr_mgr());


    std::cout << "[ s(start) | t(stop) | q(quit) ] <return>" << std::endl;

    while ( !done )
    {
        // Check for incoming CORBA requests
        //std::cout << "C" << std::flush;
        ACE_Time_Value corba_timeout(0, 100000);  // seconds, microseconds
        CorbaUtil::Instance()->OrbRun(corba_timeout);
    }

    // Detach client from recorder
    rec_manager.RemoveStatusClient(client_ref.in());

    // Destroy client servant object
    p_servant->Destroy();

    return 0;
}
