#include <cassert>

#include "corba_IngexPlayerListenerImpl.h"

#include <IngexPlayerListener.h>

extern "C" {
#include <media_player.h>
}

using namespace corba::prodauto;
using namespace std;


static void convertTimecode(const ::corba::prodauto::Timecode* corbaTimecode, ::Timecode* timecode)
{
    timecode->hour = corbaTimecode->hour;
    timecode->min = corbaTimecode->min;
    timecode->sec = corbaTimecode->sec;
    timecode->frame = corbaTimecode->frame;
}

static void convertTimecodeInfo(const ::corba::prodauto::TimecodeInfo* corbaTimecodeInfo, ::TimecodeInfo* timecodeInfo)
{
    timecodeInfo->streamId = corbaTimecodeInfo->streamId;
    switch (corbaTimecodeInfo->tcType)
    {
        case ::corba::prodauto::UNKNOWN_TIMECODE_TYPE:
            timecodeInfo->timecodeType = ::UNKNOWN_TIMECODE_TYPE;
            break;
        case ::corba::prodauto::CONTROL_TIMECODE_TYPE:
            timecodeInfo->timecodeType = ::CONTROL_TIMECODE_TYPE;
            break;
        case ::corba::prodauto::SOURCE_TIMECODE_TYPE:
            timecodeInfo->timecodeType = ::SOURCE_TIMECODE_TYPE;
            break;
        default:
            assert(false);
            timecodeInfo->timecodeType = ::UNKNOWN_TIMECODE_TYPE;
            break;
    }
    switch (corbaTimecodeInfo->tcSubType)
    {
        case ::corba::prodauto::NO_TIMECODE_SUBTYPE:
            timecodeInfo->timecodeSubType = ::NO_TIMECODE_SUBTYPE;
            break;
        case ::corba::prodauto::VITC_SOURCE_TIMECODE_SUBTYPE:
            timecodeInfo->timecodeSubType = ::VITC_SOURCE_TIMECODE_SUBTYPE;
            break;
        case ::corba::prodauto::LTC_SOURCE_TIMECODE_SUBTYPE:
            timecodeInfo->timecodeSubType = ::LTC_SOURCE_TIMECODE_SUBTYPE;
            break;
        default:
            assert(false);
            timecodeInfo->timecodeSubType = ::NO_TIMECODE_SUBTYPE;
            break;
    }
    
    convertTimecode(&corbaTimecodeInfo->tc, &timecodeInfo->timecode);
}

static void convertFrameInfo(const ::corba::prodauto::FrameInfo* corbaFrameInfo, ::FrameInfo* frameInfo)
{
    int i;
    
    for (i = 0; i < corbaFrameInfo->timecodes.length() && i < sizeof(frameInfo->timecodes); i++)
    {
        convertTimecodeInfo(&corbaFrameInfo->timecodes.operator[](i), &frameInfo->timecodes[i]);
    }
    
    frameInfo->position = corbaFrameInfo->position;
    frameInfo->frameCount = corbaFrameInfo->frameCount;
    frameInfo->rateControl = corbaFrameInfo->rateControl;
    frameInfo->mark = corbaFrameInfo->mark;
}

static void convertEvent(const ::corba::prodauto::MediaPlayerStateEvent* corbaEvent, 
    ::MediaPlayerStateEvent* event)
{
    event->lockedChanged = corbaEvent->lockedChanged;
    event->locked = corbaEvent->locked;
    event->playChanged = corbaEvent->playChanged;
    event->play = corbaEvent->play;
    event->stopChanged = corbaEvent->stopChanged;
    event->stop = corbaEvent->stop;
    event->speedFactorChanged = corbaEvent->speedFactorChanged;
    event->speedFactor = corbaEvent->speedFactor;
    event->lastMarkSetChanged = corbaEvent->lastMarkSetChanged;
    event->lastMarkSet = corbaEvent->lastMarkSet;
    event->lastMarkRemovedChanged = corbaEvent->lastMarkRemovedChanged;
    event->lastMarkRemoved = corbaEvent->lastMarkRemoved;
    event->allMarksCleared = corbaEvent->allMarksCleared;

    convertFrameInfo(&corbaEvent->displayedFrameInfo, &event->displayedFrameInfo);
}


IngexPlayerListenerImpl::IngexPlayerListenerImpl(::prodauto::IngexPlayerListener* listener)
: _listener(listener)
{
}

IngexPlayerListenerImpl::~IngexPlayerListenerImpl()
{
}

void IngexPlayerListenerImpl::destroy()
{
  // Get the POA used when activating the object.
  PortableServer::POA_var poa = this->_default_POA();

  // Get the object ID associated with this servant.
  PortableServer::ObjectId_var oid = poa->servant_to_id (this);

  // Now deactivate the object.
  poa->deactivate_object(oid.in());

  // Decrease the reference count on ourselves.
  this->_remove_ref();
}


void IngexPlayerListenerImpl::frameDisplayedEvent(const FrameInfo& corbaFrameInfo)
    throw (::CORBA::SystemException)
{
    ::FrameInfo frameInfo;
    
    convertFrameInfo(&corbaFrameInfo, &frameInfo);
    
    _listener->frameDisplayedEvent(&frameInfo);
}

void IngexPlayerListenerImpl::frameDroppedEvent(const FrameInfo& corbaLastFrameInfo)
    throw (::CORBA::SystemException)
{
    ::FrameInfo lastFrameInfo;
    
    convertFrameInfo(&corbaLastFrameInfo, &lastFrameInfo);
    
    _listener->frameDroppedEvent(&lastFrameInfo);
}

void IngexPlayerListenerImpl::stateChangeEvent(const MediaPlayerStateEvent& corbaEvent)
    throw (::CORBA::SystemException)
{
    ::MediaPlayerStateEvent event;
    
    convertEvent(&corbaEvent, &event);
    
    _listener->stateChangeEvent(&event);
}

void IngexPlayerListenerImpl::playerClose()
    throw (::CORBA::SystemException)
{
    _listener->playerClose();
}




