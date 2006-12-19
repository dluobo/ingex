/*
 * $Id: Take.h,v 1.1 2006/12/19 16:44:53 john_f Exp $
 *
 * An Item take
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
 
#ifndef __PRODAUTO_TAKE_H__
#define __PRODAUTO_TAKE_H__


#include <string>
#include <vector>

#include "DatabaseObject.h"
#include "SourceSession.h"
#include "Package.h"



namespace prodauto
{

class Item;

class Take : public DatabaseObject
{
public:
    friend class Database;
    friend class Item;
    
public:
    virtual ~Take();
    
    long number;
    std::string comment;
    int result;
    Rational editRate;
    int64_t length;
    int64_t startPosition;
    Date startDate;
    long recordingLocation;
    
private:
    // used by Item
    Take();
    void setItem(Item* item); // reference to item

    long getItemID();

    Item* _item;
};


};

#endif

