#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include <stdlib.h> /* atoi() */
#include <string.h>     /* strcmp() */
#include <getopt.h>
#include <limits.h>
#include <fcntl.h>  /* open() */
#include <unistd.h> /* close() */
#include <stdio.h>

#include "planar_YUV_io.h"

// Stuff from YUVlib
#include "YUV_frame.h"
#include "YUV_quarter_frame.h"
#include "YUV_text_overlay.h"

static struct option long_options[] =
{
    {"help",      0, NULL, '?'},
    {"interlace", 0, NULL, 0},
    {"hfiltered", 0, NULL, 1},
    {"vfiltered", 0, NULL, 2},
    {"tl",        0, NULL, 10},
    {"tr",        0, NULL, 11},
    {"bl",        0, NULL, 12},
    {"br",        0, NULL, 13},
    {"input",     1, NULL, 20},
    {"title",     1, NULL, 21},
    {"skip",      1, NULL, 22},
    {"count",     1, NULL, 23},
    {"timecode",  1, NULL, 24},
    {"label",     1, NULL, 25},
    {"alltext",   0, NULL, 26},
    {0, 0, 0, 0}
};

static void usage(char* name)
{
    fprintf(stderr, "usage: %s", name);
    int i;
    for (i = 0; long_options[i].name != 0; i++)
    {
        fprintf(stderr, " [-%s", long_options[i].name);
        if (long_options[i].has_arg)
            fprintf(stderr, " value");
        fprintf(stderr, "]");
    }
    fprintf(stderr, "\n");
}

typedef struct
{
    int     valid;
    char    text[1024];
    overlay ovly;
    int     x, y;
} title_info;

typedef struct
{
    int     valid;
    int     fd;
    char    name[1024];
    int     skip;
    int     count;
} input_info;

#define QUADRANTS   4
#define LABEL_LINES 5

int main(int argc, char *argv[])
{
    int         height = 576;
    int         width = 720;
    int         fd_out;
    int         i, n;
    YUV_frame       in_frame;
    YUV_frame       out_frame;
    int         OK;
    int         interlace_mode = 0;
    int         h_filter = 0;
    int         v_filter = 0;
    int         quadrant = 0;
    title_info      title[QUADRANTS];
    input_info      input[QUADRANTS];
    int         frameNo = 0;
    int         tc_quadrant = -1;
    int         tc_x = 0;
    int         tc_y = 0;
    title_info      label[LABEL_LINES];
    char        label_string[1024];
    int         label_quadrant = 1;
    int         all_quadrant = -1;
    p_info_rec      at_info = NULL;
    timecode_data   tc_data;
    BYTE        Y, U, V;    // label colour

    // initialise vars
    for (n = 0; n < LABEL_LINES; n++)
        label[n].valid = 0;
    for (n = 0; n < QUADRANTS; n++)
    {
        title[n].valid = 0;
        title[n].text[0] = '\0';
        input[n].valid = 0;
        input[n].skip = 0;
        input[n].count = INT_MAX;
    }

    // get command line options
    {
        int c;
        while (1)
        {
            c = getopt_long_only(argc, argv, "", long_options, NULL);
            if (c == -1)
                break;
            switch (c)
            {
                case 0:
//                    fprintf(stderr, "Interlace mode\n");
                    interlace_mode = 1;
                    break;
                case 1:
//                    fprintf(stderr, "Horizontal filtered mode\n");
                    h_filter = 1;
                    break;
                case 2:
//                    fprintf(stderr, "Vertical filtered mode\n");
                    v_filter = 1;
                    break;
                case 10:
                case 11:
                case 12:
                case 13:
                    quadrant = c - 10;
//                    fprintf(stderr, "Quadrant %d\n", quadrant);
                    break;
                case 20:
//                    fprintf(stderr, "Input %s\n", optarg);
                    strcpy(input[quadrant].name, optarg);
                    input[quadrant].valid = 1;
                    break;
                case 21:
//                    fprintf(stderr, "Title %s\n", optarg);
                    strcpy(title[quadrant].text, optarg);
                    title[quadrant].valid = 1;
                    break;
                case 22:
                    input[quadrant].skip = atoi(optarg);
//                    fprintf(stderr, "Skip %d\n", input[quadrant].skip);
                    break;
                case 23:
                    input[quadrant].count = atoi(optarg);
//                    fprintf(stderr, "Count %d\n", input[quadrant].count);
                    break;
                case 24:
                    tc_quadrant = quadrant;
                    frameNo = atoi(optarg);
//                    fprintf(stderr, "Timecode %d\n", frameNo);
                    break;
                case 25:
                    label_quadrant = quadrant;
//                    fprintf(stderr, "Label %s\n", optarg);
                    strcpy(label_string, optarg);
                    label[0].valid = 1;
                    break;
                case 26:
                    all_quadrant = quadrant;
                    break;
                case '?':
                    usage(argv[0]);
                    exit(1);
                default:
                    printf ("?? getopt returned character code 0%o ??\n", c);
            }
        }
    }
    // get command line arguments
    if (argc - optind != 0)
    {
        usage(argv[0]);
        exit(1);
    }
    // Inputs will be read into in_frame
    if (!alloc_YUV_frame(&in_frame, width, height, I420))
    {
        perror("alloc_YUV_frame");
        exit(1);
    }
    // Quad split will be constructed in out_frame
    if (!alloc_YUV_frame(&out_frame, width, height, I420))
    {
        perror("alloc_YUV_frame");
        exit(1);
    }

    // Open inputs
    for (n = 0; n < 4; n++)
    {
        if (input[n].valid)
        {
            input[n].fd = open(input[n].name, O_RDONLY);
            if (input[n].fd < 0)
            {
                perror(input[n].name);
                input[n].valid = 0;
            }
        }
        if (input[n].valid && input[n].skip > 0)
            skip_frames(&in_frame, input[n].fd, input[n].skip);
    }

    // check "alltext" quadrant is free
    if (all_quadrant >= 0 && input[all_quadrant].valid)
    {
        fprintf(stderr, "Chosen 'alltext' quadrant being used for input\n");
//        exit(1);
    }
    if (all_quadrant >= 0 && title[all_quadrant].valid)
        title[all_quadrant].valid = 0;

    // set out_frame to black
    clear_YUV_frame(&out_frame);

    // construct title overlays
    if (all_quadrant < 0)
    {
        for (n = 0; n < 4; n++)
        {
            if (title[n].valid)
            {
                if (text_to_overlay(&at_info, &title[n].ovly, title[n].text,
                                    width / 2, "Helvetica", 32, 16, 9) < 0)
                    exit(1);
            }
        }
    }
    // position title overlays
    if (all_quadrant >= 0)
    {
        if (text_to_4box(&at_info, &title[0].ovly,
                         title[0].text, title[1].text,
                         title[2].text, title[3].text,
                         width / 2, "Helvetica", 32, 16, 9) != 0)
            exit(1);
        title[0].x = (width - title[0].ovly.w) * (all_quadrant % 2);
        title[0].y = (height - title[0].ovly.h) * (all_quadrant / 2);
    }
    else
        for (n = 0; n < 4; n++)
            if (title[n].valid)
            {
                title[n].x = (width / 2) * (n % 2);
                title[n].y = (height / 2) * (n / 2);
                title[n].x += (width / 4) - (title[n].ovly.w / 2);
                title[n].y += (height / 2) - title[n].ovly.h - 20;
            }
    // display title overlays
    YUV_601(1.0, 1.0, 0.0, &Y, &U, &V);
    if (all_quadrant >= 0)
        add_overlay(&title[0].ovly, &out_frame, title[0].x, title[0].y,
                    Y, U, V, 40);
    else
    {
        for (n = 0; n < 4; n++)
            if (title[n].valid)
                add_overlay(&title[n].ovly, &out_frame, title[n].x, title[n].y,
                            Y, U, V, 40);
    }

    // Init time code generator
    if (tc_quadrant >= 0)
    {
        init_timecode(&at_info, &tc_data, "Helvetica", 48, 16, 9);
        if (input[tc_quadrant].valid)
        {
            tc_x = (width / 4) - (tc_data.width / 2);
            tc_y = 20;
            tc_x += (width / 2) * (tc_quadrant % 2);
            tc_y += (height / 2) * (tc_quadrant / 2);
        }
        else
        {
            tc_x = (width / 2) - tc_data.width;
            tc_y = (height / 2) - tc_data.height;
            tc_x += tc_data.width * (tc_quadrant % 2);
            tc_y += tc_data.height * (tc_quadrant / 2);
        }
    }

    // construct label overlays
    if (label[0].valid)
    {
        char*   sub_str;
        int count;
        int length;

        // convert '\', 'n' sequences to ' ', '\n'
        length = strlen(label_string);
        if (length >= 2)
            for (i = 0; i < length - 1; i++)
                if (label_string[i] == '\\' && label_string[i+1] == 'n')
                {
                    label_string[i]   = ' ';
                    label_string[i+1] = '\n';
                }
        sub_str = label_string;
        for (i = 0; i < LABEL_LINES; i++)
        {
            if (length <= 0)
                break;
            label[i].x = (width / 2) * (label_quadrant % 2);
            label[i].y = (height / 2) * (label_quadrant / 2);
            label[i].x += 20;
            label[i].y += 72 + (i * 32);
            label[i].valid = 1;
            count = text_to_overlay(&at_info, &label[i].ovly, sub_str,
                                    (width / 2) - 40, "Helvetica", 32, 16, 9);
            if (count < 0)
                exit(1);
            sub_str += count;
            length -= count;
            add_overlay(&label[i].ovly, &out_frame, label[i].x, label[i].y,
                        Y, U, V, 40);
        }
    }

    // get file descriptor for stdout, after flushing
    if (fflush(stdout) != 0)
    {
        perror("fflush(stdout)");
        exit(1);
    }
    setbuf(stdout, NULL);
    fd_out = fileno(stdout);

    unsigned char workSpace[width * 3];
    while (1)
    {
        OK = 0;     // Assume no inputs still active
        for (n = 0; n < 4; n++)
        {
            input[n].skip++;
            if (input[n].valid &&
                input[n].skip > 0 && input[n].skip <= input[n].count)
            {
                // Read an input frame
                if (read_frame(&in_frame, input[n].fd))
                    OK = 1;     // still got an input
                else
                {
                    fprintf(stderr, "end of file %d\n", n);
                    close(input[n].fd);
                    input[n].valid = 0;
                    clear_YUV_frame(&in_frame);
                }
                // Make quarter size copy
                quarter_frame(&in_frame, &out_frame,
                              (width / 2) * (n % 2), (height / 2) * (n / 2),
                              interlace_mode, h_filter, v_filter,
                              &workSpace);
                // Re-write title
                if (all_quadrant < 0 && title[n].valid)
                    add_overlay(&title[n].ovly, &out_frame,
                                title[n].x, title[n].y, Y, U, V, 40);
                else if (all_quadrant == n)
                    add_overlay(&title[0].ovly, &out_frame,
                                title[0].x, title[0].y, Y, U, V, 40);
                // Re-write label
                if (label_quadrant == n)
                {
                    for (i = 0; i < LABEL_LINES; i++)
                    {
                        if (label[i].valid)
                            add_overlay(&label[i].ovly, &out_frame,
                                        label[i].x, label[i].y, Y, U, V, 40);
                    }
                }
            }
        }
        // Write timecode
        if (tc_quadrant >= 0)
            add_timecode(&tc_data, frameNo++, &out_frame, tc_x, tc_y,
                         210, 128-113, 128+18, 100);

        // Write frame to stdout
        write_frame(&out_frame, fd_out);

        if (!OK)
        {
            fprintf(stderr, "end of all files\n");
            break;
        }
    }

    return 0;
}
