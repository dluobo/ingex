/*
 * $Id: MXFUtils.cpp,v 1.1 2006/12/20 14:51:07 john_f Exp $
 *
 * General MXF utilities
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
 
#include <cstdarg>
#include <cassert>

#include "MXFUtils.h"
#include <Logging.h>

extern "C"
{
    #include <mxf/mxf_logging.h>
}

using namespace std;
using namespace prodauto;



static void mxfLogging(MXFLogLevel level, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    
    switch (level)
    {
        case MXF_ELOG:
            Logging::error("libMXF: ");
            Logging::verror(format, ap);
            break;
        case MXF_WLOG:
            Logging::warning("libMXF: ");
            Logging::vwarning(format, ap);
            break;
        case MXF_ILOG:
            Logging::info("libMXF: ");
            Logging::vinfo(format, ap);
            break;
        default: // MXF_DLOG
            Logging::debug("libMXF: ");
            Logging::vdebug(format, ap);
    }

    va_end(ap);
}



void prodauto::connectLibMXFLogging()
{
    g_mxfLogLevel = MXF_DLOG;
    mxf_log = mxfLogging;
}
