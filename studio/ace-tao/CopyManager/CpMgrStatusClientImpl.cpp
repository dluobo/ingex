/*
 * $Id: CpMgrStatusClientImpl.cpp,v 1.1 2007/09/11 14:08:19 stuart_hc Exp $
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

#include <ace/Log_Msg.h>

#include "CpMgrStatusClientImpl.h"

CpMgrStatusClientImpl::CpMgrStatusClientImpl()
: mPid(0)
{
    // Create options for copy process.
	mOptions.command_line (ACE_TEXT_CHAR_TO_TCHAR("./copy.sh"));

	// Get the processwide process manager.
    mPm = ACE_Process_Manager::instance();
}

void CpMgrStatusClientImpl::ProcessStatus (
      const ::ProdAuto::StatusItem & status
  )
  throw (
      ::CORBA::SystemException
  )
{
    ACE_DEBUG(( LM_DEBUG, ACE_TEXT("%C = %C\n"),
        status.name.in(), status.value.in() ));

    if (0 == ACE_OS::strcmp("state", status.name.in()))
    {
        if (0 == ACE_OS::strcmp("stopped", status.value.in()))
        {
            StartCopying();
        }
        else if (0 == ACE_OS::strcmp("recording", status.value.in()))
        {
            StopCopying();
        }
    }
}

void CpMgrStatusClientImpl::StartCopying()
{
    // Stop any existing copy process
    StopCopying();

	// Spawn child process.
	mPid = mPm->spawn(mOptions);
	ACE_DEBUG(( LM_DEBUG, ACE_TEXT("Started process %d\n"), mPid ));
}

void CpMgrStatusClientImpl::StopCopying()
{
	// If we have a process still running, terminate it.
	if (mPid)
	{
        // See if it has exited already
        ACE_Time_Value timeout(0, 10000);
        if (mPm->wait(mPid, timeout) == mPid)
        {
			ACE_DEBUG((LM_DEBUG, "Process %d has exited.\n", mPid));
            mPid = 0;
        }
        else
        {
        // Signal process to terminate
		    if (0 == mPm->terminate(mPid, SIGINT))
		    {
			    ACE_DEBUG((LM_DEBUG, "Sent SIGINT to process %d\n", mPid));
			    mPid = 0;
		    }
            else
		    {
			    ACE_DEBUG((LM_ERROR, "Failed to signal process %d\n", mPid));
		    }
        }
	}
}


