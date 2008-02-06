#ifndef __QC_LTO_ACCESS_H__
#define __QC_LTO_ACCESS_H__


#include "media_player.h"
#include "qc_lto_extract.h"


typedef struct QCLTOAccess QCLTOAccess;


int qla_create_qc_lto_access(const char* cacheDirectory, QCLTOExtract* extract, 
    const char* deleteScriptName, const char* deleteScriptOptions, QCLTOAccess** access);
void qla_free_qc_lto_access(QCLTOAccess** access);

int qla_connect_to_player(QCLTOAccess* access, MediaPlayer* player);

int qla_get_file_to_play(QCLTOAccess* access, char directory[FILENAME_MAX], char name[FILENAME_MAX], 
    char sessionName[FILENAME_MAX]);

void qla_set_current_session_name(QCLTOAccess* access, const char* sessionName);

void qla_set_current_play_name(QCLTOAccess* access, const char* directory, const char* name);

void qla_remove_old_sessions(QCLTOAccess* access, const char* directory, const char* name);


#endif


