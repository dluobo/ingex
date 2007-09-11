#ifndef __CORBA_INGEX_PLAYER_LISTENER_IMPL_H__
#define __CORBA_INGEX_PLAYER_LISTENER_IMPL_H__


#include <IngexPlayerS.h>



namespace prodauto
{
class IngexPlayerListener;
};


namespace corba
{
namespace prodauto
{

class  IngexPlayerListenerImpl : public virtual POA_corba::prodauto::IngexPlayerListener
{
public:
    IngexPlayerListenerImpl(::prodauto::IngexPlayerListener* listener);
    virtual ~IngexPlayerListenerImpl();

    void destroy();
    
    
    virtual void frameDisplayedEvent(const ::corba::prodauto::FrameInfo& info)
        throw (::CORBA::SystemException);
    
    virtual void frameDroppedEvent(const ::corba::prodauto::FrameInfo& lastFrameInfo)
        throw (::CORBA::SystemException);
    
    virtual void stateChangeEvent(const ::corba::prodauto::MediaPlayerStateEvent& event)
        throw (::CORBA::SystemException);
    
    virtual void playerClose() 
        throw (::CORBA::SystemException);
        
private:
    ::prodauto::IngexPlayerListener* _listener;
};

    
};
};







#endif



