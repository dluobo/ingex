/*
 * $Id: CopyManager.h,v 1.1 2007/09/11 14:08:32 stuart_hc Exp $
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

#ifndef CopyManager_h
#define CopyManager_h

#include <ace/Process_Manager.h>
#include <string>


class CopyManager
{
public:
    CopyManager();
    void Command(const std::string & command);
    void StartCopying();
    void StopCopying();

private:
    bool mEnabled;
	pid_t mPid;
    ACE_Process_Manager * mPm;
    ACE_Process_Options mOptions;
};

#endif //#ifndef CopyManager_h

