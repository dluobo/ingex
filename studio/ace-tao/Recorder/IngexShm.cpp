/*
 * $Id: IngexShm.cpp,v 1.4 2008/04/18 16:15:31 john_f Exp $
 *
 * Interface for reading audio/video data from shared memory.
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

#include <ace/OS_NS_unistd.h>
#include <ace/Time_Value.h>
#include <ace/OS_NS_sys_shm.h>
#include <ace/Log_Msg.h>

#include "IngexShm.h"

// static instance pointer
IngexShm * IngexShm::mInstance = 0;

IngexShm::IngexShm(void)
: mChannels(0), mAudioTracksPerChannel(0), mTcMode(VITC)
{
}

IngexShm::~IngexShm(void)
{
}

/**
Attach to shared memory.
@return The number of buffers (channels) attached to.
*/
int IngexShm::Init()
{
// The kind of shared memory used by Ingex recorder daemon is not available on Windows
// but with the settings below the code will still compile.
#ifdef WIN32
    const key_t control_shm_key = "control";
    const key_t channel_shm_key[MAX_CHANNELS] = { "channel0", "channel1", "channel2", "channel3" };
    const int shm_flags = 0;
#else
    const key_t control_shm_key = 9;
    const key_t channel_shm_key[MAX_CHANNELS] = { 10, 11, 12, 13, 14, 15, 16, 17 };
    const int shm_flags = SHM_RDONLY;
#endif

    int control_id;
    for (int attempt = 0; attempt < 5; ++attempt)
    {
        control_id = ACE_OS::shmget(control_shm_key, sizeof(NexusControl), 0666);
        if (control_id != -1)
        {
            break;
        }
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Waiting for shared memory...\n")));
        ACE_OS::sleep(ACE_Time_Value(0, 50 * 1000));
    }

    if (control_id == -1)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to connect to shared memory!\n")));
        return 0;
    }

    // Shared memory found for control data, attach to it
    mpControl = (NexusControl *)ACE_OS::shmat(control_id, NULL, 0);
    mChannels = mpControl->channels;
    mAudioTracksPerChannel = (mpControl->audio78_offset ? 8 : 4);

    ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("Connected to p_control %@\n  channels=%d elementsize=%d ringlen=%d\n"),
        mpControl,
        mpControl->channels,
        mpControl->elementsize,
        mpControl->ringlen));

    // Attach to each video ring buffer
    for (int i = 0; i < mpControl->channels; i++)
    {
        int shm_id;
        for (int attempt = 0; attempt < 5; ++attempt)
        {
            shm_id = ACE_OS::shmget(channel_shm_key[i], mpControl->elementsize, 0444);
            if (shm_id != -1)
            {
                break;
            }
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Waiting for channel[%d] shared memory...\n"), i));
            ACE_OS::sleep(ACE_Time_Value(0, 50 * 1000));
        }
        mRing[i] = (unsigned char *)ACE_OS::shmat(shm_id, NULL, shm_flags);
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Connected to channel[%d] %@\n"), i, mRing[i]));
    }

    return mChannels;
}

std::string IngexShm::SourceName(unsigned int channel_i)
{
    if (channel_i < mChannels)
    {
        return mpControl->channel[channel_i].source_name;
    }
    else
    {
        return "";
    }
}

void IngexShm::SourceName(unsigned int channel_i, const std::string & name)
{
    if (channel_i < mChannels)
    {
        // Set name
        strncpy( mpControl->channel[channel_i].source_name, name.c_str(),
            sizeof(mpControl->channel[channel_i].source_name));
        // Signal the change
#ifndef _MSC_VER
        PTHREAD_MUTEX_LOCK(&mpControl->m_source_name_update)
#endif
        ++mpControl->source_name_update;
#ifndef _MSC_VER
        PTHREAD_MUTEX_UNLOCK(&mpControl->m_source_name_update)
#endif
    }
}



