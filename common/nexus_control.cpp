/*
 * $Id: nexus_control.cpp,v 1.5 2011/07/13 14:51:12 john_f Exp $
 *
 * Module for creating and accessing nexus shared control memory
 *
 * Copyright (C) 2008  Stuart Cunningham, British Broadcasting Corporation
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

#define _XOPEN_SOURCE 600           // for posix_memalign

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>

#include <signal.h>
#include <unistd.h> 
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>
#include <sys/shm.h>

#include "time_utils.h"
#include "nexus_control.h"

extern const char *nexus_capture_format_name(CaptureFormat fmt)
{
    const char *names[] = {
    "FormatNone",
    "Format422UYVY",
    "Format422PlanarYUV",
    "Format422PlanarYUVShifted",
    "Format420PlanarYUV",
    "Format420PlanarYUVShifted"
    };

    return names[fmt];
}

extern const char *nexus_timecode_type_name(NexusTimecode tc_type)
{
    const char * s;
    switch (tc_type)
    {
    case NexusTC_LTC:
        s = "LTC";
        break;
    case NexusTC_VITC:
        s = "VITC";
        break;
    case NexusTC_DLTC:
        s = "DLTC";
        break;
    case NexusTC_DVITC:
        s = "DVITC";
        break;
    case NexusTC_SYSTEM:
        s = "System";
        break;
    case NexusTC_DEFAULT:
        s = "Default";
        break;
    default:
        s = "Unknown";
        break;
    }

    return s;
}

extern int nexus_lastframe(NexusControl *pctl, int channel)
{
    int lastframe = 0;
    
    if (pctl)
    {
        PTHREAD_MUTEX_LOCK(&pctl->channel[channel].m_lastframe)
        lastframe = pctl->channel[channel].lastframe;
        PTHREAD_MUTEX_UNLOCK(&pctl->channel[channel].m_lastframe)
    }
    
    return lastframe;
}

extern int nexus_hwdrop(NexusControl *pctl, int channel)
{
    int hwdrop = 0;
    
    if (pctl)
    {
        // using lastframe mutex because that is what is used when dvs_sdi sets hwdrop
        PTHREAD_MUTEX_LOCK(&pctl->channel[channel].m_lastframe)
        hwdrop = pctl->channel[channel].hwdrop;
        PTHREAD_MUTEX_UNLOCK(&pctl->channel[channel].m_lastframe)
    }
    
    return hwdrop;
}

extern NexusFrameData* nexus_frame_data(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    NexusFrameData * nfd = 0;
    if (pctl)
    {
        nfd = (NexusFrameData *)(ring[channel] + pctl->elementsize * (frame % pctl->ringlen) + pctl->frame_data_offset);
    }
    return nfd;
}

extern int nexus_num_aud_samp(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    return nexus_frame_data(pctl, ring, channel, frame)->num_aud_samp;
}

extern int nexus_signal_ok(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    if (pctl)
    {
        return nexus_frame_data(pctl, ring, channel, frame)->signal_ok;
    }
    else
    {
        return 0;
    }
}

extern int nexus_frame_number(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    return nexus_frame_data(pctl, ring, channel, frame)->frame_number;
}

extern int nexus_tc(const NexusControl *pctl, uint8_t *ring[], int channel, int frame, NexusTimecode tctype)
{
    NexusFrameData * nfd = nexus_frame_data(pctl, ring, channel, frame);

    if (tctype == NexusTC_DEFAULT) {
        tctype = pctl->default_tc_type;
    }

    int tc;
    switch (tctype)
    {
    case NexusTC_VITC:
        tc = nfd->vitc;
        break;
    case NexusTC_LTC:
        tc = nfd->ltc;
        break;
    case NexusTC_DVITC:
        tc = nfd->dvitc;
        break;
    case NexusTC_DLTC:
        tc = nfd->dltc;
        break;
    case NexusTC_SYSTEM:
        tc = nfd->systc;
        break;
    default:
        tc = nfd->vitc;
        break;
    }

    return tc;
}

Ingex::Timecode nexus_timecode(const NexusControl *pctl, uint8_t *ring[], int channel, int frame, NexusTimecode tctype)
{
    NexusFrameData * nfd = nexus_frame_data(pctl, ring, channel, frame);

    if (tctype == NexusTC_DEFAULT)
    {
        tctype = pctl->default_tc_type;
    }

    Ingex::Timecode tc = nfd->tc_systc;
    switch (tctype)
    {
    case NexusTC_VITC:
        tc = nfd->tc_vitc;
        break;
    case NexusTC_LTC:
        tc = nfd->tc_ltc;
        break;
    case NexusTC_DVITC:
        tc = nfd->tc_dvitc;
        break;
    case NexusTC_DLTC:
        tc = nfd->tc_dltc;
        break;
    case NexusTC_SYSTEM:
        tc = nfd->tc_systc;
        break;
    default:
        break;
    }

    return tc;
}

extern const uint8_t *nexus_primary_video(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    uint8_t * pri_vid = 0;
    if (pctl)
    {
        pri_vid = ring[channel] + pctl->elementsize * (frame % pctl->ringlen);
    }
    return pri_vid;
}

extern const uint8_t *nexus_secondary_video(const NexusControl *pctl, uint8_t *ring[], int channel, int frame)
{
    uint8_t * sec_vid = 0;
    if (pctl)
    {
        sec_vid = ring[channel] + pctl->elementsize * (frame % pctl->ringlen) + pctl->sec_video_offset;
    }
    return sec_vid;
}

extern const uint8_t *nexus_primary_audio(const NexusControl *pctl, uint8_t *ring[], int channel, int frame, int audio_track)
{
    uint8_t * pri_aud = 0;
    if (pctl)
    {
        pri_aud = (ring[channel] + pctl->elementsize * (frame % pctl->ringlen) + pctl->audio_offset) +
			audio_track * MAX_AUDIO_SAMPLES_PER_FRAME * 4;		// 32bit audio
    }
    return pri_aud;
}

extern const uint8_t *nexus_secondary_audio(const NexusControl *pctl, uint8_t *ring[], int channel, int frame, int audio_track)
{
    uint8_t * sec_aud = 0;
    if (pctl)
    {
        sec_aud = (ring[channel] + pctl->elementsize * (frame % pctl->ringlen) + pctl->sec_audio_offset) +
			audio_track * MAX_AUDIO_SAMPLES_PER_FRAME * 2;		// 16bit audio
    }
    return sec_aud;
}

extern void nexus_set_source_name(NexusControl *pctl, int channel, const char *source_name)
{
    PTHREAD_MUTEX_LOCK(&pctl->m_source_name_update)
    strncpy(pctl->channel[channel].source_name, source_name, sizeof(pctl->channel[channel].source_name));
    pctl->channel[channel].source_name[sizeof(pctl->channel[channel].source_name) - 1] = '\0';   // guarantee null terminated string
    pctl->source_name_update++;
    PTHREAD_MUTEX_UNLOCK(&pctl->m_source_name_update)
}

extern void nexus_get_source_name(NexusControl *pctl, int channel, char *source_name, size_t source_name_size,
                                  int *update_count)
{
    PTHREAD_MUTEX_LOCK(&pctl->m_source_name_update)
    strncpy(source_name, pctl->channel[channel].source_name, source_name_size);
    source_name[source_name_size - 1] = '\0';   // guarantee null terminated string
    *update_count = pctl->source_name_update;
    PTHREAD_MUTEX_UNLOCK(&pctl->m_source_name_update)
}

extern int nexus_connect_to_shared_mem(int timeout_microsec, int read_only, int verbose, NexusConnection *p)
{
    int at_flags = 0;           // default 0 means read-write

    if (read_only)
    {
        at_flags = SHM_RDONLY;
    }

    if (timeout_microsec > 0 && verbose)
    {
        printf("Waiting for shared memory...\n");
        fflush(stdout);
    }

    // If shared memory not found, sleep and try again, up to timeout.
    int retry_time = 20 * 1000;     // 20ms
    int time_taken = 0;

    // First attach to control structure
    void * ptr;
    int id;
    for (ptr = 0; ptr == 0;)
    {
        id = shmget(control_shm_key, sizeof(NexusControl), 0444);
        if (id != -1)
        {
            ptr = shmat(id, NULL, at_flags);
        }
        if (!ptr)
        {
            usleep(retry_time);
            time_taken += retry_time;
            if (time_taken >= timeout_microsec)
            {
                return 0;
            }
        }
    }
    // We now have valid pointer to control structure.
    p->pctl = static_cast<NexusControl *>(ptr);
    if (verbose)
    {
        printf("attached to control struct id %d at %p\n", id, p->pctl);
    }

    // Now attach to the ring buffers.
    for (int i = 0; i < p->pctl->channels; ++i)
    {
        for (ptr = 0; ptr == 0;)
        {
            id = shmget(channel_shm_key[i], sizeof(NexusControl), 0444);
            if (id != -1)
            {
                ptr = shmat(id, NULL, at_flags);
            }
            if (!ptr)
            {
                usleep(retry_time);
                time_taken += retry_time;
                if (time_taken >= timeout_microsec)
                {
                    return 0;
                }
            }
        }
        // We now have valid pointer to ring buffer [i]
        p->ring[i] = static_cast<uint8_t *>(ptr);
        if (verbose)
        {
            printf("attached to channel[%d]: '%s' id %d at %p\n", i, p->pctl->channel[i].source_name, id, p->ring[i]);
        }
    }

    return 1;
}

extern int nexus_disconnect_from_shared_mem(int verbose, const NexusConnection *p)
{
    int i;
    for (i = 0; i < p->pctl->channels; i++)
    {
        if (shmdt(p->ring[i]) == -1)
        {
            fprintf(stderr, "p->ring[%d] (0x%p) ", i, p->ring[i]);
            perror("shmdt");
            return 0;
        }
        else if (verbose)
        {
            printf("detached from channel[%d]\n", i);
        }
    }

    if (shmdt(p->pctl) == -1)
    {
        fprintf(stderr, "p->pctl (0x%p) ", p->pctl);
        perror("shmdt");
        return 0;
    }
    else if (verbose)
    {
        printf("detached from control\n");
    }

    return 1;
}

// Returns 1 if connection ok, 0 otherwise and sets return flags
extern int nexus_connection_status(const NexusConnection *p, int *p_heartbeat_stopped, int *p_capture_dead)
{
    int heartbeat_stopped = 0;
    int capture_dead = 0;

    // p->pctl is NULL before first connection made
    if (p->pctl) {
        struct timeval now;
        gettimeofday(&now, NULL);
        int64_t diff = tv_diff_microsecs(&p->pctl->owner_heartbeat, &now);
        // 100ms or lower can give false positives for lost heartbeat
        if (diff > 160 * 1000) {
            heartbeat_stopped = 1;

            if (kill(p->pctl->owner_pid, 0) == -1) {
                // If dvs_sdi is alive and running as root,
                // kill() will fail with EPERM
                if (errno != EPERM)
                    capture_dead = 1;
            }
        }
    }
    else {
        heartbeat_stopped = 1;
        capture_dead = 1;
    }

    if (heartbeat_stopped == 0 && capture_dead == 0)    // i.e. connection OK
        return 1;

    if (p_heartbeat_stopped)
        *p_heartbeat_stopped = heartbeat_stopped;

    if (p_capture_dead)
        *p_capture_dead = capture_dead;

    return 0;
}

