#include <cstdio>
#include <ace/Signal.h>

#include "corba_IngexPlayerApp.h"

using namespace std;
using namespace corba::prodauto;


extern "C" void handler(int)
{
    corba::prodauto::IngexPlayerApp::Instance()->Stop();
}

int main(int argc, char * argv[])
{
    // Setup handler for ctrl-c
    ACE_Sig_Action sa ((ACE_SignalHandler) handler, SIGINT);
    
    // Get instance of app
    IngexPlayerApp* app = IngexPlayerApp::Instance();
    
    // Run the app
    if( app->Init(argc, argv) )
    {
        fprintf(stderr, "Ingex Player Started...\n");
        app->Run();
        fprintf(stderr, "Ingex Player Stopped\n");
    }
    app->Clean();
    
    return 0;
}

