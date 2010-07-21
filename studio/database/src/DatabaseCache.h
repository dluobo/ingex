/*
 * $Id: DatabaseCache.h,v 1.1 2010/07/21 16:29:34 john_f Exp $
 *
 * Copyright (C) 2010 British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __PRODAUTO_DATABASE_CACHE_H__
#define __PRODAUTO_DATABASE_CACHE_H__

#include <string>
#include <map>

#include "Package.h"



namespace prodauto
{


class DatabaseCache
{
public:
    DatabaseCache();
    ~DatabaseCache();

    std::string GetLiveRecordingLocation(long id);
    long LoadOrCreateLiveRecordingLocation(std::string name);

    ProjectName LoadOrCreateProjectName(std::string name);

private:
    std::map<long, std::string> mLiveRecodingLocations;
    std::vector<ProjectName> mProjectNames;
};


};


#endif
