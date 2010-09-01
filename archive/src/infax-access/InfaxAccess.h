/*
 * $Id: InfaxAccess.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
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
 
#ifndef __RECORDER_INFAX_ACCESS_H__
#define __RECORDER_INFAX_ACCESS_H__


#include "DatabaseThings.h"


namespace rec
{


class InfaxAccess
{
public:
    static void initialise();
    static InfaxAccess* getInstance();
    static void close();
    
private:
    static InfaxAccess* _instance;

public:
    virtual ~InfaxAccess() {}
    
    virtual Source* findSource(std::string barcode) = 0;
};

};


#endif

