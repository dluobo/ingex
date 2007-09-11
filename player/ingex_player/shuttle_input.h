#ifndef __SHUTTLE_INPUT_H__
#define __SHUTTLE_INPUT_H__



#ifdef __cplusplus
extern "C" 
{
#endif


typedef struct ShuttleInput ShuttleInput;

typedef enum
{
    SH_PING_EVENT = 0,
    SH_KEY_EVENT,
    SH_SHUTTLE_EVENT,
    SH_JOG_EVENT
} ShuttleEventType;

typedef struct
{
    unsigned int number;
    int isPressed; /* otherwise key is released */
} ShuttleKeyEvent;

typedef struct
{
    int clockwise; /* otherwise anti-clockwise */
    unsigned int speed; /* value 0..7 */
} ShuttleShuttleEvent;

typedef struct
{
    int clockwise; /* otherwise anti-clockwise */
    int position;
} ShuttleJogEvent;

typedef struct 
{
    ShuttleEventType type;
    union
    {
        ShuttleKeyEvent key;
        ShuttleShuttleEvent shuttle;
        ShuttleJogEvent jog;
    } value;
} ShuttleEvent;


typedef void (*shuttle_listener)(void* data, ShuttleEvent* event);



int shj_open_shuttle(ShuttleInput** shuttle);
int shj_register_listener(ShuttleInput* shuttle, shuttle_listener listener, void* data);
void shj_unregister_listener(ShuttleInput* shuttle, shuttle_listener listener);
void shj_start_shuttle(ShuttleInput* shuttle);
void shj_close_shuttle(ShuttleInput** shuttle);

void shj_stop_shuttle(void* shuttle);


#ifdef __cplusplus
}
#endif


#endif



