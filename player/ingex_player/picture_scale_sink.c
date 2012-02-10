/*
 * $Id: picture_scale_sink.c,v 1.7 2012/02/10 15:16:53 john_f Exp $
 *
 * Copyright (C) 2010 British Broadcasting Corporation, All Rights Reserved
 *
 * Author: Philip de Nier
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <pthread.h> /* for on_screen_display.h (error: ‘pthread_mutex_t’ does not name a type) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <yuvlib/YUV_frame.h>
#include <yuvlib/YUV_scale_pic.h>
#include <yuvlib/YUV_small_pic.h>

#include "picture_scale_sink.h"
#include "logging.h"
#include "utils.h"
#include "macros.h"


#define FIRST_OUTPUT_STREAM_ID      5000

#define INPUT_STREAM                group->scaled_streams


typedef enum
{
    NO_SPLIT = 0,
    QUAD_SPLIT,
    NONA_SPLIT,
} SplitType;

typedef struct ScaledStream
{
    struct ScaledStream *next;

    SplitType split_type;
    int y_display_offset;
    int output_stream_id;
    StreamInfo output_stream_info;

    int target_accepted_frame;
    int target_allocated_buffer;
    unsigned char *output_buffer;
    unsigned int output_buffer_size;
    unsigned int output_data_size;

    unsigned char *workspace;
    size_t workspace_size;
} ScaledStream;

typedef struct ScaledStreamGroup
{
    struct ScaledStreamGroup *next;

    int registered_input;

    unsigned char *input_buffer;
    const unsigned char *const_input_buffer;
    unsigned int input_buffer_size;
    unsigned int input_data_size;

    ScaledStream *scaled_streams;
} ScaledStreamGroup;

struct PictureScaleSink
{
    int apply_scale_filter;
    int use_display_dimensions;

    PSSRaster raster;

    MediaSink *target_sink;
    MediaSink sink;

    ScaledStreamGroup *scaled_stream_groups;
    int next_output_stream_id;
};

typedef struct
{
    PSSRaster raster;

    Rational frame_rate;
    int input_width;
    int input_height;
    Rational input_aspect_ratio;
    int y_display_offset;

    int output_width;
    int output_height;
    Rational output_aspect_ratio;
    int quad_output_width;
    int quad_output_height;
    int nona_output_width;
    int nona_output_height;
} RasterScaleTable;

typedef struct
{
    Rational frame_rate;
    int input_width;
    int input_height;
    int y_display_offset;
} SplitYOffsetTable;


static const RasterScaleTable RASTER_SCALE_TABLE[] =
{
    /* 1440x1080 -> 1920x1080 */
    {PSS_HD_1080_RASTER,
        {0, 0},     1440,   1080, {0, 0}, 0,
                    1920,   1080, {16, 9},
                    960,    540,
                    640,    360,
    },
    /* 1280x1080 -> 1920x1080 */
    {PSS_HD_1080_RASTER,
        {0, 0},     1280,   1080, {0, 0}, 0,
                    1920,   1080, {16, 9},
                    960,    540,
                    640,    360,
    },
    /* 720x576 -> 1920x1080 */
    {PSS_HD_1080_RASTER,
        {25, 1},  720,  576, {4, 3}, 0,     /* pixel aspect 59/54 */
                 1476, 1080, {16, 9},       /* pixel aspect 1/1 */
                  738,  540,
                  492,  360,
    },
    {PSS_HD_1080_RASTER,
        {25, 1},  720,  576, {16, 9}, 0,    /* pixel aspect 118/81 */
                 1920, 1056, {16, 9},       /* pixel aspect 1/1 */
                  960,  528,
                  640,  352,
    },
    /* 720x592 -> 1920x1080 */
    {PSS_HD_1080_RASTER,
        {25, 1},  720,  592, {4, 3}, 16,    /* pixel aspect 59/54 */
                 1476, 1080, {16, 9},       /* pixel aspect 1/1 */
                  738,  540,
                  492,  360,
    },
    {PSS_HD_1080_RASTER,
        {25, 1},  720,  592, {16, 9}, 16,   /* pixel aspect 118/81 */
                 1920, 1056, {16, 9},       /* pixel aspect 1/1 */
                  960,  528,
                  640,  352,
    },
    /* 720x608 -> 1920x1080 */
    {PSS_HD_1080_RASTER,
        {25, 1},  720,  608, {4, 3}, 32,    /* pixel aspect 59/54 */
                 1476, 1080, {16, 9},       /* pixel aspect 1/1 */
                  738,  540,
                  492,  360,
    },
    {PSS_HD_1080_RASTER,
        {25, 1},  720,  608, {16, 9}, 32,   /* pixel aspect 118/81 */
                 1920, 1056, {16, 9},       /* pixel aspect 1/1 */
                  960,  528,
                  640,  352,
    },
    /* 352x296 (single field 704x592) -> 1920x1080 */
    {PSS_HD_1080_RASTER,
        {25, 1},  352,  296, {4, 3}, 8,     /* pixel aspect 59/54 */
                 1404, 1080, {16, 9},       /* pixel aspect 1/1 */
                  702,  540,
                  468,  360,
    },
    {PSS_HD_1080_RASTER,
        {25, 1},  352,  296, {16, 9}, 8,    /* pixel aspect 118/81 */
                 1884, 1080, {16, 9},       /* pixel aspect 1/1 */
                  942,  540,
                  628,  360,
    },
    /* 288x296 (single field 576x592) -> 1920x1080 */
    {PSS_HD_1080_RASTER,
        {25, 1},  288,  296, {4, 3}, 8,     /* pixel aspect 59/54 */
                 1158, 1080, {16, 9},       /* pixel aspect 1/1 */
                  579,  540,
                  386,  360,
    },
    {PSS_HD_1080_RASTER,
        {25, 1},  288,  296, {16, 9}, 8,    /* pixel aspect 118/81 */
                 1548, 1080, {16, 9},       /* pixel aspect 1/1 */
                  774,  540,
                  516,  360,
    },

    /* 720x486 -> 1920x1080 */
    {PSS_HD_1080_RASTER,
        {30000, 1001},  720,  486,  {4, 3}, 0,      /* pixel aspect 11/10 */
                        1764, 1080, {16, 9},        /* pixel aspect 1/1 */
                        882,  540,
                        588,  360,
    },
    {PSS_HD_1080_RASTER,
        {30000, 1001},  720,  486,  {16, 9}, 0,     /* pixel aspect 40/33 */
                        1920, 1068, {16, 9},        /* pixel aspect 1/1 */
                        960,  534,
                        640,  356,
    },
    /* 720x480 -> 1920x1080 */
    {PSS_HD_1080_RASTER,
        {30000, 1001},  720,  480,  {4, 3}, 0,      /* pixel aspect 11/10 */
                        1788, 1080, {16, 9},        /* pixel aspect 1/1 */
                        894,  528,
                        596,  352,
    },
    {PSS_HD_1080_RASTER,
        {30000, 1001},  720,  480,  {16, 9}, 0,     /* pixel aspect 40/33 */
                        1920, 1056, {16, 9},        /* pixel aspect 1/1 */
                        960,  528,
                        640,  352,
    },
    /* 720x496 -> 1920x1080 */
    {PSS_HD_1080_RASTER,
        {30000, 1001},  720,  496,  {4, 3}, 10,     /* pixel aspect 11/10 */
                        1764, 1080, {16, 9},        /* pixel aspect 1/1 */
                        882,  540,
                        588,  360,
    },
    {PSS_HD_1080_RASTER,
        {30000, 1001},  720,  496,  {16, 9}, 10,    /* pixel aspect 40/33 */
                        1920, 1068, {16, 9},        /* pixel aspect 1/1 */
                        960,  534,
                        640,  356,
    },
    /* 720x512 -> 1920x1080 */
    {PSS_HD_1080_RASTER,
        {30000, 1001},  720,  512,  {4, 3}, 26,     /* pixel aspect 11/10 */
                        1764, 1080, {16, 9},        /* pixel aspect 1/1 */
                        882,  540,
                        588,  360,
    },
    {PSS_HD_1080_RASTER,
        {30000, 1001},  720,  512,  {16, 9}, 26,    /* pixel aspect 40/33 */
                        1764, 1068, {16, 9},        /* pixel aspect 1/1 */
                        960,  534,
                        640,  356,
    },
    /* 352x248 (single field 704x496) -> 1920x1080 */
    {PSS_HD_1080_RASTER,
        {30000, 1001},  352,  248,  {4, 3}, 4,      /* pixel aspect 11/10 */
                        1764, 1080, {16, 9},        /* pixel aspect 1/1 */
                        882,  540,
                        588,  360,
    },
    {PSS_HD_1080_RASTER,
        {30000, 1001},  352,  248,  {16, 9}, 4,     /* pixel aspect 40/33 */
                        1920, 1068, {16, 9},        /* pixel aspect 1/1 */
                        960,  534,
                        640,  356,
    },
    /* 288x248 (single field 576x496) -> 1920x1080 */
    {PSS_HD_1080_RASTER,
        {30000, 1001},  288,  248,  {4, 3}, 4,      /* pixel aspect 11/10 */
                        1404, 1080, {16, 9},        /* pixel aspect 1/1 */
                        702,  540,
                        468,  360,
    },
    {PSS_HD_1080_RASTER,
        {30000, 1001},  288,  248,  {16, 9}, 4,     /* pixel aspect 40/33 */
                        1548, 1080, {16, 9},        /* pixel aspect 1/1 */
                        774,  540,
                        516,  360,
    },

    /* 1920x1080 -> 720x576 */
    {PSS_SD_625_RASTER,
        {25, 1},    1920,   1080, {0, 0}, 0,        /* pixel aspect 1/1 */
                    708,    576, {16, 9},           /* pixel aspect 118/81 */
                    354,    288,
                    236,    192,
    },
    /* 1440x1080 -> 720x576 */
    {PSS_SD_625_RASTER,
        {25, 1},    1440,   1080, {0, 0}, 0,        /* pixel aspect 1/1 */
                    708,    576, {16, 9},           /* pixel aspect 118/81 */
                    354,    288,
                    236,    192,
    },

    /* 1920x1080 -> 720x486 */
    {PSS_SD_525_RASTER,
        {30000, 1001},  1920,   1080, {0, 0}, 0,        /* pixel aspect 1/1 */
                        708,    480, {16, 9},           /* pixel aspect 40/33 */
                        354,    240,
                        236,    160,
    },
    /* 1440x1080 -> 720x486 */
    {PSS_SD_525_RASTER,
        {30000, 1001},  1440,   1080, {0, 0}, 0,        /* pixel aspect 1/1 */
                        708,    480, {16, 9},           /* pixel aspect 40/33 */
                        354,    240,
                        236,    160,
    },
    /* 1280x1080 -> 720x486 */
    {PSS_SD_525_RASTER,
        {30000, 1001},  1280,   1080, {0, 0}, 0,        /* pixel aspect 1/1 */
                        708,    480, {16, 9},           /* pixel aspect 40/33 */
                        354,    240,
                        236,    160,
    },

    /* 352x296 (single field 704x592) -> 720x576 */
    {PSS_SD_625_RASTER,
        {25, 1},  352,  296, {4, 3}, 8,
                  708,  576, {4, 3},
                  354,  288,
                  236,  192,
    },
    {PSS_SD_625_RASTER,
        {25, 1},  352,  296, {16, 9}, 8,
                  708,  576, {16, 9},
                  354,  288,
                  236,  192,
    },
    /* 288x296 (single field 576x592) -> 720x576 */
    {PSS_SD_625_RASTER,
        {25, 1},  288,  296, {4, 3}, 8,
                  576,  576, {4, 3},
                  288,  288,
                  192,  192,
    },
    {PSS_SD_625_RASTER,
        {25, 1},  288,  296, {16, 9}, 8,
                  576,  576, {16, 9},
                  288,  288,
                  192,  192,
    },
};

#define RASTER_SCALE_TABLE_SIZE    (sizeof(RASTER_SCALE_TABLE) / sizeof(RasterScaleTable))


static const SplitYOffsetTable SPLIT_YOFFSET_TABLE[] =
{
    {{25, 1},       720,    592,    16},
    {{25, 1},       720,    608,    32},
    {{25, 1},       352,    296,    8},
    {{25, 1},       288,    296,    8},
    {{30000, 1001}, 720,    496,    10},
    {{30000, 1001}, 720,    512,    26},
    {{30000, 1001}, 352,    248,    4},
    {{30000, 1001}, 288,    248,    4},
};

#define SPLIT_YOFFSET_TABLE_SIZE   (sizeof(SPLIT_YOFFSET_TABLE) / sizeof(SplitYOffsetTable))



static size_t find_raster_scale_index(PictureScaleSink *sink, const StreamInfo *stream_info)
{
    size_t i;
    for (i = 0; i < RASTER_SCALE_TABLE_SIZE; i++) {
        if (RASTER_SCALE_TABLE[i].raster == sink->raster &&
            (RASTER_SCALE_TABLE[i].frame_rate.num == 0 ||
                memcmp(&RASTER_SCALE_TABLE[i].frame_rate, &stream_info->frameRate, sizeof(Rational)) == 0) &&
            RASTER_SCALE_TABLE[i].input_width == stream_info->width &&
            RASTER_SCALE_TABLE[i].input_height == stream_info->height &&
            (RASTER_SCALE_TABLE[i].input_aspect_ratio.num == 0 ||
                memcmp(&RASTER_SCALE_TABLE[i].input_aspect_ratio, &stream_info->aspectRatio, sizeof(Rational)) == 0))
        {
            return i;
        }
    }

    return (size_t)(-1);
}

static int find_display_yoffset(PictureScaleSink *sink, const StreamInfo *stream_info)
{
    size_t i;
    for (i = 0; i < SPLIT_YOFFSET_TABLE_SIZE; i++) {
        if (memcmp(&SPLIT_YOFFSET_TABLE[i].frame_rate, &stream_info->frameRate, sizeof(Rational)) == 0 &&
            SPLIT_YOFFSET_TABLE[i].input_width == stream_info->width &&
            SPLIT_YOFFSET_TABLE[i].input_height == stream_info->height)
        {
            return SPLIT_YOFFSET_TABLE[i].y_display_offset;
        }
    }

    return 0;
}

static void free_stream(ScaledStream **stream)
{
    if (!(*stream))
        return;

    SAFE_FREE(&(*stream)->workspace);

    SAFE_FREE(stream);
}

static void deinit_scale_stream_group(ScaledStreamGroup *group)
{
    ScaledStream *scaled_stream, *next_scaled_stream;

    if (!group)
        return;

    scaled_stream = group->scaled_streams;
    while (scaled_stream) {
        next_scaled_stream = scaled_stream->next;
        free_stream(&scaled_stream);
        scaled_stream = next_scaled_stream;
    }
    group->scaled_streams = 0;

    SAFE_FREE(&group->input_buffer);
}

static void free_stream_group(ScaledStreamGroup **group)
{
    if (!(*group))
        return;

    deinit_scale_stream_group(*group);

    SAFE_FREE(group);
}

static int init_scale_stream_group(PictureScaleSink *sink, ScaledStreamGroup *group, int input_stream_id,
                                   const StreamInfo *input_stream_info)
{
    CALLOC_ORET(INPUT_STREAM, ScaledStream, 1);

    INPUT_STREAM->split_type = NO_SPLIT;
    INPUT_STREAM->output_stream_id = input_stream_id;
    INPUT_STREAM->output_stream_info = *input_stream_info;
    if (sink->use_display_dimensions || sink->raster != PSS_UNKNOWN_RASTER)
        INPUT_STREAM->y_display_offset = find_display_yoffset(sink, input_stream_info);

    switch (input_stream_info->format)
    {
        case UYVY_FORMAT:
        case YUV422_FORMAT:
            group->input_buffer_size = input_stream_info->width * input_stream_info->height * 2;
            if (INPUT_STREAM->y_display_offset > 0)
                INPUT_STREAM->output_buffer_size = input_stream_info->width *
                                                   (input_stream_info->height - INPUT_STREAM->y_display_offset) * 2;
            break;
        case YUV420_FORMAT:
            group->input_buffer_size = input_stream_info->width * input_stream_info->height * 3 / 2;
            if (INPUT_STREAM->y_display_offset > 0)
                INPUT_STREAM->output_buffer_size = input_stream_info->width *
                                                   (input_stream_info->height - INPUT_STREAM->y_display_offset) * 3 / 2;
            break;
        default:
            assert(0);
    }
    MALLOC_ORET(group->input_buffer, unsigned char, group->input_buffer_size);


    return 1;
}

static int init_scale_stream(ScaledStream *scaled_stream, SplitType split_type, int output_stream_id,
                             const StreamInfo *output_stream_info)
{
    scaled_stream->split_type = split_type;
    scaled_stream->output_stream_id = output_stream_id;
    scaled_stream->output_stream_info = *output_stream_info;

    switch (output_stream_info->format)
    {
        case UYVY_FORMAT:
        case YUV422_FORMAT:
            scaled_stream->output_buffer_size = output_stream_info->width * output_stream_info->height * 2;
            break;
        case YUV420_FORMAT:
            scaled_stream->output_buffer_size = output_stream_info->width * output_stream_info->height * 3 / 2;
            break;
        default:
            assert(0);
    }

    scaled_stream->workspace_size = 2 * output_stream_info->width * 4;
    MALLOC_ORET(scaled_stream->workspace, unsigned char, scaled_stream->workspace_size);

    return 1;
}

static void init_split_stream_info(const ScaledStreamGroup *group, int y_display_offset, SplitType split_type,
                                   StreamInfo *output_stream_info)
{
    assert(split_type != NO_SPLIT);

    *output_stream_info = INPUT_STREAM->output_stream_info;
    output_stream_info->isScaledPicture = 1;
    output_stream_info->isSplitScaledPicture = 1;
    output_stream_info->scaledPictureSourceStreamId = INPUT_STREAM->output_stream_id;

    switch (split_type)
    {
        case QUAD_SPLIT:
            output_stream_info->width = INPUT_STREAM->output_stream_info.width / 2;
            output_stream_info->height = (INPUT_STREAM->output_stream_info.height - y_display_offset) / 2;
            break;
        case NONA_SPLIT:
            output_stream_info->width = INPUT_STREAM->output_stream_info.width / 3;
            output_stream_info->height = (INPUT_STREAM->output_stream_info.height - y_display_offset) / 3;
            break;
        default:
            assert(0);
    }
}

static void init_raster_stream_info(const ScaledStreamGroup *group, size_t raster_scale_index,
                                    StreamInfo *output_stream_info)
{
    *output_stream_info = INPUT_STREAM->output_stream_info;
    output_stream_info->isScaledPicture = 1;
    output_stream_info->isSplitScaledPicture = 0;
    output_stream_info->scaledPictureSourceStreamId = INPUT_STREAM->output_stream_id;
    output_stream_info->aspectRatio.num = RASTER_SCALE_TABLE[raster_scale_index].output_aspect_ratio.num;
    output_stream_info->aspectRatio.den = RASTER_SCALE_TABLE[raster_scale_index].output_aspect_ratio.den;
    output_stream_info->width = RASTER_SCALE_TABLE[raster_scale_index].output_width;
    output_stream_info->height = RASTER_SCALE_TABLE[raster_scale_index].output_height;
}

static void init_raster_split_stream_info(const ScaledStreamGroup *group, const ScaledStream *raster_stream,
                                          size_t raster_scale_index, SplitType split_type,
                                          StreamInfo *output_stream_info)
{
    assert(split_type != NO_SPLIT);

    *output_stream_info = raster_stream->output_stream_info;
    output_stream_info->isScaledPicture = 1;
    output_stream_info->isSplitScaledPicture = 1;
    output_stream_info->scaledPictureSourceStreamId = raster_stream->output_stream_id;
    output_stream_info->aspectRatio.num = RASTER_SCALE_TABLE[raster_scale_index].output_aspect_ratio.num;
    output_stream_info->aspectRatio.den = RASTER_SCALE_TABLE[raster_scale_index].output_aspect_ratio.den;

    switch (split_type)
    {
        case QUAD_SPLIT:
            output_stream_info->width = RASTER_SCALE_TABLE[raster_scale_index].quad_output_width;
            output_stream_info->height = RASTER_SCALE_TABLE[raster_scale_index].quad_output_height;
            break;
        case NONA_SPLIT:
            output_stream_info->width = RASTER_SCALE_TABLE[raster_scale_index].nona_output_width;
            output_stream_info->height = RASTER_SCALE_TABLE[raster_scale_index].nona_output_height;
            break;
        default:
            assert(0);
    }
}

static ScaledStreamGroup* add_stream_group(PictureScaleSink *sink, int input_stream_id,
                                           const StreamInfo *input_stream_info)
{
    ScaledStreamGroup *prev_group = 0;
    ScaledStreamGroup *new_group = 0;

    CALLOC_ORET(new_group, ScaledStreamGroup, 1);

    // append new group
    if (!sink->scaled_stream_groups) {
        sink->scaled_stream_groups = new_group;
        prev_group = 0;
    } else {
        prev_group = sink->scaled_stream_groups;
        while (prev_group->next)
            prev_group = prev_group->next;
        prev_group->next = new_group;
    }

    CHK_OFAIL(init_scale_stream_group(sink, new_group, input_stream_id, input_stream_info));

    return new_group;

fail:
    free_stream_group(&new_group);
    if (prev_group)
        prev_group->next = 0;
    else
        sink->scaled_stream_groups = 0;
    return 0;
}

static void remove_last_stream_group(PictureScaleSink *sink)
{
    ScaledStreamGroup *next_group;
    ScaledStreamGroup *group;

    if (!sink->scaled_stream_groups)
        return;

    group = sink->scaled_stream_groups;
    next_group = group->next;
    while (next_group && next_group->next) {
        group = next_group;
        next_group = next_group->next;
    }

    if (next_group) {
        free_stream_group(&next_group);
        group->next = 0;
    } else {
        free_stream_group(&sink->scaled_stream_groups);
    }
}

static ScaledStream* add_split_stream(PictureScaleSink *sink, ScaledStreamGroup *group, int y_display_offset,
                                      SplitType split_type)
{
    ScaledStream *prev_scaled_stream = 0;
    ScaledStream *new_scaled_stream = 0;

    CALLOC_ORET(new_scaled_stream, ScaledStream, 1);

    // append stream
    assert(group->scaled_streams);
    prev_scaled_stream = group->scaled_streams;
    while (prev_scaled_stream->next)
        prev_scaled_stream = prev_scaled_stream->next;
    prev_scaled_stream->next = new_scaled_stream;

    new_scaled_stream->y_display_offset = y_display_offset;

    StreamInfo mod_stream_info;
    init_split_stream_info(group, y_display_offset, split_type, &mod_stream_info);
    CHK_OFAIL(init_scale_stream(new_scaled_stream, split_type, sink->next_output_stream_id++, &mod_stream_info));

    return new_scaled_stream;

fail:
    free_stream(&new_scaled_stream);
    prev_scaled_stream->next = 0;
    return 0;
}

static ScaledStream* add_raster_stream(PictureScaleSink *sink, ScaledStreamGroup *group, size_t raster_scale_index)
{
    ScaledStream *prev_scaled_stream = 0;
    ScaledStream *new_scaled_stream = 0;

    CALLOC_ORET(new_scaled_stream, ScaledStream, 1);

    // append stream
    assert(group->scaled_streams);
    prev_scaled_stream = group->scaled_streams;
    while (prev_scaled_stream->next)
        prev_scaled_stream = prev_scaled_stream->next;
    prev_scaled_stream->next = new_scaled_stream;

    new_scaled_stream->y_display_offset = RASTER_SCALE_TABLE[raster_scale_index].y_display_offset;

    StreamInfo mod_stream_info;
    init_raster_stream_info(group, raster_scale_index, &mod_stream_info);
    CHK_OFAIL(init_scale_stream(new_scaled_stream, NO_SPLIT, sink->next_output_stream_id++, &mod_stream_info));

    return new_scaled_stream;

fail:
    free_stream(&new_scaled_stream);
    prev_scaled_stream->next = 0;
    return 0;
}

static ScaledStream* add_raster_split_stream(PictureScaleSink *sink, ScaledStreamGroup *group,
                                             ScaledStream *raster_stream,
                                             size_t raster_scale_index, SplitType split_type)
{
    ScaledStream *prev_scaled_stream = 0;
    ScaledStream *new_scaled_stream = 0;

    CALLOC_ORET(new_scaled_stream, ScaledStream, 1);

    // append stream
    assert(group->scaled_streams);
    prev_scaled_stream = group->scaled_streams;
    while (prev_scaled_stream->next)
        prev_scaled_stream = prev_scaled_stream->next;
    prev_scaled_stream->next = new_scaled_stream;

    new_scaled_stream->y_display_offset = RASTER_SCALE_TABLE[raster_scale_index].y_display_offset;

    StreamInfo mod_stream_info;
    init_raster_split_stream_info(group, raster_stream, raster_scale_index, split_type, &mod_stream_info);
    CHK_OFAIL(init_scale_stream(new_scaled_stream, split_type, sink->next_output_stream_id++, &mod_stream_info));

    return new_scaled_stream;

fail:
    free_stream(&new_scaled_stream);
    prev_scaled_stream->next = 0;
    return 0;
}

static ScaledStreamGroup* get_stream_group(PictureScaleSink *sink, int input_stream_id)
{
    ScaledStreamGroup *group = sink->scaled_stream_groups;
    while (group) {
        if (INPUT_STREAM->output_stream_id == input_stream_id)
            return group;

        group = group->next;
    }

    return 0;
}

static void reset_streams(PictureScaleSink *sink)
{
    ScaledStreamGroup *group = sink->scaled_stream_groups;
    while (group) {
        ScaledStream *scaled_stream = group->scaled_streams;
        while (scaled_stream) {
            scaled_stream->target_accepted_frame = 0;
            scaled_stream->target_allocated_buffer = 0;
            scaled_stream->output_data_size = 0;

            scaled_stream = scaled_stream->next;
        }

        group = group->next;
    }
}

static int scale_picture(PictureScaleSink *sink, ScaledStreamGroup *group, ScaledStream *scaled_stream)
{
    YUV_frame in_frame, out_frame;
    formats format;

    switch (INPUT_STREAM->output_stream_info.format)
    {
        case UYVY_FORMAT:
            format = UYVY;
            CHK_ORET(YUV_frame_from_buffer(&in_frame,
                                           (void*)(group->const_input_buffer +
                                                scaled_stream->y_display_offset * INPUT_STREAM->output_stream_info.width * 2),
                                           INPUT_STREAM->output_stream_info.width,
                                           INPUT_STREAM->output_stream_info.height - scaled_stream->y_display_offset,
                                           format));
            break;
        case YUV422_FORMAT:
            format = Y42B;
            CHK_ORET(YUV_frame_from_buffer(&in_frame, (void*)group->const_input_buffer,
                                           INPUT_STREAM->output_stream_info.width,
                                           INPUT_STREAM->output_stream_info.height,
                                           format));
            if (scaled_stream->y_display_offset > 0) {
                in_frame.Y.buff += scaled_stream->y_display_offset * in_frame.Y.lineStride;
                in_frame.U.buff += scaled_stream->y_display_offset * in_frame.U.lineStride;
                in_frame.V.buff += scaled_stream->y_display_offset * in_frame.V.lineStride;
                in_frame.Y.h -= scaled_stream->y_display_offset;
                in_frame.U.h -= scaled_stream->y_display_offset;
                in_frame.V.h -= scaled_stream->y_display_offset;
            }
            break;
        case YUV420_FORMAT:
            format = I420;
            CHK_ORET(YUV_frame_from_buffer(&in_frame, (void*)group->const_input_buffer,
                                           INPUT_STREAM->output_stream_info.width,
                                           INPUT_STREAM->output_stream_info.height,
                                           format));
            if (scaled_stream->y_display_offset > 0) {
                in_frame.Y.buff += scaled_stream->y_display_offset * in_frame.Y.lineStride;
                in_frame.U.buff += scaled_stream->y_display_offset * in_frame.U.lineStride / 2;
                in_frame.V.buff += scaled_stream->y_display_offset * in_frame.V.lineStride / 2;
                in_frame.Y.h -= scaled_stream->y_display_offset;
                in_frame.U.h -= scaled_stream->y_display_offset / 2;
                in_frame.V.h -= scaled_stream->y_display_offset / 2;
            }
            break;
        default:
            assert(0);
    }


    CHK_ORET(YUV_frame_from_buffer(&out_frame, (void*)scaled_stream->output_buffer,
                                   scaled_stream->output_stream_info.width,
                                   scaled_stream->output_stream_info.height,
                                   format));


    int x_scale = INPUT_STREAM->output_stream_info.width / scaled_stream->output_stream_info.width;
    int y_scale = (INPUT_STREAM->output_stream_info.height - scaled_stream->y_display_offset) / scaled_stream->output_stream_info.height;

    if (x_scale * scaled_stream->output_stream_info.width == INPUT_STREAM->output_stream_info.width &&
        y_scale * scaled_stream->output_stream_info.height ==
                (INPUT_STREAM->output_stream_info.height - scaled_stream->y_display_offset) - scaled_stream->y_display_offset)
    {
        /* small_pic is faster than scale_pic */

        CHK_ORET(small_pic(&in_frame, &out_frame,
                           0, 0,                /* x, y */
                           x_scale, y_scale,    /* xscale, yscale */
                           1,                   /* assume interlaced */
                           sink->apply_scale_filter, sink->apply_scale_filter,  /* horizontal, vertical filter */
                           scaled_stream->workspace) == 0);
    }
    else
    {
        CHK_ORET(scale_pic(&in_frame, &out_frame,
                           0, 0,                /* x, y */
                           scaled_stream->output_stream_info.width, scaled_stream->output_stream_info.height,
                           1,                   /* assume interlaced */
                           sink->apply_scale_filter, sink->apply_scale_filter,  /* horizontal, vertical filter */
                           scaled_stream->workspace, scaled_stream->workspace_size) == 0);
    }

    scaled_stream->output_data_size = scaled_stream->output_buffer_size;

    return 1;
}

static int crop_picture(ScaledStreamGroup *group, ScaledStream *scaled_stream)
{
    int input_width = INPUT_STREAM->output_stream_info.width;
    int input_height = INPUT_STREAM->output_stream_info.height;
    int output_width = input_width;
    int output_height = input_height - scaled_stream->y_display_offset;
    int y_offset = scaled_stream->y_display_offset;

    switch (INPUT_STREAM->output_stream_info.format)
    {
        case UYVY_FORMAT:
            memcpy(scaled_stream->output_buffer,
                   group->const_input_buffer + y_offset * input_width * 2,
                   output_width * output_height * 2);
            break;
        case YUV422_FORMAT:
            memcpy(scaled_stream->output_buffer,
                   group->const_input_buffer + y_offset * input_width,
                   output_width * output_height);
            memcpy(scaled_stream->output_buffer + output_width * output_height,
                   group->const_input_buffer + input_width * input_height + y_offset * input_width / 2,
                   output_width * output_height / 2);
            memcpy(scaled_stream->output_buffer + output_width * output_height * 3 / 2,
                   group->const_input_buffer + input_width * input_height * 3 / 2 + y_offset * input_width / 2,
                   output_width * output_height / 2);
            break;
        case YUV420_FORMAT:
            memcpy(scaled_stream->output_buffer,
                   group->const_input_buffer + y_offset * input_width,
                   output_width * output_height);
            memcpy(scaled_stream->output_buffer + output_width * output_height,
                   group->const_input_buffer + input_width * input_height + y_offset * input_width / 4,
                   output_width * output_height / 4);
            memcpy(scaled_stream->output_buffer + output_width * output_height * 5 / 4,
                   group->const_input_buffer + input_width * input_height * 5 / 4 + y_offset * input_width / 4,
                   output_width * output_height / 4);
            break;
        default:
            assert(0);
    }

    scaled_stream->output_data_size = scaled_stream->output_buffer_size;

    return 1;
}


static int pss_register_listener(void *data, MediaSinkListener *listener)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;

    return msk_register_listener(sink->target_sink, listener);
}

static void pss_unregister_listener(void *data, MediaSinkListener *listener)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;

    msk_unregister_listener(sink->target_sink, listener);
}

static int pss_accept_stream(void *data, const StreamInfo *stream_info)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;
    int result;
    StreamInfo mod_stream_info;

    if (stream_info->type != PICTURE_STREAM_TYPE ||
        (stream_info->format != UYVY_FORMAT &&
            stream_info->format != YUV422_FORMAT &&
            stream_info->format != YUV420_FORMAT))
    {
        /* scaling of input type/format not supported */
        return msk_accept_stream(sink->target_sink, stream_info);
    }


    mod_stream_info = *stream_info;
    if (sink->use_display_dimensions || sink->raster != PSS_UNKNOWN_RASTER)
        mod_stream_info.height -= find_display_yoffset(sink, stream_info);
    result = msk_accept_stream(sink->target_sink, &mod_stream_info);

    /* stream is also accepted if any of the scaled versions are accepted */

    if (stream_info->format == UYVY_FORMAT ||
        stream_info->format == YUV422_FORMAT ||
        stream_info->format == YUV420_FORMAT)
    {
        // create temp group
        ScaledStreamGroup group;
        init_scale_stream_group(sink, &group, 0, stream_info);

        /* split streams */

        int split_yoffset = find_display_yoffset(sink, stream_info);

        init_split_stream_info(&group, split_yoffset, QUAD_SPLIT, &mod_stream_info);
        result = msk_accept_stream(sink->target_sink, &mod_stream_info) || result;

        init_split_stream_info(&group, split_yoffset, NONA_SPLIT, &mod_stream_info);
        result = msk_accept_stream(sink->target_sink, &mod_stream_info) || result;


        /* raster scale and associated split streams */

        if (sink->raster != PSS_UNKNOWN_RASTER) {
            size_t raster_scale_index = find_raster_scale_index(sink, stream_info);

            if (raster_scale_index != (size_t)(-1)) {
                init_raster_stream_info(&group, raster_scale_index, &mod_stream_info);
                result = msk_accept_stream(sink->target_sink, &mod_stream_info) || result;

                /* note: using group.scaled_streams below as substitute for not yet existing raster stream */
                /*       this works ok if target sinks don't look at scaledPictureSourceStreamId at this stage */

                init_raster_split_stream_info(&group, group.scaled_streams, raster_scale_index, QUAD_SPLIT,
                                              &mod_stream_info);
                result = msk_accept_stream(sink->target_sink, &mod_stream_info) || result;

                init_raster_split_stream_info(&group, group.scaled_streams, raster_scale_index, NONA_SPLIT,
                                              &mod_stream_info);
                result = msk_accept_stream(sink->target_sink, &mod_stream_info) || result;
            }
        }

        deinit_scale_stream_group(&group);
    }

    return result;
}

static int pss_register_stream(void *data, int stream_id, const StreamInfo *stream_info)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;
    ScaledStreamGroup *group;
    ScaledStream *raster_stream;
    StreamInfo mod_stream_info;

    if (stream_info->type != PICTURE_STREAM_TYPE ||
        (stream_info->format != UYVY_FORMAT &&
            stream_info->format != YUV422_FORMAT &&
            stream_info->format != YUV420_FORMAT))
    {
        /* scaling of input type/format not supported */
        return msk_register_stream(sink->target_sink, stream_id, stream_info);
    }


    CHK_ORET((group = add_stream_group(sink, stream_id, stream_info)));


    /* raster scale and associated split streams */
    /* Note: register raster scale streams first so that it gets picked up by DVS sink */

    if (sink->raster != PSS_UNKNOWN_RASTER) {
        size_t raster_scale_index = find_raster_scale_index(sink, &INPUT_STREAM->output_stream_info);
        if (raster_scale_index != (size_t)(-1)) {
            init_raster_stream_info(group, raster_scale_index, &mod_stream_info);
            if (msk_register_stream(sink->target_sink, sink->next_output_stream_id, &mod_stream_info)) {
                CHK_OFAIL((raster_stream = add_raster_stream(sink, group, raster_scale_index)));

                init_raster_split_stream_info(group, raster_stream, raster_scale_index, QUAD_SPLIT, &mod_stream_info);
                if (msk_register_stream(sink->target_sink, sink->next_output_stream_id, &mod_stream_info)) {
                    CHK_OFAIL(add_raster_split_stream(sink, group, raster_stream, raster_scale_index, QUAD_SPLIT));
                } else {
                    init_raster_split_stream_info(group, raster_stream, raster_scale_index, NONA_SPLIT, &mod_stream_info);
                    if (msk_register_stream(sink->target_sink, sink->next_output_stream_id, &mod_stream_info))
                        CHK_OFAIL(add_raster_split_stream(sink, group, raster_stream, raster_scale_index, NONA_SPLIT));
                }
            }
        }
    }

    mod_stream_info = INPUT_STREAM->output_stream_info;
    if (sink->use_display_dimensions || sink->raster != PSS_UNKNOWN_RASTER)
        mod_stream_info.height -= find_display_yoffset(sink, &INPUT_STREAM->output_stream_info);
    group->registered_input = msk_register_stream(sink->target_sink, INPUT_STREAM->output_stream_id,
                                                  &mod_stream_info);

    /* split streams */

    if (group->registered_input) {
        int split_yoffset = find_display_yoffset(sink, &INPUT_STREAM->output_stream_info);

        init_split_stream_info(group, split_yoffset, QUAD_SPLIT, &mod_stream_info);
        if (msk_register_stream(sink->target_sink, sink->next_output_stream_id, &mod_stream_info)) {
            CHK_OFAIL(add_split_stream(sink, group, split_yoffset, QUAD_SPLIT));
        } else {
            init_split_stream_info(group, split_yoffset, NONA_SPLIT, &mod_stream_info);
            if (msk_register_stream(sink->target_sink, sink->next_output_stream_id, &mod_stream_info))
                CHK_OFAIL(add_split_stream(sink, group, split_yoffset, NONA_SPLIT));
        }
    }


    if (!group->registered_input && !group->scaled_streams->next) {
        /* nothing was registered - undo addition of group */
        remove_last_stream_group(sink);
        return 0;
    }

    return 1;

fail:
    remove_last_stream_group(sink);
    return 0;
}

static int pss_accept_stream_frame(void *data, int stream_id, const FrameInfo* frame_info)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;

    ScaledStreamGroup *group = get_stream_group(sink, stream_id);
    if (!group)
        return msk_accept_stream_frame(sink->target_sink, stream_id, frame_info);


    /* check all streams whether they accept this frame */

    int target_accepted_frame = 0;
    ScaledStream *scaled_stream = group->scaled_streams;
    while (scaled_stream) {
        scaled_stream->target_accepted_frame = msk_accept_stream_frame(sink->target_sink,
                                                                       scaled_stream->output_stream_id,
                                                                       frame_info);
        target_accepted_frame = target_accepted_frame || scaled_stream->target_accepted_frame;

        scaled_stream = scaled_stream->next;
    }

    return target_accepted_frame;
}

static int pss_get_stream_buffer(void *data, int stream_id, unsigned int buffer_size, unsigned char** buffer)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;

    ScaledStreamGroup *group = get_stream_group(sink, stream_id);
    if (!group)
        return msk_get_stream_buffer(sink->target_sink, stream_id, buffer_size, buffer);


    if (buffer_size > group->input_buffer_size)
        return 0;


    /* get stream buffers for all scaled streams that have accepted */

    int scaled_stream_target_allocated_buffer = 0;
    int target_allocated_buffer = 0;
    ScaledStream *scaled_stream = group->scaled_streams;
    if (INPUT_STREAM->y_display_offset == 0)
        scaled_stream = scaled_stream->next; /* INPUT_STREAM treated as special case below */
    while (scaled_stream) {
        if (scaled_stream->target_accepted_frame) {
            scaled_stream->target_allocated_buffer = msk_get_stream_buffer(sink->target_sink,
                                                                           scaled_stream->output_stream_id,
                                                                           scaled_stream->output_buffer_size,
                                                                           &scaled_stream->output_buffer);
            target_allocated_buffer = target_allocated_buffer || scaled_stream->target_allocated_buffer;
            scaled_stream_target_allocated_buffer = scaled_stream_target_allocated_buffer || scaled_stream->target_allocated_buffer;
        }

        scaled_stream = scaled_stream->next;
    }

    if (INPUT_STREAM->y_display_offset == 0 &&
        INPUT_STREAM->target_accepted_frame && !scaled_stream_target_allocated_buffer)
    {
        /* get target to allocate a buffer if input buffer is not required for scaling */
        INPUT_STREAM->target_allocated_buffer = msk_get_stream_buffer(sink->target_sink,
                                                                      INPUT_STREAM->output_stream_id,
                                                                      buffer_size,
                                                                      &INPUT_STREAM->output_buffer);
        target_allocated_buffer = target_allocated_buffer || INPUT_STREAM->target_allocated_buffer;
    }

    if (scaled_stream_target_allocated_buffer)
        *buffer = group->input_buffer;
    else if (INPUT_STREAM->target_allocated_buffer)
        *buffer = INPUT_STREAM->output_buffer;

    return scaled_stream_target_allocated_buffer || INPUT_STREAM->target_allocated_buffer;
}

static int pss_receive_stream_frame(void *data, int stream_id, unsigned char *buffer, unsigned int buffer_size)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;

    ScaledStreamGroup *group = get_stream_group(sink, stream_id);
    if (!group)
        return msk_receive_stream_frame(sink->target_sink, stream_id, buffer, buffer_size);


    /* send through all streams that have accepted and allocated a buffer */

    int result = 0;
    if (INPUT_STREAM->target_accepted_frame) {
        if (INPUT_STREAM->target_allocated_buffer) {
            if (INPUT_STREAM->y_display_offset > 0) {
                group->const_input_buffer = buffer;
                group->input_data_size = buffer_size;
                CHK_ORET(crop_picture(group, INPUT_STREAM));

                result = msk_receive_stream_frame(sink->target_sink,
                                                  INPUT_STREAM->output_stream_id,
                                                  INPUT_STREAM->output_buffer,
                                                  INPUT_STREAM->output_data_size) || result;
            } else {
                result = msk_receive_stream_frame(sink->target_sink,
                                                  INPUT_STREAM->output_stream_id,
                                                  buffer,
                                                  buffer_size) || result;
            }
        } else {
            result = msk_receive_stream_frame_const(sink->target_sink,
                                                    INPUT_STREAM->output_stream_id,
                                                    buffer,
                                                    buffer_size) || result;
        }
    }

    ScaledStream *scaled_stream = group->scaled_streams;
    scaled_stream = scaled_stream->next; /* skip INPUT_STREAM */
    while (scaled_stream) {
        if (scaled_stream->target_allocated_buffer) {
            group->const_input_buffer = buffer;
            group->input_data_size = buffer_size;
            CHK_ORET(scale_picture(sink, group, scaled_stream));

            result = msk_receive_stream_frame(sink->target_sink,
                                              scaled_stream->output_stream_id,
                                              scaled_stream->output_buffer,
                                              scaled_stream->output_data_size) || result;
        }

        scaled_stream = scaled_stream->next;
    }

    return result;
}

static int pss_receive_stream_frame_const(void *data, int stream_id, const unsigned char *buffer, unsigned int buffer_size)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;
    unsigned char *input_buffer;

    ScaledStreamGroup *group = get_stream_group(sink, stream_id);
    if (!group)
        return msk_receive_stream_frame_const(sink->target_sink, stream_id, buffer, buffer_size);


    if (!pss_get_stream_buffer(data, stream_id, buffer_size, &input_buffer))
        return 0;

    memcpy(input_buffer, buffer, buffer_size);

    return pss_receive_stream_frame(data, stream_id, input_buffer, buffer_size);
}

static int pss_complete_frame(void *data, const FrameInfo* frame_info)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;

    reset_streams(sink);

    return msk_complete_frame(sink->target_sink, frame_info);
}

static void pss_cancel_frame(void *data)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;

    reset_streams(sink);

    msk_cancel_frame(sink->target_sink);
}

static OnScreenDisplay* pss_get_osd(void *data)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;

    return msk_get_osd(sink->target_sink);
}

static VideoSwitchSink* pss_get_video_switch(void *data)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;

    return msk_get_video_switch(sink->target_sink);
}

static AudioSwitchSink* pss_get_audio_switch(void *data)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;

    return msk_get_audio_switch(sink->target_sink);
}

static HalfSplitSink* pss_get_half_split(void *data)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;

    return msk_get_half_split(sink->target_sink);
}

static FrameSequenceSink* pss_get_frame_sequence(void *data)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;

    return msk_get_frame_sequence(sink->target_sink);
}

static int pss_get_buffer_state(void *data, int* num_buffers, int* num_buffers_filled)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;

    return msk_get_buffer_state(sink->target_sink, num_buffers, num_buffers_filled);
}

static int pss_mute_audio(void *data, int mute)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;

    return msk_mute_audio(sink->target_sink, mute);
}

static void pss_sink_close(void *data)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;
    ScaledStreamGroup *group, *next_group;

    msk_close(sink->target_sink);

    group = sink->scaled_stream_groups;
    while (group) {
        next_group = group->next;
        free_stream_group(&group);
        group = next_group;
    }
    sink->scaled_stream_groups = 0;

    SAFE_FREE(&sink);
}

static int pss_reset_or_close(void *data)
{
    PictureScaleSink *sink = (PictureScaleSink*)data;
    ScaledStreamGroup *group, *next_group;
    int result;

    group = sink->scaled_stream_groups;
    while (group) {
        next_group = group->next;
        free_stream_group(&group);
        group = next_group;
    }
    sink->scaled_stream_groups = 0;

    sink->next_output_stream_id = FIRST_OUTPUT_STREAM_ID;


    result = msk_reset_or_close(sink->target_sink);
    if (result != 1) {
        if (result == 2) {
            /* target sink was closed */
            sink->target_sink = NULL;
        }

        pss_sink_close(data);
        return 2;
    }

    return 1;
}



int pss_create_picture_scale(MediaSink *target_sink, int apply_scale_filter, int use_display_dimensions,
                             PictureScaleSink **sink)
{
    PictureScaleSink *new_sink;

    CALLOC_ORET(new_sink, PictureScaleSink, 1);
    new_sink->apply_scale_filter = apply_scale_filter;
    new_sink->use_display_dimensions = use_display_dimensions;
    new_sink->raster = PSS_UNKNOWN_RASTER;
    new_sink->target_sink = target_sink;
    new_sink->next_output_stream_id = FIRST_OUTPUT_STREAM_ID;

    new_sink->sink.data = new_sink;
    new_sink->sink.register_listener = pss_register_listener;
    new_sink->sink.unregister_listener = pss_unregister_listener;
    new_sink->sink.accept_stream = pss_accept_stream;
    new_sink->sink.register_stream = pss_register_stream;
    new_sink->sink.accept_stream_frame = pss_accept_stream_frame;
    new_sink->sink.get_stream_buffer = pss_get_stream_buffer;
    new_sink->sink.receive_stream_frame = pss_receive_stream_frame;
    new_sink->sink.receive_stream_frame_const = pss_receive_stream_frame_const;
    new_sink->sink.complete_frame = pss_complete_frame;
    new_sink->sink.cancel_frame = pss_cancel_frame;
    new_sink->sink.get_osd = pss_get_osd;
    new_sink->sink.get_video_switch = pss_get_video_switch;
    new_sink->sink.get_audio_switch = pss_get_audio_switch;
    new_sink->sink.get_half_split = pss_get_half_split;
    new_sink->sink.get_frame_sequence = pss_get_frame_sequence;
    new_sink->sink.get_buffer_state = pss_get_buffer_state;
    new_sink->sink.mute_audio = pss_mute_audio;
    new_sink->sink.reset_or_close = pss_reset_or_close;
    new_sink->sink.close = pss_sink_close;


    *sink = new_sink;
    return 1;
}

MediaSink* pss_get_media_sink(PictureScaleSink *sink)
{
    return &sink->sink;
}

void pss_set_target_raster(PictureScaleSink *sink, PSSRaster raster)
{
    sink->raster = raster;
}
