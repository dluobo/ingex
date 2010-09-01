/*
 * $Id: HTTPRecorder.cpp,v 1.2 2010/09/01 16:05:22 philipn Exp $
 *
 * HTTP interface to the recorder
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
/*
    The set of URLs handled by this class are listed near the top of this file.
    The requests are sometimes handled by "agents" running is a new thread to 
    avoid holding up other HTTP requests.
    
    New barcodes read using a server side barcode scanner are signalled back to 
    the client browser using a barcode update count in the recorder status info.
    
    PSE reports are displayed in a HTML frame to allow users to easily print 
    and go back to the previous page using a touchscreen monitor. 
*/
 
#include "HTTPRecorder.h"
#include "JSONObject.h"
#include "InfaxAccess.h"
#include "RecorderDatabase.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "version.h"


using namespace std;
using namespace rec;


// a barcode expires (ie. will not be sent to a client) after ... seconds
#define BARCODE_EXPIRES_TIME_SEC        30

// all the HTTP service URLs handled by the HTTPRecorder
static const char* g_recorderStatusURL = "/recorder/status.json";
static const char* g_getProfileListURL = "/recorder/profilelist.json";
static const char* g_getProfileURL = "/recorder/profile.json";
static const char* g_updateProfileURL = "/recorder/updateprofile.json";
static const char* g_sourceInfoURL = "/recorder/sourceinfo.json";
static const char* g_checkSelectedDigibetaURL = "/recorder/checkselecteddigibeta.json";
static const char* g_newSessionURL = "/recorder/newsession.json";
static const char* g_sessionSourceInfoURL = "/recorder/session/sourceinfo.json";
static const char* g_startRecordingURL = "/recorder/session/start";
static const char* g_stopRecordingURL = "/recorder/session/stop";
static const char* g_chunkFileURL = "/recorder/session/chunkfile";
static const char* g_completeSessionURL = "/recorder/session/complete";
static const char* g_abortSessionURL = "/recorder/session/abort";
static const char* g_setSessionCommentsURL = "/recorder/session/setsessioncomments";
static const char* g_getSessionCommentsURL = "/recorder/session/getsessioncomments.json";
static const char* g_cacheContentsURL = "/recorder/cache/contents.json";
static const char* g_confReplayURL = "/confreplay/*";
static const char* g_replayFileURL = "/recorder/replay";
static const char* g_playItemURL = "/recorder/session/playitem";
static const char* g_playPrevItemURL = "/recorder/session/playprevitem";
static const char* g_playNextItemURL = "/recorder/session/playnextitem";
static const char* g_seekToEOPURL = "/recorder/session/seektoeop";
static const char* g_markItemStartURL = "/recorder/session/markitemstart.json";
static const char* g_clearItemURL = "/recorder/session/clearitem.json";
static const char* g_moveItemUpURL = "/recorder/session/moveitemup.json";
static const char* g_moveItemDownURL = "/recorder/session/moveitemdown.json";
static const char* g_disableItemURL = "/recorder/session/disableitem.json";
static const char* g_enableItemURL = "/recorder/session/enableitem.json";
static const char* g_getItemClipInfoURL = "/recorder/session/getitemclipinfo.json";
static const char* g_getItemSourceInfoURL = "/recorder/session/getitemsourceinfo.json";

static const char* g_pseReportsURL = "/psereports";
static const char* g_pseReportFramedURL = "/psereport_framed.shtml";

static const char* g_ssiGetPSEReportURL = "get_pse_report_url";


static const char* g_urlUnknownAspectRatioCode = "-";




static bool parse_int(string intStr, int* value)
{
    return sscanf(intStr.c_str(), "%d", value) == 1; 
}

static bool parse_aspect_ratio_codes(string aspectRatioCodesString, vector<string>* aspectRatioCodes)
{
    string aspectRatioCode;
    size_t i;
    for (i = 0; i < aspectRatioCodesString.size(); i++)
    {
        if (aspectRatioCodesString[i] == ',')
        {
            if (aspectRatioCode.empty())
            {
                return false;
            }

            if (aspectRatioCode == g_urlUnknownAspectRatioCode)
            {
                aspectRatioCodes->push_back("");
            }
            else
            {
                aspectRatioCodes->push_back(aspectRatioCode);
            }
            aspectRatioCode.clear();
        }
        else
        {
            aspectRatioCode.push_back(aspectRatioCodesString[i]);
        }
    }
    if (!aspectRatioCode.empty())
    {
        if (aspectRatioCode == g_urlUnknownAspectRatioCode)
        {
            aspectRatioCodes->push_back("");
        }
        else
        {
            aspectRatioCodes->push_back(aspectRatioCode);
        }
    }
    
    return true;
}

static bool parse_ingest_format(string arg, IngestFormat *defaultIngestFormat)
{
    int intValue;
    if (!parse_int(arg, &intValue))
    {
        return false;
    }
    
    if (intValue != UNKNOWN_INGEST_FORMAT &&
        intValue != MXF_UNC_8BIT_INGEST_FORMAT &&
        intValue != MXF_UNC_10BIT_INGEST_FORMAT &&
        intValue != MXF_D10_50_INGEST_FORMAT)
    {
        return false;
    }
    
    *defaultIngestFormat = (IngestFormat)intValue;
    return true;
}

static vector<string> parse_string_array(string arg)
{
    vector<string> retValue;
    size_t prevPos = 0;
    size_t pos = 0;
    while ((pos = arg.find(",", prevPos)) != string::npos)
    {
        retValue.push_back(arg.substr(prevPos, pos - prevPos));
        prevPos = pos + 1;
    }
    if (prevPos < arg.size())
    {
        retValue.push_back(arg.substr(prevPos, arg.size() - prevPos));
    }
    
    return retValue;
}

static bool parse_ingest_formats(string arg, map<string, IngestFormat> *ingestFormats)
{
    vector<string> strArray = parse_string_array(arg);
    if (strArray.size() % 2 != 0)
    {
        return false;
    }
    
    map<string, IngestFormat> retValue;
    
    IngestFormat ingestFormat;
    size_t i;
    for (i = 0; i < strArray.size(); i += 2)
    {
        if (strArray[i].empty())
        {
            return false;
        }
        
        if (!parse_ingest_format(strArray[i + 1], &ingestFormat))
        {
            return false;
        }
        
        retValue[strArray[i]] = ingestFormat;
    }
    
    *ingestFormats = retValue;
    return true;
}

static bool parse_primary_timecode(string arg, PrimaryTimecode *primaryTimecode)
{
    int intValue;
    if (!parse_int(arg, &intValue))
    {
        return false;
    }
    
    if (intValue != PRIMARY_TIMECODE_AUTO &&
        intValue != PRIMARY_TIMECODE_LTC &&
        intValue != PRIMARY_TIMECODE_VITC)
    {
        return false;
    }
    
    *primaryTimecode = (PrimaryTimecode)intValue;
    return true;
}

static string get_size_string(int64_t size)
{
    char buf[32];
    buf[0] = '\0';
    
    if (size < 0)
    {
        sprintf(buf, "0B");
    }
    else if (size < 1000LL)
    {
        sprintf(buf, "%dB", (int)size);
    }
    else if (size < 1000LL * 1000LL)
    {
        sprintf(buf, "%.2fKB", size / 1000.0);
    }
    else if (size < 1000LL * 1000LL * 1000LL)
    {
        sprintf(buf, "%.2fMB", size / (1000.0 * 1000.0));
    }
    else if (size < 1000LL * 1000LL * 1000LL * 1000LL)
    {
        sprintf(buf, "%.2fGB", size / (1000.0 * 1000.0 * 1000.0));
    }
    else
    {
        sprintf(buf, "%.2fTB", size / (1000.0 * 1000.0 * 1000.0 * 1000.0));
    }
    
    return buf;
}

static string get_timestamp_string(Timestamp ts)
{
    char buf[128];

    buf[0] = '\0';
    
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", ts.year, ts.month, ts.day, ts.hour, ts.min, ts.sec);
        
    return buf;
}

static string get_session_status_string(long status)
{
    switch (status)
    {
        case RECORDING_SESSION_STARTED:
            return "Started";
        case RECORDING_SESSION_COMPLETED:
            return "Completed";
        case RECORDING_SESSION_ABORTED:
            return "Aborted";
        default:
            return "Unknown";
    }
}

static string get_complete_prog_no(string magPrefix, string progNo, string prodCode)
{
    string trimProdCode = trim_string(prodCode);
    string trimMagPrefix = trim_string(magPrefix);
    
    string result;
    if (!trimMagPrefix.empty())
    {
        result = trimMagPrefix + ":";
    }
    result += trim_string(progNo);
    if (!trimProdCode.empty())
    {
        result += "/";
        if (trimProdCode.size() == 1 && 
            trimProdCode[0] >= '0' && trimProdCode[0] <= '9')
        {
            // replace single digit with 2 digit starting with '0'
            result += "0";
        }
        result += trimProdCode;
    }
    
    return result;
}

static string get_vtr_state_string(VTRState state)
{
    switch (state)
    {
        case NOT_CONNECTED_VTR_STATE:
            return "Not connected";
        case REMOTE_LOCKOUT_VTR_STATE:
            return "Remote lockout";
        case TAPE_UNTHREADED_VTR_STATE:
            return "Unthreaded";
        case STOPPED_VTR_STATE:
            return "Stopped";
        case PAUSED_VTR_STATE:
            return "Paused";
        case PLAY_VTR_STATE:
            return "Playing";
        case FAST_FORWARD_VTR_STATE:
            return "Fast forward";
        case FAST_REWIND_VTR_STATE:
            return "Fast rewind";
        case EJECTING_VTR_STATE:
            return "Ejecting";
        case RECORDING_VTR_STATE:
            return "Recording";
        case SEEKING_VTR_STATE:
            return "Seeking";
        case JOG_VTR_STATE:
            return "Jog";
        case OTHER_VTR_STATE:
            return "Other";
        default:
            return "Unknown";
    }
}






HTTPRecorder::HTTPRecorder(HTTPServer* server, Recorder* recorder, BarcodeScanner* scanner)
: _recorder(recorder), _barcodeCount(0)
{
    HTTPServiceDescription* service;
    
    service = server->registerService(new HTTPServiceDescription(g_recorderStatusURL), this);
    service->setDescription("Returns the recorder status");
    service->addArgument("barcode", "boolean", false, "Include barcode scanned in using the barcode scanner");
    service->addArgument("session", "boolean", false, "Include recording session status information");
    service->addArgument("cache", "boolean", false, "Include cache status information");
    service->addArgument("system", "boolean", false, "Include system information");
    
    service = server->registerService(new HTTPServiceDescription(g_getProfileListURL), this);
    service->setDescription("Returns the list of profile identifiers and names");
    
    service = server->registerService(new HTTPServiceDescription(g_getProfileURL), this);
    service->setDescription("Returns the profile");
    service->addArgument("id", "integer", true, "The profile identifier");
    
    service = server->registerService(new HTTPServiceDescription(g_updateProfileURL), this);
    service->setDescription("Update the profile");
    service->addArgument("id", "integer", true, "The profile identifier");
    service->addArgument("defaultingestformat", "integer", true, "The default ingest format");
    service->addArgument("ingestformats", "array of string,integer", true, "The selected ingest format for each source format code");
    
    service = server->registerService(new HTTPServiceDescription(g_sourceInfoURL), this);
    service->setDescription("Returns the source information");
    service->addArgument("barcode", "string", true, "The barcode that uniquely identifies the source item");
    service->addArgument("profiles", "boolean", false, "Include profile list");
    
    service = server->registerService(new HTTPServiceDescription(g_checkSelectedDigibetaURL), this);
    service->setDescription("Checks the status of the selected digibeta backup tape");
    service->addArgument("barcode", "string", true, "The barcode that uniquely identifies the digibeta tape");
    
    service = server->registerService(new HTTPServiceDescription(g_newSessionURL), this);
    service->setDescription("Starts a new recording session");
    service->addArgument("barcode", "string", true, "The barcode that uniquely identifies the source item");
    service->addArgument("profileid", "integer", true, "The profile identifier");
    service->addArgument("digibetabarcode", "string", false, "The Digibeta barcode that uniquely identifies the Digibeta backup tape");
    service->addArgument("aspectRatioCodes", "array of strings", false, "The aspect ratio code ('-' is unknown) for each source item");
    
    service = server->registerService(new HTTPServiceDescription(g_sessionSourceInfoURL), this);
    service->setDescription("Returns the source information associated with the current session");
    
    service = server->registerService(new HTTPServiceDescription(g_startRecordingURL), this);
    service->setDescription("Starts recording in the current session");
    
    service = server->registerService(new HTTPServiceDescription(g_stopRecordingURL), this);
    service->setDescription("Stops recording in the current session");
    
    service = server->registerService(new HTTPServiceDescription(g_chunkFileURL), this);
    service->setDescription("Initiates chunking of the file");
    
    service = server->registerService(new HTTPServiceDescription(g_completeSessionURL), this);
    service->setDescription("Complete the current recording session (POST)");
    service->addArgument("comments", "string", true, "Comments related to the completed session");
    
    service = server->registerService(new HTTPServiceDescription(g_abortSessionURL), this);
    service->setDescription("Aborts the current recording session (POST)");
    service->addArgument("comments", "string", true, "Comments related to the aborted session");

    service = server->registerService(new HTTPServiceDescription(g_cacheContentsURL), this);
    service->setDescription("Returns the contents of the cache");
    
    service = server->registerService(new HTTPServiceDescription(g_confReplayURL), this);
    service->setDescription("Forwards control commands to the confidence replay player");
    
    service = server->registerService(new HTTPServiceDescription(g_replayFileURL), this);
    service->setDescription("Plays the file if not busy recording");
    service->addArgument("name", "string", true, "The name of the file in the cache");
    
    service = server->registerService(new HTTPServiceDescription(g_setSessionCommentsURL), this);
    service->setDescription("Sets the session comments (POST)");
    service->addArgument("comments", "string", true, "Comments related to the session");
    
    service = server->registerService(new HTTPServiceDescription(g_getSessionCommentsURL), this);
    service->setDescription("Returns the session comments");
    
    service = server->registerService(new HTTPServiceDescription(g_playItemURL), this);
    service->setDescription("Play the item");
    service->addArgument("id", "integer", true, "Identifier of item irrespective of position in list");
    service->addArgument("index", "integer", true, "Index of the item");
    
    service = server->registerService(new HTTPServiceDescription(g_playPrevItemURL), this);
    service->setDescription("Plays the previous item");
    
    service = server->registerService(new HTTPServiceDescription(g_playNextItemURL), this);
    service->setDescription("Plays the next item");
    
    service = server->registerService(new HTTPServiceDescription(g_seekToEOPURL), this);
    service->setDescription("Seek to the end of the programme in the current item");
    
    service = server->registerService(new HTTPServiceDescription(g_markItemStartURL), this);
    service->setDescription("Mark the start of a item at the current playing position");
    
    service = server->registerService(new HTTPServiceDescription(g_clearItemURL), this);
    service->setDescription("Clear the item at given index");
    service->addArgument("id", "integer", true, "Identifier of item irrespective of position in list");
    service->addArgument("index", "integer", true, "Index of the item");
    
    service = server->registerService(new HTTPServiceDescription(g_moveItemUpURL), this);
    service->setDescription("Move the item up the list of items");
    service->addArgument("id", "integer", true, "Identifier of item irrespective of position in list");
    service->addArgument("index", "integer", true, "Index of the item");
    
    service = server->registerService(new HTTPServiceDescription(g_moveItemDownURL), this);
    service->setDescription("Move the item down the list of items");
    service->addArgument("id", "integer", true, "Identifier of item irrespective of position in list");
    service->addArgument("index", "integer", true, "Index of the item");
    
    service = server->registerService(new HTTPServiceDescription(g_disableItemURL), this);
    service->setDescription("Disable the item and move to the end of the list");
    service->addArgument("id", "integer", true, "Identifier of item irrespective of position in list");
    service->addArgument("index", "integer", true, "Index of the item");
    
    service = server->registerService(new HTTPServiceDescription(g_enableItemURL), this);
    service->setDescription("Enable the item and move to after the last item with clip set");
    service->addArgument("id", "integer", true, "Identifier of item irrespective of position in list");
    service->addArgument("index", "integer", true, "Index of the item");
    
    service = server->registerService(new HTTPServiceDescription(g_getItemClipInfoURL), this);
    service->setDescription("Returns the clip information associated with each item in the session");
    
    service = server->registerService(new HTTPServiceDescription(g_getItemSourceInfoURL), this);
    service->setDescription("Returns the source information associated with each item in the session");
    
    
    server->registerSSIHandler(g_ssiGetPSEReportURL, this);
    
    if (scanner != 0)
    {
        scanner->registerListener(this);
    }
}

HTTPRecorder::~HTTPRecorder()
{
}

void HTTPRecorder::newBarcode(string barcode)
{
    LOCK_SECTION(_barcodeMutex);
    
    if (barcode.compare(0, 3, "LTA") == 0)
    {
        // ignore LTO tape barcodes
        return;
    }
    
    _barcode = barcode;
    
    if (barcode.size() > 0)
    {
        _barcodeExpirationTimer.start(BARCODE_EXPIRES_TIME_SEC * SEC_IN_USEC);
        _barcodeCount++;
    }
}

void HTTPRecorder::processRequest(HTTPServiceDescription* serviceDescription, HTTPConnection* connection)
{
    // route the connection to the correct function
    
    if (serviceDescription->getURL() == g_recorderStatusURL)
    {
        getRecorderStatus(connection);
    }
    else if (serviceDescription->getURL() == g_getProfileListURL)
    {
        getProfileList(connection);
    }
    else if (serviceDescription->getURL() == g_getProfileURL)
    {
        getProfile(connection);
    }
    else if (serviceDescription->getURL() == g_updateProfileURL)
    {
        updateProfile(connection);
    }
    else if (serviceDescription->getURL() == g_sourceInfoURL)
    {
        getSourceInfo(connection);
    }
    else if (serviceDescription->getURL() == g_checkSelectedDigibetaURL)
    {
        checkSelectedDigibeta(connection);
    }
    else if (serviceDescription->getURL() == g_newSessionURL)
    {
        startNewSession(connection);
    }
    else if (serviceDescription->getURL() == g_sessionSourceInfoURL)
    {
        getSessionSourceInfo(connection);
    }
    else if (serviceDescription->getURL() == g_startRecordingURL)
    {
        startRecording(connection);
    }
    else if (serviceDescription->getURL() == g_stopRecordingURL)
    {
        stopRecording(connection);
    }
    else if (serviceDescription->getURL() == g_chunkFileURL)
    {
        chunkFile(connection);
    }
    else if (serviceDescription->getURL() == g_completeSessionURL)
    {
        completeSession(connection);
    }
    else if (serviceDescription->getURL() == g_abortSessionURL)
    {
        abortSession(connection);
    }
    else if (serviceDescription->getURL() == g_setSessionCommentsURL)
    {
        setSessionComments(connection);
    }
    else if (serviceDescription->getURL() == g_getSessionCommentsURL)
    {
        getSessionComments(connection);
    }
    else if (serviceDescription->getURL() == g_cacheContentsURL)
    {
        getCacheContents(connection);
    }
    else if (serviceDescription->getURL() == g_confReplayURL)
    {
        confReplayControl(connection);
    }
    else if (serviceDescription->getURL() == g_replayFileURL)
    {
        replayFile(connection);
    }
    else if (serviceDescription->getURL() == g_playItemURL)
    {
        playItem(connection);
    }
    else if (serviceDescription->getURL() == g_playPrevItemURL)
    {
        playPrevItem(connection);
    }
    else if (serviceDescription->getURL() == g_playNextItemURL)
    {
        playNextItem(connection);
    }
    else if (serviceDescription->getURL() == g_seekToEOPURL)
    {
        seekToEOP(connection);
    }
    else if (serviceDescription->getURL() == g_markItemStartURL)
    {
        markItemStart(connection);
    }
    else if (serviceDescription->getURL() == g_clearItemURL)
    {
        clearItem(connection);
    }
    else if (serviceDescription->getURL() == g_moveItemUpURL)
    {
        moveItemUp(connection);
    }
    else if (serviceDescription->getURL() == g_moveItemDownURL)
    {
        moveItemDown(connection);
    }
    else if (serviceDescription->getURL() == g_disableItemURL)
    {
        disableItem(connection);
    }
    else if (serviceDescription->getURL() == g_enableItemURL)
    {
        enableItem(connection);
    }
    else if (serviceDescription->getURL() == g_getItemClipInfoURL)
    {
        getItemClipInfo(connection);
    }
    else if (serviceDescription->getURL() == g_getItemSourceInfoURL)
    {
        getItemSourceInfo(connection);
    }
    else
    {
        connection->sendBadRequest("Unknown URL");
    }
}

void HTTPRecorder::processSSIRequest(string name, HTTPConnection* connection)
{
    if (name == g_ssiGetPSEReportURL)
    {
        if (connection->haveQueryValue("name"))
        {
            connection->responseStream() << join_path(g_pseReportsURL, connection->getQueryValue("name"));
        }
        connection->setResponseDataIsReady();
    }
    else
    {
        connection->sendBadRequest("Unknown URL");
    }
}

void HTTPRecorder::getRecorderStatus(HTTPConnection* connection)
{
    bool includeBarcode = false;
    bool includeSessionRecord = false;
    bool includeSessionReview = false;
    bool includeCache = false;
    bool includeSystem = false;
    bool includeDeveloper = false;
    bool includeReplay = false;
    string barcode;
    int barcodeCount = 0;
    SessionStatus sessionStatus;
    CacheStatus cacheStatus;
    RecorderSystemStatus systemStatus;
    SessionState sessionState;
    SessionResult lastSessionResult = UNKNOWN_SESSION_RESULT;
    string lastSessionSourceSpoolNo;
    string lastSessionFailureReason;
    int apiVersion;
    ConfidenceReplayStatus replayStatus;

    // process the URL query string
    string barcodeArg = connection->getQueryValue("barcode");
    if (barcodeArg == "true")
    {
        includeBarcode = true;
        
        checkBarcodeStatus();
        
        {
            LOCK_SECTION(_barcodeMutex);
            barcode = _barcode;
            barcodeCount = _barcodeCount;
        }
    }
    string sessionArg = connection->getQueryValue("sessionrecord");
    if (sessionArg == "true")
    {
        includeSessionRecord = true;
    }
    else
    {
        sessionArg = connection->getQueryValue("sessionreview");
        if (sessionArg == "true")
        {
            includeSessionReview = true;
            includeReplay = true;
        }
    }
    string developerArg = connection->getQueryValue("developer");
    if (developerArg == "true")
    {
        includeDeveloper = true;
    }
    string replayArg = connection->getQueryValue("replay");
    if (includeReplay || replayArg == "true")
    {
        includeReplay = true;
        replayStatus = _recorder->getConfidenceReplayStatus();
    }
    if (includeSessionRecord || includeSessionReview || includeDeveloper)
    {
        if (_recorder->haveSession())
        {
            sessionStatus = _recorder->getSessionStatus(&replayStatus);
        }
        _recorder->getLastSessionResult(&lastSessionResult, &lastSessionSourceSpoolNo, &lastSessionFailureReason);
    }
    string cacheArg = connection->getQueryValue("cache");
    if (cacheArg == "true")
    {
        includeCache = true;
        cacheStatus = _recorder->getCache()->getStatus();
    }
    string systemArg = connection->getQueryValue("system");
    if (systemArg == "true")
    {
        includeSystem = true;
        systemStatus = _recorder->getSystemStatus();
    }

    
    // get the recorder status
    RecorderStatus status = _recorder->getStatus();
    if (includeSessionRecord || includeSessionReview || includeDeveloper)
    {
        sessionState = sessionStatus.state;
    }
    else
    {
        sessionState = _recorder->getSessionState();
    }
    
    // get the API version
    if (_recorder->tapeBackupEnabled())
    {
        apiVersion = 1;
    }
    else
    {
        apiVersion = 2;
    }
    
    // generate JSON response
    
    JSONObject json;
    json.setString("recorderName", status.recorderName);
    json.setNumber("apiVersion", apiVersion);
    json.setBool("database", status.databaseOk);
    json.setBool("sdiCard", status.sdiCardOk);
    json.setBool("video", status.videoOk);
    json.setBool("audio", status.audioOk);
    json.setBool("vtr", status.vtrOk);
    json.setString("sourceVTRState", get_vtr_state_string(status.sourceVTRState));
    json.setString("digibetaVTRState", get_vtr_state_string(status.digibetaVTRState));
    json.setBool("readyToRecord", status.readyToRecord);
    if (includeBarcode && barcode.size() > 0)
    {
        json.setString("barcode", barcode);
        json.setNumber("barcodeCount", barcodeCount);
    }
    json.setNumber("sessionState", sessionState);
    if (includeSessionRecord)
    {
        JSONObject* jsonSessionStatus = json.setObject("sessionStatus");
        jsonSessionStatus->setNumber("state", sessionStatus.state);
        jsonSessionStatus->setNumber("itemCount", sessionStatus.itemCount);
        jsonSessionStatus->setNumber("sessionCommentsCount", sessionStatus.sessionCommentsCount);
        jsonSessionStatus->setString("sourceSpoolNo", sessionStatus.sourceSpoolNo);
        jsonSessionStatus->setString("digibetaSpoolNo", sessionStatus.digibetaSpoolNo);
        jsonSessionStatus->setString("statusMessage", sessionStatus.statusMessage);
        jsonSessionStatus->setString("vtrState", get_vtr_state_string(sessionStatus.vtrState));
        jsonSessionStatus->setNumber("vtrErrorCount", sessionStatus.vtrErrorCount);
        jsonSessionStatus->setBool("digiBetaDropoutEnabled", sessionStatus.digiBetaDropoutEnabled);
        jsonSessionStatus->setNumber("digiBetaDropoutCount", sessionStatus.digiBetaDropoutCount);
        jsonSessionStatus->setString("startVITC", sessionStatus.startVITC.toString());
        jsonSessionStatus->setString("startLTC", sessionStatus.startLTC.toString());
        jsonSessionStatus->setString("currentVITC", sessionStatus.currentVITC.toString());
        jsonSessionStatus->setString("currentLTC", sessionStatus.currentLTC.toString());
        jsonSessionStatus->setNumber("duration", sessionStatus.duration);
        jsonSessionStatus->setNumber("infaxDuration", sessionStatus.infaxDuration);
        jsonSessionStatus->setString("fileFormat", sessionStatus.fileFormat);
        jsonSessionStatus->setString("filename", sessionStatus.filename);
        jsonSessionStatus->setNumber("fileSize", sessionStatus.fileSize);
        jsonSessionStatus->setNumber("diskSpace", sessionStatus.diskSpace);
        jsonSessionStatus->setString("browseFilename", sessionStatus.browseFilename);
        jsonSessionStatus->setNumber("browseFileSize", sessionStatus.browseFileSize);
        jsonSessionStatus->setBool("browseBufferOverflow", sessionStatus.browseBufferOverflow);
        jsonSessionStatus->setBool("startBusy", sessionStatus.startBusy);
        jsonSessionStatus->setBool("stopBusy", sessionStatus.stopBusy);
        jsonSessionStatus->setBool("abortBusy", sessionStatus.abortBusy);
        jsonSessionStatus->setNumber("lastSessionResult", lastSessionResult);
        jsonSessionStatus->setString("lastSessionSourceSpoolNo", lastSessionSourceSpoolNo);
        jsonSessionStatus->setString("lastSessionFailureReason", lastSessionFailureReason);
        if (includeDeveloper)
        {
            jsonSessionStatus->setNumber("dvsBuffersEmpty", sessionStatus.dvsBuffersEmpty);
            jsonSessionStatus->setNumber("numDVSBuffers", sessionStatus.numDVSBuffers);
            jsonSessionStatus->setNumber("captureBufferPos", sessionStatus.captureBufferPos);
            jsonSessionStatus->setNumber("storeBufferPos", sessionStatus.storeBufferPos);
            jsonSessionStatus->setNumber("browseBufferPos", sessionStatus.browseBufferPos);
            jsonSessionStatus->setNumber("ringBufferSize", sessionStatus.ringBufferSize);
        }
    }
    else if (includeSessionReview)
    {
        JSONObject* jsonSessionStatus = json.setObject("sessionStatus");
        jsonSessionStatus->setNumber("state", sessionStatus.state);
        jsonSessionStatus->setNumber("itemCount", sessionStatus.itemCount);
        jsonSessionStatus->setBool("readyToChunk", sessionStatus.readyToChunk);
        jsonSessionStatus->setNumber("sessionCommentsCount", sessionStatus.sessionCommentsCount);
        jsonSessionStatus->setString("sourceSpoolNo", sessionStatus.sourceSpoolNo);
        jsonSessionStatus->setString("digibetaSpoolNo", sessionStatus.digibetaSpoolNo);
        jsonSessionStatus->setString("statusMessage", sessionStatus.statusMessage);
        jsonSessionStatus->setNumber("vtrErrorCount", sessionStatus.vtrErrorCount);
        jsonSessionStatus->setBool("digiBetaDropoutEnabled", sessionStatus.digiBetaDropoutEnabled);
        jsonSessionStatus->setNumber("digiBetaDropoutCount", sessionStatus.digiBetaDropoutCount);
        jsonSessionStatus->setNumber("pseResult", sessionStatus.pseResult);
        jsonSessionStatus->setNumber("duration", sessionStatus.duration);
        jsonSessionStatus->setNumber("infaxDuration", sessionStatus.infaxDuration);
        jsonSessionStatus->setBool("chunkBusy", sessionStatus.chunkBusy);
        jsonSessionStatus->setBool("completeBusy", sessionStatus.completeBusy);
        jsonSessionStatus->setBool("abortBusy", sessionStatus.abortBusy);
        jsonSessionStatus->setNumber("playingItemId", sessionStatus.playingItemId);
        jsonSessionStatus->setNumber("playingItemIndex", sessionStatus.playingItemIndex);
        jsonSessionStatus->setNumber("playingItemPosition", sessionStatus.playingItemPosition);
        jsonSessionStatus->setNumber("itemClipChangeCount", sessionStatus.itemClipChangeCount);
        jsonSessionStatus->setNumber("itemSourceChangeCount", sessionStatus.itemSourceChangeCount);
        jsonSessionStatus->setNumber("chunkingItemNumber", sessionStatus.chunkingItemNumber);
        jsonSessionStatus->setNumber("chunkingPosition", sessionStatus.chunkingPosition);
        jsonSessionStatus->setNumber("chunkingDuration", sessionStatus.chunkingDuration);
        jsonSessionStatus->setNumber("lastSessionResult", lastSessionResult);
        jsonSessionStatus->setString("lastSessionSourceSpoolNo", lastSessionSourceSpoolNo);
        jsonSessionStatus->setString("lastSessionFailureReason", lastSessionFailureReason);
    }
    if (includeCache)
    {
        json.setNumber("numCacheItems", cacheStatus.numItems);
        json.setNumber("statusChangeCount", cacheStatus.statusChangeCount);
        json.setBool("replayActive", status.replayActive);
        json.setString("replayFilename", status.replayFilename);
    }
    if (includeSystem)
    {
        json.setString("version", get_version());
        json.setString("buildDate", get_build_date());
        json.setNumber("diskSpace", systemStatus.remDiskSpace);
        json.setNumber("recordingTime", systemStatus.remDuration);
        json.setString("recordingTimeIngestFormat", ingest_format_to_string(systemStatus.remDurationIngestFormat, false));
    }
    if (includeReplay)
    {
        JSONObject* jsonReplayStatus = json.setObject("replayStatus");
        jsonReplayStatus->setNumber("position", replayStatus.position);
        jsonReplayStatus->setNumber("duration", replayStatus.duration);
        jsonReplayStatus->setNumber("vtrErrorLevel", replayStatus.vtrErrorLevel);
        jsonReplayStatus->setNumber("markFilter", replayStatus.markFilter);
    }

    connection->sendJSON(&json);
}

void HTTPRecorder::getProfileList(HTTPConnection* connection)
{
    JSONObject json;

    JSONArray* jsonProfileArray = json.setArray("profiles");
    
    // start with the last session profile
    int lastSessionProfileId = -1;
    Profile lastSessionProfileCopy;
    if (_recorder->getLastSessionProfileCopy(&lastSessionProfileCopy))
    {
        JSONObject* jsonProfile = jsonProfileArray->appendObject();
        jsonProfile->setNumber("id", lastSessionProfileCopy.getId());
        jsonProfile->setString("name", lastSessionProfileCopy.getName());
        
        lastSessionProfileId = lastSessionProfileCopy.getId();
    }
    
    map<int, Profile> profileCopies = _recorder->getProfileCopies();
    map<int, Profile>::const_iterator iter;
    for (iter = profileCopies.begin(); iter != profileCopies.end(); iter++)
    {
        if (iter->first != lastSessionProfileId)
        {
            JSONObject* jsonProfile = jsonProfileArray->appendObject();
            jsonProfile->setNumber("id", iter->first);
            jsonProfile->setString("name", iter->second.getName());
        }
    }

    connection->sendJSON(&json);
}

void HTTPRecorder::getProfile(HTTPConnection* connection)
{
    Profile profileCopy;
    int profileId;
    string idArg = connection->getQueryValue("id");
    if (idArg.empty())
    {
        JSONObject json;
        json.setBool("error", true);
        json.setString("errorMessage", "Missing 'id' argument");
        connection->sendJSON(&json);
        return;
    }
    else if (!parse_int(idArg, &profileId) || !_recorder->getProfileCopy(profileId, &profileCopy))
    {
        JSONObject json;
        json.setBool("error", true);
        json.setString("errorMessage", "Invalid ingest profile identifier");
        connection->sendJSON(&json);
        return;
    }
    
    JSONObject json;
    json.setNumber("id", profileCopy.getId());
    json.setString("name", profileCopy.getName());
    
    JSONArray* jsonFormatArray = json.setArray("ingestformats");
    
    map<string, vector<IngestFormat> >::const_iterator iter;
    for (iter = profileCopy.ingest_formats.begin(); iter != profileCopy.ingest_formats.end(); iter++)
    {
        JSONObject* ingestFormat = jsonFormatArray->appendObject();
        ingestFormat->setString("sourceformatcode", iter->first);
        
        JSONArray* alternativeFormats = 0;
        size_t i;
        for (i = 0; i < iter->second.size(); i++)
        {
            if (i == 0)
            {
                JSONObject* selectedFormat = ingestFormat->setObject("selected");
                selectedFormat->setNumber("ingestformatid", iter->second[i]);
                selectedFormat->setString("ingestformatname", ingest_format_to_string(iter->second[i], true));
            }
            else
            {
                if (!alternativeFormats)
                {
                    alternativeFormats = ingestFormat->setArray("alternatives");
                }
                
                JSONObject* alternativeFormat = alternativeFormats->appendObject();
                alternativeFormat->setNumber("ingestformatid", iter->second[i]);
                alternativeFormat->setString("ingestformatname", ingest_format_to_string(iter->second[i], true));
            }
        }
    }
    
    JSONObject* defaultIngestFormat = json.setObject("defaultingestformat");
    JSONArray* alternativeFormats = 0;
    size_t i;
    for (i = 0; i < profileCopy.default_ingest_format.size(); i++)
    {
        if (i == 0)
        {
            JSONObject* selectedFormat = defaultIngestFormat->setObject("selected");
            selectedFormat->setNumber("ingestformatid", profileCopy.default_ingest_format[i]);
            selectedFormat->setString("ingestformatname", 
                ingest_format_to_string(profileCopy.default_ingest_format[i], true));
        }
        else
        {
            if (!alternativeFormats)
            {
                alternativeFormats = defaultIngestFormat->setArray("alternatives");
            }
            
            JSONObject* alternativeFormat = alternativeFormats->appendObject();
            alternativeFormat->setNumber("ingestformatid", profileCopy.default_ingest_format[i]);
            alternativeFormat->setString("ingestformatname", 
                ingest_format_to_string(profileCopy.default_ingest_format[i], true));
        }
    }

    
    JSONObject* primaryTimecode = json.setObject("primarytimecode");
    JSONArray* alternativePrimaryTimecodes = 0;
    PrimaryTimecode p;
    for (p = PRIMARY_TIMECODE_AUTO; p <= PRIMARY_TIMECODE_VITC; p = (PrimaryTimecode)(p + 1))
    {
        if (p == profileCopy.primary_timecode)
        {
            JSONObject* selectedPrimaryTimecode = primaryTimecode->setObject("selected");
            selectedPrimaryTimecode->setNumber("primarytimecodeid", (int)p);
            selectedPrimaryTimecode->setString("primarytimecodename", primary_timecode_to_string(p));
        }
        else
        {
            if (!alternativePrimaryTimecodes)
            {
                alternativePrimaryTimecodes = primaryTimecode->setArray("alternatives");
            }
            
            JSONObject* alternativePrimaryTimecode = alternativePrimaryTimecodes->appendObject();
            alternativePrimaryTimecode->setNumber("primarytimecodeid", (int)p);
            alternativePrimaryTimecode->setString("primarytimecodename", primary_timecode_to_string(p));
        }
    }
    
    
    connection->sendJSON(&json);
}

void HTTPRecorder::updateProfile(HTTPConnection* connection)
{
    // parse and get the ingest profile
    Profile profileCopy;
    int profileId;
    string idArg = connection->getQueryValue("id");
    if (idArg.empty())
    {
        JSONObject json;
        json.setBool("error", true);
        json.setString("errorMessage", "Missing 'id' argument");
        connection->sendJSON(&json);
        return;
    }
    else if (!parse_int(idArg, &profileId) || !_recorder->getProfileCopy(profileId, &profileCopy))
    {
        JSONObject json;
        json.setBool("error", true);
        json.setStringP("errorMessage", "Invalid ingest profile id %d", idArg.c_str());
        connection->sendJSON(&json);
        return;
    }
    
    // parse the updated default ingest format
    IngestFormat defaultIngestFormat;
    string defaultIngestFormatArg = connection->getQueryValue("defaultingestformat");
    if (!defaultIngestFormatArg.empty() && !parse_ingest_format(defaultIngestFormatArg, &defaultIngestFormat))
    {
        JSONObject json;
        json.setBool("error", true);
        json.setStringP("errorMessage", "Invalid default ingest format value %s", defaultIngestFormatArg.c_str());
        connection->sendJSON(&json);
        return;
    }
    
    // parse the updated ingest formats
    map<string, IngestFormat> ingestFormats;
    string ingestFormatsArg = connection->getQueryValue("ingestformats");
    if (!ingestFormatsArg.empty() && !parse_ingest_formats(ingestFormatsArg, &ingestFormats))
    {
        JSONObject json;
        json.setBool("error", true);
        json.setStringP("errorMessage", "Invalid ingest formats value '%s'", ingestFormatsArg.c_str());
        connection->sendJSON(&json);
        return;
    }

    // parse the updated primary timecode
    PrimaryTimecode primaryTimecode = PRIMARY_TIMECODE_AUTO;
    string primaryTimecodeArg = connection->getQueryValue("primarytimecode");
    if (!primaryTimecodeArg.empty() && !parse_primary_timecode(primaryTimecodeArg, &primaryTimecode))
    {
        JSONObject json;
        json.setBool("error", true);
        json.setStringP("errorMessage", "Invalid default primary timecode value %s", primaryTimecodeArg.c_str());
        connection->sendJSON(&json);
        return;
    }
    
    
    // update the profile copy
    
    if (!defaultIngestFormatArg.empty())
    {
        profileCopy.updateDefaultIngestFormat(defaultIngestFormat);
    }
    
    if (!primaryTimecodeArg.empty())
    {
        profileCopy.primary_timecode = primaryTimecode;
    }
    
    map<string, IngestFormat>::const_iterator iter;
    for (iter = ingestFormats.begin(); iter != ingestFormats.end(); iter++)
    {
        if (!profileCopy.updateIngestFormat(iter->first, iter->second))
        {
            JSONObject json;
            json.setBool("error", true);
            json.setStringP("errorMessage", "Invalid ingest format value %d for source '%s'",
                iter->second, iter->first.c_str());
            connection->sendJSON(&json);
            return;
        }
    }
    
    if (!_recorder->updateProfile(&profileCopy))
    {
        JSONObject json;
        json.setBool("error", true);
        json.setStringP("errorMessage", "Failed to update profile '%s'", profileCopy.getName().c_str());
        connection->sendJSON(&json);
        return;
    }
    
    JSONObject json;
    json.setBool("error", false);
    json.setNumber("profileId", profileCopy.getId());
    connection->sendJSON(&json);
}

void HTTPRecorder::getSourceInfo(HTTPConnection* connection)
{
    // process the URL
    
    if (!connection->haveQueryValue("barcode"))
    {
        // no barcode query argument or zero length barcode
        JSONObject json;
        json.setBool("error", true);
        json.setString("barcode", "");
        json.setNumber("errorCode", 0);
        json.setString("errorMessage", "Barcode is empty string");
        connection->sendJSON(&json);
        return;
    }
    string barcode = connection->getQueryValue("barcode");

    bool includeProfiles = false;
    string profilesArg = connection->getQueryValue("profiles");
    if (profilesArg == "true")
    {
        includeProfiles = true;
    }
    

    // load source info
    
    auto_ptr<Source> source(InfaxAccess::getInstance()->findSource(barcode));
    if (source.get() == 0)
    {
        JSONObject json;
        json.setBool("error", true);
        json.setString("barcode", barcode);
        json.setNumber("errorCode", 1);
        json.setString("errorMessage", "Unknown source");
        connection->sendJSON(&json);
        return;
    }
    Logging::info("Loaded source info '%s'\n", barcode.c_str());
    
    
    // check multi-item support
    
    if (!_recorder->checkMultiItemSupport(source.get()))
    {
        JSONObject json;
        json.setBool("error", true);
        json.setString("barcode", barcode);
        json.setNumber("errorCode", 2);
        json.setString("errorMessage", "Multi-item ingest not enabled");
        connection->sendJSON(&json);
        return;
    }
    
    
    // calculate the total Infax duration
    
    int64_t totalInfaxDuration = -1;
    vector<ConcreteSource*>::const_iterator iter;
    for (iter = source->concreteSources.begin(); iter != source->concreteSources.end(); iter++)
    {
        SourceItem* sourceItem = dynamic_cast<SourceItem*>(*iter);
        
        if (sourceItem->duration > 0)
        {
            if (totalInfaxDuration < 0)
            {
                totalInfaxDuration = sourceItem->duration;
            }
            else
            {
                totalInfaxDuration += sourceItem->duration;
            }
        }
    }        
    
    
    // generate the response

    JSONObject json;
    json.setBool("error", false);
    json.setString("barcode", barcode);
        
    JSONArray* itemArray = 0;
    for (iter = source->concreteSources.begin(); iter != source->concreteSources.end(); iter++)
    {
        SourceItem* sourceItem = dynamic_cast<SourceItem*>(*iter);
        
        if (iter == source->concreteSources.begin())
        {
            JSONObject* tapeInfo = json.setObject("tapeInfo");
            tapeInfo->setString("format", sourceItem->format);
            tapeInfo->setString("spoolNo", sourceItem->spoolNo);
            tapeInfo->setString("accNo", sourceItem->accNo);
            tapeInfo->setString("spoolStatus", sourceItem->spoolStatus);
            tapeInfo->setString("stockDate", sourceItem->stockDate.toString());
            tapeInfo->setNumber("totalInfaxDuration", totalInfaxDuration);

            itemArray = json.setArray("items");
        }
        
        JSONObject* items = itemArray->appendObject();
        items->setString("progTitle", sourceItem->progTitle);
        items->setString("spoolDescr", sourceItem->spoolDescr);
        items->setString("episodeTitle", sourceItem->episodeTitle);
        items->setString("txDate", sourceItem->txDate.toString());
        items->setString("progNo", get_complete_prog_no(sourceItem->magPrefix, sourceItem->progNo, sourceItem->prodCode));
        items->setNumber("infaxDuration", sourceItem->duration);
        items->setString("catDetail", sourceItem->catDetail);
        items->setString("memo", sourceItem->memo);
        items->setNumber("itemNo", sourceItem->itemNo);
        items->setString("aspectRatioCode", sourceItem->aspectRatioCode);
        if (Recorder::isValidAspectRatioCode(sourceItem->aspectRatioCode))
        {
            items->setString("rasterAspectRatio", Recorder::getRasterAspectRatio(sourceItem->aspectRatioCode).toAspectRatioString());
        }
    }
    
    if (includeProfiles)
    {
        JSONArray* profileArray = json.setArray("profiles");
        
        // start with the last session profile
        int lastSessionProfileId = -1;
        Profile lastSessionProfileCopy;
        if (_recorder->getLastSessionProfileCopy(&lastSessionProfileCopy))
        {
            JSONObject* jsonProfile = profileArray->appendObject();
            jsonProfile->setNumber("id", lastSessionProfileCopy.getId());
            jsonProfile->setString("name", lastSessionProfileCopy.getName());
            
            lastSessionProfileId = lastSessionProfileCopy.getId();
        }
        
        map<int, Profile> profileCopies = _recorder->getProfileCopies();
        map<int, Profile>::const_iterator iter;
        for (iter = profileCopies.begin(); iter != profileCopies.end(); iter++)
        {
            if (iter->first != lastSessionProfileId)
            {
                JSONObject* jsonProfile = profileArray->appendObject();
                jsonProfile->setNumber("id", iter->first);
                jsonProfile->setString("name", iter->second.getName());
            }
        }
    }

    // set message to warn if the source has been ingested before
    if (RecorderDatabase::getInstance()->sourceUsedInCompletedSession(barcode))
    {
        json.setNumber("warningCode", 0);
        json.setString("warningMessage", "Warning: source has been ingested before");
    }
    
    
    connection->sendJSON(&json);
}

void HTTPRecorder::checkSelectedDigibeta(HTTPConnection* connection)
{
    // check tape backup is enabled
    if (!_recorder->tapeBackupEnabled())
    {
        JSONObject json;
        json.setBool("error", true);
        json.setNumber("errorCode", 0);
        json.setString("errorMessage", "Digibeta barcode check is disabled because tape backup is disabled");
        connection->sendJSON(&json);
        return;
    }
    
    // process the URL and get the barcode argument
    string barcode = connection->getQueryValue("barcode");
    if (!_recorder->isDigibetaBarcode(barcode))
    {
        JSONObject json;
        json.setBool("error", true);
        json.setString("barcode", barcode);
        json.setNumber("errorCode", 0);
        json.setString("errorMessage", "Barcode is not a Digibeta barcode");
        connection->sendJSON(&json);
        return;
    }
    
    
    // check that it is a valid barcode
    if (!_recorder->isDigibetaBarcode(barcode))
    {
        JSONObject json;
        json.setBool("error", true);
        json.setString("barcode", barcode);
        json.setNumber("errorCode", 0);
        json.setString("errorMessage", "Barcode is not a Digibeta barcode");
        connection->sendJSON(&json);
        return;
    }
    
    // generate the response
    
    JSONObject json;
    json.setBool("error", false);
    json.setString("barcode", barcode);
    
    // set message to warn if the digibeta has been used before in a completed session
    if (RecorderDatabase::getInstance()->digibetaUsedInCompletedSession(barcode))
    {
        json.setNumber("warningCode", 0);
        json.setString("warningMessage", "Warning: digibeta has been used before");
    }
    
    connection->sendJSON(&json);
}

void HTTPRecorder::startNewSession(HTTPConnection* connection)
{
    // process the URL and get the barcode argument
    string barcode = connection->getQueryValue("barcode");
    if (barcode.empty())
    {
        // no barcode query argument or zero length barcode
        JSONObject json;
        json.setBool("error", true);
        json.setString("errorMessage", "Missing 'barcode' argument");
        json.setString("barcode", "");
        connection->sendJSON(&json);
        return;
    }
    
    // get the ingest profile
    int profileId;
    string profileIdArg = connection->getQueryValue("profileid");
    if (profileIdArg.empty())
    {
        JSONObject json;
        json.setBool("error", true);
        json.setString("errorMessage", "Missing 'profileid' argument");
        json.setString("barcode", "");
        connection->sendJSON(&json);
        return;
    }
    else if (!parse_int(profileIdArg, &profileId) || !_recorder->haveProfile(profileId))
    {
        JSONObject json;
        json.setBool("error", true);
        json.setString("errorMessage", "Invalid ingest profile identifier");
        json.setString("barcode", "");
        connection->sendJSON(&json);
        return;
    }
    
    string digibetaBarcode = connection->getQueryValue("digibetabarcode");
    if (_recorder->tapeBackupEnabled() && digibetaBarcode.empty())
    {
        // no digibeta barcode query argument or zero length digibeta barcode
        JSONObject json;
        json.setBool("error", true);
        json.setString("errorMessage", "Missing 'digibetabarcode' argument");
        json.setString("barcode", "");
        connection->sendJSON(&json);
        return;
    }
    
    string aspectRatioCodesString = connection->getQueryValue("aspectRatioCodes");
    vector<string> aspectRatioCodes;
    if (!aspectRatioCodesString.empty())
    {
        if (!parse_aspect_ratio_codes(aspectRatioCodesString, &aspectRatioCodes))
        {
            JSONObject json;
            json.setBool("error", true);
            json.setString("errorMessage", "Failed to parse 'aspectRatioCodes' argument");
            json.setString("barcode", "");
            connection->sendJSON(&json);
            return;
        }
    }

    // get the source information
    auto_ptr<Source> source(InfaxAccess::getInstance()->findSource(barcode));
    if (source.get() == 0)
    {
        JSONObject json;
        json.setBool("error", true);
        json.setString("errorMessage", "Unknown source");
        json.setString("barcode", barcode);
        connection->sendJSON(&json);
        return;
    }
    Logging::info("Loaded source info '%s' for new session\n", barcode.c_str());

    
    // update the aspect ratio in the source items
    if (aspectRatioCodes.size() > 0 &&
        !_recorder->updateSourceAspectRatios(source.get(), aspectRatioCodes))
    {
        JSONObject json;
        json.setBool("error", true);
        json.setString("errorMessage", "Invalid 'aspectRatioCodes' argument");
        json.setString("barcode", "");
        connection->sendJSON(&json);
        return;
    }
    
    
    // create the session
    RecordingSession* session;
    int result = _recorder->startNewSession(profileId, source.get(), digibetaBarcode, &session);
    if (result != 0)
    {
        // an error has occurred
        JSONObject json;
        json.setBool("error", true);
        json.setNumber("errorCode", result);
        switch (result)
        {
            case SESSION_IN_PROGRESS_FAILURE:
                json.setString("errorMessage", "Session is already in progress");
                break;
            case VIDEO_SIGNAL_BAD_FAILURE:
                json.setString("errorMessage", "Video signal is bad");
                break;
            case AUDIO_SIGNAL_BAD_FAILURE:
                json.setString("errorMessage", "Audio signal is bad");
                break;
            case SOURCE_VTR_CONNECT_FAILURE:
                json.setString("errorMessage", "No source VTR connection");
                break;
            case SOURCE_VTR_REMOTE_LOCKOUT_FAILURE:
                json.setString("errorMessage", "Source VTR remote lockout");
                break;
            case NO_SOURCE_TAPE:
                json.setString("errorMessage", "No source tape");
                break;
            case DIGIBETA_VTR_CONNECT_FAILURE:
                json.setString("errorMessage", "No Digibeta VTR connection");
                break;
            case DIGIBETA_VTR_REMOTE_LOCKOUT_FAILURE:
                json.setString("errorMessage", "Digibeta VTR remote lockout");
                break;
            case NO_DIGIBETA_TAPE:
                json.setString("errorMessage", "No Digibeta tape");
                break;
            case DISK_SPACE_FAILURE:
                json.setString("errorMessage", "Not enough disk space, <" + get_size_string(DISK_SPACE_MARGIN));
                break;
            case MULTI_ITEM_MXF_EXISTS_FAILURE:
                json.setString("errorMessage", "Multi-item MXF files already in cache");
                break;
            case MULTI_ITEM_NOT_ENABLED_FAILURE:
                json.setString("errorMessage", "Multi-item ingest not enabled");
                break;
            case INVALID_ASPECT_RATIO_FAILURE:
                json.setString("errorMessage", "Item with unknown/invalid aspect ratio");
                break;
            case UNKNOWN_PROFILE_FAILURE:
                json.setString("errorMessage", "Unknown ingest profile identifier");
                break;
            case DISABLED_INGEST_FORMAT_FAILURE:
                json.setString("errorMessage", "Ingest is disabled for given source format");
                break;
            case MULTI_ITEM_INGEST_FORMAT_FAILURE:
                json.setString("errorMessage", "Multi-item ingest not supported for given ingest format");
                break;
            case INTERNAL_FAILURE:
            default:
                json.setString("errorMessage", "Internal server error");
                break;
        }
        json.setString("barcode", barcode);
        connection->sendJSON(&json);
        return;
    }
    
    // reset the barcode
    newBarcode("");
    
    
    JSONObject json;
    json.setBool("error", false);
    json.setString("barcode", barcode);
    connection->sendJSON(&json);
}

void HTTPRecorder::getSessionSourceInfo(HTTPConnection* connection)
{
    RecordingItems* recordingItems = _recorder->getSessionRecordingItems();
    if (recordingItems == 0)
    {
        // no session in progress
        connection->sendBadRequest("No session in progress");
        return;
    }
    

    // get the items and change counts
    
    vector<RecordingItem> items;
    int itemSourceChangeCount;
    int itemClipChangeCount;
    recordingItems->getItems(&items, &itemClipChangeCount, &itemSourceChangeCount);
    
    
    // generate the response
        
    JSONObject json;
    json.setNumber("itemClipChangeCount", itemClipChangeCount);
    json.setNumber("itemSourceChangeCount", itemSourceChangeCount);
    
    JSONArray* itemArray = 0;
    vector<RecordingItem>::const_iterator iter;
    for (iter = items.begin(); iter != items.end(); iter++)
    {
        const RecordingItem& item = *iter;
        
        if (iter == items.begin())
        {
            JSONObject* tapeInfo = json.setObject("tapeInfo");
            tapeInfo->setString("format", item.sourceItem->format);
            tapeInfo->setString("spoolNo", item.sourceItem->spoolNo);
            tapeInfo->setString("accNo", item.sourceItem->accNo);
            tapeInfo->setString("spoolStatus", item.sourceItem->spoolStatus);
            tapeInfo->setString("stockDate", item.sourceItem->stockDate.toString());
            tapeInfo->setNumber("totalInfaxDuration", recordingItems->getTotalInfaxDuration());

            itemArray = json.setArray("items");
        }
        
        JSONObject* items = itemArray->appendObject();
        items->setNumber("id", item.id);
        items->setNumber("index", item.index);
        items->setBool("isDisabled", item.isDisabled);
        items->setBool("isJunk", item.isJunk);
        items->setNumber("itemStartPosition", item.startPosition);
        items->setNumber("itemDuration", item.duration);
        if (!item.isJunk)
        {
            items->setString("progTitle", item.sourceItem->progTitle);
            items->setString("spoolDescr", item.sourceItem->spoolDescr);
            items->setString("episodeTitle", item.sourceItem->episodeTitle);
            items->setString("txDate", item.sourceItem->txDate.toString());
            items->setString("progNo", get_complete_prog_no(item.sourceItem->magPrefix, item.sourceItem->progNo, item.sourceItem->prodCode));
            items->setNumber("infaxDuration", item.sourceItem->duration);
            items->setString("catDetail", item.sourceItem->catDetail);
            items->setString("memo", item.sourceItem->memo);
            items->setNumber("itemNo", item.sourceItem->itemNo);
            items->setString("aspectRatioCode", item.sourceItem->aspectRatioCode);
            if (Recorder::isValidAspectRatioCode(item.sourceItem->aspectRatioCode))
            {
                items->setString("rasterAspectRatio", Recorder::getRasterAspectRatio(item.sourceItem->aspectRatioCode).toAspectRatioString());
            }
        }
    }
    
    connection->sendJSON(&json);
}

void HTTPRecorder::startRecording(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    _recorder->startRecording();
    connection->sendOk();
}

void HTTPRecorder::stopRecording(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    _recorder->stopRecording();
    connection->sendOk();
}

void HTTPRecorder::chunkFile(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }
    
    _recorder->chunkFile();
    connection->sendOk();
}

void HTTPRecorder::completeSession(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    string comments = connection->getPostValue("comments");
    
    _recorder->completeSession(comments);
    connection->sendOk();
}

void HTTPRecorder::abortSession(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }
    
    string comments = connection->getPostValue("comments");

    _recorder->abortSession(true, comments);
    connection->sendOk();
}

void HTTPRecorder::setSessionComments(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }
    
    string comments = connection->getPostValue("comments");

    _recorder->setSessionComments(comments);
    connection->sendOk();
}

void HTTPRecorder::getSessionComments(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }
    
    string comments;
    int count;
    if (!_recorder->getSessionComments(&comments, &count))
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    JSONObject json;
    json.setString("comments", comments);    
    json.setNumber("count", count);    

    connection->sendJSON(&json);
}

void HTTPRecorder::getCacheContents(HTTPConnection* connection)
{
    // load cache contents
    auto_ptr<CacheContents> contents(_recorder->getCache()->getContents());
    if (!contents.get())
    {
        Logging::warning("Failed to load cache contents\n");
        connection->sendServerError("Failed to load cache contents");
        return;
    }

    // get the cache status
    CacheStatus status = _recorder->getCache()->getStatus();

    
    // generate the response
    
    JSONObject json;
    
    json.setString("path", contents->path);
    json.setNumber("statusChangeCount", status.statusChangeCount);
    
    JSONArray* jitems = json.setArray("items");
    vector<CacheContentItem*>::const_iterator iter;
    for (iter = contents->items.begin(); iter != contents->items.end(); iter++)
    {
        JSONObject* tv = jitems->appendObject();
        tv->setNumber("identifier", (*iter)->identifier);
        tv->setString("srcFormat", (*iter)->sourceFormat);
        tv->setString("srcSpoolNo", (*iter)->sourceSpoolNo);
        tv->setNumber("srcItemNo", (*iter)->sourceItemNo);
        tv->setString("srcMPProgNo", get_complete_prog_no((*iter)->sourceMagPrefix, (*iter)->sourceProgNo, (*iter)->sourceProdCode));
        tv->setString("sessionCreation", get_timestamp_string((*iter)->sessionCreation));
        tv->setNumber("sessionStatus", (*iter)->sessionStatus);
        tv->setString("sessionStatusString", get_session_status_string((*iter)->sessionStatus));
        tv->setString("name", (*iter)->name);
        tv->setNumber("size", (*iter)->size);
        tv->setNumber("duration", (*iter)->duration);
        string pseURL = g_pseReportFramedURL;
        pseURL += "?name=" + (*iter)->pseName;
        tv->setString("pseURL", pseURL);
        tv->setNumber("pseResult", (*iter)->pseResult);
    }
    
    connection->sendJSON(&json);
}

void HTTPRecorder::confReplayControl(HTTPConnection* connection)
{
    if (!_recorder->confidenceReplayActive())
    {
        connection->sendBadRequest("Confidence replay is not active");
        return;
    }
    
    // get the command
    string requestURI = connection->getRequestURI();
    size_t prefixIndex;
    if ((prefixIndex = requestURI.find("confreplay/")) == string::npos)
    {
        connection->sendServerError("Unexpected URL format");
        return;
    }
    string command = requestURI.substr(prefixIndex + strlen("confreplay/"));
    
    // add the query string
    string queryString = connection->getQueryString();
    if (queryString.size() > 0)
    {
        command += "?" + queryString;
    }

    // forward to replay player    
    _recorder->forwardConfidenceReplayControl(command);
    connection->sendOk();
}

void HTTPRecorder::replayFile(HTTPConnection* connection)
{
    // check for the 'name' argument
    if (!connection->haveQueryValue("name"))
    {
        connection->sendBadRequest("Missing 'name' argument");
        return;
    }
    string filename = connection->getQueryValue("name");
    
    // replay file
    int result = _recorder->replayFile(filename);
    if (result != 0)
    {
        switch (result)
        {
            case SESSION_IN_PROGRESS_REPLAY_FAILURE:
                connection->sendBadRequest("Recording session in progress");
                break;
            case UNKNOWN_FILE_REPLAY_FAILURE:
                connection->sendBadRequest("Unknown file");
                break;
            case INTERNAL_REPLAY_FAILURE:
            default:
                connection->sendServerError("Replay failed");
                break;
        }

        return;
    }

    connection->sendOk();
}

void HTTPRecorder::playItem(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    int id;
    string idStr = connection->getQueryValue("id");
    if (idStr.empty() || !parse_int(idStr, &id))
    {
        connection->sendBadRequest("Missing 'id' query value");
        return;
    }
    
    int index;
    string indexStr = connection->getQueryValue("index");
    if (indexStr.empty() || !parse_int(indexStr, &index))
    {
        connection->sendBadRequest("Missing 'index' query value");
        return;
    }
    
    _recorder->playItem(id, index);
    connection->sendOk();
}

void HTTPRecorder::playPrevItem(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    _recorder->playPrevItem();
    connection->sendOk();
}

void HTTPRecorder::playNextItem(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    _recorder->playNextItem();
    connection->sendOk();
}

void HTTPRecorder::seekToEOP(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    _recorder->seekToEOP();
    connection->sendOk();
}

void HTTPRecorder::markItemStart(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    int itemId;
    bool isJunk;
    int64_t filePosition;
    int64_t fileDuration;
    int changeCount = _recorder->markItemStart(&itemId, &isJunk, &filePosition, &fileDuration);
    if (changeCount < 0)
    {
        connection->sendBadRequest("Failed to mark item start");
        return;
    }
    
    JSONObject json;
    json.setNumber("itemId", itemId);
    json.setBool("isJunk", isJunk);
    json.setNumber("filePosition", filePosition);
    json.setNumber("fileDuration", fileDuration);
    json.setNumber("itemClipChangeCount", changeCount);
    connection->sendJSON(&json);
}

void HTTPRecorder::clearItem(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    int id;
    string idStr = connection->getQueryValue("id");
    if (idStr.empty() || !parse_int(idStr, &id))
    {
        connection->sendBadRequest("Missing 'id' query value");
        return;
    }
    
    int index;
    string indexStr = connection->getQueryValue("index");
    if (indexStr.empty() || !parse_int(indexStr, &index))
    {
        connection->sendBadRequest("Missing 'index' query value");
        return;
    }
    
    int64_t filePosition;
    int changeCount = _recorder->clearItem(id, index, &filePosition);
    if (changeCount < 0)
    {
        connection->sendBadRequest("Failed to clear item");
        return;
    }

    JSONObject json;
    json.setNumber("itemId", id);
    json.setNumber("filePosition", filePosition);
    json.setNumber("itemClipChangeCount", changeCount);
    connection->sendJSON(&json);
}

void HTTPRecorder::moveItemUp(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    int id;
    string idStr = connection->getQueryValue("id");
    if (idStr.empty() || !parse_int(idStr, &id))
    {
        connection->sendBadRequest("Missing 'id' query value");
        return;
    }
    
    int index;
    string indexStr = connection->getQueryValue("index");
    if (indexStr.empty() || !parse_int(indexStr, &index))
    {
        connection->sendBadRequest("Missing 'index' query value");
        return;
    }
    
    int changeCount = _recorder->moveItemUp(id, index);
    if (changeCount < 0)
    {
        connection->sendBadRequest("Failed to move item up");
        return;
    }
        
    JSONObject json;
    json.setNumber("itemSourceChangeCount", changeCount);
    connection->sendJSON(&json);
}

void HTTPRecorder::moveItemDown(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    int id;
    string idStr = connection->getQueryValue("id");
    if (idStr.empty() || !parse_int(idStr, &id))
    {
        connection->sendBadRequest("Missing 'id' query value");
        return;
    }
    
    int index;
    string indexStr = connection->getQueryValue("index");
    if (indexStr.empty() || !parse_int(indexStr, &index))
    {
        connection->sendBadRequest("Missing 'index' query value");
        return;
    }
    
    int changeCount = _recorder->moveItemDown(id, index);
    if (changeCount < 0)
    {
        connection->sendBadRequest("Failed to move item down");
        return;
    }
    
    JSONObject json;
    json.setNumber("itemSourceChangeCount", changeCount);
    connection->sendJSON(&json);
}

void HTTPRecorder::disableItem(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    int id;
    string idStr = connection->getQueryValue("id");
    if (idStr.empty() || !parse_int(idStr, &id))
    {
        connection->sendBadRequest("Missing 'id' query value");
        return;
    }
    
    int index;
    string indexStr = connection->getQueryValue("index");
    if (indexStr.empty() || !parse_int(indexStr, &index))
    {
        connection->sendBadRequest("Missing 'index' query value");
        return;
    }
    
    int changeCount = _recorder->disableItem(id, index);
    if (changeCount < 0)
    {
        connection->sendBadRequest("Failed to disable item");
        return;
    }
    
    JSONObject json;
    json.setNumber("itemSourceChangeCount", changeCount);
    connection->sendJSON(&json);
}

void HTTPRecorder::enableItem(HTTPConnection* connection)
{
    if (!_recorder->haveSession())
    {
        connection->sendBadRequest("No session in progress");
        return;
    }

    int id;
    string idStr = connection->getQueryValue("id");
    if (idStr.empty() || !parse_int(idStr, &id))
    {
        connection->sendBadRequest("Missing 'id' query value");
        return;
    }
    
    int index;
    string indexStr = connection->getQueryValue("index");
    if (indexStr.empty() || !parse_int(indexStr, &index))
    {
        connection->sendBadRequest("Missing 'index' query value");
        return;
    }
    
    int changeCount = _recorder->enableItem(id, index);
    if (changeCount < 0)
    {
        connection->sendBadRequest("Failed to enable item");
        return;
    }
    
    JSONObject json;
    json.setNumber("itemSourceChangeCount", changeCount);
    connection->sendJSON(&json);
}

void HTTPRecorder::getItemClipInfo(HTTPConnection* connection)
{
    RecordingItems* recordingItems = _recorder->getSessionRecordingItems();
    if (recordingItems == 0)
    {
        // no session in progress
        connection->sendBadRequest("No session in progress");
        return;
    }
    

    // get the items and change counts
    
    vector<RecordingItem> items;
    int itemSourceChangeCount;
    int itemClipChangeCount;
    recordingItems->getItems(&items, &itemClipChangeCount, &itemSourceChangeCount);
    
    
    // generate the response
        
    JSONObject json;
    json.setNumber("itemClipChangeCount", itemClipChangeCount);
    json.setNumber("itemSourceChangeCount", itemSourceChangeCount);

    JSONArray* itemArray = json.setArray("items");
    vector<RecordingItem>::const_iterator iter;
    for (iter = items.begin(); iter != items.end(); iter++)
    {
        const RecordingItem& item = *iter;

        JSONObject* items = itemArray->appendObject();
        items->setNumber("id", item.id);
        items->setNumber("index", item.index);
        items->setBool("isJunk", item.isJunk);
        items->setNumber("itemStartPosition", item.startPosition);
        items->setNumber("itemDuration", item.duration);
    }
    
    connection->sendJSON(&json);
}

void HTTPRecorder::getItemSourceInfo(HTTPConnection* connection)
{
    RecordingItems* recordingItems = _recorder->getSessionRecordingItems();
    if (recordingItems == 0)
    {
        // no session in progress
        connection->sendBadRequest("No session in progress");
        return;
    }
    

    // get the items and change counts
    
    vector<RecordingItem> items;
    int itemSourceChangeCount;
    int itemClipChangeCount;
    recordingItems->getItems(&items, &itemClipChangeCount, &itemSourceChangeCount);
    
    
    // generate the response
        
    JSONObject json;
    json.setNumber("itemClipChangeCount", itemClipChangeCount);
    json.setNumber("itemSourceChangeCount", itemSourceChangeCount);

    JSONArray* itemArray = json.setArray("items");
    vector<RecordingItem>::const_iterator iter;
    for (iter = items.begin(); iter != items.end(); iter++)
    {
        const RecordingItem& item = *iter;

        JSONObject* items = itemArray->appendObject();
        items->setNumber("id", item.id);
        items->setNumber("index", item.index);
    }
    
    connection->sendJSON(&json);
}

string HTTPRecorder::getPSEReportsURL()
{
    return g_pseReportsURL;
}

void HTTPRecorder::checkBarcodeStatus()
{
    LOCK_SECTION(_barcodeMutex);
    
    if (_barcode.size() > 0 && _barcodeExpirationTimer.timeLeft() == 0)
    {
        _barcode = "";
    }
}

