/*
 * $Id: Programme.cpp,v 1.1 2006/12/20 14:37:02 john_f Exp $
 *
 * A Programme
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

#include "Programme.h"
#include "Series.h"
#include "Utilities.h"
#include "DBException.h"
#include "Logging.h"


using namespace std;
using namespace prodauto;


// allows minimum 8 inserts before all item order indexes are updated
#define INITIAL_ITEM_ORDER_INDEX_GAP     256



Programme::Programme()
: DatabaseObject(), _series(0)
{}

Programme::~Programme()
{
    removeAllItems();
    
    if (_series != 0)
    {
        _series->removeProgramme(this);
    }
}

Item* Programme::appendNewItem()
{
    auto_ptr<Item> item(new Item());
    appendItem(item.get());
    return item.release();
}

Item* Programme::insertNewItem(size_t position)
{
    auto_ptr<Item> item(new Item());
    insertItem(item.get(), position);
    return item.release();
}

void Programme::moveItem(Item* item, size_t newPosition)
{
    // find the item and erase it from he vector
    size_t itemPosition = 0;
    vector<Item*>::iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        if (*iter == item)
        {
            break;
        }
        itemPosition++;
    }
    if (iter == _items.end())
    {
        PA_LOGTHROW(DBException, ("Item for move is unknown"));
    }
    if (newPosition == itemPosition)
    {
        // no move required
        return;
    }
    _items.erase(iter);
    item->setOrderIndex(0); // invalidate the order index
    item->setProgramme(0);
    
    // reinsert the item
    if (newPosition > itemPosition)
    {
        insertItem(item, newPosition - 1);
    }
    else
    {
        insertItem(item, newPosition);
    }
}

void Programme::appendItem(Item* item)
{
    if ((_items.size() + 1) * INITIAL_ITEM_ORDER_INDEX_GAP > 2147483647 /* 2^31 - 1 */)
    {
        PA_LOGTHROW(DBException, ("Programme has too many items"));
    }
    
    if (_items.size() == 0)
    {
        item->setOrderIndex(INITIAL_ITEM_ORDER_INDEX_GAP);
    }
    else
    {
        // check that bounds are not exceeded
        if ((_items.back()->getOrderIndex() + INITIAL_ITEM_ORDER_INDEX_GAP) > 2147483647 /* 2^31 - 1*/)
        {
            // update all item order indexes when bounds is exceeded
            long newIndex = INITIAL_ITEM_ORDER_INDEX_GAP;
            vector<Item*>::iterator itemIter;
            for (itemIter = _items.begin(); itemIter != _items.end(); itemIter++)
            {
                (*itemIter)->setOrderIndex(newIndex);
                newIndex += INITIAL_ITEM_ORDER_INDEX_GAP;
            }
        }

        item->setOrderIndex(_items.back()->getOrderIndex() + INITIAL_ITEM_ORDER_INDEX_GAP);
    }

    _items.push_back(item);
    item->setProgramme(this);
}

void Programme::insertItem(Item* item, size_t position)
{
    if ((_items.size() + 1) * INITIAL_ITEM_ORDER_INDEX_GAP > 2147483647 /* 2^31 - 1 */)
    {
        PA_LOGTHROW(DBException, ("Programme has too many items"));
    }
    if (position >= _items.size())
    {
        PA_LOGTHROW(DBException, ("Item position %d is out of range 0..%d", position, _items.size() - 1));
    }

    // append if insert point is at end
    if (position == _items.size() - 1)
    {
        return appendItem(item);
    }
    
    // get the iterator after the insertion point
    vector<Item*>::iterator afterIter = _items.begin();
    size_t count = position;
    while (count > 0)
    {
        afterIter++;
        count--;
    }
    
    // create and insert new item
    vector<Item*>::iterator itemIter = _items.insert(afterIter, item);
    item->setProgramme(this);

    
    // update the order index(es)
    
    vector<Item*>::iterator beforeIter;

    afterIter = itemIter;
    afterIter++;
    beforeIter = itemIter;
    beforeIter--;
    
    if (afterIter == _items.end())
    {
        (*itemIter)->setOrderIndex((*beforeIter)->getOrderIndex() + INITIAL_ITEM_ORDER_INDEX_GAP);
    }
    else if (itemIter == _items.begin())
    {
        // try new index at halfway between 0 and the first item
        long newIndex = (*afterIter)->getOrderIndex() / 2;
        if (newIndex == 0 || newIndex >= (*afterIter)->getOrderIndex())
        {
            // update all item order indexes
            newIndex = INITIAL_ITEM_ORDER_INDEX_GAP;
            for (itemIter = _items.begin(); itemIter != _items.end(); itemIter++)
            {
                (*itemIter)->setOrderIndex(newIndex);
                newIndex += INITIAL_ITEM_ORDER_INDEX_GAP;
            }
        }
        else
        {
            (*itemIter)->setOrderIndex(newIndex);
        }
    }
    else
    {
        // try new index at halfway point
        long newIndex = ((*beforeIter)->getOrderIndex() + (*afterIter)->getOrderIndex()) / 2;
        if (newIndex <= (*beforeIter)->getOrderIndex() || newIndex >= (*afterIter)->getOrderIndex())
        {
            // update all item order indexes
            newIndex = INITIAL_ITEM_ORDER_INDEX_GAP;
            for (itemIter = _items.begin(); itemIter != _items.end(); itemIter++)
            {
                (*itemIter)->setOrderIndex(newIndex);
                newIndex += INITIAL_ITEM_ORDER_INDEX_GAP;
            }
        }
        else
        {
            (*itemIter)->setOrderIndex(newIndex);
        }
    }
    
}



void Programme::setSeries(Series* series)
{
    if (series != 0 && _series != 0)
    {
        _series->removeProgramme(this);
    }
    _series = series;
}

void Programme::removeItem(Item* item)
{
    vector<Item*>::iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        if (*iter == item)
        {
            _items.erase(iter);
            item->setProgramme(0);
            return;
        }
    }
    
    PA_LOGTHROW(DBException, ("Item cannot be removed from programme it doesn't belong to"));
}

void Programme::removeAllItems()
{
    while (_items.begin() != _items.end())
    {
        delete *_items.begin(); // side-effect is that item is erased from vector
    }
}

long Programme::getSeriesID()
{
    if (_series != 0)
    {
        return _series->getDatabaseID();
    }
    PA_LOGTHROW(DBException, ("Programme does not belong to series so series ID is unknown"));
}



