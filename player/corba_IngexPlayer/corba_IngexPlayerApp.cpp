#include <CorbaUtil.h>

#include "corba_IngexPlayerApp.h"

using namespace std;
using namespace corba::prodauto;


// Static member
IngexPlayerApp* IngexPlayerApp::mInstance = 0;

bool IngexPlayerApp::Init(int argc, char * argv[])
{
    // Create the servant object.
    mpServant = new IngexPlayerImpl();

    // other initialisation
    mTerminated = false;

    // initialise ORB
    CorbaUtil::Instance()->InitOrb(argc, argv);

    
    // TODO: initialise the IngexPlayerImpl here
    bool ok = true;

    
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
    mName[1].id = CORBA::string_dup("IngexPlayer");
    mName[2].id = CORBA::string_dup("slater");

    // Try to advertise in naming services
    if (ok)
    {
        ok = mIsAdvertised = CorbaUtil::Instance()->Advertise(mRef, mName);
    }


    return ok;
}

void IngexPlayerApp::Run()
{
    // Accept CORBA requests until told to stop
    ACE_Time_Value timeout(1,0);  // seconds, microseconds
    while(!mTerminated)
    {
        CorbaUtil::Instance()->OrbRun(timeout);
    }
}

void IngexPlayerApp::Stop()
{
    // Cause Run loop to exit
    mTerminated = true;
}

void IngexPlayerApp::Clean()
{
    // remove from naming services
    if (mIsAdvertised)
    {
        CorbaUtil::Instance()->Unadvertise(mName);
    }

    // Deactivate the CORBA object and
    // relinquish our reference to the servant.
    // The POA will delete it at an appropriate time.
    mpServant->destroy();
}



