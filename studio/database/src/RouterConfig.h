/*
 * $Id: RouterConfig.h,v 1.1 2007/09/11 14:08:39 stuart_hc Exp $
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
 
#ifndef __PRODAUTO_ROUTER_CONFIG_H__
#define __PRODAUTO_ROUTER_CONFIG_H__



#include "DatabaseObject.h"
#include "DataTypes.h"
#include "SourceConfig.h"


namespace prodauto
{

class SourceSession;
class SourcePackage;
    
class RouterInputConfig : public DatabaseObject
{
public:
    RouterInputConfig();
    ~RouterInputConfig();
    
    bool isConnectedToSource() { return sourceConfig != 0; }
    
    uint32_t index;
    std::string name;
    SourceConfig* sourceConfig; // 0 indicates the input is not connected
    uint32_t sourceTrackID;
    
};

class RouterOutputConfig : public DatabaseObject
{
public:
    RouterOutputConfig();
    ~RouterOutputConfig();
    
    uint32_t index;
    std::string name;
};

class RouterConfig : public DatabaseObject
{
public:
    RouterConfig();
    ~RouterConfig();
    
    /* returns the source config and track connected to the router input */
    bool getInputSourceConfig(uint32_t index, SourceConfig** sourceConfig, uint32_t* sourceTrackID);

    /* returns the source package and track in the given session associated with the router input */
    bool getSourcePackage(uint32_t index, SourcePackage** package, uint32_t* sourceTrackID);

    
    /* input and output indexes are 1...num */
    RouterInputConfig* getInputConfig(uint32_t index);
    RouterOutputConfig* getOutputConfig(uint32_t index);


    /* used by Database to check if source config already loaded */
    SourceConfig* getSourceConfig(long sourceConfigID, uint32_t sourceTrackID);
    
    
    std::string name;
    std::vector<RouterInputConfig*> inputConfigs;
    std::vector<RouterOutputConfig*> outputConfigs;
    std::vector<SourceConfig*> sourceConfigs;
};



};









#endif


