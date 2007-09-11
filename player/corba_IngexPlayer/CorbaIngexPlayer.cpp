#include <pthread.h>
#include <cassert>


#include <CorbaUtil.h>

#include <IngexPlayerC.h>

#include "CorbaIngexPlayer.h"
#include "corba_IngexPlayerListenerImpl.h"

#include <Macros.h>



extern "C" 
{
#include <multiple_sources.h>
#include <raw_file_source.h>
#include <mxf_source.h>
#include <x11_display_sink.h>
#include <dvs_sink.h>
#include <raw_file_sink.h>
#include <video_switch.h>
#include <logging.h>
}


using namespace prodauto;
using namespace std;


namespace prodauto
{

class CorbaIngexPlayerData : public IngexPlayerListener
{
public:
    friend class CorbaIngexPlayer;
    
public:
    CorbaIngexPlayerData()
    : mpServant(0), mRef(0), mTerminated(true)
    {}

    void init()
    {
        // Create the servant object.
        mpServant = new ::corba::prodauto::IngexPlayerListenerImpl((IngexPlayerListener*)this);
    
        // other initialisation
        mTerminated = false;
    
        // incarnate servant object
        mRef = mpServant->_this();
    }
    
    virtual ~CorbaIngexPlayerData()
    {
        //stop();
        /* wait a second or 2 */
        
        if (mpServant)
        {
            mpServant->destroy();
        }
    }

    void start()
    {
        // create thread
    }
    
    void stop()
    {
        //mTerminated = true;
    }
    
    
    // IngexPlayerListener methods
    
    virtual void frameDisplayedEvent(const FrameInfo* frameInfo)
    {
        vector<IngexPlayerListener*>::const_iterator iter;
        for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
        {
            (*iter)->frameDisplayedEvent(frameInfo);
        }
    }
    
    virtual void frameDroppedEvent(const FrameInfo* lastFrameInfo)
    {
        vector<IngexPlayerListener*>::const_iterator iter;
        for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
        {
            (*iter)->frameDroppedEvent(lastFrameInfo);
        }
    }
    
    virtual void stateChangeEvent(const MediaPlayerStateEvent* event)
    {
        vector<IngexPlayerListener*>::const_iterator iter;
        for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
        {
            (*iter)->stateChangeEvent(event);
        }
    }
    
    virtual void playerClose()
    {
        vector<IngexPlayerListener*>::const_iterator iter;
        for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
        {
            (*iter)->playerClose();
        }
    }
    
private:
    vector<IngexPlayerListener*> _listeners;

    ::corba::prodauto::IngexPlayerListenerImpl* mpServant;
    ::corba::prodauto::IngexPlayerListener_var mRef;

    // flag for terminating main loop
    bool mTerminated;
    
    ::corba::prodauto::IngexPlayer_var _corbaIngexPlayer;
};

};



CorbaIngexPlayer::CorbaIngexPlayer(int argc, char* argv[])
{
    _data = new CorbaIngexPlayerData();
    
    //_data->init(0, NULL);
    // start listener running in new thread
    

    // Initialise ORB
    CorbaUtil::Instance()->InitOrb(argc, argv);

    // Apply timeout for CORBA operations
    const int timeoutsecs = 5;
    CorbaUtil::Instance()->SetTimeout(timeoutsecs);

    // activate POA manager
    CorbaUtil::Instance()->ActivatePoaMgr();
    
    // Get the naming service object reference(s) using initial references
    // which were passed to Orb from command line arguments.
    CorbaUtil::Instance()->InitNs();

    // Get object reference for recorder from naming service
    CosNaming::Name name;
    name.length(3);
    name[0].id = CORBA::string_dup("ProductionAutomation");
    name[1].id = CORBA::string_dup("IngexPlayer");
    name[2].id = CORBA::string_dup("slater");

    CORBA::Object_var obj = CorbaUtil::Instance()->ResolveObject(name);

    // Setup IngexPlayer

    if (!CORBA::is_nil(obj.in()))
    {
        try
        {
            _data->_corbaIngexPlayer = ::corba::prodauto::IngexPlayer::_narrow(obj.in());
        }
        catch(const CORBA::Exception &)
        {
            _data->_corbaIngexPlayer = ::corba::prodauto::IngexPlayer::_nil();
        }
    }
    else
    {
        _data->_corbaIngexPlayer = ::corba::prodauto::IngexPlayer::_nil();
    }
}

CorbaIngexPlayer::~CorbaIngexPlayer()
{
    // unregister corba listener

    
    delete _data;
}


bool CorbaIngexPlayer::start(vector<string> mxfFilenames, vector<bool>& opended)
{
    bool result = false;
    
    try
    {
        const ::corba::prodauto::StringList filenames;
        ::corba::prodauto::BooleanList_var opened;
        
        result = _data->_corbaIngexPlayer->start(filenames, opened.out());
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::registerListener(IngexPlayerListener* listener)
{
    try
    {
        _data->_listeners.push_back(listener);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool CorbaIngexPlayer::unregisterListener(IngexPlayerListener* listener)
{
    try
    {
        vector<IngexPlayerListener*>::iterator iter;
        for (iter = _data->_listeners.begin(); iter != _data->_listeners.end(); iter++)
        {
            if (*iter == listener)
            {
                _data->_listeners.erase(iter);
                break;
            }
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool CorbaIngexPlayer::stop()
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->stop();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::toggleLock()
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->toggleLock();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::play()
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->play();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::pause()
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->pause();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::togglePlayPause()
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->togglePlayPause();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::seek(int64_t offset, int whence)
{
    bool result = false;
    
    try
    {
        switch (whence)
        {
            case SEEK_SET:
                result = _data->_corbaIngexPlayer->seek(offset, ::corba::prodauto::WHENCE_SEEK_SET);
                break;
            case SEEK_CUR:
                result = _data->_corbaIngexPlayer->seek(offset, ::corba::prodauto::WHENCE_SEEK_CUR);
                break;
            case SEEK_END:
                result = _data->_corbaIngexPlayer->seek(offset, ::corba::prodauto::WHENCE_SEEK_END);
                break;
            default:
                /* TODO: assert */
                return false;
        }
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::playSpeed(int speedFactor)
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->playSpeed(speedFactor);
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::step(bool forward)
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->step(forward);
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::mark()
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->mark();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::clearMark()
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->clearMark();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::clearAllMarks()
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->clearAllMarks();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::seekNextMark()
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->seekNextMark();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::seekPrevMark()
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->seekPrevMark();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::toggleOSD()
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->toggleOSD();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::nextOSDTimecode()
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->nextOSDTimecode();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::switchNextVideo()
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->switchNextVideo();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::switchPrevVideo()
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->switchPrevVideo();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}

bool CorbaIngexPlayer::switchVideo(int index)
{
    bool result = false;
    
    try
    {
        result = _data->_corbaIngexPlayer->switchVideo(index);
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
        return false;
    }
    catch (...)
    {
        return false;
    }
    return result;
}



