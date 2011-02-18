/*
 * Add burnt-in-timecode to video frame
 *
 * Copyright (C) 2011  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
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

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>

#include "video_burn_in_timecode.h"



typedef enum
{
    UYVY_FORMAT,
    YUV422_FORMAT,
    YUV420_FORMAT,
} PictureFormat;

typedef void (*burn_mask_func)(const Ingex::Timecode &timecode,
                               int x_offset, int y_offset,
                               int frame_width, int frame_height,
                               unsigned char *frame);



static void usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s [options] <input> <output>\n", cmd);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h|--help        Print this usage message and exit\n");
    fprintf(stderr, "  -f <format>      Picture format: uyvy, yuv422 or yuv420. Default uyvy\n");
    fprintf(stderr, "  -s <W>x<H>       Picture width x height dimensions. Default 720x576\n");
    fprintf(stderr, "  -r <rate>        Frame rate, 25, 29 (=30000/1001), 50 or 59 (=60000/1001). Default 25\n");
    fprintf(stderr, "  -l <dur>         Number frames to process. Default all available frames\n");
    fprintf(stderr, "  -t <tc>          Start timecode. <tc> can be a frame count or hh:mm:ss:ff. Default 0\n");
    fprintf(stderr, "  -d               Drop frame timecode\n");
    fprintf(stderr, "  -x <off>         Timecode overlay x offset. Default 0\n");
    fprintf(stderr, "  -y <off>         Timecode overlay y offset. Default 0\n");
}

int main(int argc, const char **argv)
{
    int cmdln_index;
    PictureFormat format = UYVY_FORMAT;
    int width = 720;
    int height = 576;
    int frame_rate = 25;
    int fps_num = 25, fps_den = 1;
    int duration = -1;
    Ingex::Timecode timecode;
    bool drop_frame = false;
    const char *start_tc_str = NULL;
    const char *input_filename = NULL;
    const char *output_filename = NULL;
    int x_offset = 0;
    int y_offset = 0;

    if (argc == 1) {
        usage(argv[0]);
        return 0;
    }

    for (cmdln_index = 1; cmdln_index < argc; cmdln_index++) {
        if (strcmp(argv[cmdln_index], "-h") == 0 ||
            strcmp(argv[cmdln_index], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdln_index], "-f") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (strcmp(argv[cmdln_index + 1], "uyvy") == 0)
            {
                format = UYVY_FORMAT;
            }
            else if (strcmp(argv[cmdln_index + 1], "yuv422") == 0)
            {
                format = YUV422_FORMAT;
            }
            else if (strcmp(argv[cmdln_index + 1], "yuv420") == 0)
            {
                format = YUV420_FORMAT;
            }
            else
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument '%s' for option '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-s") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%dx%d", &width, &height) != 2 ||
                width <= 0 || height <= 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument '%s' for option '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-r") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%d", &frame_rate) != 1)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument '%s' for option '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            switch (frame_rate)
            {
                case 25:
                    fps_num = 25;
                    fps_den = 1;
                    break;
                case 29:
                    fps_num = 30000;
                    fps_den = 1001;
                    break;
                case 50:
                    fps_num = 50;
                    fps_den = 1;
                    break;
                case 59:
                    fps_num = 60000;
                    fps_den = 1001;
                    break;
                default:
                    usage(argv[0]);
                    fprintf(stderr, "Invalid argument '%s' for option '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                    return 1;
            }
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-l") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%d", &duration) != 1 ||
                duration < 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument '%s' for option '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-t") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            // parsed later on when frame rate and drop frame are known
            start_tc_str = argv[cmdln_index + 1];
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-d") == 0)
        {
            drop_frame = true;
        }
        else if (strcmp(argv[cmdln_index], "-x") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%d", &x_offset) != 1 ||
                x_offset < 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument '%s' for option '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "-y") == 0)
        {
            if (cmdln_index + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for option '%s'\n", argv[cmdln_index]);
                return 1;
            }
            if (sscanf(argv[cmdln_index + 1], "%d", &y_offset) != 1 ||
                y_offset < 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument '%s' for option '%s'\n", argv[cmdln_index + 1], argv[cmdln_index]);
                return 1;
            }
            cmdln_index++;
        }
        else
        {
            break;
        }
    }

    if (cmdln_index >= argc) {
        usage(argv[0]);
        fprintf(stderr, "Missing <input> and <output> arguments\n");
        return 1;
    } else if (cmdln_index + 1 >= argc) {
        usage(argv[0]);
        fprintf(stderr, "Missing <output> argument\n");
        return 1;
    } else if (cmdln_index + 2 < argc) {
        usage(argv[0]);
        fprintf(stderr, "Unknown argument '%s'\n", argv[cmdln_index]);
        return 1;
    }

    input_filename = argv[cmdln_index];
    output_filename = argv[cmdln_index + 1];


    if (start_tc_str) {
        int hour_or_count, min, sec, frame;
        int parse_count = sscanf(start_tc_str, "%d:%d:%d:%d", &hour_or_count, &min, &sec, &frame);
        if (parse_count == 4 || parse_count == 3)
            timecode = Ingex::Timecode(start_tc_str, fps_num, fps_den, drop_frame);
        else if (parse_count == 1)
            timecode = Ingex::Timecode(hour_or_count, fps_num, fps_den, drop_frame);

        if (timecode.IsNull()) {
            usage(argv[0]);
            fprintf(stderr, "Invalid argument '%s' for option -t\n", start_tc_str);
            return 1;
        }
    } else {
        timecode = Ingex::Timecode(0, fps_num, fps_den, drop_frame);
    }



    FILE *input = fopen(input_filename, "rb");
    if (!input) {
        fprintf(stderr, "Failed to open input '%s': %s\n", input_filename, strerror(errno));
        return 1;
    }

    FILE *output = fopen(output_filename, "wb");
    if (!output) {
        fprintf(stderr, "Failed to open output '%s': %s\n", output_filename, strerror(errno));
        return 1;
    }


    int frame_size = 0;
    burn_mask_func burn_mask = burn_mask_uyvy;
    switch (format)
    {
        case UYVY_FORMAT:
            burn_mask = burn_mask_uyvy;
            frame_size = width * height * 2;
            break;
        case YUV422_FORMAT:
            burn_mask = burn_mask_yuv422;
            frame_size = width * height * 2;
            break;
        case YUV420_FORMAT:
            burn_mask = burn_mask_yuv420;
            frame_size = width * height * 3 / 2;
            break;
    }
    unsigned char *frame_buffer = new unsigned char[frame_size];


    int count = 0;
    while (duration < 0 || count < duration) {
        if (fread(frame_buffer, frame_size, 1, input) != 1)
            break;

        burn_mask(timecode, x_offset, y_offset, width, height, frame_buffer);

        if (fwrite(frame_buffer, frame_size, 1, output) != 1) {
            fprintf(stderr, "Failed to write frame: %s\n", strerror(errno));
            break;
        }

        timecode += 1;
        count++;
    }

    printf("Processed %d frames\n", count);


    delete frame_buffer;
    fclose(output);
    fclose(input);

    return 0;
}

