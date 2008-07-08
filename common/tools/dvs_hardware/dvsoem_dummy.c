/*
 * $Id: dvsoem_dummy.c,v 1.1 2008/07/08 14:59:20 philipn Exp $
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "../../video_burn_in_timecode.h"

#include "dvs_clib.h"
#include "dvs_fifo.h"

// The sv_handle structure looks like this
// typedef struct {
//   int	magic;
//   int	size;
//   int	version;
//   int	vgui;
//   int	prontovideo;
//   int	debug;
//   int	pad[10];
// } sv_handle;
//

// The DvsCard structure stores hardware parameters
// used to simulate an hardware SDI I/O card
typedef struct {
	int card;
	int channel;
	int frame_count;
	int dropped;
	struct timeval start_time;
	sv_fifo_buffer fifo_buffer;
	unsigned char *source_dmabuf;
#ifdef DVSDUMMY_LOGGING
	FILE *log_fp;
#endif
} DvsCard;

static int source_frame_size = 0;
static int source_width = 0;
static int source_height = 0;

static inline void write_bcd(unsigned char* target, int val)
{
    *target = ((val / 10) << 4) + (val % 10);
}
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

// Represent the colour and position of a colour bar
typedef struct {
	double          position;
	unsigned char   colour[4];
} bar_colour_t;

// Generate a video buffer containing uncompressed UYVY video representing
// the familiar colour bars test signal (or YUY2 video if specified).
static void create_colour_bars(unsigned char *video_buffer, int width, int height)
{
	int             i,j,b;
	bar_colour_t    UYVY_table[] = {
                {52/720.0,  {0x80,0xEB,0x80,0xEB}}, // white
                {140/720.0, {0x10,0xD2,0x92,0xD2}}, // yellow
                {228/720.0, {0xA5,0xA9,0x10,0xA9}}, // cyan
                {316/720.0, {0x35,0x90,0x22,0x90}}, // green
                {404/720.0, {0xCA,0x6A,0xDD,0x6A}}, // magenta
                {492/720.0, {0x5A,0x51,0xF0,0x51}}, // red
                {580/720.0, {0xf0,0x29,0x6d,0x29}}, // blue
                {668/720.0, {0x80,0x10,0x80,0x10}}, // black
                {720/720.0, {0x80,0xEB,0x80,0xEB}}  // white
            };

	for (j = 0; j < height; j++)
	{
        for (i = 0; i < width; i+=2)
        {
            for (b = 0; b < 9; b++)
            {
                if ((i / ((double)width)) < UYVY_table[b].position)
                {
                    // UYVY packing
                    video_buffer[j*width*2 + i*2 + 0] = UYVY_table[b].colour[0];
                    video_buffer[j*width*2 + i*2 + 1] = UYVY_table[b].colour[1];
                    video_buffer[j*width*2 + i*2 + 2] = UYVY_table[b].colour[2];
                    video_buffer[j*width*2 + i*2 + 3] = UYVY_table[b].colour[3];
                    break;
                }
            }
        }
	}
}

static int64_t tv_diff_microsecs(const struct timeval* a, const struct timeval* b)
{
	int64_t diff = (b->tv_sec - a->tv_sec) * 1000000 + b->tv_usec - a->tv_usec;
	return diff;
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
	memcpy(pbuffer->dma.addr, dvs->source_dmabuf, source_frame_size);

	// Update timecodes
	dvs->frame_count++;
	int dvs_tc = int_to_dvs_tc(dvs->frame_count);
	pbuffer->timecode.vitc_tc = dvs_tc;
	pbuffer->timecode.vitc_tc2 = dvs_tc;
	pbuffer->timecode.ltc_tc = dvs_tc;

	// Burn timecode in video
	burn_mask_uyvy(dvs->frame_count, 40, 40, 720, 576, (unsigned char *)pbuffer->dma.addr);

	// Update hardware values
	pbuffer->control.tick = dvs->frame_count * 2;
	struct timeval now_time;
	gettimeofday(&now_time, NULL);
	pbuffer->control.clock_high = now_time.tv_sec;
	pbuffer->control.clock_low = now_time.tv_usec;

	// Simulate typical behaviour of capture fifo by sleeping until next
	// expected frame boundary. The simulated hardware clock is the system
	// clock, with frames captured at every 40000 microsecond boundary since
	// the initialisation of dvs->start_time.
	int64_t diff = tv_diff_microsecs(&dvs->start_time, &now_time);
	int64_t expected_diff = 40000 * dvs->frame_count;
	if (expected_diff > diff)
		usleep(expected_diff - diff);

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
int sv_fifo_reset(sv_handle * sv, sv_fifo * pfifo)
{
	return SV_OK;
}

int sv_query(sv_handle * sv, int cmd, int par, int *val)
{
	*val = SV_OK;
	return SV_OK;
}
int sv_option_get(sv_handle * sv, int code, int *val)
{
	*val = SV_OK;
	return SV_OK;
}
int sv_currenttime(sv_handle * sv, int brecord, int *ptick, uint32 *pclockhigh, uint32 *pclocklow)
{
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
	return SV_OK;
}

int sv_openex(sv_handle ** psv, char * setup, int openprogram, int opentype, int timeout, int spare)
{
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
	DvsCard *dvs = malloc(sizeof(DvsCard));
	dvs->card = card;
	dvs->channel = channel;
	dvs->frame_count = 0;
	dvs->start_time.tv_sec = 0;
	dvs->start_time.tv_usec = 0;
	dvs->dropped = 0;
	*psv = (sv_handle*)dvs;

	// Initialise with PAL sample frame for recording
	int width = 720;
	int height = 576;
	int video_size = width * height * 2;

	// DVS dma transfer buffer is:
	// <video (UYVY)><audio ch 1+2 with fill to 0x4000><audio ch 3+4 with fill>
	source_frame_size = video_size + 0x4000 + 0x4000;
	dvs->source_dmabuf = malloc(source_frame_size);
	unsigned char *video = dvs->source_dmabuf;
	unsigned char *audio12 = video + video_size;
	unsigned char *audio34 = audio12 + 0x4000;

	// setup colorbars
	create_colour_bars(video, width, height);

	// TODO: create audio tone or click
	int i;
	for (i = 0; i < 0x4000; i += 8) {
        audio12[i+0] = 0x00;    audio34[i+0] = 0x80;
        audio12[i+1] = 0x01;    audio34[i+1] = 0x81;
        audio12[i+2] = 0x02;    audio34[i+2] = 0x82;
        audio12[i+3] = 0x03;    audio34[i+3] = 0x83;
        audio12[i+4] = 0x04;    audio34[i+4] = 0x84;
        audio12[i+5] = 0x05;    audio34[i+5] = 0x85;
        audio12[i+6] = 0x06;    audio34[i+6] = 0x86;
        audio12[i+7] = 0x07;    audio34[i+7] = 0x87;
	}

	// Setup fifo_buffer which contains offsets to audio buffers
	dvs->fifo_buffer.audio[0].addr[0] = (char*)video_size;
	dvs->fifo_buffer.audio[0].addr[1] = (char*)(video_size + 0x4000);

	source_width = width;
	source_height = height;

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
	DvsCard *dvs = (DvsCard*)sv;
	free(dvs->source_dmabuf);
	return SV_OK;
}


char * sv_geterrortext(int code)
{
	static char *str = "Error string not implemented";
	switch (code) {
		case SV_ERROR_DEVICENOTFOUND:
			return "SV_ERROR_DEVICENOTFOUND. The device could not be found.";
	}
	return str;
}


int sv_status(sv_handle * sv, sv_info * info)
{
	info->xsize = source_width;
	info->ysize = source_height;
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

export int sv_timecode_feedback(sv_handle * sv, sv_timecode_info* input, sv_timecode_info* output)
{
	return SV_OK;
}