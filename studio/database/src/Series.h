/*
 * $Id: Series.h,v 1.1 2007/09/11 14:08:39 stuart_hc Exp $
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
 
#ifndef __PRODAUTO_SERIES_H__
#define __PRODAUTO_SERIES_H__


#include <string>
#include <vector>

#include "DatabaseObject.h"
#include "Programme.h"

namespace prodauto
{

class Series : public DatabaseObject
{
public:
    friend class Database;
    friend class Programme;
    
public:
    Series();
    virtual ~Series();

    // returned Programme is owned by this Series
    Programme* createProgramme();
    
    const std::vector<Programme*>& getProgrammes() { return _programmes; }
    
    std::string name;
    
private:
    // called by Programme when it destructs
    void removeProgramme(Programme* programme);

    void removeAllProgrammes();
    
    std::vector<Programme*> _programmes;
};



};

#endif

