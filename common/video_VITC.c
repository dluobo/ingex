/*
 * $Id: video_VITC.c,v 1.4 2010/06/18 08:53:42 john_f Exp $
 *
 * Utilities for reading, writing and dealing with VITC timecodes
 *
 * Copyright (C) 2005  British Broadcasting Corporation.
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
#include <time.h>
#include <sys/time.h>
#include "video_VITC.h"

extern char *framesToStr(int tc, char *s)
{
	int frames = tc % 25;
	int hours = (int)(tc / (60 * 60 * 25));
	int minutes = (int)((tc - (hours * 60 * 60 * 25)) / (60 * 25));
	int seconds = (int)((tc - (hours * 60 * 60 * 25) - (minutes * 60 * 25)) / 25);

	sprintf(s, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
	return s;
}

// Table to quickly convert DVS style timecodes to integer timecodes
static int dvs_to_int[128] = {
 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0, 
10, 11, 12, 13, 14, 15, 16, 17, 18, 19,  0,  0,  0,  0,  0,  0, 
20, 21, 22, 23, 24, 25, 26, 27, 28, 29,  0,  0,  0,  0,  0,  0, 
30, 31, 32, 33, 34, 35, 36, 37, 38, 39,  0,  0,  0,  0,  0,  0, 
40, 41, 42, 43, 44, 45, 46, 47, 48, 49,  0,  0,  0,  0,  0,  0, 
50, 51, 52, 53, 54, 55, 56, 57, 58, 59,  0,  0,  0,  0,  0,  0,
60, 61, 62, 63, 64, 65, 66, 67, 68, 69,  0,  0,  0,  0,  0,  0,
70, 71, 72, 73, 74, 75, 76, 77, 78, 79,  0,  0,  0,  0,  0,  0
};

extern int dvs_tc_to_int(int tc)
{
	// E.g. 0x09595924
	// E.g. 0x10000000
	int hours = dvs_to_int[(tc & 0x3f000000) >> 24];
	int mins  = dvs_to_int[(tc & 0x007f0000) >> 16];
	int secs  = dvs_to_int[(tc & 0x00007f00) >> 8];
	int frames= dvs_to_int[(tc & 0x0000003f)];

	int tc_as_total =	hours * (25*60*60) +
						mins * (25*60) +
						secs * (25) +
						frames;

	return tc_as_total;
}

extern int tc_to_int(unsigned hh, unsigned mm, unsigned ss, unsigned ff)
{
	int tc_as_total =	hh * (25*60*60) +
						mm * (25*60) +
						ss * (25) +
						ff;
	return tc_as_total;
}


typedef struct {
	const unsigned char *line;
	int y_stride, y_offset;
	int bit0_centre;
	float bit_width;
} VITCinfo;

// Input must be one line of UYVY or Y video (720 pixels)
extern unsigned VBIbit(VITCinfo *pinfo, int bit)
{
	// Bits are 7.5 luma samples wide (see SMPTE-266)
	// Experiment showed bit 0 is centered on luma sample
	// 28 for DVS
	// 30 for Rubidium
	// 27 for D3 at left of picture
	//int bit_pos = (int)(bit * 7.5 + 0.5) + 28;
	int bit_pos = (int)(bit * pinfo->bit_width + 0.5) + pinfo->bit0_centre;

	// use the bit_pos index into the Y data
	unsigned char Y = pinfo->line[bit_pos * pinfo->y_stride + pinfo->y_offset];

	// If the luma value is greater than 0x7F then it is
	// likely to be a "1" bit value.  SMPTE-266M says the
	// state of 1 is indicated by 0xC0.
	return (Y > 0x7F);
}

extern unsigned readVBIbits(VITCinfo *pinfo, int bit, int width)
{
	unsigned value = 0;
	int i;

	for (i=0; i < width; i++) {
		value |= (VBIbit(pinfo, bit + i) << i);
	}
	return value;
}

extern int readVITC(const unsigned char *line, int is_uyvy,
					unsigned *hh, unsigned *mm, unsigned *ss, unsigned *ff)
{
	unsigned CRC = 0;
	int i;
	VITCinfo info;
	info.line = line;
	if (is_uyvy) {
	    // UYVY packing
	    info.y_stride = 2;
	    info.y_offset = 1;
	} else {
	    // Y only
	    info.y_stride = 1;
	    info.y_offset = 0;
	}

	// First, scan from left to find first sync pair (black->white->black).
	// Start and end pixels (18 to 42) are very conservative (VITC would
	// need to be very wrong to be close to these limits).
	int bit0_start = -1;
	int bit0_end = -1;
	int lastY, last2Y;
	lastY = 0xFF;
	last2Y = 0xFF;
	for (i = 18; i < 42; i++) {
		unsigned char Y = line[i*info.y_stride + info.y_offset];
		//printf("i=%d offset=%d Y=%x\n", i, i*info.y_stride + info.y_offset, Y);
		if (bit0_start == -1) {
			if (Y > 0xB0)
				bit0_start = i;
			continue;
		}
		// bit 0 start found, look for bit 0 end
		if (Y <= 0xB0 && lastY <= 0xB0 && Y < lastY && lastY < last2Y) {
			bit0_end = i - 2;
			break;
		}
		last2Y = lastY;
		lastY = Y;
	}
	//printf("bit0_st=%d bit0_en=%d width=%d\n", bit0_start, bit0_end, bit0_end - bit0_start + 1);
	if (bit0_start == -1 || bit0_end == -1)		// No sync at bits 0 to 1
		return 0;

	// The centre of bit zero is set to be 2 pixels before the end since the spec
	// shows the width of the peak to be 4 pixels of 0xC0 values
	info.bit0_centre = bit0_end - 2;

	// Now, find the transition from bit 80 to bit 81 (last sync pair).
	// We can't reliably tell when bit 80 started since the previous
	// bit (bit 79) is part of BG6 and may have been set high (1).
	int bit80_end = -1;

	// The centre of bit80 should be 630 = 30 + 7.5 * 80 (SMPTE 266M shows 30 as bit0 centre)
	int scan_start = 630;

	// If bit0 is not quite right (should be 30), start a little further along.
	if (info.bit0_centre > 32)
		scan_start = 634;

	lastY = 0xFF;
	last2Y = 0xFF;
	for (i = scan_start; i < scan_start + 7.5*2; i++) {
		unsigned char Y = line[i*info.y_stride + info.y_offset];
		//printf("i=%d offset=%d Y=%x\n", i, i*info.y_stride + info.y_offset, Y);
		if (Y <= 0xB0 && lastY <= 0xB0 && Y < lastY && lastY < last2Y) {
			bit80_end = i - 2;
			break;
		}
		last2Y = lastY;
		lastY = Y;
	}
	//printf("bit80_end=%d\n", bit80_end);
	//printf("bit0_centre=%d, bit_width=%.2f\n", bit0_end - 2, (bit80_end-bit0_end)/80.0);

	if (bit80_end == -1)		// No sync bit at bit 80
		return 0;

	info.bit_width = (bit80_end-bit0_end)/80.0;

	for (i = 0; i < 10; i++)
		CRC ^= readVBIbits(&info, i*8, 8);
	CRC = (((CRC & 3) ^ 1) << 6) | (CRC >> 2);
	//unsigned film_CRC = CRC ^ 0xFF;
	//unsigned prod_CRC = CRC ^ 0xF0;
	unsigned crc_read = readVBIbits(&info, 82, 8);
	//printf("crc_read=%d CRC=%u fCRC=%u pCRC=%u\n", crc_read,CRC,film_CRC,prod_CRC);
	if (crc_read != CRC)
		return 0;

	// Note that tens of frames are two bits - flags bits are ignored
	*ff = readVBIbits(&info, 2, 4) + readVBIbits(&info, 12, 2) * 10;
	*ss = readVBIbits(&info, 22, 4) + readVBIbits(&info, 32, 3) * 10;
	*mm = readVBIbits(&info, 42, 4) + readVBIbits(&info, 52, 3) * 10;
	*hh = readVBIbits(&info, 62, 4) + readVBIbits(&info, 72, 2) * 10;
	return 1;
}

// Return true if the line contains only black or grey (< 0x80)
// luminance values and no colour difference.
// Useful for detecting when there is no VITC at all on a line.
extern int black_or_grey_line(const unsigned char *uyvy_line, int width)
{
	int i;
	for (i = 0; i < width; i++) {
		unsigned char UV = uyvy_line[i*2];
		unsigned char Y = uyvy_line[i*2+1];

		if (abs(0x80 - UV) > 1) {
			// contains colour, therefore not a grey or black line
			return 0;
		}

		if (Y >= 0x80) {
			// contains a reasonably bright pixel
			return 0;
		}
	}
	return 1;
}

extern void draw_vitc_line(unsigned char value[9], unsigned char *line, int is_uyvy)
{
	// Modelled on digibeta output where it was found:
	// * peak or trough of bit is always 5 pixels wide
	// * start of first sync bit is pixel 25
	
	// stride and offset
	int y_stride = 1;
	int y_offset = 0;
	if (is_uyvy) {
	    y_stride = 2;
	    y_offset = 1;
	}

	// Draw transition from low to high for first sync bit
	int pos = 25;
	line[pos++ *y_stride + y_offset] = 0x2A;
	line[pos++ *y_stride + y_offset] = 0x68;
	line[pos++ *y_stride + y_offset] = 0xA6;
	// position is now at start of first sync bit peak (pos==28)

	int val_i, i;
	for (val_i = 0; val_i < 9; val_i++) {
		unsigned char val = value[val_i];
		//printf("drawing value %d at pixel pos %d (val_i=%d)\n", val, pos, val_i);

		// loop through 10 bits representing the byte (2 bits sync + 8 bits data)
		for (i = 0; i < 10; i++) {
			int even_bit = (i % 2) == 0;

			int bit, next_bit;
			if (i == 0) {		// handle sync bit pair specially
				bit = 1;
				next_bit = 0;
			}
			else if (i == 1) {
				bit = 0;
				next_bit = val & 1;
			}
			else {				// normal bit
				bit = (val >> (i-2)) & 1;
				// To calculate next_bit check whether we are at the end of the byte
				// in which case it will always be 1 (from sync bit) or 0 at end of the line
				if (val_i == 8)		// end of the line (CRC byte)?
					next_bit = ((i-2) == 7) ? 0 : (val >> ((i-2)+1)) & 1;
				else
					next_bit = ((i-2) == 7) ? 1 : (val >> ((i-2)+1)) & 1;
			}

			//printf("  i=%d %s b=%d nb=%d\n", i, even_bit?"even":"odd ", bit, next_bit);

			if (even_bit) {
				// draw 7 pixels for even bit
				if (bit) {
					line[pos++ *y_stride + y_offset] = 0xC0;
					line[pos++ *y_stride + y_offset] = 0xC0;
					line[pos++ *y_stride + y_offset] = 0xC0;
					line[pos++ *y_stride + y_offset] = 0xC0;
					line[pos++ *y_stride + y_offset] = 0xC0;
					if (next_bit) {				// keep high
						line[pos++ *y_stride + y_offset] = 0xC0;
						line[pos++ *y_stride + y_offset] = 0xC0;
					}
					else {						// transition to low
						line[pos++ *y_stride + y_offset] = 0x94;
						line[pos++ *y_stride + y_offset] = 0x3C;
					}
				}
				else {	// low bit
					line[pos++ *y_stride + y_offset] = 0x10;
					line[pos++ *y_stride + y_offset] = 0x10;
					line[pos++ *y_stride + y_offset] = 0x10;
					line[pos++ *y_stride + y_offset] = 0x10;
					line[pos++ *y_stride + y_offset] = 0x10;
					if (next_bit) {				// transition to high
						line[pos++ *y_stride + y_offset] = 0x3C;
						line[pos++ *y_stride + y_offset] = 0x94;
					}
					else {						// keep low
						line[pos++ *y_stride + y_offset] = 0x10;
						line[pos++ *y_stride + y_offset] = 0x10;
					}
				}
			}
			else {
				// draw 8 pixels for odd bit
				if (bit) {
					line[pos++ *y_stride + y_offset] = 0xC0;
					line[pos++ *y_stride + y_offset] = 0xC0;
					line[pos++ *y_stride + y_offset] = 0xC0;
					line[pos++ *y_stride + y_offset] = 0xC0;
					line[pos++ *y_stride + y_offset] = 0xC0;
					if (next_bit) {				// keep high
						line[pos++ *y_stride + y_offset] = 0xC0;
						line[pos++ *y_stride + y_offset] = 0xC0;
						line[pos++ *y_stride + y_offset] = 0xC0;
					}
					else {						// transition to low
						line[pos++ *y_stride + y_offset] = 0xA6;
						line[pos++ *y_stride + y_offset] = 0x68;
						line[pos++ *y_stride + y_offset] = 0x2A;
					}
				}
				else {	// low bit
					line[pos++ *y_stride + y_offset] = 0x10;
					line[pos++ *y_stride + y_offset] = 0x10;
					line[pos++ *y_stride + y_offset] = 0x10;
					line[pos++ *y_stride + y_offset] = 0x10;
					line[pos++ *y_stride + y_offset] = 0x10;
					if (next_bit) {				// transition to high
						line[pos++ *y_stride + y_offset] = 0x2A;
						line[pos++ *y_stride + y_offset] = 0x68;
						line[pos++ *y_stride + y_offset] = 0xA6;
					}
					else {						// keep low
						line[pos++ *y_stride + y_offset] = 0x10;
						line[pos++ *y_stride + y_offset] = 0x10;
						line[pos++ *y_stride + y_offset] = 0x10;
					}
				}
			}
		}
	}
}

extern void write_vitc_one_field(unsigned hh, unsigned mm, unsigned ss, unsigned ff, int field_flag,
                                 unsigned char *line, int is_uyvy)
{
	int i;
	if (is_uyvy) {
	    // Set line to black UYVY
        for (i = 0; i < 720; i++) {
            line[i*2+0] = 0x80;
            line[i*2+1] = 0x10;
        }
    } else {
        // Set Y to black
        memset(line, 0x10, 720);
    }

	// data for one line is 9 bytes (8 bytes + 1 byte CRC)
	unsigned char data[9] = {0};

	int di = 0;
	data[di++] = ff % 10;
	data[di++] = ff / 10;
	data[di++] = ss % 10;
	data[di++] = ss / 10;
	data[di++] = mm % 10;
	data[di++] = mm / 10;
	data[di++] = hh % 10;
	data[di++] = (hh / 10) | (field_flag << 3);

	//
	// calculate CRC
	//

	// create 64bt value representing 8 * 8 bits
	unsigned long long alldata = 0;
	for (i = 0; i < 8; i++) {
		alldata |= ((unsigned long long)data[i]) << (i*8);
	}
	// convert array of 8 data bytes into array of 10 values including sync bits (2 bits)
	unsigned char data10[10];
	int data10i = 0;
	int output = 0;
	int out_i = 0;
	for (i = 0; i < 8*8; i++) {
		int bit = (alldata >> i) & 0x1;
		if (i % 8 == 0) {
			// add sync bits (1 0)
			output |= 1 << (out_i++);
			output |= 0 << (out_i++);
			if (out_i == 8) {
				data10[data10i++] = output;
				out_i = 0;
				output = 0;
			}
		}
		output |= bit << (out_i++);

		if (out_i == 8) {
			data10[data10i++] = output;
			out_i = 0;
			output = 0;
		}
	}

	// Now compute the CRC by XORing 10 x 8bits
	int CRC = 0;
	for (i = 0; i < 10; i++) {
		CRC ^= data10[i];
	}
	CRC = (((CRC & 3) ^ 1) << 6) | (CRC >> 2);

	data[di++] = CRC;

	draw_vitc_line(data, line, is_uyvy);
}

extern void write_vitc(unsigned hh, unsigned mm, unsigned ss, unsigned ff, unsigned char *line, int is_uyvy)
{
	write_vitc_one_field(hh, mm, ss, ff, 0, line, is_uyvy);
	if (is_uyvy)
	    write_vitc_one_field(hh, mm, ss, ff, 1, line + 720*2, 1);
	else
	    write_vitc_one_field(hh, mm, ss, ff, 1, line + 720, 0);
}

