/*
 * $Id: convert_audio.c,v 1.3 2011/10/26 17:12:39 john_f Exp $
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "audio_utils.h"

static void usage_exit(void)
{
    fprintf(stderr, "Usage: convert_audio [options] input_audio.pcm [input_audio_n.pcm] output.wav\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  e.g. convert_audio -c 2 input0.pcm input1.pcm out2channel.wav\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -c num_chans        number of channels of input audio to read and multiplex to output\n");
    fprintf(stderr, "                        (determines how many input audio filenames required)\n");
    fprintf(stderr, "    -ib bits per samp   bits per sample of each input file (default 24)\n");
    fprintf(stderr, "    -ob bits per samp   bits per sample for output file (default 16)\n");
    fprintf(stderr, "    -wav                output to wav (default)\n");
    fprintf(stderr, "    -raw                output to raw file\n");
    exit(1);
}

extern int main(int argc, char *argv[])
{
    int             num_channels = 2;
    int             input_bps = 24;
    int             output_bps = 16;
    char            *audio_file[4] = {NULL,NULL,NULL,NULL}, *wav_file = NULL;
    FILE            *infp[4] = {NULL,NULL,NULL,NULL}, *outfp = NULL;
    int             i;
    int wav = 1;

    int n;
    for (n = 1; n < argc; n++)
    {
        if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0)
        {
            usage_exit();
        }
        else if (strcmp(argv[n], "-c") == 0)
        {
            if (n+1 >= argc ||
                sscanf(argv[n+1], "%u", &num_channels) != 1 ||
                num_channels > 4 || num_channels < 1 || num_channels == 3)
            {
                fprintf(stderr, "-c requires number of channels {1,2,4}\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-ib") == 0)
        {
            if (n+1 >= argc ||
                sscanf(argv[n+1], "%u", &input_bps) != 1 ||
                (input_bps != 8 && input_bps != 16 && input_bps && 24 != input_bps && 32))
            {
                fprintf(stderr, "-ib requires bits per sample {8,16,24,32}\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-ob") == 0)
        {
            if (n+1 >= argc ||
                sscanf(argv[n+1], "%u", &output_bps) != 1 ||
                (output_bps != 8 && output_bps != 16 && output_bps && 24 != output_bps && 32))
            {
                fprintf(stderr, "-ob requires bits per sample {8,16,24,32}\n");
                return 1;
            }
            n++;
        }
        else if (strcmp(argv[n], "-wav") == 0)
        {
            wav = 1;
        }
        else if (strcmp(argv[n], "-raw") == 0)
        {
            wav = 0;
        }
        else
        {
            int audio_arg_consumed = 0;
            for (i = 0; i < num_channels; i++)
            {
                if (! audio_file[i])
                {
                    audio_file[i] = argv[n];
                    audio_arg_consumed = 1;
                    break;
                }
            }
            if (audio_arg_consumed)
                continue;

            if (! wav_file)
            {
                wav_file = argv[n];
            }
            else
            {
                usage_exit();
            }
        }
    }

    for (i = 0; i < num_channels; i++)
    {
        if (audio_file[i] == NULL || wav_file == NULL)
        {
            fprintf(stderr, "require %d audio files and one output wav file\n", num_channels);
            usage_exit();
        }
    }

    for (i = 0; i < num_channels; i++)
    {
        if ((infp[i] = fopen(audio_file[i], "rb")) == NULL)
        {
            fprintf(stderr, "Error opening %s\n", audio_file[i]);
            perror("fopen");
            return 1;
        }
    }

    if ((outfp = fopen(wav_file, "wb")) == NULL)
    {
        fprintf(stderr, "Error opening %s\n", wav_file);
        perror("fopen");
        return 1;
    }

    if (wav && ! writeWavHeader(outfp, output_bps, num_channels))
    {
        fprintf(stderr, "Failed to write WAV header\n");
        return 1;
    }

    while (1)
    {
        int input_at_eof = 0;
        uint8_t buf[4][4];      // at most 4 channels each of 32bps

        for (i = 0; i < num_channels; i++)
        {
            if (fread(buf[i], input_bps / 8, 1, infp[i]) != 1)
            {
                if (feof(infp[i]))
                {
                    input_at_eof = 1;
                    break;
                }
                perror("fread from audio file");
                return 1;
            }
        }
        if (input_at_eof)
        {
            break;
        }

        // currently only supports reading 24-bit or 16-bit inputs
        // and writing 16-bit output
        if (input_bps == 16 && output_bps == 16)
        {
            for (i = 0; i < num_channels; i++)
            {
                fwrite(&buf[i][0], 2, 1, outfp);
            }
        }
        else if (input_bps == 24 && output_bps == 16)
        {
            for (i = 0; i < num_channels; i++)
            {
                fwrite(&buf[i][1], 2, 1, outfp);
            }
        }
    }

    if (wav)
    {
        update_WAV_header(outfp);
    }

    return 0;
}

