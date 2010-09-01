/** 
   @brief libltcsmpte - framerate convertion 
   @file framerate.h
   @author Robin Gareus <robin@gareus.org>

   Copyright (C) 2006, 2007, 2008 Robin Gareus <robin@gareus.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser Public License as published by
   the Free Software Foundation; either version 2.1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser Public License for more details.

   You should have received a copy of the GNU Lesser Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  

*/

#ifndef FRAMERATE_H
#define FRAMERATE_H

/**
 * Frame-Rate-Flags and -options:
 * multiple flags/options can be added (binary OR).
 */
typedef enum 
{
	FRF_NONE = 0,  
	FRF_DROP_FRAMES = 1, ///< use drop frame timecode
	FRF_UNUSED = 2,
	FRF_SAMPLERATE = 4, ///< has sample-rate info
	FRF_OFFA = 8, ///< has audio frame offset
	FRF_OFFV = 16, ///< has video frame offset 
	FRF_LAST = 32
} FRFlags;

typedef enum 
{
	SMPTE_FRAME = 0,
	SMPTE_SEC,
	SMPTE_MIN,
	SMPTE_HOUR,
	SMPTE_OVERFLOW,
	SMPTE_LAST
} FRSMPTE;

/**
 * Frame rate data structure
 */
typedef struct 
{
	int num; ///< numerator; rational framerate: numerator/denominator
	int den; ///< denominator; rational framerate: numerator/deominator
	int flags; ///< combination of FRFlags;
	int samplerate; ///< audio-samperate for conversion.
	long long int aoffset; ///< user-data: offset in audio-frames
	long int voffset; ///< user-data: offset in video-frames
} FrameRate;

/**
 * round framerate to the nearest integer 
 */
int FR_toint(FrameRate *fr);

/**
 * return (double) floating point representation of framerate
 */
double FR_todbl(FrameRate *fr);

/**
 * allocate memory for FrameRate struct and initialize
 * with given values.
 */
FrameRate *FR_create(int num, int den, int flags);

/**
 * free memory allocated for FrameRate data structure
 */
void FR_free(FrameRate *f);

/**
 * set audio sample rate (sps) for av,vf conversion
 */
void FR_setsamplerate(FrameRate *f, int samplerate);

/**
 * convert integer video-frames to audio frame.
 */ 
long long int FR_vf2af(FrameRate *f, long int vf);

/**
 * convert audio-frame to integer video-frames 
 * round down to the prev. video frame.
 */ 
long int FR_af2vfi(FrameRate *f, long long int af);

/**
 * convert audio-frame to fractional video-frames 
 */ 
double FR_af2vf(FrameRate *f, long long int af);

/**
 * private function - no need to call it when using the
 * FR-API for smpte/bcd conversion 
 *
 * Insert two frame numbers at the 
 * start of every minute except the tenth.
 * unit: video-frames!
 */
long int FR_insert_drop_frames(long int frames);

/**
 * private function - no need to call it when using the
 * FR-API for smpte/bcd conversion 
 *
 * Drop frame numbers (not frames) 00:00 and 00:01 at the
 * start of every minute except the tenth.
 * returns video frame number staring at zero for 29.97fps
 *
 * dropframes are not required or permitted when operating at
 * 24, 25, or 30 frames per second.
 */
long int FR_drop_frames(FrameRate *fr, int f, int s, int m, int h);

/**
 * converts video-frame number into binary coded decimal timecode
 * sets: bcd[SMPTE_FRAME] .. bcd[SMPTE_LAST] accoding to video-frame.
 *  - this supports only integer framerates! use FR_vf2smpte instead
 */
void FR_frame_to_bcd(FrameRate*f, long int *bcd, long int frame);

/**
 * converts video-frame into 13char SMPTE string.
 * smptestring needs to point to a allocated (char*) memory!
 *
 * it does handle drop-frame formats correctly.
 */
void FR_vf2smpte(FrameRate *fr, char *smptestring, long int frame);

/**
 * convert smpte into video-frame number
 * expects smpte in decimal representation as 
 * separate arguments:
 *
 * f: frame, s: second, m: minute, h: hour
 * overflow should be set to zero (days);
 * returns: video frame number, starting at zero
 */
long int FR_smpte2vf(FrameRate *fr, int f, int s, int m, int h, int overflow);

/**
 * wrapper around FR_smpte2vf
 * reads SMPTE as bcd array argument
 * and returns video frame number, starting at zero
 */
long int FR_bcd2vf(FrameRate *fr, int bcd[SMPTE_LAST]);

/**
 * directly change framerate flags 
 */
void FR_setflags(FrameRate *fr, int flags);

/**
 * set the framerate
 */
void FR_setratio(FrameRate *fr, int num, int den);

/**
 * convert double value into ratio and set framerate.
 * if mode == 1: autodetect drop-frame timecode
 *  (set flag|=FRF_DROP_FRAMES if fps==29.97 )
 */
void FR_setdbl(FrameRate *fr, double fps, int mode);

#endif
