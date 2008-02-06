/*
 * $Id: logF.h,v 1.2 2008/02/06 16:58:50 john_f Exp $
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

#ifndef UTILS_H
#define UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>		// for va_list

// Utilities
extern int openLogFileWithDate(const char *logfile);
extern int reopenLogFileWithDate(const char *logfile);
extern int openLogFile(const char *logfile);
extern int reopenLogFile(const char *logfile);
extern void logF(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern void vlogTF(const char *fmt, va_list ap) __attribute__ ((format (printf, 1, 0)));
extern void logTF(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern void logerrF(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern void logFF(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
extern void logFFi(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

extern char *framesToStr(int tc, char *s);
extern int dvs_tc_to_int(int tc);
extern int tc_to_int(unsigned hh, unsigned mm, unsigned ss, unsigned ff);
extern int readVITC(const unsigned char *line, unsigned *hh, unsigned *mm, unsigned *ss, unsigned *ff);
extern int black_or_grey_line(const unsigned char *line, int width);

#ifdef __cplusplus
}
#endif

#endif /* UTILS_H */
