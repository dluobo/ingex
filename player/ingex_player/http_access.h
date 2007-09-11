#ifndef __HTTP_ACCESS_H__
#define __HTTP_ACCESS_H__



#include "media_player.h"


typedef struct HTTPAccess HTTPAccess;


int hac_create_http_access(MediaPlayer* player, int port, HTTPAccess** access);
void hac_free_http_access(HTTPAccess** access);




#endif


