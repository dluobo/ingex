// quartzRouter.cpp

#include <ace/OS_NS_unistd.h>
#include <string>
#include <sstream>
#include <list>

#include "quartzRouter.h"

Router::svc ()
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT ("(%t) router handler running\n"))); 

    readUpdate();

    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Exiting router thread\n") )); 

    return 0;
}

/**
Default Constructor
*/
Router::Router()
: mpObserver(0), mRun(false)
{
}

/**
Constructor
*/
#if 0
Router::Router(std::string rp)
: mpObserver(0)
{

    m_routerConnected = 0;
    ACE_TTY_IO::Serial_Params myparams;
    myparams.baudrate = 38400; /* 38400 for router */
    myparams.parityenb = false;
    myparams.databits = 8;
    myparams.stopbits = 1;
    myparams.readtimeoutmsec = 100; //-1;
    myparams.ctsenb = 0;
    myparams.rcvenb = true;

    if (rp.compare("default") == 0)
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("rp =  %s\n"), rp.c_str() ));
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("default settings router.\n") ));
        for(int i = 0; i < 16; i++) // how many should we look for and what should the timeout be?
        { /* test it */
        char com[20];

        sprintf(com, "COM%d", i);
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("trying %s\n"), com ));
        //ACE_Time_Value tm(0.2);
        ACE_Time_Value tm(0, 200000);
        ACE_DEV_Connector mTestConnector;
        int ret = mTestConnector.connect(mSerialDevice, ACE_DEV_Addr(ACE_TEXT(com)), &tm);
        mSerialDevice.control(ACE_TTY_IO::SETPARAMS, &myparams);
        //ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("return value =  %d\t"), ret ));

        if(ret == 0)
        {
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("port available - %s\t"), com ));
            //check for router then close connection
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Checking for router: Sending .#01\n") ));
            mSerialDevice.send ((const void *)".#01\r", 5);
            //ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Sent\n") ));
            bool tst = readReply();
            if (tst)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Ack from router!\n") ));
                //strcpy(rp, com.c_str()); 
                rp.assign(com);
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("rp =  %s\n"), rp.c_str() ));
                i=16;
            } 
        
        }
        else
        {
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Port unavailable.\n") ));
        }
        //set rp to correct com value
        mSerialDevice.close(); // is this needed?
        } /* test it */
    } 
    //mSerialPort.Open("com1");
    //if (mDeviceConnector.connect(mSerialDevice, ACE_DEV_Addr(ACE_TEXT("com1")))== -1)
    if (mDeviceConnector.connect(mSerialDevice, ACE_DEV_Addr(ACE_TEXT(rp.c_str()))) == -1)
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Could not open router.\n") ));
    }
    else
    {
        //check for router
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Checking for router: Sending .#01\n") ));
        mSerialDevice.send ((const void *)".#01\r", 5);
        bool tst = readReply();
        if (tst)
        {
            m_routerConnected = 1;
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Ack from router!\n") ));
        } 
        else
        {
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("No router connected!\nExiting!\n") ));
        }
    }


    mSerialDevice.control(ACE_TTY_IO::SETPARAMS, &myparams);

    //Init();
}
#endif

/**
Destructor
*/
Router::~Router()
{
}

/**
Open serial port and send set-up string to router
*/
void Router::Init(const std::string & port)
{
    std::list<std::string> port_list;

    if (port.empty())
    {
        // Find available ports
        for (int i = 0; i < 8; i++) // how many should we look for
        {
            char trial_port[20];
            sprintf(trial_port, "COM%d", i);
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("trying %C"), trial_port ));

            //ACE_Time_Value tm(0.2);
            ACE_Time_Value tm(0, 200000);
            ACE_DEV_Connector test_connector;
            ACE_TTY_IO device;
            int ret = test_connector.connect(device, ACE_DEV_Addr(ACE_TEXT(trial_port)), &tm);
            if (0 == ret)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT (" - port exists\n") ));
                port_list.push_back(trial_port);
            }
            else
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT (" - no such port\n") ));
            }
            device.close();
        }
    }
    else
    {
        // Use the supplied port name
        port_list.push_back(port);
    }

    //Now search for connected router
    ACE_TTY_IO::Serial_Params myparams;
    myparams.baudrate = 38400; /* 38400 for router */
    myparams.parityenb = false;
    myparams.databits = 8;
    myparams.stopbits = 1;
    myparams.readtimeoutmsec = 100; //-1;
    myparams.ctsenb = 0;
    myparams.rcvenb = true;

    m_routerConnected = false;
    for (std::list<std::string>::const_iterator it = port_list.begin();
        !m_routerConnected && it != port_list.end(); ++it)
    {
        if (mDeviceConnector.connect(mSerialDevice, ACE_DEV_Addr(ACE_TEXT(it->c_str()))) == -1)
        {
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Could not open %C.\n"), it->c_str() ));
        }
        else
        {
            //check for router
            mSerialDevice.control(ACE_TTY_IO::SETPARAMS, &myparams);

            ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Checking for router on %C: Sending .#01\n"), it->c_str() ));
            mSerialDevice.send ((const void *)".#01\r", 5);
            bool tst = readReply();
            if (tst)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Ack from router!\n") ));
                m_routerConnected = true;
            } 
            else
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("No router connected!\n") ));
            }
        }
    }


    //mSerialDevice.send((const void *)"Hello router", 12); 

    mWritePtr = &mBuffer[0];
    mBufferEnd = mBuffer + qbufsize;
}


char Router::readByte()
{
    char charReceived;
    ssize_t bytes_read;
    //char * readPtr;
    bool carryOn = true;
    
    while (carryOn)
    {
        bytes_read = mSerialDevice.recv ((void *) &charReceived, 1);
        if (bytes_read == 1)
        {
            carryOn = false;
        }
    }
    return charReceived;
}

/**
Private method to read available data from serial port into a buffer.
*/
void Router::readUpdate()
{
    char charReceived;
    ssize_t bytes_read;
    char * readPtr;
    //bool carryOn = 1;

    // expect quartz router command as .U{level}ddd,sss\r

    const ACE_Time_Value READ_INTERVAL(0, 10000);

    mRun = true; // set running flag
    while (mRun)
    {
        bytes_read = mSerialDevice.recv ((void *) &charReceived, 1);


        if (bytes_read == 1)
        {
            //ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("read: %c\n"), charReceived));
            if (charReceived == '\r')
            {
                // convert the bytes received so far into a router command string.
                // First, see if we have received enough bytes to decode

                // find the dot . at the start of the message
                readPtr = mWritePtr - 1; // last char received
                while (readPtr >= mBuffer && *readPtr != '.')
                {
                    readPtr--;
                }

                std::string router_update_string;
                if (readPtr >= mBuffer)
                { // still in buffer, so . found
                    // get timeocde at this moment
                    //timecode = (*tcReader).getTimecode();
                    readPtr++; // skip over .
                    // next char should be a U
    // TO DO: DGK decode U string digits
                    while (readPtr < mWritePtr)
                    {
                        // find destination port number from router update string
                        // .U{level}{dest},{source}\r
                        // {level} is one or more alpha chars, so find first digit in string

                        router_update_string += *(readPtr++); // copy update command string
                        //run = 0; // exit
                    }
                }

                // reset write pointer
                mWritePtr = mBuffer;

                if (mpObserver)
                {
                    mpObserver->Observe(router_update_string);
                }
            }
            else
            {
                // save this byte in the buffer
                // is buffer full?
                if (mWritePtr >= mBufferEnd)
                {
                    mWritePtr = mBuffer; // just start at beginning of buffer
                }

                *mWritePtr = charReceived;
                mWritePtr++;

                if (charReceived == 'q')
                {
                    mRun = false;
                }
            }
        } // end of bytes read OK

        ACE_OS::sleep(READ_INTERVAL); // Don't use all the CPU time.
    } //end of while (mRun)
}


void Router::Stop()
{
    mRun = false;
    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Stopping router thread\n") )); 
}

void Router::setSource(int source)
{
    char command[20];
    SYSTEMTIME st1, st2;

    sprintf(command, ".SV016,%03.3d\r", source);
    //ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Sending %s\n"), command ));
    GetSystemTime(&st1);
    mSerialDevice.send (command, 11);
    GetSystemTime(&st2);

    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("RC at %ds %d\n"), st1.wSecond, st1.wMilliseconds));
    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Send finished at %ds %d\n"), st2.wSecond, st2.wMilliseconds));

}


// read router reply from ".01#(cr)" (is router connected?) command. Should be ".A(cr)"
bool Router::readReply()
{
    char charReceived;
    ssize_t bytes_read;
    char * readPtr;
        
    
    std::string RouterString = "";
    char * mWPtr, *mBufEnd;
    char mBuf[qbufsize];
    mWPtr = &mBuf[0];
    mBufEnd = mBuf + qbufsize;


    // expect quartz router reply as .A\r
    bool run = 1; // set running flag
    while (run)
    {
        // timeout
        //ACE_Time_Value tm(0.2);
        ACE_Time_Value tm(0, 200000);
        bytes_read = mSerialDevice.recv_n ((void *) &charReceived, 1, &tm);

        if (bytes_read == 1)
        {
            if (charReceived == '\r')
            {
                run = 0;
                // convert the bytes received so far into a router command string.
                // First, see if we have received enough bytes to decode

                // find the dot . at the start of the message
                readPtr = mWPtr - 1; // last char received
                while (readPtr >= mBuf && *readPtr != '.')
                {
                    readPtr--;
                }

                RouterString = "";                  
                if (readPtr >= mBuf)
                {
                    // still in buffer, so . found
                    readPtr++; // skip over .
                    // next char should be an A
                    while (readPtr < mWPtr)
                    {
                        RouterString += *(readPtr++); // copy update command string
                        //run = 0; // exit
                    }
                }

                // reset write pointer
                mWPtr = mBuf;
                //ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("routerstring: %s\n"), RouterString.c_str() ));
            }
            else
            {
                // save this byte in the buffer
                // is buffer full?
                if (mWPtr >= mBufEnd)
                {
                    mWPtr = mBuf; // just start at beginning of buffer
                }

                *mWPtr = charReceived;
                mWPtr++;
            }
        } // end of bytes read OK
        else
        {
            run =0;
        }
    } // end of while (run)

    //ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("found %s\n"), mBuf ));
    if (::strncmp (mBuf, ".A", 2) == 0)
    return true;
    else
    return false;
}


// read current source connected to router 
//(reply from interrogate route command, ".I{level}{dest}(cr)" )
std::string Router::CurrentSrc(unsigned int dest)
{
    //find current source connected to router
    std::ostringstream ss;
    ss << ".IV" << dest << "\r";
    const std::string & command = ss.str();
    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Sending %C\n"), command.c_str() ));
    mSerialDevice.send (command.c_str(), command.length());

    char charReceived;
    ssize_t bytes_read;
    char * readPtr;
        
    
    std::string RouterString;
    char * mWPtr, *mBufEnd;
    char mBuf[qbufsize];
    mWPtr = &mBuf[0];
    mBufEnd = mBuf + qbufsize;

    // expect quartz router reply as .I{level}{dest},{srce}\r
    bool run = 1; // set running flag NB. hiding global here!
    while (run)
    {
        //ACE_Time_Value tm(0.2);
        ACE_Time_Value tm(0, 200000);
        bytes_read = mSerialDevice.recv_n ((void *) &charReceived, 1, &tm);
        //bytes_read = mSerialDevice.recv ((void *) &charReceived, 1);

        if (bytes_read == 1)
        {
            if (charReceived == '\r')
            {
                run = 0;
                // convert the bytes received so far into a router command string.
                // First, see if we have received enough bytes to decode

                // find the dot . at the start of the message
                readPtr = mWPtr - 1; // last char received
                while (readPtr >= mBuf && *readPtr != '.')
                {
                    readPtr--;
                }

                RouterString = "";                  
                if (readPtr >= mBuf)
                {
                    // still in buffer, so . found
                    readPtr++; // skip over .
                    // next char should be an I
                    while (readPtr < mWPtr)
                    {
                        RouterString += *(readPtr++); // copy update command string
                        //run = 0; // exit
                    }
                }

                // reset write pointer
                mWPtr = mBuf;
            }
            else
            {
                // save this byte in the buffer
                // is buffer full?
                if (mWPtr >= mBufEnd)
                {
                    mWPtr = mBuf; // just start at beginning of buffer
                }

                *mWPtr = charReceived;
                mWPtr++;
            }
        } // end of bytes read OK
        else
        {
            run = false;
        }
    } // end of infinite while loop

    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("router reply [.{level}{destination},{source}]: %C\n"), RouterString.c_str() ));
    //m_CurSrc = RouterString.c_str();

    return RouterString;
}

// returns the string containing the current source connected to the router
//std::string Router::getSrc()
//{
//    return m_CurSrc;
//}

// returns true if router is connected
bool Router::isConnected()
{
    return m_routerConnected;
}