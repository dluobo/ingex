/*
 * $Id: Logfile.cpp,v 1.1 2007/09/11 14:08:33 stuart_hc Exp $
 *
 * Class to support ACE logging to a file
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
#include <ace/OS_NS_sys_stat.h>
#include <ace/OS_NS_stdio.h>
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_Thread.h>

#include "Logfile.h"

#include <sstream>
#include <fstream>

#if defined(WIN32)
const char * const delim = "\\";
#else
const char * const delim = "/";
#endif

//static members
std::vector<std::string> Logfile::mPath;
int Logfile::mDebugLevel = 0;
bool Logfile::mLoggingEnabled = false;
#if defined(WIN32)
std::string mPathRoot = "C:";
#else
std::string mPathRoot = "";
#endif

/**
The Init method enables logging to file.  Some applications such as
the workstation don't want logging but some components,
e.g. GatewayManager, will try to log to file.  So by not calling Init,
logging can be prevented.
The path to the log file directory is also cleared, ready to be rebuilt
using AddPathComponent.
So the Init method should also be used if you are changing
the log file directory.
*/
void Logfile::Init()
{
    mLoggingEnabled = true;
    mPath.clear();
}

/**
Set root for path to log file directory.
Does not include path separator so leave blank for unix root.
*/
void Logfile::PathRoot(const char * s)
{
    mPathRoot = s;
}

/**
Add a path component to the sequence that defines the log file directory.
*/
void Logfile::AddPathComponent(const char * s)
{
    mPath.push_back(s);
}

/**
Open a new logfile in the log file directory.
The filename is based on the id paramater with process and thread ids appended
and a suffix of .txt.
*/
void Logfile::Open(const char * id, bool unique)
{
    if (mLoggingEnabled)
    {
    // Create directory
        ACE_stat tmpstat;
        std::string pathname = mPathRoot;
        for (unsigned int i = 0; i < mPath.size(); ++i)
        {
            pathname += delim;
            pathname += mPath[i];
            if (0 == ACE_OS::stat(pathname.c_str(), &tmpstat))
            {
                //cout << "directory " << pathname << " exists" << endl;
            }
            else if( ENOENT == ACE_OS::last_error())
            {
                //cout << "creating directory " << pathname << endl;
                ACE_OS::mkdir(pathname.c_str());
            }
            else
            {
                ACE_OS::perror("stat failed");
            }
        }

    // Send ACE logging to file
        std::ostringstream ss;
        ss << pathname;
        ss << delim;
        ss << id;
        if (unique)
        {
            ss << "_" << ACE_OS::getpid() << "_" << ACE_OS::thr_self();
        }
        ss << ".txt";

        ACE_OSTREAM_TYPE * debugout = new std::ofstream(ss.str().c_str(), std::ios_base::app);
        ACE_LOG_MSG->msg_ostream(debugout, 1); // 1 means Log_Msg takes ownership
        ACE_LOG_MSG->set_flags(ACE_Log_Msg::OSTREAM);
        //ACE_LOG_MSG->clr_flags(ACE_Log_Msg::STDERR);
    }
}

/**
Set the debug level for all logfiles.
Range is 0 - 3; higher values give more output.
*/
void Logfile::DebugLevel(int debuglevel)
{
    if(debuglevel < 0) debuglevel = 0;
    if(debuglevel > 3) debuglevel = 3;
    mDebugLevel = debuglevel;

    switch(debuglevel)
    {
    case 0:
        ACE_LOG_MSG->priority_mask(LM_NOTICE,
            ACE_Log_Msg::PROCESS);
        break;
    case 1:
        ACE_LOG_MSG->priority_mask(LM_NOTICE | LM_ERROR | LM_WARNING,
            ACE_Log_Msg::PROCESS);
        break;
    case 2:
    default:
        ACE_LOG_MSG->priority_mask(LM_NOTICE | LM_ERROR | LM_WARNING | LM_INFO,
            ACE_Log_Msg::PROCESS);
        break;
    case 3:
        ACE_LOG_MSG->priority_mask(LM_NOTICE | LM_ERROR | LM_WARNING | LM_INFO | LM_DEBUG,
            ACE_Log_Msg::PROCESS);
        break;
    }
}

/**
Set the debug level for all logfiles.
*/
int Logfile::DebugLevel()
{
    return mDebugLevel;
}
