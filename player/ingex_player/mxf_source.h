#ifndef __MXF_SOURCE_H__
#define __MXF_SOURCE_H__



#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_source.h"
#include "archive_types.h"


typedef struct MXFFileSource MXFFileSource;


/* MXF file source */

int mxfs_open(const char* filename, int forceD3MXF, int markPSEFailure, int markVTRErrors, MXFFileSource** source);
MediaSource* mxfs_get_media_source(MXFFileSource* source);


#ifdef __cplusplus
}
#endif


#endif

