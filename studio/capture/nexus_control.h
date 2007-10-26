/*
 * $Id: nexus_control.h,v 1.2 2007/10/26 13:52:35 john_f Exp $
 *
 * Shared memory interface between SDI capture threads and reader threads.
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

#ifndef NEXUS_CONTROL_H
#define NEXUS_CONTROL_H

#ifndef _MSC_VER
#include <pthread.h>
#include <stdio.h>
#define PTHREAD_MUTEX_LOCK(x) if (pthread_mutex_lock( x ) != 0 ) fprintf(stderr, "pthread_mutex_lock failed\n");
#define PTHREAD_MUTEX_UNLOCK(x) if (pthread_mutex_unlock( x ) != 0 ) fprintf(stderr, "pthread_mutex_unlock failed\n");
#endif

typedef enum {
	FormatNone,			// Indicates off or disabled
	FormatUYVY,			// 4:2:2 buffer suitable for uncompressed capture
	FormatYUV422,		// 4:2:2 buffer suitable for JPEG encoding
	FormatYUV422DV,		// 4:2:2 buffer suitable for DV50 encoding (picture shift down 1 line)
	FormatYUV420,		// 4:2:0 buffer suitable for MPEG encoding
	FormatYUV420DV		// 4:2:0 buffer suitable for DV25 encoding (picture shift down 1 line)
} CaptureFormat;

// Each channel's ring buffer is described by the NexusBufCtl structure
typedef struct {
#ifndef _MSC_VER
	pthread_mutex_t		m_lastframe;	// mutex for lastframe counter
	// TODO: use a condition variable to signal when new frame arrives
#endif
	int		lastframe;		// last frame number stored and now available
							// use lastframe % ringlen to get buffer index
	int		hwdrop;			// frame-drops recorded by sv interface
} NexusBufCtl;

#define MAX_CHANNELS 8

// Each element in a ring buffer is structured to facilitate the
// DMA transfer from the capture card, and is structured as follows:
//	video (4:2:2 primary)	- offset 0, size given by width*height*2
//	audio channels 1,2		- offset given by audio12_offset
//	audio channels 3,4		- offset given by audio34_offset
//	(padding)
//	signal status			- offset given by signal_ok_offset
//	timecodes				- offsets given by tc_offset, ltc_offset
//	video (4:2:0 secondary)	- offset given by sec_video_offset, size width*height*3/2

// NexusControl is the top-level control struture describing how many channels
// are in use and what their parameters are
typedef struct {
	NexusBufCtl		card[MAX_CHANNELS];	// array of buffer control information
										// for all 8 possible channels

	int				cards;				// number of channels and therefore ring buffers in use
	int				ringlen;			// number of elements in ring buffer
	int				elementsize;		// an element is video + audio + timecode

	int				width;				// width of video
	int				height;				// height of video
	int				frame_rate_numer;	// frame rate numerator e.g. 25
	int				frame_rate_denom;	// frame rate denominator e.g 1
	CaptureFormat	pri_video_format;	// primary video format: usually UYVY, YUV422, ...
	CaptureFormat	sec_video_format;	// secondary video format: usually YUV420, ...

	int				audio12_offset;		// offset to start of audio ch 1,2 samples
	int				audio34_offset;		// offset to start of audio ch 3,4 samples
	int				audio_size;			// size in bytes of all audio data (4 chans)
										// including internal padding for DMA transfer
	int				signal_ok_offset;	// offset to flag for good input status
	int				ltc_offset;			// offset to start of LTC timecode data (int)
	int				tc_offset;			// offset to start of VITC timecode data (int)
	int				sec_video_offset;	// offset to secondary video buffer
} NexusControl;

#endif // NEXUS_CONTROL_H
