/*
 * $Id: Transcode.cpp,v 1.1 2007/09/11 14:08:40 stuart_hc Exp $
 *
 * A transcode work item
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
#include <sstream>

#include "Transcode.h"
#include "Utilities.h"
#include "DBException.h"


using namespace std;
using namespace prodauto;


Transcode::Transcode()
: DatabaseObject(), sourceMaterialPackageDbId(0), destMaterialPackageDbId(0), 
created(g_nullTimestamp), targetVideoResolution(0), status(0)
{}

Transcode::~Transcode()
{}

string Transcode::toString()
{
    stringstream transcodeStr;
    
    transcodeStr << "Source material package database id = " << sourceMaterialPackageDbId << endl;
    transcodeStr << "Destination material package database id = " << destMaterialPackageDbId << endl;
    transcodeStr << "Created = " << getTimestampString(created) << endl;
    transcodeStr << "Target video resolution = " << targetVideoResolution << endl;
    transcodeStr << "Status = " << status << endl;
    
    return transcodeStr.str();
}



