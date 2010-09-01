/*
 * $Id: MXFCommon.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
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

#include <libMXF++/MXF.h>

#include "MXFCommon.h"
#include "Logging.h"

using namespace rec;
using namespace mxfpp;



static void mxf_redirect_vlogging(MXFLogLevel level, const char* format, va_list ap)
{
    switch (level)
    {
        case MXF_ELOG:
            Logging::error("libMXF: ");
            Logging::verrorMore(false, format, ap);
            break;
        case MXF_WLOG:
            Logging::warning("libMXF: ");
            Logging::vwarningMore(false, format, ap);
            break;
        case MXF_ILOG:
            Logging::info("libMXF: ");
            Logging::vinfoMore(false, format, ap);
            break;
        case MXF_DLOG:
            Logging::debug("libMXF: ");
            Logging::vdebugMore(false, format, ap);
    }
}

static void mxf_redirect_logging(MXFLogLevel level, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    mxf_redirect_vlogging(level, format, ap);
    va_end(ap);
}




void rec::redirect_mxf_logging()
{
    LogLevel logLevel = Logging::getInstance()->getLogLevel();
    
    switch (logLevel)
    {
        case LOG_LEVEL_ERROR:
            g_mxfLogLevel = MXF_ELOG;
            break;
        case LOG_LEVEL_WARNING:
            g_mxfLogLevel = MXF_WLOG;
            break;
        case LOG_LEVEL_INFO:
            g_mxfLogLevel = MXF_ILOG;
            break;
        case LOG_LEVEL_DEBUG:
            g_mxfLogLevel = MXF_DLOG;
            break;
    }
    
    mxf_vlog = mxf_redirect_vlogging;
    mxf_log = mxf_redirect_logging;
}

void rec::register_archive_extensions(DataModel *data_model)
{
#define MXF_LABEL(d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13, d14, d15) \
    {d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13, d14, d15}

#define MXF_SET_DEFINITION(parentName, name, label) \
    data_model->registerSetDef(#name, &MXF_SET_K(parentName), &MXF_SET_K(name));
    
#define MXF_ITEM_DEFINITION(setName, name, label, tag, typeId, isRequired) \
    data_model->registerItemDef(#name, &MXF_SET_K(setName), &MXF_ITEM_K(setName, name), tag, typeId, isRequired);

#include <bbc_archive_extensions_data_model.h>

    data_model->finalise();
}

