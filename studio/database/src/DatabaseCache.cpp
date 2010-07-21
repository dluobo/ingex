/*
 * $Id: DatabaseCache.cpp,v 1.1 2010/07/21 16:29:34 john_f Exp $
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

#include <memory>

#include "DatabaseCache.h"
#include "Database.h"
#include "ProdAutoException.h"
#include "Logging.h"

using namespace std;
using namespace prodauto;



DatabaseCache::DatabaseCache()
{
    Database *database = Database::getInstance();
    auto_ptr<Transaction> transaction(database->getTransaction("Database cache init"));

    mLiveRecodingLocations = database->loadLiveRecordingLocations(transaction.get());
    mProjectNames = database->loadProjectNames(transaction.get());
}

DatabaseCache::~DatabaseCache()
{
}

string DatabaseCache::GetLiveRecordingLocation(long id)
{
    map<long, string>::const_iterator result = mLiveRecodingLocations.find(id);
    if (result == mLiveRecodingLocations.end())
        PA_LOGTHROW(ProdAutoException, ("Recording location %ld does not exist in database cache", id));

    return result->second;
}

long DatabaseCache::LoadOrCreateLiveRecordingLocation(string name)
{
    map<long, string>::const_iterator iter;
    for (iter = mLiveRecodingLocations.begin(); iter != mLiveRecodingLocations.end(); iter++) {
        if (iter->second == name)
            return iter->first;
    }

    long id = Database::getInstance()->saveLiveRecordingLocation(name);
    mLiveRecodingLocations[id] = name;

    return id;
}

ProjectName DatabaseCache::LoadOrCreateProjectName(string name)
{
    size_t i;
    for (i = 0; i < mProjectNames.size(); i++) {
        if (name == mProjectNames[i].name)
            return mProjectNames[i];
    }

    ProjectName project_name = Database::getInstance()->loadOrCreateProjectName(name);
    mProjectNames.push_back(project_name);

    return project_name;
}

