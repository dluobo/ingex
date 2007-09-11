// CoordinatorApp.cpp

#include <CorbaUtil.h>

#include "CoordinatorApp.h"

// Static member
CoordinatorApp * CoordinatorApp::mInstance = 0;

bool CoordinatorApp::Init(int argc, ACE_TCHAR * argv[])
{
// Create the servant object
	mpServant = new CoordinatorSubject();
// and initialise


// other initialisation
	mTerminated = false;

// initialise ORB
	CorbaUtil::Instance()->InitOrb(argc, argv);

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
	mName[1].id = CORBA::string_dup("Processors");
	mName[2].id = CORBA::string_dup("Coordinator");

// Try to advertise in naming services
	bool ok = CorbaUtil::Instance()->Advertise(mRef, mName);

	return ok;
}

void CoordinatorApp::Run()
{
	// Accept CORBA requests until told to stop
	ACE_Time_Value timeout(1,0);  // seconds, microseconds
	while(!mTerminated)
	{
		CorbaUtil::Instance()->OrbRun(timeout);
	}
}

void CoordinatorApp::Stop()
{
	// Cause Run loop to exit
	mTerminated = true;
}

void CoordinatorApp::Clean()
{
// remove from naming services
	CorbaUtil::Instance()->Unadvertise(mName);

// Deactivate the CORBA object and
// relinquish our reference to the servant.
// The POA will delete it at an appropriate time.
	mpServant->Destroy();
}


