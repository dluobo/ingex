/*
 * $Id: Recorder.cpp,v 1.3 2010/03/29 17:06:52 philipn Exp $
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
    vector<SourceConfig*>::const_iterator iter1;
    for (iter1 = sourceConfigs.begin(); iter1 != sourceConfigs.end(); iter1++)
    {
        delete *iter1;
    }

    vector<RecorderInputConfig*>::const_iterator iter2;
    for (iter2 = recorderInputConfigs.begin(); iter2 != recorderInputConfigs.end(); iter2++)
    {
        delete *iter2;
    }
}

SourceConfig* RecorderConfig::getSourceConfig(long sourceConfigID, uint32_t sourceTrackID)
{
    if (sourceConfigID == 0)
    {
        return 0;
    }
    
    vector<SourceConfig*>::const_iterator iter;
    for (iter = sourceConfigs.begin(); iter != sourceConfigs.end(); iter++)
    {
        if ((*iter)->getID() == sourceConfigID)
        {
            if ((*iter)->getTrackConfig(sourceTrackID) == 0)
            {
                break; // track with id sourceTrackID not present
            }
            return *iter;
        }
    }
    
    return 0;
}
 
SourceConfig* RecorderConfig::getSourceConfig(uint32_t inputIndex, uint32_t inputTrackIndex)
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

RecorderInputConfig* RecorderConfig::getInputConfig(uint32_t index)
{
    vector<RecorderInputConfig*>::const_iterator iter;
    for (iter = recorderInputConfigs.begin(); iter != recorderInputConfigs.end(); iter++)
    {
        if ((*iter)->index == index)
        {
            return *iter;
        }
    }
    
    return 0;
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

    if ((*iter).second.value.compare("true") == 0)
    {
        return true;
    }
    else if ((*iter).second.value.compare("false") == 0)
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
    



Recorder::Recorder()
: DatabaseObject(), _config(0)
{}

Recorder::~Recorder()
{
    vector<RecorderConfig*>::const_iterator iter;
    for (iter = _allConfigs.begin(); iter != _allConfigs.end(); iter++)
    {
        delete *iter;
    }
}

void Recorder::setConfig(RecorderConfig* config)
{
    _config = config;
    
    vector<RecorderConfig*>::const_iterator iter;
    for (iter = _allConfigs.begin(); iter != _allConfigs.end(); iter++)
    {
        if (config == *iter)
        {
            // already in the list
            return;
        }
    }
    _allConfigs.push_back(config);
}

void Recorder::setAlternateConfig(RecorderConfig* config)
{
    vector<RecorderConfig*>::const_iterator iter;
    for (iter = _allConfigs.begin(); iter != _allConfigs.end(); iter++)
    {
        if (config == *iter)
        {
            // already in the list
            return;
        }
    }
    _allConfigs.push_back(config);
}

bool Recorder::hasConfig()
{
    return _config != 0;
}

RecorderConfig* Recorder::getConfig()
{
    return _config;
}

vector<RecorderConfig*>& Recorder::getAllConfigs()
{
    return _allConfigs;
}

