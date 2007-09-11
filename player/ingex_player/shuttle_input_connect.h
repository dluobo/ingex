#ifndef __SHUTTLE_INPUT_CONNECT_H__
#define __SHUTTLE_INPUT_CONNECT_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_control.h"
#include "shuttle_input.h"


typedef struct ShuttleConnect ShuttleConnect;


int sic_create_shuttle_connect(int reviewDuration, MediaControl* control, ShuttleInput* shuttle, 
    ConnectMapping mapping, ShuttleConnect** connect);
void sic_free_shuttle_connect(ShuttleConnect** connect);



#ifdef __cplusplus
}
#endif


#endif

