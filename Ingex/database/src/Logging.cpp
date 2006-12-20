/*
 * $Id: Logging.cpp,v 1.1 2006/12/20 14:37:02 john_f Exp $
 *
 * Logging facility
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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "Logging.h"


using namespace std;
using namespace prodauto;


std::auto_ptr<Logging> Logging::_instance(0);
Mutex Logging::_loggingMutex = Mutex();


void Logging::initialise(Logging* logging)
{
    LOCK_SECTION(_loggingMutex);
    
    if (_instance.get() != 0)
    {
        delete _instance.release();
    }
    _instance = auto_ptr<Logging>(logging);
}

void Logging::initialise(LogLevel level)
{
    LOCK_SECTION(_loggingMutex);
    
    if (_instance.get() != 0)
    {
        delete _instance.release();
    }
    _instance = auto_ptr<Logging>(new Logging(level));
}

void Logging::error(const char* format, ...)
{
    LOCK_SECTION(_loggingMutex);
    Logging* instance = getInstance();
    
    va_list ap;
    va_start(ap, format);
    instance->iverror(format, ap);
    va_end(ap);
}

void Logging::warning(const char* format, ...)
{
    LOCK_SECTION(_loggingMutex);
    Logging* instance = getInstance();
    
    if (instance->_level < LOG_LEVEL_WARNING)
    {
        return;
    }
    
    va_list ap;
    va_start(ap, format);
    instance->ivwarning(format, ap);
    va_end(ap);
}

void Logging::info(const char* format, ...)
{
    LOCK_SECTION(_loggingMutex);
    Logging* instance = getInstance();
    
    if (instance->_level < LOG_LEVEL_INFO)
    {
        return;
    }
    
    va_list ap;
    va_start(ap, format);
    instance->ivinfo(format, ap);
    va_end(ap);
}

void Logging::debug(const char* format, ...)
{
    LOCK_SECTION(_loggingMutex);
    Logging* instance = getInstance();
    
    if (instance->_level < LOG_LEVEL_DEBUG)
    {
        return;
    }
    
    va_list ap;
    va_start(ap, format);
    instance->ivdebug(format, ap);
    va_end(ap);
}

void Logging::verror(const char* format, va_list ap)
{
    LOCK_SECTION(_loggingMutex);
    Logging* instance = getInstance();
    
    instance->iverror(format, ap);
}

void Logging::vwarning(const char* format, va_list ap)
{
    LOCK_SECTION(_loggingMutex);
    Logging* instance = getInstance();
    
    if (instance->_level < LOG_LEVEL_WARNING)
    {
        return;
    }
    
    instance->ivwarning(format, ap);
}

void Logging::vinfo(const char* format, va_list ap)
{
    LOCK_SECTION(_loggingMutex);
    Logging* instance = getInstance();
    
    if (instance->_level < LOG_LEVEL_INFO)
    {
        return;
    }
    
    instance->ivinfo(format, ap);
}

void Logging::vdebug(const char* format, va_list ap)
{
    LOCK_SECTION(_loggingMutex);
    Logging* instance = getInstance();
    
    if (instance->_level < LOG_LEVEL_DEBUG)
    {
        return;
    }
    
    instance->ivdebug(format, ap);
}

Logging* Logging::getInstance()
{
    if (_instance.get() == 0)
    {
        _instance = auto_ptr<Logging>(new Logging(LOG_LEVEL_WARNING));
    }
    
    return _instance.get();
}



Logging::Logging(LogLevel level)
: _level(level)
{}

Logging::~Logging()
{}

void Logging::setLogLevel(LogLevel level)
{
    _level = level;
}

void Logging::iverror(const char* format, va_list ap)
{
    vfprintf(stderr, format, ap);
}

void Logging::ivwarning(const char* format, va_list ap)
{
    vfprintf(stdout, format, ap);
}

void Logging::ivinfo(const char* format, va_list ap)
{
    vfprintf(stdout, format, ap);
}

void Logging::ivdebug(const char* format, va_list ap)
{
    vfprintf(stdout, format, ap);
}
    

