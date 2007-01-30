/*
 * $Id: utils.c,v 1.2 2007/01/30 12:06:37 john_f Exp $
 *
 * Logging and debugging utility functions.
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "utils.h"

// Utility functions
//

static FILE *pLogFile = NULL;
static char log_filename[FILENAME_MAX] = "recorder.log";
static int log_overwrite = 1;

extern int openLogFileWithDate(const char *logfile)
{
	time_t now = time(NULL);
	struct tm l;
	localtime_r(&now, &l);

	sprintf(log_filename, "%s_%02d%02d%02d_%02d%02d%02d.log",
					logfile,
					l.tm_year % 100, l.tm_mon + 1, l.tm_mday,
					l.tm_hour, l.tm_min, l.tm_sec);

	// Open the log file once for lifetime of program
	if (! pLogFile)
		pLogFile = fopen(log_filename, log_overwrite ? "w" : "a");

	return pLogFile != NULL;
}

extern int openLogFile(const char *logfile)
{
	if (logfile != NULL) {
		strcpy(log_filename, logfile);
	}

	// Backup old log file once (convenient for debugging)
	if (log_overwrite)
	{
		char *p, backup_name[FILENAME_MAX], suffix[FILENAME_MAX];
		strcpy(backup_name, log_filename);
		p = strchr(backup_name, '.');
		if (p)
		{
			strcpy(suffix, p);
			strcpy(p, "_old");
			strcat(p, suffix);
		}
		else
			strcat(backup_name, "_old");

		rename(log_filename, backup_name);
	}

	// Open the log file once for lifetime of program
	if (! pLogFile)
		pLogFile = fopen(log_filename, log_overwrite ? "w" : "a");

	return pLogFile != NULL;
}

// printf style logging to stdout and logfile

extern void logF(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);

	vprintf(fmt, ap);

	int logfile_open = 1;
	if (! pLogFile)
		logfile_open = openLogFile(NULL);

	if (logfile_open)
		vfprintf(pLogFile, fmt, ap);

	va_end(ap);
}

// logging with time stamp and flushing to stdout and file
extern void logTF(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);

	struct timeval tv;
	time_t now = time(NULL);
	gettimeofday(&tv, NULL);
	struct tm l;
	localtime_r(&now, &l);

	printf("%02d:%02d:%02d.%06ld ", l.tm_hour, l.tm_min, l.tm_sec, tv.tv_usec);
	fflush(stdout);
	vprintf(fmt, ap);

	int logfile_open = 1;
	if (! pLogFile)
		logfile_open = openLogFile(NULL);

	if (logfile_open) {
		fprintf(pLogFile, "%02d:%02d:%02d.%06ld ", l.tm_hour, l.tm_min, l.tm_sec, tv.tv_usec);
		vfprintf(pLogFile, fmt, ap);
		fflush(pLogFile);
	}

	va_end(ap);
}

// logging with time stamp and flushing to file only
extern void logFF(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);

	struct timeval tv;
	time_t now = time(NULL);
	gettimeofday(&tv, NULL);
	struct tm l;
	localtime_r(&now, &l);

	int logfile_open = 1;
	if (! pLogFile)
		logfile_open = openLogFile(NULL);

	if (logfile_open) {
		fprintf(pLogFile, "%02d:%02d:%02d.%06ld ", l.tm_hour, l.tm_min, l.tm_sec, tv.tv_usec);
		vfprintf(pLogFile, fmt, ap);
		fflush(pLogFile);
	}

	va_end(ap);
}

// logging flushing to file only
extern void logFFi(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);

	int logfile_open = 1;
	if (! pLogFile)
		logfile_open = openLogFile(NULL);

	if (logfile_open) {
		vfprintf(pLogFile, fmt, ap);
		fflush(pLogFile);
	}

	va_end(ap);
}

// printf style logging to stderr and logfile
extern void logerrF(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);

	vfprintf(stderr, fmt, ap);

	int logfile_open = 1;
	if (! pLogFile)
		logfile_open = openLogFile(NULL);

	if (logfile_open)
		vfprintf(pLogFile, fmt, ap);

	va_end(ap);
}

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
	int hours = dvs_to_int[(tc & 0x7f000000) >> 24];
	int mins  = dvs_to_int[(tc & 0x007f0000) >> 16];
	int secs  = dvs_to_int[(tc & 0x00007f00) >> 8];
	int frames= dvs_to_int[(tc & 0x0000007f)];

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

// Input must be one line of UYVY video (720 pixels, 720*2 bytes)
extern unsigned VBIbit(const unsigned char *line, int bit)
{
	// Bits are 7.5 luma samples wide (see SMPTE-266)
	// Experiment showed bit 0 is centered on luma sample
	// 28 for DVS
	// 30 for Rubidium
	// 27 for D3 at left of picture
	int bit_pos = (int)(bit * 7.5 + 0.5) + 28;

	// use the bit_pos index into the UYVY packing
	unsigned char Y = line[bit_pos * 2 + 1];

	// If the luma value is greater than 0x7F then it is
	// likely to be a "1" bit value.  SMPTE-266M says the
	// state of 1 is indicated by 0xC0.
	return (Y > 0x7F);
}

extern unsigned readVBIbits(const unsigned char *line, int bit, int width)
{
	unsigned value = 0;
	int i;
	for (i=0; i < width; i++) {
		value |= (VBIbit(line, bit + i) << i);
	}
	return value;
}

extern int readVITC(const unsigned char *line,
					unsigned *hh, unsigned *mm, unsigned *ss, unsigned *ff)
{
	unsigned CRC = 0;
	int i;
	for (i = 0; i < 10; i++)
		CRC ^= readVBIbits(line, i*8, 8);
	CRC = (((CRC & 3) ^ 1) << 6) | (CRC >> 2);
	//unsigned film_CRC = CRC ^ 0xFF;
	//unsigned prod_CRC = CRC ^ 0xF0;
	unsigned crc_read = readVBIbits(line, 82, 8);
	//printf("crc_read=%d CRC=%u fCRC=%u pCRC=%u\n", crc_read,CRC,film_CRC,prod_CRC);
	if (crc_read != CRC)
		return 0;

	// Note that tens of frames are two bits - flags bits are ignored
	*ff = readVBIbits(line, 2, 4) + readVBIbits(line, 12, 2) * 10;
	*ss = readVBIbits(line, 22, 4) + readVBIbits(line, 32, 3) * 10;
	*mm = readVBIbits(line, 42, 4) + readVBIbits(line, 52, 3) * 10;
	*hh = readVBIbits(line, 62, 4) + readVBIbits(line, 72, 2) * 10;
	return 1;
}
