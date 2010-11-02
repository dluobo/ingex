/*
 * $Id: nexus_control.h,v 1.8 2010/11/02 16:45:19 john_f Exp $
 *
 * Shared memory interface between SDI capture threads and reader threads.
 *
 * Copyright (C) 2005 - 2010 British Broadcasting Corporation
 * All rights reserved
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

#ifndef NEXUS_CONTROL_H
#define NEXUS_CONTROL_H

#define MAX_CHANNELS 8              // number of separate video+audio inputs
#define MAX_RECORDERS 2             // number of Recorders per machine
#define MAX_ENCODES_PER_CHANNEL 3   // Recorder can make this number of encodes per channel

// Mono audio data is arranged in blocks of MAX_AUDIO_SAMPLES_PER_FRAME for each audio track
#define MAX_AUDIO_SAMPLES_PER_FRAME 1920

// Shared memory keys
#ifdef WIN32
// The kind of shared memory used by Ingex recorder daemon is not available on Windows
// but the settings below allow some code to compile.
#include <ace/OS_NS_sys_shm.h>
const key_t control_shm_key = "control";
const key_t channel_shm_key[MAX_CHANNELS] = { "channel0", "channel1", "channel2", "channel3" };
const int shm_flags = 0;
#else
#include <sys/shm.h>
const key_t control_shm_key = 9;
const key_t channel_shm_key[MAX_CHANNELS] = { 10, 11, 12, 13, 14, 15, 16, 17 };
const int shm_flags = SHM_RDONLY;
#endif


#ifndef _MSC_VER
#include <pthread.h>
#include <stdio.h>
#define PTHREAD_MUTEX_LOCK(x) if (pthread_mutex_lock( x ) != 0 ) fprintf(stderr, "pthread_mutex_lock failed\n");
#define PTHREAD_MUTEX_UNLOCK(x) if (pthread_mutex_unlock( x ) != 0 ) fprintf(stderr, "pthread_mutex_unlock failed\n");

#include <sys/time.h>       // gettimeofday() and struct timeval
#else
#include <ace/OS_NS_sys_time.h>
#define PTHREAD_MUTEX_LOCK(x)
#define PTHREAD_MUTEX_UNLOCK(x)
#endif

#include "integer_types.h"

#include "VideoRaster.h"
#include "Timecode.h"

typedef enum {
    FormatNone,                // Indicates off or disabled
    Format422UYVY,             // 4:2:2 buffer suitable for uncompressed capture
    Format422PlanarYUV,        // 4:2:2 buffer suitable for JPEG or NTSC DV50 encoding
    Format422PlanarYUVShifted, // 4:2:2 buffer suitable for PAL DV50 encoding (picture shift down 1 line)
    Format420PlanarYUV,        // 4:2:0 buffer suitable for MPEG encoding
    Format420PlanarYUVShifted, // 4:2:0 buffer suitable for PAL DV25 encoding (picture shift down 1 line)
    Format411PlanarYUV         // 4:1:1 buffer suitable for NTSC DV25 encoding
} CaptureFormat;

// Currently supported types of capture timecodes.
typedef enum {
    NexusTC_LTC,        // LTC
    NexusTC_VITC,       // VITC
    NexusTC_DLTC,       // LTC from RP188/RP196 ANC data
    NexusTC_DVITC,      // VITC from RP188/RP196 ANC data
    NexusTC_SYSTEM,     // system timecode using OS's time-of-day clock
    NexusTC_DEFAULT     // default type as set by capture option
} NexusTimecode;

typedef struct {
    // Timecode objects - preferred
    Ingex::Timecode tc_vitc;
    Ingex::Timecode tc_ltc;
    Ingex::Timecode tc_dvitc;
    Ingex::Timecode tc_dltc;
    Ingex::Timecode tc_systc;
    // Timecodes as frame counts - deprecated
    int vitc;
    int ltc;
    int dvitc;
    int dltc;
    int systc;
    // Capture timestampe - microseconds since 1970
    int64_t timestamp;
    // Hardware tick count
    int tick;
    // Signal ok flag
    int signal_ok;
    // Number of audio samples for this frame
    int num_aud_samp;
    // Frame number (incrementing count)
    int frame_number;
} NexusFrameData;

// Each channel's ring buffer is described by the NexusBufCtl structure
typedef struct {
#ifndef _MSC_VER
    pthread_mutex_t     m_lastframe;    // mutex for lastframe counter
    // TODO: use a condition variable to signal when new frame arrives
#endif
    int     lastframe;          // last frame number stored and now available
                                // use lastframe % ringlen to get buffer index
    int     hwdrop;             // frame-drops recorded by sv interface
    double  hwtemperature;      // temperature of capture card hardware (degrees C)
    int     num_audio_avail;    // number of audio tracks configured and available for
                                // capture at the hardware level (not how many tracks
                                // are carried by the current SDI signal, if any).

    // Following members are written to by Recorder
    char            source_name[64];    // identifies the source for this channel
} NexusBufCtl;

typedef struct {
    int     enabled;            // set to 1 if channel and encoding is enabled
    int     recording;          // set to 1 if channel is currently recording
    int     record_error;       // set to 1 if an error occured during recording
    int     frames_written;     // frames written during a recording
    int     frames_dropped;     // frames dropped during a recording
    int     frames_in_backlog;  // number of frames captured but not encoded yet
    char    desc[64];           // string describing video format e.g. "DV25"
    char    error[128];         // string representation of last error
} NexusRecordEncodingInfo;

typedef struct {
    char                    name[64];   // Recorder name e.g. "Ingex1"
    int                     pid;        // process id of Recorder to identify Recorders
    NexusRecordEncodingInfo channel[MAX_CHANNELS][MAX_ENCODES_PER_CHANNEL];
    NexusRecordEncodingInfo quad;
} NexusRecordInfo;

// Each element in a ring buffer is structured to facilitate the
// DMA transfer from the capture card, and is structured as follows:
//  video (4:2:2 primary)   - offset 0, size given by width*height*2
//  audio 32bit mono tracks - offset given by audio_offset (4 or 8 tracks)
//  (padding)
//  audio 16bit mono tracks - offset given by sec_audio_offset (4 or 8 tracks)
//  signal status           - offset given by signal_ok_offset
//  tick                    - offset given by tick_offset
//  timecodes               - offsets given by vitc_offset, ltc_offset
//  video (4:2:0 secondary) - offset given by sec_video_offset, size width*height*3/2

// NexusControl is the top-level control struture describing how many channels
// are in use and what their parameters are
typedef struct {
    NexusBufCtl     channel[MAX_CHANNELS];  // array of buffer control information
                                            // for all 8 possible channels

                                            // Stats updated by Recorders
    NexusRecordInfo record_info[MAX_RECORDERS];

    int             channels;           // number of channels and therefore ring buffers in use
    int             ringlen;            // number of elements in ring buffer
    int             elementsize;        // an element is video + audio + timecode

    int             width;              // width of primary video
    int             height;             // height of primary video
    int             frame_rate_numer;   // frame rate numerator e.g. 25
    int             frame_rate_denom;   // frame rate denominator e.g 1
    CaptureFormat   pri_video_format;   // primary video format: usually UYVY, YUV422, ...
    CaptureFormat   sec_video_format;   // secondary video format: usually NONE, YUV420, ...
    Ingex::VideoRaster::EnumType     pri_video_raster;
    Ingex::VideoRaster::EnumType     sec_video_raster;
    Ingex::PixelFormat::EnumType     pri_pixel_format;
    Ingex::PixelFormat::EnumType     sec_pixel_format;
    int             sec_width;          // width of secondary video (if any)
    int             sec_height;         // height of secondary video (if any)
    NexusTimecode   default_tc_type;    // type of timecode to be used by recorder etc.
    int             master_tc_channel;  // channel from which master timecode is sourced; -1 for no master
    int             owner_pid;          // process id of nominal owner of shared memory e.g. dvs_sdi
    struct timeval  owner_heartbeat;    // heartbeat timestamp of owner of shared memory

    // The following describe offsets and sizes within a ring buffer element for accessing data
    int             num_audio_tracks;   // number of audio tracks captured, either 4 or 8
    int             audio_offset;       // offset to start of mono 32bit audio tracks (4/8 tracks)
    int             audio_size;         // size in bytes of all mono 32bit audio data (4/8 tracks)
    int             sec_audio_offset;   // offset to start of secondary mono audio tracks (4/8 tracks)
    int             sec_audio_size;     // size of all secondary mono audio tracks
    int             frame_data_offset;  // offset to struct with timecode and other data
    int             sec_video_offset;   // offset to secondary video buffer


#ifndef _MSC_VER
    pthread_mutex_t m_source_name_update;   // mutex for source_name_update
#endif
    int             source_name_update;     // incremented each time the source_name is changed

} NexusControl;


// Structure used by nexus_connect_to_shared_mem()
typedef struct {
    uint8_t         *ring[MAX_CHANNELS];
    NexusControl    *pctl;
} NexusConnection;

extern int nexus_connect_to_shared_mem(int timeout_microsec, int read_only, int verbose, NexusConnection *p);
extern int nexus_disconnect_from_shared_mem(const NexusConnection *p);
extern int nexus_connection_status(const NexusConnection *p, int *p_heartbeat_stopped, int *p_capture_dead);


// Return a char* string name for the CaptureFormat
extern const char *nexus_capture_format_name(CaptureFormat fmt);

// Return a char* string name for the NexusTimecode type
extern const char *nexus_timecode_type_name(NexusTimecode tc_type);

extern int nexus_lastframe(NexusControl *pctl, int channel);
extern int nexus_hwdrop(NexusControl *pctl, int channel);

extern NexusFrameData* nexus_frame_data(const NexusControl *pctl, uint8_t *ring[], int channel, int frame);

extern int nexus_num_aud_samp(const NexusControl *pctl, uint8_t *ring[], int channel, int frame);
extern int nexus_signal_ok(const NexusControl *pctl, uint8_t *ring[], int channel, int frame);
extern int nexus_frame_number(const NexusControl *pctl, uint8_t *ring[], int channel, int frame);

extern int nexus_tc(const NexusControl *pctl, uint8_t *ring[], int channel, int frame, NexusTimecode tctype);
Ingex::Timecode nexus_timecode(const NexusControl *pctl, uint8_t *ring[], int channel, int frame, NexusTimecode tctype);

extern const uint8_t *nexus_primary_video(const NexusControl *pctl, uint8_t *ring[], int channel, int frame);
extern const uint8_t *nexus_secondary_video(const NexusControl *pctl, uint8_t *ring[], int channel, int frame);

extern const uint8_t *nexus_primary_audio(const NexusControl *pctl, uint8_t *ring[], int channel, int frame, int audio_track);
extern const uint8_t *nexus_secondary_audio(const NexusControl *pctl, uint8_t *ring[], int channel, int frame, int audio_track);

extern void nexus_set_source_name(NexusControl *pctl, int channel, const char *source_name);

extern void nexus_get_source_name(NexusControl *pctl, int channel, char *source_name, size_t source_name_size,
                                  int *update_count);

#endif // NEXUS_CONTROL_H

