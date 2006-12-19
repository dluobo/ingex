/*
 * $Id: Programme.h,v 1.1 2006/12/19 16:44:53 john_f Exp $
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
 
#ifndef __PRODAUTO_PROGRAMME_H__
#define __PRODAUTO_PROGRAMME_H__


#include <string>
#include <vector>

#include "DatabaseObject.h"
#include "Item.h"

namespace prodauto
{

class Series;

class Programme : public DatabaseObject
{
public:
    friend class Database;
    friend class Series;
    friend class Item;
    
public:
    virtual ~Programme();
    
    Item* appendNewItem(); // Item is owned by this Programme
    Item* insertNewItem(size_t position); // Item is owned by this Programme
    void moveItem(Item* item, size_t newPosition);
    
    const std::vector<Item*>& getItems() { return _items; }
    
    std::string name;
    
private:
    // used by Series
    Programme();
    void setSeries(Series* series); // store reference to owner

    // called by Item when it destructs
    void removeItem(Item* item);
    
    void removeAllItems();
    
    long getSeriesID();

    void appendItem(Item* item); // transfers ownership
    void insertItem(Item* item, size_t position);  // transfers ownership
    
    std::vector<Item*> _items;
    Series* _series;
};


};

#endif

