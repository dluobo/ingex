/*
 * $Id: term_keyboard_input.h,v 1.4 2010/06/02 11:12:14 philipn Exp $
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

#ifndef __TERM_KEYBOARD_INPUT_H__
#define __TERM_KEYBOARD_INPUT_H__



#include "keyboard_input.h"


typedef struct TermKeyboardInput TermKeyboardInput;


int tki_create_term_keyboard(TermKeyboardInput** termInput);
KeyboardInput* tki_get_keyboard_input(TermKeyboardInput* termInput);
void tki_start_term_keyboard(TermKeyboardInput* termInput);
void tki_stop_term_keyboard(void* arg);

/* used when a seg fault occurs and we just restore the terminal settings */
void tki_restore_term_settings(TermKeyboardInput* termInput);



#endif

