/** 
   @brief libltcsmpte - en+decode linear SMPTE timecode
   @file ltcsmpte.h
   @author Robin Gareus <robin@gareus.org>

   Copyright (C) 2006 Robin Gareus <robin@gareus.org>
   Copyright (C) 2008-2009 Jan <jan@geheimwerk.de>

   inspired by SMPTE Decoder - Maarten de Boer <mdeboer@iua.upf.es>

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
#ifndef LTCSMPTE_H
#define LTCSMPTE_H 1

#ifndef DOXYGEN_IGNORE
// libltcsmpte version
#define LIBLTCSMPTE_VERSION "0.4.0"
#define LIBLTCSMPTE_VERSION_MAJOR  0
#define LIBLTCSMPTE_VERSION_MINOR  4
#define LIBLTCSMPTE_VERSION_MICRO  0

//interface revision number
//http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
#define LIBLTCSMPTE_CUR  1
#define LIBLTCSMPTE_REV  0
#define LIBLTCSMPTE_AGE  0
#endif

#include <sys/types.h>
#include <framerate.h>


typedef double timeu;
#define FPRNT_TIME "%lf"
#define TIME_DELIM	"\t"

#ifdef DIAGNOSTICS_OUTPUT
	#define WRITE_DECODER_BIPHASE_DIAGNOSTICS(A, B, C, D, E)	SMPTEDecoderPrintDiagnosticsForAudacity(A, B, C, D, E, 0)
	#define WRITE_DECODER_BITS_DIAGNOSTICS(A, B, C, D, E)		SMPTEDecoderPrintDiagnosticsForAudacity(A, B, C, D, E, 1)
#else
	#define WRITE_DECODER_BIPHASE_DIAGNOSTICS(A, B, C, D, E)	
	#define WRITE_DECODER_BITS_DIAGNOSTICS(A, B, C, D, E)		
#endif

#ifdef USE_FLOAT
	typedef float sample_t;
	typedef float curve_sample_t;
	//#define SAMPLE_AND_CURVE_ARE_DIFFERENT_TYPE
	#define SAMPLE_CENTER	0
	#define CURVE_MIN		-0.9765625
	#define CURVE_MAX		0.9765625
	//#define SAMPLE_IS_UNSIGNED
	//#define SAMPLE_IS_INTEGER
	typedef float diagnostics_t;
#else
	#ifdef USE16BIT
		typedef short sample_t;
		typedef short curve_sample_t;
		//#define SAMPLE_AND_CURVE_ARE_DIFFERENT_TYPE
		#define SAMPLE_CENTER	0
		#define CURVE_MIN		-32000
		#define CURVE_MAX		32000
		//#define SAMPLE_IS_UNSIGNED
		#define SAMPLE_IS_INTEGER
		typedef int diagnostics_t;
	#else
		#define USE8BIT
		typedef unsigned char sample_t;
		typedef short curve_sample_t;
		#define SAMPLE_AND_CURVE_ARE_DIFFERENT_TYPE
		#define SAMPLE_CENTER	128
		#define CURVE_MIN		-32000
		#define CURVE_MAX		32000
		#define SAMPLE_IS_UNSIGNED
		#define SAMPLE_IS_INTEGER
		typedef int diagnostics_t;
	#endif
#endif


/**
 * Raw 80 bit SMPTE frame 
 */
#define LTC_FRAME_BIT_COUNT	80

#ifdef __BIG_ENDIAN__
// Big Endian version, bytes are "upside down"
typedef struct SMPTEFrame {
	unsigned int user1:4;
	unsigned int frameUnits:4;
	
	unsigned int user2:4;
	unsigned int colFrm:1;
	unsigned int dfbit:1;
	unsigned int frameTens:2;
	
	unsigned int user3:4;
	unsigned int secsUnits:4;
	
	unsigned int user4:4;
	unsigned int biphaseMarkPhaseCorrection:1;
	unsigned int secsTens:3;
	
	unsigned int user5:4;
	unsigned int minsUnits:4;
	
	unsigned int user6:4;
	unsigned int binaryGroupFlagBit1:1;
	unsigned int minsTens:3;
	
	unsigned int user7:4;
	unsigned int hoursUnits:4;
	
	unsigned int user8:4;
	unsigned int binaryGroupFlagBit2:1;
	unsigned int reserved:1;
	unsigned int hoursTens:2;
	
	unsigned int syncWord:16;
} SMPTEFrame;

#else
// Little Endian version (default)
typedef struct SMPTEFrame {
	unsigned int frameUnits:4;
	unsigned int user1:4;
	
	unsigned int frameTens:2;
	unsigned int dfbit:1;
	unsigned int colFrm:1;
	unsigned int user2:4;
	
	unsigned int secsUnits:4;
	unsigned int user3:4;
	
	unsigned int secsTens:3;
	unsigned int biphaseMarkPhaseCorrection:1;
	unsigned int user4:4;
	
	unsigned int minsUnits:4;
	unsigned int user5:4;
	
	unsigned int minsTens:3;
	unsigned int binaryGroupFlagBit1:1;
	unsigned int user6:4;
	
	unsigned int hoursUnits:4;
	unsigned int user7:4;
	
	unsigned int hoursTens:2;
	unsigned int reserved:1;
	unsigned int binaryGroupFlagBit2:1;
	unsigned int user8:4;
	
	unsigned int syncWord:16;
} SMPTEFrame;

#endif

/**
 * Human readable time representation
 */
typedef struct SMPTETime {
// these are only set when compiled with ENABLE_DATE
	char timezone[6];
	unsigned char years;
	unsigned char months;
	unsigned char days;
// 
	unsigned char hours;
	unsigned char mins;
	unsigned char secs;
	unsigned char frame;
} SMPTETime;




/**
 * Extended SMPTE frame 
 * The maximum error for startpos is 1/80 of a frame. Usually it is 0;
 */
typedef struct SMPTEFrameExt {
	SMPTEFrame base; ///< the SMPTE decoded from the audio
	int delayed; ///< detected jitter in LTC-framerate/80 unit(s) - bit count in LTC frame.
	long int startpos; ///< the approximate sample in the stream corresponding to the start of the LTC SMPTE frame. 
	long int endpos; ///< the sample in the stream corresponding to the end of the LTC SMPTE frame.
} SMPTEFrameExt;


/**
 * opaque structure.
 * see: SMPTEDecoderCreate, SMPTEFreeDecoder
 */
typedef struct SMPTEDecoder SMPTEDecoder;

/**
 * opaque structure
 * see: SMPTEEncoderCreate, SMPTEFreeEncoder
 */
typedef struct SMPTEEncoder SMPTEEncoder;


/**
 * convert binary SMPTEFrame into SMPTETime struct 
 */
int SMPTEFrameToTime(SMPTEFrame* frame, SMPTETime* stime);

/**
 * convert SMPTETime struct into it's binary SMPTE representation.
 */
int SMPTETimeToFrame(SMPTETime* stime, SMPTEFrame* frame);

/**
 * set all values of a SMPTE FRAME to zero but the sync-word (0x3FFD) at the end.
 */
int SMPTEFrameReset(SMPTEFrame* frame);

/**
 * increments the SMPTE by one SMPTE-Frame (1/10 of framerate)
 * NOTE: this does not yet handle drop-frame rates correctly!
 */
int SMPTEFrameIncrease(SMPTEFrame *frame, int framesPerSec);


/**
 * Create a new decoder. Pass sample rate, number of smpte frames per 
 * seconds, and the size of the internal queue where decoded frames are
 * stored. Set correctJitter flag, to correct jitter resulting from
 * audio fragment size when decoding from a realtime audiostream. This
 * works only correctly when buffers of exactly fragment size are passed
 * to SMPTEDecoderWrite. (as discussed on LAD [msgID])
 */
SMPTEDecoder * SMPTEDecoderCreate(int sampleRate, FrameRate *fps, int queueSize, int correctJitter);


/**
 * release memory of Decoder structure.
 */
int SMPTEFreeDecoder(SMPTEDecoder *d);

/**
 * Reset the decoder error tracking internals
 */
int SMPTEDecoderErrorReset(SMPTEDecoder *decoder);

/**
 * Feed the SMPTE decoder with new samples. 
 *
 * parse raw audio for LTC timestamps. If found, store them in the 
 * Decoder queue (see SMPTEDecoderRead)
 * always returns 1 ;-)
 * d: the decoder 
 * buf: pointer to sample_t (defaults to unsigned 8 bit) mono audio data
 * size: number of bytes to parse
 * posinfo: (optional) byte offset in stream to set LTC location offset
 */
int SMPTEDecoderWrite(SMPTEDecoder *decoder, sample_t *buf, int size, long int posinfo);

/**
 * All decoded SMPTE frames are placed in a queue. This function gets 
 * a frame from the queue, and stores it in SMPTEFrameExt* frame.
 * Returns 1 on success, 0 when no frames where on the queue, and
 * and errorcode otherwise.
 */
int SMPTEDecoderRead(SMPTEDecoder *decoder, SMPTEFrameExt *frame);

/** 
 * flush unread LTCs in queue and return the last timestamp.
 * (note that the last timestamp may not be the latest if the queue has
 * overflown!)
 */
int SMPTEDecoderReadLast(SMPTEDecoder* decoder, SMPTEFrameExt* frame);

/** 
 * Convert the frame to time in milliseconds. This uses the 
 * position of the end of the frame related to the sample-time
 * to compensate for jitter.
 */
int SMPTEDecoderFrameToMillisecs(SMPTEDecoder* decoder, SMPTEFrameExt* frame, int *timems);

/** 
 * Convert the index or position of a sample to 
 * its position in time (seconds) relative to the first sample. 
 */
timeu SMPTEDecoderSamplesToSeconds(SMPTEDecoder* d, long int sampleCount);

/**
 * get accumulated error count since last reset.
 */
int SMPTEDecoderErrors(SMPTEDecoder *decoder, int *errors);

/** 
 * Allocate and initialize LTC encoder
 * sampleRate: audio sample rate (eg. 48000)
 * fps: framerate
 */
SMPTEEncoder * SMPTEEncoderCreate(int sampleRate, FrameRate *fps);

/** 
 * release encoder data structure
 */
int SMPTEFreeEncoder(SMPTEEncoder *e);

/**
 * set internal Audio-sample counter. 
 * SMPTEEncode() increments this counter when it encodes samples.
 */
int SMPTESetNsamples(SMPTEEncoder *e, int val);

/**
 * returns the current values of the Audio-sample counter.
 * ie. the number of encoded samples.
 */
int SMPTEGetNsamples(SMPTEEncoder *e);

/**
 * moves the SMPTE to the next frame.
 * it uses SMPTEFrameIncrease() which still lacks drop-frame TC support.
 */
int SMPTEEncIncrease(SMPTEEncoder *e);

/**
 * sets the current encoder SMPTE frame to SMPTETime.
 */
int SMPTESetTime(SMPTEEncoder *e, SMPTETime *t);

/** 
 * set t from current Encoder timecode
 */
int SMPTEGetTime(SMPTEEncoder *e, SMPTETime *t);

/**
 * returns and flushes the accumulated Encoded Audio.
 * retuns the number of bytes written to the memory area
 * pointed pointed by buf.
 *
 * there is no overflow check perfomed! use SMPTEGetBuffersize() .
 */
int SMPTEGetBuffer(SMPTEEncoder *e, sample_t *buf);

/**
 * returns the size of the accumulated encoded Audio in bytes.
 */
size_t SMPTEGetBuffersize(SMPTEEncoder *e);

/**
 * Generate LTC audio for byte "byteCnt" of the current frame into the buffer of Encoder e.
 * LTC has 10 bytes per frame: 0 <= bytecnt < 10 
 * use SMPTESetTime(..) to set the current frame before Encoding.
 * see tests/encoder.c for an example.
 */
int SMPTEEncode(SMPTEEncoder *e, int byteCnt);

#endif
