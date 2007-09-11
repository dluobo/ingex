#ifndef __KEYBOARD_INPUT_CONNECT_H__
#define __KEYBOARD_INPUT_CONNECT_H__



#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_control.h"
#include "keyboard_input.h"


typedef struct KeyboardConnect KeyboardConnect;


int kic_create_keyboard_connect(int reviewDuration, MediaControl* control, 
    KeyboardInput* input, ConnectMapping mapping, KeyboardConnect** connect);
void kic_free_keyboard_connect(KeyboardConnect** connect);



#ifdef __cplusplus
}
#endif



#endif


