/*
 * $Id: Package.h,v 1.1 2006/12/20 12:28:29 john_f Exp $
 *
 * Container for arbitrary data.
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

#ifndef Package_h
#define Package_h

#include <tao/corba.h>
#include "StatusClientC.h"

/**
Base class for generic data for passing to a Forwarder.
See R&D Tech Note no. 2467.
*/
class Package
{
    public:
        virtual ~Package(void);
        bool IsEmpty() { return empty_;};
        
    protected:
        Package(void);
        Package(bool empty);
    private:
        bool empty_;
};

/**
Status data for passing to a StatusForwarder.
*/
class StatusPackage : public Package
{
    public:
        StatusPackage();
        StatusPackage(const ProdAuto::StatusItem & status);
        virtual ~StatusPackage(void);
        const ProdAuto::StatusItem & status(void) const;

    private:
        ProdAuto::StatusItem status_;
};

        
#endif  //#ifndef Package_h
