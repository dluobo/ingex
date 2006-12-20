/*
 * $Id: App.h,v 1.1 2006/12/20 12:28:28 john_f Exp $
 *
 * Base class for application control.
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef App_h
#define App_h

class App
{
public:
// Application control methods
    virtual bool Init(int argc, char * argv[]) = 0;
    virtual void Run() = 0;
    virtual void Clean() = 0;
    virtual void Stop() = 0;
};

#endif //#ifndef App_h
