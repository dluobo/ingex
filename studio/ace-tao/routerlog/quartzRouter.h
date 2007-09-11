// quartzRouter.h

#ifndef quartzRouter_h
#define quartzRouter_h

#include "ace/DEV_Addr.h"
#include "ace/DEV_Connector.h"
#include "ace/DEV_IO.h"
#include "ace/TTY_IO.h"

#include "ace/Task.h"
#include "Observer.h"

#include <string>

const int qbufsize = 128;

class Observer;

/**
Interface to Quartz router
*/
class Router : public ACE_Task_Base 
{
public:
// methods
    Router();
    Router(std::string rp);
    ~Router();
    void Init(const std::string & port);
    void Stop();

    virtual int svc ();

    void SetObserver(Observer * obs) { mpObserver = obs; }

    char readByte();
    void readUpdate();
    bool readReply();
    std::string CurrentSrc(unsigned int dest);
    void setSource(int source);
    std::string getSrc();
    bool isConnected();

private:
// methods

// members
    ACE_TTY_IO mSerialDevice;
    ACE_DEV_Connector mDeviceConnector;
    //ACE_Thread_Mutex routerMutex;

    char mBuffer[qbufsize];
    char * mWritePtr;
    char * mBufferEnd;
    bool mRun;
    bool m_routerConnected;

    Observer * mpObserver;
};

#endif //#ifndef quartzRouter_h

