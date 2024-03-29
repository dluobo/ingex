/*
 * $Id: dual_sink.h,v 1.11 2010/10/26 18:28:23 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
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

#ifndef __DUAL_SINK_H__
#define __DUAL_SINK_H__



#include "dvs_sink.h"
#include "x11_common.h"
#include "media_control.h"
#include "video_switch_sink.h"


typedef struct DualSink DualSink;


int dusk_open(int reviewDuration, int dvsCard, int dvsChannel, SDIVITCSource sdiVITCSource,
              SDIVITCSource extraSDIVITCSource, int numBuffers, int useXV, int disableSDIOSD, int disableX11OSD,
              const Rational* pixelAspectRatio, const Rational* monitorAspectRatio,
              float scale, int swScale, int applyScaleFilter, int fitVideo, X11WindowInfo* windowInfo,
              DualSink** dualSink);
MediaSink* dusk_get_media_sink(DualSink* dualSink);
void dusk_set_media_control(DualSink* dualSink, ConnectMapping mapping, VideoSwitchSink* videoSwitch, MediaControl* control);
void dusk_unset_media_control(DualSink* dualSink);

DVSSink* dusk_get_dvs_sink(DualSink* dualSink);

void dusk_set_x11_window_name(DualSink* dualSink, const char* name);
void dusk_fit_x11_window(DualSink* dualSink);

void dusk_register_window_listener(DualSink* dualSink, X11WindowListener* listener);
void dusk_unregister_window_listener(DualSink* dualSink, X11WindowListener* listener);

void dusk_register_keyboard_listener(DualSink* dualSink, KeyboardInputListener* listener);
void dusk_unregister_keyboard_listener(DualSink* dualSink, KeyboardInputListener* listener);

void dusk_register_progress_bar_listener(DualSink* dualSink, ProgressBarInputListener* listener);
void dusk_unregister_progress_bar_listener(DualSink* dualSink, ProgressBarInputListener* listener);

void dusk_register_mouse_listener(DualSink* dualSink, MouseInputListener* listener);
void dusk_unregister_mouse_listener(DualSink* dualSink, MouseInputListener* listener);





#endif


