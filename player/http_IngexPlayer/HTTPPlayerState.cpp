/*
 * $Id: HTTPPlayerState.cpp,v 1.2 2010/07/14 13:06:36 john_f Exp $
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define __STDC_FORMAT_MACROS 1

#include <pthread.h>
#include <stdio.h>

#include "HTTPPlayerState.h"
#include "Logging.h"
#include "IngexException.h"
#include "Utilities.h"


using namespace std;
using namespace ingex;


static bool parse_timecode(JSONObject* json, ::Timecode* timecode)
{
    bool bValue;
    int64_t iValue;

    if (!json->getBoolValue2("isDropFrame", &bValue))
    {
        return false;
    }
    timecode->isDropFrame = bValue;
    if (!json->getNumberValue2("hour", &iValue))
    {
        return false;
    }
    timecode->hour = (int)iValue;
    if (!json->getNumberValue2("min", &iValue))
    {
        return false;
    }
    timecode->min = (int)iValue;
    if (!json->getNumberValue2("sec", &iValue))
    {
        return false;
    }
    timecode->sec = (int)iValue;
    if (!json->getNumberValue2("frame", &iValue))
    {
        return false;
    }
    timecode->frame = (int)iValue;

    return true;
}

static bool parse_timecode_info(JSONObject* json, ::TimecodeInfo* timecodeInfo)
{
    int64_t iValue;
    JSONObject* obj;

    if (!json->getNumberValue2("streamId", &iValue))
    {
        return false;
    }
    timecodeInfo->streamId = (int)iValue;
    if (!json->getNumberValue2("timecodeType", &iValue))
    {
        return false;
    }
    if ((int)iValue < UNKNOWN_TIMECODE_TYPE || (int)iValue > SOURCE_TIMECODE_TYPE)
    {
        return false;
    }
    timecodeInfo->timecodeType = (TimecodeType)iValue;
    if (!json->getNumberValue2("timecodeSubType", &iValue))
    {
        return false;
    }
    if ((int)iValue < NO_TIMECODE_SUBTYPE || (int)iValue > LTC_SOURCE_TIMECODE_SUBTYPE)
    {
        return false;
    }
    timecodeInfo->timecodeSubType = (TimecodeSubType)iValue;

    obj = json->getObjectValue("timecode");
    if (obj == 0)
    {
        return false;
    }
    if (!parse_timecode(obj, &timecodeInfo->timecode))
    {
        return false;
    }

    return true;
}

static bool parse_mark_selection(JSONObject* json, ::FrameInfo* frameInfo, int index)
{
    int64_t iValue;

    if (!json->getNumberValue2("markType", &iValue))
    {
        return false;
    }
    frameInfo->markTypes[index] = (unsigned int)iValue;
    if (!json->getNumberValue2("markTypeMask", &iValue))
    {
        return false;
    }
    frameInfo->markTypeMasks[index] = (unsigned int)iValue;

    return true;
}

static JSONObject* serialize_timecode(const ::Timecode* timecode)
{
    auto_ptr<JSONObject> result(new JSONObject());

    result->setBool("isDropFrame", timecode->isDropFrame);
    result->setNumber("hour", timecode->hour);
    result->setNumber("min", timecode->min);
    result->setNumber("sec", timecode->sec);
    result->setNumber("frame", timecode->frame);

    return result.release();
}

static JSONObject* serialize_timecode_info(const ::TimecodeInfo* timecodeInfo)
{
    auto_ptr<JSONObject> result(new JSONObject());

    result->setNumber("streamId", timecodeInfo->streamId);
    result->setNumber("timecodeType", timecodeInfo->timecodeType);
    result->setNumber("timecodeSubType", timecodeInfo->timecodeSubType);
    result->setObject("timecode", serialize_timecode(&timecodeInfo->timecode));

    return result.release();
}

static JSONObject* serialize_mark_selection(const ::FrameInfo* frameInfo, int index)
{
    auto_ptr<JSONObject> result(new JSONObject());

    result->setNumber("markType", frameInfo->markTypes[index]);
    result->setNumber("markTypeMask", frameInfo->markTypeMasks[index]);

    return result.release();
}



bool ingex::parse_frame_info(JSONObject* json, ::FrameInfo* frameInfo)
{
    bool bValue;
    int64_t iValue;
    vector<JSONValue*>* timecodes;
    vector<JSONValue*>* markSelections;

    if (!json->getNumberValue2("position", &frameInfo->position))
    {
        return false;
    }
    if (!json->getNumberValue2("sourceLength", &frameInfo->sourceLength))
    {
        return false;
    }
    if (!json->getNumberValue2("availableSourceLength", &frameInfo->availableSourceLength))
    {
        return false;
    }
    if (!json->getNumberValue2("startOffset", &frameInfo->startOffset))
    {
        return false;
    }
    if (!json->getNumberValue2("frameCount", &frameInfo->frameCount))
    {
        return false;
    }
    if (!json->getBoolValue2("rateControl", &bValue))
    {
        return false;
    }
    frameInfo->rateControl = bValue;
    if (!json->getBoolValue2("reversePlay", &bValue))
    {
        return false;
    }
    frameInfo->reversePlay = bValue;
    if (!json->getBoolValue2("isRepeat", &bValue))
    {
        return false;
    }
    frameInfo->isRepeat = bValue;
    if (!json->getBoolValue2("muteAudio", &bValue))
    {
        return false;
    }
    frameInfo->muteAudio = bValue;
    if (!json->getBoolValue2("locked", &bValue))
    {
        return false;
    }
    frameInfo->locked = bValue;
    if (!json->getBoolValue2("droppedFrame", &bValue))
    {
        return false;
    }
    frameInfo->droppedFrame = bValue;
    if (!json->getNumberValue2("vtrErrorLevel", &iValue))
    {
        return false;
    }
    frameInfo->vtrErrorLevel = (VTRErrorLevel)iValue;
    if (!json->getBoolValue2("isMarked", &bValue))
    {
        return false;
    }
    frameInfo->isMarked = bValue;
    if (!json->getNumberValue2("vtrErrorCode", &iValue))
    {
        return false;
    }
    frameInfo->vtrErrorCode = iValue;

    frameInfo->numMarkSelections = 0;
    if (json->getArrayValue2("markSelections", &markSelections))
    {
        INGEX_CHECK(markSelections->size() < sizeof(frameInfo->markTypes) / sizeof(unsigned int));

        vector<JSONValue*>::const_iterator iter;
        for (iter = markSelections->begin(); iter != markSelections->end(); iter++)
        {
            JSONObject* obj = dynamic_cast<JSONObject*>(*iter);
            if (obj == 0)
            {
                return false;
            }
            if (!parse_mark_selection(obj, frameInfo, frameInfo->numMarkSelections))
            {
                return false;
            }

            frameInfo->numMarkSelections++;
        }
    }

    frameInfo->numTimecodes = 0;
    if (json->getArrayValue2("timecodes", &timecodes))
    {
        INGEX_CHECK(timecodes->size() < sizeof(frameInfo->timecodes) / sizeof(TimecodeInfo));

        vector<JSONValue*>::const_iterator iter;
        for (iter = timecodes->begin(); iter != timecodes->end(); iter++)
        {
            JSONObject* obj = dynamic_cast<JSONObject*>(*iter);
            if (obj == 0)
            {
                return false;
            }
            if (!parse_timecode_info(obj, &frameInfo->timecodes[frameInfo->numTimecodes]))
            {
                return false;
            }

            frameInfo->numTimecodes++;
        }
    }

    return true;
}

bool ingex::parse_state_event(JSONObject* json, ::MediaPlayerStateEvent* event)
{
    bool bValue;
    int64_t iValue;
    JSONObject* obj;

    if (!json->getBoolValue2("lockedChanged", &bValue))
    {
        return false;
    }
    event->lockedChanged = bValue;
    if (!json->getBoolValue2("locked", &bValue))
    {
        return false;
    }
    event->locked = bValue;
    if (!json->getBoolValue2("playChanged", &bValue))
    {
        return false;
    }
    event->playChanged = bValue;
    if (!json->getBoolValue2("play", &bValue))
    {
        return false;
    }
    event->play = bValue;
    if (!json->getBoolValue2("stopChanged", &bValue))
    {
        return false;
    }
    event->stopChanged = bValue;
    if (!json->getBoolValue2("stop", &bValue))
    {
        return false;
    }
    event->stop = bValue;
    if (!json->getBoolValue2("speedChanged", &bValue))
    {
        return false;
    }
    event->speedChanged = bValue;
    if (!json->getNumberValue2("speed", &iValue))
    {
        return false;
    }
    event->speed = (int)iValue;

    obj = json->getObjectValue("displayedFrameInfo");
    if (obj == 0)
    {
        return false;
    }
    if (!parse_frame_info(obj, &event->displayedFrameInfo))
    {
        return false;
    }

    return true;
}

bool ingex::parse_player_state(JSONObject* json, HTTPPlayerState* state)
{
    int64_t iValue;
    JSONObject* obj;

    if (!json->getBoolValue2("locked", &state->locked))
    {
        return false;
    }
    if (!json->getBoolValue2("play", &state->play))
    {
        return false;
    }
    if (!json->getBoolValue2("stop", &state->stop))
    {
        return false;
    }
    if (!json->getNumberValue2("speed", &iValue))
    {
        return false;
    }
    state->speed = (int)iValue;

    obj = json->getObjectValue("displayedFrame");
    if (obj == 0)
    {
        return false;
    }
    if (!parse_frame_info(obj, &state->displayedFrame))
    {
        return false;
    }

    return true;
}


JSONObject* ingex::serialize_frame_info(const ::FrameInfo* frameInfo)
{
    int i;
    JSONArray* timecodeArray;
    JSONArray* markSelectionArray;
    auto_ptr<JSONObject> result(new JSONObject());

    result->setNumber("position", frameInfo->position);
    result->setNumber("sourceLength", frameInfo->sourceLength);
    result->setNumber("availableSourceLength", frameInfo->availableSourceLength);
    result->setNumber("startOffset", frameInfo->startOffset);
    result->setNumber("frameCount", frameInfo->frameCount);
    result->setBool("rateControl", frameInfo->rateControl);
    result->setBool("reversePlay", frameInfo->reversePlay);
    result->setBool("isRepeat", frameInfo->isRepeat);
    result->setBool("muteAudio", frameInfo->muteAudio);
    result->setBool("locked", frameInfo->locked);
    result->setBool("droppedFrame", frameInfo->droppedFrame);
    result->setNumber("vtrErrorLevel", frameInfo->vtrErrorLevel);
    result->setBool("isMarked", frameInfo->isMarked);
    markSelectionArray = result->setArray("markSelections");
    for (i = 0; i < frameInfo->numMarkSelections; i++)
    {
        markSelectionArray->appendObject(serialize_mark_selection(frameInfo, i));
    }
    result->setNumber("vtrErrorCode", frameInfo->vtrErrorCode);

    timecodeArray = result->setArray("timecodes");
    for (i = 0; i < frameInfo->numTimecodes; i++)
    {
        timecodeArray->appendObject(serialize_timecode_info(&frameInfo->timecodes[i]));
    }

    return result.release();
}

JSONObject* ingex::serialize_state_event(const ::MediaPlayerStateEvent* event)
{
    auto_ptr<JSONObject> result(new JSONObject());

    result->setBool("lockedChanged", event->lockedChanged);
    result->setBool("locked", event->locked);
    result->setBool("playChanged", event->playChanged);
    result->setBool("play", event->play);
    result->setBool("stopChanged", event->stopChanged);
    result->setBool("stop", event->stop);
    result->setBool("speedChanged", event->speedChanged);
    result->setNumber("speed", event->speed);
    result->setObject("displayedFrameInfo", serialize_frame_info(&event->displayedFrameInfo));

    return result.release();
}

JSONObject* ingex::serialize_player_state(const HTTPPlayerState* state)
{
    auto_ptr<JSONObject> result(new JSONObject());

    result->setBool("locked", state->locked);
    result->setBool("play", state->play);
    result->setBool("stop", state->stop);
    result->setNumber("speed", state->speed);
    result->setObject("displayedFrame", serialize_frame_info(&state->displayedFrame));

    return result.release();
}
