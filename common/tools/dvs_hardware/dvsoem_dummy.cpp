/*
 * $Id: dvsoem_dummy.cpp,v 1.1 2010/07/06 14:15:13 john_f Exp $
 *
 * Implement a debug-only DVS hardware library for testing.
 *
 * Copyright (C) 2007  Stuart Cunningham <stuart_hc@users.sourceforge.net>
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

#define __STDC_CONSTANT_MACROS  // for INT64_C

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "video_burn_in_timecode.h"
#include "video_test_signals.h"
#include "audio_utils.h"
#include "time_utils.h"

#include "Timecode.h"

#include "dummy_include/dvs_clib.h"
#include "dummy_include/dvs_fifo.h"

// The sv_handle structure looks like this
// typedef struct {
//   int    magic;
//   int    size;
//   int    version;
//   int    vgui;
//   int    prontovideo;
//   int    debug;
//   int    pad[10];
// } sv_handle;
//

// Represents whether LTC, VITC or both are being generated
typedef enum {
    DvsTcAll,
    DvsTcLTC,
    DvsTcVITC
} DvsTcQuality;

// The DvsCard structure stores hardware parameters
// used to simulate an hardware SDI I/O card
typedef struct {
    int card;
    int channel;
    int width;
    int height;
    int frame_rate_numer;
    int frame_rate_denom;
    int videomode;
    int audio_channels;
    int frame_size;
    int frame_count;
    Ingex::Timecode timecode;
    int dropped;
    int source_input_frames;
    DvsTcQuality tc_quality;
    struct timeval start_time;
    sv_fifo_buffer fifo_buffer;
    unsigned char *source_dmabuf;
#ifdef DVSDUMMY_LOGGING
    FILE *log_fp;
#endif
} DvsCard;

static DvsCard * dvs_channel[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

static inline void write_bcd(unsigned char* target, int val)
{
    *target = ((val / 10) << 4) + (val % 10);
}

/*
static int int_to_dvs_tc(int tc)
{
    // e.g. turn 1020219 into hex 0x11200819
    int fr = tc % 25;
    int hr = (int)(tc / (60*60*25));
    int mi = (int)((tc - (hr * 60*60*25)) / (60 * 25));
    int se = (int)((tc - (hr * 60*60*25) - (mi * 60*25)) / 25);

    unsigned char raw_tc[4];
    write_bcd(raw_tc + 0, fr);
    write_bcd(raw_tc + 1, se);
    write_bcd(raw_tc + 2, mi);
    write_bcd(raw_tc + 3, hr);
    int result = *(int *)(raw_tc);
    return result;
}
*/

static int timecode_to_dvs_tc(const Ingex::Timecode & tc)
{
    // e.g. turn 1020219 into hex 0x11200819
    unsigned char raw_tc[4];
    write_bcd(raw_tc + 0, tc.Frames());
    write_bcd(raw_tc + 1, tc.Seconds());
    write_bcd(raw_tc + 2, tc.Minutes());
    write_bcd(raw_tc + 3, tc.Hours());
    if (tc.DropFrame())
    {
        raw_tc[0] |= 0x00000040;
    }
    int result = *(int *)(raw_tc);
    return result;
}

static int is_rec601(int videomode)
{
    return videomode == SV_MODE_PAL || videomode == SV_MODE_NTSC;
}

// Represent the colour and position of a colour bar for dummy video
typedef struct {
    double          position;
    unsigned char   colour[4];
} bar_colour_t;

// Dummy audio: 500Hz tone fits neatly into 1920 samples (1 PAL video frame).
// Use a simple table of signed 16bit samples to avoid linking maths library,
// and to aid debugging when looking through hex data.
static int16_t tone_500Hz_16bit_1cycle[96] = {
0x0000, 0x006B, 0x00D6, 0x0140, 0x01A7, 0x0210, 0x0271, 0x02D8,
0x032F, 0x0393, 0x03E1, 0x043B, 0x0486, 0x04CF, 0x0515, 0x0551,
0x058D, 0x05BA, 0x05EE, 0x060B, 0x0632, 0x0645, 0x065A, 0x0661,
0x0668, 0x0661, 0x065B, 0x0645, 0x062F, 0x060F, 0x05EA, 0x05BE,
0x058A, 0x0552, 0x0514, 0x04D0, 0x0487, 0x0437, 0x03E7, 0x038C,
0x0335, 0x02D4, 0x0273, 0x020F, 0x01A8, 0x013F, 0x00D7, 0x006A,
0x0000, 0xFF96, 0xFF29, 0xFEC2, 0xFE56, 0xFDF2, 0xFD8E, 0xFD29,
0xFCD0, 0xFC6F, 0xFC1C, 0xFBC7, 0xFB7A, 0xFB2F, 0xFAEE, 0xFAAC,
0xFA76, 0xFA42, 0xFA16, 0xF9F2, 0xF9D0, 0xF9B9, 0xF9A9, 0xF99B,
0xF99C, 0xF99B, 0xF9A9, 0xF9B9, 0xF9D0, 0xF9F3, 0xFA13, 0xFA47,
0xFA70, 0xFAB2, 0xFAE9, 0xFB32, 0xFB79, 0xFBC7, 0xFC1C, 0xFC6F,
0xFCD0, 0xFD29, 0xFD8F, 0xFDEF, 0xFE5A, 0xFEBF, 0xFF2B, 0xFF94 };

static void setup_source_buf(DvsCard * dvs)
{
    //fprintf(stderr, "DUMMY: setup_source_buf()\n");

    // A ring buffer can be used for sample video and audio loaded from files
    FILE *fp_video = NULL, *fp_audio = NULL;
    int64_t vidfile_size = 0;
    dvs->source_input_frames = 1;

    // Check DVSDUMMY_PARAM environment variable and change defaults if necessary.
    // The value is a ':' separated list of parameters such as:
    //
    // video=1920x1080i50 or 1920x1080i60 or 1280x720p50 or 1280x720p60 or NTSC
    // good_tc=A/L/V
    // vidfile=filename.uyvy            // UYVY 4:2:2
    // audfile=filename.pcm             // 16bit stereo pair at 48kHz
    const char *p_env = getenv("DVSDUMMY_PARAM");
    if (p_env) {
        // parse ':' separated list
        const char *p;
        do {
            char param[FILENAME_MAX];

            p = strchr(p_env, ':');
            if (p == NULL) {
                strcpy(param, p_env);
            }
            else {
                int len = p - p_env;
                strncpy(param, p_env, len);
                param[len] = '\0';
            }

            printf("DVSDUMMY: processing param[%s]\n", param);

            if (strncmp(param, "help", strlen("help")) == 0) {
                printf("DVSDUMMY: valid parameters are:\n");
                printf("video=PAL (default)\n");
                printf("video=NTSC\n");
                printf("video=1920x1080i50\n");
                printf("video=1920x1080i60\n");
                printf("video=1280x720p50\n");
                printf("video=1280x720p60\n");
                printf("vidfile=filename.uyvy (uncompressed UYVY)\n");
                printf("audfile=audio12.pcm (16bit 48kHz stereo audio)\n");
                printf("good_tc=A/L/V (A=All (default), L=LTC, V=VITC)\n");
            }
            else if (strncmp(param, "video=", strlen("video=")) == 0) {
                int tmpw, tmph, rate;
                char ff;
                if (strcmp(param, "video=PAL") == 0) {
                    dvs->width = 720;
                    dvs->height = 576;
                    dvs->frame_rate_numer = 25;
                    dvs->frame_rate_denom = 1;
                    dvs->videomode = SV_MODE_PAL;
                }
                else if (strcmp(param, "video=NTSC") == 0) {
                    dvs->width = 720;
                    dvs->height = 480;
                    dvs->frame_rate_numer = 30000;
                    dvs->frame_rate_denom = 1001;
                    dvs->videomode = SV_MODE_NTSC;
                }
                else if (sscanf(param, "video=%dx%d%c%d", &tmpw, &tmph, &ff, &rate) == 4) {
                    dvs->width = tmpw;
                    dvs->height = tmph;
                    if (ff == 'p' && rate == 50) {
                        dvs->frame_rate_numer = 50;
                        dvs->frame_rate_denom = 1;
                        dvs->videomode = SV_MODE_SMPTE296_50P;
                    }
                    else if (ff == 'p' && rate == 60) {
                        dvs->frame_rate_numer = 60000;
                        dvs->frame_rate_denom = 1001;
                        dvs->videomode = SV_MODE_SMPTE296_60P;
                    }
                    else if (ff == 'i' && rate == 50) {
                        dvs->frame_rate_numer = 25;
                        dvs->frame_rate_denom = 1;
                        dvs->videomode = SV_MODE_SMPTE274_25I;
                    }
                    else if (ff == 'i' && rate == 60) {
                        dvs->frame_rate_numer = 30000;
                        dvs->frame_rate_denom = 1001;
                        dvs->videomode = SV_MODE_SMPTE274_30I;
                    }
                }
                else {
                    fprintf(stderr, "DVSDUMMY_PARAM: unsupported %s\n", param);
                }
            }
            else if (strncmp(param, "good_tc=", strlen("good_tc=")) == 0) {
                const char *p_qual = param + strlen("good_tc=");
                if (*p_qual == 'L')
                    dvs->tc_quality = DvsTcLTC;
                if (*p_qual == 'V')
                    dvs->tc_quality = DvsTcVITC;
            }
            else if (strncmp(param, "vidfile=", strlen("vidfile=")) == 0) {
                const char *vidfile = param + strlen("vidfile=");
                if ((fp_video = fopen(vidfile, "rb")) == NULL) {
                    fprintf(stderr, "Could not open %s\n", vidfile);
                    return;
                }
                struct stat buf;
                if (stat(vidfile, &buf) != 0)
                    return;
                vidfile_size = buf.st_size;
            }
            else if (strncmp(param, "audfile=", strlen("audfile=")) == 0) {
                const char *audfile = param + strlen("audfile=");
                if ((fp_audio = fopen(audfile, "rb")) == NULL) {
                    fprintf(stderr, "Could not open %s\n", audfile);
                    return;
                }
            }

            if (!p)
                break;
            p_env = p + 1;
        } while (*p_env != '\0');
    }

    int video_size = dvs->width * dvs->height * 2;

    // DVS dma transfer buffer is:
    // <video (UYVY)><audio ch 1+2 with fill to 0x4000><audio ch 3+4 with fill>
    // (if SV_MODE_AUDIO_8CHANNEL)<audio ch 5+6><audio ch 7+8>
    dvs->frame_size = video_size + 0x4000 + 0x4000;
    if (dvs->audio_channels == 8) {
        dvs->frame_size += 0x4000 + 0x4000;    // 4 extra audio channels
    }

    if (fp_video && fp_audio && vidfile_size) {
        int max_memory = 103680000;         // 5 seconds of 720x576
        if (vidfile_size < max_memory)
            dvs->source_input_frames = vidfile_size / video_size;
        else
            dvs->source_input_frames = max_memory / video_size;
    }

    // (Re)allocate buffer for the source video/audio.
    free(dvs->source_dmabuf);
    dvs->source_dmabuf = (unsigned char *)malloc(dvs->frame_size * dvs->source_input_frames);

    if (!fp_video) {
        unsigned char *video = dvs->source_dmabuf;
        unsigned char *audio12 = video + video_size;
        unsigned char *audio34 = audio12 + 0x4000;
    
        // setup colorbars
        uyvy_color_bars(dvs->width, dvs->height, is_rec601(dvs->videomode), video);
    
        // Create audio
        int32_t * audio1 = (int32_t *)audio12;
        int32_t * audio2 = audio1 + 1;
        int32_t * audio3 = (int32_t *)audio34;
        int32_t * audio4 = audio3 + 1;

        // Create 500Hz tone on audio 1,2 and 1000Hz tone on audio 3,4
        int i;
        for (i = 0; i < 1920; i++)
        {
            // shift 16bit sample to 32bit
            int sample500 = tone_500Hz_16bit_1cycle[i % 96] << 16;

            // double frequency for audio 3 and 4
            int sample1000 = tone_500Hz_16bit_1cycle[(i*2) % 96] << 16;

            audio1[i * 2] = sample500;
            audio2[i * 2] = sample500;
            audio3[i * 2] = sample1000;
            audio4[i * 2] = sample1000;
        }

        if (dvs->audio_channels == 8) {
            // The 4 extra channels are arranged after the first 4 (size 0x8000)
            int32_t * audio5 = (int32_t *)(audio12 + 0x8000);
            int32_t * audio6 = audio5 + 1;
            int32_t * audio7 = (int32_t *)(audio34 + 0x8000);
            int32_t * audio8 = audio7 + 1;
    
            // Create 500Hz tone on audio 5,6 and 1000Hz tone on audio 7,8
            int i;
            for (i = 0; i < 1920; i++)
            {
                // shift 16bit sample to 32bit
                int sample500 = tone_500Hz_16bit_1cycle[i % 96] << 16;
    
                // double frequency for audio 7 and 8
                int sample1000 = tone_500Hz_16bit_1cycle[(i*2) % 96] << 16;
    
                audio5[i * 2] = sample500;
                audio6[i * 2] = sample500;
                audio7[i * 2] = sample1000;
                audio8[i * 2] = sample1000;
            }
        }
    }
    else {
        int i;
        int audio_size = 1920*2*2;
        unsigned char *video = dvs->source_dmabuf;
        for (i = 0; i < dvs->source_input_frames; i++) {
            if (fread(video, video_size, 1, fp_video) != 1) {
                fprintf(stderr, "Could not read frame %d from video file\n", i);
                return;
            }
            if (fp_audio) {
                uint8_t tmp[audio_size];
                if (fread(tmp, audio_size, 1, fp_audio) != 1) {
                    fprintf(stderr, "Could not read frame %d from audio file\n", i);
                    return;
                }
                pair16bit_to_dvsaudio32(1920, tmp, video + video_size);
            }
            video += dvs->frame_size;
        }
    }

    // Setup fifo_buffer which contains offsets to audio buffers
    dvs->fifo_buffer.audio[0].addr[0] = (char*)0 + video_size;
    dvs->fifo_buffer.audio[0].addr[1] = (char*)0 + video_size + 0x4000;
}

// SV implementations
int sv_fifo_init(sv_handle * sv, sv_fifo ** ppfifo, int bInput, int bShared, int bDMA, int flagbase, int nframes)
{
    return SV_OK;
}
int sv_fifo_free(sv_handle * sv, sv_fifo * pfifo)
{
    return SV_OK;
}
int sv_fifo_start(sv_handle * sv, sv_fifo * pfifo)
{
    return SV_OK;
}
int sv_fifo_getbuffer(sv_handle * sv, sv_fifo * pfifo, sv_fifo_buffer ** pbuffer, sv_fifo_bufferinfo * bufferinfo, int flags)
{
    DvsCard *dvs = (DvsCard*)sv;
    *pbuffer = &dvs->fifo_buffer;

    return SV_OK;
}
int sv_fifo_putbuffer(sv_handle * sv, sv_fifo * pfifo, sv_fifo_buffer * pbuffer, sv_fifo_bufferinfo * bufferinfo)
{
    DvsCard *dvs = (DvsCard*)sv;

    // Initialise start_time for first call
    if (dvs->start_time.tv_sec == 0)
        gettimeofday(&dvs->start_time, NULL);

    // For record, copy video + audio to dma.addr
    int source_offset = dvs->frame_count % dvs->source_input_frames;
    memcpy(pbuffer->dma.addr, dvs->source_dmabuf + source_offset * dvs->frame_size, dvs->frame_size);

    // Update frame count and timecodes
    dvs->frame_count++;
    dvs->timecode += 1;
    int dvs_tc = timecode_to_dvs_tc(dvs->timecode);
    pbuffer->timecode.vitc_tc = (dvs->tc_quality == DvsTcLTC) ? 0 : dvs_tc;
    pbuffer->timecode.vitc_tc2 = (dvs->tc_quality == DvsTcLTC) ? 0 : dvs_tc;
    pbuffer->timecode.ltc_tc = (dvs->tc_quality == DvsTcVITC) ? 0 : dvs_tc;
    pbuffer->anctimecode.dvitc_tc[0] = (dvs->tc_quality == DvsTcLTC) ? 0 : dvs_tc;
    pbuffer->anctimecode.dvitc_tc[1] = (dvs->tc_quality == DvsTcLTC) ? 0 : dvs_tc;
    pbuffer->anctimecode.dltc_tc = (dvs->tc_quality == DvsTcVITC) ? 0 : dvs_tc;

    // For NTSC audio, vary the number of samples in a 5 frame sequence of:
    // 1602, 1602, 1602, 1602, 1600 (found by experiment using DVS cards)
    //
    // Other formats:
    // 1920 fixed                   - PAL and SMPTE274/25I (1920x1080i50)
    //  960 fixed                   - SMPTE296/50P (1280x720p50)
    // 1600 fixed                   - SMPTE274/30I (60Hz interlaced)
    // 1602, 1602, 1602, 1602, 1600 - SMPTE274/29I (59.94Hz interlaced)
    //  800 fixed                   - SMPTE296/60P (1280x720p60)
    //  802, 800, 802, 800, 800     - SMPTE296/59P (1280x720p59.94)
    int num_samples = 1920;
    switch (dvs->videomode & SV_MODE_MASK) {
        case SV_MODE_NTSC:
            num_samples = (dvs->frame_count % 5 == 0) ? 1600 : 1602;
            break;
        case SV_MODE_SMPTE274_25I:
            num_samples = 1920;
            break;
        case SV_MODE_SMPTE274_29I:
            num_samples = (dvs->frame_count % 5 == 0) ? 1600 : 1602;
            break;
        case SV_MODE_SMPTE274_30I:
            num_samples = 1600;
            break;
        case SV_MODE_SMPTE296_50P:
            num_samples = 960;
            break;
        case SV_MODE_SMPTE296_59P:
            num_samples = (dvs->frame_count % 5 == 0 ||
                            dvs->frame_count % 5 == 2) ? 802 : 800;
            break;
        case SV_MODE_SMPTE296_60P:
            num_samples = 800;
            break;
    }

    // audio size is samples *2 (channels) *4 (32bit)
    dvs->fifo_buffer.audio[0].size = num_samples * 2 * 4;

    // Burn timecode in video at x,y offset 40,40
    burn_mask_uyvy(dvs->timecode, 40, 40, dvs->width, dvs->height, (unsigned char *)pbuffer->dma.addr);

    // Update hardware values
    pbuffer->control.tick = dvs->frame_count * 2;
    int64_t currenttime = gettimeofday64();
    pbuffer->control.clock_high = currenttime / INT64_C(0x100000000);
    pbuffer->control.clock_low = currenttime % INT64_C(0x100000000);

    // Simulate typical behaviour of capture fifo by sleeping until next
    // expected frame boundary. The simulated hardware clock is the system
    // clock, with frames captured at every 40000 (for PAL) microsecond
    // boundary since the initialisation of dvs->start_time.
    struct timeval now_time;
    gettimeofday(&now_time, NULL);
    int64_t diff = tv_diff_microsecs(&dvs->start_time, &now_time);
    int64_t expected_diff = (int64_t)dvs->frame_count * dvs->frame_rate_denom * 1000000 / dvs->frame_rate_numer;
    if (expected_diff > diff) {
        //printf("%dx%d usleep(%lld)\n", dvs->width, dvs->height, expected_diff - diff);
        usleep(expected_diff - diff);
    }

#ifdef DVSDUMMY_LOGGING
    if (dvs->log_fp)
        fprintf(dvs->log_fp, "sv_fifo_putbuffer() frame_count=%d diff=%lld expected_diff=%lld slept=%lld\n",
            dvs->frame_count, diff, expected_diff, (expected_diff > diff) ? expected_diff - diff : 0);
#endif

    return SV_OK;
}

int sv_fifo_stop(sv_handle * sv, sv_fifo * pfifo, int flags)
{
    return SV_OK;
}

int sv_fifo_status(sv_handle * sv, sv_fifo * pfifo, sv_fifo_info * pinfo)
{
    DvsCard *dvs = (DvsCard*)sv;

    // Clear structure
    memset(pinfo, 0, sizeof(sv_fifo_info));

    // Record dropped frames
    pinfo->dropped = dvs->dropped;

    return SV_OK;
}
int sv_fifo_wait(sv_handle * sv, sv_fifo * pfifo)
{
    return SV_OK;
}
int sv_fifo_reset(sv_handle * sv, sv_fifo * pfifo)
{
    return SV_OK;
}

int sv_query(sv_handle * sv, int cmd, int par, int * val)
{
    DvsCard * dvs = (DvsCard *)sv;

    switch (cmd)
    {
    case SV_QUERY_MODE_CURRENT:
        *val = dvs->videomode;
        break;
    case SV_QUERY_DMAALIGNMENT:
        *val = 16;
        break;
    case SV_QUERY_SERIALNUMBER:
        *val = 0;
        break;
    default:
        *val = 0;
        break;
    }

    return SV_OK;
}
int sv_option_set(sv_handle * sv, int code, int val)
{
    return SV_OK;
}
int sv_option_get(sv_handle * sv, int code, int *val)
{
    *val = SV_OK;
    return SV_OK;
}
int sv_currenttime(sv_handle * sv, int brecord, int *ptick, uint32 *pclockhigh, uint32 *pclocklow)
{
    DvsCard * dvs = (DvsCard *)sv;
    *ptick = dvs->frame_count * 2;

    int64_t currenttime = gettimeofday64();
    *pclockhigh = currenttime / INT64_C(0x100000000);
    *pclocklow = currenttime % INT64_C(0x100000000);

    return SV_OK;
}
int sv_version_status( sv_handle * sv, sv_version * version, int versionsize, int deviceid, int moduleid, int spare)
{
    sprintf(version->module, "driver");
    version->release.v.major = 3;
    version->release.v.minor = 2;
    version->release.v.patch = 1;
    version->release.v.fix = 0;
    return SV_OK;
}
int sv_videomode(sv_handle * sv, int videomode)
{
    //fprintf(stderr, "DUMMY: sv_videomode(0x%08x)\n", videomode);
    DvsCard * dvs = (DvsCard *)sv;
    dvs->videomode = videomode;

    switch (videomode & SV_MODE_MASK)
    {
    case SV_MODE_PAL:
        dvs->width = 720;
        dvs->height = 576;
        dvs->frame_rate_numer = 25;
        dvs->frame_rate_denom = 1;
        break;
    case SV_MODE_NTSC:
        dvs->width = 720;
        dvs->height = 480;
        dvs->frame_rate_numer = 30000;
        dvs->frame_rate_denom = 1001;
        break;
    case SV_MODE_SMPTE274_25I:
        dvs->width = 1920;
        dvs->height = 1080;
        dvs->frame_rate_numer = 25;
        dvs->frame_rate_denom = 1;
        break;
    case SV_MODE_SMPTE274_30I:
        dvs->width = 1920;
        dvs->height = 1080;
        dvs->frame_rate_numer = 30000;
        dvs->frame_rate_denom = 1001;
        break;
    case SV_MODE_SMPTE296_50P:
        dvs->width = 1280;
        dvs->height = 720;
        dvs->frame_rate_numer = 50;
        dvs->frame_rate_denom = 1;
        break;
    case SV_MODE_SMPTE296_60P:
        dvs->width = 1280;
        dvs->height = 720;
        dvs->frame_rate_numer = 60000;
        dvs->frame_rate_denom = 1001;
        break;
    }

    // Only 4 and 8 channel audio implemented (more audio modes are possible)
    switch (videomode & SV_MODE_AUDIO_MASK)
    {
    case SV_MODE_AUDIO_8CHANNEL:
        dvs->audio_channels = 8;
        break;
    default:
        dvs->audio_channels = 4;
        break;
    }

    setup_source_buf(dvs);

    return SV_OK;
}
int sv_sync(sv_handle * sv, int sync)
{
    return SV_OK;
}

int sv_openex(sv_handle ** psv, char * setup, int openprogram, int opentype, int timeout, int spare)
{
    //fprintf(stderr, "DUMMY: sv_openex(%s)\n", setup);

    int card = 0, channel = 0;

    // Parse card string
    if (sscanf(setup, "PCI,card=%d,channel=%d", &card, &channel) == 2) {
        if (card > 3)
            return SV_ERROR_DEVICENOTFOUND;
        if (channel > 1)
            return SV_ERROR_DEVICENOTFOUND;
    }
    else if (sscanf(setup, "PCI,card=%d", &card) == 1) {
        if (card > 3)
            return SV_ERROR_DEVICENOTFOUND;
    }
    else {
        return SV_ERROR_SVOPENSTRING;
    }

    // Allocate private DVS card structure and return it as if it were a sv_handle*
    int channel_index = card * 2 + channel;
    DvsCard * dvs = dvs_channel[channel_index];
    if (dvs == 0)
    {
        // Allocate struct
        dvs = dvs_channel[channel_index] = (DvsCard *)malloc(sizeof(DvsCard));

        // Initialise struct
        dvs->card = card;
        dvs->channel = channel;
        dvs->frame_count = 0;
        dvs->start_time.tv_sec = 0;
        dvs->start_time.tv_usec = 0;
        dvs->dropped = 0;
        dvs->tc_quality = DvsTcAll;
        // Default is PAL width & height
        dvs->width = 720;
        dvs->height = 576;
        dvs->frame_rate_numer = 25;
        dvs->frame_rate_denom = 1;
        dvs->videomode = SV_MODE_PAL;

        // Set initial timecode from time-of-day
        struct timeval tod_tv;
        gettimeofday(&tod_tv, NULL);
        time_t tod_time = tod_tv.tv_sec;
        struct tm tod_tm;
        localtime_r(&tod_time, &tod_tm);
        dvs->timecode = Ingex::Timecode(tod_tm.tm_hour, tod_tm.tm_min, tod_tm.tm_sec, 0, dvs->frame_rate_numer, dvs->frame_rate_denom, false);

        // Setup source audio/video
        setup_source_buf(dvs);
    }

    *psv = (sv_handle*)dvs;

#ifdef DVSDUMMY_LOGGING
    // logging for debugging
    char log_path[FILENAME_MAX];
    sprintf(log_path, "/tmp/dvsdummy_card%d_chan%d", card, channel);
    dvs->log_fp = fopen(log_path, "w");
    if (dvs->log_fp == NULL)
        fprintf(stderr, "Warning, fopen(%s) failed\n", log_path);
#endif

    return SV_OK;
}

int sv_close(sv_handle * sv)
{
    return SV_OK;
}


char * sv_geterrortext(int code)
{
    const char * str;
    switch (code)
    {
    case SV_ERROR_DEVICENOTFOUND:
        str = "SV_ERROR_DEVICENOTFOUND. The device could not be found.";
        break;
    default:
        str = "Error string not implemented";
        break;
    }
    return (char *)str;
}


int sv_status(sv_handle * sv, sv_info * info)
{
    DvsCard *dvs = (DvsCard*)sv;
    info->xsize = dvs->width;
    info->ysize = dvs->height;
    return SV_OK;
}

int sv_storage_status(sv_handle *sv, int cookie, sv_storageinfo * psiin, sv_storageinfo * psiout, int psioutsize, int flags)
{
    return SV_OK;
}

int sv_stop(sv_handle * sv)
{
    return SV_OK;
}

void sv_errorprint(sv_handle * sv, int code)
{
    return;
}

int sv_timecode_feedback(sv_handle * sv, sv_timecode_info* input, sv_timecode_info* output)
{
    return SV_OK;
}
