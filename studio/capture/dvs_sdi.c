/*
 * $Id: dvs_sdi.c,v 1.3 2007/10/26 14:43:06 john_f Exp $
 *
 * Record multiple SDI inputs to shared memory buffers.
 *
 * Copyright (C) 2005  Stuart Cunningham <stuart_hc@users.sourceforge.net>
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

#define _XOPEN_SOURCE 600			// for posix_memalign

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

#include "dvs_clib.h"
#include "dvs_fifo.h"

#include "nexus_control.h"
#include "logF.h"
#include "video_conversion.h"
#include "video_test_signals.h"
#include "avsync_analysis.h"

// Globals
#define MAX_CHANNELS 8				// each DVS card can have 2 channels, so 4 cards gives 8 channels
pthread_t		sdi_thread[MAX_CHANNELS];
sv_handle		*a_sv[MAX_CHANNELS];
int				num_sdi_threads = 0;
NexusControl	*p_control = NULL;
uint8_t			*ring[MAX_CHANNELS] = {0};
int				control_id, ring_id[MAX_CHANNELS];
int				width = 0, height = 0;
int				element_size = 0, dma_video_size = 0, audio_offset = 0, audio_size = 0;
CaptureFormat	video_format = FormatYUV422;
CaptureFormat	video_secondary_format = FormatNone;
static int verbose = 0;
static int verbose_card = -1;	// which card to show when verbose is on
static int test_avsync = 0;
pthread_mutex_t	m_log = PTHREAD_MUTEX_INITIALIZER;		// logging mutex to prevent intermixing logs

// Master Timecode support, where one timecode is distributed to other cards
typedef enum {
	MasterNone,			// no timecode distribution
	MasterVITC,			// use VITC as master
	MasterLTC			// use LTC as master
} MasterTimecodeType;

// Recover timecode type is the type of timecode to use to generate dummy frames
typedef enum {
	RecoverVITC,
	RecoverLTC
} RecoverTimecodeType;

pthread_mutex_t	m_master_tc = PTHREAD_MUTEX_INITIALIZER;
static MasterTimecodeType master_type = MasterNone;	// do not use master timecode
static int master_card = 0;				// card with master timecode source (default card 0)
// TODO: use mutex to guard access
static int master_tc = 0;				// timecode as integer
static int64_t master_tod = 0;			// gettimeofday value corresponding to master_tc

// When generating dummy frames, use LTC differences to calculate how many frames
static RecoverTimecodeType recover_timecode_type = RecoverLTC;

static uint8_t *no_video_frame = NULL;	// captioned black frame saying "NO VIDEO"

#define SV_CHECK(x) {int res = x; if (res != SV_OK) { fprintf(stderr, "sv call failed=%d  %s line %d\n", res, __FILE__, __LINE__); sv_errorprint(sv,res); cleanup_exit(1, sv); } }

// Define missing macro for older SDKs (e.g. 2.7)
#ifndef SV_OPTION_MULTICHANNEL
#define SV_OPTION_MULTICHANNEL              184
#endif

// Returns time-of-day as microseconds
static int64_t gettimeofday64(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	int64_t tod = (int64_t)tv.tv_sec * 1000000 + tv.tv_usec ;
	return tod;
}

static void cleanup_shared_mem(void)
{
	int				i, id;
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

static void cleanup_exit(int res, sv_handle *sv)
{
	printf("cleaning up\n");
	// attempt to avoid lockup when stopping playback
	if (sv != NULL)
	{
		// sv_fifo_wait does nothing for record, only useful for playback
		//sv_fifo_wait(sv, poutput);
		sv_close(sv);
	}

	// delete shared memory
	cleanup_shared_mem();

	usleep(100 * 1000);		// 0.1 seconds

	printf("done - exiting\n");
	exit(res);
}

static int check_sdk_version3(void)
{
	sv_handle *sv;
	sv_version version;
	int device = 0;
    //int module = 0;
	int result = 0;

	SV_CHECK( sv_openex(&sv, "PCI,card=0", SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_INPUT, 0, 0) );
	//for (module = 0; module < 2; module++) {
		SV_CHECK( sv_version_status(sv, &version, sizeof(version), device, 1, 0) );
		printf("%s version = (%d,%d,%d,%d)\n", version.module, version.release.v.major, version.release.v.minor, version.release.v.patch, version.release.v.fix);
		if (/*strstr( version.module, "clib-oem" ) &&*/ version.release.v.major >= 3)
			result = 1;
	//}

	SV_CHECK( sv_close(sv) );

	return result;
}

static void catch_sigusr1(int sig_number)
{
	// toggle a flag
}

static void catch_sigint(int sig_number)
{
	printf("\nReceived signal %d - ", sig_number);
	cleanup_exit(0, NULL);
}

static void log_avsync_analysis(int card, int lastframe, const uint8_t *addr, unsigned long audio12_offset, unsigned long audio34_offset)
{
	int line_size = width*2;
	int click1 = 0, click1_off = -1;
    int click2 = 0, click2_off = -1;
    int click3 = 0, click3_off = -1;
    int click4 = 0, click4_off = -1;

	int flash = find_red_flash_uyvy(addr,  line_size);
	find_audio_click_32bit_stereo(addr + audio12_offset,
												&click1, &click1_off,
												&click2, &click2_off);
	find_audio_click_32bit_stereo(addr + audio34_offset,
												&click3, &click3_off,
												&click4, &click4_off);

	if (flash || click1 || click2 || click3 || click4) {
		if (flash)
			logFF("card %d: %5d  red-flash      \n", card, lastframe + 1);
		if (click1)
			logFF("card %d: %5d  a1off=%d %.1fms\n", card, lastframe + 1, click1_off, click1_off / 1920.0 * 40);
		if (click2)
			logFF("card %d: %5d  a2off=%d %.1fms\n", card, lastframe + 1, click2_off, click2_off / 1920.0 * 40);
		if (click3)
			logFF("card %d: %5d  a3off=%d %.1fms\n", card, lastframe + 1, click3_off, click3_off / 1920.0 * 40);
		if (click4)
			logFF("card %d: %5d  a4off=%d %.1fms\n", card, lastframe + 1, click4_off, click4_off / 1920.0 * 40);
	}
}

// Read available physical memory stats and
// allocate shared memory ring buffers accordingly
static int allocate_shared_buffers(int num_cards)
{
	long long	k_shmmax = 0;
	int			ring_len, i;
	FILE		*procfp;

	// Linux specific way to read shmmax value
	if ((procfp = fopen("/proc/sys/kernel/shmmax", "r")) == NULL)
	{
		perror("fopen(/proc/sys/kernel/shmmax)");
		return 0;
	}
	fscanf(procfp, "%lld", &k_shmmax);
	fclose(procfp);

	// Reduce maximum to 1GiB to avoid misleading shmmat errors since
	// Documentation/sysctl/kernel.txt says "memory segments up to 1Gb are now supported"
	if (k_shmmax > 0x40000000) {
		printf("shmmax=%lld (%.3fMiB) probably too big, reducing to 1024MiB\n", k_shmmax, k_shmmax / (1024*1024.0));
		k_shmmax = 0x40000000;	// 1GiB
	}

	// calculate reasonable ring buffer length
	// reduce by small number 5 to leave a little room for other shared mem
	ring_len = k_shmmax / num_cards / element_size - 5;
	printf("shmmax=%lld (%.3fMiB) calculated per card ring_len=%d\n", k_shmmax, k_shmmax / (1024*1024.0), ring_len);

	printf("element_size=%d ring_len=%d (%.2f secs) (total=%lld)\n", element_size, ring_len, ring_len / 25.0, (long long)element_size * ring_len);
	if (ring_len < 10)
	{
		printf("ring_len=%d too small - try increasing shmmax:\n", ring_len);
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
				return 0;
			}
		}
		else
		{
			perror("shmget control key 9");
			return 0;
		}
	}
	p_control = (NexusControl*)shmat(control_id, NULL, 0);

	p_control->cards = num_cards;
	p_control->ringlen = ring_len;
	p_control->elementsize = element_size;

	p_control->width = width;
	p_control->height = height;
	p_control->frame_rate_numer = 25;
	p_control->frame_rate_denom = 1;
	p_control->pri_video_format = video_format;
	p_control->sec_video_format = video_secondary_format;

	p_control->audio12_offset = audio_offset;
	p_control->audio34_offset = audio_offset + 0x4000;
	p_control->audio_size = audio_size;

	p_control->tc_offset = audio_offset + audio_size - sizeof(int);
	p_control->ltc_offset = p_control->tc_offset - sizeof(int);
	p_control->signal_ok_offset = p_control->ltc_offset - sizeof(int);
	p_control->sec_video_offset = dma_video_size + audio_size;

	// Allocate multiple element ring buffers containing video + audio + tc
	//
	// by default 32MB is available (40 PAL frames), or change it with:
	//  root# echo  104857600 >> /proc/sys/kernel/shmmax  # 120 frms (100MB)
	//  root# echo  201326592 >> /proc/sys/kernel/shmmax  # 240 frms (192MB)
	//  root# echo 1073741824 >> /proc/sys/kernel/shmmax  # 1294 frms (1GB)

	// key for variable number of ring buffers can be 10, 11, 12, 13, 14, 15, 16, 17
	for (i = 0; i < num_cards; i++)
	{
		int key = i + 10;
		ring_id[i] = shmget(key, element_size * ring_len, IPC_CREAT | IPC_EXCL | 0666);
		if (ring_id[i] == -1)	/* shm error */
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
			fprintf(stderr, "shmat failed for card[%d], ring_id[%d] = %d\n", i, i, ring_id[i]);
			perror("shmat");
			return 0;
		}
		p_control->card[i].lastframe = -1;
		p_control->card[i].hwdrop = 0;

		// initialise mutex in shared memory
		if (pthread_mutex_init(&p_control->card[i].m_lastframe, NULL) != 0)
			fprintf(stderr, "Mutex init error\n");

		// Set ring frames to black - too slow!
		// memset(ring[i], 0x80, element_size * ring_len);
	}

	return 1;
}

//
// write_picture()
//
// Gets a frame from the SDI FIFO and stores it to memory ring buffer
// (also can write to disk as a debugging option)
static int write_picture(int chan, sv_handle *sv, sv_fifo *poutput, int recover_from_video_loss)
{
	sv_fifo_buffer		*pbuffer;
	sv_fifo_bufferinfo	bufferinfo;
	int					get_res;
	int					ring_len = p_control->ringlen;
	NexusBufCtl			*pc = &(p_control->card[chan]);

	// Get sv memmory buffer
	// Ignore problems with missing audio, and carry on with good video but zeroed audio
	get_res = sv_fifo_getbuffer(sv, poutput, &pbuffer, NULL, 0);
	if (get_res != SV_OK && get_res != SV_ERROR_INPUT_AUDIO_NOAIV
			&& get_res != SV_ERROR_INPUT_AUDIO_NOAESEBU)
	{
		// reset master timecode to "unset"
		if (chan == master_card) {
			PTHREAD_MUTEX_LOCK( &m_master_tc )
			master_tod = 0;
			master_tc = 0;
			PTHREAD_MUTEX_UNLOCK( &m_master_tc )
		}

		return get_res;
	}

	// dma buffer is structured as follows
	//
	// 0x0000 video frame					offset= 0
	// 0x0000 audio ch. 0, 1 interleaved	offset= pbuffer->audio[0].addr[0]
	// 0x0000 audio ch. 2, 3 interleaved	offset= pbuffer->audio[0].addr[1]
	//

    // check audio offset
    /*fprintf(stderr, "chan = %d, audio_offset = %d, video[0].addr = %p, audio[0].addr[0-4] = %p %p %p %p\n",
        chan,
        audio_offset,
        pbuffer->video[0].addr,
        pbuffer->audio[0].addr[0],
        pbuffer->audio[0].addr[1],
        pbuffer->audio[0].addr[2],
        pbuffer->audio[0].addr[3]);*/

	uint8_t *vid_dest = ring[chan] + element_size * ((pc->lastframe+1) % ring_len);
	uint8_t *dma_dest = vid_dest;
	if (video_format == FormatYUV422 || video_format == FormatYUV422DV) {
		// Store frame at pc->lastframe+2 to allow space for UYVY->YUV422 conversion
		dma_dest = ring[chan] + element_size * ((pc->lastframe+2) % ring_len);
	}
	pbuffer->dma.addr = (char *)dma_dest;
	pbuffer->dma.size = element_size;			// video + audio + tc + yuv

	// read frame from DVS chan
	// reception of a SIGUSR1 can sometimes cause this to fail
	// If it fails we should restart fifo.
	if (sv_fifo_putbuffer(sv, poutput, pbuffer, &bufferinfo) != SV_OK)
	{
		fprintf(stderr, "sv_fifo_putbuffer failed, restarting fifo\n");
		SV_CHECK( sv_fifo_reset(sv, poutput) );
		SV_CHECK( sv_fifo_start(sv, poutput) );
	}

	if (test_avsync) {
		log_avsync_analysis(chan, pc->lastframe,
			dma_dest, (unsigned long)pbuffer->audio[0].addr[0], (unsigned long)pbuffer->audio[0].addr[1]);
	}

	// Get current time-of-day time and this chan's current hw clock.
	int current_tick;
	unsigned int h_clock, l_clock;
	SV_CHECK( sv_currenttime(sv, SV_CURRENTTIME_CURRENT, &current_tick, &h_clock, &l_clock) );
	int64_t tod = gettimeofday64();

	// Use the hw-clock time the putbuffer'd frame was recorded,
	// to calculate the time-of-day the frame was recorded.
	// (pbuffer->control.clock_high/low appears identical to bufferinfo.clock_high/low)
	int64_t cur_clock = (int64_t)h_clock * 0x100000000LL + l_clock;
	int64_t rec_clock = (int64_t)bufferinfo.clock_high * 0x100000000LL + bufferinfo.clock_low;
	int64_t clock_diff = cur_clock - rec_clock;
	int64_t tod_rec = tod - clock_diff;

    // set flag so we can zero audio if not present
    int no_audio = (SV_ERROR_INPUT_AUDIO_NOAIV == get_res
                || SV_ERROR_INPUT_AUDIO_NOAESEBU == get_res
                || 0 == pbuffer->audio[0].addr[0]);

	if (video_format == FormatYUV422 || video_format == FormatYUV422DV)
    {
		// Repack to planar YUV 4:2:2
		uyvy_to_yuv422(		width, height,
							video_format == FormatYUV422DV,	// do DV50 line shift?
							dma_dest,						// input
							vid_dest);						// output

		// copy audio to match
        if (!no_audio)
        {
            memcpy(	vid_dest + audio_offset,	// dest
                    dma_dest + audio_offset,	// src
                    audio_size);
        }
	}

    if (no_audio)
    {
        memset( vid_dest + audio_offset, 0, audio_size);
    }

	if (video_secondary_format != FormatNone) {
		// Downconvert to 4:2:0 YUV buffer located just after audio
		uyvy_to_yuv420(width, height,
					video_secondary_format == FormatYUV420DV,	// do DV25 line shift?
					dma_dest,									// input
					vid_dest + (audio_offset + audio_size));	// output
	}

	// Handle buggy field order (can happen with misconfigured camera)
	// Incorrect field order causes vitc_tc and vitc2 to be swapped.
	// If the high bit is set on vitc use vitc2 instead.
	int vitc_to_use = pbuffer->timecode.vitc_tc;
	if ((unsigned)pbuffer->timecode.vitc_tc >= 0x80000000) {
		vitc_to_use = pbuffer->timecode.vitc_tc2;
		if (verbose) {
			PTHREAD_MUTEX_LOCK( &m_log )
			logFF("chan %d: vitc_tc >= 0x80000000 (0x%08x), using vitc_tc2 (0x%08x)\n", chan, pbuffer->timecode.vitc_tc, vitc_to_use);
			PTHREAD_MUTEX_UNLOCK( &m_log )
		}
	}
	int tc_as_int = dvs_tc_to_int(vitc_to_use);
	int orig_tc_as_int = tc_as_int;

	// A similar check must be done for LTC since the field
	// flag is occasionally set when the fifo call returns
	int ltc_to_use = pbuffer->timecode.ltc_tc;
	if ((unsigned)pbuffer->timecode.ltc_tc >= 0x80000000) {
		ltc_to_use = (unsigned)pbuffer->timecode.ltc_tc & 0x7fffffff;
		if (verbose) {
			PTHREAD_MUTEX_LOCK( &m_log )
			logFF("chan %d: ltc_tc >= 0x80000000 (0x%08x), masking high bit (0x%08x)\n", chan, pbuffer->timecode.ltc_tc, ltc_to_use);
			PTHREAD_MUTEX_UNLOCK( &m_log )
		}
	}
	int ltc_as_int = dvs_tc_to_int(ltc_to_use);
	int orig_ltc_as_int = ltc_as_int;

	// If enabled, handle master timecode distribution
	int derived_tc = 0;
	int64_t diff_to_master = 0;
	if (master_type != MasterNone) {
		if (chan == master_card) {
			// Store timecode and time-of-day the frame was recorded in shared variable
			PTHREAD_MUTEX_LOCK( &m_master_tc )
			master_tc = (master_type == MasterLTC) ? ltc_as_int : tc_as_int;
			master_tod = tod_rec;
			PTHREAD_MUTEX_UNLOCK( &m_master_tc )
		}
		else {
			// if master_tod not set, derived_tc is left as 0 (meaning dischaned)

			PTHREAD_MUTEX_LOCK( &m_master_tc )
			if (master_tod) {
				// compute offset between this chan's recorded frame's time-of-day and master's
				diff_to_master = tod_rec - master_tod;
				int int_diff = diff_to_master;
				//int div = (int_diff + 20000) / 40000;
				int div;
				if (int_diff < 0)	// -ve range is from -20000 to -1
					div = (int_diff - 19999) / 40000;
				else				// +ve range is from 0 to 19999
					div = (int_diff + 20000) / 40000;
				derived_tc = master_tc + div;
			}
			PTHREAD_MUTEX_UNLOCK( &m_master_tc )

			// Save calculated timecode
			if (master_type == MasterLTC)
				ltc_as_int = derived_tc;
			else
				tc_as_int = derived_tc;
		}
	}

	// Lookup last frame's timecodes to calculate timecode discontinuity
	int last_tc = *(int *)(ring[chan] + element_size * ((pc->lastframe) % ring_len) + p_control->tc_offset);
	int last_ltc = *(int *)(ring[chan] + element_size * ((pc->lastframe) % ring_len) + p_control->tc_offset-4);

	// Check number of frames to recover if any
	// A maximum of 50 frames will be recovered.
	int recover_diff = (recover_timecode_type == RecoverVITC) ? tc_as_int - last_tc : ltc_as_int - last_ltc;
	if (recover_from_video_loss && recover_diff > 1 && recover_diff < 52) {
		logTF("chan %d: Need to recover %d frames\n", chan, recover_diff);

		tc_as_int += recover_diff - 1;
		ltc_as_int += recover_diff - 1;

		// Increment pc->lastframe by amount to avoid discontinuity
		pc->lastframe += recover_diff - 1;

		// Ideally we would copy the dma transferred frame into its correct position
		// but this needs testing.
		//uint8_t *tmp_frame = (uint8_t*)malloc(width*height*2);
		//memcpy(tmp_frame, vid_dest, width*height*2);
		//free(tmp_frame);
		logTF("chan %d: Recovered.  lastframe=%d ltc=%d\n", chan, pc->lastframe, ltc_as_int);
	}

	// Copy timecode into last 4 bytes of element_size
	// This is a bit sneaky but saves maintaining a separate ring buffer
	// Also copy LTC just before VITC timecode as int.
	memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + p_control->tc_offset, &tc_as_int, 4);
	memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + p_control->tc_offset-4, &ltc_as_int, 4);

	// Copy "frame" tick (tick / 2) into unused area at end of element_size
	int frame_tick = pbuffer->control.tick / 2;
	memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + p_control->tc_offset-8, &frame_tick, 4);

	// Get info structure for statistics
	sv_fifo_info info;
	SV_CHECK(sv_fifo_status(sv, poutput, &info));

	// Timecode error occurs when difference is not exactly 1
	// (ignore the first frame captured when lastframe is -1)
	int tc_diff = tc_as_int - last_tc;
	int ltc_diff = ltc_as_int - last_ltc;
	int tc_err = (tc_diff != 1 && ltc_diff != 1) && pc->lastframe != -1;

	// log timecode discontinuity if any, or give verbose log of specified chan
	if (tc_err || (verbose && chan == verbose_card)) {
		PTHREAD_MUTEX_LOCK( &m_log )
		logTF("chan %d: lastframe=%6d tick/2=%7d hwdrop=%5d vitc_tc=%8x vitc_tc2=%8x ltc=%8x tc_int=%d ltc_int=%d last_tc=%d tc_diff=%d %s ltc_diff=%d %s\n",
		chan, pc->lastframe, pbuffer->control.tick / 2,
		info.dropped,
		pbuffer->timecode.vitc_tc,
		pbuffer->timecode.vitc_tc2,
		pbuffer->timecode.ltc_tc,
		tc_as_int,
		ltc_as_int,
		last_tc,
		tc_diff,
		tc_diff != 1 ? "!" : "",
		ltc_diff,
		ltc_diff != 1 ? "!" : "");
		PTHREAD_MUTEX_UNLOCK( &m_log )
	}

	if (verbose) {
		PTHREAD_MUTEX_LOCK( &m_log )		// guard logging with mutex to avoid intermixing

		if (info.dropped != pc->hwdrop)
			logTF("chan %d: lf=%7d vitc=%8d ltc=%8d dropped=%d\n", chan, pc->lastframe+1, tc_as_int, ltc_as_int, info.dropped);

		logFF("chan[%d]: tick=%d hc=%u,%u diff_to_mast=%12lld  lf=%7d vitc=%8d ltc=%8d (orig v=%8d l=%8d] drop=%d\n", chan,
			pbuffer->control.tick,
			pbuffer->control.clock_high, pbuffer->control.clock_low,
			diff_to_master,
			pc->lastframe+1, tc_as_int, ltc_as_int,
			orig_tc_as_int, orig_ltc_as_int,
			info.dropped);

		PTHREAD_MUTEX_UNLOCK( &m_log )		// end logging guard
	}

	// signal frame is now ready
	PTHREAD_MUTEX_LOCK( &pc->m_lastframe )
	pc->hwdrop = info.dropped;
	pc->lastframe++;
	PTHREAD_MUTEX_UNLOCK( &pc->m_lastframe )

	return SV_OK;
}

static int write_dummy_frames(sv_handle *sv, int chan, int current_frame_tick, int tick_last_dummy_frame)
{
	// No video so copy "no video" frame instead
	//
	// Use this chan's internal tick counter to determine when and
	// how many frames to fabricate
	if (verbose > 1)
		logTF("chan: %d frame_tick=%d tick_last_dummy_frame=%d\n", chan, current_frame_tick, tick_last_dummy_frame);

	if (tick_last_dummy_frame == -1 || current_frame_tick - tick_last_dummy_frame > 0) {
		// Work out how many frames to make
		int num_dummy_frames;
		if (tick_last_dummy_frame == -1)
			num_dummy_frames = 1;
		else
			num_dummy_frames = current_frame_tick - tick_last_dummy_frame;

		int i;
		for (i = 0; i < num_dummy_frames; i++) {
			// Read ring buffer info
			int					ring_len = p_control->ringlen;
			NexusBufCtl			*pc = &(p_control->card[chan]);

			// dummy video frame
			uint8_t *vid_dest = ring[chan] + element_size * ((pc->lastframe+1) % ring_len);
			memcpy(vid_dest, no_video_frame, width*height*2);

			// write silent 4 channels of 32bit audio
			memset(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + audio_offset, 0, 1920*4*4);

			// Increment timecode by 1
			int last_tc = *(int *)(ring[chan] + element_size * ((pc->lastframe) % ring_len) + p_control->tc_offset);
			int last_ltc = *(int *)(ring[chan] + element_size * ((pc->lastframe) % ring_len) + p_control->tc_offset-4);
			int last_ftk = *(int *)(ring[chan] + element_size * ((pc->lastframe) % ring_len) + p_control->tc_offset-8);
			last_tc++;
			last_ltc++;
			last_ftk++;
			memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + p_control->tc_offset, &last_tc, 4);
			memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + p_control->tc_offset-4, &last_ltc, 4);
			memcpy(ring[chan] + element_size * ((pc->lastframe+1) % ring_len) + p_control->tc_offset-8, &last_ftk, 4);

			if (verbose)
				logTF("chan: %d i:%2d  cur_frame_tick=%d tick_last_dummy_frame=%d last_ftk=%d tc=%d ltc=%d\n", chan, i, current_frame_tick, tick_last_dummy_frame, last_ftk, last_tc, last_ltc);

			// signal frame is now ready
			PTHREAD_MUTEX_LOCK( &pc->m_lastframe )
			pc->lastframe++;
			PTHREAD_MUTEX_UNLOCK( &pc->m_lastframe )

			tick_last_dummy_frame = current_frame_tick - num_dummy_frames + 1;
		}
	}

	return tick_last_dummy_frame;
}

static void wait_for_good_signal(sv_handle *sv, int chan, int required_good_frames)
{
	int good_frames = 0;
	int tick_last_dummy_frame = -1;

	// Poll until we get a good video signal for at least required_good_frames
	while (1) {
		int sdiA, videoin, audioin, timecode;
		unsigned int h_clock, l_clock;
		int current_tick;
		SV_CHECK( sv_currenttime(sv, SV_CURRENTTIME_CURRENT, &current_tick, &h_clock, &l_clock) );
		SV_CHECK( sv_query(sv, SV_QUERY_INPUTRASTER_SDIA, 0, &sdiA) );
		SV_CHECK( sv_query(sv, SV_QUERY_VIDEOINERROR, 0, &videoin) );
		SV_CHECK( sv_query(sv, SV_QUERY_AUDIOINERROR, 0, &audioin) );
		SV_CHECK( sv_query(sv, SV_QUERY_VALIDTIMECODE, 0, &timecode) );

		if (verbose > 1) {
			printf("\r%8d raster=0x%08x video=%s audio=%s timecode=%s%s%s%s%s%s (0x%x)",
				current_tick,
				sdiA,
				videoin == SV_OK ? "OK" : "error",
				audioin == SV_OK ? "OK" : "error",
				(timecode & SV_VALIDTIMECODE_LTC) ? "LTC " : "",
				(timecode & SV_VALIDTIMECODE_DLTC) ? "DLTC " : "",
				(timecode & SV_VALIDTIMECODE_VITC_F1) ? "VITC/F1 " : "",
				(timecode & SV_VALIDTIMECODE_VITC_F2) ? "VITC/F2 " : "",
				(timecode & SV_VALIDTIMECODE_DVITC_F1) ? "DVITC/F1 " : "",
				(timecode & SV_VALIDTIMECODE_DVITC_F2) ? "DVITC/F2 " : "",
				timecode
				);
			fflush(stdout);
		}

		if (videoin == SV_OK) {
			good_frames++;
			if (good_frames == required_good_frames) {
				if (verbose)
					printf("\n");
				break;				// break from while(1) loop
			}
		}

		// tick in terms of frames, ignoring odd ticks representing fields
		int current_frame_tick = current_tick / 2;

		// Write dummy frames to chan's ring buffer and return tick of last frame written
		tick_last_dummy_frame = write_dummy_frames(sv, chan, current_frame_tick, tick_last_dummy_frame);

		usleep(18*1000);		// sleep slightly less than one tick (20,000)
	}

	return;
}

// channel number passed as void * (using cast)
static void * sdi_monitor(void *arg)
{
	long				chan = (long)arg;
	sv_handle			*sv = a_sv[chan];
	sv_fifo				*poutput;
	sv_info				status_info;
	sv_storageinfo		storage_info;


	SV_CHECK( sv_status( sv, &status_info) );

	SV_CHECK( sv_storage_status(sv,
							0,
							NULL,
							&storage_info,
							sizeof(storage_info),
							0) );

	// To aid debugging, delay cards by a small amount of time so that
	// they appear in log files in card 0, 1, 2, 3 order
	if (chan > 0) {
		usleep(10000 + 5000 * chan);				
	}

	SV_CHECK( sv_fifo_init(	sv,
							&poutput,		// FIFO handle
							TRUE,			// bInput (FALSE for playback)
							FALSE,			// bShared (TRUE for input/output share memory)
							TRUE,			// bDMA
							FALSE,			// reserved
							0) );			// nFrames (0 means use maximum)

	SV_CHECK( sv_fifo_start(sv, poutput) );

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

			// Note the card's tick counter appears to pause while
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


static void usage_exit(void)
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
	fprintf(stderr, "                         MPEG   - secondary buffer is planar YUV 4:2:0\n");
	fprintf(stderr, "                         DV25   - secondary buffer is planar YUV 4:2:0 with field order line shift\n");
	fprintf(stderr, "    -mt <master tc type> type of master card timecode to use: VITC, LTC, OFF\n");
	fprintf(stderr, "    -mc <master card>    card to use as timecode master: 0, 1, 2, 3\n");
	fprintf(stderr, "    -rt <recover type>   timecode type to calculate missing frames to recover: VITC, LTC\n");
	fprintf(stderr, "    -c <max channels>    maximum number of channels to use for capture\n");
	fprintf(stderr, "    -avsync              perform avsync analysis - requires clapper-board input video\n");
	fprintf(stderr, "    -q                   quiet operation (fewer messages)\n");
	fprintf(stderr, "    -v                   increase verbosity\n");
	fprintf(stderr, "    -ld                  logfile directory\n");
	fprintf(stderr, "    -d <card>            card number to print verbose debug messages for\n");
	fprintf(stderr, "    -h                   usage\n");
	fprintf(stderr, "\n");
	exit(1);
}

int main (int argc, char ** argv)
{
	int					n, max_channels = MAX_CHANNELS;
	char *logfiledir = ".";

	time_t now;
	struct tm *tm_now;

	now = time ( NULL );
	tm_now = localtime ( &now );

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
		else if (strcmp(argv[n], "-ld") == 0)
		{
			if (argc <= n+1)
				usage_exit();

			logfiledir = argv[n+1];
			n++;
		}
		else if (strcmp(argv[n], "-mt") == 0)
		{
			if (argc <= n+1)
				usage_exit();

			if (strcmp(argv[n+1], "VITC") == 0) {
				master_type = MasterVITC;
			}
			else if (strcmp(argv[n+1], "LTC") == 0) {
				master_type = MasterLTC;
			}
			else if (strcmp(argv[n+1], "OFF") == 0 || strcmp(argv[n+1], "None") == 0) {
				master_type = MasterNone;
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

			if (sscanf(argv[n+1], "%d", &master_card) != 1 ||
				master_card > 3 || master_card < 0)
			{
				fprintf(stderr, "-mc requires card number from 0 .. 3\n");
				return 1;
			}
			n++;
		}
		else if (strcmp(argv[n], "-rt") == 0)
		{
			if (argc <= n+1)
				usage_exit();

			if (strcmp(argv[n+1], "VITC") == 0) {
				recover_timecode_type = RecoverVITC;
			}
			else if (strcmp(argv[n+1], "LTC") == 0) {
				recover_timecode_type = RecoverLTC;
			}
			else {
				usage_exit();
			}
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
		else if (strcmp(argv[n], "-d") == 0)
		{
			if (argc <= n+1)
				usage_exit();

			if (sscanf(argv[n+1], "%d", &verbose_card) != 1 ||
				verbose_card > 3 || verbose_card < 0)
			{
				fprintf(stderr, "-d requires card number {0,1,2,3}\n");
				return 1;
			}
			n++;
		}
		else if (strcmp(argv[n], "-f") == 0)
		{
			if (argc <= n+1)
				usage_exit();

			if (strcmp(argv[n+1], "UYVY") == 0) {
				video_format = FormatUYVY;
			}
			else if (strcmp(argv[n+1], "YUV422") == 0) {
				video_format = FormatYUV422;
			}
			else if (strcmp(argv[n+1], "DV50") == 0) {
				video_format = FormatYUV422DV;
			}
			else {
				usage_exit();
			}
			n++;
		}
		else if (strcmp(argv[n], "-s") == 0)
		{
			if (argc <= n+1)
				usage_exit();

			if (strcmp(argv[n+1], "None") == 0) {
				video_secondary_format = FormatNone;
			}
			else if (strcmp(argv[n+1], "MPEG") == 0) {
				video_secondary_format = FormatYUV420;
			}
			else if (strcmp(argv[n+1], "DV25") == 0) {
				video_secondary_format = FormatYUV420DV;
			}
			else {
				usage_exit();
			}
			n++;
		}
	}

	switch (video_format) {
		case FormatUYVY:
				logTF("Capturing video as UYVY (4:2:2)\n"); break;
		case FormatYUV422:
				logTF("Capturing video as planar YUV 4:2:2\n"); break;
		case FormatYUV422DV:
				logTF("Capturing video as YUV 4:2:2 planar with picture shifted down by one line\n"); break;
		default:
				logTF("Unsupported video buffer format\n");
				return 1;
	}

	switch (video_secondary_format) {
		case FormatNone:
				logTF("Secondary video buffer is disabled\n"); break;
		case FormatYUV420:
				logTF("Secondary video buffer is YUV 4:2:0 planar\n"); break;
		case FormatYUV420DV:
				logTF("Secondary video buffer is YUV 4:2:0 planar with picture shifted down by one line\n"); break;
		default:
				logTF("Unsupported Secondary video buffer format\n");
				return 1;
	}

	if (master_type == MasterNone)
		logTF("Master timecode not used\n");
	else
		logTF("Master timecode type is %s using card %d\n",
				master_type == MasterVITC ? "VITC" : "LTC", master_card);

	logTF("Using %s to determine number of frames to recover when video re-aquired\n",
			recover_timecode_type == RecoverVITC ? "VITC" : "LTC");

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

	//////////////////////////////////////////////////////
	// Attempt to open all sv cards
	//
	// card specified by string of form "PCI,card=n" where n = 0,1,2,3
	// For each card try to open the second channel "PCI,card=n,channel=1"
	// If you try "PCI,card=0,channel=0" on an old sdk you get SV_ERROR_SVOPENSTRING
	// If you see SV_ERROR_WRONGMODE, you need to "svram multichannel on"
	//

	// Check SDK multichannel capability
	if (check_sdk_version3()) {
		sv_info status_info;
		int card = 0;
		int channel = 0;
		for (card = 0; card < max_channels && channel < max_channels; card++)
		{
			char card_str[64] = {0};
	
			snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d,channel=0", card);
	
			int res = sv_openex(&a_sv[channel],
								card_str,
								SV_OPENPROGRAM_DEMOPROGRAM,
								SV_OPENTYPE_INPUT,		// Open V+A input, not output too
								0,
								0);
			if (res == SV_OK) {
				sv_status(a_sv[channel], &status_info);
				width = status_info.xsize;
				height = status_info.ysize;
				logTF("card %d: device present (%dx%d)\n", card, width, height);
				channel++;
				num_sdi_threads++;

				// If card has a multichannel mode on, open second channel
				int multichannel_mode = 0;
				if ((res = sv_option_get(a_sv[channel-1], SV_OPTION_MULTICHANNEL, &multichannel_mode)) != SV_OK) {
					logTF("card %d: sv_option_get(SV_OPTION_MULTICHANNEL) failed: %s\n", card, sv_geterrortext(res));
				}
				if (multichannel_mode) {
					snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d,channel=1", card);
					int res = sv_openex(&a_sv[channel],
								card_str,
								SV_OPENPROGRAM_DEMOPROGRAM,
								SV_OPENTYPE_INPUT,		// Open V+A input, not output too
								0,
								0);
					if (res == SV_OK) {
						sv_status(a_sv[channel], &status_info);
						logTF("card %d: opened second channel (%dx%d)\n", card, status_info.xsize, status_info.ysize);
						channel++;
						num_sdi_threads++;
					}
					else {
						logTF("card %d: failed opening second channel: %s\n", card, sv_geterrortext(res));
					}
				}
				else {
					logTF("card %d: multichannel mode off\n", card);
				}
	
			}
			else
			{
				// typical reasons include:
				//	SV_ERROR_DEVICENOTFOUND
				//	SV_ERROR_DEVICEINUSE
				logTF("card %d: sv_openex(%s) %s\n", card, card_str, sv_geterrortext(res));
				continue;
			}
		}
	}
	else {
		//////////////////////////////////////////////////////
		// Attempt to open all sv cards
		//
		// card specified by string of form "PCI,card=n" where n = 0,1,2,3
		//
		sv_info status_info;
		int card;
		for (card = 0; card < max_channels; card++)
		{
			char card_str[20] = {0};
	
			snprintf(card_str, sizeof(card_str)-1, "PCI,card=%d", card);
	
			int res = sv_openex(&a_sv[card],
								card_str,
								SV_OPENPROGRAM_DEMOPROGRAM,
								SV_OPENTYPE_INPUT,		// Open V+A input, not output too
								0,
								0);
			if (res == SV_OK) {
				sv_status(a_sv[card], &status_info);
				width = status_info.xsize;
				height = status_info.ysize;
				logTF("card %d: device present (%dx%d)\n", card, width, height);
                num_sdi_threads++;
			}
			else
			{
				// typical reasons include:
				//	SV_ERROR_DEVICENOTFOUND
				//	SV_ERROR_DEVICEINUSE
				logTF("card %d: %s\n", card, sv_geterrortext(res));
			}
		}
	}

	if (num_sdi_threads < 1)
	{
		logTF("Error: No SDI monitor threads created.  Exiting\n");
		return 1;
	}

	// Ideally we would get the dma_video_size and audio_size parameters
	// from a fifo's pbuffer structure.  But you must complete a
	// successful sv_fifo_getbuffer() before those parameters are known.
	// Instead use the following values found experimentally since we need
	// to allocate buffers before successful dma transfers occur.
#if DVS_VERSION_MAJOR >= 3
	// TODO: test this for HD-SDI
	dma_video_size = width*height*2 + 6144;
#else
	dma_video_size = width*height*2;
#endif
	audio_size = 0x8000;
	audio_offset = dma_video_size;

	// An element in the ring buffer contains: video(4:2:2) + audio + video(4:2:0)
	element_size =	dma_video_size		// video frame as captured by dma transfer
					+ audio_size		// size of internal structure of dvs dma transfer
										// for 4 channels.  This is greater than 1920 * 4 * 4
										// so we use the last 4 spare bytes for timecode
					+ width*height*3/2;	// YUV 4:2:0 video buffer


	// Create "NO VIDEO" frame
	no_video_frame = (uint8_t*)malloc(width*height*2);
	uyvy_no_video_frame(width, height, no_video_frame);
	if (video_format == FormatYUV422 || video_format == FormatYUV422DV) {
		uint8_t *tmp_frame = (uint8_t*)malloc(width*height*2);
		uyvy_no_video_frame(width, height, tmp_frame);

		// Repack to planar YUV 4:2:2
		uyvy_to_yuv422(		width, height,
							video_format == FormatYUV422DV,	// do DV50 line shift?
							tmp_frame,						// input
							no_video_frame);				// output

		free(tmp_frame);
	}

	if (! allocate_shared_buffers(num_sdi_threads))
	{
		return 1;
	}

	int chan;
	for (chan= 0; chan < num_sdi_threads; chan++)
	{
		int err;
		logTF("chan %d: starting capture thread\n", chan);

		if ((err = pthread_create(&sdi_thread[chan], NULL, sdi_monitor, (void *)chan)) != 0)
		{
			logTF("Failed to create sdi_monitor thread: %s\n", strerror(err));
			return 1;
		}
	}

	// SDI monitor threads never terminate so a waiting in a join will wait forever
	pthread_join(sdi_thread[0], NULL);

	return 0; // silence gcc warning
}