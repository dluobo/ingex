#ifndef __TERM_KEYBOARD_INPUT_H__
#define __TERM_KEYBOARD_INPUT_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "keyboard_input.h"


typedef struct TermKeyboardInput TermKeyboardInput;


int tki_create_term_keyboard(TermKeyboardInput** termInput);
KeyboardInput* tki_get_keyboard_input(TermKeyboardInput* termInput);
void tki_start_term_keyboard(TermKeyboardInput* termInput);
void tki_stop_term_keyboard(void* arg);

/* used when a seg fault occurs and we just restore the terminal settings */
void tki_restore_term_settings(TermKeyboardInput* termInput);


#ifdef __cplusplus
}
#endif


#endif

