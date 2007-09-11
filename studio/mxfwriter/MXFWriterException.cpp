/*
 * $Id: MXFWriterException.cpp,v 1.1 2007/09/11 14:08:44 stuart_hc Exp $
 *
 * MXF writer exception
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
 
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>


#include "MXFWriterException.h"

using namespace std;
using namespace prodauto;


MXFWriterException::MXFWriterException(const char* format, ...)
: ProdAutoException()
{
    char message[512];
    
    va_list p_arg;
    va_start(p_arg, format);
#if defined(_MSC_VER)
    _vsnprintf(message, 512, format, p_arg);
#else
    vsnprintf(message, 512, format, p_arg);
#endif
    va_end(p_arg);
    
    _message = message;
}


MXFWriterException::~MXFWriterException()
{}

