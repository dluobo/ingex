#ifndef __CONSOLE_MONITOR_H__
#define __CONSOLE_MONITOR_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_player.h"

typedef struct ConsoleMonitor ConsoleMonitor;


int csm_open(MediaPlayer* player, ConsoleMonitor** monitor);
void csm_close(ConsoleMonitor** monitor);





#ifdef __cplusplus
}
#endif


#endif


