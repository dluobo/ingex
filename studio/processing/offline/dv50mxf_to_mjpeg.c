/*
 * $Id: dv50mxf_to_mjpeg.c,v 1.1 2007/09/11 14:08:45 stuart_hc Exp $
 *
 * Test the MXF Reader 
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <mxf_reader.h>

#include "yuv_to_mjpeg.h"

#include <avcodec.h>
#include <avformat.h>

struct _MXFReaderListenerData
{
    FILE* outFile;
	AVCodecContext *dec;
	AVFrame *decFrame;
	struct	jpeg_compress_struct cinfo;

    MXFReader* input;
    
    uint8_t* buffer;
    uint32_t bufferSize;
};

static void print_timecode(MXFTimecode* timecode)
{
    char* sep;

    if (timecode->isDropFrame)
    {
        sep = ";";
    }
    else
    {
        sep = ":";
    }
    
    printf("%02u%s%02u%s%02u%s%02u", timecode->hour, sep, timecode->min, sep, timecode->sec, sep, timecode->frame);
}

static int accept_frame(MXFReaderListener* listener, int trackIndex)
{
    return 1;
}

static int allocate_buffer(MXFReaderListener* listener, int trackIndex, uint8_t** buffer, uint32_t bufferSize)
{
    MXFTrack* track;
    
    if (listener->data->bufferSize >= bufferSize)
    {
        *buffer = listener->data->buffer;
        return 1;
    }
    
    track = get_mxf_track(listener->data->input, trackIndex);
    if (track == NULL)
    {
        fprintf(stderr, "Received allocate buffer from unknown track %d\n", trackIndex);
        return 0;
    }
    
    if (listener->data->buffer != NULL)
    {
        free(listener->data->buffer);
    }
    listener->data->buffer = (uint8_t*)malloc(bufferSize);
    if (buffer == NULL)
    {
        fprintf(stderr, "Failed to allocate buffer\n");
        return 0;
    }
    listener->data->bufferSize = bufferSize;
    
    *buffer = listener->data->buffer;
    return 1;
}

static void deallocate_buffer(MXFReaderListener* listener, int trackIndex, uint8_t** buffer)
{
    MXFTrack* track;
    
    assert(*buffer == listener->data->buffer);

    track = get_mxf_track(listener->data->input, trackIndex);
    if (track == NULL)
    {
        fprintf(stderr, "Received deallocate buffer from unknown track %d\n", trackIndex);
    }
    
    if (listener->data->buffer != NULL)
    {
        free(listener->data->buffer);
        listener->data->buffer = NULL;
        *buffer = NULL;
    }
}

static int receive_frame(MXFReaderListener* listener, int trackIndex, uint8_t* buffer, uint32_t bufferSize)
{
    MXFTrack* track;
    
    trackIndex = trackIndex; /* avoid compiler warning */

    track = get_mxf_track(listener->data->input, trackIndex);
    if (track == NULL)
    {
        fprintf(stderr, "Received frame from unknown track %d\n", trackIndex);
        return 0;
    }
    printf("  received frame from track index %d with size %d\n", trackIndex, bufferSize);
        
    if (track->isVideo)
    {
		// decode frame
		int finished;
		avcodec_decode_video(listener->data->dec, listener->data->decFrame, &finished, buffer, 288000);
		if(!finished) {
			printf("error decoding DV video\n");
			return 0;
		}

		// encode uncompressed to 20:1 MJPEG and write to output file

		write_field_as_jpeg(...);
		write_field_as_jpeg(...);
    }
    
    
    return 1;
}

static int test1(const char* mxfFilename, const char* outFilename)
{
    MXFReader* input;
    MXFClip* clip;
    int64_t frameCount;
    int64_t frameNumber;
    MXFReaderListenerData data;
    MXFReaderListener listener;
    MXFFile* stdinMXFFile = NULL;
    MXFTimecode playoutTimecode;
    int i;
    MXFTimecode sourceTimecode;
    int type;
    int count;
    int result;
    
    memset(&data, 0, sizeof(MXFReaderListenerData));
    listener.data = &data;
    listener.accept_frame = accept_frame;
    listener.allocate_buffer = allocate_buffer;
    listener.deallocate_buffer = deallocate_buffer;
    listener.receive_frame = receive_frame;
    
    if (strcmp("-", mxfFilename) != 0)
    {
        if (!open_mxf_reader(mxfFilename, &input))
        {
            fprintf(stderr, "Failed to open MXF reader\n");
            return 0;
        }
    }
    else
    {
        if (!mxf_stdin_wrap_read(&stdinMXFFile))
        {
            fprintf(stderr, "Failed to open standard input MXF file\n");
            return 0;
        }
        if (!init_mxf_reader(&stdinMXFFile, &input))
        {
            mxf_file_close(&stdinMXFFile);
            fprintf(stderr, "Failed to open MXF reader using standard input\n");
            return 0;
        }
    }
    listener.data->input = input;
  
  	// Setup ffmpeg decoder
	AVCodec *decoder = avcodec_find_decoder(CODEC_ID_DVVIDEO);
    if (!decoder) {
		fprintf(stderr, "Could not find decoder\n");
		return 0;
    }
    
    data.dec = avcodec_alloc_context();
    if (!data.dec) {
		fprintf(stderr, "Could not allocate decoder context\n");
		return 0;
    }

    avcodec_set_dimensions(data.dec, 720, 576);
    data.dec->pix_fmt = PIX_FMT_YUV422P;
    
    if (avcodec_open(data.dec, decoder) < 0) {
		fprintf(stderr, "Could not open decoder\n");
		return 0;
    }

	data.decFrame = avcodec_alloc_frame();
    if (!data.decFrame) {
		fprintf(stderr, "Could not allocate decoded frame\n");
		return 0;
    }

    //if(thread_count > 1) {
	//avcodec_thread_init(data.dec, thread_count);
	//data.dec->thread_count = thread_count;
    //}

	// Setup JPEG compressor and output file
	struct	jpeg_error_mgr jerr;
	data.cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&data.cinfo);

    if ((data.outFile = fopen(outFilename, "wb")) == NULL) {
        fprintf(stderr, "Failed to open output data file\n");
        return 0;
    }
	jpeg_custom_dest(&data.cinfo, data.outFile);
    
    clip = get_mxf_clip(input);
    
    frameCount = 0;
    while (read_next_frame(input, &listener) == 1)
    {
        frameNumber = get_frame_number(input);
        printf("frame =  %"PFi64"\n", frameNumber);
        get_playout_timecode(input, &playoutTimecode);
        printf("playout timecode = ");
        print_timecode(&playoutTimecode);
        printf("\n");
        for (i = 0; i < get_num_source_timecodes(input); i++)
        {
            if ((result = get_source_timecode(input, i, &sourceTimecode, &type, &count)) == 1)
            {
                printf("source timecode (type = %d, count = %d) = ", type, count);
                print_timecode(&sourceTimecode);
                printf("\n");
            }
            else if (result == -1)
            {
                printf("source timecode (type = %d, count = %d) unavailable", type, count);
                printf("\n");
            }
        }
        frameCount++;
    }
    if (clip->duration != -1 && frameCount != clip->duration)
    {
        fprintf(stderr, "1) Frame count %"PFi64" != duration %"PFi64"\n", frameCount, clip->duration);
        return 0;
    }
    
    close_mxf_reader(&input);
    fclose(data.outFile);
    if (data.buffer != NULL)
    {
        free(data.buffer);
    }
    
    return 1;
}

static void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s (<mxf filename> | -) <output filename>\n", cmd);
}


int main(int argc, const char* argv[])
{
    const char* mxfFilename;
    const char* outFilename;
    int cmdlIndex;

    cmdlIndex = 1;
    
    if (argc != 3)
    {
        usage(argv[0]);
        return 1;
    }
    mxfFilename = argv[cmdlIndex];
    cmdlIndex++;
    outFilename = argv[cmdlIndex];

    if (!test1(mxfFilename, outFilename)) {
        return 1;
    }

    return 0;
}

