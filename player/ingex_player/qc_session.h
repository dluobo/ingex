#ifndef __QC_SESSION_H__
#define __QC_SESSION_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_player.h"


typedef struct QCSession QCSession;


int qcs_open(const char* mxfFilename, int argc, const char** argv, const char* name, QCSession** qcSession);
int qcs_connect_to_player(QCSession* qcSession, MediaPlayer* player);
void qcs_write_marks(QCSession* qcSession, const MarkConfigs* markConfigs, Mark* marks, int numMarks);
void qcs_flush(QCSession* qcSession);
void qcs_close(QCSession** qcSession);

int qcs_set_marks(MediaControl* control, const char* filename);

int qcs_is_session_file(const char* filename, const char* mxfFilename);

const char* qcs_get_session_name(QCSession* qcSession);


#ifdef __cplusplus
}
#endif


#endif


