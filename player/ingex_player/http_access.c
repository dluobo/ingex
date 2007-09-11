#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/time.h>
#include <unistd.h>

#include "http_access.h"


#if !defined(HAVE_SHTTPD)

int hac_create_http_access(MediaPlayer* player, int port, HTTPAccess** access)
{
    return 0;
}

void hac_free_http_access(HTTPAccess** access)
{
    return;
}


#else

#include "http_access_resources.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"

#include <shttpd.h>


#define STATE_UPDATE_INTERVAL           40000


struct HTTPAccess
{
    MediaPlayerListener playerListener;
    MediaControl* control;
    
    HTTPAccessResources* resources;
    
    pthread_t httpThreadId;
    int stopped;
    pthread_mutex_t playerStateMutex;
    
    MediaPlayerStateEvent currentPlayerState;
    MediaPlayerStateEvent prevPlayerState;
    
    FrameInfo currentFrameInfo;
    FrameInfo prevFrameInfo;
    
    struct shttpd_ctx* ctx;
};


static int get_query_value(struct shttpd_arg* arg, const char* name, char* value, int valueSize)
{
    const char* queryString;
    value[0] = '\0';

    queryString = shttpd_get_env(arg, "QUERY_STRING");
    if (queryString == NULL)
    {
        return 0;
    }
    
    int result = shttpd_get_var(name, queryString, strlen(queryString), value, valueSize);
    if (result < 0 || result == valueSize)
    {
        return 0;
    }
    value[valueSize - 1] = '\0';
    
    return 1;
}


static void http_static_content(struct shttpd_arg* arg)
{
    HTTPAccess* access = (HTTPAccess*)arg->user_data;
	const char* requestURI;

	requestURI = shttpd_get_env(arg, "REQUEST_URI");
 
    const HTTPAccessResource* content = har_get_resource(access->resources, requestURI);
    if (content != NULL)
    {
        shttpd_printf(arg, "HTTP/1.1 200 OK\r\n");
        shttpd_printf(arg, "Content-Type: %s\r\n\r\n", content->contentType);
        
        /* TODO: don't assume content size < buffer size */
        memcpy(&arg->out.buf[arg->out.num_bytes], content->data, content->dataSize);
        arg->out.num_bytes += content->dataSize;
    }
    /* else TODO return error */
    
	arg->flags |= SHTTPD_END_OF_OUTPUT;
}

static void http_player_page(struct shttpd_arg* arg)
{
    HTTPAccess* access = (HTTPAccess*)arg->user_data;

    const HTTPAccessResource* playerPage = har_get_resource(access->resources, "player_page");
    if (playerPage != NULL)
    {
        shttpd_printf(arg, "HTTP/1.1 200 OK\r\n");
        shttpd_printf(arg, "Content-Type: %s\r\n\r\n", playerPage->contentType);
        
        shttpd_printf(arg, "%s", (const char*)playerPage->data);
    }
    /* else TODO return error */
    
	arg->flags |= SHTTPD_END_OF_OUTPUT;
}

static void http_player_state(struct shttpd_arg* arg)
{
    HTTPAccess* access = (HTTPAccess*)arg->user_data;

    shttpd_printf(arg, "HTTP/1.1 200 OK\r\n");
    shttpd_printf(arg, "Content-type: application/xml\r\n\r\n");
    
    shttpd_printf(arg, "<?xml version='1.0'?>\n");
    shttpd_printf(arg, "<player_state>\n");
	shttpd_printf(arg, "    <timecode>%02d:%02d:%02d:%02d</timecode>\n", 
        access->currentFrameInfo.timecodes[0].timecode.hour,
        access->currentFrameInfo.timecodes[0].timecode.min,
        access->currentFrameInfo.timecodes[0].timecode.sec,
        access->currentFrameInfo.timecodes[0].timecode.frame);
    shttpd_printf(arg, "</player_state>\n");

	arg->flags |= SHTTPD_END_OF_OUTPUT;
}

static void http_player_control(struct shttpd_arg* arg)
{
    HTTPAccess* access = (HTTPAccess*)arg->user_data;
	const char* requestURI;
    char queryValue[64];
    int64_t offset = 0;
    int whence = SEEK_SET;
    int speed = 1;
    PlayUnit unit = FRAME_PLAY_UNIT;
    int forward = 1;
    int queryOk;
    int queryValueCount;
    int pause = 0;
    

	requestURI = shttpd_get_env(arg, "REQUEST_URI");

    if (strcmp("/player/control/play", requestURI) == 0)
    {
        mc_play(access->control);
    }
    else if (strcmp("/player/control/stop", requestURI) == 0)
    {
        mc_stop(access->control);
    }
    else if (strcmp("/player/control/pause", requestURI) == 0)
    {
        mc_pause(access->control);
    }
    else if (strcmp("/player/control/toggle-play-pause", requestURI) == 0)
    {
        mc_toggle_play_pause(access->control);
    }
    else if (strcmp("/player/control/seek", requestURI) == 0)
    {
        queryOk = 1;
        queryValueCount = 0;
        
        if (get_query_value(arg, "offset", queryValue, sizeof(queryValue)))
        {
            queryValueCount++;
            offset = atol(queryValue);
        }
        if (get_query_value(arg, "whence", queryValue, sizeof(queryValue)))
        {
            queryValueCount++;
            if (strcmp("SEEK_SET", queryValue) == 0)
            {
                whence = SEEK_SET;
            }
            else if (strcmp("SEEK_CUR", queryValue) == 0)
            {
                whence = SEEK_CUR;
            }
            else if (strcmp("SEEK_END", queryValue) == 0)
            {
                whence = SEEK_END;
            }
            else
            {
                queryOk = 0;
            }
        }
        if (get_query_value(arg, "unit", queryValue, sizeof(queryValue)))
        {
            queryValueCount++;
            if (strcmp("FRAME_PLAY_UNIT", queryValue) == 0)
            {
                unit = FRAME_PLAY_UNIT;
            }
            else if (strcmp("PERCENTAGE_PLAY_UNIT", queryValue) == 0)
            {
                unit = PERCENTAGE_PLAY_UNIT;
            }
            else
            {
                queryOk = 0;
            }
        }
        if (get_query_value(arg, "pause", queryValue, sizeof(queryValue)))
        {
            if (strcmp("true", queryValue) == 0)
            {
                pause = 1;
            }
        }
        
        if (queryOk && queryValueCount == 3)
        {
            if (pause)
            {
                mc_pause(access->control);
            }
            mc_seek(access->control, offset, whence, unit);
        }
    }
    else if (strcmp("/player/control/play-speed", requestURI) == 0)
    {
        queryOk = 1;
        queryValueCount = 0;
        
        if (get_query_value(arg, "speed", queryValue, sizeof(queryValue)))
        {
            queryValueCount++;
            if ((speed = atoi(queryValue)) == 0)
            {
                queryOk = 0;
            }
        }
        if (get_query_value(arg, "unit", queryValue, sizeof(queryValue)))
        {
            queryValueCount++;
            if (strcmp("FRAME_PLAY_UNIT", queryValue) == 0)
            {
                unit = FRAME_PLAY_UNIT;
            }
            else if (strcmp("PERCENTAGE_PLAY_UNIT", queryValue) == 0)
            {
                unit = PERCENTAGE_PLAY_UNIT;
            }
            else
            {
                queryOk = 0;
            }
        }
        
        if (queryOk && queryValueCount == 2)
        {
            mc_play_speed(access->control, speed, unit);
        }
    }
    else if (strcmp("/player/control/step", requestURI) == 0)
    {
        queryOk = 1;
        queryValueCount = 0;
        
        if (get_query_value(arg, "forward", queryValue, sizeof(queryValue)))
        {
            queryValueCount++;
            if (strcmp("true", queryValue) == 0)
            {
                forward = 1;
            }
            else if (strcmp("false", queryValue) == 0)
            {
                forward = 0;
            }
            else
            {
                queryOk = 0;
            }
        }
        if (get_query_value(arg, "unit", queryValue, sizeof(queryValue)))
        {
            queryValueCount++;
            if (strcmp("FRAME_PLAY_UNIT", queryValue) == 0)
            {
                unit = FRAME_PLAY_UNIT;
            }
            else if (strcmp("PERCENTAGE_PLAY_UNIT", queryValue) == 0)
            {
                unit = PERCENTAGE_PLAY_UNIT;
            }
            else
            {
                queryOk = 0;
            }
        }
        
        if (queryOk && queryValueCount == 2)
        {
            mc_step(access->control, forward, unit);
        }
    }
    else if (strcmp("/player/control/home", requestURI) == 0)
    {
        if (get_query_value(arg, "pause", queryValue, sizeof(queryValue)))
        {
            if (strcmp("true", queryValue) == 0)
            {
                mc_pause(access->control);
            }
        }
        mc_seek(access->control, 0, SEEK_SET, FRAME_PLAY_UNIT);
    }
    else if (strcmp("/player/control/end", requestURI) == 0)
    {
        if (get_query_value(arg, "pause", queryValue, sizeof(queryValue)))
        {
            if (strcmp("true", queryValue) == 0)
            {
                mc_pause(access->control);
            }
        }
        mc_seek(access->control, 0, SEEK_END, FRAME_PLAY_UNIT);
    }
    else if (strcmp("/player/control/prev-mark", requestURI) == 0)
    {
        mc_seek_prev_mark(access->control);
    }
    else if (strcmp("/player/control/next-mark", requestURI) == 0)
    {
        mc_seek_next_mark(access->control);
    }
    else if (strcmp("/player/control/next-osd-screen", requestURI) == 0)
    {
        mc_next_osd_screen(access->control);
    }
    else if (strcmp("/player/control/next-osd-timecode", requestURI) == 0)
    {
        mc_next_osd_timecode(access->control);
    }
    
    shttpd_printf(arg, "HTTP/1.1 200 OK\r\n\r\n");
	arg->flags |= SHTTPD_END_OF_OUTPUT;
}


static void* http_thread(void* arg)
{
    HTTPAccess* access = (HTTPAccess*)arg;

    while (!access->stopped)
    {
        shttpd_poll(access->ctx, 1000);
    }
 
    shttpd_fini(access->ctx);
    
    pthread_exit((void*) 0);
}




static void hac_frame_displayed_event(void* data, const FrameInfo* frameInfo)
{
    HTTPAccess* access = (HTTPAccess*)data;
    
    PTHREAD_MUTEX_LOCK(&access->playerStateMutex);
    access->currentFrameInfo = *frameInfo;
    PTHREAD_MUTEX_UNLOCK(&access->playerStateMutex);
}

static void hac_frame_dropped_event(void* data, const FrameInfo* lastFrameInfo)
{
}

static void hac_state_change_event(void* data, const MediaPlayerStateEvent* event)
{
    HTTPAccess* access = (HTTPAccess*)data;
    
    PTHREAD_MUTEX_LOCK(&access->playerStateMutex);
    access->currentPlayerState = *event;
    PTHREAD_MUTEX_UNLOCK(&access->playerStateMutex);
}

static void hac_end_of_source_event(void* data, const FrameInfo* lastReadFrameInfo)
{
}

static void hac_start_of_source_event(void* data, const FrameInfo* firstReadFrameInfo)
{
}

static void hac_player_closed(void* data)
{
}


int hac_create_http_access(MediaPlayer* player, int port, HTTPAccess** access)
{
    HTTPAccess* newAccess;
    
    CALLOC_ORET(newAccess, HTTPAccess, 1);
    
    CHK_OFAIL(har_create_resources(&newAccess->resources));
    
    newAccess->control = ply_get_media_control(player);
    if (newAccess->control == NULL)
    {
        fprintf(stderr, "Media player has no control\n");
        goto fail;
    }

    newAccess->playerListener.data = newAccess;
    newAccess->playerListener.frame_displayed_event = hac_frame_displayed_event;
    newAccess->playerListener.frame_dropped_event = hac_frame_dropped_event;
    newAccess->playerListener.state_change_event = hac_state_change_event;
    newAccess->playerListener.end_of_source_event = hac_end_of_source_event;
    newAccess->playerListener.start_of_source_event = hac_start_of_source_event;
    newAccess->playerListener.player_closed = hac_player_closed;
    
    if (!ply_register_player_listener(player, &newAccess->playerListener))
    {
        fprintf(stderr, "Failed to register http access as player listener\n");
        goto fail;
    }
    
    CHK_OFAIL((newAccess->ctx = shttpd_init(NULL, "document_root", "/dev/null", NULL)) != NULL);
    shttpd_register_uri(newAccess->ctx, "/", &http_player_page, newAccess);
    shttpd_register_uri(newAccess->ctx, "/player.html", &http_player_page, newAccess);
    shttpd_register_uri(newAccess->ctx, "/index.html", &http_player_page, newAccess);
    shttpd_register_uri(newAccess->ctx, "/resources/*", &http_static_content, newAccess);
    shttpd_register_uri(newAccess->ctx, "/player/state", &http_player_state, newAccess);
    shttpd_register_uri(newAccess->ctx, "/player/control/*", &http_player_control, newAccess);
    CHK_OFAIL(shttpd_listen(newAccess->ctx, port, 0));

    
    CHK_OFAIL(init_mutex(&newAccess->playerStateMutex));
    CHK_OFAIL(create_joinable_thread(&newAccess->httpThreadId, http_thread, newAccess));
    
    
    *access = newAccess;
    return 1;
    
fail:
    hac_free_http_access(&newAccess);
    return 0;
}

void hac_free_http_access(HTTPAccess** access)
{
    if (*access == NULL)
    {
        return;
    }
    
    (*access)->stopped = 1;
    join_thread(&(*access)->httpThreadId, NULL, NULL);
    
    har_free_resources(&(*access)->resources);
    
    destroy_mutex(&(*access)->playerStateMutex);
    
    SAFE_FREE(access);
}


#endif


