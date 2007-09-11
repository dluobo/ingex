/*
 * $Id: CopyManager.cpp,v 1.1 2007/09/11 14:08:32 stuart_hc Exp $
 *
 * Class to manage file copying in a separate process.
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

#include "CopyManager.h"


CopyManager::CopyManager()
: mEnabled(false), mPid(0)
{
    // Get the processwide process manager.
    mPm = ACE_Process_Manager::instance();
}

void CopyManager::Command(const std::string & command)
{
    if (command.empty())
    {
        mEnabled = false;
    }
    else
    {
        // Create options for copy process.
        mEnabled = true;
        mOptions.command_line (ACE_TEXT_CHAR_TO_TCHAR(command.c_str()));
    }
}

void CopyManager::StartCopying()
{
    // Stop any existing copy process
    StopCopying();

    // Spawn child process.
    if (mEnabled && mPm)
    {
        mPid = mPm->spawn(mOptions);
        ACE_DEBUG(( LM_DEBUG, ACE_TEXT("Started process %d\n"), mPid ));
    }
}

void CopyManager::StopCopying()
{
    // If we have a process still running, terminate it.
    if (mPid)
    {
        // See if it has exited already
        if (mPm->wait(mPid, ACE_Time_Value(0, 1000)) == mPid)
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
            }
            else
            {
                ACE_DEBUG((LM_ERROR, "Failed to signal process %d\n", mPid));
            }
	// And try to collect its exit status.
	// Probably too soon but we will try again on next start.
	    if (mPm->wait(mPid, ACE_Time_Value(0, 1000)) == mPid)
	    {
		ACE_DEBUG((LM_DEBUG, "Process %d has exited.\n", mPid));
		mPid = 0;
	    }
        }
    }
}
