/*
 * $Id: SourceConfig.h,v 1.1 2007/09/11 14:08:39 stuart_hc Exp $
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
 
#ifndef __PRODAUTO_SOURCECONFIG_H__
#define __PRODAUTO_SOURCECONFIG_H__


#include <string>
#include <vector>

#include "DatabaseObject.h"
#include "DataTypes.h"
#include "Package.h"



namespace prodauto
{

class SourceTrackConfig : public DatabaseObject
{
public:
    SourceTrackConfig();
    ~SourceTrackConfig();

    uint32_t id;
    uint32_t number;
    std::string name;
    int dataDef;
    Rational editRate;
    int64_t length;
};


class SourceConfig : public DatabaseObject
{
public:
    SourceConfig();
    ~SourceConfig();
    
    long getID() { return getDatabaseID(); }
    
    SourceTrackConfig* getTrackConfig(uint32_t id);

    void setSessionSourcePackage();
    void setSourcePackage(std::string name);
    SourcePackage* getSourcePackage();    
    
    std::string name;
    int type;
    std::string spoolNumber;
    long recordingLocation;
    std::vector<SourceTrackConfig*> trackConfigs;
    
private:    
    SourcePackage* _sourcePackage;
};




};


#endif


