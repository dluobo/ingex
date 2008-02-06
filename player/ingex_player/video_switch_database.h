#ifndef __VIDEO_SWITCH_DATABASE_H__
#define __VIDEO_SWITCH_DATABASE_H__


#include "types.h"


#ifdef __cplusplus
extern "C" 
{
#endif

#define VIDEO_SWITCH_DB_ENTRY_SIZE      64


typedef struct VideoSwitchDatabase VideoSwitchDatabase;


int vsd_open_write(const char* filename, VideoSwitchDatabase** db);
int vsd_open_append(const char* filename, VideoSwitchDatabase** db);
int vsd_open_read(const char* filename, VideoSwitchDatabase** db);
void vsd_close(VideoSwitchDatabase** db);


/* writer */

int vsd_append_entry(VideoSwitchDatabase* db, int sourceIndex, const char* sourceId, const Timecode* tc);


/* reader */

typedef struct
{
    int sourceIndex;
    const char* sourceId;
    int year;
    int month;
    int day;
    Timecode tc;
} VideoSwitchDbEntry;

int vsd_get_num_entries(VideoSwitchDatabase* db);
int vsd_load(VideoSwitchDatabase* db, int startEntry, unsigned char* buffer, int bufferSize, int* numEntries);
int vsd_parse_entry(const unsigned char* entryBuffer, int entryBufferSize, VideoSwitchDbEntry* entry);


int vsd_dump(const char* filename, FILE* output);


#ifdef __cplusplus
}
#endif


#endif


