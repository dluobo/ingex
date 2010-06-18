/*
 * $Id: video_VITC.h,v 1.4 2010/06/18 08:53:42 john_f Exp $
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

#ifndef VIDEO_VITC_H
#define VIDEO_VITC_H

#ifdef __cplusplus
extern "C" {
#endif

extern char *framesToStr(int tc, char *s);
extern int dvs_tc_to_int(int tc);
extern int tc_to_int(unsigned hh, unsigned mm, unsigned ss, unsigned ff);

extern int readVITC(const unsigned char *line, int is_uyvy, unsigned *hh, unsigned *mm, unsigned *ss, unsigned *ff);
extern int black_or_grey_line(const unsigned char *uyvy_line, int width);

extern void draw_vitc_line(unsigned char value[9], unsigned char *line, int is_uyvy);
extern void write_vitc_one_field(unsigned hh, unsigned mm, unsigned ss, unsigned ff, int field_flag,
                                 unsigned char *line, int is_uyvy);
extern void write_vitc(unsigned hh, unsigned mm, unsigned ss, unsigned ff, unsigned char *line, int is_uyvy);

#ifdef __cplusplus
}
#endif

#endif /* VIDEO_VITC_H */
