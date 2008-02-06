#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "frame_info.h"
#include "logging.h"
#include "macros.h"


static const char* g_streamTypeStrings[] = 
{
    "unknown",
    "picture",
    "sound",
    "timecode",
    "event"
};

static const char* g_streamFormatStrings[] = 
{
    "unknown",
    "uyvy",
    "uyvy10bit",
    "yuv420",
    "yuv411",
    "yuv422",
    "yuv444",
    "dv25_420",
    "dv25_411",
    "dv50",
    "d10_picture",
    "avid_mjpeg",
    "avid_dnxhd",
    "pcm",
    "timecode",
    "source_event"
};

/* keep length <= MAX_SOURCE_INFO_NAME_LEN */
static const char* g_sourceInfoNames[] = 
{ 
    "Filename", 
    "File type",
    "File duration", 
    
    "Orig filename", 
    "Creation date", 
    "LTO spool no.", 
    "D3 spool no.", 
    "Prog title", 
    "Episode title", 
    "Tx date", 
    "Prog number", 
    "Prog duration", 
    
    "Title",
    
    "Unknown"
};



const char* get_stream_type_string(StreamType type)
{
    if (type >= sizeof(g_streamTypeStrings) / sizeof(char*))
    {
        assert(type >= sizeof(g_streamTypeStrings) / sizeof(char*));
        ml_log_error("unknown stream type %d\n", type);
        return g_streamTypeStrings[0];
    }
    
    return g_streamTypeStrings[type];
}

const char* get_stream_format_string(StreamFormat format)
{
    if (format >= sizeof(g_streamFormatStrings) / sizeof(char*))
    {
        assert(format >= sizeof(g_streamFormatStrings) / sizeof(char*));
        ml_log_error("unknown stream format %d\n", format);
        return g_streamFormatStrings[0];
    }
    
    return g_streamFormatStrings[format];
}


int initialise_stream_info(StreamInfo* streamInfo)
{
    memset(streamInfo, 0, sizeof(StreamInfo));
    
    CALLOC_ORET(streamInfo->sourceInfoValues, SourceInfoValue, 32);
    streamInfo->numSourceInfoValuesAlloc = 32;
    
    return 1;
}

int add_source_info(StreamInfo* streamInfo, const char* name, const char* value)
{
    SourceInfoValue* newArray = NULL;
    char* newName = NULL;
    char* newValue = NULL;
    
    /* copy new name-value pair */
    if (name == NULL)
    {
        MALLOC_OFAIL(newName, char, 1);
        newName[0] = '\0';
    }
    else
    {
        MALLOC_OFAIL(newName, char, MAX_SOURCE_INFO_NAME_LEN + 1);
        strncpy(newName, name, MAX_SOURCE_INFO_NAME_LEN);
        newName[MAX_SOURCE_INFO_NAME_LEN] = '\0';
    }
    if (value == NULL)
    {
        MALLOC_OFAIL(newValue, char, 1);
        newValue[0] = '\0';
    }
    else
    {
        MALLOC_OFAIL(newValue, char, MAX_SOURCE_INFO_VALUE_LEN + 1);
        strncpy(newValue, value, MAX_SOURCE_INFO_VALUE_LEN);
        newValue[MAX_SOURCE_INFO_VALUE_LEN] = '\0';
    }
    
    if (streamInfo->numSourceInfoValues + 1 > streamInfo->numSourceInfoValuesAlloc)
    {
        /* allocate new array and copy over old values */
        CALLOC_OFAIL(newArray, SourceInfoValue, streamInfo->numSourceInfoValuesAlloc + 32);
        memcpy(newArray, streamInfo->sourceInfoValues, streamInfo->numSourceInfoValuesAlloc * sizeof(SourceInfoValue));
        
        SAFE_FREE(&streamInfo->sourceInfoValues);
        streamInfo->sourceInfoValues = newArray;
        newArray = NULL;
        streamInfo->numSourceInfoValuesAlloc += 32;
    }

    streamInfo->numSourceInfoValues++;
    streamInfo->sourceInfoValues[streamInfo->numSourceInfoValues - 1].name = newName;
    newName = NULL;
    streamInfo->sourceInfoValues[streamInfo->numSourceInfoValues - 1].value = newValue;
    newValue = NULL;
    
    return 1;
    
fail:
    SAFE_FREE(&newName);
    SAFE_FREE(&newValue);
    SAFE_FREE(&newArray);
    return 0;
}

int add_known_source_info(StreamInfo* streamInfo, SourceInfoName name, const char* value)
{
    if (name > sizeof(g_sourceInfoNames) / sizeof(char*))
    {
        ml_log_warn("Unknown name enum %d\n", name);
        return add_source_info(streamInfo, g_sourceInfoNames[SRC_INFO_UNKNOWN], value);
    }
    else
    {
        return add_source_info(streamInfo, g_sourceInfoNames[name], value);
    }
}

void clear_stream_info(StreamInfo* streamInfo)
{
    int i;
    
    if (streamInfo == NULL)
    {
        return;
    }
    
    for (i = 0; i < streamInfo->numSourceInfoValues; i++)
    {
        SAFE_FREE(&streamInfo->sourceInfoValues[i].name);
        SAFE_FREE(&streamInfo->sourceInfoValues[i].value);
    }
    SAFE_FREE(&streamInfo->sourceInfoValues);

    memset(streamInfo, 0, sizeof(StreamInfo));
}


int select_frame_timecode(const FrameInfo* frameInfo, int tcIndex, int tcType, int tcSubType, Timecode* timecode)
{
    int typedIndex = 0;
    int index = 0;
    int i;
    for (i = 0; i < frameInfo->numTimecodes; i++)
    {
        if ((tcIndex < 0 || tcIndex == typedIndex) &&
            (tcType < 0 || frameInfo->timecodes[i].timecodeType == (TimecodeType)tcType) &&
            (tcSubType < 0 || frameInfo->timecodes[i].timecodeSubType == (TimecodeSubType)tcSubType))
        {
            *timecode = frameInfo->timecodes[i].timecode;
            return 1;
        }
        else if ((tcType < 0 || frameInfo->timecodes[i].timecodeType == (TimecodeType)tcType) &&
            (tcSubType < 0 || frameInfo->timecodes[i].timecodeSubType == (TimecodeSubType)tcSubType))
        {
            typedIndex++;
        }
        index++;
    }
    
    return 0;
}

