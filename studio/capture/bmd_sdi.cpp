/*
 * $Id: bmd_sdi.cpp,v 1.2 2012/01/24 15:18:42 john_f Exp $
 *
 * Record multiple SDI inputs to shared memory buffers.
 *
 * Copyright (C) 2005 - 2011 British Broadcasting Corporation
 * Copyright (C) 2010  Shigetaka Furukawa <moroheiya@aa-sys.co.jp>
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

#include "DeckLinkAPI.h"

#include "nexus_control.h"
#include "logF.h"
#include "video_VITC.h"
#include "video_conversion.h"
#include "video_test_signals.h"
#include "avsync_analysis.h"
#include "time_utils.h"
#include "yuvlib/YUV_scale_pic.h"
#include "Rational.h"

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <assert.h>

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

const bool DEBUG_OFFSETS = false;
const size_t DMA_ALIGN = 256;
const int MAX_AUDIO_DMA_PAIR_SIZE = 0x6000; // Size of audio DMA block can vary with SDK version
const int MAX_VIDEO_DMA_SIZE = 0x3F6000; // Size varies with raster and card type

// Option for selecting more efficient SV_FIFO_FLAG_AUDIOINTERLEAVED for audio capture
// Currently disabled as code out of date
bool AUDIO_INTERLEAVED = false;

const int PAL_AUDIO_SAMPLES = 1920;
#if 0
// This is the sequence required in D10 MXF OP1A
const unsigned int NTSC_AUDIO_SAMPLES[5] = { 1602, 1601, 1602, 1601, 1602 };
#else
// This is the sequence you get from a DVS card
const unsigned int NTSC_AUDIO_SAMPLES[5] = { 1602, 1602, 1602, 1602, 1600 };
#endif
//static int ntsc_audio_seq = 0;

const int64_t microseconds_per_day = 24 * 60 * 60 * INT64_C(1000000);

// Blackmagic
namespace
{
class CPThreadAutoLock;
class CBmdInputHandler;
int BmdSetupBoards(int channel, BMDDisplayMode displayMode);
void BmdCleanupObjects();
void BmdRunBoards();
void BmdStopBoards();
int BmdCheckAndUpdateVideoParameter(BMDDisplayMode displayMode);
int BmdGetInstalledBoards();
void BmdPrintAvailableBufferCount();
}

// Each thread uses the following thread-specific data
typedef struct {
    struct SwsContext* scale_context;
    AVFrame *inFrame;
    AVFrame *outFrame;
} SDIThreadData;

// Global variables for local context
namespace
{

int64_t midnight_microseconds = 0;

pthread_t       sdi_thread[MAX_CHANNELS] = {0};
SDIThreadData   td[MAX_CHANNELS];
NexusControl    *p_control = NULL;
uint8_t         *ring[MAX_CHANNELS] = {0};
int             control_id, ring_id[MAX_CHANNELS];
int             width = 0, height = 0;
int             video_size = 0;
int             sec_width = 0, sec_height = 0;
int             frame_rate_numer = 0, frame_rate_denom = 0;
VideoRaster::EnumType primary_video_raster = VideoRaster::NONE;
VideoRaster::EnumType secondary_video_raster = VideoRaster::NONE;
Interlace::EnumType interlace = Interlace::NONE;
Ingex::Rational image_aspect = Ingex::RATIONAL_16_9;
int element_size = 0, max_dma_size = 0;
int      primary_audio_offset = 0, primary_audio_size = 0;
int      secondary_audio_offset = 0, secondary_audio_size = 0;
int      secondary_video_offset = 0;

int             frame_data_offset = 0;

// Blackmagic
CBmdInputHandler*   bmd_Handlers[MAX_CHANNELS]  = {NULL};
bool        bmd_Setup75         = false;
bool        bmd_EnableD1DVCrop  = false;
int         bmd_NumChannels     = 1;
BMDVideoInputFlags  bmd_VideoInputFlags = bmdVideoInputFlagDefault;
// Bmd coms
IDeckLink*          bmd_pBmdArray[MAX_CHANNELS]         = {NULL};
IDeckLinkInput*     bmd_pBmdInputArray[MAX_CHANNELS]    = {NULL};
IDeckLinkConfiguration* bmd_pBmdConfigArray[MAX_CHANNELS]   = {NULL};

CaptureFormat   primary_video_format = Format422PlanarYUV; // Only retained for backward compatibility
CaptureFormat   secondary_video_format = FormatNone;  // Only retained for backward compatibility
Ingex::PixelFormat::EnumType primary_pixel_format = Ingex::PixelFormat::NONE;
Ingex::PixelFormat::EnumType secondary_pixel_format = Ingex::PixelFormat::NONE;
int primary_line_shift = 0;
int secondary_line_shift = 0;

int verbose = 0;
int verbose_channel = -1;    // which channel to show when verbose is on
bool show_card_info = false;
unsigned int naudioch = 2; // Decklink SDK only supports 2, 8 or 16
int test_avsync = 0;
uint8_t *dma_buffer[MAX_CHANNELS];
int benchmark = 0;
uint8_t *benchmark_video = NULL;
char *video_sample_file = NULL;
int aes_audio = 0;
int use_ffmpeg_hd_sd_scaling = 0;
int use_yuvlib_filter = 0;
uint8_t *hd2sd_interm[MAX_CHANNELS];
uint8_t *hd2sd_interm2[MAX_CHANNELS];
uint8_t *hd2sd_workspace[MAX_CHANNELS];

//pthread_mutex_t m_log = PTHREAD_MUTEX_INITIALIZER;      // logging mutex to prevent intermixing logs

NexusTimecode timecode_type = NexusTC_DVITC;  // default timecode is DVITC - others don't seem to work
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


// Functions in local context
namespace
{

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

    // control
    id = shmget(control_shm_key, sizeof(*p_control), 0444);
    if (id == -1)
    {
        fprintf(stderr, "No shmem id for control\n");
    }
    else if (shmctl(id, IPC_RMID, &shm_desc) == -1)
    {
        perror("shmctl(id, IPC_RMID):");
    }

    // channel buffers
    for (i = 0; i < bmd_NumChannels; i++)
    {
        id = shmget(channel_shm_key[i], sizeof(*p_control), 0444);
        if (id == -1)
        {
            fprintf(stderr, "No shmem id for channel %d\n", i);
            continue;
        }
        if (shmctl(id, IPC_RMID, &shm_desc) == -1)
        {
            perror("shmctl(id, IPC_RMID):");
        }
    }
}

void cleanup_exit(int res)
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

    // Clean up Bmd boards.
    BmdCleanupObjects();

    // delete shared memory
    cleanup_shared_mem();

    usleep(100 * 1000);     // 0.1 seconds

    printf("done - exiting\n");
    exit(res);
}

/*
int get_video_raster(int channel, VideoRaster::EnumType & video_raster)
{
    int dvs_mode;
    int ret = sv_query(a_sv[channel], SV_QUERY_MODE_CURRENT, 0, &dvs_mode);

    //fprintf(stderr, "channel %d mode %x\n", channel, dvs_mode);

    switch (dvs_mode & SV_MODE_MASK)
    {
    case SV_MODE_PAL:
        if (Ingex::RATIONAL_4_3 == image_aspect)
        {
            video_raster = VideoRaster::PAL_4x3;
        }
        else
        {
            video_raster = VideoRaster::PAL_16x9;
        }
        break;
    case SV_MODE_PALFF:
        if (Ingex::RATIONAL_4_3 == image_aspect)
        {
            video_raster = VideoRaster::PAL_592_4x3;
        }
        else
        {
            video_raster = VideoRaster::PAL_592_16x9;
        }
        break;
    case SV_MODE_PAL608:
        if (Ingex::RATIONAL_4_3 == image_aspect)
        {
            video_raster = VideoRaster::PAL_608_4x3;
        }
        else
        {
            video_raster = VideoRaster::PAL_608_16x9;
        }
        break;
    case SV_MODE_NTSC:
        if (Ingex::RATIONAL_4_3 == image_aspect)
        {
            video_raster = VideoRaster::NTSC_4x3;
        }
        else
        {
            video_raster = VideoRaster::NTSC_16x9;
        }
        break;
    case SV_MODE_NTSCFF:
        if (Ingex::RATIONAL_4_3 == image_aspect)
        {
            video_raster = VideoRaster::NTSC_502_4x3;
        }
        else
        {
            video_raster = VideoRaster::NTSC_502_16x9;
        }
        break;
    case SV_MODE_SMPTE274_25I:
        video_raster = VideoRaster::SMPTE274_25I;
        break;
    case SV_MODE_SMPTE274_25sF:
        video_raster = VideoRaster::SMPTE274_25PSF;
        break;
    case SV_MODE_SMPTE274_29I:
        video_raster = VideoRaster::SMPTE274_29I;
        break;
    case SV_MODE_SMPTE274_29sF:
        video_raster = VideoRaster::SMPTE274_29PSF;
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
*/

VideoRaster::EnumType sd_raster(VideoRaster::EnumType raster)
{
    VideoRaster::EnumType sd_raster;

    switch (raster)
    {
    case VideoRaster::PAL_4x3:
    case VideoRaster::PAL_4x3_B:
    case VideoRaster::PAL_16x9:
    case VideoRaster::PAL_16x9_B:
    case VideoRaster::PAL_592_4x3:
    case VideoRaster::PAL_592_4x3_B:
    case VideoRaster::PAL_592_16x9:
    case VideoRaster::PAL_592_16x9_B:
    case VideoRaster::PAL_608_4x3:
    case VideoRaster::PAL_608_4x3_B:
    case VideoRaster::PAL_608_16x9:
    case VideoRaster::PAL_608_16x9_B:
    case VideoRaster::NTSC_4x3:
    case VideoRaster::NTSC_16x9:
    case VideoRaster::NTSC_502_4x3:
    case VideoRaster::NTSC_502_16x9:
        sd_raster = raster;
        break;
    case VideoRaster::SMPTE274_25I:
    case VideoRaster::SMPTE274_25PSF:
    case VideoRaster::SMPTE296_50P:
        sd_raster = VideoRaster::PAL_16x9;
        break;
    case VideoRaster::SMPTE274_29I:
    case VideoRaster::SMPTE274_29PSF:
    case VideoRaster::SMPTE296_59P:
        sd_raster = VideoRaster::NTSC_16x9;
        break;
    default:
        sd_raster = VideoRaster::NONE;
        break;
    }

    return sd_raster;
}


void catch_sigusr1(int sig_number)
{
    (void) sig_number;
    // toggle a flag
}

void catch_sigint(int sig_number)
{
    printf("\nReceived signal %d - ", sig_number);
    cleanup_exit(0);
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
    printf("buffer size %lld (%.3f MiB), channels %d, calculated per channel ring_len %d\n", max_memory, max_memory / (1024*1024.0), num_channels, ring_len);

    printf("element_size=%d(0x%x) ring_len=%d (%.2f secs) (total=%lld)\n", element_size, element_size, ring_len, ring_len / 25.0, (long long)element_size * ring_len);
    if (ring_len < 10)
    {
        printf("ring_len=%d too small (< 10) - try increasing shmmax:\n", ring_len);
        printf("  echo 1073741824 >> /proc/sys/kernel/shmmax\n");
        return 0;
    }

    // Allocate memory for control structure which is fixed size
    if ((control_id = shmget(control_shm_key, sizeof(*p_control), IPC_CREAT | IPC_EXCL | 0666)) == -1)
    {
        if (errno == EEXIST)
        {
            fprintf(stderr, "shmget: shm control segment exists, deleting all related segments\n");
            cleanup_shared_mem();
            if ((control_id = shmget(control_shm_key, sizeof(*p_control), IPC_CREAT | IPC_EXCL | 0666)) == -1)
            {
                perror("shmget control key");
                fprintf(stderr, "Use\n\tipcs | grep '0x0000000[9abcd]'\nplus ipcrm -m <id> to cleanup\n");
                return 0;
            }
        }
        else
        {
            perror("shmget control key");
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

    p_control->pri_video_raster = primary_video_raster;
    p_control->pri_pixel_format = primary_pixel_format;
    p_control->pri_video_format = primary_video_format;
    p_control->width = width;
    p_control->height = height;
    
    p_control->sec_video_raster = secondary_video_raster;
    p_control->sec_pixel_format = secondary_pixel_format;
    p_control->sec_video_format = secondary_video_format;
    p_control->sec_width = sec_width;
    p_control->sec_height = sec_height;

    p_control->default_tc_type = timecode_type;
    p_control->master_tc_channel = master_channel;

    p_control->num_audio_tracks = naudioch;
    p_control->audio_offset = primary_audio_offset;
    p_control->audio_size = primary_audio_size;
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
        ring_id[i] = shmget(channel_shm_key[i], element_size * ring_len, IPC_CREAT | IPC_EXCL | 0666);
        if (ring_id[i] == -1)   /* shm error */
        {
            int save_errno = errno;
            fprintf(stderr, "Attemp to shmget for channel %d: ", i);
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


int BmdBcdToInt32(BMDTimecodeBCD srcBCD, BMDTimeValue numerator, BMDTimeValue denominator)
{
    float FramePerSec = static_cast<float>(numerator) / denominator;
    // Get components bcd
    int HoursBcd    = ((srcBCD & 0xFF000000) >> 24);
    int MinutesBcd  = ((srcBCD & 0x00FF0000) >> 16);
    int SecsBcd     = ((srcBCD & 0x0000FF00) >>  8);
    int FramesBcd   = ((srcBCD & 0x000000FF) >>  0);
    // Bcd to Hours.
    int Hours       = ((HoursBcd    & 0xF0 >> 4) * 10) + (HoursBcd      & 0x0F);
    int Minutes     = ((MinutesBcd  & 0xF0 >> 4) * 10) + (MinutesBcd    & 0x0F);
    int Secs        = ((SecsBcd     & 0xF0 >> 4) * 10) + (SecsBcd       & 0x0F);
    int Frames      = ((FramesBcd   & 0xF0 >> 4) * 10) + (FramesBcd     & 0x0F);
    // Get Tc total
    int TcTotal =   static_cast<int>(Hours  * (FramePerSec * 60 * 60) +
                                    Minutes * (FramePerSec * 60) +
                                    Secs    * (FramePerSec) +
                                    Frames);
    return TcTotal;
}

int FramesToBcd(int srcFrames, BMDTimeValue numerator, BMDTimeValue denominator)
{
    int FramePerSec = (numerator / denominator);
    //FramePerSec += (numerator % denominator);
    int Frames  = static_cast<int>(srcFrames % FramePerSec);
    int Hours   = static_cast<int>(srcFrames / (60 * 60 * FramePerSec));
    int Minutes = static_cast<int>((srcFrames - (Hours * 60 * 60 * FramePerSec)) / (60 * FramePerSec) );
    int Secs    = static_cast<int>((srcFrames - (Hours * 60 * 60 * FramePerSec) - (Minutes * 60 * FramePerSec)) / FramePerSec);
    
    uint8_t RawTcBcd[4];
    RawTcBcd[0] = ((Frames  / 10) << 4) + (Frames  % 10);
    RawTcBcd[1] = ((Secs    / 10) << 4) + (Secs    % 10);
    RawTcBcd[2] = ((Minutes / 10) << 4) + (Minutes % 10);
    RawTcBcd[3] = ((Hours   / 10) << 4) + (Hours   % 10);
    //int32_t RawBcd = 0;
    //RawBcd |= ( (((Frames  / 10) << 4) + (Frames  % 10)) );
    //RawBcd |= ( (((Secs    / 10) << 4) + (Secs    % 10)) << 8);
    //RawBcd |= ( (((Minutes / 10) << 4) + (Minutes % 10)) << 16);
    //RawBcd |= ( (((Hours   / 10) << 4) + (Hours   % 10)) << 24 );
    int32_t Result = static_cast<int32_t>(*RawTcBcd);
    return Result;
}

void print_GetTimecode_result(HRESULT hr, const char * tc_name)
{
    switch (hr)
    {
    case S_OK:
        fprintf(stderr,"Read %s OK\n", tc_name);
        break;
    case S_FALSE:
        fprintf(stderr,"%s not present or valid\n", tc_name);
        break;
    case E_FAIL:
        fprintf(stderr,"Failed to read %s\n", tc_name);
        break;
    case E_ACCESSDENIED:
        fprintf(stderr,"%s invalid or unsupported\n", tc_name);
        break;
    }
}

//! /brief AutoLock handlers
class CPThreadAutoLock
{
private: 
    pthread_mutex_t* m_pLock;
public:
    CPThreadAutoLock(pthread_mutex_t* pLock)
    {
        m_pLock = pLock;
        PTHREAD_MUTEX_LOCK(m_pLock)
    }
    ~CPThreadAutoLock()
    {
        PTHREAD_MUTEX_UNLOCK(m_pLock)
    }
};

//! /brief BlackMagicDesign DeckLink/Intensity Input handler class
class CBmdInputHandler
        : public IDeckLinkInputCallback
{
private:
    int m_RefCount;
    int m_Index;
    // Locks
    pthread_mutex_t m_Lock;
    volatile bool m_InProcessing;
    
public:
    //! /brief Constructor
    CBmdInputHandler(int index)
        : m_RefCount(0)
        , m_InProcessing(false)
    {
        m_Index = index;
        //m_Lock    = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_init(&m_Lock, NULL);
    }
    //! /brief Destructor
    ~CBmdInputHandler()
    {
        pthread_mutex_destroy(&m_Lock);
    }
    
    //! /brief IUnknown::QueryInterface
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv)
    {
        (void) iid;
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    //! /brief IUnknown::AddRef
    virtual ULONG STDMETHODCALLTYPE AddRef(void)
    {
        m_RefCount++;
        return 1;
    }
    //! /brief IUnknown::Release
    virtual ULONG STDMETHODCALLTYPE  Release(void)
    {
        m_RefCount--;
        return 1;
    }
    
    //! /brief IDeckLinkInputCallback::VideoInputFormatChanged
    virtual HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents events, IDeckLinkDisplayMode* newDisplayMode, BMDDetectedVideoInputFormatFlags formatFlags)
    {
        (void) newDisplayMode;
        fprintf(stderr, "Info: Input format changed. Event:0x%x FormatFlag:0x%x", events, formatFlags); 
        return S_OK;
    }
    
    //! /brief IDeckLinkInputCallback::VideoInputFrameArrived
    virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket)
    {
        HRESULT     hr = S_OK;
        
        void*       VideoPointer            = NULL;
        uint8_t*    VideoBytes              = NULL;
        long        SrcRowBytes             = 0;
        long        SrcWidth                = 0;
        long        SrcHeight               = 0;
        void*       AudioPointer            = NULL;
        uint8_t*    AudioBytes              = NULL;
        long        AudioSampleCount        = 0;
        int             ring_len            = p_control->ringlen;
        NexusBufCtl*    pc  = &(p_control->channel[m_Index]);
        NexusFrameData* FrameData           = reinterpret_cast<NexusFrameData*>( ring[m_Index] + (element_size * ((pc->lastframe + 1) % ring_len) + frame_data_offset) );
        int             CurrentLastFrame    = pc->lastframe;
        IDeckLinkTimecode* BmdTimeCode      = NULL;
        
        if (videoFrame == NULL)
        {
            return E_FAIL;
        }
        
        // Get Frame buffer pointer
        hr = videoFrame->GetBytes(&VideoPointer);
        SrcWidth    = videoFrame->GetWidth();
        SrcHeight   = videoFrame->GetHeight();

        if (bmd_EnableD1DVCrop && (SrcWidth == 720) && (SrcHeight == 486) )
        {
            VideoBytes = reinterpret_cast<uint8_t*>(VideoPointer) + (SrcRowBytes * 3);
        }
        else
        {
            VideoBytes = reinterpret_cast<uint8_t*>(VideoPointer);
        }
        if (VideoBytes == NULL)
        {
            return E_FAIL;
        }
        
        // In Processing
        CPThreadAutoLock lck(&m_Lock);
        m_InProcessing = true;

        // get video frame stats
        SrcRowBytes = videoFrame->GetRowBytes();
        
        //logTF("Got video frame %ldx%ld\n", SrcWidth, SrcHeight);

        // Put video in dma_dest (as it would be in DVS version)
        uint8_t * dma_dest = dma_buffer[m_Index];
        memcpy(dma_dest, VideoBytes, SrcRowBytes * height);

        // Destination in ring buffer
        uint8_t * vid_dest = ring[m_Index] + element_size * ((pc->lastframe + 1) % ring_len);

        // DMA transfer gives video as UYVY.  Convert to Planar YUV if required.
        if (Ingex::PixelFormat::YUV_PLANAR_422 == primary_pixel_format)
        {
            uint8_t *vid_input = dma_dest;

            // Use hard-to-code picture when benchmarking
            if (benchmark)
            {
                vid_input = benchmark_video;
            }

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
        }
        else
        {
            // No video reformat required, we just copy.
            memcpy(vid_dest, dma_dest, video_size);
        }

        // Convert primary video into secondary video buffer
        if (Ingex::PixelFormat::NONE != secondary_pixel_format)
        {
            unsigned char *reformat_input_buffer = dma_dest;

            if (width > 720)
            {
                // downscale HD primary to SD secondary

                // use intermediate buffer if a pixel reformat is required
                unsigned char *scale_output_buffer;
                if (Ingex::PixelFormat::UYVY_422 != secondary_pixel_format)
                {
                    scale_output_buffer = hd2sd_interm[m_Index];
                }
                else
                {
                    scale_output_buffer = vid_dest + secondary_video_offset;
                }

                if (use_ffmpeg_hd_sd_scaling)
                {
                    // FFmpeg scaling. Doesn't properly handle interlaced pictures
                    td[m_Index].scale_context = sws_getCachedContext(td[m_Index].scale_context,
                            width, height,
                            PIX_FMT_UYVY422,
                            sec_width, sec_height,
                            PIX_FMT_UYVY422,
                            SWS_FAST_BILINEAR,
                            NULL, NULL, NULL);
                            // other flags include: SWS_FAST_BILINEAR, SWS_BILINEAR, SWS_BICUBIC, SWS_POINT
                            // SWS_AREA, SWS_BICUBLIN, SWS_GAUSS, ... SWS_PRINT_INFO (debug)
                    avpicture_fill( (AVPicture*)td[m_Index].inFrame,
                                    dma_dest,                                   // input dma video
                                    PIX_FMT_UYVY422,
                                    width, height);
                    avpicture_fill( (AVPicture*)td[m_Index].outFrame,
                                    scale_output_buffer,                        // output to secondary video
                                    PIX_FMT_UYVY422,
                                    sec_width, sec_height);

                    sws_scale(td[m_Index].scale_context,
                                    td[m_Index].inFrame->data, td[m_Index].inFrame->linesize,
                                    0, height,
                                    td[m_Index].outFrame->data, td[m_Index].outFrame->linesize);
                }
                else
                {
                    // YUVlib scaling. Slower than FFmpeg if use_yuvlib_filter equals true, but handles interlaced pictures
                    YUV_frame yuv_hd_frame, yuv_sd_frame;
                    YUV_frame_from_buffer(&yuv_hd_frame, dma_dest, width, height, UYVY);
                    YUV_frame_from_buffer(&yuv_sd_frame, scale_output_buffer, sec_width, sec_height, UYVY);

    #if 0
                    scale_pic(&yuv_hd_frame, &yuv_sd_frame,
                              0, 0, sec_width, sec_height,
                              interlace,
                              use_yuvlib_filter, use_yuvlib_filter,
                              hd2sd_workspace[chan]);
    #else
    // More efficient if we first scale by a factor of 2 horizontally
                    YUV_frame yuv_int_frame;
                    YUV_frame_from_buffer(&yuv_int_frame, hd2sd_interm2[m_Index], width/2, sec_height, UYVY);

                    scale_pic(&yuv_hd_frame, &yuv_int_frame,
                              0, 0, width/2, sec_height,
                              interlace,
                              use_yuvlib_filter, use_yuvlib_filter,
                              hd2sd_workspace[m_Index]);

                    scale_pic(&yuv_int_frame, &yuv_sd_frame,
                              0, 0, sec_width, sec_height,
                              interlace,
                              use_yuvlib_filter, use_yuvlib_filter,
                              hd2sd_workspace[m_Index]);
    #endif
                }


                reformat_input_buffer = scale_output_buffer;
            }

            // reformat for SD secondary
            if (Ingex::PixelFormat::YUV_PLANAR_422 == secondary_pixel_format)
            {
                // Repack to planar YUV 4:2:2
                uyvy_to_yuv422( sec_width, sec_height,
                                secondary_line_shift,
                                reformat_input_buffer,                      // input
                                vid_dest + secondary_video_offset);         // output
            }
            else if (Ingex::PixelFormat::YUV_PLANAR_420_DV == secondary_pixel_format)
            {
                // Downsample and repack to planar YUV 4:2:0 for PAL DV
                uyvy_to_yuv420_DV_sampling( sec_width, sec_height,
                            secondary_line_shift,                       // should be true
                            reformat_input_buffer,                      // input
                            vid_dest + secondary_video_offset);         // output
            }
            else if (Ingex::PixelFormat::YUV_PLANAR_411 == secondary_pixel_format)
            {
                // Downsample and repack to planar YUV 4:1:1 for NTSC DV
                uyvy_to_yuv411( sec_width, sec_height,
                            secondary_line_shift,                       // should be false
                            reformat_input_buffer,                      // input
                            vid_dest + secondary_video_offset);         // output
            }
            else if (Ingex::PixelFormat::YUV_PLANAR_420_MPEG == secondary_pixel_format)
            {
                // Downsample and repack to planar YUV 4:2:0 for MPEG
                uyvy_to_yuv420( sec_width, sec_height,
                            0,                                          // no DV25 line shift
                            reformat_input_buffer,                      // input
                            vid_dest + secondary_video_offset);         // output
            }
        }
        
        // Copy audio data
        // Audio channels are interleaved
        if (audioPacket != NULL)
        {
            AudioSampleCount = audioPacket->GetSampleFrameCount();
            //logTF("Got audio data %ld samples\n", AudioSampleCount);
            hr = audioPacket->GetBytes(&AudioPointer);
            AudioBytes = reinterpret_cast<uint8_t*>(AudioPointer);
            
            if (AudioSampleCount != 0 && AudioBytes != 0)
            {
                if (0)
                {
                    // Debug
                    logTF("Got audio data [%d] %ld samples, %d channels\n", m_Index, AudioSampleCount, naudioch);
                    for (int i = 0; i < 8; ++i)
                    {
                        fprintf(stderr, " %02x", *(AudioBytes + i));
                    }
                    fprintf(stderr, "\n");
                }

                // Copy and de-interleave
                // primary 32-bit audio and secondary 16-bit audio
                int32_t * src = reinterpret_cast<int32_t*>(AudioPointer);
                int32_t * pdest0 = reinterpret_cast<int32_t *>(vid_dest + primary_audio_offset);
                int32_t * pdest[MAX_AUDIO_TRACKS];
                int16_t * sdest0 = reinterpret_cast<int16_t *>(vid_dest + secondary_audio_offset);
                int16_t * sdest[MAX_AUDIO_TRACKS];
                for (unsigned int ch = 0; ch < naudioch; ++ch)
                {
                    pdest[ch] = pdest0 + ch * PAL_AUDIO_SAMPLES;
                    sdest[ch] = sdest0 + ch * PAL_AUDIO_SAMPLES;
                }
                for (unsigned int i = 0; i < AudioSampleCount; ++i)
                {
                    for (unsigned int ch = 0; ch < naudioch; ++ch)
                    {
                        *pdest[ch]++ = (*src);
                        *sdest[ch]++ = (*src++) >> 16;
                    }
                }
            }
        }
        else
        {
            // No audio present in signal, so add silence
            memset(vid_dest + primary_audio_offset, 0, primary_audio_size);
            memset(vid_dest + secondary_audio_offset, 0, secondary_audio_size);
        }
        
        
        // Update timecodes etc.
        FrameData = reinterpret_cast<NexusFrameData*>(vid_dest + frame_data_offset);

        uint8_t hh, mm, ss, ff;
        BMDTimecodeFlags flags = bmdTimecodeFlagDefault;;

        // LTC not supported
        FrameData->tc_ltc = Ingex::Timecode();
        FrameData->ltc = 0;

        // VITC
        hr = videoFrame->GetTimecode(bmdTimecodeVITC, &BmdTimeCode);
        if (BmdTimeCode)
        {
            BmdTimeCode->GetComponents(&hh, &mm, &ss, &ff);
            flags = BmdTimeCode->GetFlags();
            Ingex::Timecode timecode(hh, mm, ss, ff, 0, frame_rate_numer, frame_rate_denom, bmdTimecodeIsDropFrame & flags);
            FrameData->tc_vitc = timecode;
            FrameData->vitc = timecode.FramesSinceMidnight();
        }
        //print_GetTimecode_result(hr, "VITC");

        // ANC VITC
        hr = videoFrame->GetTimecode(bmdTimecodeRP188VITC1, &BmdTimeCode);
        if (BmdTimeCode)
        {
            BmdTimeCode->GetComponents(&hh, &mm, &ss, &ff);
            flags = BmdTimeCode->GetFlags();
            Ingex::Timecode timecode(hh, mm, ss, ff, 0, frame_rate_numer, frame_rate_denom, bmdTimecodeIsDropFrame & flags);
            FrameData->tc_dvitc = timecode;
            FrameData->dvitc = timecode.FramesSinceMidnight();
        }
        //print_GetTimecode_result(hr, "ANC VITC");

        // ANC LTC
        hr = videoFrame->GetTimecode(bmdTimecodeRP188LTC, &BmdTimeCode);
        if (BmdTimeCode)
        {
            BmdTimeCode->GetComponents(&hh, &mm, &ss, &ff);
            flags = BmdTimeCode->GetFlags();
            Ingex::Timecode timecode(hh, mm, ss, ff, 0, frame_rate_numer, frame_rate_denom, bmdTimecodeIsDropFrame & flags);
            FrameData->tc_dltc = timecode;
            FrameData->dltc = timecode.FramesSinceMidnight();
        }
        //print_GetTimecode_result(hr, "ANC LTC");

        // SYS
        int frames = 0;
        if (1)
        {
            // Time now rather than time of capture
            int64_t SystemTime      = gettimeofday64();
            int64_t SystemTime_us   = SystemTime - (today_midnight_time() * INT64_C(1000000) );
            frames = static_cast<int>( (SystemTime_us * frame_rate_numer) / (INT64_C(1000000) * frame_rate_denom) );
        }
        else
        {
            // Time of capture with origin at start of capture
            BMDTimeValue HwFrameTime;
            BMDTimeValue HwFrameDuration;
            hr = videoFrame->GetStreamTime(&HwFrameTime, &HwFrameDuration, frame_rate_numer);
            if (S_OK == hr && 0 != HwFrameDuration)
            {
                frames = HwFrameTime / (HwFrameDuration * frame_rate_denom);;
            }
        }
        Ingex::Timecode timecode(frames, frame_rate_numer, frame_rate_denom, bmdTimecodeIsDropFrame & flags);
        FrameData->tc_systc = timecode;
        FrameData->systc = timecode.FramesSinceMidnight();

        // Audio samples and frame number
        FrameData->num_aud_samp = AudioSampleCount;
        FrameData->frame_number = CurrentLastFrame;

        // Hardcoded
        pc->hwtemperature = 30;
        pc->hwdrop = 0;

        // Signal to valid
        FrameData->signal_ok    = true;
        
        // signal frame is now ready
        PTHREAD_MUTEX_LOCK(&pc->m_lastframe)
        pc->lastframe++;
        PTHREAD_MUTEX_UNLOCK(&pc->m_lastframe)

        // exit processing
        m_InProcessing = false;
        return S_OK;
    }
    
    //! /brief Class is in processing.
    bool IsProcessing()
    {
        return m_InProcessing;
    }
    
    int UpdateOrGetMasterTimecode(int timeCodeAsInt, int64_t recTimeOfDay, int64_t* pDiffToMaster)
    {
        if (master_channel >= 0)
        {
            if (m_Index == master_channel)
            {
                CPThreadAutoLock TcLock(&m_master_tc);
                master_tc   = Ingex::Timecode(timeCodeAsInt, master_tc.FrameRateNumerator(), master_tc.FrameRateDenominator(), false);
                master_tod  = recTimeOfDay;
                // return this as master timecode
                return timeCodeAsInt;
            }
            else
            {
                CPThreadAutoLock TcLock(&m_master_tc);
                Ingex::Timecode derived_tc = master_tc;
                int64_t diff_to_master = 0;
                // if master_tod is not set, derived_tc is also left as 0
                if (master_tod != 0)
                {
                    // compute offset between this chan's recorded frame's time-of-day and master's
                    diff_to_master  = recTimeOfDay - master_tod;

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
                // return diff_to_master for logging purposes
                if (pDiffToMaster != NULL)
                {
                    *pDiffToMaster = diff_to_master;
                }
                return derived_tc.FramesSinceMidnight();
            }
        }
        // not set master timecode, return this.
        return timeCodeAsInt;
    }
};

int BmdSetupBoards(int channel, BMDDisplayMode displayMode)
{
    HRESULT hr = S_OK;
    // Create iterator
    IDeckLinkIterator* pIterator = CreateDeckLinkIteratorInstance();
    if (pIterator == NULL)
    {
        hr = E_FAIL;
        fprintf(stderr, "Error: Can not create DeckLink Iterator. Please check DeckLink install is correctly.\n");
        goto end;
    }
    
    for (int i = 0; (i < channel) && (hr == 0); i++)
    {
        hr = pIterator->Next(&bmd_pBmdArray[i]);
        if (hr == 0)
        {
            // Create input.
            if ( S_OK != (hr = bmd_pBmdArray[i]->QueryInterface(IID_IDeckLinkInput,
                            reinterpret_cast<void**>(&bmd_pBmdInputArray[i]))) )
            {
                fprintf(stderr, "Error: Can not create DeckLink Input on %d Result:0x%x\n", i, hr);
                goto end;
            }
            // Config Bmd board.
            if ( S_OK == (hr = bmd_pBmdArray[i]->QueryInterface(IID_IDeckLinkConfiguration,
                reinterpret_cast<void**>(&bmd_pBmdConfigArray[i]))) )
            {
                if (bmd_pBmdConfigArray[i] != NULL)
                {
                    hr = bmd_pBmdConfigArray[i]->SetInt(bmdDeckLinkConfigVideoInputConnection, bmdVideoConnectionSDI);
                    hr = bmd_pBmdConfigArray[i]->SetInt(bmdDeckLinkConfigAudioInputConnection, bmdAudioConnectionEmbedded);
                    hr = bmd_pBmdConfigArray[i]->SetFlag(bmdDeckLinkConfig444SDIVideoOutput, false);
                    hr = bmd_pBmdConfigArray[i]->SetFlag(bmdDeckLinkConfig3GBpsVideoOutput, false);
                    if (bmd_Setup75)
                    {
                        //hr = bmd_pBmdConfigArray[i]->SetAnalogVideoInputFlags(bmdAnalogVideoFlagCompositeSetup75);
                    }
                    else
                    {
                        //hr = bmd_pBmdConfigArray[i]->SetAnalogVideoInputFlags(0);
                    }
                }
            }
            // Setup handlers
            bmd_Handlers[i] = new CBmdInputHandler(i);
            if ( S_OK != (hr = bmd_pBmdInputArray[i]->SetCallback(bmd_Handlers[i])) )
            {
                fprintf(stderr, "Error: Can not set callback. Board:%d Result:0x%x\n", i, hr);
                goto end;
            }
            
            // Enable video
            if ( S_OK != (hr = bmd_pBmdInputArray[i]->EnableVideoInput(displayMode, bmdFormat8BitYUV, bmd_VideoInputFlags)) )
            {
                fprintf(stderr, "Error: Can not enable and set input Video display mode. Board:%d Result:0x%x\n", i, hr);
                goto end;
            }
            // Enable audio
            if ( S_OK != (hr = bmd_pBmdInputArray[i]->EnableAudioInput(bmdAudioSampleRate48kHz, bmdAudioSampleType32bitInteger, naudioch)) )
            {
                fprintf(stderr, "Error: Can not enable Audio. Board:%d Result:0x%x\n", i, hr);
                goto end;
            }
            // get model name
            char* DeviceNameString = NULL;
            hr = bmd_pBmdArray[i]->GetModelName(const_cast<const char**>(&DeviceNameString));
            fprintf(stderr, "Info: Create DeckLink Input:%d ModelName:%s.\n", i, DeviceNameString);
            free(DeviceNameString);
            // setup thread data
            td[i].scale_context = NULL;
            td[i].inFrame = avcodec_alloc_frame();
            td[i].outFrame = avcodec_alloc_frame();
        }
    }
    
end:
    if (pIterator != NULL)
    {
        pIterator->Release();
        pIterator = NULL;
    }
    if (hr != S_OK)
    {
        BmdCleanupObjects();
        return 1;
    }
    return 0;
}

void BmdCleanupObjects()
{
    HRESULT hr = S_OK;
    // Stop Bmd board first.
    BmdStopBoards();
    
    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        // clean up input
        if (bmd_pBmdInputArray[i] != NULL)
        {
            // cause SIGABORT?
            hr = bmd_pBmdInputArray[i]->FlushStreams();
            hr = bmd_pBmdInputArray[i]->DisableAudioInput();
            hr = bmd_pBmdInputArray[i]->DisableVideoInput();
            bmd_pBmdInputArray[i]->Release();
            bmd_pBmdInputArray[i] = NULL;
        }
        if (bmd_pBmdConfigArray[i] != NULL)
        {
            bmd_pBmdConfigArray[i]->Release();
            bmd_pBmdConfigArray[i] = NULL;
        }
        if (bmd_pBmdArray[i] != NULL)
        {
            bmd_pBmdArray[i]->Release();
            bmd_pBmdArray[i] = NULL;
        }
        // clean up handlers after decklink object is clean
        if (bmd_Handlers[i] != NULL)
        {
            // wait process is complete.
            while(bmd_Handlers[i]->IsProcessing());
            bmd_Handlers[i]->~CBmdInputHandler();
            bmd_Handlers[i] = NULL;
        }
    }
}

void BmdRunBoards()
{
    HRESULT hr = S_OK;
    for (int i = 0; (i < MAX_CHANNELS); i++)
    {
        if (bmd_pBmdInputArray[i] != NULL)
        {
            if ( S_OK != (hr = bmd_pBmdInputArray[i]->StartStreams()) )
            {
                fprintf(stderr, "Error: Can not start streams. Board:%d Result:0x%x\n", i, hr);
            }
            else
            {
                fprintf(stderr, "Info: Start streams. Board:%d Result:0x%x\n", i, hr);
            }
        }
    }
}

void BmdStopBoards()
{
    HRESULT hr = S_OK;
    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        if (bmd_pBmdInputArray[i] != NULL)
        {
            // wait process is complete.
            while(bmd_Handlers[i]->IsProcessing())
            {
                usleep(100 * 100);
            }
            if ( S_OK != (hr = bmd_pBmdInputArray[i]->StopStreams()) )
            {
                fprintf(stderr, "Error: Can not start streams. Board:%d Result:0x%x\n", i, hr);
            }
            else
            {
                fprintf(stderr, "Info: Stop streams. Board:%d Result:0x%x\n", i, hr);
            }
        }
    }
}

int BmdCheckAndUpdateVideoParameter(BMDDisplayMode displayMode)
{
    HRESULT hr = S_OK;
    bool    Result = false;
    IDeckLinkIterator*              pIterator = CreateDeckLinkIteratorInstance();
    IDeckLink*                      pBmd = NULL;
    IDeckLinkInput*                 pBmdInput = NULL;
    IDeckLinkDisplayModeIterator*   pDisplayIterator = NULL;
    IDeckLinkDisplayMode*           pDisplayMode = NULL;
    
    if (pIterator == NULL)
    {
        fprintf(stderr, "Error: Can not create DeckLink Iterator Result:0x%x\n", hr);
        goto end;
    }
    hr = pIterator->Next(&pBmd);
    if ( (pBmd == NULL) || (hr != S_OK) )
    {
        fprintf(stderr, "Error: Can not create DeckLink board object Result:0x%x\n", hr);
        goto end;
    }
    hr = pBmd->QueryInterface(IID_IDeckLinkInput, reinterpret_cast<void**>(&pBmdInput));
    if ( (pBmdInput == NULL) || (hr != S_OK) )
    {
        fprintf(stderr, "Error: Can not create DeckLink Input Result:0x%x\n", hr);
        goto end;
    }
    hr = pBmdInput->GetDisplayModeIterator(&pDisplayIterator);
    if ( (pDisplayIterator == NULL) || (hr != S_OK) )
    {
        fprintf(stderr, "Error: Can not create DeckLink Display Iterator Result:0x%x\n", hr);
        goto end;
    }
    // Search display mode.
    while (pDisplayIterator->Next(&pDisplayMode) == S_OK)
    {
        BMDDisplayMode EvalDisplayMode;
        if (pDisplayMode != NULL)
        {
            EvalDisplayMode = pDisplayMode->GetDisplayMode();
            if (displayMode == EvalDisplayMode)
            {
                // found and update parameters
                BMDTimeValue Numerator = 0;
                BMDTimeValue Denominator = 0;
                width = pDisplayMode->GetWidth();
                // if format is NTSC, need D1 to DV convert
                if ( (bmdModeNTSC == displayMode) && (486 == pDisplayMode->GetHeight()) )
                {
                    height = 480;
                    bmd_EnableD1DVCrop = true;
                }
                else
                {
                    height = pDisplayMode->GetHeight();
                    bmd_EnableD1DVCrop = false;
                }
                hr = pDisplayMode->GetFrameRate(&Denominator, &Numerator);
                frame_rate_numer = Numerator;
                frame_rate_denom = Denominator;
                // simplify the rational to avoid 25000/1000 etc.
                if (0 == frame_rate_numer % frame_rate_denom)
                {
                    frame_rate_numer /= frame_rate_denom;
                    frame_rate_denom = 1;
                }
                // print result
                fprintf(stderr, "Info: Found Display Mode:0x%x\n", EvalDisplayMode);
                fprintf(stderr, "Info: Found Display Mode Width:%d Height:%d Numerator:%d Denominator:%d\n"
                                , width, height, frame_rate_numer, frame_rate_denom);
                Result = true;
            }
            // Release
            pDisplayMode->Release();
            pDisplayMode = NULL;
        }
    }
    // Not found.
    if (!Result)
    {
        fprintf(stderr, "Error: Not found Display Mode.\n");
    }
    
end:
    if (pDisplayMode != NULL)
    {
        pDisplayMode->Release();
        pDisplayMode = NULL;
    }
    if (pDisplayIterator != NULL)
    {
        pDisplayIterator->Release();
        pDisplayIterator = NULL;
    }
    if (pBmdInput != NULL)
    {
        pBmdInput->Release();
        pBmdInput = NULL;
    }
    if (pBmd != NULL)
    {
        pBmd->Release();
        pBmd = NULL;
    }
    if (pIterator != NULL)
    {
        pIterator->Release();
        pIterator = NULL;
    }
    // return result.
    return !Result;
}

int BmdGetInstalledBoards()
{
    HRESULT hr = S_OK;
    // Create iterator
    IDeckLinkIterator* pIterator = CreateDeckLinkIteratorInstance();
    IDeckLink* pBmd = NULL;
    char* DeviceNameString = NULL;
    int NumDevices = 0;
    
    if (pIterator == NULL)
    {
        fprintf(stderr, "Error: Can not create IDeckLinkIterator object.\n");
        return -1;
    }
    while (pIterator->Next(&pBmd) == S_OK)
    {
        NumDevices++;
        // get model name
        hr = pBmd->GetModelName(const_cast<const char**>(&DeviceNameString));
        fprintf(stderr, "Info: Channel and Board Number:%d ModelName:%s.\n", NumDevices, DeviceNameString);
        free(DeviceNameString);
        // Release.
        pBmd->Release();
        pBmd = NULL;
    }
    if (pIterator != NULL)
    {
        pIterator->Release();
        pIterator = NULL;
    }
    // return nums
    return NumDevices;
}

void BmdPrintAvailableBufferCount()
{
    HRESULT hr = S_OK;
    uint32_t FrameCount = 0;
    uint32_t SampleCount = 0;
    for (int i = 0; (i < MAX_CHANNELS); i++)
    {
        if (bmd_pBmdInputArray[i] != NULL)
        {
            hr = bmd_pBmdInputArray[i]->GetAvailableVideoFrameCount(&FrameCount);
            hr = bmd_pBmdInputArray[i]->GetAvailableAudioSampleFrameCount(&SampleCount);
            if (hr == S_OK)
            {
                fprintf(stderr, "Info: Current available frame count Board:%d Frames:%d Samples:%d\n", i, FrameCount, SampleCount);
            }
        }
    }
}


void usage_exit(void)
{
    fprintf(stderr, "Usage: bmd_sdi [options]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "E.g.   bmd_sdi -c 2\n");
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
    fprintf(stderr, "                         PAL, NTSC, PAL_592, PAL_608, NTSC_502\n");
    fprintf(stderr, "                         1920x1080i25, 1920x1080p25sf, 1920x1080i29, 1920x1080p29sf,\n");
    fprintf(stderr, "                         1280x720p50, 1280x720p59\n");
    fprintf(stderr, "                         AUDIO8 enables 8 audio channels per SDI input\n");
    fprintf(stderr, "                         E.g. -mode 1920x1080i29:AUDIO8\n");
    fprintf(stderr, "    -16x9                Video aspect ratio is 16x9 (default)\n");
    fprintf(stderr, "    -4x3                 Video aspect ratio is 4x3\n");
    fprintf(stderr, "    -tt <tc type>        preferred type of timecode to use: VITC, LTC, DVITC, DLTC, SYS [default DVITC]\n");
    fprintf(stderr, "    -mc <master ch>      channel to use as timecode master: 0..7 [default -1 i.e. no master]\n");
    fprintf(stderr, "    -c <max channels>    maximum number of channels to use for capture\n");
    fprintf(stderr, "    -m <max memory MiB>  maximum memory to use for ring buffers in MiB [default use all available]\n");
    fprintf(stderr, "    -a8                  use 8 audio tracks per video channel (default 4)\n");
    fprintf(stderr, "    -a16                 use 16 audio tracks per video channel (default 4)\n");
    fprintf(stderr, "    -aes                 use AES/EBU audio with default routing (8:8 if multichannel, else 16:0)\n");
    fprintf(stderr, "    -avsync              perform avsync analysis - requires clapper-board input video\n");
    fprintf(stderr, "    -b <video_sample>    benchmark using video_sample instead of captured UYVY video frames\n");
    fprintf(stderr, "    -q                   quiet operation (fewer messages)\n");
    fprintf(stderr, "    -v                   increase verbosity\n");
    fprintf(stderr, "    -ld                  logfile directory\n");
    fprintf(stderr, "    -d <channel>         channel number to print verbose debug messages for\n");
    fprintf(stderr, "    -h2s_ffmpeg          use ffmpeg swscale to convert HD to SD [default use YUVlib]\n");
    fprintf(stderr, "    -h2s_filter          use filter when converting HD to SD using YUVlib\n");
    fprintf(stderr, "    -info                show info on installed DVS cards\n");
    fprintf(stderr, "    -h                   usage\n");
    fprintf(stderr, "\n");
    exit(1);
}

} // unnamed namespace

int main (int argc, char ** argv)
{
    int             n;
    int max_channels = MAX_CHANNELS;
    long long       opt_max_memory = 0;
    const char      *logfiledir = ".";
    //int             opt_video_mode = -1;
    //int             current_video_mode = -1;
    //int             opt_sync_type = -1;
    const char * mode_string = 0;
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
        else if (strcmp(argv[n], "-info") == 0)
        {
            show_card_info = true;
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
            if (sscanf(argv[n+1], "%lld", &opt_max_memory) != 1)
            {
                fprintf(stderr, "-m requires integer maximum memory in MB\n");
                return 1;
            }
            opt_max_memory = opt_max_memory * 1024 * 1024;
            n++;
        }
        else if (strcmp(argv[n], "-c") == 0)
        {
            if (argc <= n+1)
            {
                usage_exit();
            }

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
            if (++n < argc)
            {
                mode_string = argv[n];
            }
        }
        else if (strcmp(argv[n], "-16x9") == 0)
        {
            image_aspect = Ingex::RATIONAL_16_9;
        }
        else if (strcmp(argv[n], "-4x3") == 0)
        {
            image_aspect = Ingex::RATIONAL_4_3;
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
                secondary_video_format = FormatNone;
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
            naudioch = 8;
        }
        else if (strcmp(argv[n], "-a16") == 0)
        {
            naudioch = 16;
        }
        else if (strcmp(argv[n], "-aes") == 0)
        {
            aes_audio = 1;
        }
        else if (strcmp(argv[n], "-h2s_ffmpeg") == 0)
        {
            use_ffmpeg_hd_sd_scaling = 1;
        }
        else if (strcmp(argv[n], "-h2s_filter") == 0)
        {
            use_yuvlib_filter = 1;
        }
        else
        {
            fprintf(stderr, "unknown argument \"%s\"\n", argv[n]);
            return 1;
        }
    }

    // Process mode argument now image aspect is confirmed
    if (mode_string)
    {
        char vidmode[256] = "", audmode[256] = "";
        if (sscanf(mode_string, "%[^:]:%s", vidmode, audmode) != 2)
        {
            if (sscanf(mode_string, "%s", vidmode) != 1)
            {
                fprintf(stderr, "-mode requires option of the form videomode[:audiomode]\n");
                return 1;
            }
        }

        if (strcmp(vidmode, "PAL") == 0)
        {
            if (Ingex::RATIONAL_4_3 == image_aspect)
            {
                mode_video_raster = VideoRaster::PAL_4x3;
            }
            else
            {
                mode_video_raster = VideoRaster::PAL_16x9;
            }
        }
        else if (strcmp(vidmode, "NTSC") == 0)
        {
            if (Ingex::RATIONAL_4_3 == image_aspect)
            {
                mode_video_raster = VideoRaster::NTSC_4x3;
            }
            else
            {
                mode_video_raster = VideoRaster::NTSC_16x9;
            }
        }
        else if (strcmp(vidmode, "PAL_592") == 0)
        {
            if (Ingex::RATIONAL_4_3 == image_aspect)
            {
                mode_video_raster = VideoRaster::PAL_592_4x3;
            }
            else
            {
                mode_video_raster = VideoRaster::PAL_592_16x9;
            }
        }
        else if (strcmp(vidmode, "PAL_608") == 0)
        {
            if (Ingex::RATIONAL_4_3 == image_aspect)
            {
                mode_video_raster = VideoRaster::PAL_608_4x3;
            }
            else
            {
                mode_video_raster = VideoRaster::PAL_608_16x9;
            }
        }
        else if (strcmp(vidmode, "NTSC_502") == 0)
        {
            if (Ingex::RATIONAL_4_3 == image_aspect)
            {
                mode_video_raster = VideoRaster::NTSC_502_4x3;
            }
            else
            {
                mode_video_raster = VideoRaster::NTSC_502_16x9;
            }
        }
        else if (strcmp(vidmode, "1920x1080i25") == 0)
        {
            mode_video_raster = VideoRaster::SMPTE274_25I;
        }
        else if (strcmp(vidmode, "1920x1080p25sf") == 0)
        {
            mode_video_raster = VideoRaster::SMPTE274_25PSF;
        }
        else if (strcmp(vidmode, "1920x1080i29") == 0)
        {
            mode_video_raster = VideoRaster::SMPTE274_29I;
        }
        else if (strcmp(vidmode, "1920x1080p29sf") == 0)
        {
            mode_video_raster = VideoRaster::SMPTE274_29PSF;
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
        if (strcmp(audmode, "AUDIO8") == 0)
        {
            naudioch = 8;
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
    BMDDisplayMode  bmd_CaptureMode = 0;
    switch (mode_video_raster)
    {
    case Ingex::VideoRaster::PAL_4x3:
    case Ingex::VideoRaster::PAL_16x9:
        bmd_CaptureMode = bmdModePAL;
        break;
    case Ingex::VideoRaster::NTSC_4x3:
    case Ingex::VideoRaster::NTSC_16x9:
        bmd_CaptureMode = bmdModeNTSC;
        bmd_Setup75 = true;
        break;
    case Ingex::VideoRaster::SMPTE274_25I:
        bmd_CaptureMode = bmdModeHD1080i50;
        break;
    case Ingex::VideoRaster::SMPTE274_29I:
        bmd_CaptureMode = bmdModeHD1080i5994;
        break;
    // Not handling all rasters at present
    default:
        break;
    }

    //////////////////////////////////////////////////////
    // Attempt to open all BMD cards
    bmd_NumChannels = BmdGetInstalledBoards();
    if (bmd_NumChannels < 1)
    {
        fprintf(stderr, "Error: No installed boards. Exiting\n");
        return 1;
    }
    else if (bmd_NumChannels > max_channels)
    {
        bmd_NumChannels = max_channels;
    }

    if (BmdCheckAndUpdateVideoParameter(bmd_CaptureMode))
    {
        fprintf(stderr, "Error: Can not support this display mode. Exiting\n");
        return 1;
    }

    // Set primary raster (should really read it back from card)
    primary_video_raster = mode_video_raster;

    // We now know primary_video_raster, width, height, frame_rate and interlace.

    // Set secondary_video_raster
    if (NONE != secondary_capture_format)
    {
        // Secondary format is always SD even when primary is HD
        secondary_video_raster = sd_raster(primary_video_raster);
    }

    // Modify PAL rasters for line shift
    if (DV50 == primary_capture_format)
    {
        Ingex::VideoRaster::ModifyLineShift(primary_video_raster, true);
    }
    primary_line_shift = Ingex::VideoRaster::LineShift(primary_video_raster);
    if (DV50 == secondary_capture_format || DV25 == secondary_capture_format)
    {
        Ingex::VideoRaster::ModifyLineShift(secondary_video_raster, true);
    }
    secondary_line_shift = Ingex::VideoRaster::LineShift(secondary_video_raster);

    // Set pixel format needed for capture format and raster
    switch (primary_capture_format)
    {
    case UYVY:
        primary_pixel_format = Ingex::PixelFormat::UYVY_422;
        primary_video_format = Format422UYVY;
        break;
    case YUV422:
        primary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_422;
        primary_video_format = Format422PlanarYUV;
        break;
    case DV50:
        switch (primary_video_raster)
        {
        case Ingex::VideoRaster::PAL_4x3_B:
        case Ingex::VideoRaster::PAL_16x9_B:
            primary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_422;
            primary_video_format = Format422PlanarYUVShifted;
            break;
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            primary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_422;
            primary_video_format = Format422PlanarYUV;
            break;
        default:
            break;
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
        secondary_video_format = Format422PlanarYUV;
        break;
    case DV50:
        switch (secondary_video_raster)
        {
        case Ingex::VideoRaster::PAL_4x3_B:
        case Ingex::VideoRaster::PAL_16x9_B:
            secondary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_422;
            secondary_video_format = Format422PlanarYUVShifted;
            break;
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            secondary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_422;
            secondary_video_format = Format422PlanarYUV;
            break;
        default:
            break;
        }
        break;
    case MPEG:
        secondary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_420_MPEG;
        secondary_video_format = Format420PlanarYUV;
        break;
    case DV25:
        switch (secondary_video_raster)
        {
        case Ingex::VideoRaster::PAL_4x3_B:
        case Ingex::VideoRaster::PAL_16x9_B:
            secondary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_420_DV;
            secondary_video_format = Format420PlanarYUVShifted;
            break;
        case Ingex::VideoRaster::NTSC_4x3:
        case Ingex::VideoRaster::NTSC_16x9:
            secondary_pixel_format = Ingex::PixelFormat::YUV_PLANAR_411;
            secondary_video_format = Format411PlanarYUV;
            break;
        default:
            break;
        }
        break;
    case UYVY:
        // Not valid as secondary format
        break;
    case NONE:
        secondary_pixel_format = Ingex::PixelFormat::NONE;
        secondary_video_format = FormatNone;
        break;
    }


    bool dvs_dummy = false;

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

    // Dummy only supports 8 audio channels at present
    if (dvs_dummy && naudioch > 8)
    {
        logTF("Audio channels limited to 8 in dvs_dummy mode\n");
        naudioch = 8;
    }

    logTF("Using %s to determine number of frames to recover when video re-aquired\n",
            nexus_timecode_type_name(timecode_type));

    switch (naudioch)
    {
    case 8:
        logTF("Audio 8 channel mode enabled\n");
        break;
    case 16:
        logTF("Audio 16 channel mode enabled\n");
        break;
    }

    if (AUDIO_INTERLEAVED)
    {
        logTF("Audio interleaved mode enabled\n");
    }


    // Ring buffer offsets no longer tied to DMA structure
    // New structure is:
    //   primary video
    //   secondary video (optional)
    //   primary audio
    //   secondary audio
    //   frame data

    // Compute size...
    // Video size for 422
    video_size = width * height * 2;

    // primary video
    size_t primary_video_size = video_size;

    // secondary video (if any)
    size_t secondary_video_size = 0;
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

    // primary and secondary audio sizes
    // PAL_AUDIO_SAMPLES is maximum of all SDI audio formats
    primary_audio_size   = naudioch * PAL_AUDIO_SAMPLES * 4;
    secondary_audio_size = naudioch * PAL_AUDIO_SAMPLES * 2;

    // frame data
    size_t frame_data_size = sizeof(NexusFrameData);


    // Compute offsets and total size
    size_t offset = 0;
    secondary_video_offset = (offset += primary_video_size);
    primary_audio_offset = (offset += secondary_video_size);
    secondary_audio_offset = (offset += primary_audio_size);
    frame_data_offset = (offset += secondary_audio_size);
    element_size = (offset += frame_data_size);

    // Round up the element size
    element_size = ((element_size + 0xff) / 0x100) * 0x100;

    if (DEBUG_OFFSETS)
    {
        fprintf(stderr, "secondary_video_offset = %06x\n", secondary_video_offset);
        fprintf(stderr, "primary_audio_offset   = %06x\n", primary_audio_offset);
        fprintf(stderr, "secondary_audio_offset = %06x\n", secondary_audio_offset);
        fprintf(stderr, "frame_data_offset      = %06x\n", frame_data_offset);
        fprintf(stderr, "element_size           = %06x\n", element_size);
    }

    // Work out max DMA size so we can allocate a buffer

    // Set up audio sizes for DMA transferred audio and reformatted secondary audio
    unsigned int max_audio_dma_size = MAX_AUDIO_DMA_PAIR_SIZE * naudioch / 2;

    // For interleaved audio capture, audio size is fixed at 16 channels
    if (AUDIO_INTERLEAVED)
    {
        max_audio_dma_size = MAX_AUDIO_DMA_PAIR_SIZE * 8;
    }

    max_dma_size = MAX_VIDEO_DMA_SIZE + max_audio_dma_size;


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

    // Allocate working buffers for video conversions (uvyv to 422, HD to SD, etc.)
    // This is faster than doing it in the shared memory.
    int chan;
    for (chan = 0; chan < MAX_CHANNELS; chan++)
    {
        if (chan < bmd_NumChannels)
        {
            dma_buffer[chan] = (uint8_t *) memalign(DMA_ALIGN, max_dma_size);
            if (!dma_buffer[chan])
            {
                fprintf(stderr, "Failed to allocate dma buffer.\n");
                return 1;
            }

            hd2sd_interm[chan] = (uint8_t *)malloc(width*height*2);
            if (!hd2sd_interm[chan])
            {
                fprintf(stderr, "Failed to allocate HD-to-SD intermediate buffer.\n");
                return 1;
            }

            hd2sd_interm2[chan] = (uint8_t *)malloc((width/2) * sec_height * 2);
            if (!hd2sd_interm2[chan])
            {
                fprintf(stderr, "Failed to allocate HD-to-SD intermediate buffer 2.\n");
                return 1;
            }

            hd2sd_workspace[chan] = (uint8_t *)malloc(2*width*4);
            if (!hd2sd_workspace[chan])
            {
                fprintf(stderr, "Failed to allocate HD-to-SD work buffer.\n");
                return 1;
            }
        }
        else
        {
            dma_buffer[chan] = NULL;
            hd2sd_interm[chan] = NULL;
            hd2sd_interm2[chan] = NULL;
            hd2sd_workspace[chan] = NULL;
        }
    }


    // Allocate shared memory buffers
    if (! allocate_shared_buffers(bmd_NumChannels, opt_max_memory))
    {
        return 1;
    }

    // setup bmd boards. after buffer arrangement.
    if (BmdSetupBoards(bmd_NumChannels, bmd_CaptureMode) != 0)
    {
        fprintf(stderr, "Error in BmdSetupBoards()\n");
        return 1;
    }

    // Running Bmd boards.
    BmdRunBoards();

    // Update the heartbeat 10 times a second
    p_control->owner_pid = getpid();
    while (1) {
        gettimeofday(&p_control->owner_heartbeat, NULL);
        usleep(100 * 1000);
    }
    pthread_join(sdi_thread[0], NULL);

    // Cleanup
    BmdPrintAvailableBufferCount();
    BmdCleanupObjects();

    for (chan = 0; chan < MAX_CHANNELS; chan++)
    {
        if (dma_buffer[chan])
        {
            free(dma_buffer[chan]);
        }
        if (hd2sd_interm[chan])
        {
            free(hd2sd_interm[chan]);
        }
        if (hd2sd_interm2[chan])
        {
            free(hd2sd_interm2[chan]);
        }
        if (hd2sd_workspace[chan])
        {
            free(hd2sd_workspace[chan]);
        }
    }

    return 0; // silence gcc warning
}
