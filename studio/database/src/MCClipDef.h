/*
 * $Id: MCClipDef.h,v 1.1 2007/09/11 14:08:39 stuart_hc Exp $
 *
 * A multi-camera clip definition
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
 
#ifndef __PRODAUTO_MCCLIPDEF_H__
#define __PRODAUTO_MCCLIPDEF_H__


#include <string>
#include <vector>
#include <map>

#include "DatabaseObject.h"
#include "DataTypes.h"
#include "SourceConfig.h"


namespace prodauto
{

class MCSelectorDef : public DatabaseObject
{
public:
    MCSelectorDef();
    ~MCSelectorDef();

    bool isConnectedToSource() { return sourceConfig != 0; }

    uint32_t index;
    SourceConfig* sourceConfig; // can be 0
    uint32_t sourceTrackID;
};


class MCTrackDef : public DatabaseObject
{
public:
    MCTrackDef();
    ~MCTrackDef();
    
    uint32_t index;
    uint32_t number;
    std::map<uint32_t, MCSelectorDef*> selectorDefs;
    
};

    
class MCClipDef : public DatabaseObject
{
public:
    MCClipDef();
    ~MCClipDef();
    
    SourceConfig* getSourceConfig(long sourceConfigID, uint32_t sourceTrackID);
    
    std::string name;
    std::vector<SourceConfig*> sourceConfigs;
    std::map<uint32_t, MCTrackDef*> trackDefs;
};




};














#endif

