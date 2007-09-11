#ifndef __PRODAUTO_CORBA_INGEX_PLAYER_LISTENER_H__
#define __PRODAUTO_CORBA_INGEX_PLAYER_LISTENER_H__


#include <IngexPlayerListener.h>

#include <IngexPlayerC.h>


namespace prodauto
{

    
class CorbaIngexPlayerListener : public IngexPlayerListener
{
public:
    CorbaIngexPlayerListener(::corba::prodauto::IngexPlayerListener_ptr corbaListener);
    virtual ~CorbaIngexPlayerListener();

    virtual void frameDisplayedEvent(const FrameInfo* frameInfo);
    virtual void frameDroppedEvent(const FrameInfo* lastFrameInfo);
    virtual void stateChangeEvent(const MediaPlayerStateEvent* event);
    virtual void playerClose();
    
    ::corba::prodauto::IngexPlayerListener_ptr getCorbaListener()
    {
        return _corbaListener;
    }
    
private:
    ::corba::prodauto::IngexPlayerListener_var _corbaListener;
};



};

#endif

