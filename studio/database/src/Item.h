/*
 * $Id: Item.h,v 1.1 2007/09/11 14:08:39 stuart_hc Exp $
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
 
#ifndef __PRODAUTO_ITEM_H__
#define __PRODAUTO_ITEM_H__


#include <string>
#include <vector>

#include "DatabaseObject.h"
#include "Take.h"

namespace prodauto
{

class Programme;

class Item : public DatabaseObject
{
public:
    friend class Database;
    friend class Programme;
    friend class Take;
    
public:
    virtual ~Item();
    
    Take* createTake(); // Take owned by this Item
    
    const std::vector<Take*>& getTakes() { return _takes; }
    
    std::string description;
    std::vector<std::string> scriptSectionRefs;
    
    virtual void wasCommitted(long databaseID);

private:
    // used by Programme
    Item();
    void setProgramme(Programme* programme); // stores reference to owner

    // called by Take when it destructs
    void removeTake(Take* take);
    
    void removeAllTakes();
    
    long getProgrammeID();
    
    void setOrderIndex(long value);
    long getOrderIndex();
    void setDBOrderIndex(long value);

    std::vector<Take*> _takes;
    Programme* _programme;

    long _orderIndex;
    long _prevOrderIndex;
};


};

#endif

