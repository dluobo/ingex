// ClockTime.cpp
#include "ClockTime.h"

#include <time.h>
#include <sys/timeb.h>
#include <string>

// static
const char * ClockTime::Timecode()
{
	static char buf[32];

	_timeb tb;
	_ftime(&tb);
	tm * ptm = localtime(&tb.time);
	sprintf(buf,"%02d:%02d:%02d:%02d",
		ptm->tm_hour,
		ptm->tm_min,
		ptm->tm_sec,
		tb.millitm / 40);

	return buf;
}

// static
const char * ClockTime::Date()
{
	static char buf[16];

	_timeb tb;
	_ftime(&tb);
	tm * ptm = localtime(&tb.time);
	sprintf(buf,"%02d.%02d.%04d",
		ptm->tm_mday,
		ptm->tm_mon + 1,
		ptm->tm_year + 1900
		);
	return buf;
}


