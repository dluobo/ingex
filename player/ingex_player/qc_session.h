#ifndef __QC_SESSION_H__
#define __QC_SESSION_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_player.h"

#define MAX_SESSION_COMMENTS_SIZE       400


typedef struct QCSession QCSession;


int qcs_open(const char* mxfFilename, MediaSource* source, int argc, const char** argv, 
    const char* name, const char* loadedSessionFilename, const char* userName, const char* hostName, 
    QCSession** qcSession);
int qcs_connect_to_player(QCSession* qcSession, MediaPlayer* player);
void qcs_write_marks(QCSession* qcSession, int includeAll, int clipMarkType, const MarkConfigs* markConfigs, Mark* marks, int numMarks);
void qcs_flush(QCSession* qcSession);
void qcs_close(QCSession** qcSession, const char* reportDirectory, const char* sessionScriptName, const char* sessionScriptOptions, int reportAccessPort);

int qcs_restore_session(MediaControl* control, const char* filename, char* sessionComments);

int qcs_is_session_file(const char* filename, const char* mxfFilename);

const char* qcs_get_session_name(QCSession* qcSession);

int qcs_extract_timestamp(const char* sessionFilename, int* year, int* month, int* day, int* hour, int* min, int* sec);

void qcs_set_comments(QCSession* qcSession, const char* sessionComments);


#ifdef __cplusplus
}
#endif


#endif


