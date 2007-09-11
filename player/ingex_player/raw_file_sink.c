#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "raw_file_sink.h"
#include "logging.h"
#include "macros.h"


/* a large number that we don't axpect to exceed */
#define MAX_OUTPUT_FILES         256


typedef struct
{
    int streamId;
    unsigned char* buffer;
    unsigned int bufferSize; 
    int isPresent;
    FILE* outputFile;
} RawStream;

typedef struct
{
    /* media sink interface */
    MediaSink mediaSink;
    
    /* filename template */
    char* filenameTemplate;
    
    /* listener */
    MediaSinkListener* listener;
    
    /* streams */
    RawStream streams[MAX_OUTPUT_FILES];
    int numStreams;
} RawFileSink;



static void reset_streams(RawFileSink* sink)
{
    int i;

    for (i = 0; i < sink->numStreams; i++)
    {
        sink->streams[i].isPresent = 0;
    }
}

static RawStream* get_raw_stream(RawFileSink* sink, int streamId)
{
    int i;
    
    for (i = 0; i < sink->numStreams; i++)
    {
        if (streamId == sink->streams[i].streamId)
        {
            return &sink->streams[i];
        }
    }
    return NULL;
}


static int rms_register_listener(void* data, MediaSinkListener* listener)
{
    RawFileSink* sink = (RawFileSink*)data;
    
    sink->listener = listener;
    
    return 1;
}

static void rms_unregister_listener(void* data, MediaSinkListener* listener)
{
    RawFileSink* sink = (RawFileSink*)data;
    
    if (sink->listener == listener)
    {
        sink->listener = NULL;
    }
}

static int rms_accept_stream(void* data, const StreamInfo* streamInfo)
{
    /* TODO: add option to set what the raw file sink should accept */
    
    /* make sure that the picture is UYVY */
    if (streamInfo->type == PICTURE_STREAM_TYPE &&
        streamInfo->format != UYVY_FORMAT)
    {
        return 0;
    }
    /* make sure the sound is PCM */
    else if (streamInfo->type == SOUND_STREAM_TYPE &&
        streamInfo->format != PCM_FORMAT)
    {
        return 0;
    }
    return 1;
}

static int rms_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    RawFileSink* sink = (RawFileSink*)data;
    char filename[FILENAME_MAX];

    /* this should've been checked, but we do it here anyway */
    if (!rms_accept_stream(data, streamInfo))
    {
        ml_log_error("Can't register a stream %d that is not accepted\n", streamId);
        return 0;
    }

    if (sink->numStreams + 1 >= MAX_OUTPUT_FILES)
    {
        ml_log_warn("Number of streams exceeds hard coded maximum %d\n", MAX_OUTPUT_FILES);
        return 0;
    }
    
    sink->streams[sink->numStreams].streamId = streamId;
    if (snprintf(filename, FILENAME_MAX, sink->filenameTemplate, streamId) < 0)
    {
        ml_log_error("Failed to create raw output filename from template '%s'\n", sink->filenameTemplate);
        return 0;
    }
    if ((sink->streams[sink->numStreams].outputFile = fopen(filename, "wb")) == NULL)
    {
        ml_log_error("Failed to open raw output file '%s'\n", filename);
        return 0;
    }
    sink->numStreams++;
    
    return 1;
}

static int rms_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    return 1;
}

static int rms_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    RawFileSink* sink = (RawFileSink*)data;
    RawStream* rawStream;
    
    if ((rawStream = get_raw_stream(sink, streamId)) == NULL)
    {
        ml_log_error("Unknown stream %d for raw file output\n", streamId);
        return 0;
    }
    
    if (rawStream->bufferSize != bufferSize)
    {
        if ((rawStream->buffer = realloc(rawStream->buffer, bufferSize)) == NULL)
        {
            ml_log_error("Failed to realloc memory for raw stream buffer\n");
            return 0;
        }
        rawStream->bufferSize = bufferSize;
    }
    
    *buffer = rawStream->buffer;
    
    return 1;
}

static int rms_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    RawFileSink* sink = (RawFileSink*)data;
    RawStream* rawStream;
    
    if ((rawStream = get_raw_stream(sink, streamId)) == NULL)
    {
        ml_log_error("Unknown stream %d for raw file output\n", streamId);
        return 0;
    }
    if (rawStream->bufferSize != bufferSize)
    {
        ml_log_error("Buffer size (%d) != raw stream buffer size (%d)\n", bufferSize, rawStream->bufferSize);
        return 0;
    }
        
    rawStream->isPresent = 1;

    return 1;
}

static int rms_complete_frame(void* data, const FrameInfo* frameInfo)
{
    RawFileSink* sink = (RawFileSink*)data;
    int i;

    for (i = 0; i < sink->numStreams; i++)
    {
        if (sink->streams[i].isPresent)
        {
            if (fwrite(sink->streams[i].buffer, sink->streams[i].bufferSize, 1, sink->streams[i].outputFile) != 1)
            {
                perror("fwrite");
                ml_log_error("Failed to write raw data to file\n");
            }
        }
    }
    
    msl_frame_displayed(sink->listener, frameInfo);

    reset_streams(sink);
    
    return 1;    
}

static void rms_cancel_frame(void* data)
{
    RawFileSink* sink = (RawFileSink*)data;

    reset_streams(sink);
}

static void rms_close(void* data)
{
    RawFileSink* sink = (RawFileSink*)data;
    int i;

    if (data == NULL)
    {
        return;
    }
    
    for (i = 0; i < sink->numStreams; i++)
    {
        SAFE_FREE(&sink->streams[i].buffer);
        if (sink->streams[i].outputFile != NULL)
        {
            fclose(sink->streams[i].outputFile);
        }
    }
    SAFE_FREE(&sink->filenameTemplate);
    SAFE_FREE(&sink);
}


int rms_open(const char* filenameTemplate, MediaSink** sink)
{
    RawFileSink* newSink;
    
    if (strstr(filenameTemplate, "%d") == NULL || 
        strchr(strchr(filenameTemplate, '%') + 1, '%') != NULL)
    {
        ml_log_error("Invalid filename template '%s': must contain a single '%%d'\n", filenameTemplate);
        return 0;
    }
    
    CALLOC_ORET(newSink, RawFileSink, 1);

    MALLOC_OFAIL(newSink->filenameTemplate, char, strlen(filenameTemplate) + 1);
    strcpy(newSink->filenameTemplate, filenameTemplate);
    
    
    newSink->mediaSink.data = newSink;
    newSink->mediaSink.register_listener = rms_register_listener;
    newSink->mediaSink.unregister_listener = rms_unregister_listener;
    newSink->mediaSink.accept_stream = rms_accept_stream;
    newSink->mediaSink.register_stream = rms_register_stream;
    newSink->mediaSink.accept_stream_frame = rms_accept_stream_frame;
    newSink->mediaSink.get_stream_buffer = rms_get_stream_buffer;
    newSink->mediaSink.receive_stream_frame = rms_receive_stream_frame;
    newSink->mediaSink.complete_frame = rms_complete_frame;
    newSink->mediaSink.cancel_frame = rms_cancel_frame;
    newSink->mediaSink.close = rms_close;
    
    
    *sink = &newSink->mediaSink;
    return 1;
    
fail:
    rms_close(newSink);
    return 0;
}



