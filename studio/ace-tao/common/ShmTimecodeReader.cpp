/*
 * $Id: ShmTimecodeReader.cpp,v 1.4 2010/06/02 13:09:53 john_f Exp $
 *
 * Timecode reader using Ingex shared memory as source.
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
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
#include "IngexShm.h"
#include "Timecode.h"
#include "ShmTimecodeReader.h"

ShmTimecodeReader::ShmTimecodeReader()
{
    // Instantiate IngexShm class
    IngexShm::Instance();
    /*
    if (0 == IngexShm::Instance()->Channels())
    {
        IngexShm::Instance()->Init();
    }
    */
}

/*
void ShmTimecodeReader::TcMode(TcEnum tc_mode)
{
    switch (tc_mode)
    {
    case VITC:
        IngexShm::Instance()->TcMode(IngexShm::VITC);
        break;
    case LTC:
    default:
        IngexShm::Instance()->TcMode(IngexShm::LTC);
        break;
    }
}
*/

std::string ShmTimecodeReader::Timecode()
{
    std::string timecode;

    if (IngexShm::Instance()->Channels())
    {
        Ingex::Timecode tc = IngexShm::Instance()->CurrentTimecode(0); // using first channel
        timecode = tc.Text();
    }
    return timecode;
}

