#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

#include "media_player.h"
#include "shuttle_input_connect.h"
#include "keyboard_input_connect.h"
#include "term_keyboard_input.h"
#include "multiple_sources.h"
#include "raw_file_source.h"
#include "shared_mem_source.h"
#include "buffered_media_source.h"
#include "mxf_source.h"
#include "system_timecode_source.h"
#include "x11_xv_display_sink.h"
#include "x11_display_sink.h"
#include "dvs_sink.h"
#include "dual_sink.h"
#include "raw_file_sink.h"
#include "null_sink.h"
#include "buffered_media_sink.h"
#include "console_monitor.h"
#include "video_switch_sink.h"
#include "half_split_sink.h"
#include "frame_sequence_sink.h"
#include "audio_level_sink.h"
#include "qc_session.h"
#include "http_access.h"
#include "bouncing_ball_source.h"
#include "audio_sink.h"
#include "blank_source.h"
#include "version.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


#define MAX_INPUTS                  64

typedef enum
{
    UNKNOWN_OUTPUT = 0,
    DVS_OUTPUT,
    X11_AUTO_DISPLAY_OUTPUT,
    X11_XV_DISPLAY_OUTPUT,
    X11_DISPLAY_OUTPUT,
    DUAL_OUTPUT,
    RAW_STREAM_OUTPUT,
    NULL_OUTPUT,
} OutputType;

typedef enum
{
    UNKNOWN_INPUT = 0,
    RAW_INPUT,
    SHM_INPUT,
    MXF_INPUT,
    BBALLS_INPUT,
    BLANK_INPUT
} InputType;

typedef struct
{
    const char* filename;
    InputType type;
    
    /* raw files */
    StreamInfo streamInfo;
    
    /* bouncing balls */
    int numBalls;
    
    int disableAudio;
} InputInfo;

typedef struct
{
    MediaSink* mediaSink;
    MediaPlayer* mediaPlayer;
    MediaSource* mediaSource;
    ConsoleMonitor* consoleMonitor;
    QCSession* qcSession;
    HTTPAccess* httpAccess;
    FILE* bufferStateLogFile;
    
    X11DisplaySink* x11DisplaySink;
    X11XVDisplaySink* x11XVDisplaySink;
    DualSink* dualSink;
    DVSSink* dvsSink;
    
    X11WindowListener x11WindowListener;

    OutputType outputType;
    
    MarkConfigs markConfigs;
    
    ShuttleInput* shuttle;
    ShuttleConnect* shuttleConnect;
    ConnectMapping connectMapping;
	pthread_t shuttleThreadId;
    
    TermKeyboardInput* termKeyboardInput;
    KeyboardConnect* termKeyboardConnect;
	pthread_t termKeyboardThreadId;
} Player;

typedef struct
{
    const char* input;
    const char* description;
} ControlInputHelp;

/* TODO: shuttle and keyboard connects should hold this info */
static const ControlInputHelp g_defaultKeyboardInputHelp[] = 
{
    {"'q'", "Quit"},
    {"<SPACE>", "Toggle play/pause"},
    {"<HOME>", "Seek to start"},
    {"<END>", "Seek to end"},
    {"'i'", "Toggle lock"},
    {"'o'", "Display next OSD screen"},
    {"'t'", "Display next timecode"},
    {"'m'", "Set mark (type M0)"},
    {"'c'", "Clear mark"},
    {"'b'", "Clear all marks"},
    {"','", "Seek to previous mark"},
    {"'.'", "Seek to next mark"},
    {"'a'", "Review start"},
    {"'z'", "Review end"},
    {"'x'", "Review mark"},
    {"'j'", "Increment reverse play speed"},
    {"'k'", "Pause"},
    {"'l'", "Increment play speed"},
    {"'s'", "Toggle half split orientation"},
    {"'d'", "Toggle continuous split"},
    {"'f'", "Toggle show half split as black line"},
    {"'g'", "Move half split in left/upwards direction"},
    {"'h'", "Move half split in right/downwards direction"},
    {"1..9", "Switch to video #"},
    {"0", "Switch to quad-split video"},
    {"<RIGHT ARROW>", "Step forward"},
    {"<LEFT ARROW>", "Step backward"},
    {"<UP ARROW>", "Fast forward"},
    {"<DOWN ARROW>", "Fast rewind"},
    {"<PAGE UP>", "Forward 1 minute"},
    {"<PAGE DOWN>", "Backward 1 minute"}
};

static const ControlInputHelp g_defaultShuttleInputHelp[] = 
{
    {"1", "Toggle lock"},
    {"2", "Display next OSD screen"},
    {"3", "Display next timecode"},
//    {"4", ""},
    {"5", "Toggle play/pause"},
    {"6", "Seek to start"},
    {"7", "Seek to end"},
    {"8", "Clear all marks"},
    {"9", "Quit"},
    {"10", "Set mark (type M0)"},
    {"11", "Clear mark"},
    {"12", "Seek to previous mark"},
    {"13", "Seek to next mark"},
    {"14", "Switch to previous video"},
    {"15", "Switch to next video"},
    {"<SHUTTLE>", "Fast forward/rewind (clockwise/anti-clockwise)"},
    {"<JOG>", "Step forward/backward (clockwise/anti-clockwise)"}
};

static const ControlInputHelp g_qcKeyboardInputHelp[] = 
{
    {"'q'", "Quit"},
    {"<SPACE>", "Toggle play/pause"},
    {"<HOME>", "Seek to start"},
    {"<END>", "Seek to end"},
    {"'i'", "Toggle lock"},
    {"'o'", "Display next OSD screen"},
    {"'t'", "Display next timecode"},
    {"'m'", "Toggle mark red (type M0)"},
    {"'c'", "Clear mark (except D3 VTR error and PSE failure)"},
    {"'b'", "Clear all marks (except D3 VTR error and PSE failure)"},
    {"','", "Seek to previous mark"},
    {"'.'", "Seek to next mark"},
    {"'a'", "Review start"},
    {"'z'", "Review end"},
    {"'x'", "Review mark"},
    {"'j'", "Increment reverse play speed"},
    {"'k'", "Pause"},
    {"'l'", "Increment play speed"},
    {"'s'", "Toggle half split orientation"},
    {"'d'", "Toggle continuous split"},
    {"'f'", "Toggle show half split as black line"},
    {"'g'", "Move half split in left/upwards direction"},
    {"'h'", "Move half split in right/downwards direction"},
    {"1..9", "Switch to video #"},
    {"0", "Switch to quad-split video"},
    {"<RIGHT ARROW>", "Step forward"},
    {"<LEFT ARROW>", "Step backward"},
    {"<UP ARROW>", "Fast forward"},
    {"<DOWN ARROW>", "Fast rewind"},
    {"<PAGE UP>", "Forward 1 minute"},
    {"<PAGE DOWN>", "Backward 1 minute"}
};

static const ControlInputHelp g_qcShuttleInputHelp[] = 
{
    {"1", "Display next OSD screen"},
    {"2", "Display next timecode"},
    {"3", "Toggle lock"},
    {"4", "Quit after a 1.5 second hold"},
    {"5", "Toggle mark magenta (type M1)"},
    {"6", "Toggle mark green (type M2)"},
    {"7", "Toggle mark blue (type M3)"},
    {"8", "Toggle mark cyan (type M4)"},
    {"9", "Clear mark or clear all marks after a 1.5 second hold (except D3 VTR error and PSE failure)"},
    {"10", "Play"},
    {"11", "Review mark"},
    {"12", "Pause"},
    {"13", "Toggle mark red (type M0)"},
    {"14", "Seek to previous mark or seek to start after a 1.5 second hold"},
    {"15", "Seek to next mark or seek to end after a 1.5 second hold"},
    {"<SHUTTLE>", "Fast forward/rewind (clockwise/anti-clockwise)"},
    {"<JOG>", "Step forward/backward (clockwise/anti-clockwise)"}
};


static void* shuttle_control_thread(void* arg)
{
    Player* player = (Player*)arg;
    
    shj_start_shuttle(player->shuttle);
    
    pthread_exit((void *) 0);
}

static void* term_keyboard_control_thread(void* arg)
{
    Player* player = (Player*)arg;
    
    tki_start_term_keyboard(player->termKeyboardInput);
    
    pthread_exit((void *) 0);
}

static int start_control_threads(Player* player, int reviewDuration)
{
    int result;
    
    /* shuttle input connect */
    if (player->shuttle != NULL)
    {
        result = sic_create_shuttle_connect(
            reviewDuration, 
            ply_get_media_control(player->mediaPlayer), 
            player->shuttle, 
            player->connectMapping, 
            &player->shuttleConnect);
        if (result)
        {
            if (!create_joinable_thread(&player->shuttleThreadId, shuttle_control_thread, player)) 
            {
                shj_close_shuttle(&player->shuttle);
                sic_free_shuttle_connect(&player->shuttleConnect);
                player->shuttleThreadId = 0;
                return 0;
            }
        }
        else
        {
            ml_log_error("Failed to connect to shuttle input\n");
        }
    }

    
    /* terminal keyboard input */
    if (player->termKeyboardInput != NULL)
    {
        result = kic_create_keyboard_connect(
            reviewDuration, 
            ply_get_media_control(player->mediaPlayer),
            tki_get_keyboard_input(player->termKeyboardInput), 
            player->connectMapping, 
            &player->termKeyboardConnect);
        if (result)
        {
            if (!create_joinable_thread(&player->termKeyboardThreadId, term_keyboard_control_thread, player)) 
            {
                kic_free_keyboard_connect(&player->termKeyboardConnect);
                kip_close(tki_get_keyboard_input(player->termKeyboardInput));
                player->termKeyboardThreadId = 0;
                player->termKeyboardInput = NULL;
                return 0;
            }
        }
        else
        {
            ml_log_error("Failed to create terminal keyboard connect\n");
        }
    }

    return 1;
}

static void terminate_control_threads(Player* player)
{
    /* stop, join and free the shuttle */    
    if (player->shuttleThreadId != 0)
    {
        join_thread(&player->shuttleThreadId, player->shuttle, shj_stop_shuttle);
        sic_free_shuttle_connect(&player->shuttleConnect);
        shj_close_shuttle(&player->shuttle);
    }
    
    /* stop, join and free the terminal keyboard */    
    if (player->termKeyboardThreadId != 0)
    {
        join_thread(&player->termKeyboardThreadId, player->termKeyboardInput, tki_stop_term_keyboard);
        kic_free_keyboard_connect(&player->termKeyboardConnect);
        kip_close(tki_get_keyboard_input(player->termKeyboardInput));
        player->termKeyboardInput = NULL;
    }
}




Player g_player;


static void cleanup_exit(int res)
{
    Mark* marks = NULL;
    int numMarks = 0;
    
    /* reset signal handlers */
    if (signal(SIGINT, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGINT)");
    }
    if (signal(SIGTERM, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGTERM)");
    }
    if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGHUP)");
    }
    if (signal(SIGSEGV, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGSEGV)");
    }

    /* close in this order */
    
    msc_close(g_player.mediaSource);
    g_player.mediaSource = NULL;

    if (g_player.x11XVDisplaySink != NULL)
    {
        xvsk_unset_media_control(g_player.x11XVDisplaySink);
        xvsk_unregister_window_listener(g_player.x11XVDisplaySink, &g_player.x11WindowListener);
    }
    else if (g_player.x11DisplaySink != NULL)
    {
        xsk_unset_media_control(g_player.x11DisplaySink);
        xsk_unregister_window_listener(g_player.x11DisplaySink, &g_player.x11WindowListener);
    }
    else if (g_player.dualSink != NULL)
    {
        dusk_unset_media_control(g_player.dualSink);
        dusk_unregister_window_listener(g_player.dualSink, &g_player.x11WindowListener);
    }

    terminate_control_threads(&g_player);
    if (g_player.shuttle != NULL)
    {
        shj_close_shuttle(&g_player.shuttle);
    }        
    if (g_player.termKeyboardInput != NULL)
    {
        kip_close(tki_get_keyboard_input(g_player.termKeyboardInput));
        g_player.termKeyboardInput = NULL;
    }
    
        if (g_player.mediaPlayer != NULL)
    {
        numMarks = ply_get_marks(g_player.mediaPlayer, &marks);
        ply_close_player(&g_player.mediaPlayer);
    }

    msk_close(g_player.mediaSink);
    g_player.mediaSink = NULL;
    
    if (g_player.bufferStateLogFile != NULL)
    {
        fclose(g_player.bufferStateLogFile);
        g_player.bufferStateLogFile = NULL;
    }
        
    if (g_player.qcSession != NULL)
    {
        if (numMarks > 0)
        {
            qcs_write_marks(g_player.qcSession, &g_player.markConfigs, marks, numMarks);
            SAFE_FREE(&marks);
            numMarks = 0;
        }
        qcs_close(&g_player.qcSession);
    }

    if (g_player.consoleMonitor != NULL)
    {
        csm_close(&g_player.consoleMonitor);
    }
    
    hac_free_http_access(&g_player.httpAccess);

    ml_log_file_close();
    
    exit(res);
}

static void catch_sigint(int sig_number)
{
    /* reset signal handlers */
    if (signal(SIGINT, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGINT)");
    }
    if (signal(SIGTERM, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGTERM)");
    }
    if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGHUP)");
    }
    if (signal(SIGSEGV, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGSEGV)");
    }

    ml_log_info("Received signal %d\n", sig_number);
    
    /* make sure the terminal settings are restored */
    if (g_player.termKeyboardInput != NULL)
    {
        tki_restore_term_settings(g_player.termKeyboardInput);
    }
    
    /* make sure the DVS card is closed */
    dvs_close_card(g_player.dvsSink);
    
    exit(1);
}

static void x11_window_close_request(void* data)
{
    mc_stop(ply_get_media_control(g_player.mediaPlayer));
}

static void input_help()
{
    size_t i;
    fprintf(stderr, "\n");
    fprintf(stderr, "Control Inputs:\n");
    fprintf(stderr, "Default keyboard:\n");
    for (i = 0; i < sizeof(g_defaultKeyboardInputHelp) / sizeof(ControlInputHelp); i++)
    {
        fprintf(stderr, "  %-15s%s\n", g_defaultKeyboardInputHelp[i].input, g_defaultKeyboardInputHelp[i].description);
    }
    
    fprintf(stderr, "\n");
    fprintf(stderr, "Default shuttle:\n");
    for (i = 0; i < sizeof(g_defaultShuttleInputHelp) / sizeof(ControlInputHelp); i++)
    {
        fprintf(stderr, "  %-15s%s\n", g_defaultShuttleInputHelp[i].input, g_defaultShuttleInputHelp[i].description);
    }
    fprintf(stderr, "\n");

    fprintf(stderr, "\n");
    fprintf(stderr, "QC keyboard:\n");
    for (i = 0; i < sizeof(g_qcKeyboardInputHelp) / sizeof(ControlInputHelp); i++)
    {
        fprintf(stderr, "  %-15s%s\n", g_qcKeyboardInputHelp[i].input, g_qcKeyboardInputHelp[i].description);
    }
    fprintf(stderr, "\n");
    
    fprintf(stderr, "\n");
    fprintf(stderr, "QC shuttle:\n");
    for (i = 0; i < sizeof(g_qcShuttleInputHelp) / sizeof(ControlInputHelp); i++)
    {
        fprintf(stderr, "  %-15s%s\n", g_qcShuttleInputHelp[i].input, g_qcShuttleInputHelp[i].description);
    }
    fprintf(stderr, "\n");
}

static int parse_config_marks(const char* val, MarkConfigs* markConfigs)
{
    const char* configSep = val - 1; /* will + 1 below */
    int i;
    const char* typeStr;
    const char* nameStr;
    const char* endNameStr;
    const char* colourStr;
    int typeBit;
    struct ColourInfo
    {
        const char* name;
        Colour colour;
    } colourInfos[] = 
    {
        {"white", WHITE_COLOUR},
        {"yellow", YELLOW_COLOUR},
        {"cyan", CYAN_COLOUR},
        {"green", GREEN_COLOUR},
        {"magenta", MAGENTA_COLOUR},
        {"red", RED_COLOUR},
        {"blue", BLUE_COLOUR},
        {"orange", ORANGE_COLOUR}
    };
    
    if (val == NULL || val[0] == '\0')
    {
        return 0;
    }
    
    markConfigs->numConfigs = 0;
    
    do
    {
        /* prepare parse type,name,colour */
        typeStr = configSep + 1;
        nameStr = strchr(typeStr, ',');
        if (nameStr == NULL)
        {
            fprintf(stderr, "Missing mark name\n");
            return 0;
        }
        nameStr += 1;
        while (*nameStr == ' ')
        {
            nameStr++;
        }
        colourStr = strchr(nameStr, ',');
        if (colourStr == NULL)
        {
            fprintf(stderr, "Missing mark colour\n");
            return 0;
        }
        endNameStr = colourStr;
        while (endNameStr != nameStr && *endNameStr == ' ')
        {
            endNameStr--;
        }
        if (nameStr == endNameStr)
        {
            fprintf(stderr, "Empty name string\n");
            return 0;
        }
        colourStr += 1;
        while (*colourStr == ' ')
        {
            colourStr++;
        }
        
        /* parse type */
        if (sscanf(typeStr, "%d", &typeBit) != 1 || typeBit < 1 || typeBit > 32)
        {
            fprintf(stderr, "Invalid mark type\n");
            return 0;
        }
        markConfigs->configs[markConfigs->numConfigs].type = 1 << (typeBit - 1);
        
        /* copy name */
        strncpy(markConfigs->configs[markConfigs->numConfigs].name, nameStr,
            ((endNameStr - nameStr) > 32) ? 32 : (endNameStr - nameStr));
            
        /* parse colour */
        for (i = 0; i < (int)(sizeof(colourInfos) / sizeof(struct ColourInfo)); i++)
        {
            if (strncmp(colourInfos[i].name, colourStr, strlen(colourInfos[i].name)) == 0)
            {
                markConfigs->configs[markConfigs->numConfigs].colour = colourInfos[i].colour;
                break;
            }
        }
        if (i == sizeof(colourInfos) / sizeof(struct ColourInfo))
        {
            fprintf(stderr, "Failed to parse colour\n");
            return 0;
        }
        
        markConfigs->numConfigs++;
        if (markConfigs->numConfigs > 32)
        {
            fprintf(stderr, "Too many mark configs - only maximum 32 allowed\n");
            return 0;
        }
    }
    while ((configSep = strchr(configSep + 1, ':')) != NULL);
    
    return 1;
}

int complete_source_info(InputInfo* info)
{
    if (info->streamInfo.type == UNKNOWN_STREAM_TYPE)
    {
        info->streamInfo.type = PICTURE_STREAM_TYPE;
    }
    
    if (info->streamInfo.type == PICTURE_STREAM_TYPE)
    {
        if (info->streamInfo.format == UNKNOWN_FORMAT)
        {
            info->streamInfo.format = UYVY_FORMAT;
        }
        
        if (info->streamInfo.width < 1 || info->streamInfo.height < 1)
        {
            info->streamInfo.width = 720;
            info->streamInfo.height = 576;
        }
        
        if (info->streamInfo.frameRate.num < 1 || info->streamInfo.frameRate.num < 1)
        {
            info->streamInfo.frameRate = g_palFrameRate;
        }
        
        if (info->streamInfo.aspectRatio.num < 1 || info->streamInfo.aspectRatio.num < 1)
        {
            info->streamInfo.aspectRatio.num = 4;
            info->streamInfo.aspectRatio.den = 3;
        }
    }
    else if (info->streamInfo.type == SOUND_STREAM_TYPE)
    {
        /* TODO: frame rate should be determined by other inputs is possible */
        if (info->streamInfo.frameRate.num < 1 ||
            info->streamInfo.frameRate.den < 1)
        {
            info->streamInfo.frameRate = g_palFrameRate;
        }
        
        if (info->streamInfo.samplingRate.num < 1 ||
            info->streamInfo.samplingRate.den < 1)
        {
            info->streamInfo.samplingRate.num = 48000;
            info->streamInfo.samplingRate.den = 1;
        }
        
        if (info->streamInfo.numChannels < 1)
        {
            info->streamInfo.numChannels = 1;
        }
        
        if (info->streamInfo.bitsPerSample < 1)
        {
            info->streamInfo.bitsPerSample = 16;
        }
    }
    else if (info->streamInfo.type == TIMECODE_STREAM_TYPE)
    {
        if (info->streamInfo.timecodeType == UNKNOWN_TIMECODE_TYPE)
        {
            info->streamInfo.timecodeType = SYSTEM_TIMECODE_TYPE;
        }

        if (info->streamInfo.frameRate.num < 1 || info->streamInfo.frameRate.num < 1)
        {
            info->streamInfo.frameRate = g_palFrameRate;
        }
    }
    else
    {
        return 0;
    }
    
    return 1;
}

static void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s [options] [inputs]\n", cmd);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help               Display this usage message plus keyboard and shuttle input help\n");
    fprintf(stderr, "  -v, --version            Display the player version\n");
    fprintf(stderr, "  --log-file <name>        Output log messages to file\n");
    fprintf(stderr, "  --log-level <level>      Output log level; 0=debug, 1=info, 2=warning, 3=error (default %d)\n", DEBUG_LOG_LEVEL);
    fprintf(stderr, "  --log-buf <name>         Log source and sink buffer state to file\n");
#if defined(HAVE_DVS)    
    fprintf(stderr, "  --dvs                    SDI ouput using the DVS card\n");
    fprintf(stderr, "  --dvs-buf <size>         Size of the DVS buffer (default is 12; must be >= 4; 0 means maximim)\n");
    fprintf(stderr, "  --dual                   Dual sink with both DVS card output and X server display output\n");
    fprintf(stderr, "  --disable-sdi-osd        Disable the OSD on the SDI output\n");
    fprintf(stderr, "  --ltc-as-vitc            Outputs the source LTC to the SDI output VITC\n");
    fprintf(stderr, "  --count-as-vitc          Outputs the frame count to the SDI output VITC\n");
    fprintf(stderr, "  --extra-vitc             Outputs extra SDI VITC lines\n");
    fprintf(stderr, "  --extra-ltc_as_vitc      Outputs the source LTC to the extra SDI VITC lines\n");
    fprintf(stderr, "  --extra-count-as-vitc    Outputs the frame count to the extra SDI VITC lines\n");
    fprintf(stderr, "  --disable-fit-video      Disable fitting the video to the SDI raster\n");
#endif    
    fprintf(stderr, "  --xv                     X11 Xv extension display output (YUV colourspace)\n");
    fprintf(stderr, "  --x11                    X11 display output (RGB colourspace)\n");
    fprintf(stderr, "  --disable-x11-osd        Disable the OSD on the X11 or X11 Xv output\n");
    fprintf(stderr, "  --raw-out <template>     Raw stream output files. Template must contain '%%d'\n");
    fprintf(stderr, "  --null-out               Accepts streams and does nothing\n");
    fprintf(stderr, "  --svitc <timecode>       Start at specified VITC timecode (hh:mm:ss:ff)\n");
    fprintf(stderr, "  --sltc <timecode>        Start at specified LTC timecode (hh:mm:ss:ff)\n");
    fprintf(stderr, "  --video-switch           Use video switcher to switch between multiple video streams\n");
    fprintf(stderr, "  --quad-split             Add a quad split view to the video switch (--video-switch is also set)\n");
    fprintf(stderr, "  --no-quad-filter         Don't apply horizontal and vertical filtering in quad-split\n");
    fprintf(stderr, "  --lock                   Start with lock on\n");
    fprintf(stderr, "  --fthreads <num>         Number if FFMPEG threads to use when decoding (default = 4)\n");
    fprintf(stderr, "  --wthreads               Use worker threads for decoding essence\n");
    fprintf(stderr, "  --qc-control             Use the QC media control mappings\n");
    fprintf(stderr, "  --qc-session <prefix>        Log quality control session to file with filename <prefix>_qclog_yyyymmdd_hhmmss.txt\n");
    fprintf(stderr, "  --exit-at-end            Close the player when the end of stream is reached\n");
    fprintf(stderr, "  --src-buf  <size>        Size of the media source buffer (default is 0)\n");
    fprintf(stderr, "  --force-d3-mxf           (Use only for MXF inputs using deprecated keys). Force the treatment of the MXF inputs to be BBC D3 MXF\n");
    fprintf(stderr, "  --mark-pse-fails         Add marks for PSE failures recorded in the D3 MXF file\n");
    fprintf(stderr, "  --mark-vtr-errors        Add marks for VTR playback errors recorded in the D3 MXF file\n");
    fprintf(stderr, "  --pixel-aspect <W:H>     Video pixel aspect ratio of the display (default uses screen resolution and assumes a 4:3 monitor)\n");
    fprintf(stderr, "  --monitor-aspect <W:H>   Pixel aspect ratio is calculated using the screen resolution and this monitor aspect ratio\n");
    fprintf(stderr, "  --source-aspect <W:H>    Force the video aspect ratio (currently only works for the X11 Xv extension output)\n");
    fprintf(stderr, "  --scale <float>          Scale the video (currently only works for the X11 Xv extension output)\n");
    fprintf(stderr, "  --loop                   Seek back to start when the last frame is reached\n");
    fprintf(stderr, "  --review-dur <sec>       Review duration in seconds (default 20 seconds)\n");
    fprintf(stderr, "  --half-split             A video switch with a half split view\n");
    fprintf(stderr, "  --audio-mon <num>        Display audio levels for first <num> audio streams (maximum 16)\n");
    fprintf(stderr, "  --show-field-symbol      Show the field symbol for the even fields in the OSD\n");
    fprintf(stderr, "  --restore-marks <file>   Restore marks from the QC session file\n");
    fprintf(stderr, "  --config-marks <val>     Configure how marks are named and displayed\n");
    fprintf(stderr, "                               val = \"type,name,colour(:type,name,colour)*\", where\n");
    fprintf(stderr, "                                  type is 1...32 (bit position in the 32-bit value used internally),\n");
    fprintf(stderr, "                                  name is a string with maximum length 31 and\n");
    fprintf(stderr, "                                  colour is one of white|yellow|cyan|green|magenta|red|blue|orange\n");
#if defined(HAVE_SHTTPD)    
    fprintf(stderr, "  --http-access <port>     Player access via http <port>\n");
#endif    
    fprintf(stderr, "  --frame-sequence         Show sequence of frames in each frame\n");         
#if defined(HAVE_PORTAUDIO)    
    fprintf(stderr, "  --disable-pc-audio       Disable audio output to the PC sound devices\n");
    fprintf(stderr, "  --audio-dev <num>        Select an audio device (default is audio device 0)\n");
#endif    
    fprintf(stderr, "  --hide-progress-bar      Don't show the progress bar shown in the OSD\n");
    fprintf(stderr, "  --audio-lineup <level>   Audio line-up level in dBFS (default -18.0)\n");
    fprintf(stderr, "  --enable-term-keyb       Enable terminal window keyboard input.\n");
    fprintf(stderr, "  --show-tc <type>.<index> Initially show the <index> (starting from 0) stream with specified timecode <type> (default is the first timecode stream)\n");
    fprintf(stderr, "                           Options are:\n");
    fprintf(stderr, "                               ALL: all timecodes types\n");
    fprintf(stderr, "                               CTS: control timecode\n");
    fprintf(stderr, "                               VITC: vertical interval timecode\n");
    fprintf(stderr, "                               LTC: linear timecode\n");
    fprintf(stderr, "                           Examples:\n");
    fprintf(stderr, "                               ALL.3: show the 4th timecode stream\n");
    fprintf(stderr, "                               VITC.1: show the 2nd VITC timecode stream\n");
    fprintf(stderr, "  --disable-console-mon    Disable player monitoring via console.\n");
    fprintf(stderr, "  --src-rate-limit <value> Limit the byte rate from the bufferred source. <value> unit is bytes/sec\n");
    fprintf(stderr, "                           Only works when --src-buf is used\n");
    fprintf(stderr, "                           Note: only takes bytes passed to the sink into account and this can\n");
    fprintf(stderr, "                               be less than the number of bytes read from disk\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Inputs:\n");
    fprintf(stderr, "  -m, --mxf  <file>        MXF file input\n");
    fprintf(stderr, "  --system-tc <timecode>   Add a system timecode with given start timecode (hh:mm:ss:ff or 'now')\n");
    fprintf(stderr, "  --max-length <dur>       Limit the source length played (hh:mm:ss:ff)\n");
    fprintf(stderr, "  --src-format <format>    Source video format: uyvy, yuv420, yuv422, yuv444, pcm, timecode (default uyvy)\n");
    fprintf(stderr, "  --src-size <WxH>         Width and height for source video input (default is 720x576)\n");
    fprintf(stderr, "  --src-bps <num>          Audio bits per sample (default 16)\n");
    fprintf(stderr, "  --raw-in  <file>         Raw file input\n");
    fprintf(stderr, "  --balls <num>            Bouncing balls\n");
    fprintf(stderr, "  --blank                  Blank video source\n");
#if !defined(DISABLE_SHARED_MEM_SOURCE)
    fprintf(stderr, "  --shm-in <id>            Shared memory source. <id> is '(card number)p|s'. Eg. '0p'\n");
    fprintf(stderr, "                               card = the SDI input card number (typically 1..4)\n");
    fprintf(stderr, "                               p = select the primary input format\n");
    fprintf(stderr, "                               s = select the secondary input format\n");
#endif
    fprintf(stderr, "  --disable-audio          Disable audio from the next input\n");
    fprintf(stderr, "\n");
}

int main(int argc, const char **argv)
{
    int cmdlnIndex = 1;
    InputInfo inputs[MAX_INPUTS];
    int numInputs = 0;
    OutputType outputType = X11_AUTO_DISPLAY_OUTPUT;
    OutputType xOutputType = X11_AUTO_DISPLAY_OUTPUT;
    MediaSource* mediaSource = NULL;
    int i;
    const char* rawFilenameTemplate = NULL;
    Timecode startVITCTimecode = g_invalidTimecode;
    Timecode startLTCTimecode = g_invalidTimecode;
    int addVideoSwitch = 0;
    int addQuadSplitToVideoSwitch = 0;
    VideoSwitchSink* videoSwitch = NULL;
    const char* qcSessionFilename = NULL;
    const char* logFilename = NULL;
    int lock = 0;
    int numFFMPEGThreads = 4;
    int qcControl = 0;
    BufferedMediaSink* bufSink = NULL;
    int dvsBufferSize = 12;
    int useWorkerThreads = 0;
    int closeAtEnd = 0;
    struct timeval startTime;
    struct timeval endTime;
    time_t timeDiffSec;
    int timeDiffUSec;
    int applyQuadSplitFilter = 1;
    int srcBufferSize = 0;
    MultipleMediaSources* multipleSource = NULL;
    BufferedMediaSource* bufferedSource = NULL;
    MXFFileSource* mxfSource = NULL;
    int disableSDIOSD = 0;
    int disableX11OSD = 0;
    int logLevel = DEBUG_LOG_LEVEL;
    int forceD3MXFInput = 0;
    int markPSEFails = 0;
    int markVTRErrors = 0;
    Rational pixelAspectRatio = {0, 0};
    Rational monitorAspectRatio = {0, 0};
    Rational sourceAspectRatio = {0, 0};
    float scale = 0.0;
    SDIVITCSource sdiVITCSource = VITC_AS_SDI_VITC;
    int loop = 0;
    int extraSDIVITCSource = 0; 
    int64_t systemStartTimecode = -1;
    int reviewDuration = 20;
    HalfSplitSink* halfSplitSink = NULL;
    int halfSplit = 0;
    AudioLevelSink* audioLevelSink = NULL;
    int audioLevelStreams = -1;
    int showFieldSymbol = 0;
    const char* restoreMarksFilename = NULL;
#if defined(HAVE_SHTTPD)    
    int httpPort = -1;
#endif
    MarkConfigs markConfigs;
    int64_t maxLength = -1;
    int showFrameSequence = 0;
    FrameSequenceSink* frameSequenceSink = NULL;
    const char* bufferStateLogFilename = NULL;
    int fitVideo = 1;
#if defined(HAVE_PORTAUDIO)    
    int disablePCAudio = 0;
    int audioDevice = -1;
#endif
    int hideProgressBar = 0;
    float audioLineupLevel = -18.0;
    int enableTermKeyboard = 0;
    int startTimecodeType = -1;
    int startTimecodeSubType = -1;
    int startTimecodeIndex = -1;
    int disableConsoleMonitor = 0;
    float srcRateLimit = -1.0;
    
    
    memset(inputs, 0, sizeof(inputs));
    memset(&markConfigs, 0, sizeof(markConfigs));
    
    
    while (cmdlnIndex < argc)
    {
        if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
            strcmp(argv[cmdlnIndex], "--help") == 0)
        {
            usage(argv[0]);
            input_help();
            return 0;
        }
        if (strcmp(argv[cmdlnIndex], "-v") == 0 ||
            strcmp(argv[cmdlnIndex], "--version") == 0)
        {
            printf("Version: %s, build: %s\n", get_player_version(), get_player_build_timestamp());
            cmdlnIndex += 1;
            if (cmdlnIndex == argc)
            {
                /* exit if this was the only parameter */
                return 0;
            }
        }
        else if (strcmp(argv[cmdlnIndex], "--log-file") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            logFilename = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--log-level") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d\n", &logLevel) != 1)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--log-buf") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            bufferStateLogFilename = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
#if defined(HAVE_DVS)    
        else if (strcmp(argv[cmdlnIndex], "--dvs") == 0)
        {
            outputType = DVS_OUTPUT;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--dvs-buf") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d\n", &dvsBufferSize) != 1 ||
                (dvsBufferSize != 0 && dvsBufferSize < 4))
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--dual") == 0)
        {
            outputType = DUAL_OUTPUT;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--disable-sdi-osd") == 0)
        {
            disableSDIOSD = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--ltc-as-vitc") == 0)
        {
            sdiVITCSource = LTC_AS_SDI_VITC;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--count-as-vitc") == 0)
        {
            sdiVITCSource = COUNT_AS_SDI_VITC;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--extra-vitc") == 0)
        {
            extraSDIVITCSource = VITC_AS_SDI_VITC;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--extra-ltc-as-vitc") == 0)
        {
            extraSDIVITCSource = LTC_AS_SDI_VITC;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--extra-count-as-vitc") == 0)
        {
            extraSDIVITCSource = COUNT_AS_SDI_VITC;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--disable-fit-video") == 0)
        {
            fitVideo = 0;
            cmdlnIndex += 1;
        }
#endif        
        else if (strcmp(argv[cmdlnIndex], "--xv") == 0)
        {
            xOutputType = X11_XV_DISPLAY_OUTPUT;
#if defined(HAVE_DVS)    
            if (outputType != DUAL_OUTPUT)
            {
#endif                
                outputType = X11_XV_DISPLAY_OUTPUT;
#if defined(HAVE_DVS)    
            }
#endif            
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--x11") == 0)
        {
            xOutputType = X11_DISPLAY_OUTPUT;
#if defined(HAVE_DVS)    
            if (outputType != DUAL_OUTPUT)
            {
#endif                
                outputType = X11_DISPLAY_OUTPUT;
#if defined(HAVE_DVS)    
            }
#endif            
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--disable-x11-osd") == 0)
        {
            disableX11OSD = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--raw-out") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            outputType = RAW_STREAM_OUTPUT;
            rawFilenameTemplate = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--null-out") == 0)
        {
            outputType = NULL_OUTPUT;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--svitc") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            int value[4];
            if (sscanf(argv[cmdlnIndex + 1], "%u:%u:%u:%u", &value[0], &value[1], &value[2], &value[3]) < 4)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            startVITCTimecode.isDropFrame = 0;
            startVITCTimecode.hour = value[0];
            startVITCTimecode.min = value[1];
            startVITCTimecode.sec = value[2];
            startVITCTimecode.frame = value[3];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--sltc") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            int value[4];
            if (sscanf(argv[cmdlnIndex + 1], "%u:%u:%u:%u", &value[0], &value[1], &value[2], &value[3]) < 4)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            startLTCTimecode.isDropFrame = 0;
            startLTCTimecode.hour = value[0];
            startLTCTimecode.min = value[1];
            startLTCTimecode.sec = value[2];
            startLTCTimecode.frame = value[3];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--video-switch") == 0)
        {
            addVideoSwitch = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--quad-split") == 0)
        {
            addVideoSwitch = 1;
            addQuadSplitToVideoSwitch = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--no-quad-filterr") == 0)
        {
            applyQuadSplitFilter = 0;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--qc-session") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            qcSessionFilename = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--lock") == 0)
        {
            lock = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--fthreads") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d\n", &numFFMPEGThreads) != 1 ||
                numFFMPEGThreads < 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--wthreads") == 0)
        {
            useWorkerThreads = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--qc-control") == 0)
        {
            qcControl = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--exit-at-end") == 0)
        {
            closeAtEnd = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--src-buf") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d\n", &srcBufferSize) != 1 || srcBufferSize < 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--force-d3-mxf") == 0)
        {
            forceD3MXFInput = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--mark-pse-fails") == 0)
        {
            markPSEFails = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--mark-vtr-errors") == 0)
        {
            markVTRErrors = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--pixel-aspect") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%u:%u", &pixelAspectRatio.num, &pixelAspectRatio.den) != 2 ||
                pixelAspectRatio.num <= 0 || pixelAspectRatio.den <= 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid value for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--monitor-aspect") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%u:%u", &monitorAspectRatio.num, &monitorAspectRatio.den) != 2 ||
                monitorAspectRatio.num <= 0 || monitorAspectRatio.den <= 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid value for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--source-aspect") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%u:%u", &sourceAspectRatio.num, &sourceAspectRatio.den) != 2 ||
                sourceAspectRatio.num <= 0 || sourceAspectRatio.den <= 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--scale") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%f", &scale) != 1 || scale <= 0.0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--loop") == 0)
        {
            loop = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--review-dur") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d\n", &reviewDuration) != 1 || reviewDuration <= 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--half-split") == 0)
        {
            halfSplit = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--audio-mon") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d", &audioLevelStreams) != 1 || 
                audioLevelStreams < 0 || audioLevelStreams > 16)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--show-field-symbol") == 0)
        {
            showFieldSymbol = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--restore-marks") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            restoreMarksFilename = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--config-marks") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (!parse_config_marks(argv[cmdlnIndex + 1], &markConfigs))
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
#if defined(HAVE_SHTTPD)    
        else if (strcmp(argv[cmdlnIndex], "--http-access") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d\n", &httpPort) != 1 || httpPort < 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
#endif        
        else if (strcmp(argv[cmdlnIndex], "--frame-sequence") == 0)
        {
            showFrameSequence = 1;
            cmdlnIndex += 1;
        }
#if defined(HAVE_PORTAUDIO)    
        else if (strcmp(argv[cmdlnIndex], "--disable-pc-audio") == 0)
        {
            disablePCAudio = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--audio-dev") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d", &audioDevice) != 1)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
#endif        
        else if (strcmp(argv[cmdlnIndex], "--hide-progress-bar") == 0)
        {
            hideProgressBar = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--enable-term-keyb") == 0)
        {
            enableTermKeyboard = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--show-tc") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (strncmp(argv[cmdlnIndex + 1], "ALL.", strlen("ALL.")) == 0 &&
                sscanf(argv[cmdlnIndex + 1] + strlen("ALL."), "%d", &startTimecodeIndex) == 1)
            {
                startTimecodeType = -1;
                startTimecodeSubType = -1;
            }
            else if (strncmp(argv[cmdlnIndex + 1], "CTC.", strlen("CTC.")) == 0 &&
                sscanf(argv[cmdlnIndex + 1] + strlen("CTC."), "%d", &startTimecodeIndex) == 1)
            {
                startTimecodeType = CONTROL_TIMECODE_TYPE;
                startTimecodeSubType = NO_TIMECODE_SUBTYPE;
            }
            else if (strncmp(argv[cmdlnIndex + 1], "VITC.", strlen("VITC.")) == 0 &&
                sscanf(argv[cmdlnIndex + 1] + strlen("VITC."), "%d", &startTimecodeIndex) == 1)
            {
                startTimecodeType = SOURCE_TIMECODE_TYPE;
                startTimecodeSubType = VITC_SOURCE_TIMECODE_SUBTYPE;
            }
            else if (strncmp(argv[cmdlnIndex + 1], "LTC.", strlen("LTC.")) == 0 &&
                sscanf(argv[cmdlnIndex + 1] + strlen("LTC."), "%d", &startTimecodeIndex) == 1)
            {
                startTimecodeType = SOURCE_TIMECODE_TYPE;
                startTimecodeSubType = LTC_SOURCE_TIMECODE_SUBTYPE;
            }
            else
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--disable-console-mon") == 0)
        {
            disableConsoleMonitor = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--src-rate-limit") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%f", &srcRateLimit) != 1 || srcRateLimit <= 0.0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "-m") == 0 ||
            strcmp(argv[cmdlnIndex], "--mxf") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            inputs[numInputs].type = MXF_INPUT;
            inputs[numInputs].filename = argv[cmdlnIndex + 1];
            numInputs++;
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--audio-lineup") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%f", &audioLineupLevel) != 1)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--system-tc") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            int value[4];
            if (strcmp("now", argv[cmdlnIndex + 1]) == 0)
            {
                systemStartTimecode = get_timecode_now();
            }
            else
            {
                if (sscanf(argv[cmdlnIndex + 1], "%u:%u:%u:%u", &value[0], &value[1], &value[2], &value[3]) < 4)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                systemStartTimecode = value[0] * 60 * 60 * 25 + 
                    value[1] * 60 * 25 + value[2] * 25 + value[3];
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--max-length") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            int value[4];
            if (sscanf(argv[cmdlnIndex + 1], "%u:%u:%u:%u", &value[0], &value[1], &value[2], &value[3]) < 4)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            maxLength = value[0] * 60 * 60 * 25 + 
                value[1] * 60 * 25 + value[2] * 25 + value[3];
            cmdlnIndex += 2;
        }
#if !defined(DISABLE_SHARED_MEM_SOURCE)        
        else if (strcmp(argv[cmdlnIndex], "--shm-in") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            inputs[numInputs].type = SHM_INPUT;
            inputs[numInputs].filename = argv[cmdlnIndex + 1];

            numInputs++;
            cmdlnIndex += 2;
        }
#endif        
        else if (strcmp(argv[cmdlnIndex], "--src-format") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (strcmp(argv[cmdlnIndex + 1], "uyvy") == 0) 
            {
                inputs[numInputs].streamInfo.type = PICTURE_STREAM_TYPE;
                inputs[numInputs].streamInfo.format = UYVY_FORMAT;
            }
            else if (strcmp(argv[cmdlnIndex + 1], "yuv420") == 0) 
            {
                inputs[numInputs].streamInfo.type = PICTURE_STREAM_TYPE;
                inputs[numInputs].streamInfo.format = YUV420_FORMAT;
            }
            else if (strcmp(argv[cmdlnIndex + 1], "yuv422") == 0) 
            {
                inputs[numInputs].streamInfo.type = PICTURE_STREAM_TYPE;
                inputs[numInputs].streamInfo.format = YUV422_FORMAT;
            }
            else if (strcmp(argv[cmdlnIndex + 1], "yuv444") == 0) 
            {
                inputs[numInputs].streamInfo.type = PICTURE_STREAM_TYPE;
                inputs[numInputs].streamInfo.format = YUV444_FORMAT;
            }
            else if (strcmp(argv[cmdlnIndex + 1], "pcm") == 0) 
            {
                inputs[numInputs].streamInfo.type = SOUND_STREAM_TYPE;
                inputs[numInputs].streamInfo.format = PCM_FORMAT;
            }
            else if (strcmp(argv[cmdlnIndex + 1], "timecode") == 0) 
            {
                inputs[numInputs].streamInfo.type = TIMECODE_STREAM_TYPE;
                inputs[numInputs].streamInfo.format = TIMECODE_FORMAT;
            }
            else
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--src-size") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%dx%d", &inputs[numInputs].streamInfo.width, 
                    &inputs[numInputs].streamInfo.height) != 2 || 
                inputs[numInputs].streamInfo.width < 1 || inputs[numInputs].streamInfo.height < 1)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--src-bps") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%dx", &inputs[numInputs].streamInfo.bitsPerSample) != 1 || 
                (inputs[numInputs].streamInfo.bitsPerSample < 1 || inputs[numInputs].streamInfo.bitsPerSample > 32))
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--raw-in") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            inputs[numInputs].type = RAW_INPUT;
            inputs[numInputs].filename = argv[cmdlnIndex + 1];
            if (!complete_source_info(&inputs[numInputs]))
            {
                usage(argv[0]);
                fprintf(stderr, "Unknown source input format for raw input\n");
                return 1;
            }
            numInputs++;
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--balls") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d", &inputs[numInputs].numBalls) != 1 || 
                inputs[numInputs].numBalls <= 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            inputs[numInputs].type = BBALLS_INPUT;
            inputs[numInputs].streamInfo.type = PICTURE_STREAM_TYPE;
            if (!complete_source_info(&inputs[numInputs]))
            {
                usage(argv[0]);
                fprintf(stderr, "Unknown source input format for balls\n");
                return 1;
            }
            numInputs++;
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--blank") == 0)
        {
            inputs[numInputs].type = BLANK_INPUT;
            inputs[numInputs].streamInfo.type = PICTURE_STREAM_TYPE;
            numInputs++;
            cmdlnIndex++;
        }
        else if (strcmp(argv[cmdlnIndex], "--disable-audio") == 0)
        {
            inputs[numInputs].disableAudio = 1;
            cmdlnIndex++;
        }
        else
        {
            usage(argv[0]);
            fprintf(stderr, "Unknown argument '%s'\n", argv[cmdlnIndex]);
            return 1;
        }
        
        if (numInputs >= MAX_INPUTS)
        {
            fprintf(stderr, "Max inputs, %d, exceeded\n", MAX_INPUTS);
            return 1;
        }
    }
    
    if (numInputs == 0)
    {
        usage(argv[0]);
        fprintf(stderr, "No inputs\n");
        return 1;
    }
    
    /* if restoring marks then don't mark PSE failures and VTR errors */
    if (restoreMarksFilename != NULL)
    {
        markPSEFails = 0;
        markVTRErrors = 0;
    }
    

    /* set log level */
    ml_set_log_level(logLevel);
    
    
    /* open the log file */
    if (logFilename != NULL)
    {
        if (!ml_log_file_open(logFilename))
        {
            fprintf(stderr, "Failed to open log file '%s'\n", logFilename);
            return 1;
        }
    }

    /* log the player version */
    ml_log_info("Version: %s, build: %s\n", get_player_version(), get_player_build_timestamp());
    ml_log_file_flush();
    
    /* initialise the player struct */
    memset(&g_player, 0, sizeof(Player));
    g_player.outputType = outputType;
    if (qcControl)
    {
        g_player.connectMapping = QC_MAPPING;
    }
    else
    {
        g_player.connectMapping = DEFAULT_MAPPING;
    }
    g_player.markConfigs = markConfigs;
    
    
    /* set signal handlers to clean up cleanly */
    if (signal(SIGINT, catch_sigint) == SIG_ERR)
    {
        perror("signal(SIGINT)");
        return 1;
    }
    if (signal(SIGTERM, catch_sigint) == SIG_ERR)
    {
        perror("signal(SIGTERM)");
        return 1;
    }
    if (signal(SIGHUP, catch_sigint) == SIG_ERR)
    {
        perror("signal(SIGHUP)");
        return 1;
    }
    if (signal(SIGSEGV, catch_sigint) == SIG_ERR)
    {
        perror("signal(SIGSEGV)");
        return 1;
    }


    /* open the qc session */
    
    if (qcSessionFilename != NULL)
    {
        if (!qcs_open(qcSessionFilename, argc, argv, NULL, &g_player.qcSession))
        {
            fprintf(stderr, "Failed to open QC session\n");
            goto fail;
        }
        qcs_flush(g_player.qcSession);
    }
    

    
    /* open the shuttle input */

    if (!shj_open_shuttle(&g_player.shuttle))
    {
        ml_log_warn("Failed to open shuttle input\n");
    }
    
    
    /* open the terminal keyboard input */

    if (enableTermKeyboard && 
        !tki_create_term_keyboard(&g_player.termKeyboardInput))
    {
        ml_log_warn("Failed to create terminal keyboard input\n");
    }
    
    
    /* create multiple source source */
    
    if (!mls_create(&sourceAspectRatio, maxLength, &multipleSource))
    {
        ml_log_error("Failed to create multiple source data\n");
        goto fail;
    }
    g_player.mediaSource = mls_get_media_source(multipleSource);
    
    
    /* open the media sources */
    
    for (i = 0; i < numInputs; i++)
    {
        switch (inputs[i].type)
        {
            case MXF_INPUT:
                if (!mxfs_open(inputs[i].filename, forceD3MXFInput, markPSEFails, markVTRErrors, &mxfSource))
                {
                    ml_log_error("Failed to open MXF file source\n");
                    goto fail;
                }
                mediaSource = mxfs_get_media_source(mxfSource);
                break;

            case SHM_INPUT:
                if (!shared_mem_open(inputs[i].filename, &mediaSource))
                {
                    ml_log_error("Failed to open shared memory source\n");
                    goto fail;
                }
                break;
                
            case RAW_INPUT:
                if (!rfs_open(inputs[i].filename, &inputs[i].streamInfo, &mediaSource))
                {
                    ml_log_error("Failed to open raw file source\n");
                    goto fail;
                }
                break;
                
           case BBALLS_INPUT:    
                if (!bbs_create(&inputs[i].streamInfo, 2160000, inputs[i].numBalls, &mediaSource))
                {
                    ml_log_error("Failed to create bouncing balls source\n");
                    goto fail;
                }
                break;
                
           case BLANK_INPUT:    
                if (!bks_create(&inputs[i].streamInfo, 2160000, &mediaSource))
                {
                    ml_log_error("Failed to create blank source\n");
                    goto fail;
                }
                break;
                
                
            default:
				ml_log_error("Unknown input type (%d) for input %d\n", inputs[i].type, i);
                assert(0);
        }
        
        /* disable audio */
        if (inputs[i].disableAudio)
        {
            msc_disable_audio(mediaSource);
        }

        /* add to collection */
        if (!mls_assign_source(multipleSource, &mediaSource))
        {
            ml_log_error("Failed to assign media source to multiple source\n");
            goto fail;
        }
    }
    
    /* finalise the blank video sources */
    if (!mls_finalise_blank_sources(multipleSource))
    {
        ml_log_error("Failed to finalise blank video sources\n");
        goto fail;
    }
    
    
    /* add a system timecode source */
    
    if (systemStartTimecode >= 0)
    {
        if (!sts_create(systemStartTimecode, &mediaSource))
        {
            ml_log_error("Failed to create a system timecode source\n");
            goto fail;
        }

        /* add to collection */
        if (!mls_assign_source(multipleSource, &mediaSource))
        {
            ml_log_error("Failed to assign media source to multiple source\n");
            goto fail;
        }
    }
    
    
    /* open buffered media source */
    
    if (srcBufferSize > 0)
    {
        if (!bmsrc_create(g_player.mediaSource, srcBufferSize, 1, srcRateLimit, &bufferedSource))
        {
            ml_log_error("Failed to create buffered media source\n");
            goto fail;
        }
        g_player.mediaSource = bmsrc_get_source(bufferedSource);
    }
    

    /* auto-detect whether X11 Xv is available as output type */
    
    if (outputType == X11_AUTO_DISPLAY_OUTPUT)
    {
        if (xvsk_check_is_available())
        {
            ml_log_info("Auto detected X11 XV output available\n");
            outputType = X11_XV_DISPLAY_OUTPUT;
        }
        else
        {
            ml_log_info("Auto detected X11 XV output NOT available - using non-XV X11 output\n");
            outputType = X11_DISPLAY_OUTPUT;
        }
    }
    else if (outputType == DUAL_OUTPUT && xOutputType == X11_AUTO_DISPLAY_OUTPUT)
    {
        if (xvsk_check_is_available())
        {
            ml_log_info("Auto detected X11 XV output available\n");
            xOutputType = X11_XV_DISPLAY_OUTPUT;
        }
        else
        {
            ml_log_info("Auto detected X11 XV output NOT available - using non-XV X11 output\n");
            xOutputType = X11_DISPLAY_OUTPUT;
        }
    }

    
    /* open media sink */

    switch (outputType)
    {
        case X11_XV_DISPLAY_OUTPUT:
            g_player.x11WindowListener.data = &g_player;
            g_player.x11WindowListener.close_request = x11_window_close_request;
            
            if (!xvsk_open(reviewDuration, disableX11OSD, &pixelAspectRatio, &monitorAspectRatio,
                scale, &g_player.x11XVDisplaySink))
            {
                ml_log_error("Failed to open x11 xv display sink\n");
                goto fail;
            }
            xvsk_set_window_name(g_player.x11XVDisplaySink, "Ingex Player");
            xvsk_register_window_listener(g_player.x11XVDisplaySink, &g_player.x11WindowListener);
            g_player.mediaSink = xvsk_get_media_sink(g_player.x11XVDisplaySink);
            
            if (!bms_create(&g_player.mediaSink, 2, 0, &bufSink))
            {
                ml_log_error("Failed to create bufferred x11 xv display sink\n");
                goto fail;
            }
            g_player.mediaSink = bms_get_sink(bufSink);
            break;

        case X11_DISPLAY_OUTPUT:
            g_player.x11WindowListener.data = &g_player;
            g_player.x11WindowListener.close_request = x11_window_close_request;
            
            if (!xsk_open(reviewDuration, disableX11OSD, &pixelAspectRatio, &monitorAspectRatio,
                scale, &g_player.x11DisplaySink))
            {
                ml_log_error("Failed to open x11 display sink\n");
                goto fail;
            }
            xsk_set_window_name(g_player.x11DisplaySink, "Ingex Player");
            xsk_register_window_listener(g_player.x11DisplaySink, &g_player.x11WindowListener);
            g_player.mediaSink = xsk_get_media_sink(g_player.x11DisplaySink);
            
            if (!bms_create(&g_player.mediaSink, 2, 0, &bufSink))
            {
                ml_log_error("Failed to create bufferred x11 display sink\n");
                goto fail;
            }
            g_player.mediaSink = bms_get_sink(bufSink);
            break;

        case DVS_OUTPUT:
            if (!dvs_open(sdiVITCSource, extraSDIVITCSource, dvsBufferSize, disableSDIOSD, fitVideo, &g_player.dvsSink))
            {
                ml_log_error("Failed to open DVS card sink\n");
                goto fail;
            }
            g_player.mediaSink = dvs_get_media_sink(g_player.dvsSink);
            break;
            
        case DUAL_OUTPUT:
            g_player.x11WindowListener.data = &g_player;
            g_player.x11WindowListener.close_request = x11_window_close_request;
            
            if (!dusk_open(reviewDuration, sdiVITCSource, extraSDIVITCSource, dvsBufferSize, 
                xOutputType == X11_XV_DISPLAY_OUTPUT, disableSDIOSD, disableX11OSD, &pixelAspectRatio, &monitorAspectRatio,
                scale, fitVideo, &g_player.dualSink))
            {
                ml_log_error("Failed to open dual X11 and DVS sink\n");
                goto fail;
            }
            dusk_set_x11_window_name(g_player.dualSink, "Ingex Player");
            dusk_register_window_listener(g_player.dualSink, &g_player.x11WindowListener);
            g_player.mediaSink = dusk_get_media_sink(g_player.dualSink);
            break;
            
        case RAW_STREAM_OUTPUT:
            if (!rms_open(rawFilenameTemplate, &g_player.mediaSink))
            {
                ml_log_error("Failed to open raw file sink\n");
                goto fail;
            }
            break;
            
        case NULL_OUTPUT:
            if (!nms_open(&g_player.mediaSink))
            {
                ml_log_error("Failed to open null sink\n");
                goto fail;
            }
            break;
            
        default:
            assert(0);
    }

#if defined(HAVE_PORTAUDIO)    
    /* create audio sink */
    if (!disablePCAudio)
    {
        if (outputType == X11_XV_DISPLAY_OUTPUT ||
            outputType == X11_DISPLAY_OUTPUT)
            /* || outputType == DUAL_OUTPUT) */
        {
            AudioSink* audioSink;
            if (!aus_create_audio_sink(g_player.mediaSink, audioDevice, &audioSink))
            {
                ml_log_warn("Audio device is not available and will be disabled\n");
            }
            else
            {
                g_player.mediaSink = aus_get_media_sink(audioSink);
            }
        }
    }
#endif    

    /* disable the OSD progress bar */
    if (hideProgressBar)
    {
        OnScreenDisplay* osd = msk_get_osd(g_player.mediaSink);
        if (osd != NULL)
        {
            osd_set_progress_bar_visibility(osd, !hideProgressBar);
        }
    }

    
    /* create video switch, half split or frame sequence sink */
    
    if (addVideoSwitch)
    {
        if (!qvs_create_video_switch(g_player.mediaSink, addQuadSplitToVideoSwitch, 
            applyQuadSplitFilter, &videoSwitch))
        {
            ml_log_error("Failed to create video switch\n");
            goto fail;
        }
        g_player.mediaSink = vsw_get_media_sink(videoSwitch);
    }
    else if (halfSplit)
    {
        if (!hss_create_half_split(g_player.mediaSink, 0, DUAL_PAN_SPLIT_TYPE, 1, &halfSplitSink))
        {
            ml_log_error("Failed to create half split sink\n");
            goto fail;
        }
        g_player.mediaSink = hss_get_media_sink(halfSplitSink);
    }
    else if (showFrameSequence)
    {
        if (!fss_create_frame_sequence(g_player.mediaSink, &frameSequenceSink))
        {
            ml_log_error("Failed to create frame sequence sink\n");
            goto fail;
        }
        g_player.mediaSink = fss_get_media_sink(frameSequenceSink);
    }
    
    
    /* create audio level sink */
    
    if (audioLevelStreams > 0)
    {
        if (!als_create_audio_level_sink(g_player.mediaSink, audioLevelStreams, audioLineupLevel, &audioLevelSink))
        {
            ml_log_error("Failed to create audio level sink\n");
            goto fail;
        }
        g_player.mediaSink = als_get_media_sink(audioLevelSink);
    }
    
    
    /* create buffer state log */
    
    if (bufferStateLogFilename != NULL)
    {
        if ((g_player.bufferStateLogFile = fopen(bufferStateLogFilename, "wb")) == NULL)
        {
            ml_log_error("Failed top open buffer state log file: %s\n", strerror(errno));
            goto fail;
        }
        
        fprintf(g_player.bufferStateLogFile, "# Columns: frame, source buffers filled, sink buffers filled\n"); 
    }
    
    
    /* create the player */
    
    if (!ply_create_player(g_player.mediaSource, g_player.mediaSink, lock, closeAtEnd, 
        numFFMPEGThreads, useWorkerThreads, loop, showFieldSymbol, 
        &startVITCTimecode, &startLTCTimecode, g_player.bufferStateLogFile, &g_player.mediaPlayer))
    {
        ml_log_error("Failed to create media player\n");
        goto fail;
    }
    
    if (bufferedSource != NULL)
    {
        bmsrc_set_media_player(bufferedSource, g_player.mediaPlayer);
    }

    
    
    /* restore marks from QC session */
    
    if (restoreMarksFilename != NULL)
    {
        if (!qcs_set_marks(ply_get_media_control(g_player.mediaPlayer), restoreMarksFilename))
        {
            ml_log_warn("Failed to restore marks from the QC session file\n");
        }
    }

    
    /* connect the X11 display keyboard input */
    
    if (outputType == X11_XV_DISPLAY_OUTPUT)
    {
        xvsk_set_media_control(g_player.x11XVDisplaySink, g_player.connectMapping, ply_get_media_control(g_player.mediaPlayer));
    }
    else if (outputType == X11_DISPLAY_OUTPUT)
    {
        xsk_set_media_control(g_player.x11DisplaySink, g_player.connectMapping, ply_get_media_control(g_player.mediaPlayer));
    }
    else if (outputType == DUAL_OUTPUT)
    {
        dusk_set_media_control(g_player.dualSink, g_player.connectMapping, ply_get_media_control(g_player.mediaPlayer));
    }
    
    
    /* create the console monitor */

    if (!disableConsoleMonitor &&
        !csm_open(g_player.mediaPlayer, &g_player.consoleMonitor))
    {
        ml_log_error("Failed to create console monitor\n");
        goto fail;
    }
    
    
    /* connect the QC session to the player */
    
    if (g_player.qcSession != NULL)
    {
        if (!qcs_connect_to_player(g_player.qcSession, g_player.mediaPlayer))
        {
            ml_log_error("Failed to connect the QC session to the player\n");
            goto fail;
        }
    }

#if defined(HAVE_SHTTPD)    
    /* create the http access */

    if (httpPort >= 0)
    {
        if (!hac_create_http_access(g_player.mediaPlayer, httpPort, &g_player.httpAccess))
        {
            ml_log_error("Failed to create http access\n");
            goto fail;
        }
    }
#endif    


    /* create and start shuttle/keyboard control threads */
    
    if (!start_control_threads(&g_player, reviewDuration))
    {
		ml_log_error("Failed to start control threads:\n");
        goto fail;
    }

    
    /* qc or not control stuff */
    
    if (qcControl)
    {
        /* start with info screen and pause */
        mc_pause(ply_get_media_control(g_player.mediaPlayer));
        mc_set_osd_screen(ply_get_media_control(g_player.mediaPlayer), OSD_SOURCE_INFO_SCREEN);
        
        /* set default mark configs if neccessary */
        if (g_player.markConfigs.numConfigs == 0)
        {
            MarkConfig configs[] = 
            {
                {M0_MARK_TYPE, "red", RED_COLOUR},
                {M1_MARK_TYPE, "magenta", MAGENTA_COLOUR},
                {M2_MARK_TYPE, "green", GREEN_COLOUR},
                {M3_MARK_TYPE, "blue", BLUE_COLOUR},
                {M4_MARK_TYPE, "cyan", CYAN_COLOUR},
                {D3_VTR_ERROR_MARK_TYPE, "yellow (D3 VTR)", YELLOW_COLOUR},
                {D3_PSE_FAILURE_MARK_TYPE, "orange (PSE)", ORANGE_COLOUR},
            };
            memcpy(&g_player.markConfigs.configs, configs, sizeof(configs));
            g_player.markConfigs.numConfigs = sizeof(configs) / sizeof(MarkConfig);
        }
        
        osd_set_mark_display(msk_get_osd(g_player.mediaSink), &g_player.markConfigs);
    }
    else
    {
        /* start with player state screen */
        mc_set_osd_screen(ply_get_media_control(g_player.mediaPlayer), OSD_PLAY_STATE_SCREEN);

        /* set mark configs */
        if (g_player.markConfigs.numConfigs > 0)
        {
            osd_set_mark_display(msk_get_osd(g_player.mediaSink), &g_player.markConfigs);
        }
    }
    
    
    /* set the timecode type to start with */
    
    if (startTimecodeIndex >= 0)
    {
        mc_set_osd_timecode(ply_get_media_control(g_player.mediaPlayer), startTimecodeIndex, startTimecodeType, startTimecodeSubType);
    }
    
    
    /* start playing... */
    
    gettimeofday(&startTime, NULL);
    ml_log_file_flush();

    if (!ply_start_player(g_player.mediaPlayer))
    {
        ml_log_error("Media player failed to play\n");
        goto fail;
    }
    
    ml_log_file_flush();
    gettimeofday(&endTime, NULL);
    
    timeDiffSec = endTime.tv_sec - startTime.tv_sec;
    timeDiffUSec = endTime.tv_usec - startTime.tv_usec;
    if (timeDiffUSec < 0)
    {
        timeDiffSec -= 1;
        timeDiffUSec += 1000000;
    }
    ml_log_info("Playing time was %ld secs and %06d usecs\n", timeDiffSec, timeDiffUSec);
    
    
    /* close down */

    cleanup_exit(0);
    return 0; /* for the benefit of the compiler */
    
fail:
    if (logFilename != NULL)
    {
        fprintf(stderr, "Failed - see %s for details\n", logFilename);
    }
    else
    {
        fprintf(stderr, "Failed\n");
    }
    cleanup_exit(1);
    return 1; /* for the benefit of the compiler */
}
