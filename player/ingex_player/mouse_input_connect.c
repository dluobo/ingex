/*
 * $Id: mouse_input_connect.c,v 1.4 2011/10/27 13:45:37 philipn Exp $
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "mouse_input_connect.h"
#include "logging.h"
#include "macros.h"


struct MouseConnect
{
    MediaControl* control;
    VideoSwitchSink* videoSwitch;
    MouseInput* input;
    MouseInputListener listener;
};

static void mic_click(void* data, int imageWidth, int imageHeight, int xPos, int yPos)
{
    MouseConnect* connect = (MouseConnect*)data;
    int indexAtClickPos, index;

    if (vsw_get_video_index_at_pos(connect->videoSwitch, imageWidth, imageHeight, xPos, yPos, &indexAtClickPos))
    {
        if (!vsw_get_video_index(connect->videoSwitch, &index) || indexAtClickPos != index)
        {
            mc_switch_video(connect->control, indexAtClickPos);
        }
        else
        {
            mc_switch_video(connect->control, 0);
        }
    }
}

int mic_create_mouse_connect(MediaControl* control, VideoSwitchSink* videoSwitch,
    MouseInput* input, MouseConnect** connect)
{
    MouseConnect* newConnect;

    CALLOC_ORET(newConnect, MouseConnect, 1);

    newConnect->control = control;
    newConnect->videoSwitch = videoSwitch;
    newConnect->input = input;

    newConnect->listener.data = newConnect;
    newConnect->listener.click = mic_click;

    mip_set_listener(input, &newConnect->listener);

    *connect = newConnect;
    return 1;
}

void mic_free_mouse_connect(MouseConnect** connect)
{
    if (*connect == NULL)
    {
        return;
    }

    mip_unset_listener((*connect)->input);

    SAFE_FREE(connect);
}



