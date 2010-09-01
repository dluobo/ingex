/*
 * $Id: InfaxAccess.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * Provides access to the Infax database
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

#include "InfaxAccess.h"
#include "LocalInfaxAccess.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"

using namespace std;
using namespace rec;



InfaxAccess* InfaxAccess::_instance = 0;

void InfaxAccess::initialise()
{
    REC_ASSERT(!_instance);

    // TODO: support local access only at the moment
    _instance = new LocalInfaxAccess();
}

InfaxAccess* InfaxAccess::getInstance()
{
    REC_ASSERT(_instance);

    return _instance;
}

void InfaxAccess::close()
{
    SAFE_DELETE(_instance);
}



