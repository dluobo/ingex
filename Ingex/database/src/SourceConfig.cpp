/*
 * $Id: SourceConfig.cpp,v 1.1 2006/12/20 14:37:03 john_f Exp $
 *
 * Live recording or tape Source configuration
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
 
#include <cassert>

#include "SourceConfig.h"
#include "Utilities.h"
#include "DBException.h"


using namespace std;
using namespace prodauto;


SourceTrackConfig::SourceTrackConfig()
: DatabaseObject(), id(0), number(0), dataDef(0), editRate(g_palEditRate), length(0)
{}

SourceTrackConfig::~SourceTrackConfig()
{}


SourceConfig::SourceConfig()
: DatabaseObject(), type(0), recordingLocation(0)
{}

SourceConfig::~SourceConfig()
{
    vector<SourceTrackConfig*>::const_iterator iter;
    for (iter = trackConfigs.begin(); iter != trackConfigs.end(); iter++)
    {
        delete *iter;
    }
}
    
SourceTrackConfig* SourceConfig::getTrackConfig(uint32_t id)
{
    vector<SourceTrackConfig*>::const_iterator iter;
    for (iter = trackConfigs.begin(); iter != trackConfigs.end(); iter++)
    {
        if ((*iter)->id == id)
        {
            return *iter;
        }
    }
    
    return 0;
}

