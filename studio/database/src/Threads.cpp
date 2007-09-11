/*
 * $Id: Threads.cpp,v 1.1 2007/09/11 14:08:40 stuart_hc Exp $
 *
 * Support for threading
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
 
#include "Threads.h"
#include "ProdAutoException.h"
#include "Logging.h"


using namespace std;
using namespace prodauto;


// TODO: WIN32 threads


Mutex::Mutex()
{
#if !defined(_WIN32)
    if (pthread_mutex_init(&_pthreadMutex, NULL) != 0)
    {
        PA_LOGTHROW(ProdAutoException, ("Failed to initialise mutex"));
    }
#endif
}

Mutex::~Mutex()
{
#if !defined(_WIN32)
    if (pthread_mutex_destroy(&_pthreadMutex) != 0)
    {
        PA_LOG(error, ("Failed to destroy mutex in Mutex destructor\n"));
    }
#endif
}
    
void Mutex::lock()
{
#if !defined(_WIN32)    
    if (pthread_mutex_lock(&_pthreadMutex) != 0)
    {
        PA_LOGTHROW(ProdAutoException, ("Failed to get a mutex lock"));
    }
#endif
}

void Mutex::unlock()
{
#if !defined(_WIN32)    
    if (pthread_mutex_unlock(&_pthreadMutex) != 0)
    {
        PA_LOGTHROW(ProdAutoException, ("Failed to unlock mutex"));
    }
#endif
}
    
MutexLocker::MutexLocker(Mutex& mutex)
: _mutex(mutex)
{
    mutex.lock();
}

MutexLocker::~MutexLocker()
{
    _mutex.unlock();
}
    


