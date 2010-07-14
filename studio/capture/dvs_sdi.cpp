/*
 * $Id: dvs_sdi.cpp,v 1.2 2010/07/14 13:06:36 john_f Exp $
 *
 * Record multiple SDI inputs to shared memory buffers.
 *
 * Copyright (C) 2005 - 2010 British Broadcasting Corporation
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

#define __STDC_CONSTANT_MACROS
#define __STDC_FORMAT_MACROS

#include "dvs_clib.h"
#include "dvs_fifo.h"

#include "nexus_control.h"
#include "logF.h"
#include "video_VITC.h"
#include "video_conversion.h"
#include "video_test_signals.h"
#include "avsync_analysis.h"
#include "time_utils.h"

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
#ifdef __MMX__
#include <mmintrin.h>
#endif
#include <sched.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

using namespace Ingex;

const int PAL_AUDIO_SAMPLES = 1920;
const int NTSC_AUDIO_SAMPLES[5] = { 1602, 1601, 1602, 1601, 1602 };
static int ntsc_audio_seq = 0;

const int64_t microseconds_per_day = 24 * 60 * 60 * INT64_C(1000000);

// Each thread uses the following thread-specific data
typedef struct {
    struct SwsContext* scale_context;
    AVFrame *inFrame;
    AVFrame *outFrame;
} SDIThreadData;

// Global variables for local context
namespace
{

// Debug
int DEBUG_TIMING = 0;

int64_t midnight_microseconds = 0;

pthread_t       sdi_thread[MAX_CHANNELS] = {0};
sv_handle       *a_sv[MAX_CHANNELS] = {0};
SDIThreadData   td[MAX_CHANNELS];
int             num_sdi_threads = 0;
NexusControl    *p_control = NULL;
uint8_t         *ring[MAX_CHANNELS] = {0};
int             control_id, ring_id[MAX_CHANNELS];
int             width = 0, height = 0;
int             sec_width = 0, sec_height = 0;
int             frame_rate_numer = 0, frame_rate_denom = 0;
int fps = 0;
bool df = false;
VideoRaster::EnumType primary_video_raster = VideoRaster::NONE;
VideoRaster::EnumType secondary_video_raster = VideoRaster::NONE;
Interlace::EnumType interlace = Interlace::NONE;
int             element_size = 0, dma_video_size = 0, dma_total_size = 0;
int      audio_offset = 0, audio_size = 0;
int      secondary_audio_offset = 0, secondary_audio_size = 0;
int      secondary_video_offset = 0;

int             frame_data_offset = 0;

CaptureFormat   video_format = Format422PlanarYUV;    // Only retained for backward compatibility
CaptureFormat   video_secondary_format = FormatNone;  // Only retained for backward compatibility
Ingex::PixelFormat::EnumType primary_pixel_format = Ingex::PixelFormat::NONE;
Ingex::PixelFormat::EnumType secondary_pixel_format = Ingex::PixelFormat::NONE;
int primary_line_shift = 0;
int secondary_line_shift = 0;

int verbose = 0;
int verbose_channel = -1;    // which channel to show when verbose is on
int audio8ch = 0;
int test_avsync = 0;
uint8_t *video_work_area[MAX_CHANNELS];
int benchmark = 0;
uint8_t *benchmark_video = NULL;
char *video_sample_file = NULL;
int aes_audio = 0;
int aes_routing = 0;

pthread_mutex_t m_log = PTHREAD_MUTEX_INITIALIZER;      // logging mutex to prevent intermixing logs

NexusTimecode timecode_type = NexusTC_VITC;   // default timecode is VITC
int master_channel = -1;                      // default is no master timecode distribution

// The mutex m_master_tc guards all access to master_tc and master_tod variables
pthread_mutex_t m_master_tc = PTHREAD_MUTEX_INITIALIZER;
//int master_tc = 0;               // timecode as integer
Ingex::Timecode master_tc;       // master timecode
int64_t master_tod = 0;          // gettimeofday value corresponding to master_tc

// When generating dummy frames, use LTC differences to calculate how many frames
//NexusTimecode recover_timecode_type = RecoverLTC;

uint8_t *no_video_frame = NULL;              // captioned black frame saying "NO VIDEO"
uint8_t *no_video_secondary_frame = NULL;    // captioned black frame saying "NO VIDEO"

}

#define SV_CHECK(x) {int res = x; if (res != SV_OK) { fprintf(stderr, "sv call failed=%d  %s line %d\n", res, __FILE__, __LINE__); sv_errorprint(sv,res); cleanup_exit(1, sv); } }

// Define missing macros for older SDKs
#ifndef SV_OPTION_MULTICHANNEL
#define SV_OPTION_MULTICHANNEL      184
#endif
#ifndef SV_OPTION_AUDIOAESROUTING
#define SV_OPTION_AUDIOAESROUTING   198
#endif
#ifndef SV_AUDIOAESROUTING_DEFAULT
#define SV_AUDIOAESROUTING_DEFAULT  0
#endif
#ifndef SV_AUDIOAESROUTING_8_8
#define SV_AUDIOAESROUTING_8_8      2
#endif
#ifndef SV_AUDIOAESROUTING_4_4
#define SV_AUDIOAESROUTING_4_4      3
#endif

// Functions in local context
namespace
{

void timestamp_decode(int64_t timestamp, int * year, int * month, int * day, int * hour, int * minute, int * sec, int * microsec)
{
    time_t time_sec = timestamp / INT64_C(1000000);
    struct tm my_tm;
    localtime_r(&time_sec, &my_tm);
    *year = my_tm.tm_year + 1900;
    *month = my_tm.tm_mon + 1;
    *day = my_tm.tm_mday;
    *hour = my_tm.tm_hour;
    *minute = my_tm.tm_min;
    *sec = my_tm.tm_sec;
    *microsec = timestamp % INT64_C(1000000);
}

Ingex::Timecode timecode_from_dvs_bits(int bits, int fps_num, int fps_den)
{
    int hr10  = (bits & 0x30000000) >> 28;
    int hr01  = (bits & 0x0f000000) >> 24;
    int hr = 10 * hr10 + hr01;
    int min10 = (bits & 0x00700000) >> 20;
    int min01 = (bits & 0x000f0000) >> 16;
    int min = 10 * min10 + min01;
    int sec10 = (bits & 0x00007000) >> 12;
    int sec01 = (bits & 0x00000f00) >> 8;
    int sec = 10 * sec10 + sec01;
    int fm10  = (bits & 0x00000030) >> 4;
    int fm01  = bits & 0x0000000f;
    int fm = 10 * fm10 + fm01;

    bool drop = (bits & 0x00000040) != 0;

    //fprintf(stderr, "%08x %02d:%02d:%02d:%02d %s\n", bits, hr, min, sec, fm, drop ? "DF" : "NDF");

    return Ingex::Timecode(hr, min, sec, fm, fps_num, fps_den, drop);
}

void show_scheduler()
{
    int sched = sched_getscheduler(0);
    switch (sched)
    {
    case SCHED_OTHER:
        logTF("SCHED_OTHER\n");
        break;
    case SCHED_FIFO:
        logTF("SCHED_FIFO\n");
        break;
    case SCHED_RR:
        logTF("SCHED_RR\n");
        break;
    default:
        logTF("SCHED unknown\n");
        break;
    }
}

int init_process_shared_mutex(pthread_mutex_t *mutex)
{
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) {
        fprintf(stderr, "Failed to initialize mutex attribute\n");
        return 0;
    }
    
    // set pshared to PTHREAD_PROCESS_SHARED to allow other processes to use the mutex
    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
        fprintf(stderr, "Failed to set pshared mutex attribute to PTHREAD_PROCESS_SHARED\n");
        return 0;
    }
    
    if (pthread_mutex_init(mutex, &attr) != 0) {
        fprintf(stderr, "Mutex init error\n");
        return 0;
    }
    
    pthread_mutexattr_destroy(&attr);
    
    return 1;
}

void cleanup_shared_mem(void)
{
    int             i, id;
    struct shmid_ds shm_desc;

    for (i = 0; i < (1 + num_sdi_threads); i++)
    {
        id = shmget(9 + i, sizeof(*p_control), 0444);
        if (id == -1)
        {
            fprintf(stderr, "No shmem id for key %d\n", 9 + i);
            continue;
        }
        if (shmctl(id, IPC_RMID, &shm_desc) == -1)
        {
            perror("shmctl(id, IPC_RMID):");
        }
    }
}

void cleanup_exit(int res, sv_handle *sv)
{
    printf("cleaning up\n");

    // Cancel all threads (other than main thread)
    int i;
    for (i = 0; i < MAX_CHANNELS; i++) {
        if (sdi_thread[i] != 0) {
            logTF("cancelling thread %d (id %lu)\n", i, sdi_thread[i]);
            pthread_cancel(sdi_thread[i]);
            pthread_join(sdi_thread[i], NULL);
            //logTF("thread %d joined ok\n", i);
        }
    }

    // Close all sv handles
    for (i = 0; i < MAX_CHANNELS; i++) {
        if (a_sv[i] != 0) {
            printf("closing sv_handle index %d\n", i);
            sv_close(a_sv[i]);
        }
    }

    // delete shared memory
    cleanup_shared_mem();

    usleep(100 * 1000);     // 0.1 seconds

    printf("done - exiting\n");
    exit(res);
}

int check_sdk_version3(void)
{
    sv_handle *sv;
    sv_version version;
    int device = 0;
    int result = 0;

    SV_CHECK( sv_openex(&sv, (char*)"PCI,card=0", SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_QUERY, 0, 0) );
    SV_CHECK( sv_version_status(sv, &version, sizeof(version), device, 1, 0) );
    printf("%s version = (%d,%d,%d,%d)\n", version.module, version.release.v.major, version.release.v.minor, version.release.v.patch, version.release.v.fix);
    if (version.release.v.major >= 3)
        result = 1;

    SV_CHECK( sv_close(sv) );

    return result;
}

/*
void framerate_for_videomode(int videomode, int *p_numer, int *p_denom)
{
    int video = videomode & SV_MODE_MASK;       // mask off everything except video

    if (video == SV_MODE_PAL || video == SV_MODE_SMPTE274_25I) {
        *p_numer = 25;
        *p_denom = 1;
    }
    else if (video == SV_MODE_NTSC || video == SV_MODE_SMPTE274_30I) {
        *p_numer = 30000;
        *p_denom = 1001;
    }
    else if (video == SV_MODE_SMPTE296_50P) {
        *p_numer = 50;
        *p_denom = 1;
    }
    else if (video == SV_MODE_SMPTE296_60P) {
        *p_numer = 60000;
        *p_denom = 1001;
    }
    else {
        *p_numer = 25;
        *p_denom = 1;
    }
}
*/

int get_video_raster(int channel, VideoRaster::EnumType & video_raster)
{
    int dvs_mode;
    int ret = sv_query(a_sv[channel], SV_QUERY_MODE_CURRENT, 0, &dvs_mode);

    switch (dvs_mode & SV_MODE_MASK)
    {
    case SV_MODE_PAL:
        video_raster = VideoRaster::PAL;
        break;
    case SV_MODE_NTSC:
        video_raster = VideoRaster::NTSC;
        break;
    case SV_MODE_SMPTE274_25I:
        video_raster = VideoRaster::SMPTE274_25I;
        break;
    case SV_MODE_SMPTE274_29I:
        video_raster = VideoRaster::SMPTE274_29I;
        break;
    case SV_MODE_SMPTE296_50P:
        video_raster = VideoRaster::SMPTE296_50P;
        break;
    case SV_MODE_SMPTE296_59P:
        video_raster = VideoRaster::SMPTE296_59P;
        break;
    default:
        video_raster = VideoRaster::NONE;
        break;
    }

    return ret;
}

VideoRaster::EnumType sd_raster(VideoRaster::EnumType raster)
{
    VideoRaster::EnumType sd_raster;
    switch (raster)
    {
    case VideoRaster::PAL:
    case VideoRaster::SMPTE274_25I:
    case VideoRaster::SMPTE296_50P:
        sd_raster = VideoRaster::PAL;
        break;
    case VideoRaster::NTSC:
    case VideoRaster::SMPTE274_29I:
    case VideoRaster::SMPTE296_59P:
        sd_raster = VideoRaster::NTSC;
        break;
    default:
        sd_raster = VideoRaster::NONE;
        break;
    }

    return sd_raster;
}

int set_videomode_on_all_channels(int max_channels, VideoRaster::EnumType video_raster, int n_audio)
{
    int dvs_mode = SV_MODE_COLOR_YUV422 | SV_MODE_STORAGE_FRAME;


    if (n_audio == 0)
    {
        dvs_mode |= SV_MODE_AUDIO_NOAUDIO;
    }
    else if (n_audio <= 4)
    {
        dvs_mode |= SV_MODE_AUDIO_4CHANNEL;
    }
    else
    {
        dvs_mode |= SV_MODE_AUDIO_8CHANNEL;
    }

    switch (video_raster)
    {
    case VideoRaster::PAL:
    case VideoRaster::PAL_B:
        dvs_mode |= SV_MODE_PAL;
        break;
    case VideoRaster::NTSC:
        dvs_mode |= SV_MODE_NTSC;
        break;
    case VideoRaster::SMPTE274_25I:
        dvs_mode |= SV_MODE_SMPTE274_25I;
        break;
    case VideoRaster::SMPTE274_29I:
        dvs_mode |= SV_MODE_SMPTE274_29I;
        break;
    case VideoRaster::SMPTE296_50P:
        dvs_mode |= SV_MODE_SMPTE296_50P;
        break;
    case VideoRaster::SMPTE296_59P:
        dvs_mode |= SV_MODE_SMPTE296_59P;
        break;
    default:
        break;
    }

    // Assume SDK multichannel capability (> major version 3)
    int card = 0;
    int channel = 0;
    for (card = 0; card < max_channels && channel < max_channels; card++)
    {
        char card_str[64] = {0};

        snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d,channel=0", card);

        int res = sv_openex(&a_sv[channel],
                            card_str, SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_DEFAULT, 0, 0);
        if (res == SV_ERROR_WRONGMODE)
        {
            // Multichannel mode not on so doesn't like "channel=0"
            snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d", card);
            res = sv_openex(&a_sv[channel], card_str, SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_DEFAULT, 0, 0);
        }
        if (res == SV_OK)
        {
            res = sv_videomode(a_sv[channel], dvs_mode);
            if (res != SV_OK) {
                fprintf(stderr, "sv_videomode(channel=%d, 0x%08x) failed: ", channel, dvs_mode);
                sv_errorprint(a_sv[channel], res);
                sv_close(a_sv[channel]);
                return 0;
            }

            // check for multichannel mode
            int multichannel_mode = 0;
            if ((res = sv_option_get(a_sv[channel], SV_OPTION_MULTICHANNEL, &multichannel_mode)) != SV_OK) {
                logTF("card %d: sv_option_get(SV_OPTION_MULTICHANNEL) failed: %s\n", card, sv_geterrortext(res));
            }

            channel++;

            // If card has a multichannel mode on, open second channel
            if (multichannel_mode)
            {
                if (channel < max_channels)
                {
                    snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d,channel=1", card);
                    int res = sv_openex(&a_sv[channel], card_str, SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_DEFAULT, 0, 0);
                    if (res == SV_OK)
                    {
                        res = sv_videomode(a_sv[channel], dvs_mode);
                        if (res != SV_OK) {
                            logTF("card %d: channel=1, sv_videomode(0x%08x) failed, trying without AUDIO\n", card, dvs_mode);
                            dvs_mode = dvs_mode & (~SV_MODE_AUDIO_MASK);
                            res = sv_videomode(a_sv[channel], dvs_mode);
                            if (res != SV_OK) {
                                fprintf(stderr, "sv_videomode(channel=%d, 0x%08x) failed: ", channel, dvs_mode);
                                sv_errorprint(a_sv[channel], res);
                                sv_close(a_sv[channel]);
                                return 0;
                            }
                            logTF("card %d: channel=1, sv_videomode(0x%08x) succeeded without AUDIO\n", card, dvs_mode);
                        }

                        sv_close(a_sv[channel]);

                        channel++;
                    }
                    else
                    {
                        logTF("card %d: failed opening second channel: %s\n", card, sv_geterrortext(res));
                    }
                    sv_close(a_sv[channel-2]);
                }
                else
                {
                    sv_close(a_sv[channel-1]);
                }
            }
            else
            {
                logTF("card %d: multichannel mode off\n", card);
                sv_close(a_sv[channel-1]);
            }
        }
        else
        {
            // SV_ERROR_DEVICENOTFOUND is returned when there is no more hardware
            if (res == SV_ERROR_DEVICENOTFOUND)
                return 1;

            logTF("card %d: sv_openex(%s) %s\n", card, card_str, sv_geterrortext(res));
            return 0;
        }
    }

    return 1;
}

void set_aes_option(int channel, int enable_aes)
{
    int opt_value = (enable_aes ? SV_AUDIOINPUT_AESEBU : SV_AUDIOINPUT_AIV);
    int res = sv_option_set(a_sv[channel], SV_OPTION_AUDIOINPUT, opt_value);
    if (SV_OK != res)
    {
        logTF("channel %d: sv_option_set(SV_OPTION_AUDIOINPUT) failed: %s\n", channel, sv_geterrortext(res));
    }
}

void set_sync_option(int channel, int sync_type, int width)
{
    if (sync_type == -1) {
        // default to bilevel for SD or trilevel for HD
        if (width <= 720)
            sync_type = SV_SYNC_BILEVEL;
        else
            sync_type = SV_SYNC_TRILEVEL;
    }

    int res = sv_sync(a_sv[channel], sync_type);
    if (res != SV_OK) {
        logTF("channel %d: sv_sync(0x%08x) failed: %s\n", channel, sync_type, sv_geterrortext(res));
    }
}

void catch_sigusr1(int sig_number)
{
    // toggle a flag
}

void catch_sigint(int sig_number)
{
    printf("\nReceived signal %d - ", sig_number);
    cleanup_exit(0, NULL);
}

void log_avsync_analysis(int chan, int lastframe, const uint8_t *addr, unsigned long audio12_offset, unsigned long audio34_offset)
{
    int line_size = width*2;
    int click1 = 0, click1_off = -1;
    int click2 = 0, click2_off = -1;
    int click3 = 0, click3_off = -1;
    int click4 = 0, click4_off = -1;

    int flash = find_red_flash_uyvy(addr, line_size);
    find_audio_click_32bit_stereo(addr + audio12_offset,
                                                &click1, &click1_off,
                                                &click2, &click2_off);
    find_audio_click_32bit_stereo(addr + audio34_offset,
                                                &click3, &click3_off,
                                                &click4, &click4_off);

    if (flash || click1 || click2 || click3 || click4) {
        if (flash)
            logTF("chan %d: %5d  red-flash      \n", chan, lastframe + 1);
        if (click1)
            logTF("chan %d: %5d  a1off=%d %.1fms%s\n", chan, lastframe + 1, click1_off, click1_off / 48.0 , (click1_off > 480 ? " A/V sync error!" : ""));
        if (click2)
            logTF("chan %d: %5d  a2off=%d %.1fms\n", chan, lastframe + 1, click2_off, click2_off / 48.0);
        if (click3)
            logTF("chan %d: %5d  a3off=%d %.1fms\n", chan, lastframe + 1, click3_off, click3_off / 48.0);
        if (click4)
            logTF("chan %d: %5d  a4off=%d %.1fms\n", chan, lastframe + 1, click4_off, click4_off / 48.0);
    }
}

// Read available physical memory stats and
// allocate shared memory ring buffers accordingly.
int allocate_shared_buffers(int num_channels, long long max_memory)
{
    long long   k_shmmax = 0;
    int         ring_len, i;
    FILE        *procfp;

    // Default maximum of 1 GiB to avoid misleading shmmat errors.
    // /usr/src/linux/Documentation/sysctl/kernel.txt says:
    // "memory segments up to 1Gb are now supported"
    const long long shm_default = 0x40000000; // 1GiB
    if (max_memory <= 0)
    {
        max_memory = shm_default;
    }

    // Linux specific way to read shmmax value.
    // With opensuse it will be -1 by default.
    // Change shmmax with...
    //  root# echo  104857600 >> /proc/sys/kernel/shmmax  # (100 MiB)
    //  root# echo 1073741824 >> /proc/sys/kernel/shmmax  # (1 GiB)

    if ((procfp = fopen("/proc/sys/kernel/shmmax", "r")) == NULL)
    {
        perror("fopen(/proc/sys/kernel/shmmax)");
        return 0;
    }
    fscanf(procfp, "%lld", &k_shmmax);
    fclose(procfp);

    if (k_shmmax > 0 && max_memory > k_shmmax)
    {
        // Limit to kernel max segment size
        printf("Buffer size request %lld MiB too large, reducing to kernel limit %lld MiB\n", max_memory / (1024 * 1024), k_shmmax / (1024 * 1024) );
        max_memory = k_shmmax;
    }

    // calculate reasonable ring buffer length
    // reduce by small number 5 to leave a little room for other shared mem e.g. database servers
    ring_len = max_memory / num_channels / element_size - 5;
    printf("buffer size %lld (%.3f MiB) calculated per channel ring_len %d\n", max_memory, max_memory / (1024*1024.0), ring_len);

    printf("element_size=%d(0x%x) ring_len=%d (%.2f secs) (total=%lld)\n", element_size, element_size, ring_len, ring_len / 25.0, (long long)element_size * ring_len);
    if (ring_len < 10)
    {
        printf("ring_len=%d too small (< 10) - try increasing shmmax:\n", ring_len);
        printf("  echo 1073741824 >> /proc/sys/kernel/shmmax\n");
        return 0;
    }

    // Allocate memory for control structure which is fixed size
    if ((control_id = shmget(9, sizeof(*p_control), IPC_CREAT | IPC_EXCL | 0666)) == -1)
    {
        if (errno == EEXIST)
        {
            fprintf(stderr, "shmget: shm segment exists, deleting all related segments\n");
            cleanup_shared_mem();
            if ((control_id = shmget(9, sizeof(*p_control), IPC_CREAT | IPC_EXCL | 0666)) == -1)
            {
                perror("shmget control key 9");
                fprintf(stderr, "Use\n\tipcs | grep '0x0000000[9abcd]'\nplus ipcrm -m <id> to cleanup\n");
                return 0;
            }
        }
        else
        {
            perror("shmget control key 9");
            fprintf(stderr, "Use\n\tipcs | grep '0x0000000[9abcd]'\nplus ipcrm -m <id> to cleanup\n");
            return 0;
        }
    }
    p_control = (NexusControl*)shmat(control_id, NULL, 0);

    p_control->channels = num_channels;
    p_control->ringlen = ring_len;
    p_control->elementsize = element_size;

    p_control->frame_rate_numer = frame_rate_numer;
    p_control->frame_rate_denom = frame_rate_denom;

    //p_control->drop_frame = df;

    p_control->pri_video_raster = primary_video_raster;
    p_control->pri_pixel_format = primary_pixel_format;
    p_control->pri_video_format = video_format;
    p_control->width = width;
    p_control->height = height;
    
    p_control->sec_video_raster = secondary_video_raster;
    p_control->sec_pixel_format = secondary_pixel_format;
    p_control->sec_video_format = video_secondary_format;
    p_control->sec_width = sec_width;
    p_control->sec_height = sec_height;

    p_control->default_tc_type = timecode_type;
    p_control->master_tc_channel = master_channel;

    p_control->num_audio_tracks = (audio8ch ? 8 : 4);
    p_control->audio_offset = audio_offset;
    p_control->audio_size = audio_size;
    p_control->sec_audio_offset = secondary_audio_offset;
    p_control->sec_audio_size = secondary_audio_size;

    p_control->sec_video_offset = secondary_video_offset;

    p_control->frame_data_offset = frame_data_offset;

    p_control->source_name_update = 0;
    if (!init_process_shared_mutex(&p_control->m_source_name_update))
    {
        return 0;
    }

    // Allocate multiple element ring buffers containing video + audio + tc
    // key for variable number of ring buffers can be 10, 11, 12, 13, 14, 15, 16, 17
    for (i = 0; i < num_channels; i++)
    {
        int key = i + 10;
        ring_id[i] = shmget(key, element_size * ring_len, IPC_CREAT | IPC_EXCL | 0666);
        if (ring_id[i] == -1)   /* shm error */
        {
            int save_errno = errno;
            fprintf(stderr, "Attemp to shmget for key %d: ", key);
            perror("shmget");
            if (save_errno == EEXIST)
                fprintf(stderr, "Use\n\tipcs | grep '0x0000000[9abcd]'\nplus ipcrm -m <id> to cleanup\n");
            if (save_errno == ENOMEM)
                fprintf(stderr, "Perhaps you could increase shmmax: e.g.\n  echo 1073741824 >> /proc/sys/kernel/shmmax\n");
            if (save_errno == EINVAL)
                fprintf(stderr, "You asked for too much?\n");
    
            return 0;
        }

        ring[i] = (uint8_t *)shmat(ring_id[i], NULL, 0);
        if (ring[i] == (void *)-1) {
            fprintf(stderr, "shmat failed for channel[%d], ring_id[%d] = %d\n", i, i, ring_id[i]);
            perror("shmat");
            return 0;
        }
        p_control->channel[i].lastframe = -1;
        p_control->channel[i].hwdrop = 0;
        sprintf(p_control->channel[i].source_name, "ch%d", i);
        p_control->channel[i].source_name[sizeof(p_control->channel[i].source_name) - 1] = '\0';

        // initialise mutex in shared memory
        if (!init_process_shared_mutex(&p_control->channel[i].m_lastframe))
            return 0;

        // Set ring frames to black - too slow!
        // memset(ring[i], 0x80, element_size * ring_len);
    }

    return 1;
}

// Return number of seconds between 1 Jan 1970 and last midnight
// where midnight is localtime not GMT.
/*
time_t today_midnight_time(void)
{
    struct tm now_tm;

    time_t now = time(NULL);
    localtime_r(&now, &now_tm);
    now_tm.tm_sec = 0;
    now_tm.tm_min = 0;
    now_tm.tm_hour = 0;
    return mktime(&now_tm);
}
*/

void dvsaudio32_deinterleave_32bit(uint8_t *audio12, uint8_t *audio34, uint8_t *a32[])
{
	// Copy all 32bits, de-interleaving pairs
    for (int i = 0; i < MAX_AUDIO_SAMPLES_PER_FRAME; i++) {
        int src_idx = i*4*2;                // index into 32bit interleaved pairs
        int mon_idx = i*4;                  // index into 32bit mono audio
        a32[0][mon_idx + 0] = audio12[src_idx + 0];
        a32[0][mon_idx + 1] = audio12[src_idx + 1];
        a32[0][mon_idx + 2] = audio12[src_idx + 2];
        a32[0][mon_idx + 3] = audio12[src_idx + 3];
        a32[1][mon_idx + 0] = audio12[src_idx + 4];
        a32[1][mon_idx + 1] = audio12[src_idx + 5];
        a32[1][mon_idx + 2] = audio12[src_idx + 6];
        a32[1][mon_idx + 3] = audio12[src_idx + 7];

        a32[2][mon_idx + 0] = audio34[src_idx + 0];
        a32[2][mon_idx + 1] = audio34[src_idx + 1];
        a32[2][mon_idx + 2] = audio34[src_idx + 2];
        a32[2][mon_idx + 3] = audio34[src_idx + 3];
        a32[3][mon_idx + 0] = audio34[src_idx + 4];
        a32[3][mon_idx + 1] = audio34[src_idx + 5];
        a32[3][mon_idx + 2] = audio34[src_idx + 6];
        a32[3][mon_idx + 3] = audio34[src_idx + 7];
	}
}

void dvsaudio32_deinterleave_16bit(uint8_t *audio12, uint8_t *audio34, uint8_t *a16[])
{
    // Copy 16 most significant bits out of 32 bit audio pairs
    // for 4 channels each loop iteraction
    for (int i = 0; i < MAX_AUDIO_SAMPLES_PER_FRAME; i++) {
        int tmp_idx = i*4*2;                // index into 32bit interleaved pairs
        int sec_idx = i*2;                  // index into 16bit mono audio buffers
        a16[0][sec_idx + 0] = audio12[tmp_idx + 2];
        a16[0][sec_idx + 1] = audio12[tmp_idx + 3];
        a16[1][sec_idx + 0] = audio12[tmp_idx + 6];
        a16[1][sec_idx + 1] = audio12[tmp_idx + 7];

        a16[2][sec_idx + 0] = audio34[tmp_idx + 2];
        a16[2][sec_idx + 1] = audio34[tmp_idx + 3];
        a16[3][sec_idx + 0] = audio34[tmp_idx + 6];
        a16[3][sec_idx + 1] = audio34[tmp_idx + 7];
    }
}

void dvsaudio32_to_4_mono_tracks(uint8_t *src, uint8_t *dst32, uint8_t *dst16)
{
    // src contains either 4 channels or 8 channels as multiplexed pairs
    // where each pair is aligned on a 0x4000 boundary
    uint8_t *audio12 = src;
    uint8_t *audio34 = src + 0x4000;

    // De-interleave to 32bit mono buffers
    //
    // Setup pointers for 32bit mono audio
    uint8_t *a32[4];
    a32[0] = dst32;
    a32[1] = dst32 + MAX_AUDIO_SAMPLES_PER_FRAME*4 * 1;
    a32[2] = dst32 + MAX_AUDIO_SAMPLES_PER_FRAME*4 * 2;
    a32[3] = dst32 + MAX_AUDIO_SAMPLES_PER_FRAME*4 * 3;

	dvsaudio32_deinterleave_32bit(audio12, audio34, a32);

    // De-interleave and truncate to 16bit mono
    //
    // Setup pointers for 16bit mono audio
    uint8_t *a16[4];
    a16[0] = dst16;
    a16[1] = dst16 + MAX_AUDIO_SAMPLES_PER_FRAME*2 * 1;
    a16[2] = dst16 + MAX_AUDIO_SAMPLES_PER_FRAME*2 * 2;
    a16[3] = dst16 + MAX_AUDIO_SAMPLES_PER_FRAME*2 * 3;

	dvsaudio32_deinterleave_16bit(audio12, audio34, a16);
}

void dvsaudio32_to_mono_audio(uint8_t *src, uint8_t *dst32, uint8_t *dst16, int audio8)
{
    // src and dst32 can be the same buffer, so first read full src audio into tmp buffer
    uint8_t tmp[audio_size];

    memcpy(tmp, src, audio_size);

	dvsaudio32_to_4_mono_tracks(tmp, dst32, dst16);

	if (audio8)
	{
		// 32bit buffer is fixed by DVS internals to have channel 5,6 at offset 0x8000
		// 16bit buffer has channel 5 start immediately after channel 4
		dvsaudio32_to_4_mono_tracks(tmp + 0x8000,
						            dst32 + 0x8000,
									dst16 + MAX_AUDIO_SAMPLES_PER_FRAME*2 * 4);
	}
}

// Given a 64bit time-of-day corresponding to when a frame was captured,
// calculate a derived timecode from the global master timecode.
Ingex::Timecode derive_timecode_from_master(int64_t tod_rec, int64_t *p_diff_to_master)
{
    Ingex::Timecode derived_tc = master_tc;
    int64_t diff_to_master = 0;
    // tod_rec is microseconds since 1970

    PTHREAD_MUTEX_LOCK( &m_master_tc )

    if (master_tod && !master_tc.IsNull())
    {
        // compute offset between this chan's recorded frame's time-of-day and master's
        diff_to_master = tod_rec - master_tod;
        int int_diff = diff_to_master;  // microseconds, should be a relatively small number
        int sign_diff = 1;
        // round to nearest frame
        if (int_diff < 0)
        {
            int_diff = -int_diff;
            sign_diff = -1;
        }
        int microsecs_per_frame = 1000000 * master_tc.FrameRateDenominator() / master_tc.FrameRateNumerator();
        int div = (int_diff + microsecs_per_frame / 2) / microsecs_per_frame;
        derived_tc = master_tc + sign_diff * div;
    }

    PTHREAD_MUTEX_UNLOCK( &m_master_tc )

    // return diff_to_master for logging purposes
    if (p_diff_to_master)
    {
        *p_diff_to_master = diff_to_master;
    }

    return derived_tc;
}

//
// write_picture()
//
// Gets a frame from the SDI FIFO and stores it to memory ring buffer
// (also can write to disk as a debugging option)
int write_picture(int chan, sv_handle *sv, sv_fifo *poutput, int recover_from_video_loss)
{
    sv_fifo_buffer      *pbuffer;
    //sv_fifo_bufferinfo  bufferinfo;
    int                 get_res;
    int                 put_res;
    int                 ring_len = p_control->ringlen;
    NexusBufCtl         *pc = &(p_control->channel[chan]);

    // Get sv memmory buffer
    // Ignore problems with missing audio, and carry on with good video but zeroed audio
    int flags = 0;
    if (recover_from_video_loss)
    {
        flags |= SV_FIFO_FLAG_FLUSH;
        logTF("chan %d: Setting SV_FIFO_FLAG_FLUSH\n", chan);
    }


    /*
    Call sv_fifo_getbuffer().
    This returns a fifo buffer containing the next captured frame,
    blocking if necessary to wait for a frame to be available.
    */

    int64_t tod1;
    int64_t tod2;
    int64_t tod3;
    if (DEBUG_TIMING)
    {
        tod1 = gettimeofday64();
    }
    //logTF("chan %d: calling sv_fifo_getbuffer()...\n", chan);
    get_res = sv_fifo_getbuffer(sv, poutput, &pbuffer, NULL, flags);
    //logTF("chan %d: sv_fifo_getbuffer() returned\n", chan);

    if (get_res != SV_OK && get_res != SV_ERROR_INPUT_AUDIO_NOAIV
            && get_res != SV_ERROR_INPUT_AUDIO_NOAESEBU)
    {
        return get_res;
    }

    // Check FIFO status
    sv_fifo_info info;
    int status_res = sv_fifo_status(sv, poutput, &info);

    if (SV_OK == status_res && info.availbuffers > 1)
    {
        logTF("chan %d: frame backlog %d\n", chan, info.availbuffers - 1);
    }

    // dma buffer is structured as follows
    //
    // 0x0000 video frame                   offset= 0
    // 0x0000 audio ch. 0, 1 interleaved    offset= pbuffer->audio[0].addr[0]
    // 0x0000 audio ch. 2, 3 interleaved    offset= pbuffer->audio[0].addr[1]
    //

    // check audio offset
    if (0)
    {
        fprintf(stderr, "chan = %d, audio_offset = %x, video[0].addr = %p, audio[0].addr[0-4] = %p %p %p %p\n",
            chan,
            audio_offset,
            pbuffer->video[0].addr,
            pbuffer->audio[0].addr[0],
            pbuffer->audio[0].addr[1],
            pbuffer->audio[0].addr[2],
            pbuffer->audio[0].addr[3]);
    }

    // Clear frame number.
    // Do this first in order to mark video as changed before we start to change it.
    // We can't actually set the frame number now because the audio DMA will overwrite it.
    int frame_number = 0;
    NexusFrameData * nfd = (NexusFrameData *)(ring[chan] + element_size * ((pc->lastframe + 1) % ring_len) + frame_data_offset);
    nfd->frame_number = frame_number;

    uint8_t *vid_dest = ring[chan] + element_size * ((pc->lastframe+1) % ring_len);
    uint8_t *dma_dest = vid_dest;

    // If primary format is not native DMA format (UYVY) use video_work_area[]
    // buffer to store temporary copy of DMA'd video and audio.
    if (Ingex::PixelFormat::UYVY_422 != primary_pixel_format)
    {
#if 0
        // Store frame at pc->lastframe+2 to allow space for UYVY->YUV422 conversion
        dma_dest = ring[chan] + element_size * ((pc->lastframe+2) % ring_len);
#else
        // Store frame in video work area (it is faster)
        dma_dest = video_work_area[chan];
#endif
    }
    pbuffer->dma.addr = (char *)dma_dest;
    pbuffer->dma.size = dma_total_size;         // video + audio

    /*
    Call sv_fifo_putbuffer().
    This performs the DMA transfer of the frame and then releases the buffer back to the fifo.
    (It also fills in our bufferinfo structure. No longer used)
    */

    if (DEBUG_TIMING)
    {
        tod2 = gettimeofday64();
    }
    // read frame from DVS chan
    // reception of a SIGUSR1 can sometimes cause this to fail
    // If it fails we should restart fifo.
    //logTF("chan %d: calling sv_fifo_putbuffer()...\n", chan);
    //put_res = sv_fifo_putbuffer(sv, poutput, pbuffer, &bufferinfo);
    put_res = sv_fifo_putbuffer(sv, poutput, pbuffer, NULL);

    if (DEBUG_TIMING && SV_OK == status_res)
    {
        tod3 = gettimeofday64();
        //logTF("tick/2 %d getbuffer %"PRId64" putbuffer %"PRId64" microseconds\n", pbuffer->control.tick / 2, tod2 - tod1, tod3 - tod2);
        logTF("chan %d: tick/2 %d drop %d buffers %d / %d getbuffer %"PRId64" microseconds\n", chan, pbuffer->control.tick / 2,
            info.dropped, info.availbuffers, info.nbuffers, tod2 - tod1);
    }

    if (put_res != SV_OK)
    {
        sv_errorprint(sv, put_res);
        fprintf(stderr, "sv_fifo_putbuffer failed, restarting fifo\n");
        SV_CHECK( sv_fifo_reset(sv, poutput) );
        SV_CHECK( sv_fifo_start(sv, poutput) );
    }
    //logTF("chan %d: sv_fifo_putbuffer() returned ok\n", chan);

    // set flag so we can zero audio if not present
    int no_audio = (SV_ERROR_INPUT_AUDIO_NOAIV == get_res
                || SV_ERROR_INPUT_AUDIO_NOAESEBU == get_res
                || 0 == pbuffer->audio[0].addr[0]);

    if (test_avsync && !no_audio)
    {
        log_avsync_analysis(chan, pc->lastframe,
            dma_dest, (unsigned long)pbuffer->audio[0].addr[0], (unsigned long)pbuffer->audio[0].addr[1]);
    }

    // Set number of audio samples captured into ring element (not constant for NTSC)
    // audio[0].size is total size in bytes of 2 channels of 32bit samples
    int num_audio_samples = pbuffer->audio[0].size / 2 / 4;

    // Get current time-of-day time and this chan's current hw clock.
    int current_tick;
    // Note DVS type uint32 rather than uint32_t
    uint32 h_clock;
    uint32 l_clock;
    SV_CHECK( sv_currenttime(sv, SV_CURRENTTIME_CURRENT, &current_tick, &h_clock, &l_clock) );
    int64_t tod = gettimeofday64();

    // Use the hw-clock time the putbuffer'd frame was recorded,
    // to calculate the time-of-day the frame was recorded.
    // (pbuffer->control.clock_high/low appears identical to bufferinfo.clock_high/low)
    // Comment from Jim:
    // I found that as well, except for the 10th or 11th frame (when the FIFO
    // first wraps round?), when it jumps back in time a lot. Using pbuffer
    // means I don't have to pass a sv_fifo_bufferinfo to putbuffer.
    // Comment from John:
    // Have changed code to use pbuffer->control.clock_high/low.

    int64_t cur_clock = h_clock * INT64_C(0x100000000) + l_clock;
    //int64_t rec_clock = bufferinfo.clock_high * INT64_C(0x100000000) + (unsigned)bufferinfo.clock_low;
    int64_t rec_clock = pbuffer->control.clock_high * INT64_C(0x100000000) + (unsigned int)pbuffer->control.clock_low;
    int64_t clock_diff = cur_clock - rec_clock;
    // The frame was captured clock_diff ago
    int64_t tod_rec = tod - clock_diff;
    //logTF("tod = %"PRId64", cur_clock = %"PRId64", rec_clock = %"PRId64" microseconds\n", tod, cur_clock, rec_clock);

    // DMA transfer gives video as UYVY.  Convert to Planar YUV if required.
    if (Ingex::PixelFormat::YUV_PLANAR_422 == primary_pixel_format)
    {
        uint8_t *vid_input = dma_dest;

        // Use hard-to-code picture when benchmarking
        if (benchmark)
            vid_input = benchmark_video;

        // Time this
        //int64_t start = gettimeofday64();

        // Repack to planar YUV 4:2:2
        uyvy_to_yuv422(     width, height,
                            primary_line_shift,
                            vid_input,                      // input
                            vid_dest);                      // output

        // End of timing
        //int64_t end = gettimeofday64();
        //fprintf(stderr, "uyvy_to_yuv422() took %6"PRIi64" microseconds\n", end - start);

        // copy audio to match
        if (!no_audio)
        {
            dvsaudio32_to_mono_audio(
                    dma_dest + audio_offset,              // src
                    vid_dest + audio_offset,              // 32bit dest
                    vid_dest + secondary_audio_offset,    // 16bit dest
                    audio8ch);                            // 4 or 8 channels
        }
    }
    else
    {
        // No video reformat required, just de-interleave audio to give mono audio
        if (!no_audio)
        {
            // reformats 32bit interleaved and writes back to same audio buffer
            // adds 16bit version of same audio to secondary audio buffer
            dvsaudio32_to_mono_audio(
                    vid_dest + audio_offset,              // src
                    vid_dest + audio_offset,              // 32bit dest
                    vid_dest + secondary_audio_offset,    // 16bit dest
                    audio8ch);                            // 4 or 8 channels
        }
    }

    // No audio present in signal, so add silence
    if (no_audio)
    {
        memset(vid_dest + audio_offset, 0, audio_size);
        memset(vid_dest + secondary_audio_offset, 0, secondary_audio_size);
    }

    // Convert primary video into secondary video buffer
    if (Ingex::PixelFormat::NONE != secondary_pixel_format)
    {
        if (width > 720)
        {
            // HD primary video, scale and reformat to SD secondary
            ::PixelFormat in_pixfmt;
            if (Ingex::PixelFormat::UYVY_422 == primary_pixel_format)
            {
                in_pixfmt = PIX_FMT_UYVY422;
            }
            else
            {
                in_pixfmt = PIX_FMT_YUV422P;
            }
            ::PixelFormat out_pixfmt;
            if (Ingex::PixelFormat::YUV_PLANAR_422 == secondary_pixel_format)
            {
                out_pixfmt = PIX_FMT_YUV422P;
            }
            else
            {
                out_pixfmt = PIX_FMT_YUV420P;
            }

            td[chan].scale_context = sws_getCachedContext(td[chan].scale_context,
                    width, height,          // input WxH
                    in_pixfmt,
                    sec_width, sec_height,  // output WxH
                    out_pixfmt,
                    SWS_FAST_BILINEAR,
                    NULL, NULL, NULL);
                    // other flags include: SWS_FAST_BILINEAR, SWS_BILINEAR, SWS_BICUBIC, SWS_POINT
                    // SWS_AREA, SWS_BICUBLIN, SWS_GAUSS, ... SWS_PRINT_INFO (debug)
            avpicture_fill( (AVPicture*)td[chan].inFrame,
                            vid_dest,                                   // captured video
                            in_pixfmt,
                            width, height);
            avpicture_fill( (AVPicture*)td[chan].outFrame,
                            vid_dest + secondary_video_offset,          // output to secondary video
                            out_pixfmt,
                            sec_width, sec_height);
            sws_scale(td[chan].scale_context,
                            td[chan].inFrame->data, td[chan].inFrame->linesize,
                            0, height,
                            td[chan].outFrame->data, td[chan].outFrame->linesize);
        }
        else
        {
            // SD primary video, reformat for SD secondary
            if (Ingex::PixelFormat::YUV_PLANAR_422 == secondary_pixel_format)
            {
                // Repack to planar YUV 4:2:2
                uyvy_to_yuv422( width, height,
                                secondary_line_shift,
                                dma_dest,                                   // input
                                vid_dest + secondary_video_offset);         // output
            }
            else if (Ingex::PixelFormat::YUV_PLANAR_420_DV == secondary_pixel_format)
            {
                // Downsample and repack to planar YUV 4:2:0 for PAL DV
                uyvy_to_yuv420_DV_sampling( width, height,
                            secondary_line_shift,                       // should be true
                            dma_dest,                                   // input
                            vid_dest + secondary_video_offset);         // output
            }
            else if (Ingex::PixelFormat::YUV_PLANAR_411 == secondary_pixel_format)
            {
                // Downsample and repack to planar YUV 4:1:1 for NTSC DV
                uyvy_to_yuv411( width, height,
                            secondary_line_shift,                       // should be false
                            dma_dest,                                   // input
                            vid_dest + secondary_video_offset);         // output
            }
            else if (Ingex::PixelFormat::YUV_PLANAR_420_MPEG == secondary_pixel_format)
            {
                // Downsample and repack to planar YUV 4:2:0 for MPEG
                uyvy_to_yuv420( width, height,
                            0,                                          // no DV25 line shift
                            dma_dest,                                   // input
                            vid_dest + secondary_video_offset);         // output
            }
        }
    }

    // The various timecodes...

    // SMPTE-12M LTC and VITC
    int ltc_bits = pbuffer->timecode.ltc_tc;
    int vitc1_bits = pbuffer->timecode.vitc_tc;
    int vitc2_bits = pbuffer->timecode.vitc_tc2;
    // "DLTC" and "DVITC" timecodes from RP188/RP196 ANC data
    int dltc_bits = pbuffer->anctimecode.dltc_tc;
    int dvitc1_bits = pbuffer->anctimecode.dvitc_tc[0];
    int dvitc2_bits = pbuffer->anctimecode.dvitc_tc[1];


    // Handle buggy field order (can happen with misconfigured camera)
    // Incorrect field order causes vitc_tc and vitc2 to be swapped.
    // If the high bit is set on vitc use vitc2 instead.
    int vitc_bits;
    if ((unsigned)vitc1_bits >= 0x80000000 && (unsigned)vitc2_bits < 0x80000000)
    {
        vitc_bits = vitc2_bits;
        if (1) //(verbose)
        {
            PTHREAD_MUTEX_LOCK( &m_log )
            logTF("chan %d: 1st vitc value >= 0x80000000 (0x%08x), using 2nd vitc (0x%08x)\n", chan, vitc1_bits, vitc2_bits);
            PTHREAD_MUTEX_UNLOCK( &m_log )
        }
    }
    else
    {
        vitc_bits = vitc1_bits;
    }
    int dvitc_bits;
    if ((unsigned)dvitc1_bits >= 0x80000000 && (unsigned)dvitc2_bits < 0x80000000)
    {
        dvitc_bits = dvitc2_bits;
        if (1) //(verbose)
        {
            PTHREAD_MUTEX_LOCK( &m_log )
            logTF("chan %d: 1st dvitc value >= 0x80000000 (0x%08x), using 2nd vitc (0x%08x)\n", chan, dvitc1_bits, dvitc2_bits);
            PTHREAD_MUTEX_UNLOCK( &m_log )
        }
    }
    else
    {
        dvitc_bits = dvitc1_bits;
    }

    // A similar check must be done for LTC since the field
    // flag is occasionally set when the fifo call returns
    if ((unsigned)ltc_bits >= 0x80000000)
    {
        if (0) //(verbose)
        {
            PTHREAD_MUTEX_LOCK( &m_log )
            logTF("chan %d: ltc tc >= 0x80000000 (0x%08x), masking high bit\n", chan, ltc_bits);
            PTHREAD_MUTEX_UNLOCK( &m_log )
        }
        ltc_bits = (unsigned)ltc_bits & 0x7fffffff;
    }
    if ((unsigned)dltc_bits >= 0x80000000)
    {
        if (0) //(verbose)
        {
            PTHREAD_MUTEX_LOCK( &m_log )
            logTF("chan %d: dltc tc >= 0x80000000 (0x%08x), masking high bit\n", chan, dltc_bits);
            PTHREAD_MUTEX_UNLOCK( &m_log )
        }
        dltc_bits = (unsigned)dltc_bits & 0x7fffffff;
    }

    Ingex::Timecode tc_ltc = timecode_from_dvs_bits(ltc_bits, frame_rate_numer, frame_rate_denom);
    Ingex::Timecode tc_vitc = timecode_from_dvs_bits(vitc_bits, frame_rate_numer, frame_rate_denom);
    Ingex::Timecode tc_dltc = timecode_from_dvs_bits(dltc_bits, frame_rate_numer, frame_rate_denom);
    Ingex::Timecode tc_dvitc = timecode_from_dvs_bits(dvitc_bits, frame_rate_numer, frame_rate_denom);

    // System timecode - start with microseconds since midnight (tod_rec is in microsecs since 1970)
    int64_t sys_time_microsec = (tod_rec - midnight_microseconds) % microseconds_per_day;

    // Compute system timecode as int number of frames since midnight
    int systc_as_int = (int)(sys_time_microsec * frame_rate_numer / (INT64_C(1000000) * frame_rate_denom));
    // Choose drop-frame mode for systc if rate not integral
    bool systc_drop = (frame_rate_numer % frame_rate_denom != 0);
    Ingex::Timecode tc_systc(systc_as_int, frame_rate_numer, frame_rate_denom, systc_drop);

    // Convert timecodes to frames since midnight
    int ltc_as_int = tc_ltc.FramesSinceMidnight();
    int vitc_as_int = tc_vitc.FramesSinceMidnight();
    int dltc_as_int = tc_dltc.FramesSinceMidnight();
    int dvitc_as_int = tc_dvitc.FramesSinceMidnight();

    int orig_vitc_as_int = vitc_as_int;
    int orig_ltc_as_int = ltc_as_int;

    // If enabled, handle master timecode distribution
    int64_t diff_to_master = 0;
    if (master_channel >= 0 && NexusTC_SYSTEM != timecode_type)
    {
        if (chan == master_channel)
        {
            // Store timecode and time-of-day the frame was recorded in shared variable
            PTHREAD_MUTEX_LOCK( &m_master_tc )
            switch (timecode_type)
            {
            case NexusTC_LTC:
                master_tc = tc_ltc;
                break;
            case NexusTC_VITC:
                master_tc = tc_vitc;
                break;
            case NexusTC_DLTC:
                master_tc = tc_dltc;
                break;
            case NexusTC_DVITC:
                master_tc = tc_dvitc;
                break;
            case  NexusTC_SYSTEM:
            case  NexusTC_DEFAULT:
                // not applicable
                break;
            }
            master_tod = tod_rec;
            PTHREAD_MUTEX_UNLOCK( &m_master_tc )
        }
        else
        {
            Ingex::Timecode derived_tc = derive_timecode_from_master(tod_rec, &diff_to_master);

            switch (timecode_type)
            {
            case NexusTC_LTC:
                tc_ltc = derived_tc;
                ltc_as_int = derived_tc.FramesSinceMidnight();
                break;
            case NexusTC_VITC:
                tc_vitc = derived_tc;
                vitc_as_int = derived_tc.FramesSinceMidnight();
                break;
            case NexusTC_DLTC:
                tc_dltc = derived_tc;
                dltc_as_int = derived_tc.FramesSinceMidnight();
                break;
            case NexusTC_DVITC:
                tc_dvitc = derived_tc;
                dvitc_as_int = derived_tc.FramesSinceMidnight();
                break;
            case  NexusTC_SYSTEM:
            case  NexusTC_DEFAULT:
                // not applicable
                break;
            }
        }
    }

    // Lookup last frame's timecodes to calculate timecode discontinuity
    int last_vitc = 0;
    int last_ltc = 0;
    int last_dvitc = 0;
    int last_dltc = 0;
    int last_systc = 0;
    int64_t last_timestamp = 0;

    // Only process discontinuity when not the first frame
    if (pc->lastframe > -1)
    {
        NexusFrameData * last_nfd = (NexusFrameData *)(ring[chan] + element_size * ((pc->lastframe) % ring_len) + frame_data_offset);
        last_vitc = last_nfd->vitc;
        last_ltc = last_nfd->ltc;
        last_dvitc = last_nfd->dvitc;
        last_dltc = last_nfd->dltc;
        last_systc = last_nfd->systc;
        last_timestamp = last_nfd->timestamp;

        // Check number of frames to recover if any
        // A maximum of 50 frames will be recovered.
        int diff;
        switch (timecode_type)
        {
        case NexusTC_LTC:
            diff = ltc_as_int - last_ltc;
            break;
        case NexusTC_VITC:
            diff = vitc_as_int - last_vitc;
            break;
        case NexusTC_DLTC:
            diff = dltc_as_int - last_dltc;
            break;
        case NexusTC_DVITC:
            diff = dvitc_as_int - last_dvitc;
            break;
        case NexusTC_SYSTEM:
        default:
            diff = systc_as_int - last_systc;
            break;
        }
        // We expect diff to be 1 frame
        int missing = diff - 1;

        // Compare timestamps
        int64_t timestamp_diff = tod_rec - last_timestamp;
        int64_t expected_diff = (INT64_C(1000000) * frame_rate_denom) / frame_rate_numer;
        int64_t timestamp_variation = timestamp_diff - expected_diff;
        if (timestamp_variation > 10000 || timestamp_variation < -10000)
        {
            logTF("chan %d: Timestamp differs from expected by %"PRId64" ms\n",
                chan, timestamp_variation / 1000);
            if (0)
            {
                // Additional debug
                int year1, month1, day1, hour1, minute1, sec1, microsec1;
                int year2, month2, day2, hour2, minute2, sec2, microsec2;
                timestamp_decode(tod_rec, &year1, &month1, &day1, &hour1, &minute1, &sec1, &microsec1);
                timestamp_decode(tod, &year2, &month2, &day2, &hour2, &minute2, &sec2, &microsec2);
                logTF("chan %d:\n"
                    "  tod_rec = %"PRId64" %"PRIx64" %04d-%02d-%02d %02d:%02d:%02d.%06d\n"
                    "  tod     = %"PRId64" %"PRIx64" %04d-%02d-%02d %02d:%02d:%02d.%06d\n",
                    chan,
                    tod_rec, tod_rec,
                    year1, month1, day1, hour1, minute1, sec1, microsec1,
                    tod, tod,
                    year2, month2, day2, hour2, minute2, sec2, microsec2);
            }
        }

        if (recover_from_video_loss && missing > 0 && missing <= 50)
        {
            logTF("chan %d: Need to recover %d frames\n", chan, missing);

            // Increment pc->lastframe by amount to avoid discontinuity.
            // Future frames will go in correct place in buffer
            pc->lastframe += missing;

            // Ideally we would copy the dma transferred frame into its correct position
            // but this needs testing.
            //uint8_t *tmp_frame = (uint8_t*)malloc(width*height*2);
            //memcpy(tmp_frame, vid_dest, width*height*2);
            //free(tmp_frame);

            logTF("chan %d: Recovered.  lastframe=%d ltc=%d\n", chan, pc->lastframe, ltc_as_int);
        }
    }

    // Copy frame data such as timecodes to the end of the ring element.
    // This is a bit sneaky but saves maintaining a separate ring buffer.
    // Note that pc->lastframe may have been incremented above so we
    // re-calculate the pointer to NexusFrameData
    nfd = (NexusFrameData *)(ring[chan] + element_size * ((pc->lastframe + 1) % ring_len) + frame_data_offset);

    // LTC, VITC, DLTC, DVITC, SYS as Ingex::Timecode
    nfd->tc_ltc = tc_ltc;
    nfd->tc_vitc = tc_vitc;
    nfd->tc_dltc = tc_dltc;
    nfd->tc_dvitc = tc_dvitc;
    nfd->tc_systc = tc_systc;

    // LTC, VITC, DLTC, DVITC, SYS as integer frame count
    nfd->vitc = vitc_as_int;
    nfd->ltc = ltc_as_int;
    nfd->dvitc = dvitc_as_int;
    nfd->dltc = dltc_as_int;
    nfd->systc = systc_as_int;

    // "frame" tick (tick / 2)
    int frame_tick = pbuffer->control.tick / 2;
    nfd->tick = frame_tick;

    // Set signal_ok flag
    nfd->signal_ok = 1;

    // Audio samples is 1920 for PAL frame rates, but varies between 1600 and 1602 for NTSC rates
    nfd->num_aud_samp = num_audio_samples;
    //logTF("chan %d: num_audio_samples = %d\n", chan, num_audio_samples);

    // store timestamp of when frame was captured (microseconds since 1970)
    nfd->timestamp = tod_rec;

    // Timecode error occurs when difference is not exactly 1
    // or (around midnight) not exactly -2159999
    // (ignore the first frame captured when lastframe is -1)
    int vitc_diff = vitc_as_int - last_vitc;
    int ltc_diff = ltc_as_int - last_ltc;
    int dvitc_diff = dvitc_as_int - last_dvitc;
    int dltc_diff = dltc_as_int - last_dltc;

    int tc_bits;
    int tc_frames;
    int tc_diff;
    Ingex::Timecode tc_tc = tc_systc;
    const char * tc_name = "";
    switch (timecode_type)
    {
    case NexusTC_VITC:
        tc_name = "VITC";
        tc_bits = vitc_bits;
        tc_frames = vitc_as_int;
        tc_diff = vitc_diff;
        tc_tc = tc_vitc;
        break;
    case NexusTC_LTC:
        tc_name = "LTC";
        tc_bits = ltc_bits;
        tc_frames = ltc_as_int;
        tc_diff = ltc_diff;
        tc_tc = tc_ltc;
        break;
    case NexusTC_DVITC:
        tc_name = "DVITC";
        tc_bits = vitc_bits;
        tc_frames = dvitc_as_int;
        tc_diff = dvitc_diff;
        tc_tc = tc_dvitc;
        break;
    case NexusTC_DLTC:
        tc_name = "DLTC";
        tc_bits = dltc_bits;
        tc_frames = dltc_as_int;
        tc_diff = dltc_diff;
        tc_tc = tc_dltc;
        break;
    default:
        tc_bits = 0;
        tc_frames = 0;
        tc_diff = 1;
    }
    int tc_err = (tc_diff != 1 && tc_diff != 2159999 && pc->lastframe != -1); // NB. 2159999 assumes 25 fps

    // log timecode discontinuity if any, or give verbose log of specified chan
    if (tc_err || (verbose && chan == verbose_channel))
    {
        PTHREAD_MUTEX_LOCK( &m_log )
        if (0)
        {
            // All timecode info
            logTF("chan %d: lastframe=%6d tick/2=%7d hwdrop=%3d vitc1=%08x vitc2=%08x ltc=%08x dvitc1=%08x dvitc2=%08x dltc=%08x vitc_diff=%d%s ltc_diff=%d%s dvitc_diff=%d%s dltc_diff=%d%s\n",
            chan, pc->lastframe, pbuffer->control.tick / 2,
            info.dropped,
            vitc1_bits,
            vitc2_bits,
            ltc_bits,
            vitc1_bits,
            vitc2_bits,
            dltc_bits,
            vitc_diff,
            vitc_diff != 1 ? "!" : "",
            ltc_diff,
            ltc_diff != 1 ? "!" : "",
            dvitc_diff,
            dvitc_diff != 1 ? "!" : "",
            dltc_diff,
            dltc_diff != 1 ? "!" : "");
        }
        else
        {
            // Selected timecode info
            logTF("chan %d: lastframe=%6d tick/2=%7d hwdrop=%3d tc=%s tc_err=%d\n",
                chan, pc->lastframe, pbuffer->control.tick / 2, info.dropped,
                tc_tc.Text(), tc_diff - 1);
        }
        PTHREAD_MUTEX_UNLOCK( &m_log )
    }

    if (verbose)
    {
        PTHREAD_MUTEX_LOCK( &m_log )        // guard logging with mutex to avoid intermixing

        if (info.dropped != pc->hwdrop)
        {
            // dropped count has changed
            logTF("chan %d: lf=%7d vitc=%8d ltc=%8d dropped=%d\n", chan, pc->lastframe+1, vitc_as_int, ltc_as_int, info.dropped);
        }

        logTF("chan[%d]: tick=%d hc=%u,%u diff_to_mast=%12"PRIi64"  lf=%7d vitc=%8d ltc=%8d (orig v=%8d l=%8d] drop=%d\n", chan,
            pbuffer->control.tick,
            pbuffer->control.clock_high, pbuffer->control.clock_low,
            diff_to_master,
            pc->lastframe+1, vitc_as_int, ltc_as_int,
            orig_vitc_as_int, orig_ltc_as_int,
            info.dropped);

        PTHREAD_MUTEX_UNLOCK( &m_log )      // end logging guard
    }

    // Query hardware health (once a second to reduce excessive sv_query calls)
    if ((pc->lastframe+1) % 25 == 0)
    {
        int temp;
        if (sv_query(sv, SV_QUERY_TEMPERATURE, 0, &temp) == SV_OK)
        {
            // Ignore bad temperature values - sometimes returned by DVS SDK
            if (temp != 0xFF0000)
            {
                pc->hwtemperature = (double)temp / 65536.0;
            }
        }
    }

    // Set frame number
    frame_number = pc->lastframe + 1;
    nfd->frame_number = frame_number;

    // signal frame is now ready
    PTHREAD_MUTEX_LOCK( &pc->m_lastframe )
    pc->hwdrop = info.dropped;
    pc->lastframe++;
    PTHREAD_MUTEX_UNLOCK( &pc->m_lastframe )

    return SV_OK;
}

int write_dummy_frames(sv_handle *sv, int chan, int current_frame_tick, int tick_last_dummy_frame, int64_t tod_tc_read)
{
    // No video so copy "no video" frame instead
    //
    // Use this chan's internal tick counter to determine when and
    // how many frames to fabricate
    if (verbose > 1)
    {
        logTF("chan: %d frame_tick=%d tick_last_dummy_frame=%d\n", chan, current_frame_tick, tick_last_dummy_frame);
    }

    if (tick_last_dummy_frame == -1 || current_frame_tick - tick_last_dummy_frame > 0)
    {
        // Work out how many frames to make
        int num_dummy_frames;
        if (tick_last_dummy_frame == -1)
            num_dummy_frames = 1;
        else
            num_dummy_frames = current_frame_tick - tick_last_dummy_frame;

        // Master timecode -
        // first dummy frame will get the closest timecode to the master timecode
        // subsequent dummy frames will increment by 1
        Ingex::Timecode derived_tc = derive_timecode_from_master(tod_tc_read, NULL);

        // Timecode from system clock
        // (tod_tc_read is in microsecs since 1970)
        //int64_t sys_time_microsec = tod_tc_read - (today_midnight_time() * INT64_C(1000000));
        int64_t sys_time_microsec = (tod_tc_read - midnight_microseconds) % microseconds_per_day;
        // Compute timecode as int number of frames since midnight
        int sys_tc = (int)(sys_time_microsec * frame_rate_numer / (INT64_C(1000000) * frame_rate_denom));
        Ingex::Timecode system_tc(sys_tc, frame_rate_numer, frame_rate_denom, false);

        if (derived_tc.IsNull())
        {
            // Use system clock
            derived_tc = system_tc;
        }

        /*
        int last_tc = derive_timecode_from_master(tod_tc_read, NULL).FramesSinceMidnight();
        int last_vitc = last_tc;
        int last_ltc = last_tc;
        int last_dvitc = last_tc;
        int last_dltc = last_tc;

        int last_systc = sys_tc;
        */

        for (int i = 0; i < num_dummy_frames; i++)
        {
            // Read ring buffer info
            int                 ring_len = p_control->ringlen;
            NexusBufCtl         *pc = &(p_control->channel[chan]);
            NexusFrameData * nfd = (NexusFrameData *)(ring[chan] + element_size * ((pc->lastframe + 1) % ring_len) + frame_data_offset);

            // Set frame number
            // Do this first in order to mark video as changed before we start to change it.
            int frame_number = pc->lastframe + 1;
            nfd->frame_number = frame_number;

            // dummy video frame
            uint8_t *vid_dest = ring[chan] + element_size * ((pc->lastframe+1) % ring_len);
            memcpy(vid_dest, no_video_frame, width*height*2);

            // dummy video frame in secondary buffer
            if (no_video_secondary_frame)
            {
                uint8_t *vid_dest_sec = ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + secondary_video_offset;
                if (Ingex::PixelFormat::YUV_PLANAR_422 == secondary_pixel_format)
                {
                    // 4:2:2
                    memcpy(vid_dest_sec, no_video_secondary_frame, sec_width*sec_height*2);
                }
                else
                {
                    // 4:2:0 or 4:1:1
                    memcpy(vid_dest_sec, no_video_secondary_frame, sec_width*sec_height*3/2);
                }
            }

            // write silent audio
            int n_audio_samples;
            switch (primary_video_raster)
            {
            case Ingex::VideoRaster::PAL:
            case Ingex::VideoRaster::SMPTE274_25I:
            case Ingex::VideoRaster::SMPTE274_25P:
            default:
                n_audio_samples = PAL_AUDIO_SAMPLES;
                break;
            case Ingex::VideoRaster::NTSC:
            case Ingex::VideoRaster::SMPTE274_29I:
            case Ingex::VideoRaster::SMPTE274_29P:
                n_audio_samples = NTSC_AUDIO_SAMPLES[ntsc_audio_seq];
                ntsc_audio_seq++;
                ntsc_audio_seq %= 5;
                break;
            }
            if (audio8ch)
            {
                memset(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + audio_offset, 0, n_audio_samples * 4 * 8);
            }
            else
            {
                memset(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + audio_offset, 0, n_audio_samples * 4 * 4);
            }

            NexusFrameData * last_nfd = (NexusFrameData *)(ring[chan] + element_size * ((pc->lastframe) % ring_len) + frame_data_offset);
            // Increment timecode by 1 for dummy frames after the first
            if (i > 0)
            {
                derived_tc += 1;
                system_tc += 1;
            }
            int last_ftk = 0;
            if (pc->lastframe >= 0)
            {
                last_ftk = last_nfd->tick;
            }
            last_ftk++;

            nfd->tc_vitc = derived_tc;
            nfd->tc_ltc = derived_tc;
            nfd->tc_dvitc = derived_tc;
            nfd->tc_dltc = derived_tc;
            nfd->tc_systc = system_tc;

            nfd->vitc = derived_tc.FramesSinceMidnight();
            nfd->ltc = derived_tc.FramesSinceMidnight();
            nfd->dvitc = derived_tc.FramesSinceMidnight();
            nfd->dltc = derived_tc.FramesSinceMidnight();
            nfd->systc = system_tc.FramesSinceMidnight();

            nfd->tick = last_ftk;

            nfd->timestamp = tod_tc_read;

            nfd->num_aud_samp = n_audio_samples;

            // Indicate we've got a bad video signal
            nfd->signal_ok = 0;

            if (verbose)
            {
                logTF("chan: %d i:%2d  cur_frame_tick=%d tick_last_dummy_frame=%d last_ftk=%d tc=%s\n", chan, i, current_frame_tick, tick_last_dummy_frame, last_ftk, derived_tc.Text());
            }

            // signal frame is now ready
            PTHREAD_MUTEX_LOCK( &pc->m_lastframe )
            pc->lastframe++;
            PTHREAD_MUTEX_UNLOCK( &pc->m_lastframe )

            tick_last_dummy_frame = current_frame_tick - num_dummy_frames + 1;
        }
    }

    return tick_last_dummy_frame;
}

void wait_for_good_signal(sv_handle *sv, int chan, int required_good_frames)
{
    int good_frames = 0;
    int tick_last_dummy_frame = -1;

    // Poll until we get a good video signal for at least required_good_frames
    while (1)
    {
        int                 current_tick;
        unsigned int        h_clock, l_clock;
        sv_timecode_info    timecodes;
        int                 sdiA, videoin, audioin, tc_status;

        // When master video signal is not present, we may still be able to read LTC and use for master timecode.

        // get time/tick from card
        SV_CHECK( sv_currenttime(sv, SV_CURRENTTIME_CURRENT, &current_tick, &h_clock, &l_clock) );
        // try to read current input timecodes
        SV_CHECK( sv_timecode_feedback(sv, &timecodes, NULL) );
        // update time-of-day the timecodes were read
        int64_t tod_tc_read = gettimeofday64();

        if (chan == master_channel)
        {
            PTHREAD_MUTEX_LOCK( &m_master_tc )
            switch (timecode_type)
            {
            case NexusTC_LTC:
                master_tc = timecode_from_dvs_bits(timecodes.altc_tc, frame_rate_numer, frame_rate_denom);
                break;
            default:
                break;
            }
            PTHREAD_MUTEX_UNLOCK( &m_master_tc )

            master_tod = tod_tc_read;
        }

        // Check for input signals
        SV_CHECK( sv_query(sv, SV_QUERY_INPUTRASTER_SDIA, 0, &sdiA) );
        SV_CHECK( sv_query(sv, SV_QUERY_VIDEOINERROR, 0, &videoin) );
        SV_CHECK( sv_query(sv, SV_QUERY_AUDIOINERROR, 0, &audioin) );
        SV_CHECK( sv_query(sv, SV_QUERY_VALIDTIMECODE, 0, &tc_status) );

        if (verbose > 1)
        {
            printf("\r%8d raster=0x%08x video=%s audio=%s tc_status=%s%s%s%s%s%s (0x%x)",
                current_tick,
                sdiA,
                videoin == SV_OK ? "OK" : "error",
                audioin == SV_OK ? "OK" : "error",
                (tc_status & SV_VALIDTIMECODE_LTC) ? "LTC " : "",
                (tc_status & SV_VALIDTIMECODE_DLTC) ? "DLTC " : "",
                (tc_status & SV_VALIDTIMECODE_VITC_F1) ? "VITC/F1 " : "",
                (tc_status & SV_VALIDTIMECODE_VITC_F2) ? "VITC/F2 " : "",
                (tc_status & SV_VALIDTIMECODE_DVITC_F1) ? "DVITC/F1 " : "",
                (tc_status & SV_VALIDTIMECODE_DVITC_F2) ? "DVITC/F2 " : "",
                tc_status
                );
            fflush(stdout);
        }

        // See if input signal restored
        if (videoin == SV_OK)
        {
            good_frames++;
            if (good_frames == required_good_frames)
            {
                if (verbose)
                {
                    printf("\n");
                }
                break;              // break from while(1) loop
            }
        }

        // tick in terms of frames, ignoring odd ticks representing fields
        int current_frame_tick = current_tick / 2;

        // Write dummy frames to chan's ring buffer and return tick of last frame written
        tick_last_dummy_frame = write_dummy_frames(sv, chan, current_frame_tick, tick_last_dummy_frame, tod_tc_read);

        usleep(18*1000);        // sleep slightly less than one tick (20,000)
    }

    return;
}

// channel number passed as void * (using cast)
void * sdi_monitor(void *arg)
{
    long                chan = (long)arg;
    sv_handle           *sv = a_sv[chan];
    sv_fifo             *poutput;
    sv_info             status_info;
    sv_storageinfo      storage_info;

    // Setup thread-specific data;
    td[chan].scale_context = NULL;
    td[chan].inFrame = avcodec_alloc_frame();
    td[chan].outFrame = avcodec_alloc_frame();

    // Get info on SDI capture
    SV_CHECK( sv_status( sv, &status_info) );

    SV_CHECK( sv_storage_status(sv,
                            0,
                            NULL,
                            &storage_info,
                            sizeof(storage_info),
                            0) );

    // To aid debugging, delay channels by a small amount of time so that
    // they appear in log files in channel 0..7 order
    if (chan > 0) {
        usleep(10000 + 5000 * chan);                
    }

#ifdef SV_FIFO_FLAG_NO_LIVE
    // No loop-through to avoid clash with player
    // (macro available in sdk3.2.14.0 and later versions)
    const int flagbase = SV_FIFO_FLAG_NO_LIVE;
#else
    const int flagbase = 0;
#endif

    SV_CHECK( sv_fifo_init( sv,
                            &poutput,   // Returned FIFO handle
                            1,          // Input
                            0,          // bShared (obsolete, must be zero)
                            1,          // DMA FIFO
                            flagbase,   // Base SV_FIFO_FLAG_xxxx flags
                            0) );       // nFrames (0 means use maximum)

    SV_CHECK( sv_fifo_start(sv, poutput) );

    logTF("chan %ld: fifo init/start completed\n", chan);

    if (0)
    {
        // Experiment with real-time scheduling
        show_scheduler();

        int my_scheduler = SCHED_RR;
        struct sched_param my_param;
        my_param.sched_priority = sched_get_priority_max(my_scheduler);

        sched_setscheduler(0, my_scheduler, &my_param);

        show_scheduler();
    }

    int recover_from_video_loss = 0;
    int last_res = -1;
    while (1)
    {
        int res;

        // write_picture() returns the result of sv_fifo_getbuffer()
        if ((res = write_picture(chan, sv, poutput, recover_from_video_loss)) != SV_OK)
        {
            // Display error only when things change
            if (res != last_res) {
                logTF("chan %ld: failed to capture video: (%d) %s\n", chan, res,
                    res == SV_ERROR_INPUT_VIDEO_NOSIGNAL ? "INPUT_VIDEO_NOSIGNAL" : sv_geterrortext(res));
            }
            last_res = res;

            SV_CHECK( sv_fifo_stop(sv, poutput, 0) );
            wait_for_good_signal(sv, chan, 25);

            // Note the channel's tick counter appears to pause while
            // the init() and start() take place
            //SV_CHECK( sv_fifo_init(sv, &poutput, TRUE, FALSE, TRUE, FALSE, 0) );
            SV_CHECK( sv_fifo_reset(sv, poutput) );
            SV_CHECK( sv_fifo_start(sv, poutput) );

            // flag that we need to recover from loss in video signal
            recover_from_video_loss = 1;
            continue;
        }

        // res will be SV_OK to reach this point
        if (res != last_res) {
            logTF("chan %ld: Video signal OK\n", chan);
        }
        recover_from_video_loss = 0;
        last_res = res;
    }

    return NULL;
}


void usage_exit(void)
{
    fprintf(stderr, "Usage: dvs_sdi [options]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "E.g.   dvs_sdi -c 2\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -f <video format>    uncompressed video format to store in ring buffer:\n");
    fprintf(stderr, "                         YUV422 - Planar YUV 4:2:2 16bpp (default)\n");
    fprintf(stderr, "                         DV50   - Planar YUV 4:2:2 with field order line shift\n");
    fprintf(stderr, "                         UYVY   - UYVY 4:2:2 16bpp\n");
    fprintf(stderr, "    -s <secondary fmt>   uncompressed video format for the secondary video buffer:\n");
    fprintf(stderr, "                         None   - secondary buffer disabled (default)\n");
    fprintf(stderr, "                         YUV422 - secondary buffer is planar YUV 4:2:2 16bpp\n");
    fprintf(stderr, "                         DV50   - secondary buffer is planar YUV 4:2:2 with field order line shift\n");
    fprintf(stderr, "                         MPEG   - secondary buffer is planar YUV 4:2:0\n");
    fprintf(stderr, "                         DV25   - secondary buffer is planar YUV 4:2:0 suitable for DV25\n");
    fprintf(stderr, "    -mode vid[:AUDIO8]   set input mode on all DVS cards, vid is one of:\n");
    fprintf(stderr, "                         PAL,NTSC,1920x1080i25,1920x1080i29,1280x720p50,1280x720p59\n");
    fprintf(stderr, "                         AUDIO8 enables 8 audio channels per SDI input\n");
    fprintf(stderr, "                         E.g. -mode 1920x1080i29:AUDIO8\n");
    fprintf(stderr, "    -sync <type>         set input sync type on all DVS cards, sync is one of:\n");
    fprintf(stderr, "                         bilevel   - analog bilevel sync [default for PAL/NTSC mode]\n");
    fprintf(stderr, "                         trilevel  - analog trilevel sync [default for HD modes]\n");
    fprintf(stderr, "                         internal  - freerunning\n");
    fprintf(stderr, "                         external  - sync to incoming SDI signal\n");
    fprintf(stderr, "    -tt <tc type>        preferred type of timecode to use: VITC, LTC, DVITC, DLTC, SYS [default VITC]\n");
    fprintf(stderr, "    -mc <master ch>      channel to use as timecode master: 0..7 [default -1 i.e. no master]\n");
    fprintf(stderr, "    -c <max channels>    maximum number of channels to use for capture\n");
    fprintf(stderr, "    -m <max memory MiB>  maximum memory to use for ring buffers in MiB [default use all available]\n");
    fprintf(stderr, "    -a8                  use 8 audio tracks per video channel\n");
    fprintf(stderr, "    -aes                 use AES/EBU audio with default routing\n");
    fprintf(stderr, "    -aes4                use AES/EBU audio with 4,4 routing\n");
    fprintf(stderr, "    -aes8                use AES/EBU audio with 8,8 routing\n");
    fprintf(stderr, "    -avsync              perform avsync analysis - requires clapper-board input video\n");
    fprintf(stderr, "    -b <video_sample>    benchmark using video_sample instead of captured UYVY video frames\n");
    fprintf(stderr, "    -q                   quiet operation (fewer messages)\n");
    fprintf(stderr, "    -v                   increase verbosity\n");
    fprintf(stderr, "    -ld                  logfile directory\n");
    fprintf(stderr, "    -d <channel>         channel number to print verbose debug messages for\n");
    fprintf(stderr, "    -h                   usage\n");
    fprintf(stderr, "\n");
    exit(1);
}

} // unnamed namespace

int main (int argc, char ** argv)
{
    int             n, max_channels = MAX_CHANNELS;
    long long       opt_max_memory = 0;
    const char      *logfiledir = ".";
    //int             opt_video_mode = -1;
    //int             current_video_mode = -1;
    int             opt_sync_type = -1;
    VideoRaster::EnumType mode_video_raster = VideoRaster::NONE;

    enum CaptureFmt { NONE, UYVY, YUV422, DV50, MPEG, DV25 };
    CaptureFmt primary_capture_format = YUV422;
    CaptureFmt secondary_capture_format = NONE;

    // Set local midnight time for calculation of system timecode
    time_t utc = time(NULL);
    struct tm local_tm;
    localtime_r(&utc, &local_tm);
    local_tm.tm_sec = 0;
    local_tm.tm_min = 0;
    local_tm.tm_hour = 0;
    midnight_microseconds = INT64_C(1000000) * mktime(&local_tm);

    // process command-line args
    for (n = 1; n < argc; n++)
    {
        if (strcmp(argv[n], "-v") == 0)
        {
            verbose++;
        }
        else if (strcmp(argv[n], "-q") == 0)
        {
            verbose = 0;
        }
        else if (strcmp(argv[n], "-h") == 0)
        {
            usage_exit();
        }
        else if (strcmp(argv[n], "-avsync") == 0)
        {
            test_avsync = 1;
        }
        else if (strcmp(argv[n], "-b") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            benchmark = 1;
            video_sample_file = argv[n+1];
            n++;
        }
        else if (strcmp(argv[n], "-ld") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            logfiledir = argv[n+1];
            n++;
        }
        else if (strcmp(argv[n], "-tt") == 0)
        {
            if (argc <= n+1) {
                usage_exit();
            }

            if (strcmp(argv[n+1], "VITC") == 0) {
                timecode_type = NexusTC_VITC;
            }
            else if (strcmp(argv[n+1], "LTC") == 0) {
                timecode_type = NexusTC_LTC;
            }
            else if (strcmp(argv[n+1], "DVITC") == 0) {
                timecode_type = NexusTC_DVITC;
            }
            else if (strcmp(argv[n+1], "DLTC") == 0) {
                timecode_type = NexusTC_DLTC;
            }
            else if (strcmp(argv[n+1], "SYS") == 0) {
                timecode_type = NexusTC_SYSTEM;
            }
            else {
                usage_exit();
            }
            n++;
        }
        else if (strcmp(argv[n], "-mc") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            if (sscanf(argv[n+1], "%d", &master_channel) != 1 ||
                master_channel > 7)
            {
                fprintf(stderr, "-mc requires channel number from 0..7 or -1 for no master\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-m") == 0)
        {
            if (sscanf(argv[n+1], "%lld", &opt_max_memory) != 1) {
                fprintf(stderr, "-m requires integer maximum memory in MB\n");
                return 1;
            }
            opt_max_memory = opt_max_memory * 1024 * 1024;
            n++;
        }
        else if (strcmp(argv[n], "-c") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            if (sscanf(argv[n+1], "%d", &max_channels) != 1 ||
                max_channels > MAX_CHANNELS || max_channels <= 0)
            {
                fprintf(stderr, "-c requires integer maximum channel number <= %d\n", MAX_CHANNELS);
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-mode") == 0)
        {
            char vidmode[256] = "", audmode[256] = "";
            if (sscanf(argv[n+1], "%[^:]:%s", vidmode, audmode) != 2) {
                if (sscanf(argv[n+1], "%s", vidmode) != 1) {
                    fprintf(stderr, "-mode requires option of the form videomode[:audiomode]\n");
                    return 1;
                }
            }

            if (strcmp(vidmode, "PAL") == 0)
            {
                mode_video_raster = VideoRaster::PAL;
            }
            else if (strcmp(vidmode, "NTSC") == 0)
            {
                mode_video_raster = VideoRaster::NTSC;
            }
            else if (strcmp(vidmode, "1920x1080i25") == 0)
            {
                mode_video_raster = VideoRaster::SMPTE274_25I;
            }
            else if (strcmp(vidmode, "1920x1080i29") == 0)
            {
                mode_video_raster = VideoRaster::SMPTE274_29I;
            }
            else if (strcmp(vidmode, "1280x720p50") == 0)
            {
                mode_video_raster = VideoRaster::SMPTE296_50P;
            }
            else if (strcmp(vidmode, "1280x720p59") == 0)
            {
                mode_video_raster = VideoRaster::SMPTE296_59P;
            }
            else
            {
                fprintf(stderr, "video mode \"%s\" not supported\n", vidmode);
                return 1;
            }

            // Default audio mode is 4 channels per SDI input
            if (strcmp(audmode, "AUDIO8") == 0) {
                audio8ch = 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-sync") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            if (strcmp(argv[n+1], "bilevel") == 0) {
                opt_sync_type = SV_SYNC_BILEVEL;
            }
            else if (strcmp(argv[n+1], "trilevel") == 0) {
                opt_sync_type = SV_SYNC_TRILEVEL;
            }
            else if (strcmp(argv[n+1], "internal") == 0) {
                opt_sync_type = SV_SYNC_INTERNAL;
            }
            else if (strcmp(argv[n+1], "external") == 0) {
                opt_sync_type = SV_SYNC_EXTERNAL;
            }
            else {
                usage_exit();
            }
            n++;
        }
        else if (strcmp(argv[n], "-d") == 0)
        {
            if (argc <= n+1)
                usage_exit();

            if (sscanf(argv[n+1], "%d", &verbose_channel) != 1 ||
                verbose_channel > 7 || verbose_channel < 0)
            {
                fprintf(stderr, "-d requires channel number {0..7}\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-f") == 0)
        {
            if (argc <= n+1)
            {
                usage_exit();
            }
            else if (strcmp(argv[n+1], "UYVY") == 0)
            {
                primary_capture_format = UYVY;
            }
            else if (strcmp(argv[n+1], "YUV422") == 0)
            {
                primary_capture_format = YUV422;
            }
            else if (strcmp(argv[n+1], "DV50") == 0)
            {
                primary_capture_format = DV50;
            }
            else
            {
                usage_exit();
            }
            n++;
        }
        else if (strcmp(argv[n], "-s") == 0)
        {
            if (argc <= n+1)
            {
                usage_exit();
            }
            else if (strcmp(argv[n+1], "None") == 0)
            {
                video_secondary_format = FormatNone;
            }
            else if (strcmp(argv[n+1], "YUV422") == 0)
            {
                secondary_capture_format = YUV422;
            }
            else if (strcmp(argv[n+1], "DV50") == 0)
            {
                secondary_capture_format = DV50;
            }
            else if (strcmp(argv[n+1], "MPEG") == 0)
            {
                secondary_capture_format = MPEG;
            }
            else if (strcmp(argv[n+1], "DV25") == 0)
            {
                secondary_capture_format = DV25;
            }
            else
            {
                usage_exit();
            }
            n++;
        }
        else if (strcmp(argv[n], "-a8") == 0)
        {
            audio8ch = 1;
        }
        else if (strcmp(argv[n], "-aes") == 0)
        {
            aes_audio = 1;
            aes_routing = 0;
        }
        else if (strcmp(argv[n], "-aes4") == 0)
        {
            aes_audio = 1;
            aes_routing = 4;
        }
        else if (strcmp(argv[n], "-aes8") == 0)
        {
            aes_audio = 1;
            aes_routing = 8;
        }
        else
        {
            fprintf(stderr, "unknown argument \"%s\"\n", argv[n]);
            return 1;
        }
    }


    char logfile[FILENAME_MAX];
    strcpy(logfile, logfiledir);
    strcat(logfile, "/dvs_sdi");
    openLogFileWithDate(logfile);

    // Install signal handlers to do clean exit
    if (signal(SIGINT, catch_sigint) == SIG_ERR)
    {
        perror("signal(SIGINT)");
        return 1;
    }
    if (signal(SIGTERM, catch_sigint) == SIG_ERR)
    {
        perror("signal(SIGTERM)");
        return 1;
    }
    if (signal(SIGHUP, catch_sigint) == SIG_ERR)
    {
        perror("signal(SIGHUP)");
        return 1;
    }
    if (signal(SIGUSR1, catch_sigusr1) == SIG_ERR)
    {
        perror("signal(SIGUSR1)");
        return 1;
    }

    // Set video mode if requested
    if (VideoRaster::NONE != mode_video_raster)
    {
        set_videomode_on_all_channels(max_channels, mode_video_raster, audio8ch ? 8 : 4);
    }

    //////////////////////////////////////////////////////
    // Attempt to open all sv cards
    //
    // card specified by string of form "PCI,card=n" where n = 0,1,2,3
    // For each card try to open the second channel "PCI,card=n,channel=1"
    // If you try "PCI,card=0,channel=0" on an old sdk you get SV_ERROR_SVOPENSTRING
    // If you see SV_ERROR_WRONGMODE, you need to "svram multichannel on"
    //

    // Check SDK multichannel capability
    if (check_sdk_version3())
    {
        //sv_info status_info;
        int card = 0;
        int channel = 0;
        for (card = 0; card < max_channels && channel < max_channels; card++)
        {
            char card_str[64] = {0};

            snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d,channel=0", card);
    
            int res = sv_openex(&a_sv[channel],
                                card_str,
                                SV_OPENPROGRAM_DEMOPROGRAM,
                                SV_OPENTYPE_INPUT,      // Open V+A input, not output too
                                0,
                                0);
            if (res == SV_ERROR_WRONGMODE)
            {
                // Multichannel mode not on so doesn't like "channel=0"
                snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d", card);
        
                res = sv_openex(&a_sv[channel],
                                    card_str,
                                    SV_OPENPROGRAM_DEMOPROGRAM,
                                    SV_OPENTYPE_INPUT,      // Open V+A input, not output too
                                    0,
                                    0);
            }
            if (res == SV_OK)
            {
                /*
                int alignment = 0;
                sv_query(a_sv[channel], SV_QUERY_DMAALIGNMENT, 0, &alignment);
                fprintf(stderr, "Minimum memory alignment for DMA transfers = %d\n", alignment);
                */

                //sv_status(a_sv[channel], &status_info);
                //sv_query(a_sv[channel], SV_QUERY_MODE_CURRENT, 0, &current_video_mode);
                //framerate_for_videomode(current_video_mode, &frame_rate_numer, &frame_rate_denom);
                VideoRaster::EnumType ch_raster = VideoRaster::NONE;
                get_video_raster(channel, ch_raster);
                int ch_width;
                int ch_height;
                int ch_fps_num;
                int ch_fps_den;
                Interlace::EnumType ch_interlace;
                VideoRaster::GetInfo(ch_raster, ch_width, ch_height, ch_fps_num, ch_fps_den, ch_interlace);

                logTF("card %d: device present (%dx%d %d/%d %s)\n",
                    card, ch_width, ch_height, ch_fps_num, ch_fps_den,
                    Interlace::NONE == ch_interlace ? "progressive" : "interlaced");

                if (channel == 0)
                {
                    // Set params from first channel
                    primary_video_raster = ch_raster;
                    width = ch_width;
                    height = ch_height;
                    frame_rate_numer = ch_fps_num;
                    frame_rate_denom = ch_fps_den;
                    interlace = ch_interlace;
                }
                else if (width != ch_width || height != ch_height
                    || frame_rate_numer != ch_fps_num || frame_rate_denom != ch_fps_den
                    || (interlace != ch_interlace))
                {
                    // Warn if other channels different from first
                    logTF("card %d: warning: different video raster!\n", card);
                }

                // Set AES input
                set_aes_option(channel, aes_audio);

                set_sync_option(channel, opt_sync_type, ch_width);

                // check for multichannel mode
                int multichannel_mode = 0;
                if ((res = sv_option_get(a_sv[channel], SV_OPTION_MULTICHANNEL, &multichannel_mode)) != SV_OK) {
                    logTF("card %d: sv_option_get(SV_OPTION_MULTICHANNEL) failed: %s\n", card, sv_geterrortext(res));
                }

                channel++;
                num_sdi_threads++;

                // If card has a multichannel mode on, open second channel
                if (multichannel_mode)
                {
                    if (channel < max_channels)
                    {
                        snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d,channel=1", card);
                        int res = sv_openex(&a_sv[channel],
                                    card_str,
                                    SV_OPENPROGRAM_DEMOPROGRAM,
                                    SV_OPENTYPE_INPUT,      // Open V+A input, not output too
                                    0,
                                    0);
                        if (res == SV_OK)
                        {
                            VideoRaster::EnumType ch_raster = VideoRaster::NONE;
                            get_video_raster(channel, ch_raster);
                            int ch_width;
                            int ch_height;
                            int ch_fps_num;
                            int ch_fps_den;
                            Interlace::EnumType ch_interlace;
                            VideoRaster::GetInfo(ch_raster, ch_width, ch_height, ch_fps_num, ch_fps_den, ch_interlace);

                            logTF("card %d: second channel (%dx%d %d/%d %s)\n",
                                card, ch_width, ch_height, ch_fps_num, ch_fps_den,
                                Interlace::NONE == ch_interlace ? "progressive" : "interlaced");

                            // Set AES audio routing
                            if (aes_audio)
                            {
                                int opt_value;
                                switch (aes_routing)
                                {
                                case 4:
                                    opt_value = SV_AUDIOAESROUTING_4_4;
                                    break;
                                case 8:
                                    opt_value = SV_AUDIOAESROUTING_8_8;
                                    break;
                                case 0:
                                default:
                                    opt_value = SV_AUDIOAESROUTING_DEFAULT;
                                    break;
                                }
                                res = sv_option_set(a_sv[channel-1], SV_OPTION_AUDIOAESROUTING, opt_value);
                                if (SV_OK != res)
                                {
                                    logTF("card %d: sv_option_set(SV_OPTION_AUDIOAESROUTING) failed: %s\n", card, sv_geterrortext(res));
                                }
                            }

                            // Set AES input
                            set_aes_option(channel, aes_audio);

                            // Don't set the sync option for the second channel (set_sync_option())
                            // since you get a SV_ERROR_JACK_INVALID error
                            // (perhaps this is a hardware limitation which will eventually go away).

                            channel++;
                            num_sdi_threads++;
                        }
                        else
                        {
                            logTF("card %d: failed opening second channel: %s\n", card, sv_geterrortext(res));
                        }
                    }
                }
                else
                {
                    logTF("card %d: multichannel mode off\n", card);
                }
    
            }
            else
            {
                // typical reasons include:
                //  SV_ERROR_DEVICENOTFOUND
                //  SV_ERROR_DEVICEINUSE
                logTF("card %d: sv_openex(%s) %s\n", card, card_str, sv_geterrortext(res));
                continue;
            }
        }
    }
    else
    {
        // older SDK
        //
        // card specified by string of form "PCI,card=n" where n = 0,1,2,3
        //
        //sv_info status_info;
        int card;
        for (card = 0; card < max_channels; card++)
        {
            char card_str[20] = {0};
    
            snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d", card);
    
            int res = sv_openex(&a_sv[card],
                                card_str,
                                SV_OPENPROGRAM_DEMOPROGRAM,
                                SV_OPENTYPE_INPUT,      // Open V+A input, not output too
                                0,
                                0);
            if (res == SV_OK)
            {
                VideoRaster::EnumType ch_raster = VideoRaster::NONE;
                get_video_raster(card, ch_raster);
                int ch_width;
                int ch_height;
                int ch_fps_num;
                int ch_fps_den;
                Interlace::EnumType ch_interlace;
                VideoRaster::GetInfo(ch_raster, ch_width, ch_height, ch_fps_num, ch_fps_den, ch_interlace);

                logTF("card %d: device present (%dx%d %d/%d %s)\n",
                    card, ch_width, ch_height, ch_fps_num, ch_fps_den,
                    Interlace::NONE == ch_interlace ? "progressive" : "interlaced");

                num_sdi_threads++;

                if (card == 0)
                {
                    // Set params from first channel
                    primary_video_raster = ch_raster;
                    width = ch_width;
                    height = ch_height;
                    frame_rate_numer = ch_fps_num;
                    frame_rate_denom = ch_fps_den;
                    interlace = ch_interlace;
                }
                else if (width != ch_width || height != ch_height
                    || frame_rate_numer != ch_fps_num || frame_rate_denom != ch_fps_den
                    || (interlace != ch_interlace))
                {
                    // Warn if other channels different from first
                    logTF("card %d: warning: different video raster!\n", card);
                }

                // Set AES input
                set_aes_option(card, aes_audio);

                set_sync_option(card, opt_sync_type, ch_width);
            }
            else
            {
                // typical reasons include:
                //  SV_ERROR_DEVICENOTFOUND
                //  SV_ERROR_DEVICEINUSE
                logTF("card %d: %s\n", card, sv_geterrortext(res));
            }
        }
    }

    // We now know primary_video_raster, width, height, frame_rate and interlace (from card).
    
    // Set fps and df for constructing Timecodes
    // Currently assuming drop frame for NTSC frame rate but this is not necessarily the case!
    fps = frame_rate_numer / frame_rate_denom;
    df = false;
    if (frame_rate_numer % frame_rate_denom > 0)
    {
        df = true;
        ++fps;
    }

    // Set secondary_video_raster
    if (NONE != secondary_capture_format)
    {
        // Secondary format is always SD even when primary is HD
        secondary_video_raster = sd_raster(primary_video_raster);
    }

    // Set pixel format needed for capture format and raster
    switch (primary_capture_format)
    {
    case UYVY:
        primary_pixel_format = Ingex::PixelFormat::UYVY_422;
        video_format = Format422UYVY;
        break;
    case YUV422:
        primary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_422;
        video_format = Format422PlanarYUV;
        break;
    case DV50:
        if (Ingex::VideoRaster::PAL == primary_video_raster)
        {
            primary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_422;
            primary_line_shift = 1;
            primary_video_raster = Ingex::VideoRaster::PAL_B;
            video_format = Format422PlanarYUVShifted;
        }
        else if (Ingex::VideoRaster::NTSC == primary_video_raster)
        {
            primary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_422;
            video_format = Format422PlanarYUV;
        }
        else
        {
            // Not valid
        }
        break;
    case MPEG:
    case DV25:
    case NONE:
        // Not valid as primary format
        break;
    }

    switch (secondary_capture_format)
    {
    case YUV422:
        secondary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_422;
        video_secondary_format = Format422PlanarYUV;
        break;
    case DV50:
        if (Ingex::VideoRaster::PAL == secondary_video_raster)
        {
            secondary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_422;
            secondary_line_shift = 1;
            secondary_video_raster = Ingex::VideoRaster::PAL_B;
            video_secondary_format = Format422PlanarYUVShifted;
        }
        else if (Ingex::VideoRaster::NTSC == secondary_video_raster)
        {
            secondary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_422;
            video_secondary_format = Format422PlanarYUV;
        }
        else
        {
            // Not valid
        }
        break;
    case MPEG:
        secondary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_420_MPEG;
        video_secondary_format = Format422PlanarYUV;
        break;
    case DV25:
        if (Ingex::VideoRaster::PAL == secondary_video_raster)
        {
            secondary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_420_DV;
            secondary_line_shift = 1;
            secondary_video_raster = Ingex::VideoRaster::PAL_B;
            video_secondary_format = Format420PlanarYUVShifted;
        }
        else if (Ingex::VideoRaster::NTSC == secondary_video_raster)
        {
            secondary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_411;
            video_secondary_format = Format411PlanarYUV;
        }
        else
        {
            // Not valid
        }
        break;
    case UYVY:
        // Not valid as secondary format
        break;
    case NONE:
        secondary_pixel_format = Ingex::PixelFormat::NONE;
        video_secondary_format = FormatNone;
        break;
    }


    if (num_sdi_threads < 1)
    {
        logTF("Error: No SDI monitor threads created.  Exiting\n");
        return 1;
    }

    /*
    // Display info on primary video format
    switch (video_format)
    {
        case Format422UYVY:
                logTF("Capturing video as UYVY (4:2:2)\n"); break;
        case Format422PlanarYUV:
                logTF("Capturing video as YUV 4:2:2 planar\n"); break;
        case Format422PlanarYUVShifted:
                logTF("Capturing video as YUV 4:2:2 planar with picture shifted down by one line\n"); break;
        default:
                logTF("Unsupported video buffer format\n");
                return 1;
    }

    // Display info on secondary video format
    switch (video_secondary_format)
    {
        case FormatNone:
                logTF("Secondary video buffer is disabled\n"); break;
        case Format422PlanarYUV:
                logTF("Secondary video buffer is YUV 4:2:2 planar\n"); break;
        case Format422PlanarYUVShifted:
                logTF("Secondary video buffer is YUV 4:2:2 planar with picture shifted down by one line\n"); break;
        case Format420PlanarYUV:
                logTF("Secondary video buffer is YUV 4:2:0 planar\n"); break;
        case Format420PlanarYUVShifted:
                logTF("Secondary video buffer is YUV 4:2:0 planar with picture shifted down by one line\n"); break;
        default:
                logTF("Unsupported Secondary video buffer format\n");
                return 1;
    }
    */

    // Ideally we would get the dma_video_size and audio_size parameters
    // from a fifo's pbuffer structure.  But you must complete a
    // successful sv_fifo_getbuffer() before those parameters are known.
    // Instead use the following values found experimentally since we need
    // to allocate buffers before successful dma transfers occur.

    // Need to know card type. (We assume all the same type.)
    bool dvs_dummy = false;
    int sn = 0;
    sv_query(a_sv[0], SV_QUERY_SERIALNUMBER, 0, &sn);
    sn /= 1000000;
    int extra_offset;
    switch (sn)
    {
    case 0:  // dvs_dummy
        dvs_dummy = true;
        extra_offset = 0;
        break;
    case 11: // SDStationOEM
    case 19: // SDStationOEMII
        extra_offset = 0;
        break;
    case 13: // Centaurus
    case 20: // CentaurusII PCI-X
    case 23: // CentaurusII PCIe
    default:
        switch (primary_video_raster)
        {
        case Ingex::VideoRaster::PAL:
        case Ingex::VideoRaster::PAL_B:
        case Ingex::VideoRaster::SMPTE274_25I:
        case Ingex::VideoRaster::SMPTE274_25P:
        case Ingex::VideoRaster::SMPTE296_50P:
        case Ingex::VideoRaster::SMPTE274_29I:
        case Ingex::VideoRaster::SMPTE274_29P:
        case Ingex::VideoRaster::SMPTE296_59P:
            extra_offset = 0x1800;
            break;
        case Ingex::VideoRaster::NTSC:
            extra_offset = 0x3400;
            break;
        default:
            extra_offset = 0;
            break;
        }
        break;
    }

    // Report on capture formats
    logTF("Primary capture   %s, %s\n", Ingex::VideoRaster::Name(primary_video_raster).c_str(), Ingex::PixelFormat::Name(primary_pixel_format).c_str());
    logTF("Secondary capture %s, %s\n", Ingex::VideoRaster::Name(secondary_video_raster).c_str(), Ingex::PixelFormat::Name(secondary_pixel_format).c_str());

    if (master_channel > max_channels-1)
    {
        logTF("Master channel number (%d) greater than highest channel number (%d)\n", master_channel, max_channels-1);
        return 1;
    }

    if (dvs_dummy && master_channel >= 0)
    {
        logTF("Master timecode disabled in dvs_dummy mode\n");
    }
    else if (master_channel < 0)
    {
        logTF("Master timecode not used\n");
    }
    else
    {
        logTF("Master timecode type is %s using channel %d\n",
            nexus_timecode_type_name(timecode_type), master_channel);
    }

    // Master timecode does not work particularly well in dummy mode and
    // is not needed, so we disable it.
    if (dvs_dummy)
    {
        master_channel = -1;
    }

    logTF("Using %s to determine number of frames to recover when video re-aquired\n",
            nexus_timecode_type_name(timecode_type));

    if (audio8ch)
    {
        logTF("Audio 8 channel mode enabled\n");
    }

    // Set the DMA size, taking into account the
    // empirically determined offsets.
    dma_video_size = width*height*2 + extra_offset;

    // Set up audio sizes for DMA transferred audio and reformatted secondary audio
    int audio_pair_size = 0x4000;
    if (audio8ch)
    {
        audio_size = audio_pair_size * 4;
    }
    else
    {
        audio_size = audio_pair_size * 2;
    }
    audio_offset = dma_video_size;

    // Max audio_size (8ch) is 0x10000      = 65536
    // Max audio data (8ch) is 1920 * 4 * 8 = 61440
    //
    // So we use the spare bytes at end for timecode and other per-frame data
    // (a historical artifact from when the dma_total_size was set to be identical
    // to element_size, in order to avoid perceived memory aligment issues)
    frame_data_offset = audio_offset + audio_size - sizeof(NexusFrameData);

    // An element in the ring buffer contains: video(4:2:2) + audio + video(4:2:0)
    dma_total_size = dma_video_size     // video frame as captured by dma transfer
                    + audio_size;       // DVS internal structure for audio channels

    // Compute size of secondary audio (always present as 16bit audio is always useful)
    if (audio8ch)
    {
        // PAL_AUDIO_SAMPLES is maximum of all SDI audio formats
        secondary_audio_size = 8 * PAL_AUDIO_SAMPLES * 2;
    }
    else
    {
        secondary_audio_size = 4 * PAL_AUDIO_SAMPLES * 2;
    }
    secondary_audio_offset = audio_offset + audio_size;

    // Compute size of secondary video (if any)
    int secondary_video_size = 0;
    if (Ingex::VideoRaster::NONE != secondary_video_raster)
    {
        int sec_fps_num;
        int sec_fps_den;
        Interlace::EnumType sec_interlace;
        VideoRaster::GetInfo(secondary_video_raster, sec_width, sec_height, sec_fps_num, sec_fps_den, sec_interlace);

        // Secondary video can be 4:2:2 or 4:2:0 so work out the correct size
        switch (secondary_pixel_format)
        {
        case Ingex::PixelFormat::YUV_PLANAR_422:
            secondary_video_size = sec_width * sec_height *  2;
            break;
        case Ingex::PixelFormat::YUV_PLANAR_420_MPEG:
        case Ingex::PixelFormat::YUV_PLANAR_420_DV:
        case Ingex::PixelFormat::YUV_PLANAR_411:
            secondary_video_size = sec_width * sec_height * 3 / 2;
            break;
        default:
            break;
        }
    }
	secondary_video_offset = secondary_audio_offset + secondary_audio_size;

    // Element size made up from DMA transferred buffer plus 16bit audio, plus secondary video (if any)
    //
    // TODO: Check whether element_size permit contiguous elements to be aligned on a large enough
    // boundary for DMA transfers.  Historically DMA required a 4096 (0x1000) boundary, but
    // sv_query(..., SV_QUERY_DMAALIGNMENT, ...) should tell us if padding is needed here.
    element_size = dma_total_size + secondary_audio_size + secondary_video_size;

    // Create "NO VIDEO" frame
    no_video_frame = (uint8_t*)malloc(width*height*2);
    if (Ingex::PixelFormat::UYVY_422 == primary_pixel_format)
    {
        // UYVY 422
        uyvy_no_video_frame(width, height, no_video_frame);
    }
    else if (Ingex::PixelFormat::YUV_PLANAR_422 == primary_pixel_format)
    {
        // Planar YUV 422
        uint8_t *tmp_frame = (uint8_t*)malloc(width*height*2);
        uyvy_no_video_frame(width, height, tmp_frame);

        // Repack to planar YUV 4:2:2
        uyvy_to_yuv422( width, height,
                        primary_line_shift,
                        tmp_frame,                      // input
                        no_video_frame);                // output

        free(tmp_frame);
    }
    else
    {
        // Shouldn't get here, above are the only possibilities
    }

    // Create "NO VIDEO" frame for secondary buffer
    if (Ingex::PixelFormat::NONE != secondary_pixel_format)
    {
        uint8_t *tmp_frame = (uint8_t*)malloc(sec_width*sec_height*2);
        uyvy_no_video_frame(sec_width, sec_height, tmp_frame);
        if (Ingex::PixelFormat::YUV_PLANAR_422 == secondary_pixel_format)
        {
            // Repack to planar YUV 4:2:2
            no_video_secondary_frame = (uint8_t*)malloc(sec_width*sec_height*2);
            uyvy_to_yuv422( sec_width, sec_height,
                            secondary_line_shift,
                            tmp_frame,                      // input
                            no_video_secondary_frame);      // output

        }
        else if (Ingex::PixelFormat::YUV_PLANAR_420_DV == secondary_pixel_format)
        {
            // Repack to planar YUV 4:2:0 for PAL DV
            no_video_secondary_frame = (uint8_t*)malloc(sec_width*sec_height*3/2);
            uyvy_to_yuv420_DV_sampling( sec_width, sec_height,
                            secondary_line_shift,           // should be 1
                            tmp_frame,                      // input
                            no_video_secondary_frame);      // output
        }
        else if (Ingex::PixelFormat::YUV_PLANAR_411 == secondary_pixel_format)
        {
            // Repack to planar YUV 4:1:1 for NTSC DV
            no_video_secondary_frame = (uint8_t*)malloc(sec_width*sec_height*3/2);
            uyvy_to_yuv411( sec_width, sec_height,
                            secondary_line_shift,           // should be 0
                            tmp_frame,                      // input
                            no_video_secondary_frame);      // output
        }
        else if (Ingex::PixelFormat::YUV_PLANAR_420_MPEG == secondary_pixel_format)
        {
            // Repack to planar YUV 4:2:0 for MPEG
            no_video_secondary_frame = (uint8_t*)malloc(sec_width*sec_height*3/2);
            uyvy_to_yuv420( sec_width, sec_height,
                            0,                              // no line shift
                            tmp_frame,                      // input
                            no_video_secondary_frame);      // output
        }
        else
        {
            // Shouldn't get here, above are the only possibilities
        }
        free(tmp_frame);
    }

    // Create benchmark video if specified
    if (benchmark) {
        benchmark_video = (uint8_t*)malloc(width*height*2);
        FILE *fp_sample = fopen(video_sample_file, "rb");
        if (! fp_sample) {
            fprintf(stderr, "Error opening benchmark video file \"%s\" ", video_sample_file);
            perror("fopen");
            return 1;
        }
        if (fread(benchmark_video, width*height*2, 1, fp_sample) != 1) {
            printf("Could not read sample video frame\n");
            return 1;
        }
        logTF("Read one frame (%dx%d) for use as benchmark frame from %s\n", width, height, video_sample_file);
    }

    // Allocate working buffers for video conversions (uvyv to 422 etc.)
    // This is faster than doing it in the shared memory.
    int chan;
    for (chan = 0; chan < MAX_CHANNELS; chan++)
    {
        if (chan < num_sdi_threads)
        {
            video_work_area[chan] = (uint8_t *) memalign(16, dma_total_size);
            if (!video_work_area[chan])
            {
                fprintf(stderr, "Failed to allocate video work buffer.\n");
                return 1;
            }
        }
        else
        {
            video_work_area[chan] = NULL;
        }
    }

    // Allocate shared memory buffers
    if (! allocate_shared_buffers(num_sdi_threads, opt_max_memory))
    {
        return 1;
    }

    for (chan = 0; chan < num_sdi_threads; chan++)
    {
        int err;
        logTF("chan %d: starting capture thread\n", chan);

        if ((err = pthread_create(&sdi_thread[chan], NULL, sdi_monitor, (void *)chan)) != 0)
        {
            logTF("chan %d: failed to create sdi_monitor thread: %s\n", chan, strerror(err));
            return 1;
        }
        else
        {
            //logTF("chan %d: started capture thread ok\n", chan);
        }
    }

    // Update the heartbeat 10 times a second
    p_control->owner_pid = getpid();
    while (1) {
        gettimeofday(&p_control->owner_heartbeat, NULL);
        usleep(100 * 1000);
    }
    pthread_join(sdi_thread[0], NULL);

    // Cleanup
    for (chan = 0; chan < MAX_CHANNELS; chan++)
    {
        free (video_work_area[chan]);
    }

    return 0; // silence gcc warning
}
