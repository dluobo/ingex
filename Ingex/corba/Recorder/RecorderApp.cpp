/*
 * $Id: RecorderApp.cpp,v 1.2 2007/01/30 12:27:46 john_f Exp $
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

#include <CorbaUtil.h>
#include <Logfile.h>

#include "RecorderApp.h"

// Static member
RecorderApp * RecorderApp::mInstance = 0;

bool RecorderApp::Init(int argc, char * argv[])
{
// Set verbosity of debug messages
    Logfile::DebugLevel(2);  // need level 3 for LM_DEBUG messages

// Create the servant object.
    mpServant = new IngexRecorderImpl();

// other initialisation
    mTerminated = false;

// initialise ORB
    CorbaUtil::Instance()->InitOrb(argc, argv);

// Now that InitOrb has consumed its arguments, check for
// command line arguments.
    std::string recorder_name;
    std::string db_user;
    std::string db_pw;
    if (argc > 1)
    {
        recorder_name = argv[1];
    }
    if (argc > 2)
    {
        db_user = argv[2];
    }
    if (argc > 3)
    {
        db_pw = argv[3];
    }


// and initialise recorder with name
    bool ok = mpServant->Init(recorder_name, db_user, db_pw);

// apply timeout for CORBA operations
    const int timeoutsecs = 8;
    CorbaUtil::Instance()->SetTimeout(timeoutsecs);

// activate POA manager
    CorbaUtil::Instance()->ActivatePoaMgr();

// incarnate servant object
    mRef = mpServant->_this();

// get the NameService object reference(s)
// which were passed to Orb from command line arguments
    CorbaUtil::Instance()->InitNs();

// Set the name to be used in Naming Service
    mName.length(3);
    mName[0].id = CORBA::string_dup("ProductionAutomation");
    mName[1].id = CORBA::string_dup("RecordingDevices");
    mName[2].id = CORBA::string_dup(recorder_name.c_str());

// Try to advertise in naming services
    if(ok)
    {
        ok = mIsAdvertised = CorbaUtil::Instance()->Advertise(mRef, mName);
    }


    return ok;
}

void RecorderApp::Run()
{
    // Accept CORBA requests until told to stop
    ACE_Time_Value timeout(1,0);  // seconds, microseconds
    while(!mTerminated)
    {
        CorbaUtil::Instance()->OrbRun(timeout);
    }
}

void RecorderApp::Stop()
{
    // Cause Run loop to exit
    mTerminated = true;
}

void RecorderApp::Clean()
{
// remove from naming services
    if (mIsAdvertised)
    {
        CorbaUtil::Instance()->Unadvertise(mName);
    }

// Deactivate the CORBA object and
// relinquish our reference to the servant.
// The POA will delete it at an appropriate time.
    mpServant->Destroy();
}


