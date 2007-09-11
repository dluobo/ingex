#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/X.h>
#include <X11/keysym.h>


#include "emulate_key.h"


#define CHECK_FATAL(cond) \
    if (!(cond)) \
    { \
        fprintf(stderr, "%s failed in %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    }

    
    
int main (int argc, const char** argv)
{
    EmulateKey* emu;

    
    CHECK_FATAL(create_emu(&emu));

    /* 'a' */
    CHECK_FATAL(emu_key(emu, XK_a, 0));
    
    /* '+' becomes '=' because no shift modifier */
    CHECK_FATAL(emu_key(emu, '+', 0));
    
    /* '+' ok with shift modifier */
    CHECK_FATAL(emu_key(emu, '+', ShiftMask));
    
    /* '1' */
    CHECK_FATAL(emu_key_down(emu, '1', 0));
    CHECK_FATAL(emu_key_up(emu, '1', 0));
    
    /* '3' x 3 */
    CHECK_FATAL(emu_key_down(emu, '3', 0));
    CHECK_FATAL(emu_key_down(emu, '3', 0));
    CHECK_FATAL(emu_key_down(emu, '3', 0));
    CHECK_FATAL(emu_key_up(emu, '3', 0));
    
    free_emu(&emu);
    
    
    return 0;
}


