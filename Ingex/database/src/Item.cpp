/*
 * $Id: Item.cpp,v 1.1 2006/12/20 14:37:02 john_f Exp $
 *
 * A programme item
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

#include "Item.h"
#include "Programme.h"
#include "Utilities.h"
#include "DBException.h"
#include "Logging.h"


using namespace std;
using namespace prodauto;



Item::Item()
: DatabaseObject(), _programme(0), _orderIndex(0), _prevOrderIndex(0)
{}

Item::~Item()
{
    removeAllTakes();

    if (_programme != 0)
    {
        _programme->removeItem(this);
    }
}

Take* Item::createTake()
{
    Take* take = new Take();
    _takes.push_back(take);
    take->setItem(this);
    
    return take;
}
    
void Item::wasCommitted(long databaseID)
{
    _prevOrderIndex = _orderIndex;
    DatabaseObject::wasCommitted(databaseID);
}


void Item::setProgramme(Programme* programme)
{
    if (programme != 0 && _programme != 0)
    {
        _programme->removeItem(this);
    }
    _programme = programme;
}

void Item::removeTake(Take* take)
{
    vector<Take*>::iterator iter;
    for (iter = _takes.begin(); iter != _takes.end(); iter++)
    {
        if (*iter == take)
        {
            _takes.erase(iter);
            take->setItem(0);
            return;
        }
    }
    
    PA_LOGTHROW(DBException, ("Take cannot be removed from item it doesn't belong to"));
}

void Item::removeAllTakes()
{
    while (_takes.begin() != _takes.end())
    {
        delete *_takes.begin(); // side-effect is that take is erased from vector
    }
}

long Item::getProgrammeID()
{
    if (_programme != 0)
    {
        return _programme->getDatabaseID();
    }
    PA_LOGTHROW(DBException, ("Item does not belong to programme so programme ID is unknown"));
}

void Item::setOrderIndex(long value)
{
    _orderIndex = value;
}

void Item::setDBOrderIndex(long value)
{
    _orderIndex = value;
    _prevOrderIndex = value;
}

long Item::getOrderIndex()
{
    return _orderIndex;
}

