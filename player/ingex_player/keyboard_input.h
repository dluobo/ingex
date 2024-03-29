/*
 * $Id: keyboard_input.h,v 1.6 2010/06/02 11:12:14 philipn Exp $
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

#ifndef __KEYBOARD_INPUT_H__
#define __KEYBOARD_INPUT_H__




#define SHIFT_KEY_MODIFIER           0x01
#define CONTROL_KEY_MODIFIER         0x02
#define ALT_KEY_MODIFIER             0x04


typedef struct
{
    void* data; /* passed to functions */

    void (*key_pressed)(void* data, int key, int modifier);
    void (*key_released)(void* data, int key, int modifier);
} KeyboardInputListener;

typedef struct
{
    void* data; /* passed to functions */

    void (*set_listener)(void* data, KeyboardInputListener* listener);
    void (*unset_listener)(void* data);
    void (*close)(void* data);
} KeyboardInput;


/* utility functions for calling KeyboardInputListener functions */

void kil_key_pressed(KeyboardInputListener* listener, int key, int modifier);
void kil_key_released(KeyboardInputListener* listener, int key, int modifier);


/* utility functions for calling KeyboardInput functions */

void kip_set_listener(KeyboardInput* input, KeyboardInputListener* listener);
void kip_unset_listener(KeyboardInput* input);
void kip_close(KeyboardInput* input);






#endif


