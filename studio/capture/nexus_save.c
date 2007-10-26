/*
 * $Id: nexus_save.c,v 1.1 2007/10/26 16:54:19 john_f Exp $
 *
 * Utility to store video frames from dvs_sdi ring buffer to disk files
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

// compile with:
// gcc -Wall -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -O3 -o save_mem save_mem.c
//
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <signal.h>
#include <unistd.h>	
#include <sys/ipc.h>
#include <sys/shm.h>

#include <unistd.h>
#include <sys/times.h>

#include "nexus_control.h"


int verbose = 1;

static char *framesToStr(int tc, char *s)
{
	int frames = tc % 25;
	int hours = (int)(tc / (60 * 60 * 25));
	int minutes = (int)((tc - (hours * 60 * 60 * 25)) / (60 * 25));
	int seconds = (int)((tc - (hours * 60 * 60 * 25) - (minutes * 60 * 25)) / 25);

	if (tc < 0 || frames < 0 || hours < 0 || minutes < 0 || seconds < 0
		|| hours > 59 || minutes > 59 || seconds > 59 || frames > 24)
		sprintf(s, "             ");
		//sprintf(s, "* INVALID *  ");
	else
		sprintf(s, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
	return s;
}

static void usage_exit(void)
{
    fprintf(stderr, "Usage: save_mem [options] videofile\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -c card    save video on specified card [default 0]\n");
    fprintf(stderr, "    -s         save the secondary (4:2:0) video frame\n");
	exit(1);
}

extern int main(int argc, char *argv[])
{
	int				shm_id, control_id;
	uint8_t			*ring[MAX_CHANNELS];
	NexusControl	*pctl = NULL;
	int				cardnum = 0, alt_video = 0;
	char			*video_file = NULL;
	FILE			*outfp = NULL;

	int n;
	for (n = 1; n < argc; n++)
	{
		if (strcmp(argv[n], "-q") == 0)
		{
			verbose = 0;
		}
		else if (strcmp(argv[n], "-c") == 0)
		{
			if (n+1 >= argc ||
				sscanf(argv[n+1], "%d", &cardnum) != 1 ||
				cardnum > 3 || cardnum < 0)
			{
				fprintf(stderr, "-c requires integer card number {0,1,2,3}\n");
				return 1;
			}
			n++;
		}
		else if (strcmp(argv[n], "-a") == 0)
		{
			alt_video = 1;
		}
		else if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0)
		{
			usage_exit();
		}
		else {
			video_file = argv[n];
		}
	}

	if (video_file == NULL) {
		usage_exit();
	}

	// If shared memory not found, sleep and try again
	if (verbose) {
		printf("Waiting for shared memory... ");
		fflush(stdout);
	}

	while (1)
	{
		control_id = shmget(9, sizeof(*pctl), 0444);
		if (control_id != -1)
			break;
		usleep(20 * 1000);
	}

	pctl = (NexusControl*)shmat(control_id, NULL, SHM_RDONLY);
	if (verbose)
		printf("connected to pctl\n");

	if (verbose)
		printf("  cards=%d elementsize=%d ringlen=%d\n",
				pctl->cards,
				pctl->elementsize,
				pctl->ringlen);

	if (cardnum+1 > pctl->cards)
	{
		printf("  cardnum not available\n");
		return 1;
	}

	int i;
	for (i = 0; i < pctl->cards; i++)
	{
		while (1)
		{
			shm_id = shmget(10 + i, pctl->elementsize, 0444);
			if (shm_id != -1)
				break;
			usleep(20 * 1000);
		}
		ring[i] = (uint8_t*)shmat(shm_id, NULL, SHM_RDONLY);
		if (verbose)
			printf("  attached to card[%d]\n", i);
	}

	NexusBufCtl *pc;
	pc = &pctl->card[cardnum];
	int tc, ltc;
	int last_saved = -1;
	int width = pctl->width;
	int height = pctl->height;

	// Default to primary 4:2:2 video
 	int frame_size = width*height*2;
	int video_offset = 0;

	if (alt_video) {
		// get the alternative video frame (4:2:0 planar)
		frame_size = width*height*3/2;
		video_offset = pctl->sec_video_offset;
	}

	if ((outfp = fopen(video_file, "wb")) == NULL) {
		perror("fopen for write");
		return 1;
	}

	while (1)
	{
		if (last_saved == pc->lastframe)
		{
			usleep(20 * 1000);		// 0.020 seconds = 50 times a sec
			continue;
		}

		tc = *(int*)(ring[cardnum] + pctl->elementsize *
									(pc->lastframe % pctl->ringlen)
							+ pctl->tc_offset);
		ltc = *(int*)(ring[cardnum] + pctl->elementsize *
									(pc->lastframe % pctl->ringlen)
							+ pctl->ltc_offset);

		if (fwrite(	ring[cardnum] + video_offset +
								pctl->elementsize *
								(pc->lastframe % pctl->ringlen),
					frame_size,
					1,
					outfp) != 1) {
			perror("fwrite");
			return 1;					
		}

		if (verbose) {
			char tcstr[32], ltcstr[32];
			printf("\rcam%d lastframe=%d  tc=%10d  %s   ltc=%11d  %s ",
					cardnum, pc->lastframe,
					tc, framesToStr(tc, tcstr), ltc, framesToStr(ltc, ltcstr));
			fflush(stdout);
		}

		last_saved = pc->lastframe;
	}

	return 0;
}
