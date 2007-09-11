#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <ctype.h>
#include <assert.h>

#include "qc_session.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


#define QC_LOG_LINE_ELEMENTS        9


struct QCSession
{
    char* sessionName;
    FILE* sessionFile;
    MediaPlayerListener playerListener;
    FrameInfo lastFrameInfo;
    int haveFirstFrame;
    int haveEndOfSource;
    
    int64_t controlTCStartPos;
};

typedef char LogLine[QC_LOG_LINE_ELEMENTS][64];

static const char* g_qcSessionPreSuf = "_qcsession_";



static char* get_mark_name(const MarkConfigs* markConfigs, int typeBit, char name[32])
{
    int i;
    int bitMask;
    
    name[0] = '\0';
    name[31] = '\0';
    
    if (typeBit >= 1 && typeBit <= 32)
    {
        bitMask = 0x1 << (typeBit - 1);
        
        /* try find a config */
        for (i = 0; i < markConfigs->numConfigs; i++)
        {
            if (markConfigs->configs[i].type & bitMask)
            {
                strncpy(name, markConfigs->configs[i].name, 31);
                break;
            }
        }
        
        /* if no config, then use generic naming scheme */
        if (i == markConfigs->numConfigs)
        {
            snprintf(name, 32, "M%d", typeBit - 1);
        }
    }
    
    return name;
}

static void init_log_line(LogLine logLine)
{
    memset(logLine, 0, sizeof(LogLine));
}

static void write_log_line_element(LogLine logLine, int elementNum, const char* format, ...)
{
    va_list p_arg;
    
    if (format == NULL)
    {
        return;
    }
    
    va_start(p_arg, format);
    vsprintf(logLine[elementNum], format, p_arg);
    va_end(p_arg);
}

static void write_comment(QCSession* qcSession, const char* format, ...)
{
    va_list p_arg;
    int i;
    
    if (format == NULL)
    {
        return;
    }
    
    fprintf(qcSession->sessionFile, "# ");
    va_start(p_arg, format);
    vfprintf(qcSession->sessionFile, format, p_arg);
    va_end(p_arg);
    
    for (i = 0; i < QC_LOG_LINE_ELEMENTS - 1; i++)
    {
        fprintf(qcSession->sessionFile, ", ");
    }
    fprintf(qcSession->sessionFile, "\n");
}

static void write_comment_start(QCSession* qcSession)
{
    fprintf(qcSession->sessionFile, "# ");
}

static void write_comment_middle(QCSession* qcSession, const char* format, ...)
{
    va_list p_arg;
    
    if (format == NULL)
    {
        return;
    }
    
    va_start(p_arg, format);
    vfprintf(qcSession->sessionFile, format, p_arg);
    va_end(p_arg);
}

static void write_comment_end(QCSession* qcSession)
{
    int i;
    
    for (i = 0; i < QC_LOG_LINE_ELEMENTS - 1; i++)
    {
        fprintf(qcSession->sessionFile, ", ");
    }
    fprintf(qcSession->sessionFile, "\n");
}

static void qcs_write_timecodes(LogLine logLine, const FrameInfo* frameInfo)
{
    int i;
    int element;
    int haveControlTC = 0;
    int haveVITC = 0;
    int haveLTC = 0;
    
    for (i = 0; i < frameInfo->numTimecodes; i++)
    {
        if (!haveControlTC && frameInfo->timecodes[i].timecodeType == CONTROL_TIMECODE_TYPE)
        {
            element = QC_LOG_LINE_ELEMENTS - 3;
            haveControlTC = 1;
        }
        else if (!haveVITC && frameInfo->timecodes[i].timecodeType == SOURCE_TIMECODE_TYPE &&
            frameInfo->timecodes[i].timecodeSubType == VITC_SOURCE_TIMECODE_SUBTYPE)
        {
            element = QC_LOG_LINE_ELEMENTS - 2;
            haveVITC = 1;
        }
        else if (!haveLTC && frameInfo->timecodes[i].timecodeType == SOURCE_TIMECODE_TYPE &&
            frameInfo->timecodes[i].timecodeSubType == LTC_SOURCE_TIMECODE_SUBTYPE)
        {
            element = QC_LOG_LINE_ELEMENTS - 1;
            haveLTC = 1;
        }
        else
        {
            element = -1;
        }

        if (element >= 0)
        {
            sprintf(logLine[element], 
                "%02u:%02u:%02u:%02u", 
                frameInfo->timecodes[i].timecode.hour, 
                frameInfo->timecodes[i].timecode.min, 
                frameInfo->timecodes[i].timecode.sec, 
                frameInfo->timecodes[i].timecode.frame);
        }
    }
}

static void write_log_line(QCSession* qcSession, LogLine logLine, const FrameInfo* frameInfo)
{
    int i;
    
    if (frameInfo != NULL)
    {
        qcs_write_timecodes(logLine, frameInfo);
    }
    
    for (i = 0; i < QC_LOG_LINE_ELEMENTS; i++)
    {
        if (i != 0)
        {
            fprintf(qcSession->sessionFile, ", ");
        }
        fprintf(qcSession->sessionFile, "%s", logLine[i]);
    }
    fprintf(qcSession->sessionFile, "\n");
}

static void qcs_frame_displayed_event(void* data, const FrameInfo* frameInfo)
{
    QCSession* qcSession = (QCSession*)data;
    LogLine logLine;
    int i;
    
    if (!qcSession->haveFirstFrame)
    {
        init_log_line(logLine);
        
        write_log_line_element(logLine, 1, "First frame displayed");
        write_log_line(qcSession, logLine, frameInfo);

        /* get the control timecode position */        
        qcSession->controlTCStartPos = 0;
        for (i = 0; i < frameInfo->numTimecodes; i++)
        {
            if (frameInfo->timecodes[i].timecodeType == CONTROL_TIMECODE_TYPE)
            {
                qcSession->controlTCStartPos = frameInfo->timecodes[i].timecode.hour * 60 * 60 * 25 +
                    frameInfo->timecodes[i].timecode.min * 60 * 25 +
                    frameInfo->timecodes[i].timecode.sec * 25 +
                    frameInfo->timecodes[i].timecode.frame;
                if (qcSession->controlTCStartPos < frameInfo->position)
                {
                    ml_log_warn("QC session found negative control timecode start position - ignoring and assuming 0 start position\n"); 
                    qcSession->controlTCStartPos = 0;
                }
                else
                {
                    qcSession->controlTCStartPos -= frameInfo->position;
                }
                break;
            }
        }

        qcSession->haveFirstFrame = 1;
    }
    qcSession->lastFrameInfo = *frameInfo;
}

static void qcs_frame_dropped_event(void* data, const FrameInfo* displayedFrameInfo)
{
//    QCSession* qcSession = (QCSession*)data;
}

static void qcs_state_change_event(void* data, const MediaPlayerStateEvent* event)
{
    QCSession* qcSession = (QCSession*)data;
    LogLine logLine;
    int elementNum;
    
    init_log_line(logLine);
    elementNum = 2;
    
    if (event->lockedChanged)
    {
        if (event->locked)
        {
            write_log_line_element(logLine, elementNum, "Lock");
        }
        else
        {
            write_log_line_element(logLine, elementNum, "Unlock");
        }
    }
    elementNum++;
    
    if (event->playChanged)
    {
        if (event->play)
        {
            write_log_line_element(logLine, elementNum, "Play");
        }
        else
        {
            write_log_line_element(logLine, elementNum, "Pause");
        }
    }
    elementNum++;
    
    if (event->stopChanged)
    {
        if (event->stop)
        {
            write_log_line_element(logLine, elementNum, "Stop");
        }
        else
        {
            write_log_line_element(logLine, elementNum, "Start");
        }
    }
    elementNum++;
    
    if (event->speedChanged)
    {
        write_log_line_element(logLine, elementNum, "%dx", event->speed);
    }
    elementNum++;
    

    write_log_line(qcSession, logLine, &event->displayedFrameInfo);
}

static void qcs_player_closed(void* data)
{
    QCSession* qcSession = (QCSession*)data;
    LogLine logLine;
    
    if (data == NULL)
    {
        return;
    }
    
    if (qcSession->haveFirstFrame)
    {
        init_log_line(logLine);
        
        write_log_line_element(logLine, 1, "Last frame displayed");
        write_log_line(qcSession, logLine, &qcSession->lastFrameInfo);
    }
}


int qcs_open(const char* mxfFilename, int argc, const char** argv, const char* name, QCSession** qcSession)
{
    QCSession* newQCSession;
    char timestampStr[MAX_TIMESTAMP_STRING_SIZE];
    int i;
    LogLine logLine;
    char filename[FILENAME_MAX];
    char* lastSep;


    CALLOC_ORET(newQCSession, QCSession, 1);
    
    get_short_timestamp_string(timestampStr);
    snprintf(filename, FILENAME_MAX, "%s%s%s.txt", mxfFilename, g_qcSessionPreSuf, timestampStr);
    if ((lastSep = strrchr(filename, '/')) == NULL)
    {
        CALLOC_OFAIL(newQCSession->sessionName, char, strlen(filename) + 1);
        strcpy(newQCSession->sessionName, filename);
    }
    else
    {
        CALLOC_OFAIL(newQCSession->sessionName, char, strlen(lastSep + 1) + 1);
        strcpy(newQCSession->sessionName, lastSep + 1);
    }
    if ((newQCSession->sessionFile = fopen(filename, "wb")) == NULL)
    {
        ml_log_error("Failed to open QC session '%s' for writing\n", filename);;
        goto fail;
    }
    
    newQCSession->playerListener.data = newQCSession;
    newQCSession->playerListener.frame_displayed_event = qcs_frame_displayed_event;
    newQCSession->playerListener.frame_dropped_event = qcs_frame_dropped_event;
    newQCSession->playerListener.state_change_event = qcs_state_change_event;
    newQCSession->playerListener.player_closed = qcs_player_closed;

    /* write header */
    write_comment(newQCSession, "Quality Control Log");
    write_comment(newQCSession, "");
    if (name != NULL)
    {
        write_comment(newQCSession, "Name: %s", name);
    }
    get_timestamp_string(timestampStr);
    write_comment(newQCSession, "Started: %s", timestampStr);
    write_comment_start(newQCSession);
    if (argc > 0)
    {
        write_comment_middle(newQCSession, "Command: ");
        for (i = 0; i < argc; i++)
        {
            write_comment_middle(newQCSession, "%s ", argv[i]);
        }
    }
    write_comment_end(newQCSession);
    write_comment(newQCSession, "");
    write_comment(newQCSession, "");
    write_comment(newQCSession, "Player state");
    write_comment(newQCSession, "");

    /* write state header */
    init_log_line(logLine);
    write_log_line_element(logLine, 0, "#");
    write_log_line_element(logLine, 1, "First/Last frame displayed");
    write_log_line_element(logLine, 2, "Lock/Unlock");
    write_log_line_element(logLine, 3, "Play/Pause");
    write_log_line_element(logLine, 4, "Stop/Start");
    write_log_line_element(logLine, 5, "Speed");
    write_log_line_element(logLine, 6, "Control TC");
    write_log_line_element(logLine, 7, "VITC");
    write_log_line_element(logLine, 8, "LTC");
    write_log_line(newQCSession, logLine, NULL);
    write_comment(newQCSession, "");
    
    *qcSession = newQCSession;
    return 1;
    
fail:
    qcs_close(&newQCSession);
    return 0;
}

int qcs_connect_to_player(QCSession* qcSession, MediaPlayer* player)
{
    return ply_register_player_listener(player, &qcSession->playerListener);
}

void qcs_write_marks(QCSession* qcSession, const MarkConfigs* markConfigs, Mark* marks, int numMarks)
{
    int i;
    int j;
    LogLine logLine;
    int64_t controlTCPos;
    int mask;
    char markName[32];
    
    write_comment(qcSession, "");
    write_comment(qcSession, "");
    write_comment(qcSession, "Marks");
    write_comment(qcSession, "");

    init_log_line(logLine);
    write_log_line_element(logLine, 0, "#");
    write_log_line_element(logLine, 1, "Position");
    write_log_line_element(logLine, 2, "Control TC");
    write_log_line_element(logLine, 3, "Type");
    write_log_line_element(logLine, 4, "Description");
    write_log_line(qcSession, logLine, NULL);

    write_comment(qcSession, "");
    
    for (i = 0; i < numMarks; i++)
    {
        /*fprintf(qcSession->sessionFile, "");*/
        
        fprintf(qcSession->sessionFile, ", %"PRId64"", marks[i].position);

        controlTCPos = marks[i].position + qcSession->controlTCStartPos;
        fprintf(qcSession->sessionFile, ", %02"PRId64":%02"PRId64":%02"PRId64":%02"PRId64"",
            controlTCPos / (25 * 60 * 60),
            (controlTCPos % (25 * 60 * 60)) / (25 * 60),
            ((controlTCPos % (25 * 60 * 60)) % (25 * 60)) / 25,
            ((controlTCPos % (25 * 60 * 60)) % (25 * 60)) % 25);
            
        fprintf(qcSession->sessionFile, ", 0x%08x", marks[i].type);
        
        fprintf(qcSession->sessionFile, ", ");
        mask = 0x00000001;
        for (j = 1; j <= 32; j++)
        {
            if (marks[i].type & mask)
            {
                get_mark_name(markConfigs, j, markName);
                fprintf(qcSession->sessionFile, "'%s' ", markName);
            }
            mask <<= 1;
        }

        for (j = 0; j < QC_LOG_LINE_ELEMENTS - 5; j++)
        {
            fprintf(qcSession->sessionFile, ", ");
        }
        fprintf(qcSession->sessionFile, "\n");
    }
}

void qcs_flush(QCSession* qcSession)
{
    fflush(qcSession->sessionFile);
}

void qcs_close(QCSession** qcSession)
{
    char timestampStr[MAX_TIMESTAMP_STRING_SIZE];
    
    if (*qcSession == NULL)
    {
        return;
    }
    
    write_comment((*qcSession), "");
    get_timestamp_string(timestampStr);
    write_comment((*qcSession), "Ended: %s", timestampStr);
    write_comment((*qcSession), "");
    
    if ((*qcSession)->sessionFile != NULL)
    {
        fclose((*qcSession)->sessionFile);
        (*qcSession)->sessionFile = NULL;
    }
    
    SAFE_FREE(&(*qcSession)->sessionName);
    
    SAFE_FREE(qcSession);
}


int qcs_set_marks(MediaControl* control, const char* filename)
{
    FILE* qcSessionFile = NULL;
    int state = 0;
    char buffer[128];
    int done = 0;
    int64_t position;
    int type;
    char* first;
    char* second;
    char* third;
    int64_t count = 0;
    int startHex;
    
    /* open QC session file */
    if ((qcSessionFile = fopen(filename, "rb")) == NULL)
    {
        ml_log_error("Failed to open QC session %s for reading\n", filename);;
        return 0;
    }
    
    /* parse marks and set in player */
    while (!done)
    {
        switch (state)
        {
            /* find "# Marks" */
            case 0:
                if (fgets(buffer, 128, qcSessionFile) == NULL)
                {
                    /* EOF */
                    done = 1;
                    break;
                }
                
                if (strncmp("# Marks", buffer, 7) == 0)
                {
                    state = 1;
                }
                
                /* skip rest of line */
                while (strchr(buffer, '\n') == NULL)
                {
                    if (fgets(buffer, 128, qcSessionFile) == NULL)
                    {
                        /* EOF */
                        done = 1;
                        break;
                    }
                }
                break;

            /* parse marks, where position is column 2 and type is in column 4 */
            case 1:
                /* read the next line */
                if (fgets(buffer, 128, qcSessionFile) == NULL)
                {
                    /* EOF */
                    done = 1;
                    break;
                }
                
                if (buffer[0] != '#')
                {
                    first = NULL;
                    second = NULL;
                    third = NULL;
                    
                    first = strchr(buffer, ',');
                    if (first)
                    {
                        second = strchr(first + 1, ',');
                    }
                    if (second)
                    {
                        third = strchr(second + 1, ',');
                    }
                    
                    if (first && third)
                    {
                        if (sscanf(first + 1, "%"PRId64"", &position) != 1)
                        {
                            ml_log_warn("Failed to parse mark in QC session\n");
                        }
                        else
                        {
                            /* hex value starts after '0x' and ends at space or ',' */
                            third++;
                            startHex = 0;
                            type = 0;
                            while (*third != '\0')
                            {
                                if (*third == ',')
                                {
                                    break;
                                }
                                else if (startHex)
                                {
                                    if (*third >= '0' && *third <= '9')
                                    {
                                        type <<= 4;
                                        type |= *third - '0';
                                    }
                                    else if (*third >= 'a' && *third <= 'f')
                                    {
                                        type <<= 4;
                                        type |= 0x0a + *third - 'a';
                                    }
                                    else if (*third >= 'A' && *third <= 'F')
                                    {
                                        type <<= 4;
                                        type |= 0x0a + *third - 'A';
                                    }
                                    else if (*third == ' ')
                                    {
                                        break;
                                    }
                                    else
                                    {
                                        /* parse error */
                                        startHex = 0;
                                        break;
                                    }
                                }
                                else if (*third == 'X' || *third == 'x')
                                {
                                    startHex = 1;
                                }
                                third++;
                            }

                            if (startHex)
                            {
                                mc_mark_position(control, position, type, 0);
                                count++;
                            }
                            else
                            {
                                ml_log_warn("Failed to parse mark in QC session\n");
                            }
                        }
                    }
                }
                
                /* skip rest of line */
                while (strchr(buffer, '\n') == NULL)
                {
                    if (fgets(buffer, 128, qcSessionFile) == NULL)
                    {
                        /* EOF */
                        done = 1;
                        break;
                    }
                }

                break;
        }
    }
                
    ml_log_info("Set %"PRId64" marks from qc session\n", count);
    
    fclose(qcSessionFile);
    return 1;
}

int qcs_is_session_file(const char* filename, const char* mxfFilename)
{
    size_t filenameLen = strlen(filename);
    size_t mxfFilenameLen = strlen(mxfFilename);
    
    /* return 1 if filename = mxfFilename + (.*) + '.txt' */
    
    if (strncmp(filename, mxfFilename, mxfFilenameLen) != 0)
    {
        return 0;
    }
    if (filenameLen < mxfFilenameLen + 4)
    {
        return 0;
    }
    if (filename[filenameLen - 4] != '.' ||
        tolower(filename[filenameLen - 3]) != 't' ||
        tolower(filename[filenameLen - 2]) != 'x' ||
        tolower(filename[filenameLen - 1]) != 't')
    {
        return 0;
    }
    
    return 1;
}

const char* qcs_get_session_name(QCSession* qcSession)
{
    return qcSession->sessionName;
}




