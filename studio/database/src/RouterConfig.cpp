/*
 * $Id: RouterConfig.cpp,v 1.1 2007/09/11 14:08:39 stuart_hc Exp $
 *
 * Router configuration
 *
 * Copyright (C) 2007  Philip de Nier <philipn@users.sourceforge.net>
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
 
#include <cassert>

#include "RouterConfig.h"
#include "Utilities.h"
#include "DBException.h"
#include "Logging.h"


using namespace std;
using namespace prodauto;


RouterInputConfig::RouterInputConfig()
: DatabaseObject(), index(0), sourceConfig(0), sourceTrackID(0)
{}

RouterInputConfig::~RouterInputConfig()
{}


RouterOutputConfig::RouterOutputConfig()
: DatabaseObject(), index(0)
{}

RouterOutputConfig::~RouterOutputConfig()
{}


RouterConfig::RouterConfig()
: DatabaseObject()
{}

RouterConfig::~RouterConfig()
{
    vector<SourceConfig*>::const_iterator iter1;
    for (iter1 = sourceConfigs.begin(); iter1 != sourceConfigs.end(); iter1++)
    {
        delete *iter1;
    }

    vector<RouterInputConfig*>::const_iterator iter2;
    for (iter2 = inputConfigs.begin(); iter2 != inputConfigs.end(); iter2++)
    {
        delete *iter2;
    }

    vector<RouterOutputConfig*>::const_iterator iter3;
    for (iter3 = outputConfigs.begin(); iter3 != outputConfigs.end(); iter3++)
    {
        delete *iter3;
    }
}

bool RouterConfig::getInputSourceConfig(uint32_t index, SourceConfig** sourceConfig, 
    uint32_t* sourceTrackID)
{
    RouterInputConfig* inputConfig;
    if ((inputConfig = getInputConfig(index)) == 0)
    {
        // unknown input
        return false;
    }
    
    if (!inputConfig->isConnectedToSource())
    {
        // input is not connected
        return false;
    }
    
    *sourceConfig = inputConfig->sourceConfig;
    *sourceTrackID = inputConfig->sourceTrackID;
    return true;
}

bool RouterConfig::getSourcePackage(uint32_t index, SourcePackage** package,
    uint32_t* sourceTrackID)
{
    SourceConfig* sourceConfig;
    uint32_t sourceConfigTrackID;
    
    if (!getInputSourceConfig(index, &sourceConfig, &sourceConfigTrackID))
    {
        // no source config connected to input
        return false;
    }
    
    *package = sourceConfig->getSourcePackage();
    *sourceTrackID = sourceConfigTrackID;
    return true;
}

RouterInputConfig* RouterConfig::getInputConfig(uint32_t index)
{
    vector<RouterInputConfig*>::const_iterator iter;
    for (iter = inputConfigs.begin(); iter != inputConfigs.end(); iter++)
    {
        if ((*iter)->index == index)
        {
            return *iter;
        }
    }
    
    return 0;
}

RouterOutputConfig* RouterConfig::getOutputConfig(uint32_t index)
{
    vector<RouterOutputConfig*>::const_iterator iter;
    for (iter = outputConfigs.begin(); iter != outputConfigs.end(); iter++)
    {
        if ((*iter)->index == index)
        {
            return *iter;
        }
    }
    
    return 0;
}

SourceConfig* RouterConfig::getSourceConfig(long sourceConfigID, uint32_t sourceTrackID)
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
    

