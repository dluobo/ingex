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

	sprintf(s, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
	return s;
}

extern int main(int argc, char *argv[])
{
	int				shm_id, control_id;
	uint8_t			*ring[MAX_CHANNELS];
	NexusControl	*pctl = NULL;
	int				tc_loc = 0;			// 0 for VITC, -4 for LTC

	int n;
	for (n = 1; n < argc; n++)
	{
		if (strcmp(argv[n], "-q") == 0)
		{
			verbose = 0;
		}
		else if (strcmp(argv[n], "-v") == 0)
		{
			verbose++;
		}
		else if (strcmp(argv[n], "-t") == 0)
		{
			if (strcmp(argv[n+1], "ltc") == 0) {
				tc_loc = -4;
			}
			n++;
		}
	}

	if (tc_loc == -4)
		printf("Displaying LTC\n");
	else
		printf("Displaying VITC\n");

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

	if (verbose) {
		printf("  channels=%d elementsize=%d ringlen=%d width,height=%dx%d fr=%d/%d\n",
				pctl->channels,
				pctl->elementsize,
				pctl->ringlen,
				pctl->width,
				pctl->height,
				pctl->frame_rate_numer,
				pctl->frame_rate_denom);
		printf("  pri_format=%d sec_format=%d a12_offset=%d a34_offset=%d audio_size=%d\n",
				pctl->pri_video_format,
				pctl->sec_video_format,
				pctl->audio12_offset,
				pctl->audio34_offset,
				pctl->audio_size);
		printf("  signal_ok_offset=%d ltc_offset=%d vitc_offset=%d sec_video_offset=%d\n",
				pctl->signal_ok_offset,
				pctl->ltc_offset,
				pctl->vitc_offset,
				pctl->sec_video_offset);
	}

	int i;
	for (i = 0; i < pctl->channels; i++)
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
			printf("  attached to channel[%d]: '%s'\n", i, pctl->channel[i].source_name);
	}

	int tc[MAX_CHANNELS];
	int last_saved[MAX_CHANNELS] = {-1, -1, -1, -1, -1, -1, -1, -1};
	int first_display = 1;

	while (1)
	{
		for (i = 0; i < pctl->channels; i++)
		{
			NexusBufCtl *pc;
			pc = &pctl->channel[i];
			if (i == 0 && last_saved[0] == pc->lastframe)
			{
				usleep(20 * 1000);		// 0.020 seconds = 50 times a sec
				continue;
			}

			tc[i] = *(int*)(ring[i] + pctl->elementsize * (pc->lastframe % pctl->ringlen)
								+ pctl->vitc_offset + tc_loc);


			last_saved[i] = pc->lastframe;
		}
		if (verbose < 2)
			printf("\r");
		for (i = 0; i < pctl->channels; i++)
		{
			char tcstr[32];
			printf("%d,%d,%s ",
					i, last_saved[i],
					framesToStr(tc[i], tcstr));
		}
		if (pctl->channels == 3)
			printf("offset to 0:%3d,%3d", tc[1] - tc[0], tc[2] - tc[0]);
		else if (pctl->channels == 4)
			printf("offset to 0:%3d,%3d,%3d", tc[1] - tc[0], tc[2] - tc[0], tc[3] - tc[0]);

		if (first_display) {
			printf("\n\n");
			first_display = 0;
		}
		if (verbose >= 2)
			printf("\n");
		else
			fflush(stdout);
	}

	return 0;
}

