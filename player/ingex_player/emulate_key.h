#ifndef __EMULATE_KEY_H__
#define __EMULATE_KEY_H__


#ifdef __cplusplus
extern "C" 
{
#endif


/* key codes are defined in /usr/include/X11/keysymdef.h (include keysym.h and not keysymdef.h) 
   and modifiers (masks, eg. ShiftMask) in /usr/include/X11/X.h */


typedef struct EmulateKey EmulateKey;


int create_emu(EmulateKey** emu);
int emu_key_down(EmulateKey* emu, int keysym, int modifier);
int emu_key_up(EmulateKey* emu, int keysym, int modifier);
/* emu_key is equivalent to emu_key_down followed by emu_key_up */ 
int emu_key(EmulateKey* emu, int keysym, int modifier);
void free_emu(EmulateKey** emu);



#ifdef __cplusplus
}
#endif


#endif


