/*
 * $Id: Recorder.cpp,v 1.6 2010/07/14 13:06:36 john_f Exp $
 *
 * Recorder and configuration
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#include <cstdio>
#include <cassert>

#include "Recorder.h"
#include "DatabaseEnums.h"
#include "Utilities.h"
#include "DBException.h"
#include "Logging.h"


using namespace std;
using namespace prodauto;



RecorderInputTrackConfig::RecorderInputTrackConfig()
: DatabaseObject(), index(0), number(0), sourceConfig(0), sourceTrackID(0)
{}

RecorderInputTrackConfig::~RecorderInputTrackConfig()
{}

RecorderInputTrackConfig* RecorderInputTrackConfig::clone(map<SourceConfig*, SourceConfig*> &clonedSourceConfigs)
{
    RecorderInputTrackConfig *config = new RecorderInputTrackConfig();
    config->index = index;
    config->number = number;
    if (sourceConfig)
    {
        map<SourceConfig*, SourceConfig*>::const_iterator result = clonedSourceConfigs.find(sourceConfig);
        if (result == clonedSourceConfigs.end())
        {
            config->sourceConfig = sourceConfig->clone();
            clonedSourceConfigs[sourceConfig] = config->sourceConfig;
        }
        else
        {
            config->sourceConfig = result->second;
        }
    }
    config->sourceTrackID = sourceTrackID;

    DatabaseObject::clone(config);

    return config;
}



RecorderInputConfig::RecorderInputConfig()
: DatabaseObject(), index(0)
{}

RecorderInputConfig::~RecorderInputConfig()
{
    vector<RecorderInputTrackConfig*>::const_iterator iter;
    for (iter = trackConfigs.begin(); iter != trackConfigs.end(); iter++)
    {
        delete *iter;
    }
}

RecorderInputTrackConfig* RecorderInputConfig::getTrackConfig(uint32_t index)
{
    vector<RecorderInputTrackConfig*>::const_iterator iter;
    for (iter = trackConfigs.begin(); iter != trackConfigs.end(); iter++)
    {
        if ((*iter)->index == index)
        {
            return *iter;
        }
    }
    
    return 0;
}

RecorderInputConfig* RecorderInputConfig::clone(map<SourceConfig*, SourceConfig*> &clonedSourceConfigs)
{
    RecorderInputConfig *config = new RecorderInputConfig();
    config->index = index;
    config->name = name;

    size_t i;
    for (i = 0; i < trackConfigs.size(); i++)
    {
        config->trackConfigs.push_back(trackConfigs[i]->clone(clonedSourceConfigs));
    }

    DatabaseObject::clone(config);

    return config;
}



RecorderParameter::RecorderParameter()
: DatabaseObject(), type(0)
{}

RecorderParameter::RecorderParameter(string n, string v, int t)
: DatabaseObject(), name(n), value(v), type(t)
{}

RecorderParameter::RecorderParameter(const RecorderParameter& param)
: DatabaseObject(param), name(param.name), value(param.value), type(param.type)
{}

RecorderParameter::~RecorderParameter()
{}



RecorderConfig::RecorderConfig()
: DatabaseObject()
{}

RecorderConfig::~RecorderConfig()
{
}

bool RecorderConfig::haveParam(string name)
{
    return parameters.find(name) != parameters.end();
}

string RecorderConfig::getParam(string name, string defaultValue)
{
    map<string, RecorderParameter>::iterator iter = parameters.find(name);
    
    if (iter == parameters.end())
    {
        return defaultValue;
    }
    
    return (*iter).second.value;
}

string RecorderConfig::getStringParam(string name, string defaultValue)
{
    return getParam(name, defaultValue);
}

int RecorderConfig::getIntParam(string name, int defaultValue)
{
    map<string, RecorderParameter>::iterator iter = parameters.find(name);
    
    if (iter == parameters.end())
    {
        return defaultValue;
    }
    
    int value;
    if (sscanf((*iter).second.value.c_str(), "%d", &value) != 1)
    {
        Logging::warning("Getting invalid int parameter name '%s', value '%s'\n", 
            name.c_str(),
            (*iter).second.value.c_str());
        return defaultValue;
    }
    
    return value;
}

bool RecorderConfig::getBoolParam(string name, bool defaultValue)
{
    map<string, RecorderParameter>::iterator iter = parameters.find(name);
    
    if (iter == parameters.end())
    {
        return defaultValue;
    }

    if ((*iter).second.value == "true")
    {
        return true;
    }
    else if ((*iter).second.value == "false")
    {
        return false;
    }
    
    Logging::warning("Getting invalid boolean parameter name '%s', value '%s'"
        "; should be either 'true' or 'false'\n",
        name.c_str(),
        (*iter).second.value.c_str());
    return defaultValue;
}

Rational RecorderConfig::getRationalParam(string name, Rational defaultValue)
{
    map<string, RecorderParameter>::iterator iter = parameters.find(name);
    
    if (iter == parameters.end())
    {
        return defaultValue;
    }
    
    long num;
    long denom;
    if (sscanf((*iter).second.value.c_str(), " %ld / %ld ", &num, &denom) != 2)
    {
        Logging::warning("Getting invalid rational parameter name '%s', value '%s'"
            "; should be '<numerator>/<denominator>'\n",
            name.c_str(),
            (*iter).second.value.c_str());
        return defaultValue;
    }
    
    Rational value;
    value.numerator = num;
    value.denominator = denom;
    
    return value;

}

void RecorderConfig::setParam(string name, string value)
{
    if (value.length() >= MAX_PARAMETER_LENGTH)
    {
        PA_LOGTHROW(DBException, ("Parameter exceeds maximum size of %d characters", 
            MAX_PARAMETER_LENGTH));
    }
    
    pair<map<string, RecorderParameter>::iterator, bool> result = 
        parameters.insert(pair<string, RecorderParameter>(
            name, RecorderParameter(name, value, RECORDER_PARAM_TYPE_ANY)));
    if (!result.second)
    {
        parameters.erase(result.first);
        parameters.insert(pair<string, RecorderParameter>(
            name, RecorderParameter(name, value, RECORDER_PARAM_TYPE_ANY)));
    }
}

void RecorderConfig::setStringParam(string name, string value)
{
    setParam(name, value);
}
    
void RecorderConfig::setIntParam(string name, int value)
{
    char buffer[11];
    if (sprintf(buffer, "%d", value) < 0)
    {
        PA_LOGTHROW(DBException, ("Failed to set parameter '%s' with int value",
            name.c_str()));
    }
    
    setParam(name, buffer);
}
    
void RecorderConfig::setBoolParam(string name, bool value)
{
    if (value)
    {
        setParam(name, "true");
    }
    else
    {
        setParam(name, "false");
    }
}
    
void RecorderConfig::setRationalParam(string name, Rational value)
{
    char buffer[24];
    if (sprintf(buffer, "%ld/%ld", (long)value.numerator, (long)value.denominator) < 0)
    {
        PA_LOGTHROW(DBException, ("Failed to set parameter '%s' with rational value",
            name.c_str()));
    }
    
    setParam(name, buffer);
}

RecorderConfig* RecorderConfig::clone()
{
    RecorderConfig *config = new RecorderConfig();
    config->name = name;
    config->parameters = parameters;

    DatabaseObject::clone(config);

    return config;
}



Recorder::Recorder()
: DatabaseObject(), config(0)
{}

Recorder::~Recorder()
{
    delete config;

    size_t i;
    for (i = 0; i < sourceConfigs.size(); i++)
    {
        delete sourceConfigs[i];
    }

    for (i = 0; i < recorderInputConfigs.size(); i++)
    {
        delete recorderInputConfigs[i];
    }
}

SourceConfig* Recorder::getSourceConfig(long sourceConfigID, uint32_t sourceTrackID)
{
    if (sourceConfigID == 0)
    {
        return 0;
    }
    
    size_t i;
    for (i = 0; i < sourceConfigs.size(); i++)
    {
        if (sourceConfigs[i]->getID() == sourceConfigID)
        {
            if (sourceConfigs[i]->getTrackConfig(sourceTrackID) == 0)
            {
                break; // track with id sourceTrackID not present
            }
            return sourceConfigs[i];
        }
    }
    
    return 0;
}
 
SourceConfig* Recorder::getSourceConfig(uint32_t inputIndex, uint32_t inputTrackIndex)
{
    RecorderInputConfig* inputConfig = getInputConfig(inputIndex);
    if (inputConfig == 0)
    {
        return 0;
    }
    
    RecorderInputTrackConfig* trackConfig = inputConfig->getTrackConfig(inputTrackIndex);
    if (trackConfig == 0)
    {
        return 0;
    }
    
    return trackConfig->sourceConfig;
}

RecorderInputConfig* Recorder::getInputConfig(uint32_t index)
{
    size_t i;
    for (i = 0; i < recorderInputConfigs.size(); i++)
    {
        if (recorderInputConfigs[i]->index == index)
        {
            return recorderInputConfigs[i];
        }
    }
    
    return 0;
}

Recorder* Recorder::clone()
{
    Recorder *recorder = new Recorder();
    recorder->name = name;

    PA_ASSERT(config);
    recorder->config = config->clone();

    map<SourceConfig*, SourceConfig*> clonedSourceConfigs;
    size_t i;
    for (i = 0; i < recorderInputConfigs.size(); i++)
    {
        recorder->recorderInputConfigs.push_back(recorderInputConfigs[i]->clone(clonedSourceConfigs));
    }

    map<SourceConfig*, SourceConfig*>::const_iterator iter;
    for (iter = clonedSourceConfigs.begin(); iter != clonedSourceConfigs.end(); iter++)
    {
        recorder->sourceConfigs.push_back(iter->second);
    }

    DatabaseObject::clone(recorder);

    return recorder;
}

