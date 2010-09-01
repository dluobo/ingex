/* 
   libltcsmpte - en+decode linear SMPTE timecode

   Copyright (C) 2006 Robin Gareus <robin@gareus.org>
   Copyright (C) 2008-2009 Jan <jan@geheimwerk.de>

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ltcsmpte.h>
#include <decoder.h>
#include <encoder.h>
#include <smpte.h>

void addvalues(SMPTEEncoder *e, int n);

/* -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * Decoder 
 */

int SMPTEDecoderErrorReset(SMPTEDecoder* d) {
	d->firstFrame = d->lastFrame = d->errorCnt = 0;
	return 1;
}

int SMPTEDecoderErrors(SMPTEDecoder* decoder, int *errors) {
	*errors = decoder->errorCnt;
	return 1;
}

SMPTEDecoder* SMPTEDecoderCreate(int sampleRate, FrameRate *fps, int queueLen, int correctJitter) {
	SMPTEDecoder* d = (SMPTEDecoder*) calloc(1, sizeof(SMPTEDecoder));
	d->sampleRate = sampleRate;
	d->fps.num = fps->num;
	d->fps.den = fps->den;
	d->fps.flags = fps->flags;
	d->qLen = queueLen;
	d->queue = (SMPTEFrameExt*) calloc(d->qLen, sizeof(SMPTEFrameExt));
	d->biphaseToBinaryState = 1;
	d->soundToBiphasePeriod = d->sampleRate / FR_toint(&(d->fps)) / 80;
#ifdef SAMPLE_IS_UNSIGNED
	d->soundToBiphaseLimit = (d->soundToBiphasePeriod * 14) >> 4;
#else
	d->soundToBiphaseLimit = (d->soundToBiphasePeriod * 14) / 16;
#endif
	d->correctJitter = correctJitter;
	
	d->samplesToSeconds = 1 / ((timeu)d->sampleRate * sizeof(sample_t));

#ifdef DIAGNOSTICS_OUTPUT
	d->diagnosticsPos = 0;
	d->diagnosticsFile = fopen(DIAGNOSTICS_FILEPATH, "w");
	d->diagnosticsBiphaseFile = fopen(DIAGNOSTICS_FILEPATH "-biphase.txt", "w");
	d->diagnosticsBitsFile = fopen(DIAGNOSTICS_FILEPATH "-bits.txt", "w");
#endif
	
	return d;
}

int SMPTEFreeDecoder(SMPTEDecoder *d) {
	if (!d) return 1;
	if (d->queue) free(d->queue);
#ifdef DIAGNOSTICS_OUTPUT
	if (d->diagnosticsFile) fclose(d->diagnosticsFile);
	if (d->diagnosticsBiphaseFile) fclose(d->diagnosticsBiphaseFile);
	if (d->diagnosticsBitsFile) fclose(d->diagnosticsBitsFile);
#endif
	free(d);
	
	return 0;
}

int SMPTEDecoderFrameToMillisecs(SMPTEDecoder* d, SMPTEFrameExt* frame, int* timems) {
	*timems = frame_to_ms(&frame->base, &d->fps);
	if (d->correctJitter) {
		*timems += 1000*frame->delayed/d->sampleRate;
	}
	return 1;	
}

timeu SMPTEDecoderSamplesToSeconds(SMPTEDecoder* d, long int sampleCount) {
	return (timeu)sampleCount * d->samplesToSeconds;
}

#ifdef DIAGNOSTICS_OUTPUT
void SMPTEDecoderPrintDiagnosticsForAudacity(SMPTEDecoder* d, unsigned char* values, unsigned long* offsets, int size, long int posinfo, int bitsMode) {
	FILE *outFile;
	
	if (bitsMode) {
		outFile = d->diagnosticsBitsFile;
	}
	else {
		outFile = d->diagnosticsBiphaseFile;
	}
	
	static timeu start = 0.0;
	timeu end, posinfo_time;
	int i;
	posinfo_time = (timeu)posinfo * d->samplesToSeconds;
	
	for (i = 0; i < size; i++) {
		if (    (i == 0) 
			 || ( (i > 0) 
				  && (offsets[i] != offsets[i-1]) 
				  //&& (values[i] != values[i-1]) 
				)  
			) { 
			// This is not a dupe
			end = posinfo_time + ((timeu)offsets[i] * d->samplesToSeconds);
			//end = start + 
			fprintf(outFile, "" FPRNT_TIME TIME_DELIM FPRNT_TIME TIME_DELIM "", start, end); // start is approximate
			fprintf(outFile, "%c\n", (values[i] ? '1' : '0'));
			start = end;
		}
	}

}
#endif

int SMPTEDecoderWrite(SMPTEDecoder *d, sample_t *buf, int size, long int posinfo) {
	// The offset values below mark the last sample belonging to the respective value.
	unsigned char code[1024]; // decoded biphase values, only one bit per byte is used // TODO: make boolean 1 bit
	unsigned long offs[1024]; // positions in the sample buffer (buf) where each of the values in code was detected
	unsigned char bits[1024]; // bits decoded from code, only one bit per byte is used // TODO: make boolean 1 bit 
	unsigned long offb[1024]; // positions in the sample buffer (buf) where each of the values in bits was detected

	//TODO check if size <= 1024; dynamic alloc buffers in Decoder struct.

	size = audio_to_biphase(d, buf, code, offs, size);
	WRITE_DECODER_BIPHASE_DIAGNOSTICS(d, code, offs, size, posinfo);
	size = biphase_decode(d, code, offs, bits, offb, size);
	WRITE_DECODER_BITS_DIAGNOSTICS(d, bits, offb, size, posinfo);
	return ltc_decode(d, bits, offb, posinfo, size);
}

int SMPTEDecoderRead(SMPTEDecoder* decoder, SMPTEFrameExt* frame) {
	if (!frame) return 0;
	if (decoder->qReadPos != decoder->qWritePos) {
		memcpy(frame, &decoder->queue[decoder->qReadPos], sizeof(SMPTEFrameExt));
		decoder->qReadPos++;
		if (decoder->qReadPos == decoder->qLen)
			decoder->qReadPos = 0;
		return 1;
	}
	return 0;
}

int SMPTEDecoderReadLast(SMPTEDecoder* decoder, SMPTEFrameExt* frame) {
	int rv = 0;
	while (decoder->qReadPos != decoder->qWritePos) {
		memcpy(frame, &decoder->queue[decoder->qReadPos], sizeof(SMPTEFrameExt));
		decoder->qReadPos++;
		if (decoder->qReadPos == decoder->qLen)
			decoder->qReadPos = 0;
		rv = 1;
	}
	return(rv);
}

/* -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * Encoder 
 */

SMPTEEncoder* SMPTEEncoderCreate(int sampleRate, FrameRate *fps) {
	int baud = FR_toint(fps) * 80; // FIXME  non integer framerates!
	SMPTEEncoder* encoder = (SMPTEEncoder*)( calloc(1, sizeof(SMPTEEncoder)) );
	encoder->nsamples = 0;
	encoder->fps.num = fps->num;
	encoder->fps.den = fps->den;
	encoder->fps.flags = fps->flags;
	encoder->samplesPerClock = ((float)sampleRate / (float)baud);
	encoder->samplesPerHalveClock = encoder->samplesPerClock / 2.;
	encoder->remainder = 0.5;
	SMPTEFrameReset(&encoder->f);
	encoder->bufsize = 4096; // FIXME
	encoder->buf = calloc(encoder->bufsize, sizeof(sample_t));
	return encoder;
}

int SMPTEFreeEncoder(SMPTEEncoder *e) {
	if (!e) return 1;

	if (e->buf) free(e->buf);
	free(e);

	return (0);
}

int SMPTEEncode(SMPTEEncoder *e, int byteCnt) {
/* TODO
	assert(frame >= 0);
	assert(frame < e->fps);
	assert(byteCnti >= 0);
	assert(byteCnt < 10);
*/			
#ifdef __BIG_ENDIAN__
	// Byte swap hack for 16 bit LTC Sync Word (bytes 8 and 9)
	/*
	if (byteCnt == 8)
	else if (byteCnt == 9)
		byteCnt = 8;
	*/
	
	switch (byteCnt) {
		case 8:
			byteCnt = 9;
			break;
			
		case 9:
			byteCnt = 8;
			break;
			
		default:
			break;
			
	}
#endif
	
	unsigned char c = ((unsigned char*)&e->f)[byteCnt];
	unsigned char b = 1;

	do
	{	
		int n;
		if ((c & b) == 0) {
			n = (int)(e->samplesPerClock + e->remainder);
			e->remainder = (e->samplesPerClock + e->remainder) - (float)n;
			e->nsamples += n;
			e->state = !e->state;
			addvalues(e, n);
		}
		else {
			n = (int)(e->samplesPerHalveClock + e->remainder);
			e->remainder = (e->samplesPerHalveClock + e->remainder) - (float)n;
			e->nsamples += n;
			e->state = !e->state;
			addvalues(e, n);

			n = (int)(e->samplesPerHalveClock + e->remainder);
			e->remainder = (e->samplesPerHalveClock + e->remainder) - (float)n;
			e->nsamples += n;
			e->state = !e->state;
			addvalues(e, n);
		}
		b <<= 1;
	} while (b);
	
	return 0;
}

int SMPTESetNsamples(SMPTEEncoder *e, int val) {
	e->nsamples = val;
	return(e->nsamples);
}

int SMPTEGetNsamples(SMPTEEncoder *e) {
	return(e->nsamples);
}

int SMPTEGetTime(SMPTEEncoder *e, SMPTETime *t) {
	if (!t) return 0;
	return SMPTEFrameToTime(&e->f, t);
}

int SMPTESetTime(SMPTEEncoder *e, SMPTETime *t) {
	// assert bytes
	return SMPTETimeToFrame(t, &e->f);
}

int SMPTEEncIncrease(SMPTEEncoder *e) {
	return SMPTEFrameIncrease(&e->f, FR_toint(&e->fps));
}

size_t SMPTEGetBuffersize(SMPTEEncoder *e) {
	return(e->bufsize);
}

int SMPTEGetBuffer(SMPTEEncoder *e, sample_t *buf) {
	int len = e->offset;
	memcpy( buf, e->buf, (len * sizeof(sample_t)) );
	e->offset = 0;
	return(len);
}
