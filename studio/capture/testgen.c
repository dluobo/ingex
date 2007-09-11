/*
 * $Id: testgen.c,v 1.1 2007/09/11 14:08:35 stuart_hc Exp $
 *
 * Dummy SDI input for testing shared memory video & audio interface.
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

#define _XOPEN_SOURCE 600       // for posix_memalign
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <inttypes.h>
#include <string.h>

#include <signal.h>
#include <unistd.h> 
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

#include "nexus_control.h"
#include "video_conversion.h"

// Globals
pthread_t       sdi_thread[4];
int             num_sdi_threads = 0;
NexusControl    *p_control = NULL;
unsigned char   *ring[4] = {0};
int             timecode[4] = {0};
int             control_id, ring_id[4];
int             element_size;
int             g_timecode = 0x10000000;        // 10:00:00:00
char * video_file = 0;
char * audio_file = 0;
int             audio_offset = 720*576*2;
int             audio_size = 0x8000;

static int verbose = 1;

static void cleanup_shared_mem(void)
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

static void cleanup_exit(int res)
{
    printf("cleaning up\n");

    // delete shared memory
    cleanup_shared_mem();

    usleep(100 * 1000);     // 0.1 seconds

    printf("done - exiting\n");
    exit(res);
}

static void catch_sigusr1(int sig_number)
{
    // toggle a flag
}

static void catch_sigint(int sig_number)
{
    printf("\nReceived signal %d - ", sig_number);
    cleanup_exit(0);
}

// Read available physical memory stats and
// allocate shared memory ring buffers accordingly
static int allocate_shared_buffers(int num_cards)
{
    long long   k_shmmax = 0;
    int         ring_len, i;
    FILE        *procfp;

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
    if (k_shmmax > 0x40000000)
    {
        printf("shmmax=%lld (%.3fMiB) probably too big, reducing to 1024MiB\n", k_shmmax, k_shmmax / (1024*1024.0));
        k_shmmax = 0x40000000;  // 1GiB
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
    p_control->elementsize = element_size;
    p_control->audio12_offset = audio_offset;
    p_control->audio34_offset = audio_offset + 0x4000;
    p_control->audio_size = audio_size;
    p_control->tc_offset = audio_offset + audio_size - sizeof(int);
    p_control->ringlen = ring_len;

    // Allocate multiple element ring buffers containing video + audio + tc
    //
    // by default 32MB is available (40 PAL frames), or change it with:
    //  root# echo  104857600 >> /proc/sys/kernel/shmmax  # 120 frms (100MB)
    //  root# echo  201326592 >> /proc/sys/kernel/shmmax  # 240 frms (192MB)
    //  root# echo 1073741824 >> /proc/sys/kernel/shmmax  # 1294 frms (1GB)

    // key for variable number of ring buffers can be 10, 11, 12, 13
    for (i = 0; i < num_cards; i++)
    {
        int key = i + 10;
        ring_id[i] = shmget(key, element_size * ring_len, IPC_CREAT | IPC_EXCL | 0666);
        if (ring_id[i] == -1)   /* shm error */
        {
            int save_errno = errno;
            fprintf(stderr, "Attemp to shmget for key %d: ", key);
            perror("shmget");
            switch (save_errno)
            {
            case EEXIST:
                fprintf(stderr, "Use\n\tipcs | grep '0x0000000[9abcd]'\nplus ipcrm -m <id> to cleanup\n");
                break;
            case ENOMEM:
                fprintf(stderr, "Perhaps you could increase shmmax: e.g.\n  echo 1073741824 >> /proc/sys/kernel/shmmax\n");
                break;
            case EINVAL:
                fprintf(stderr, "You asked for too much?\n");
                break;
            }
    
            return 0;
        }

        ring[i] = (unsigned char*)shmat(ring_id[i], NULL, 0);
        if (ring[i] == (void *)-1)
        {
            fprintf(stderr, "shmat failed for card[%d], ring_id[%d] = %d\n", i, i, ring_id[i]);
            perror("shmat");
            return 0;
        }
        p_control->card[i].key = key;
        p_control->card[i].lastframe = -1;
        p_control->card[i].hwdrop = 0;

        // initialise mutex in shared memory
        if (pthread_mutex_init(&p_control->card[i].m_lastframe, NULL) != 0)
        {
            fprintf(stderr, "Mutex init error\n");
        }

        // Set ring frames to black - too slow!
        // memset(ring[i], 0x80, element_size * ring_len);
    }

    return 1;
}

static int fill_buffers(int num_cards, int shift)
{
    // Open sample video and audio files
    FILE * fp_video = 0;
    FILE * fp_audio = 0;
    if (video_file && (fp_video = fopen(video_file, "rb")) == NULL)
    {
        perror("fopen input video file");
        //return 0;
    }
    if (audio_file && (fp_audio = fopen(audio_file, "rb")) == NULL)
    {
        perror("fopen input video file");
        //return 0;
    }
    if (fp_audio)
    {
        fseek(fp_audio, 44, SEEK_SET);      // skip WAV header
    }

    // Read from sample files storing video + audio in ring buffer
    uint8_t video_uyvy[720*576*2];
    uint8_t video_yuv422[720*576*2];
    uint8_t video_yuv[720*576*3/2];
    uint8_t audio_buf[1920*2*4];

    printf("Filling buffers...\n");
    int frame_num;
    for (frame_num = 0; p_control && frame_num < p_control->ringlen; ++ frame_num)
    {
        if (fp_video && fread(video_uyvy, sizeof(video_uyvy), 1, fp_video) == 1)
        {
            // Video frame successfully read
        }
        else
        {
            memset(video_uyvy, 0x80, sizeof(video_uyvy));
        }
        if (fp_audio && fread(audio_buf, sizeof(audio_buf), 1, fp_audio) == 1)
        {
            // Audio frame successfully read
        }
        else
        {
            memset(audio_buf, 0, sizeof(audio_buf));
        }

        // create YUV buffer
        uyvy_to_yuv420(720, 576, shift, video_uyvy, video_yuv);

        // convert UYVY to planar 4:2:2
        uyvy_to_yuv422(720, 576, shift, video_uyvy, video_yuv422);

        // Copy video and audio across all cards
        int i;
        for (i = 0; i < num_cards; i++)
        {
            memcpy(ring[i] + element_size * frame_num, video_yuv422, sizeof(video_yuv422));
            memcpy(ring[i] + element_size * frame_num + sizeof(video_yuv422), audio_buf, sizeof(audio_buf));
            memcpy(ring[i] + element_size * frame_num + p_control->audio12_offset + p_control->audio_size,
                video_yuv, sizeof(video_yuv));
        }
    }

    // NB. Should report how many frames loaded from files.
    printf("Loaded %d frames of test video & audio into ring buffer\n", frame_num);

    if (fp_video)
    {
        fclose(fp_video);
    }
    if (fp_audio)
    {
        fclose(fp_audio);
    }

    return 1;
}

static char * framesToStr(int tc, char *s)
{
    int frames = tc % 25;
    int hours = (int)(tc / (60 * 60 * 25));
    int minutes = (int)((tc - (hours * 60 * 60 * 25)) / (60 * 25));
    int seconds = (int)((tc - (hours * 60 * 60 * 25) - (minutes * 60 * 25)) / 25);

    sprintf(s, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
    return s;
}

//
// write_picture()
//
// Gets a frame from the SDI FIFO and stores it to memory ring buffer
// (also can write to disk as a debugging option)
static int write_picture(int card)
{
    int                 ring_len = p_control->ringlen;
    NexusBufCtl         *pc = &(p_control->card[card]);

    // Copy timecode into last 4 bytes of element_size
    // This is a bit sneaky but saves maintaining a separate ring buffer
    memcpy(ring[card] + element_size * ((pc->lastframe+1) % ring_len)
            + p_control->tc_offset,
            &timecode[card], sizeof(int));
    // And same in LTC position
    memcpy(ring[card] + element_size * ((pc->lastframe+1) % ring_len)
            + p_control->tc_offset - 4,
            &timecode[card], sizeof(int));

    // Increment timecode by 1 for each frame for testing
    timecode[card] = (timecode[card] + 1) % (24 * 60 * 60 * 25);

    // TODO: Update a burnt-in timecode in the test picture residing in the ring buffer

    if (verbose > 1)
    {
        // timecode:
        //   vitc_tc=268441105 vitc_tc2=-1879042543   for "10:00:16:11"
        char tcstr[32];

        printf("card %d: Wrote frame %5d  size=%d vitc_tc=%s\n",
                    card, pc->lastframe, element_size,
                    framesToStr(timecode[card], tcstr)
                    );
        fflush(stdout);
    }

    // signal frame is now ready
    PTHREAD_MUTEX_LOCK( &pc->m_lastframe )
    pc->lastframe++;
    PTHREAD_MUTEX_UNLOCK( &pc->m_lastframe )

    return 0;
}

// card number passed as void * (using cast)
static void * sdi_monitor(void * arg)
{
    int card = (int)arg;

    // Write frames at 25 fps
    while (1)
    {
        write_picture(card);

        usleep(40 * 1000);
    }

    return NULL;
}

static void usage_exit(void)
{
    fprintf(stderr, "Usage: testgen [-v] [-q] [-h] [-c cards] video_file audio_file\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "e.g.   testgen -c 4 video.uyvy audio.wav\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -c cards     number of cards to simulate\n");
    fprintf(stderr, "    -q           quiet operation\n");
    fprintf(stderr, "    -h           help message\n");
    fprintf(stderr, "\n");
    exit(1);
}

int main (int argc, char ** argv)
{
    int n;
    int max_cards = 4;
    int shift = 1; // Shift field order for DV

    time_t now;
    struct tm * tm_now;

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
        else if (strcmp(argv[n], "-c") == 0)
        {
            if (sscanf(argv[n+1], "%d", &max_cards) != 1 ||
                max_cards > 4 || max_cards <= 0)
            {
                fprintf(stderr, "-c requires integer maximum card number <= 4\n");
                return 1;
            }
            n++;
        }
        // Set video and audio files
        else
        {
            if (video_file == 0)
            {
                video_file = argv[n];
            }
            else if (audio_file == 0)
            {
                audio_file = argv[n];
            }
        }
    }

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
    //
    int card;
    for (card = 0; card < max_cards; card++)
    {
        num_sdi_threads++;
    }

    if (num_sdi_threads < 1)
    {
        fprintf(stderr, "Error: No SDI monitor threads created.  Exiting\n");
        return 1;
    }

    // FIXME - this could be calculated correctly by using a struct for example
    element_size =  720*576*2       // video frame
                    + 0x8000        // size of internal structure of dvs dma transfer
                                    // for 4 channels.  This is greater than 1920 * 4 * 4
                                    // so we use the last 4 bytes for timecode

                    + 720*576*3/2;  // YUV 4:2:0 video buffer

    audio_offset = 720*576*2;
    audio_size = 0x8000;

    if (! allocate_shared_buffers(num_sdi_threads))
    {
        return 1;
    }

    if (! fill_buffers(num_sdi_threads, shift))
    {
        return 1;
    }

    for (card = 0; card < num_sdi_threads; card++)
    {
        int err;
        fprintf(stderr, "card %d: starting capture thread\n", card);

        if ((err = pthread_create(&sdi_thread[card], NULL, sdi_monitor, (void *)card)) != 0)
        {
            fprintf(stderr, "Failed to create sdi_monitor thread: %s\n", strerror(err));
            return 1;
        }
    }

    // SDI monitor threads never terminate.
    // Loop forever monitoring status of threads for logging purposes
    while (1)
    {
        usleep( 2 * 1000 * 1000);
    }
}
