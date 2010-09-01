/*
 * $Id: LocalInfaxAccess.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * Provides access to the local dump of the Infax database
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "LocalInfaxAccess.h"
#include "Config.h"
#include "Logging.h"
#include "RecorderException.h"

using namespace std;
using namespace rec;



LocalInfaxAccess::LocalInfaxAccess()
{
    _database = new LocalInfaxDatabase(Config::local_infax_db_host,
                                       Config::local_infax_db_name,
                                       Config::local_infax_db_user,
                                       Config::local_infax_db_password);
}

LocalInfaxAccess::~LocalInfaxAccess()
{
}

Source* LocalInfaxAccess::findSource(string barcode)
{
    return _database->loadSource(barcode);
}

