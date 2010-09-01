/* 
   libltcsmpte - en+decode linear SMPTE timecode

   Copyright (C) 2006, 2008 Robin Gareus <robin@gareus.org>

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
#include <smpte.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef ENABLE_DATE
/**
 * SMPTE Timezones 
 */
struct SMPTETimeZonesStruct {
	unsigned char code; //actually 6 bit!
	char timezone[6];
};

/**
 * SMPTE Timezone codes as per http://www.barney-wol.net/time/timecode.html
 */
struct SMPTETimeZonesStruct SMPTETimeZones[] =
{
	/*	code,	timezone (UTC+)		//Standard time					//Daylight saving	*/
	{	0x00,	"+0000"				/* Greenwich */					/* - */				},
	{	0x00,	"-0000"				/* Greenwich */					/* - */				},
	{	0x01,	"-0100"				/* Azores */					/* - */				},
	{	0x02,	"-0200"				/* Mid-Atlantic */				/* - */				},
	{	0x03,	"-0300"				/* Buenos Aires */				/* Halifax */		},
	{	0x04,	"-0400"				/* Halifax */					/* New York */		},
	{	0x05,	"-0500"				/* New York */					/* Chicago */		},
	{	0x06,	"-0600"				/* Chicago Denver */			/* - */				},
	{	0x07,	"-0700"				/* Denver */					/* Los Angeles */	},
	{	0x08,	"-0800"				/* Los Angeles */				/* - */				},
	{	0x09,	"-0900"				/* Alaska */					/* - */				},
	{	0x10,	"-1000"				/* Hawaii */					/* - */				},
	{	0x11,	"-1100"				/* Midway Island */				/* - */				},
	{	0x12,	"-1200"				/* Kwaialein */					/* - */				},
	{	0x13,	"+1300"				/* - */							/* New Zealand */	},
	{	0x14,	"+1200"				/* New Zealand */				/* - */				},
	{	0x15,	"+1100"				/* Solomon Islands */			/* - */				},
	{	0x16,	"+1000"				/* Guam */						/* - */				},
	{	0x17,	"+0900"				/* Tokyo */						/* - */				},
	{	0x18,	"+0800"				/* Beijing */					/* - */				},
	{	0x19,	"+0700"				/* Bangkok */					/* - */				},
	{	0x20,	"+0600"				/* Dhaka */						/* - */				},
	{	0x21,	"+0500"				/* Islamabad */					/* - */				},
	{	0x22,	"+0400"				/* Abu Dhabi */					/* - */				},
	{	0x23,	"+0300"				/* Moscow */					/* - */				},
	{	0x24,	"+0200"				/* Eastern Europe */			/* - */				},
	{	0x25,	"+0100"				/* Central Europe */			/* - */				},
/*	{	0x26,	"Undefined"			Reserved; do not use								},*/
/*	{	0x27,	"Undefined"			Reserved; do not use								},*/
	{	0x28,	"TP-03"				/* Time precision class 3 */	/* - */				},
	{	0x29,	"TP-02"				/* Time precision class 2 */	/* - */				},
	{	0x30,	"TP-01"				/* Time precision class 1 */	/* - */				},
	{	0x31,	"TP-00"				/* Time precision class 0 */	/* - */				},
	{	0x0A,	"+0030"				/* - */							/* - */				},
	{	0x0B,	"-0130"				/* - */							/* - */				},
	{	0x0C,	"-0230"				/* - */							/* Newfoundland */	},
	{	0x0D,	"-0330"				/* Newfoundland */				/* - */				},
	{	0x0E,	"-0430"				/* - */							/* - */				},
	{	0x0F,	"-0530"				/* - */							/* - */				},
	{	0x1A,	"-0630"				/* - */							/* - */				},
	{	0x1B,	"-0730"				/* - */							/* - */				},
	{	0x1C,	"-0830"				/* - */							/* - */				},
	{	0x1D,	"-0930"				/* Marquesa Islands */			/* - */				},
	{	0x1E,	"-1030"				/* - */							/* - */				},
	{	0x1F,	"-1130"				/* - */							/* - */				},
	{	0x2A,	"+1130"				/* Norfolk Island */			/* - */				},
	{	0x2B,	"+1030"				/* Lord Howe Is. */				/* - */				},
	{	0x2C,	"+0930"				/* Darwin */					/* - */				},
	{	0x2D,	"+0830"				/* - */							/* - */				},
	{	0x2E,	"+0730"				/* - */							/* - */				},
	{	0x2F,	"+0630"				/* Rangoon */					/* - */				},
	{	0x3A,	"+0530"				/* Bombay */					/* - */				},
	{	0x3B,	"+0430"				/* Kabul */						/* - */				},
	{	0x3C,	"+0330"				/* Tehran */					/* - */				},
	{	0x3D,	"+0230"				/* - */							/* - */				},
	{	0x3E,	"+0130"				/* - */							/* - */				},
	{	0x3F,	"+0030"				/* - */							/* - */				},
	{	0x32,	"+1245"				/* Chatham Island */			/* - */				},
/*	{	0x33,	"Undefined"			Reserved; do not use								},*/
/*	{	0x34,	"Undefined"			Reserved; do not use								},*/
/*	{	0x35,	"Undefined"			Reserved; do not use								},*/
/*	{	0x36,	"Undefined"			Reserved; do not use								},*/
/*	{	0x37,	"Undefined"			Reserved; do not use								},*/
	{	0x38,	"+XXXX"				/* User defined time offset */	/* - */				},
/*	{	0x39,	"Undefined"			Unknown							Unknown				},*/
/*	{	0x39,	"Undefined"			Unknown							Unknown				},*/
	
	{	0xFF,	""					/* The End */										}
};

int SMPTESetTimeZoneString(SMPTEFrame* frame, SMPTETime* stime) {
	int i = 0, j = 0;

	unsigned char code;
	code = frame->user7;
	code += frame->user8 << 4;

	char timezone[6] = "+0000";
	
	// Find timezone string for code
	// Primitive search
	for (i = 0 ; SMPTETimeZones[i].code != 0xFF ; i++) {
		if ( SMPTETimeZones[i].code == code ) {
			// FIXME: There has to be a better way to passing the value of a char constant to a char array
			for (j = 0 ; j < sizeof(timezone) ; j++) timezone[j] = SMPTETimeZones[i].timezone[j];
			break;
		}
	}
	
	// FIXME: There has to be a better way
	for (i = 0 ; i < sizeof(timezone) ; i++) stime->timezone[i] = timezone[i];

	return 1;
}

int SMPTESetTimeZoneCode(SMPTETime* stime, SMPTEFrame* frame) {
	int i = 0;
	unsigned char code = 0x00;
	
	// Find code for timezone string
	// Primitive search
	for (i = 0 ; SMPTETimeZones[i].code != 0xFF ; i++) {
		if ( (strcmp(SMPTETimeZones[i].timezone, stime->timezone)) == 0 ) {
			// FIXME: There has to be a better way to assing the value of a char constant to a char array
			code = SMPTETimeZones[i].code;
			break;
		}
	}
	
	frame->user7 = code & 0x0F;
	frame->user8 = (code & 0xF0) >> 4;

	return 1;
}
#endif

int SMPTEFrameToTime(SMPTEFrame* frame, SMPTETime* stime) {
#ifdef ENABLE_DATE
	// FIXME: what role does the MJD flag play?
	SMPTESetTimeZoneString(frame, stime);
	
	stime->years = frame->user5 + frame->user6*10;
	stime->months = frame->user3 + frame->user4*10;
	stime->days = frame->user1 + frame->user2*10;
#endif
	stime->hours = frame->hoursUnits + frame->hoursTens*10;
	stime->mins = frame->minsUnits + frame->minsTens*10;
	stime->secs = frame->secsUnits + frame->secsTens*10;
	stime->frame = frame->frameUnits + frame->frameTens*10;
	return 1;
}

int SMPTETimeToFrame(SMPTETime* stime, SMPTEFrame* frame) {
#ifdef ENABLE_DATE
	// FIXME: what role does the MJD flag play?
	SMPTESetTimeZoneCode(stime, frame);
	frame->user6 = stime->years/10;
	frame->user5 = stime->years - frame->user6*10;
	frame->user4 = stime->months/10;
	frame->user3 = stime->months - frame->user4*10;
	frame->user2 = stime->days/10;
	frame->user1 = stime->days - frame->user2*10;
#endif
	frame->hoursTens = stime->hours/10;
	frame->hoursUnits = stime->hours - frame->hoursTens*10;
	frame->minsTens = stime->mins/10;
	frame->minsUnits = stime->mins - frame->minsTens*10;
	frame->secsTens = stime->secs/10;
	frame->secsUnits = stime->secs - frame->secsTens*10;
	frame->frameTens = stime->frame/10;
	frame->frameUnits = stime->frame - frame->frameTens*10;
	return 1;
}

int SMPTEFrameReset(SMPTEFrame* frame) {
	memset(frame,0,sizeof(SMPTEFrame));
	/* syncword = 0x3FFD; mirrored bit order: BFFC */
	frame->syncWord = 0xBFFC;
	return 1;
}

int SMPTEFrameIncrease(SMPTEFrame* frame,int framesPerSec) {
	frame->frameUnits++;
	if (frame->frameUnits == 10)
	{
		frame->frameUnits = 0;
		frame->frameTens++;
	}
	if (framesPerSec == frame->frameUnits+frame->frameTens*10)
	{
		frame->frameUnits = 0;
		frame->frameTens = 0;
		frame->secsUnits++;
		if (frame->secsUnits == 10)
		{
			frame->secsUnits = 0;
			frame->secsTens++;
			if (frame->secsTens == 6)
			{
				frame->secsTens = 0;
				frame->minsUnits++;
				if (frame->minsUnits == 10)
				{
					frame->minsUnits = 0;
					frame->minsTens++;
					if (frame->minsTens == 6)
					{
						frame->minsTens = 0;
						frame->hoursUnits++;
						if (frame->hoursUnits == 10)
						{
							frame->hoursUnits = 0;
							frame->hoursTens++;
						}
					}
				}
			}
		}
	}
	
	return 1;
}


/* private fn */
/** convert SMPTEFrame into milliseconds using fps as framerate.
 */
int frame_to_ms(SMPTEFrame *f, FrameRate *fps) {
	return( 60*60*1000*(f->hoursUnits+f->hoursTens*10) +
		60*1000*(f->minsUnits+f->minsTens*10) +
		1000*(f->secsUnits+f->secsTens*10) +
		1000*(f->frameUnits+f->frameTens*10)/ FR_toint(fps));
}

