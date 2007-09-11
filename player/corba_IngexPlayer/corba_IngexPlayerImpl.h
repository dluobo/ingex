#ifndef __CORBA_INGEX_PLAYER_IMPL_H__
#define __CORBA_INGEX_PLAYER_IMPL_H__

#include <vector>

#include <IngexPlayerS.h>

#include <LocalIngexPlayer.h>

#include "CorbaIngexPlayerListener.h"



namespace corba
{
namespace prodauto
{

class IngexPlayerImpl : public virtual ::POA_corba::prodauto::IngexPlayer
{
public:
    IngexPlayerImpl();
    virtual ~IngexPlayerImpl();

    void destroy();
    
    
    virtual ::CORBA::Boolean registerListener(::corba::prodauto::IngexPlayerListener_ptr listener)
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean unregisterListener(::corba::prodauto::IngexPlayerListener_ptr listener)
        throw (::CORBA::SystemException);
    

    virtual ::CORBA::Boolean start(const ::corba::prodauto::StringList & filenames,
        ::corba::prodauto::BooleanList_out opened)
        throw (::CORBA::SystemException);
    
        
    virtual ::CORBA::Boolean stop()
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean toggleLock()
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean play()
        throw (::CORBA::SystemException);
    
    virtual
    ::CORBA::Boolean pause()
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean togglePlayPause()
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean seek(::CORBA::LongLong offset, ::corba::prodauto::WhenceType whence)
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean playSpeed(::CORBA::Long speedFactor)
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean step(::CORBA::Boolean forward)
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean mark()
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean clearMark()
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean clearAllMarks()
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean seekNextMark()
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean seekPrevMark()
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean toggleOSD()
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean nextOSDTimecode()
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean switchNextVideo()
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean switchPrevVideo()
        throw (::CORBA::SystemException);
    
    virtual ::CORBA::Boolean switchVideo(::CORBA::Long index)
        throw (::CORBA::SystemException);
        
private:
    ::prodauto::LocalIngexPlayer* _localPlayer;
    
    std::vector< ::prodauto::CorbaIngexPlayerListener*> _listeners;
};


};
};

#endif

