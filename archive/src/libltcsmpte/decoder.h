/* 
   libltcsmpte - en+decode linear SMPTE timecode

   Copyright (C) 2006 Robin Gareus <robin@gareus.org>

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
#include <framerate.h>

struct SMPTEDecoder {
	int sampleRate;
	FrameRate fps;

	SMPTEFrameExt* queue;
	int qLen;
	int qReadPos;
	int qWritePos;

	int firstFrame;
	int lastFrame;
	int errorCnt;
	
	unsigned char biphaseToBinaryState;
	unsigned char biphaseToBinaryPrev;
	unsigned char soundToBiphaseState;
	int soundToBiphaseCnt;		// counts the samples in the current period, resets every period
	int soundToBiphasePeriod;	// length of a period (tracks speed variations)
	int soundToBiphaseLimit;	// specifies when a state-change is considered biphase-clock or 2*biphase-clock
	sample_t soundToBiphaseMin;
	sample_t soundToBiphaseMax;
	unsigned short decodeSyncWord;
	SMPTEFrame decodeFrame;
	int decodeBitCnt;
	long int decodeFrameStartPos;
	
	int correctJitter;
	
	timeu samplesToSeconds;
	
#ifdef TRACK_DECODE_FRAME_BIT_OFFSETS
	long int decodeFrameBitOffsets[LTC_FRAME_BIT_COUNT];
#endif
	
#ifdef DIAGNOSTICS_OUTPUT
	#define DIAGNOSTICS_DATA_SIZE 10
	diagnostics_t diagnosticsPos;
	diagnostics_t diagnosticsData[DIAGNOSTICS_DATA_SIZE];
	FILE* diagnosticsFile;
	FILE* diagnosticsBiphaseFile;
	FILE* diagnosticsBitsFile;
#endif
	
};


int audio_to_biphase(SMPTEDecoder *d, sample_t *sound, unsigned char *biphase, unsigned long *offset, int size);
int biphase_decode(SMPTEDecoder *d, unsigned char *biphase, unsigned long *offset_in, unsigned char *bits,  unsigned long *offset_out, int size);
int ltc_decode(SMPTEDecoder *d, unsigned char *bits, unsigned long *offset, long int posinfo, int size);

