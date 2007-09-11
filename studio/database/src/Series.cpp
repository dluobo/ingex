/*
 * $Id: Series.cpp,v 1.1 2007/09/11 14:08:39 stuart_hc Exp $
 *
 * A Series
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

#include "Series.h"
#include "Utilities.h"
#include "DBException.h"
#include "Logging.h"


using namespace std;
using namespace prodauto;



Series::Series()
: DatabaseObject()
{}

Series::~Series()
{
    removeAllProgrammes();
}

Programme* Series::createProgramme()
{
    Programme* programme = new Programme();
    _programmes.push_back(programme);
    programme->setSeries(this);
    
    return programme;
}

void Series::removeProgramme(Programme* programme)
{
    vector<Programme*>::iterator iter;
    for (iter = _programmes.begin(); iter != _programmes.end(); iter++)
    {
        if (*iter == programme)
        {
            _programmes.erase(iter);
            programme->setSeries(0);
            return;
        }
    }

    PA_LOGTHROW(DBException, ("Programme cannot be removed from series it doesn't belong to"));
}

void Series::removeAllProgrammes()
{
    while (_programmes.begin() != _programmes.end())
    {
        delete *_programmes.begin(); // side-effect is that programme is erased from vector
    }
}


