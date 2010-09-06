/*
 * $Id: simple_mxf_demux.c,v 1.1 2010/09/06 13:44:07 john_f Exp $
 *
 * Demultiplex MXF file essence container element data
 *
 * Copyright (C) 2008  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Stuart Cunningham
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
#include <unistd.h>
#include <inttypes.h>

static int verbose = 0;

typedef struct {
    uint8_t _key[16];
} Key;

// Frist 12 bytes of essence element key
// Following 4 bytes give Item Type Identifier, element count, element type, element number
static Key g_ess_container_key =
        { { 0x06, 0x0e, 0x2b, 0x34, 0x01, 0x02, 0x01, 0x01, 0x0d, 0x01, 0x03, 0x01} };

static void printKey(Key *p_key)
{
    int i;
    printf("K=");
    for (i = 0; i < 16; i++)
        printf("%02x ", p_key->_key[i]);
    printf("\n");
}

static int readKL(FILE *fp, Key *p_key, uint64_t *p_len)
{
    int i;

    if (fread(p_key->_key, 16, 1, fp) != 1) {
        if (feof(fp))
            return 0;

        perror("fread");
        fprintf(stderr, "Could not read Key\n");
        return 0;
    }

    // Read BER integer (ISO/IEC 8825-1).
    int c;
    uint64_t length = 0;
    if ((c = fgetc(fp)) == EOF) {
        perror("fgetc");
        fprintf(stderr, "Could not read Length\n");
                return 0;
    }

    if (c < 128) {          // high-bit set on first byte?
        length = c;
    }
    else {              // else stores number of bytes in BER integer
        int bytes_to_read = c & 0x7f;
        for (i = 0; i < bytes_to_read; i++) {
            if ((c = fgetc(fp)) == EOF) {
                perror("fgetc");
                fprintf(stderr, "Could not read Length\n");
                return 0;
            }
            length = length << 8;
            length = length | c;
        }
    }
    *p_len = length;

    if (verbose > 1) {
        printf("L=%10"PRId64, length);
        printKey(p_key);
    }

    return 1;
}

static int find_essence_container(FILE *fp, uint64_t *p_len, uint32_t *p_track_num)
{
    uint64_t    len = 0;
    Key         key = {{0}};

    while (1) {
        if (! readKL(fp, &key, &len))
            break;

        // Comparing 8 bytes is sufficient to test for essence container
        // in practice.  See also S379M Table 2
        if (memcmp(key._key, g_ess_container_key._key, 8) == 0)
        {
            *p_track_num =  key._key[12] << 24 |
                            key._key[13] << 16 |
                            key._key[14] << 8 |
                            key._key[15] << 0;
            *p_len = len;
            // file pointer now at start of essence
            return 1;
        }
        fseeko(fp, len, SEEK_CUR);
    }
    // Failed to find an essence container
    return 0;
}

static void usage_exit(void)
{
    fprintf(stderr, "simple_mxf_demux [-n] [-p prefix] input.mxf\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    -n           dry-run no files written\n");
    fprintf(stderr, "    -p prefix    prefix to give to output filenames\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    demuxed data is written to [prefix_]0x<track number>\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    FILE            *fp = NULL;
    int             dryrun = 0;
    char            *input_name = NULL;
    const char      *prefix = 0;

    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            usage_exit();
        }
        else if (strcmp(argv[i], "-n") == 0) {
            dryrun = 1;
        }
        else if (strcmp(argv[i], "-v") == 0) {
            verbose++;
        }
        else if (strcmp(argv[i], "-p") == 0) {
            prefix = argv[i+1];
            i++;
        }
        else {
            if (!input_name) {
                input_name = argv[i];
                continue;
            }
            fprintf(stderr, "Unrecognied argument %s\n", argv[i]);
            usage_exit();
        }
    }
    if (! input_name) {
        usage_exit();
    }

    if ((fp = fopen(input_name, "rb")) == NULL)
    {
        perror("fopen");
        return 1;
    }

    int tracks_found = 0;
    uint32_t a_track_num[65536];
    FILE *a_outfp[65536];

    while (1) {
        uint64_t length = 0;
        uint32_t track_num = 0;
        if (!find_essence_container(fp, &length, &track_num))
            break;

        // search for this track in array of known tracks
        int index = -1;
        for (i = 0; i < tracks_found; i++) {
            if (a_track_num[i] == track_num)
                index = i;
        }

        // not found so add to array of known tracks
        int b_new_track = 0;
        if (index == -1) {
            a_track_num[tracks_found] = track_num;
            index = tracks_found;
            b_new_track = 1;
            tracks_found++;
        }

        if (b_new_track && verbose) {
            off_t pos = ftello(fp);
            printf("Found Essence offset=%"PRId64", track_num=0x%08x, ready to playback %"PRId64" bytes\n", pos, track_num, length);
        }

        if (dryrun) {
            fseeko(fp, length, SEEK_CUR);   // skip over essence
            continue;
        }

        if (b_new_track) {
            // create a suitable output filename using track number
            char outfile[FILENAME_MAX];
            if (prefix)
                sprintf(outfile, "%s_0x%08x", prefix, track_num);
            else
                sprintf(outfile, "0x%08x", track_num);

            if ((a_outfp[index] = fopen(outfile, "wb")) == NULL)
            {
                fprintf(stderr, "Cannot open for write file %s\n", outfile);
                perror("fopen write");
                return 1;
            }
        }

        // write essence to file
        size_t total_read = 0;
        while (total_read < length) {
            uint8_t buf[65536];
            size_t to_read = sizeof(buf);

            if (length - total_read < to_read) {
                // calculate remaining bytes for final read
                to_read = length - total_read;
            }

            size_t r = fread(buf, 1, to_read, fp);
            if (r != to_read) {
                fprintf(stderr, "Could not read from essence stream");
                return 1;
            }
            total_read += r;

            if (fwrite(buf, 1, r, a_outfp[index]) != r) {
                perror("fwrite");
                return 1;
            }
        }
    }

    if (! tracks_found) {
        fprintf(stderr, "Could not find an essence container\n");
        return 1;
    }

    return 0;
}
