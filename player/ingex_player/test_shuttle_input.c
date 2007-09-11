#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "shuttle_input.h"


#define CHECK_FATAL(cond) \
    if (!(cond)) \
    { \
        fprintf(stderr, "%s failed in %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    }

    
static void listener(void* data, ShuttleEvent* event)
{
    switch (event->type)
    {
        case SH_KEY_EVENT:
            printf("Key: number = %u, is pressed = %d\n", event->value.key.number, 
                event->value.key.isPressed); 
            break;
        case SH_SHUTTLE_EVENT:
            printf("Shuttle: is clockwise = %d, speed = %u\n", event->value.shuttle.clockwise, 
                event->value.shuttle.speed); 
            break;
        case SH_JOG_EVENT:
            printf("Jog: is clockwise = %d, position = %d\n", event->value.jog.clockwise,
                 event->value.jog.position); 
            break;
        default:
            fprintf(stderr, "Unknown event type %d\n", event->type);
    }
}
    
    
int main (int argc, const char** argv)
{
    ShuttleInput* shuttle;

    
    CHECK_FATAL(shj_open_shuttle(&shuttle));
    CHECK_FATAL(register_listener(shuttle, listener, NULL));
    
    shj_start_shuttle(shuttle);
    
    shj_close_shuttle(&shuttle);
    
    
    return 0;
}


