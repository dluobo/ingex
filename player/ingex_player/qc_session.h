#ifndef __QC_SESSION_H__
#define __QC_SESSION_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_player.h"


typedef struct QCSession QCSession;


int qcs_open(const char* mxfFilename, MediaSource* source, int argc, const char** argv, 
    const char* name, const char* loadedSessionFilename, QCSession** qcSession);
int qcs_connect_to_player(QCSession* qcSession, MediaPlayer* player);
void qcs_write_marks(QCSession* qcSession, int includeAll, int clipMarkType, const MarkConfigs* markConfigs, Mark* marks, int numMarks);
void qcs_flush(QCSession* qcSession);
void qcs_close(QCSession** qcSession, const char* sessionScriptName, const char* sessionScriptOptions);

int qcs_set_marks(MediaControl* control, const char* filename);

int qcs_is_session_file(const char* filename, const char* mxfFilename);

const char* qcs_get_session_name(QCSession* qcSession);

int qcs_extract_timestamp(const char* sessionFilename, int* year, int* month, int* day, int* hour, int* min, int* sec);


#ifdef __cplusplus
}
#endif


#endif


