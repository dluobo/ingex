#ifndef __QC_HTTP_ACCESS_H__
#define __QC_HTTP_ACCESS_H__



#include "media_player.h"


typedef struct QCHTTPAccess QCHTTPAccess;


int qch_create_qc_http_access(MediaPlayer* player, int port, const char* cacheDirectory, 
    const char* reportDirectory, QCHTTPAccess** access);
void qch_free_qc_http_access(QCHTTPAccess** access);




#endif


