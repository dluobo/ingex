#include "corba_IngexPlayerImpl.h"


using namespace corba::prodauto;
using namespace std;


IngexPlayerImpl::IngexPlayerImpl()
{
    _localPlayer = new ::prodauto::LocalIngexPlayer(::prodauto::X11_OUTPUT, true, 4, false);
}

IngexPlayerImpl::~IngexPlayerImpl()
{
    vector< ::prodauto::CorbaIngexPlayerListener*>::iterator iter = _listeners.begin();
    while (iter != _listeners.end())
    {
        _listeners.erase(iter);
        delete *iter;
    }

    delete _localPlayer;
}

void IngexPlayerImpl::destroy()
{
  // Get the POA used when activating the object.
  PortableServer::POA_var poa = this->_default_POA();

  // Get the object ID associated with this servant.
  PortableServer::ObjectId_var oid = poa->servant_to_id(this);

  // Now deactivate the object.
  poa->deactivate_object(oid.in());

  // Decrease the reference count on ourselves.
  this->_remove_ref();
}


::CORBA::Boolean IngexPlayerImpl::registerListener(::corba::prodauto::IngexPlayerListener_ptr listener)
    throw (::CORBA::SystemException)
{
    bool result = false;
    try
    {
        auto_ptr< ::prodauto::CorbaIngexPlayerListener> corbaListener(
            new ::prodauto::CorbaIngexPlayerListener(listener));
        
        result = _localPlayer->registerListener(corbaListener.get());
        if (result)
        {
            _listeners.push_back(corbaListener.release());
        }
    }
    catch (...)
    {
        return false;
    }
    return result;
}

::CORBA::Boolean IngexPlayerImpl::unregisterListener(::corba::prodauto::IngexPlayerListener_ptr listener)
    throw (::CORBA::SystemException)
{
    try
    {
        vector< ::prodauto::CorbaIngexPlayerListener*>::iterator iter;
        for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
        {
            if ((*iter)->getCorbaListener() == listener)
            {
                _localPlayer->unregisterListener(*iter);
                _listeners.erase(iter);
                delete *iter;
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

::CORBA::Boolean IngexPlayerImpl::start(const ::corba::prodauto::StringList & filenames, 
    ::corba::prodauto::BooleanList_out opened)
    throw (::CORBA::SystemException)
{
    vector<string> l_mxfFilenames;
    vector<bool> l_opened;
    opened = new ::corba::prodauto::BooleanList;
    
    l_mxfFilenames.push_back("/home/philipn/work/dv-samples/bear_25.dv_v1.mxf");
    if (_localPlayer->start(l_mxfFilenames, l_opened))
    {
        return true;
    }
    else
    {
        return false;
    }
    
    return true;
}

::CORBA::Boolean IngexPlayerImpl::stop()
    throw (::CORBA::SystemException)
{
    return _localPlayer->stop();
}

::CORBA::Boolean IngexPlayerImpl::toggleLock()
    throw (::CORBA::SystemException)
{
    return _localPlayer->toggleLock();
}

::CORBA::Boolean IngexPlayerImpl::play()
    throw (::CORBA::SystemException)
{
    return _localPlayer->play();
}

::CORBA::Boolean IngexPlayerImpl::pause()
    throw (::CORBA::SystemException)
{
    return _localPlayer->pause();
}

::CORBA::Boolean IngexPlayerImpl::togglePlayPause()
    throw (::CORBA::SystemException)
{
    return _localPlayer->togglePlayPause();
}

::CORBA::Boolean IngexPlayerImpl::seek(::CORBA::LongLong offset, ::corba::prodauto::WhenceType whence)
    throw (::CORBA::SystemException)
{
    switch (whence)
    {
        case WHENCE_SEEK_SET:
            return _localPlayer->seek(offset, SEEK_SET);
        case WHENCE_SEEK_CUR:
            return _localPlayer->seek(offset, SEEK_CUR);
        case WHENCE_SEEK_END:
            return _localPlayer->seek(offset, SEEK_END);
        default:
            /* TODO: assertion */
            return false;
    }
}

::CORBA::Boolean IngexPlayerImpl::playSpeed(::CORBA::Long speedFactor)
    throw (::CORBA::SystemException)
{
    return _localPlayer->playSpeed(speedFactor);
}

::CORBA::Boolean IngexPlayerImpl::step(::CORBA::Boolean forward)
    throw (::CORBA::SystemException)
{
    return _localPlayer->step(forward);
}

::CORBA::Boolean IngexPlayerImpl::mark()
    throw (::CORBA::SystemException)
{
    return _localPlayer->mark();
}

::CORBA::Boolean IngexPlayerImpl::clearMark()
    throw (::CORBA::SystemException)
{
    return _localPlayer->clearMark();
}

::CORBA::Boolean IngexPlayerImpl::clearAllMarks()
    throw (::CORBA::SystemException)
{
    return _localPlayer->clearAllMarks();
}

::CORBA::Boolean IngexPlayerImpl::seekNextMark()
    throw (::CORBA::SystemException)
{
    return _localPlayer->seekNextMark();
}

::CORBA::Boolean IngexPlayerImpl::seekPrevMark()
    throw (::CORBA::SystemException)
{
    return _localPlayer->seekPrevMark();
}

::CORBA::Boolean IngexPlayerImpl::toggleOSD()
    throw (::CORBA::SystemException)
{
    return _localPlayer->toggleOSD();
}

::CORBA::Boolean IngexPlayerImpl::nextOSDTimecode()
    throw (::CORBA::SystemException)
{
    return _localPlayer->nextOSDTimecode();
}

::CORBA::Boolean IngexPlayerImpl::switchNextVideo()
    throw (::CORBA::SystemException)
{
    return _localPlayer->switchNextVideo();
}

::CORBA::Boolean IngexPlayerImpl::switchPrevVideo()
    throw (::CORBA::SystemException)
{
    return _localPlayer->switchPrevVideo();
}

::CORBA::Boolean IngexPlayerImpl::switchVideo(::CORBA::Long index)
    throw (::CORBA::SystemException)
{
    return _localPlayer->switchVideo(index);
}


