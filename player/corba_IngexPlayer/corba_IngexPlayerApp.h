#ifndef __CORBA_INGEX_PLAYER_APP_H__
#define __CORBA_INGEX_PLAYER_APP_H__


#include <orbsvcs/CosNamingC.h>

#include "corba_IngexPlayerImpl.h"


namespace corba
{
namespace prodauto
{

class IngexPlayerApp
{
public:
    // singleton access
    static IngexPlayerApp * Instance()
    {
        if (0 == mInstance)
        {
            mInstance = new IngexPlayerApp();
        }
        return mInstance;
    }
    
    // singleton destroy
    void Destroy() { mInstance = 0; delete this; }
    
    // control methods
    bool Init(int argc, char * argv[]);
    void Run();
    void Stop();
    void Clean();

protected:
    // constructor protected as this is a singleton
    IngexPlayerApp() {};

private:
    // singleton instance pointer
    static IngexPlayerApp * mInstance;

    // CORBA servant
    IngexPlayerImpl* mpServant;
    IngexPlayer_var mRef;
    CosNaming::Name mName;
    bool mIsAdvertised;

    // flag for terminating main loop
    bool mTerminated;
};

};
};

#endif

