#include <pthread.h>
#include <cassert>


#include <CorbaUtil.h>

#include <IngexPlayerC.h>

#include "CorbaIngexPlayerListener.h"
#include "corba_IngexPlayerListenerImpl.h"

extern "C" 
{
#include <logging.h>
}
#include <Macros.h>


using namespace prodauto;
using namespace std;


static void convertTimecode(const Timecode* timecode, ::corba::prodauto::Timecode* corbaTimecode)
{
    corbaTimecode->hour = timecode->hour;
    corbaTimecode->min = timecode->min;
    corbaTimecode->sec = timecode->sec;
    corbaTimecode->frame = timecode->frame;
}

static void convertTimecodeInfo(const TimecodeInfo* timecodeInfo, ::corba::prodauto::TimecodeInfo* corbaTimecodeInfo)
{
    corbaTimecodeInfo->streamId = timecodeInfo->streamId;
    switch (timecodeInfo->timecodeType)
    {
        case UNKNOWN_TIMECODE_TYPE:
            corbaTimecodeInfo->tcType = ::corba::prodauto::UNKNOWN_TIMECODE_TYPE;
            break;
        case CONTROL_TIMECODE_TYPE:
            corbaTimecodeInfo->tcType = ::corba::prodauto::CONTROL_TIMECODE_TYPE;
            break;
        case SOURCE_TIMECODE_TYPE:
            corbaTimecodeInfo->tcType = ::corba::prodauto::SOURCE_TIMECODE_TYPE;
            break;
        default:
            assert(false);
            corbaTimecodeInfo->tcType = ::corba::prodauto::UNKNOWN_TIMECODE_TYPE;
            break;
    }
    switch (timecodeInfo->timecodeSubType)
    {
        case NO_TIMECODE_SUBTYPE:
            corbaTimecodeInfo->tcSubType = ::corba::prodauto::NO_TIMECODE_SUBTYPE;
            break;
        case VITC_SOURCE_TIMECODE_SUBTYPE:
            corbaTimecodeInfo->tcSubType = ::corba::prodauto::VITC_SOURCE_TIMECODE_SUBTYPE;
            break;
        case LTC_SOURCE_TIMECODE_SUBTYPE:
            corbaTimecodeInfo->tcSubType = ::corba::prodauto::LTC_SOURCE_TIMECODE_SUBTYPE;
            break;
        default:
            assert(false);
            corbaTimecodeInfo->tcSubType = ::corba::prodauto::NO_TIMECODE_SUBTYPE;
            break;
    }
    
    convertTimecode(&timecodeInfo->timecode, &corbaTimecodeInfo->tc);
}

static void convertFrameInfo(const FrameInfo* frameInfo, ::corba::prodauto::FrameInfo* corbaFrameInfo)
{
    int i;
    
    corbaFrameInfo->timecodes.length(frameInfo->numTimecodes);
    for (i = 0; i < frameInfo->numTimecodes; i++)
    {
        convertTimecodeInfo(&frameInfo->timecodes[i], &corbaFrameInfo->timecodes.operator[](i));
    }
    
    corbaFrameInfo->position = frameInfo->position;
    corbaFrameInfo->frameCount = frameInfo->frameCount;
    corbaFrameInfo->rateControl = frameInfo->rateControl;
    corbaFrameInfo->mark = frameInfo->mark;
}

static void convertEvent(const MediaPlayerStateEvent* event, ::corba::prodauto::MediaPlayerStateEvent* corbaEvent)
{
    corbaEvent->lockedChanged = event->lockedChanged;
    corbaEvent->locked = event->locked;
    corbaEvent->playChanged = event->playChanged;
    corbaEvent->play = event->play;
    corbaEvent->stopChanged = event->stopChanged;
    corbaEvent->stop = event->stop;
    corbaEvent->speedFactorChanged = event->speedFactorChanged;
    corbaEvent->speedFactor = event->speedFactor;
    corbaEvent->lastMarkSetChanged = event->lastMarkSetChanged;
    corbaEvent->lastMarkSet = event->lastMarkSet;
    corbaEvent->lastMarkRemovedChanged = event->lastMarkRemovedChanged;
    corbaEvent->lastMarkRemoved = event->lastMarkRemoved;
    corbaEvent->allMarksCleared = event->allMarksCleared;

    convertFrameInfo(&event->displayedFrameInfo, &corbaEvent->displayedFrameInfo);
}


CorbaIngexPlayerListener::CorbaIngexPlayerListener(::corba::prodauto::IngexPlayerListener_ptr corbaListener)
{
    _corbaListener = corbaListener;
}

CorbaIngexPlayerListener::~CorbaIngexPlayerListener()
{}

void CorbaIngexPlayerListener::frameDisplayedEvent(const FrameInfo* frameInfo)
{
    try
    {
        ::corba::prodauto::FrameInfo corbaFrameInfo;
        convertFrameInfo(frameInfo, &corbaFrameInfo);
        
        _corbaListener->frameDisplayedEvent(corbaFrameInfo);
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
    }
    catch (...)
    {
        // do nothing
    }
}

void CorbaIngexPlayerListener::frameDroppedEvent(const FrameInfo* lastFrameInfo)
{
    try
    {
        ::corba::prodauto::FrameInfo corbaLastFrameInfo;
        convertFrameInfo(lastFrameInfo, &corbaLastFrameInfo);
        
        _corbaListener->frameDroppedEvent(corbaLastFrameInfo);
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
    }
    catch (...)
    {
        // do nothing
    }
}

void CorbaIngexPlayerListener::stateChangeEvent(const MediaPlayerStateEvent* event)
{
    try
    {
        ::corba::prodauto::MediaPlayerStateEvent corbaEvent;
        convertEvent(event, &corbaEvent);
        
        _corbaListener->stateChangeEvent(corbaEvent);
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
    }
    catch (...)
    {
        // do nothing
    }
}

void CorbaIngexPlayerListener::playerClose()
{
    try
    {
        _corbaListener->playerClose();
    }
    catch (const CORBA::Exception& ex)
    {
        ml_log_error("Corba exception: '%s', near '%s', line %d\n", ex._name(), __FILE__, __LINE__);
    }
    catch (...)
    {
        // do nothing
    }
}
