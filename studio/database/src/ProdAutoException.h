/*
 * $Id: ProdAutoException.h,v 1.4 2010/10/26 18:39:54 john_f Exp $
 *
 * General exception class
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
 
#ifndef __PRODAUTO_PRODAUTOEXCEPTION_H__
#define __PRODAUTO_PRODAUTOEXCEPTION_H__

#include <string>
#include <cstdarg>
#include <cassert>

#include "Logging.h" // for PA_LOGTHROW

#if defined(NDEBUG)
#define PA_ASSERT(cond) \
    if (!(cond)) \
    {\
        PA_LOGTHROW(prodauto::ProdAutoException, ("'%s' assertion failed", #cond)); \
    }
#else
#define PA_ASSERT(cond) \
    assert(cond)
#endif

#define PA_CHECK(cond) \
    if (!(cond)) \
    {\
        PA_LOGTHROW(prodauto::ProdAutoException, ("'%s' check failed", #cond)); \
    }



namespace prodauto
{

class ProdAutoException
{
public:
    ProdAutoException();
    ProdAutoException(const char* format, ...);
    virtual ~ProdAutoException();
    
    std::string getMessage() const;
    
protected:
    std::string _message;
};


};


#endif


