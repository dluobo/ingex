/*
 * $Id: CpMgrStatusClientImpl.h,v 1.1 2007/09/11 14:08:19 stuart_hc Exp $
 *
 * StatusClient implementation for the CopyManager.
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


#ifndef CpMgrStatusClientImpl_h
#define CpMgrStatusClientImpl_h

#include <ace/Process_Manager.h>

#include "StatusClientImpl.h"

class CpMgrStatusClientImpl : public StatusClientImpl
{
public:
    CpMgrStatusClientImpl();

    void ProcessStatus (
        const ::ProdAuto::StatusItem & status
    )
    throw (
        ::CORBA::SystemException
    );

private:
    void StartCopying();
    void StopCopying();

	pid_t mPid;
    ACE_Process_Manager * mPm;
    ACE_Process_Options mOptions;
};

#endif //#ifndef CpMgrStatusClientImpl_h

