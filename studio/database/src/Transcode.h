/*
 * $Id: Transcode.h,v 1.1 2007/09/11 14:08:40 stuart_hc Exp $
 *
 * A transcode work item
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
 
#ifndef __PRODAUTO_TRANSCODE_H__
#define __PRODAUTO_TRANSCODE_H__


#include <string>

#include "DatabaseObject.h"
#include "DataTypes.h"


namespace prodauto
{

class Transcode : public DatabaseObject
{
public:
    Transcode();
    virtual ~Transcode();
    
    virtual std::string toString();

    long sourceMaterialPackageDbId;
    long destMaterialPackageDbId;
    Timestamp created; // set by the database
    int targetVideoResolution;
    int status;
};
    
    
};





#endif

