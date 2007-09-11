#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "video_switch_sink.h"
#include "YUV_frame.h"
#include "YUV_quarter_frame.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


typedef struct VideoStreamElement
{
    struct VideoStreamElement* next;
    struct VideoStreamElement* prev;
    
    int index; /* used by quad split to limit quad split streams to 4 */
    int streamId;
    StreamInfo streamInfo;    
} VideoStreamElement;


typedef struct 
{
    int withQuadSplit;
    int applyQuadSplitFilter;
    
    MediaSink* targetSink;

    VideoSwitchSink switchSink; 
    MediaSink sink;
    
    /* targetSinkListener listens to target sink and forwards to the switch sink listener */
    MediaSinkListener targetSinkListener;
    MediaSinkListener* switchListener;
    
    VideoStreamElement streams;
    VideoStreamElement* firstInputStream;
    VideoStreamElement* quadSplitStream;
    VideoStreamElement* currentStream;
    
    pthread_mutex_t nextCurrentStreamMutex;
    VideoStreamElement* nextCurrentStream;
    int disableSwitching;
    int haveSwitched;

    unsigned char* quadSplitWorkspaceTopLeft;
    unsigned char* quadSplitWorkspaceTopRight;
    unsigned char* quadSplitWorkspaceBottomLeft;
    unsigned char* quadSplitWorkspaceBottomRight;
    unsigned char* quadSplitInputBufferTopLeft;
    unsigned char* quadSplitInputBufferTopRight;
    unsigned char* quadSplitInputBufferBottomLeft;
    unsigned char* quadSplitInputBufferBottomRight;
    unsigned int quadSplitInputBufferSize; 
    YUV_frame quadSplitFrameInTopLeft;
    YUV_frame quadSplitFrameInTopRight;
    YUV_frame quadSplitFrameInBottomLeft;
    YUV_frame quadSplitFrameInBottomRight;
    unsigned char* quadSplitOutputBuffer; /* reference to sink buffer, size == quadSplitInputBufferSize */
    YUV_frame quadSplitFrameOut;
    formats yuvFormat;

    int haveCheckedFirstInputStream;
    int haveAcceptedFirstInputStream;
    int haveQuadSplitOutputBuffer;
} QuadVideoSwitch;



static int add_stream(QuadVideoSwitch* swtch, int streamId, const StreamInfo* streamInfo)
{
    VideoStreamElement* ele = &swtch->streams;
    VideoStreamElement* newEle = NULL;

    /* handle first append */
    if (swtch->firstInputStream == NULL && !swtch->withQuadSplit)
    {
        ele->index = 1;
        ele->streamId = streamId;
        ele->streamInfo = *streamInfo;
        swtch->firstInputStream = ele;
        swtch->currentStream = ele;
        swtch->nextCurrentStream = ele;
        return 1;
    }
    
    /* move to end */
    while (ele->next != NULL)
    {
        ele = ele->next;
    }

    /* create element */
    CALLOC_ORET(newEle, VideoStreamElement, 1);
    
    /* append */
    newEle->prev = ele;
    newEle->index = ele->index + 1;
    newEle->streamId = streamId;
    newEle->streamInfo = *streamInfo;
    if (swtch->withQuadSplit && swtch->quadSplitStream == NULL)
    {
        /* TODO: allow for different size inputs */
        
        /* allocate the quad split input buffer and initialise the YUV_lib frame for the input */
        if (streamInfo->format == UYVY_FORMAT)
        {
            swtch->yuvFormat = UYVY;
            swtch->quadSplitInputBufferSize = streamInfo->width * streamInfo->height * 2;
        }
        else if (streamInfo->format == YUV422_FORMAT)
        {
            swtch->yuvFormat = YV12;
            swtch->quadSplitInputBufferSize = streamInfo->width * streamInfo->height * 2;
        }
        else if (streamInfo->format == YUV420_FORMAT)
        {
            swtch->yuvFormat = I420;
            swtch->quadSplitInputBufferSize = streamInfo->width * streamInfo->height * 3 / 2;
        }
        else
        {
            ml_log_error("Video format not supported: %d\n", streamInfo->format);
        }
        
        MALLOC_OFAIL(swtch->quadSplitInputBufferTopLeft, unsigned char, swtch->quadSplitInputBufferSize);
        YUV_frame_from_buffer(&swtch->quadSplitFrameInTopLeft, swtch->quadSplitInputBufferTopLeft, 
            streamInfo->width, streamInfo->height, swtch->yuvFormat);
        MALLOC_OFAIL(swtch->quadSplitInputBufferTopRight, unsigned char, swtch->quadSplitInputBufferSize);
        YUV_frame_from_buffer(&swtch->quadSplitFrameInTopRight, swtch->quadSplitInputBufferTopRight, 
            streamInfo->width, streamInfo->height, swtch->yuvFormat);
        MALLOC_OFAIL(swtch->quadSplitInputBufferBottomLeft, unsigned char, swtch->quadSplitInputBufferSize);
        YUV_frame_from_buffer(&swtch->quadSplitFrameInBottomLeft, swtch->quadSplitInputBufferBottomLeft, 
            streamInfo->width, streamInfo->height, swtch->yuvFormat);
        MALLOC_OFAIL(swtch->quadSplitInputBufferBottomRight, unsigned char, swtch->quadSplitInputBufferSize);
        YUV_frame_from_buffer(&swtch->quadSplitFrameInBottomRight, swtch->quadSplitInputBufferBottomRight, 
            streamInfo->width, streamInfo->height, swtch->yuvFormat);
        
        /* initialise the quad split work buffers */
        MALLOC_OFAIL(swtch->quadSplitWorkspaceTopLeft, unsigned char, streamInfo->width * 3);
        MALLOC_OFAIL(swtch->quadSplitWorkspaceTopRight, unsigned char, streamInfo->width * 3);
        MALLOC_OFAIL(swtch->quadSplitWorkspaceBottomLeft, unsigned char, streamInfo->width * 3);
        MALLOC_OFAIL(swtch->quadSplitWorkspaceBottomRight, unsigned char, streamInfo->width * 3);

        /* quad split stream is the list root */
        swtch->quadSplitStream = &swtch->streams;
        swtch->firstInputStream = newEle;
        swtch->currentStream = &swtch->streams; 
        swtch->nextCurrentStream = swtch->currentStream;
    }
    ele->next = newEle;
    
    assert(swtch->firstInputStream != NULL);
    
    return 1;
    
fail:
    SAFE_FREE(&newEle);
    return 0;
}

static int is_switchable_stream(QuadVideoSwitch* swtch, int streamId)
{
    VideoStreamElement* ele = swtch->firstInputStream;
    while (ele != NULL)
    {
        if (ele->streamId == streamId)
        {
            return 1;
        }
        ele = ele->next;
    }
    return 0;
}

/* returns 1..4, or 0 if it is not part of the quad split */
static int get_quad_split_index(QuadVideoSwitch* swtch, int streamId)
{
    VideoStreamElement* ele = swtch->firstInputStream;
    while (ele != NULL)
    {
        if (ele->streamId == streamId)
        {
            if (ele->index >= 1 && ele->index <= 4)
            {
                return ele->index;
            }
            else
            {
                /* not part of the quad split */
                return 0;
            }
          
        }
        ele = ele->next;
    }
    return 0;
}



static void qvs_frame_displayed(void* data, const FrameInfo* frameInfo)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;
    
    msl_frame_displayed(swtch->switchListener, frameInfo);
}

static void qvs_frame_dropped(void* data, const FrameInfo* lastFrameInfo)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    msl_frame_dropped(swtch->switchListener, lastFrameInfo);
}

static void qvs_refresh_required(void* data)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    msl_refresh_required(swtch->switchListener);
}



static int qvs_register_listener(void* data, MediaSinkListener* listener)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    swtch->switchListener = listener;
    
    return msk_register_listener(swtch->targetSink, &swtch->targetSinkListener);
}

static void qvs_unregister_listener(void* data, MediaSinkListener* listener)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    if (swtch->switchListener == listener)
    {
        swtch->switchListener = NULL;
    }
    
    msk_unregister_listener(swtch->targetSink, &swtch->targetSinkListener);
}

static int qvs_accept_stream(void* data, const StreamInfo* streamInfo)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    return msk_accept_stream(swtch->targetSink, streamInfo);
}

static int qvs_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    if (streamInfo->type == PICTURE_STREAM_TYPE && 
        (streamInfo->format == UYVY_FORMAT || 
            streamInfo->format == YUV422_FORMAT ||
            streamInfo->format == YUV420_FORMAT))
    {
        if (swtch->firstInputStream == NULL)
        {
            /* if sink accepts this stream then it is the first input stream */
            if (msk_register_stream(swtch->targetSink, streamId, streamInfo))
            {
                return add_stream(swtch, streamId, streamInfo);
            }
        }
        else
        {
            /* if stream matches 1st stream then add to list */
            if (streamInfo->format == swtch->firstInputStream->streamInfo.format &&
                memcmp(&streamInfo->frameRate, &swtch->firstInputStream->streamInfo.frameRate, sizeof(Rational)) == 0 &&
                streamInfo->width == swtch->firstInputStream->streamInfo.width &&
                streamInfo->height == swtch->firstInputStream->streamInfo.height)
            {
                return add_stream(swtch, streamId, streamInfo);
            }
        }
        return 0;
    }
    else
    {
        return msk_register_stream(swtch->targetSink, streamId, streamInfo);
    }
}

static int qvs_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    if (is_switchable_stream(swtch, streamId))
    {
        if (!swtch->disableSwitching)
        {
            /* switch stream if required */
            PTHREAD_MUTEX_LOCK(&swtch->nextCurrentStreamMutex);
            if (swtch->currentStream != swtch->nextCurrentStream)
            {
                swtch->haveSwitched = 1;
                swtch->currentStream = swtch->nextCurrentStream;
            }
            swtch->disableSwitching = 1;
            PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentStreamMutex);
        }

        if (swtch->currentStream == swtch->quadSplitStream)
        {
            if (!swtch->haveCheckedFirstInputStream)
            {
                swtch->haveAcceptedFirstInputStream = msk_accept_stream_frame(swtch->targetSink, 
                    swtch->firstInputStream->streamId, frameInfo);
                swtch->haveCheckedFirstInputStream = 1;
            }
            return swtch->haveAcceptedFirstInputStream && 
                get_quad_split_index(swtch, streamId) != 0;
        }
        else
        {
            if (!swtch->haveCheckedFirstInputStream)
            {
                swtch->haveAcceptedFirstInputStream = msk_accept_stream_frame(swtch->targetSink, 
                    swtch->firstInputStream->streamId, frameInfo);
                swtch->haveCheckedFirstInputStream = 1;
            }
            return swtch->haveAcceptedFirstInputStream && swtch->currentStream->streamId == streamId;
        }
    }
    else
    {
        return msk_accept_stream_frame(swtch->targetSink, streamId, frameInfo);
    }
}

static int qvs_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    if (is_switchable_stream(swtch, streamId))
    {
        if (swtch->currentStream == swtch->quadSplitStream)
        {
            /* allocate the output buffer if we haven't done so already */
            if (!swtch->haveQuadSplitOutputBuffer)
            {
                /* check the buffer size */
                if (swtch->quadSplitInputBufferSize != bufferSize)
                {
                    fprintf(stderr, "Requested buffer size (%d) != quad split buffer size (%d)\n", 
                        bufferSize, swtch->quadSplitInputBufferSize);
                    return 0;
                }
                
                CHK_ORET(msk_get_stream_buffer(swtch->targetSink, swtch->firstInputStream->streamId, 
                    bufferSize, &swtch->quadSplitOutputBuffer));
                    
                /* initialise the YUV_lib frame for the sink buffer */
                YUV_frame_from_buffer(&swtch->quadSplitFrameOut, swtch->quadSplitOutputBuffer, 
                    swtch->firstInputStream->streamInfo.width, swtch->firstInputStream->streamInfo.height, swtch->yuvFormat);

                /* always clear the frame, eg. to prevent video from non-quad showing through
                after a switch or a disabled OSD */
                clear_YUV_frame(&swtch->quadSplitFrameOut);
                
                swtch->haveQuadSplitOutputBuffer = 1;
            }

            /* set the input buffer */
            switch (get_quad_split_index(swtch, streamId))
            {
                /* top left */
                case 1:
                    *buffer = swtch->quadSplitInputBufferTopLeft;
                    break;
                /* top right */
                case 2:
                    *buffer = swtch->quadSplitInputBufferTopRight;
                    break;
                /* bottom left */    
                case 3:
                    *buffer = swtch->quadSplitInputBufferBottomLeft;
                    break;
                /* bottom right */    
                case 4:
                    *buffer = swtch->quadSplitInputBufferBottomRight;
                    break;
                default:
                    /* shouldn't be here if accept_stream_frame was called */
                    assert(0);
                    return 0;
            }
            
            return 1;
        }
        else if (swtch->currentStream->streamId == streamId)
        {
            return msk_get_stream_buffer(swtch->targetSink, swtch->firstInputStream->streamId, bufferSize, buffer);
        }
        
        /* shouldn't be here if qvs_accept_stream_frame was called */
        assert(0);
        return 0;
    }
    else
    {
        return msk_get_stream_buffer(swtch->targetSink, streamId, bufferSize, buffer);
    }
}

static int qvs_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    if (is_switchable_stream(swtch, streamId))
    {
        if (swtch->currentStream == swtch->quadSplitStream)
        {
            /* check the buffer size */
            if (swtch->quadSplitInputBufferSize != bufferSize)
            {
                ml_log_error("Buffer size (%d) != quad split data size (%d)\n", bufferSize, swtch->quadSplitInputBufferSize);
                return 0;
            }
            
            /* quad split */
            switch (get_quad_split_index(swtch, streamId))
            {
                /* top left */
                case 1:
                    quarter_frame(&swtch->quadSplitFrameInTopLeft, 
                        &swtch->quadSplitFrameOut,
                        0, 
                        0, 
                        1, swtch->applyQuadSplitFilter, 
                        swtch->applyQuadSplitFilter, 
                        swtch->quadSplitWorkspaceTopLeft);
                    break;
                /* top right */
                case 2:
                    quarter_frame(&swtch->quadSplitFrameInTopRight, 
                        &swtch->quadSplitFrameOut, 
                        swtch->firstInputStream->streamInfo.width / 2, 
                        0,
                        1, 
                        swtch->applyQuadSplitFilter, 
                        swtch->applyQuadSplitFilter, 
                        swtch->quadSplitWorkspaceTopRight);
                    break;
                /* bottom left */    
                case 3:
                    quarter_frame(&swtch->quadSplitFrameInBottomLeft, 
                        &swtch->quadSplitFrameOut, 
                        0, 
                        swtch->firstInputStream->streamInfo.height / 2, 
                        1, 
                        swtch->applyQuadSplitFilter, 
                        swtch->applyQuadSplitFilter, 
                        swtch->quadSplitWorkspaceBottomLeft);
                    break;
                /* bottom right */    
                case 4:
                    quarter_frame(&swtch->quadSplitFrameInBottomRight, 
                        &swtch->quadSplitFrameOut, 
                        swtch->firstInputStream->streamInfo.width / 2, 
                        swtch->firstInputStream->streamInfo.height / 2, 
                        1, swtch->applyQuadSplitFilter, 
                        swtch->applyQuadSplitFilter, 
                        swtch->quadSplitWorkspaceBottomRight);
                    break;
                default:
                    /* shouldn't be here if accept_stream_frame was called */
                    assert(0);
                    return 0;
            }
            
            /* we don't send the quad split until complete_frame is called */
            return 1;
        }
        else if (swtch->currentStream->streamId == streamId)
        {
            return msk_receive_stream_frame(swtch->targetSink, swtch->firstInputStream->streamId, buffer, bufferSize);
        }

        /* shouldn't be here if accept_stream_frame was called */
        assert(0);
        return 0;
    }
    else
    {
        return msk_receive_stream_frame(swtch->targetSink, streamId, buffer, bufferSize);
    }
}

static int qvs_receive_stream_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;
    YUV_frame inputFrame;

    if (is_switchable_stream(swtch, streamId))
    {
        if (swtch->currentStream == swtch->quadSplitStream)
        {
            /* check the buffer size */
            if (swtch->quadSplitInputBufferSize != bufferSize)
            {
                ml_log_error("Buffer size (%d) != quad split data size (%d)\n", bufferSize, swtch->quadSplitInputBufferSize);
                return 0;
            }

            /* allocate the output buffer if we haven't done so already */
            if (!swtch->haveQuadSplitOutputBuffer)
            {
                CHK_ORET(msk_get_stream_buffer(swtch->targetSink, swtch->firstInputStream->streamId, 
                    bufferSize, &swtch->quadSplitOutputBuffer));
                    
                /* initialise the YUV_lib frame for the sink buffer */
                YUV_frame_from_buffer(&swtch->quadSplitFrameOut, swtch->quadSplitOutputBuffer, 
                    swtch->firstInputStream->streamInfo.width, swtch->firstInputStream->streamInfo.height, swtch->yuvFormat);

                /* always clear the frame, eg. to prevent video from non-quad showing through
                after a switch or a disabled OSD */
                clear_YUV_frame(&swtch->quadSplitFrameOut);
                
                swtch->haveQuadSplitOutputBuffer = 1;
            }
            
            /* initialise the input YUV frame. We know quarter_frame will not modify the buffer,
            so we can cast buffer to non-const */
            YUV_frame_from_buffer(&inputFrame, (void*)buffer, swtch->firstInputStream->streamInfo.width, 
                swtch->firstInputStream->streamInfo.height, swtch->yuvFormat);
                
            /* quad split */
            switch (get_quad_split_index(swtch, streamId))
            {
                /* top left */
                case 1:
                    quarter_frame(&inputFrame, 
                        &swtch->quadSplitFrameOut,
                        0, 
                        0, 
                        1, 
                        swtch->applyQuadSplitFilter, 
                        swtch->applyQuadSplitFilter, 
                        swtch->quadSplitWorkspaceTopLeft);
                    break;
                /* top right */
                case 2:
                    quarter_frame(&inputFrame, 
                        &swtch->quadSplitFrameOut, 
                        swtch->firstInputStream->streamInfo.width / 2, 
                        0,
                        1, 
                        swtch->applyQuadSplitFilter, 
                        swtch->applyQuadSplitFilter, 
                        swtch->quadSplitWorkspaceTopRight);
                    break;
                /* bottom left */    
                case 3:
                    quarter_frame(&inputFrame, 
                    &swtch->quadSplitFrameOut, 
                        0, 
                        swtch->firstInputStream->streamInfo.height / 2, 
                        1, 
                        swtch->applyQuadSplitFilter, 
                        swtch->applyQuadSplitFilter, 
                        swtch->quadSplitWorkspaceBottomLeft);
                    break;
                /* bottom right */    
                case 4:
                    quarter_frame(&inputFrame, 
                        &swtch->quadSplitFrameOut, 
                        swtch->firstInputStream->streamInfo.width / 2, 
                        swtch->firstInputStream->streamInfo.height / 2, 
                        1, 
                        swtch->applyQuadSplitFilter, 
                        swtch->applyQuadSplitFilter, 
                        swtch->quadSplitWorkspaceBottomRight);
                    break;
                default:
                    /* shouldn't be here if accept_stream_frame was called */
                    assert(0);
                    return 0;
            }
            
            /* we don't send the quad split until complete_frame is called */
            return 1;
        }
        else if (swtch->currentStream->streamId == streamId)
        {
            return msk_receive_stream_frame_const(swtch->targetSink, swtch->firstInputStream->streamId, buffer, bufferSize);
        }

        /* shouldn't be here if accept_stream_frame was called */
        assert(0);
        return 0;
    }
    else
    {
        return msk_receive_stream_frame_const(swtch->targetSink, streamId, buffer, bufferSize);
    }
}

static int qvs_complete_frame(void* data, const FrameInfo* frameInfo)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    /* resets */
    swtch->haveCheckedFirstInputStream = 0;
    swtch->haveAcceptedFirstInputStream = 0;
    swtch->haveQuadSplitOutputBuffer = 0;
    swtch->disableSwitching = 0;
    swtch->haveSwitched = 0;
    
    if (swtch->currentStream != NULL && swtch->currentStream == swtch->quadSplitStream)
    {
        /* send the quad split */
        if (!msk_receive_stream_frame(swtch->targetSink, swtch->firstInputStream->streamId, 
            swtch->quadSplitOutputBuffer, swtch->quadSplitInputBufferSize))
        {
            ml_log_error("Failed to send quad split to sink\n");
            return 0;
        }
    }
    
    return msk_complete_frame(swtch->targetSink, frameInfo);
}

static void qvs_cancel_frame(void* data)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    msk_cancel_frame(swtch->targetSink);
}

static OnScreenDisplay* qvs_get_osd(void* data)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    return msk_get_osd(swtch->targetSink);
}
    
static VideoSwitchSink* qvs_get_video_switch(void* data)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    return &swtch->switchSink;
}
    
static HalfSplitSink* qvs_get_half_split(void* data)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    return msk_get_half_split(swtch->targetSink);
}
    
static FrameSequenceSink* qvs_get_frame_sequence(void* data)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    return msk_get_frame_sequence(swtch->targetSink);
}
    
static int qvs_get_buffer_state(void* data, int* numBuffers, int* numBuffersFilled)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;

    return msk_get_buffer_state(swtch->targetSink, numBuffers, numBuffersFilled);
}
    
static void qvs_close(void* data)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;
    VideoStreamElement* ele;
    VideoStreamElement* nextEle;

    if (data == NULL)
    {
        return;
    }
    
    msk_close(swtch->targetSink);
    
    ele = swtch->streams.next;
    while (ele != NULL)
    {
        nextEle = ele->next;
        SAFE_FREE(&ele);
        ele = nextEle;
    }
    
    SAFE_FREE(&swtch->quadSplitInputBufferTopLeft);
    SAFE_FREE(&swtch->quadSplitInputBufferTopRight);
    SAFE_FREE(&swtch->quadSplitInputBufferBottomLeft);
    SAFE_FREE(&swtch->quadSplitInputBufferBottomRight);
    SAFE_FREE(&swtch->quadSplitWorkspaceTopLeft);
    SAFE_FREE(&swtch->quadSplitWorkspaceTopRight);
    SAFE_FREE(&swtch->quadSplitWorkspaceBottomLeft);
    SAFE_FREE(&swtch->quadSplitWorkspaceBottomRight);
    
    destroy_mutex(&swtch->nextCurrentStreamMutex);
    
    SAFE_FREE(&swtch);
}

static int qvs_reset_or_close(void* data)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;
    VideoStreamElement* ele;
    VideoStreamElement* nextEle;
    int result;

    ele = swtch->streams.next;
    while (ele != NULL)
    {
        nextEle = ele->next;
        SAFE_FREE(&ele);
        ele = nextEle;
    }
    memset(&swtch->streams, 0, sizeof(VideoStreamElement));
    
    swtch->firstInputStream = NULL;
    swtch->quadSplitStream = NULL;
    swtch->currentStream = NULL;
    
    swtch->nextCurrentStream = NULL;
    swtch->disableSwitching = 0;
    swtch->haveSwitched = 0;
    
    SAFE_FREE(&swtch->quadSplitInputBufferTopLeft);
    SAFE_FREE(&swtch->quadSplitInputBufferTopRight);
    SAFE_FREE(&swtch->quadSplitInputBufferBottomLeft);
    SAFE_FREE(&swtch->quadSplitInputBufferBottomRight);
    SAFE_FREE(&swtch->quadSplitWorkspaceTopLeft);
    SAFE_FREE(&swtch->quadSplitWorkspaceTopRight);
    SAFE_FREE(&swtch->quadSplitWorkspaceBottomLeft);
    SAFE_FREE(&swtch->quadSplitWorkspaceBottomRight);
    memset(&swtch->quadSplitFrameInTopLeft, 0, sizeof(YUV_frame));
    memset(&swtch->quadSplitFrameInTopRight, 0, sizeof(YUV_frame));
    memset(&swtch->quadSplitFrameInBottomLeft, 0, sizeof(YUV_frame));
    memset(&swtch->quadSplitFrameInBottomRight, 0, sizeof(YUV_frame));
    swtch->quadSplitOutputBuffer = NULL;
    
    swtch->haveCheckedFirstInputStream = 0;
    swtch->haveAcceptedFirstInputStream = 0;
    swtch->haveQuadSplitOutputBuffer = 0;
    
    result = msk_reset_or_close(swtch->targetSink);
    if (result != 1)
    {
        if (result == 2)
        {
            /* target sink was closed */
            swtch->targetSink = NULL;
        }
        goto fail;
    }
    
    return 1;
    
fail:
    qvs_close(data);
    return 2;
}


static MediaSink* qvs_get_media_sink(void* data)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;
    
    return &swtch->sink;
}

static int qvs_switch_next_video(void* data)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;
    
    if (swtch->nextCurrentStream == NULL)
    {
        return 0;
    }
    
    PTHREAD_MUTEX_LOCK(&swtch->nextCurrentStreamMutex);
    if (swtch->nextCurrentStream->next != NULL)
    {
        swtch->nextCurrentStream = swtch->nextCurrentStream->next;
    }
    else
    {
        swtch->nextCurrentStream = &swtch->streams;
    }
    PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentStreamMutex);
    
    msl_refresh_required(swtch->switchListener);
    
    return 1;
}

static int qvs_switch_prev_video(void* data)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;
    
    if (swtch->nextCurrentStream == NULL)
    {
        return 0;
    }
    
    PTHREAD_MUTEX_LOCK(&swtch->nextCurrentStreamMutex);
    if (swtch->nextCurrentStream->prev != NULL)
    {
        swtch->nextCurrentStream = swtch->nextCurrentStream->prev;
    }
    else
    {
        while (swtch->nextCurrentStream->next != NULL)
        {
            swtch->nextCurrentStream = swtch->nextCurrentStream->next;
        }
    }
    PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentStreamMutex);

    msl_refresh_required(swtch->switchListener);
    
    return 1;
}

static int qvs_switch_video(void* data, int index)
{
    QuadVideoSwitch* swtch = (QuadVideoSwitch*)data;
    VideoStreamElement* ele;
    int haveSwitched = 0;
    
    if (swtch->nextCurrentStream == NULL)
    {
        return 0;
    }
    
    PTHREAD_MUTEX_LOCK(&swtch->nextCurrentStreamMutex);
    ele = &swtch->streams;
    while (ele != NULL)
    {
        if (index == ele->index)
        {
            swtch->nextCurrentStream = ele;
            haveSwitched = 1;
            break;
        }
        ele = ele->next;
    }
    PTHREAD_MUTEX_UNLOCK(&swtch->nextCurrentStreamMutex);
    
    if (haveSwitched)
    {
        msl_refresh_required(swtch->switchListener);
    }
    
    return haveSwitched;
}



int qvs_create_video_switch(MediaSink* sink, int withQuadSplit, int applyQuadSplitFilter, VideoSwitchSink** swtch)
{
    QuadVideoSwitch* newQSSwtch;
    
    CALLOC_ORET(newQSSwtch, QuadVideoSwitch, 1);
    
    newQSSwtch->withQuadSplit = withQuadSplit;
    newQSSwtch->applyQuadSplitFilter = withQuadSplit && applyQuadSplitFilter;
    
    newQSSwtch->targetSink = sink;

    newQSSwtch->haveSwitched = 1; /* force clear of quad split frame */
    
    newQSSwtch->switchSink.data = newQSSwtch;
    newQSSwtch->switchSink.get_media_sink = qvs_get_media_sink;
    newQSSwtch->switchSink.switch_next_video = qvs_switch_next_video;
    newQSSwtch->switchSink.switch_prev_video = qvs_switch_prev_video;
    newQSSwtch->switchSink.switch_video = qvs_switch_video;
    
    newQSSwtch->targetSinkListener.data = newQSSwtch;
    newQSSwtch->targetSinkListener.frame_displayed = qvs_frame_displayed;
    newQSSwtch->targetSinkListener.frame_dropped = qvs_frame_dropped;
    newQSSwtch->targetSinkListener.refresh_required = qvs_refresh_required;
    
    newQSSwtch->sink.data = newQSSwtch;
    newQSSwtch->sink.register_listener = qvs_register_listener;
    newQSSwtch->sink.unregister_listener = qvs_unregister_listener;
    newQSSwtch->sink.accept_stream = qvs_accept_stream;
    newQSSwtch->sink.register_stream = qvs_register_stream;
    newQSSwtch->sink.accept_stream_frame = qvs_accept_stream_frame;
    newQSSwtch->sink.get_stream_buffer = qvs_get_stream_buffer;
    newQSSwtch->sink.receive_stream_frame = qvs_receive_stream_frame;
    newQSSwtch->sink.receive_stream_frame_const = qvs_receive_stream_frame_const;
    newQSSwtch->sink.complete_frame = qvs_complete_frame;
    newQSSwtch->sink.cancel_frame = qvs_cancel_frame;
    newQSSwtch->sink.get_osd = qvs_get_osd;
    newQSSwtch->sink.get_video_switch = qvs_get_video_switch;
    newQSSwtch->sink.get_half_split = qvs_get_half_split;
    newQSSwtch->sink.get_frame_sequence = qvs_get_frame_sequence;
    newQSSwtch->sink.get_buffer_state = qvs_get_buffer_state;
    newQSSwtch->sink.reset_or_close = qvs_reset_or_close;
    newQSSwtch->sink.close = qvs_close;
    
    CHK_OFAIL(init_mutex(&newQSSwtch->nextCurrentStreamMutex));
    
    
    *swtch = &newQSSwtch->switchSink;
    return 1;
    
fail:
    qvs_close(newQSSwtch);
    return 0;
}


MediaSink* vsw_get_media_sink(VideoSwitchSink* swtch)
{
    if (swtch && swtch->get_media_sink)
    {
        return swtch->get_media_sink(swtch->data);
    }
    return NULL;
}

int vsw_switch_next_video(VideoSwitchSink* swtch)
{
    if (swtch && swtch->switch_next_video)
    {
        return swtch->switch_next_video(swtch->data);
    }
    return 0;
}

int vsw_switch_prev_video(VideoSwitchSink* swtch)
{
    if (swtch && swtch->switch_prev_video)
    {
        return swtch->switch_prev_video(swtch->data);
    }
    return 0;
}

int vsw_switch_video(VideoSwitchSink* swtch, int index)
{
    if (swtch && swtch->switch_video)
    {
        return swtch->switch_video(swtch->data, index);
    }
    return 0;
}


