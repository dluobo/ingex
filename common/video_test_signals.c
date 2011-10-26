/*
 * $Id: video_test_signals.c,v 1.5 2011/10/26 17:11:32 john_f Exp $
 *
 * Video test frames
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
 
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include "video_test_signals.h"

// Represent the colour and position of a colour bar
typedef struct {
	double			position;  // end of bar (right hand edge)
	unsigned char	colour[4]; // UYVY
} bar_colour_t;

// Generate a video buffer containing uncompressed UYVY video representing
// the 100 % colour bars test signal (or YUY2 video if specified).
static void create_colour_bars(unsigned char *video_buffer, int width, int height, int convert_to_YUY2, int rec601)
{
	int				i,j,b;
    const int n_bars = 8;
	bar_colour_t	rec601_UYVY_table[] = {
				{96/720.0,	{0x80,0xEB,0x80,0xEB}},	// white
				{184/720.0,	{0x10,0xD2,0x92,0xD2}},	// yellow
				{272/720.0,	{0xA5,0xA9,0x10,0xA9}},	// cyan
				{360/720.0,	{0x35,0x90,0x22,0x90}},	// green
				{448/720.0,	{0xCA,0x6A,0xDD,0x6A}},	// magenta
				{536/720.0,	{0x5A,0x51,0xF0,0x51}},	// red
				{624/720.0,	{0xf0,0x29,0x6d,0x29}},	// blue
				{720/720.0,	{0x80,0x10,0x80,0x10}},	// black
			};
	bar_colour_t	rec709_UYVY_table[] = {
				{96/720.0,	{0x80,0xEB,0x80,0xEB}},	// white
				{184/720.0,	{0x10,0xDB,0x8A,0xDB}},	// yellow
				{272/720.0,	{0x9A,0xBC,0x10,0xBC}},	// cyan
				{360/720.0,	{0x2A,0xAD,0x1A,0xAD}},	// green
				{448/720.0,	{0xD6,0x4E,0xE6,0x4E}},	// magenta
				{536/720.0,	{0x66,0x3F,0xF0,0x3F}},	// red
				{624/720.0,	{0xF0,0x20,0x76,0x20}},	// blue
				{720/720.0,	{0x80,0x10,0x80,0x10}},	// black
			};
	bar_colour_t *UYVY_table = (rec601 ? rec601_UYVY_table : rec709_UYVY_table);

	for (j = 0; j < height; j++)
	{
		for (i = 0; i < width; i+=2)
		{
			for (b = 0; b < n_bars; b++)
			{
				if ((i / ((double)width)) < UYVY_table[b].position)
				{
					if (convert_to_YUY2) {
						// YUY2 packing
						video_buffer[j*width*2 + i*2 + 1] = UYVY_table[b].colour[0];
						video_buffer[j*width*2 + i*2 + 0] = UYVY_table[b].colour[1];
						video_buffer[j*width*2 + i*2 + 3] = UYVY_table[b].colour[2];
						video_buffer[j*width*2 + i*2 + 2] = UYVY_table[b].colour[3];
					}
					else {
						// UYVY packing
						video_buffer[j*width*2 + i*2 + 0] = UYVY_table[b].colour[0];
						video_buffer[j*width*2 + i*2 + 1] = UYVY_table[b].colour[1];
						video_buffer[j*width*2 + i*2 + 2] = UYVY_table[b].colour[2];
						video_buffer[j*width*2 + i*2 + 3] = UYVY_table[b].colour[3];
					}
					break;
				}
			}
		}
	}
}

// Random number between 16 and 235 inclusive (range of 220)
static int rand_y(void)
{
	return rand() % 220 + 16;
}

// Random number between 16 and 240 inclusive (range of 225)
static int rand_uv(void)
{
	return rand() % 225 + 16;
}

// Generate a random video frame, adhering to 16-235 legal range for Y and 16-240 for UV
// Useful for stress-testing video encoders
void uyvy_random_frame(int width, int height, unsigned char *video_buffer)
{
	int				i,j;

	for (j = 0; j < height; j++)
	{
		for (i = 0; i < width; i+=2)
		{
			// UYVY packing
			video_buffer[j*width*2 + i*2 + 0] = rand_uv();
			video_buffer[j*width*2 + i*2 + 1] = rand_y();
			video_buffer[j*width*2 + i*2 + 2] = rand_uv();
			video_buffer[j*width*2 + i*2 + 3] = rand_y();
		}
	}
}


void uyvy_color_bars(int width, int height, int rec601, uint8_t *output)
{
	create_colour_bars(output, width, height, 0, rec601);
}

void uyvy_black_frame(int width, int height, uint8_t *output)
{
	int video_size = width * height * 2;	// UYVY is 4:2:2

	int i;
	for (i = 0; i < video_size; i+= 4) {
		output[i+0] = 0x80;		// U
		output[i+1] = 0x10;		// Y
		output[i+2] = 0x80;		// V
		output[i+3] = 0x10;		// Y
	}
}	

#include "video_captions.h"

void uyvy_no_video_frame(int width, int height, uint8_t *output)
{
	int caption_width = 192;
	int caption_height = 28;

	int x = width / 2 - caption_width / 2;
	int y = height / 2 - caption_height / 2;

	// start with black frame
	uyvy_black_frame(width, height, output);

	// add caption
	int i;
	int start_pos = y * width * 2+ x * 2;
	for (i = 0; i < caption_width * caption_height * 2; i++) {
		output[i + start_pos] = uyvy_no_video[i];
		if ((i+1) % (caption_width*2) == 0) {
			start_pos += (width - caption_width) * 2;
		}
	}
}	

void yuv420p_black_frame(int width, int height, uint8_t *output)
{
    int ySize = width * height;
    int uSize = width * height / 4;

    uint8_t* y = output;
    uint8_t* u = output + ySize;
    uint8_t* v = output + ySize + uSize;
    
    int i;
    for (i = 0; i < ySize; i++)
    {
        (*y++) = 0x10;
        if (i < uSize)
        {
            (*u++) = 0x80;
            (*v++) = 0x80;
        }
    }
}


