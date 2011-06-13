/*
 * $Id: CopyManager.cpp,v 1.9 2011/06/13 15:57:51 john_f Exp $
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

#include "CopyManager.h"
#include "TcpPort.h"

#include <ace/Log_Msg.h>
#include <sstream>

CopyManager::CopyManager(CopyMode::EnumType mode)
: mMode(mode), mPid(0)
{
    // Get the processwide process manager.
    mPm = ACE_Process_Manager::instance();
}

void CopyManager::Command(const std::string & command)
{
    switch (mMode)
    {
    case CopyMode::OLD:
        // Store command
        mCommand = command;
        break;

    case CopyMode::NEW:
        // Command not used
        break;
    }
}

void CopyManager::RecorderName(const std::string & s)
{
    // Only applicable to CopyMode::NEW
    mRecorderName = s;
}

void CopyManager::ClearSrcDest()
{
    // Only applicable to CopyMode::NEW
    mArgs.clear();
}

void CopyManager::AddSrcDest(const std::string & src, const std::string & dest, int priority)
{
    if (!src.empty() && !dest.empty())
    {
        std::ostringstream ss;
        switch (mMode)
        {
        case CopyMode::OLD:
            ss << " " << priority << " \"" << src << "\" \"" << dest << "\"";
            mArgs += ss.str();
            break;
        case CopyMode::NEW:
            ss << priority << "\n"
                << src << "\n"
                << dest << "\n";
            mArgs += ss.str();
            break;
        }
    }
}

void CopyManager::StartCopying(unsigned int index)
{
    // Stop any existing copy process
    StopProcess();

    switch (mMode)
    {
    case CopyMode::OLD:
        // Spawn child process.
        if (!mCommand.empty() && !mArgs.empty() && mPm)
        {
            // command plus src/dest args
            std::ostringstream ss;
            ss << mCommand << " " << mRecorderName << " " << index << mArgs;
            mOptions.command_line (ACE_TEXT_CHAR_TO_TCHAR(ss.str().c_str()));
            ACE_DEBUG(( LM_DEBUG, ACE_TEXT("Spawning command \"%s\"\n"), mOptions.command_line_buf() ));
            mPid = mPm->spawn(mOptions);
            ACE_DEBUG(( LM_DEBUG, ACE_TEXT("Started process %d\n"), mPid ));
        }
        break;

    case CopyMode::NEW:
        //if (!mArgs.empty())
        {
            // Create command
            std::ostringstream ss;
            ss << mRecorderName << "\n" << index << "\n" << mArgs;
            // Send it
            TcpPort port;
            if (port.Connect("127.0.0.1:2000"))
            {
                port.Send(ss.str().c_str(), ss.str().size());
            }
            else
            {
                ACE_DEBUG((LM_WARNING, ACE_TEXT("CopyManager failed to connect to xferserver.\n")));
            }
        }
        break;
    }
}

void CopyManager::StopCopying(unsigned int index)
{
    StopProcess();

    switch (mMode)
    {
    case CopyMode::OLD:
        if (!mCommand.empty() && !mArgs.empty() && mPm)
        {
            // command only
            std::ostringstream ss;
            ss << mCommand << " " << mRecorderName << " " << index;
            mOptions.command_line (ACE_TEXT_CHAR_TO_TCHAR(ss.str().c_str()));
            ACE_DEBUG(( LM_DEBUG, ACE_TEXT("Spawning command \"%s\"\n"), mOptions.command_line_buf() ));
            mPid = mPm->spawn(mOptions);
            ACE_DEBUG(( LM_DEBUG, ACE_TEXT("Started process %d\n"), mPid ));
        }
        break;

    case CopyMode::NEW:
        //if (!mArgs.empty())
        {
            // Create command
            std::ostringstream ss;
            ss << mRecorderName << "\n" << index << "\n";
            // Send it
            TcpPort port;
            if (port.Connect("127.0.0.1:2000"))
            {
                port.Send(ss.str().c_str(), ss.str().size());
            }
            else
            {
                ACE_DEBUG((LM_WARNING, ACE_TEXT("CopyManager failed to connect to xferserver.\n")));
            }
        }
        break;
    }
}

void CopyManager::StopProcess()
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
        // Otherwise, signal process to terminate
        else
        {
            if (0 == mPm->terminate(mPid, SIGINT))
            {
                ACE_DEBUG((LM_DEBUG, "Sent SIGINT to process %d\n", mPid));
            }
            else
            {
                ACE_DEBUG((LM_ERROR, "Failed to signal process %d\n", mPid));
            }
            // Try to collect its exit status.
            // Probably too soon but we will try again on next start.
            if (mPm->wait(mPid, ACE_Time_Value(0, 1000)) == mPid)
            {
                ACE_DEBUG((LM_DEBUG, "Process %d has exited.\n", mPid));
                mPid = 0;
            }
        }
    }
}

