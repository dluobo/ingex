/*
 * $Id: vitc_reader_sink_source.h,v 1.1 2010/06/18 09:44:51 philipn Exp $
 *
 * Copyright (C) 2010 British Broadcasting Corporation, All Rights Reserved
 *
 * Author: Philip de Nier
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __VITC_READER_SINK_SOURCE_H__
#define __VITC_READER_SINK_SOURCE_H__


#include "media_sink.h"
#include "media_source.h"



typedef struct VITCReaderSinkSource VITCReaderSinkSource;



int vss_create_vitc_reader(unsigned int *vitc_lines, int num_vitc_lines, VITCReaderSinkSource **sonk);
void vss_set_target_sink(VITCReaderSinkSource *sonk, MediaSink *target_sink);

MediaSink* vss_get_media_sink(VITCReaderSinkSource *sonk);
MediaSource* vss_get_media_source(VITCReaderSinkSource *sonk);



#endif
