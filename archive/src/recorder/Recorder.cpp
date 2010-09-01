/*
 * $Id: Recorder.cpp,v 1.2 2010/09/01 16:05:22 philipn Exp $
 *
 * Provides main access to the recorder application
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
    The Recorder is used to start and stop a recording session. Access to the
    Recording Session is provided through the Recorder.
    
    The Recorder listens to the state of the DVS capture, preventing a session
    from starting if the signal is bad or initiating a session abort if the 
    signal is bad or buffer overflows whilst recording.
    
    The Recorder can be used to playback a file and control the player.
*/


#include "Recorder.h"
#include "DummyVTRControl.h"
#include "RecorderDatabase.h"
#include "ArchiveMXFWriter.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Config.h"


using namespace std;
using namespace rec;



#define SESSION_ACCESS_SECTION() \
    checkSessionStatus(); \
    READ_LOCK_SECTION(_sessionRWLock);

#define REPLAY_ACCESS_SECTION() \
   LOCK_SECTION(_replayMutex); \
   if (_replay != 0 && !_replay->isRunning()) \
   { \
       ConfidenceReplayStatus replayStatus = _replay->getStatus(); \
       try \
       { \
           SAFE_DELETE(_replay); \
           Logging::info("Closed replay of cache file '%s'\n", replayStatus.filename.c_str()); \
       } \
       catch (...) \
       { \
           Logging::error("Failed to delete replay for cache file '%s'\n", replayStatus.filename.c_str()); \
           _replay = 0; \
       } \
   }  




static int check_vtr_ready(VTRControl* control)
{
    if (control == 0)
    {
        return 1;
    }
    VTRState vtrState = control->getState();
    if (vtrState == NOT_CONNECTED_VTR_STATE)
    {
        return 2;
    }
    else if (vtrState == REMOTE_LOCKOUT_VTR_STATE)
    {
        return 3;
    }
    else if (vtrState == TAPE_UNTHREADED_VTR_STATE || vtrState == EJECTING_VTR_STATE)
    {
        return 4;
    }
    
    return 0;
}

static bool parse_vtr_device_type_code(string symbol, int* deviceTypeCode)
{
    if (symbol.size() < 4)
    {
        return false;
    }
    
    int code;
    if (sscanf(&symbol.c_str()[symbol.size() - 4], "%x", &code) != 1)
    {
        return false;
    }
    
    *deviceTypeCode = code;
    return true;
}

static void parse_config_vtr_list(vector<string> &stringArray, vector<int>* deviceTypeCodes)
{
    size_t i;
    int deviceTypeCode;
    for (i = 0; i < stringArray.size(); i++)
    {
        if (!parse_vtr_device_type_code(stringArray[i], &deviceTypeCode))
        {
            REC_LOGTHROW(("Failed to parse VTR device code '%s'", stringArray[i].c_str()));
        }
        
        deviceTypeCodes->push_back(deviceTypeCode);
    }
}



Rational Recorder::getRasterAspectRatio(string aspectRatioCode)
{
    // handle special cases used internally
    
    if (aspectRatioCode == "xxx12")
    {
        // 4:3 raster aspect ratio, with unknown full code
        return Rational(4, 3);
    }
    else if (aspectRatioCode == "xxx16")
    {
        // 16:9 raster aspect ratio, with unknown full code
        return Rational(16, 9);
    }
    
    
    // parse the aspect ratio code
    
    int activeImageAR = 0;
    char displayFormat = 0;
    int rasterImageAR = 0;
    char protection = 0;
    
    string number;
    int state = 0;
    size_t i = 0;
    while (i < aspectRatioCode.size() && state < 4)
    {
        switch (state)
        {
            case 0: // parse active image aspect ratio
                if (aspectRatioCode[i] >= '0' && aspectRatioCode[i] <= '9')
                {
                    number.push_back(aspectRatioCode[i]);
                    i++;
                }
                else
                {
                    if (sscanf(number.c_str(), "%d", &activeImageAR) != 1)
                    {
                        return g_nullRational;
                    }
                    state = 1;
                    number.clear();
                }
                break;
                
            case 1: // parse display format char
                if (aspectRatioCode[i] != 'P' && aspectRatioCode[i] != 'L' && aspectRatioCode[i] != 'F' &&
                    aspectRatioCode[i] != 'M')
                {
                    return g_nullRational;
                }
                displayFormat = aspectRatioCode[i];
                state = 2;
                i++;
                break;
                
            case 2: // parse raster image aspect ratio
                if (aspectRatioCode[i] >= '0' && aspectRatioCode[i] <= '9')
                {
                    number.push_back(aspectRatioCode[i]);
                    i++;
                }
                else
                {
                    if (sscanf(number.c_str(), "%d", &rasterImageAR) != 1)
                    {
                        return g_nullRational;
                    }
                    state = 3;
                }
                break;

            case 3: // parse protection char
                if (aspectRatioCode[i] != 'A' && aspectRatioCode[i] != 'B' && aspectRatioCode[i] != 'C' &&
                    aspectRatioCode[i] != 'N')
                {
                    return g_nullRational;
                }
                protection = aspectRatioCode[i];
                state = 4;
                i++;
                break;
                
            default:
                break;
        }
    }
    if (state == 2 && number.size() > 0)
    {
        if (sscanf(number.c_str(), "%d", &rasterImageAR) != 1)
        {
            return g_nullRational;
        }
    }
    
    
    if (rasterImageAR == 12)
    {
        return Rational(4, 3);
    }
    else if (rasterImageAR == 16)
    {
        return Rational(16, 9);
    }

    // only 4:3 (12:9) and 16:9 raster aspect ratios are supported 
    return g_nullRational;
}

bool Recorder::isValidAspectRatioCode(string aspectRatioCode)
{
    return !getRasterAspectRatio(aspectRatioCode).isNull();
}




Recorder::Recorder(string name, std::string cacheDirectory, std::string browseDirectory, std::string pseDirectory, 
    int replayPort, string vtrSerialDeviceName1, string vtrSerialDeviceName2)
: _profileManager(0), _lastSessionProfileId(-1), _name(name), _videotapeBackup(false), _replayPort(replayPort),
_capture(0), _vtrControl1(0), _vtrControl2(0),
_sessionDone(false), _session(0), _sessionThread(0),
_lastSessionRecordingItems(0), _lastSessionResult(UNKNOWN_SESSION_RESULT),
_cache(0), _recorderTable(0), _replay(0), _jogShuttleControl(0)
{
    // create and load profiles
    
    _profileManager = new ProfileManager(Config::profile_directory, Config::profile_filename_suffix);

    
    // assume initially 8-bit uncompressed with 4 audio tracks
    _remDurationFactor = ArchiveMXFWriter::getContentPackageSize(false, 4, true);
    _remDurationIngestFormat = MXF_UNC_8BIT_INGEST_FORMAT;

    // create and initialise SDI capture system
    
    _capture = new ::Capture(this);
    
    _capture->set_loglev(Config::capture_log_level);
    _capture->set_browse_thread_count(Config::browse_thread_count);
    _capture->set_browse_overflow_frames(Config::browse_overflow_frames);
    _capture->set_digibeta_dropout_thresholds(Config::digibeta_dropout_lower_threshold,
        Config::digibeta_dropout_upper_threshold,
        Config::digibeta_dropout_store_threshold);

    bool palff_mode = Config::palff_mode;
    _capture->set_palff_mode(palff_mode);
    if (palff_mode)
    {
        vector<int> vitcLines = Config::vitc_lines;
        vector<int> ltcLines = Config::ltc_lines;
        
        _capture->set_vitc_lines(&vitcLines[0], (int)vitcLines.size());
        _capture->set_ltc_lines(&ltcLines[0], (int)ltcLines.size());
    }
    
    if (Config::read_analogue_ltc)
    {
        _capture->set_ltc_source(ANALOGUE_LTC_SOURCE, -1);
    }
    else if (Config::read_digital_ltc)
    {
        _capture->set_ltc_source(DIGITAL_LTC_SOURCE, -1);
    }
    else if (Config::read_audio_track_ltc >= 0)
    {
        _capture->set_ltc_source(AUDIO_TRACK_LTC_SOURCE, Config::read_audio_track_ltc);
    }
    else if (palff_mode)
    {
        _capture->set_ltc_source(VBI_LTC_SOURCE, -1);
    }
    else
    {
        _capture->set_ltc_source(NO_LTC_SOURCE, -1);
    }

    if (palff_mode)
    {
        _capture->set_vitc_source(VBI_VITC_SOURCE);
    }
    else
    {
        _capture->set_vitc_source(ANALOGUE_VITC_SOURCE);
    }
    
    _capture->set_debug_avsync(Config::dbg_av_sync);
    _capture->set_debug_vitc_failures(Config::dbg_vitc_failures);
    _capture->set_debug_store_thread_buffer(Config::dbg_store_thread_buffer);
    _capture->set_mxf_page_size(MXF_PAGE_FILE_SIZE);
    
    if (!_capture->init_capture(Config::ring_buffer_size))
    {
        REC_LOGTHROW(("Failed to initialise the DVS SDI Ingex capture system"));
    }
    Logging::info("Initialised the capture system\n");
    
    
    // videotape backup configs
    _videotapeBackup = Config::videotape_backup;
    if (_videotapeBackup)
    {
        parse_config_vtr_list(Config::source_vtr, &_sourceVTRDeviceTypeCodes);
        parse_config_vtr_list(Config::backup_vtr, &_backupVTRDeviceTypeCodes);
        
        if (_sourceVTRDeviceTypeCodes.empty() && _backupVTRDeviceTypeCodes.empty())
        {
            REC_LOGTHROW(("Missing source_vtr and/or backup_vtr config values")); 
        }
        
        _digibetaBarcodePrefixes = Config::digibeta_barcode_prefixes;
    }

    
    // connect to VTRs
    if (vtrSerialDeviceName1 == "DUMMY_D3")
    {
        _vtrControl1 = new DummyVTRControl(AJ_D350_625LINE_DEVICE_TYPE);
        Logging::warning("Opened dummy D3 vtr control 1\n");
    }
    else if (vtrSerialDeviceName1 == "DUMMY_DIGIBETA")
    {
        _vtrControl1 = new DummyVTRControl(DVW_A500_625LINE_DEVICE_TYPE);
        Logging::warning("Opened dummy Digibeta vtr control 1\n");
    }
    else
    {
        _vtrControl1 = new DefaultVTRControl(vtrSerialDeviceName1, Config::recorder_vtr_serial_type_1);
        Logging::info("Using vtr control serial device 1 '%s'\n", vtrSerialDeviceName1.c_str());
    }
    
    if (_videotapeBackup)
    {
        if (vtrSerialDeviceName2 == "DUMMY_D3")
        {
            _vtrControl2 = new DummyVTRControl(AJ_D350_625LINE_DEVICE_TYPE);
            Logging::warning("Opened dummy D3 vtr control 2\n");
        }
        else if (vtrSerialDeviceName2 == "DUMMY_DIGIBETA")
        {
            _vtrControl2 = new DummyVTRControl(DVW_A500_625LINE_DEVICE_TYPE);
            Logging::warning("Opened dummy Digibeta vtr control 2\n");
        }
        else
        {
           _vtrControl2 = new DefaultVTRControl(vtrSerialDeviceName2, Config::recorder_vtr_serial_type_2);
            Logging::info("Using vtr control serial device 2 '%s'\n", vtrSerialDeviceName2.c_str());
        }
    }
    
    _vtrControl1->registerListener(this);
    if (_videotapeBackup)
    {
        _vtrControl2->registerListener(this);
    }
    
    // server-side jog-shuttle control
    _jogShuttleControl = new JogShuttleControl(this);
    
    
    // load or create recorder data from the database
    _recorderTable = RecorderDatabase::getInstance()->loadRecorder(name);
    if (_recorderTable)
    {
        Logging::info("Loaded recorder '%s' data from database\n", name.c_str());
    }
    else
    {
        auto_ptr<RecorderTable> newRecorderTable(new RecorderTable());
        newRecorderTable->name = name;
        RecorderDatabase::getInstance()->saveRecorder(newRecorderTable.get());
        _recorderTable = newRecorderTable.release();

        Logging::info("Created and saved recorder '%s' data to database\n", name.c_str());
    }
    
    
    // create the cache
    _cache = new Cache(_recorderTable->databaseId, cacheDirectory, browseDirectory, pseDirectory);
    
    
    // abort sessions that were left in the start state

    AutoPointerVector<RecordingSessionTable> startedSessions(
        RecorderDatabase::getInstance()->loadMinimalSessions(_recorderTable->databaseId, 
            RECORDING_SESSION_STARTED));
            
    vector<RecordingSessionTable*>::const_iterator sessIter;
    for (sessIter = startedSessions.get().begin(); sessIter != startedSessions.get().end(); sessIter++)
    {
        // update session status to abort
        (*sessIter)->status = RECORDING_SESSION_ABORTED;
        (*sessIter)->abortInitiator = SYSTEM_ABORT_SESSION;
        (*sessIter)->comments = "Existing session was aborted at recorder system startup";
        RecorderDatabase::getInstance()->updateSession(*sessIter);
        Logging::warning("Aborted existing session (%s) when restarting recorder system\n", 
            get_user_timestamp_string((*sessIter)->creation).c_str());
        
        // remove items created by the session from the cache
        // note: any files on disk have probably already been deleted from the cache creating
        // directory when creating the _cache above
        AutoPointerVector<Destination> sessionDestinations(
            RecorderDatabase::getInstance()->loadSessionDestinations((*sessIter)->databaseId));
        HardDiskDestination* hdd;
        vector<Destination*>::iterator destIter;
        for (destIter = sessionDestinations.get().begin(); destIter != sessionDestinations.get().end(); destIter++)
        {
            if ((*destIter)->concreteDestination != 0 && 
                (*destIter)->concreteDestination->getTypeId() == HARD_DISK_DEST_TYPE)
            {
                hdd = dynamic_cast<HardDiskDestination*>((*destIter)->concreteDestination);
                if (_cache->removeItem(hdd->databaseId))
                {
                    Logging::warning("Removed file '%s' from aborted session from cache\n", hdd->name.c_str());
                }
            }
        }
    }
}

Recorder::~Recorder()
{
    delete _profileManager;
    delete _capture;
    // _session is owned by _sessionThread
    delete _sessionThread;
    delete _lastSessionRecordingItems;
    delete _recorderTable;
    delete _vtrControl1;
    delete _vtrControl2;
    delete _replay;
}

void Recorder::sdiStatusChanged(bool videoOK, bool audioOK, bool dltcOk, bool vitcOK, bool recordingOK)
{
    // not used
    (void)dltcOk;
    (void)vitcOK;
    
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        if (!videoOK || !audioOK)
        {
            // only abort if the recording was started or was not stopped
            if (_session->abortIfRecording(false, "SDI signal bad"))
            {
                Logging::warning("Sending system 'abort if recording' to recording session because SDI signal is bad\n"); 
            }
        }
        else if (!recordingOK)
        {
            // only abort if the recording was started or was not stopped
            if (_session->abortIfRecording(false, "Capture system stopped"))
            {
                Logging::warning("Sending system 'abort if recording' to recording session because capture system stopped\n"); 
            }
        }
    }
}

void Recorder::sdiCaptureDroppedFrame()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        Logging::warning("Sending system 'abort' because the SDI capture dropped a frame\n"); 
        _session->abort(false, "SDI capture dropped frame");
    }
}

void Recorder::mxfBufferOverflow()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        Logging::warning("Sending system 'abort' because the MXF buffer has overflowed - problem with disk I/O?\n"); 
        _session->abort(false, "MXF buffer overflow");
    }
}

void Recorder::browseBufferOverflow()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        _session->browseBufferOverflow();
    }
}

void Recorder::vtrDeviceType(VTRControl* vtrControl, int deviceTypeCode, DeviceType deviceType)
{
    if (_videotapeBackup)
    {
        // only poll the extended state from the source VTR
        vtrControl->pollExtState(isSourceVTR(deviceTypeCode));
    }
    else
    {
        vtrControl->pollExtState(true);
    }
}

int Recorder::startNewSession(int profileId, Source* source, string digibetaBarcode, RecordingSession** session)
{
    SESSION_ACCESS_SECTION();
    LOCK_SECTION(_profileManagerMutex);
    
    // no session must be active
    if (_session)
    {
        return SESSION_IN_PROGRESS_FAILURE;
    }

    // must be ready to record
    ::GeneralStats captureStats;
    _capture->get_general_stats(&captureStats);

    if (!captureStats.video_ok)
    {
        return VIDEO_SIGNAL_BAD_FAILURE;
    }
    if (!captureStats.audio_ok)
    {
        return AUDIO_SIGNAL_BAD_FAILURE;
    }
    if (captureStats.recording)
    {
        Logging::warning("Capture system running without a controlling recording session\n");
        if (_capture->abort_record())
        {
            Logging::info("Capture was aborted\n");
        }
        else
        {
            // TODO: need to halt the recorder completely
            Logging::warning("Failed to abort capture\n");
        }
        return INTERNAL_FAILURE;
    }

    
    // check source and Digibeta VTRs can be controlled and that tape are present
    
    VTRControl* sourceVTRControl = getSourceVTRControl();
    int result = check_vtr_ready(sourceVTRControl);
    if (result != 0)
    {
        switch (result)
        {
            case 3:
                return SOURCE_VTR_REMOTE_LOCKOUT_FAILURE;
            case 4:
                return NO_SOURCE_TAPE;
            case 1:
            case 2:
            default:
                return SOURCE_VTR_CONNECT_FAILURE;
        }
    }

    VTRControl* digibetaVTRControl = 0;
    if (_videotapeBackup)
    {
        digibetaVTRControl = getBackupVTRControl();
        result = check_vtr_ready(digibetaVTRControl);
        if (result != 0)
        {
            switch (result)
            {
                case 3:
                    return DIGIBETA_VTR_REMOTE_LOCKOUT_FAILURE;
                case 4:
                    return NO_DIGIBETA_TAPE;
                case 1:
                case 2:
                default:
                    return DIGIBETA_VTR_CONNECT_FAILURE;
            }
        }
    }
    
    
    // check that multi-item support is enabled if source has multiple items
    if (!checkMultiItemSupport(source))
    {
        return MULTI_ITEM_NOT_ENABLED_FAILURE;
    }
    
    // check the multi-item MXF file are not present in the cache
    if (source->concreteSources.size() > 1 &&
        !_cache->checkMultiItemMXF(source->barcode, source->concreteSources.size()))
    {
        return MULTI_ITEM_MXF_EXISTS_FAILURE;
    }
    
    // check all source item aspect ratios are known
    if (!checkSourceAspectRatios(source))
    {
        return INVALID_ASPECT_RATIO_FAILURE;
    }
    
    // get the profile
    Profile* profile = _profileManager->getProfile(profileId);
    if (profile == NULL)
    {
        return UNKNOWN_PROFILE_FAILURE;
    }
    
    // get ingest format
    IngestFormat ingestFormat = profile->getIngestFormat(source->getFirstSourceItem()->format);
    
    // check not disabled
    if (ingestFormat == UNKNOWN_INGEST_FORMAT)
    {
        return DISABLED_INGEST_FORMAT_FAILURE;
    }
    
    // check multi-item ingest is supported for ingest format
    if (source->concreteSources.size() > 1 &&
        ingestFormat != MXF_UNC_8BIT_INGEST_FORMAT &&
        ingestFormat != MXF_UNC_10BIT_INGEST_FORMAT &&
        ingestFormat != MXF_D10_50_INGEST_FORMAT)
    {
        return MULTI_ITEM_INGEST_FORMAT_FAILURE;
    }
    
    
    // check minimum disk space is available
    if (getRemainingDiskSpace() == 0)
    {
        return DISK_SPACE_FAILURE;
    }
    
    
    // stop replay
    try
    {
        REPLAY_ACCESS_SECTION();

        if (_replay)
        {
            SAFE_DELETE(_replay);
            Logging::info("Closed replay of cache file\n");
        }
    }
    catch (...)
    {
        Logging::error("Failed to stop file replay\n");
        _replay = 0;
        return INTERNAL_FAILURE;
    }
    
    // start the session
    try
    {
        _session = new RecordingSession(this, profile, source, digibetaBarcode, _replayPort,
            sourceVTRControl, digibetaVTRControl);
        _sessionThread = new Thread(_session, true);
        _sessionThread->start();
        
        _lastSessionProfileId = profile->getId();
        
        {
            LOCK_SECTION(_remDurationMutex);
            
            _remDurationFactor = _session->getContentPackageSize();
            _remDurationIngestFormat = _session->getIngestFormat();
        }
    }
    catch (...)
    {
        Logging::error("Failed to start a new session\n");
        return INTERNAL_FAILURE;
    }
    
    *session = _session;
    return 0;
}

bool Recorder::haveSession()
{
    SESSION_ACCESS_SECTION();
    
    return _session != 0;
}    

SessionState Recorder::getSessionState()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->getState();
    }
    
    return SessionState();
}

SessionStatus Recorder::getSessionStatus(const ConfidenceReplayStatus* replayStatus)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->getStatus(replayStatus);
    }
    
    return SessionStatus();
}

void Recorder::startRecording()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        Logging::info("Sending user 'start record' to recording session\n");
        _session->startRecording();
    }
}

void Recorder::stopRecording()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        Logging::info("Sending user 'stop record' to recording session\n");
        _session->stopRecording();
    }
}

void Recorder::chunkFile()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        Logging::info("Sending user 'chunk file' to recording session\n");
        _session->chunkFile();
    }
}

void Recorder::completeSession(string comments)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        Logging::info("Sending user 'complete session' to recording session\n");
        _session->complete(comments);
    }
}

void Recorder::abortSession(bool fromUser, string comments)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        Logging::info("Sending user 'abort session' to recording session\n");
        _session->abort(fromUser, comments);
    }
}

void Recorder::setSessionComments(string comments)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        string trimmedComments = trim_string(comments);
        if (!trimmedComments.empty())
        {
            Logging::info("Setting session comments\n");
            _session->setSessionComments(trimmedComments);
        }
    }
}

bool Recorder::confidenceReplayActive()
{
    // try local replay
    {
        REPLAY_ACCESS_SECTION();

        if (_replay)
        {
            return true;
        }
    }
    
    // try the session replay
    {
        SESSION_ACCESS_SECTION();
        
        if (_session)
        {
            return _session->confidenceReplayActive();
        }
    }
    
    return false;
}

void Recorder::forwardConfidenceReplayControl(string command)
{
    // try local replay
    {
        REPLAY_ACCESS_SECTION();
        
        if (_replay)
        {
            _replay->forwardControl(command);
            return;
        }
    }
    
    // try the session replay
    {
        SESSION_ACCESS_SECTION();
        
        if (_session)
        {
            _session->forwardConfidenceReplayControl(command);
        }
    }
}

ConfidenceReplayStatus Recorder::getConfidenceReplayStatus()
{
    // try local replay
    {
        REPLAY_ACCESS_SECTION();
        
        if (_replay)
        {
            return _replay->getStatus();
        }
    }
    
    // try the session replay
    {
        SESSION_ACCESS_SECTION();
        
        if (_session)
        {
            return _session->getConfidenceReplayStatus();
        }
    }
    
    return ConfidenceReplayStatus();
}

int Recorder::replayFile(string name)
{
    SESSION_ACCESS_SECTION();
    REPLAY_ACCESS_SECTION(); // place after session access section 
    
    if (_session)
    {
        return SESSION_IN_PROGRESS_REPLAY_FAILURE;
    }
    if (!_cache->itemExists(name))
    {
        return UNKNOWN_FILE_REPLAY_FAILURE;
    }
    
    if (_replay)
    {
        // play file or seek to start if the file is already playing
        if (_replay->getStatus().filename == _cache->getCompleteFilename(name))
        {
            _replay->seekPositionOrPlay(0);
            return 0;
        }
        
        // close existing replay
        try
        {
            SAFE_DELETE(_replay);
            Logging::info("Closed replay of cache file\n");
        }
        catch (...)
        {
            Logging::error("Failed to close replay in recorder\n");
            _replay = 0;
            return INTERNAL_REPLAY_FAILURE;
        }
    }
    
    try
    {
        string mxfFilename = _cache->getCompleteFilename(name);
        string eventFilename = _cache->getEventFilename(mxfFilename);
        
        _replay = new ConfidenceReplay(mxfFilename, eventFilename, _replayPort, 0, false);
        _replay->start();
        Logging::info("Started replay of file '%s' in cache\n", name.c_str());
    }
    catch (...)
    {
        try { delete _replay; } catch (...) {}
        Logging::error("Failed to open replay in recorder\n");
        _replay = 0;
        return INTERNAL_REPLAY_FAILURE;
    }
    
    return 0;
}

void Recorder::playItem(int id, int index)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        _session->playItem(id, index);
    }
}

void Recorder::playPrevItem()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        _session->playPrevItem();
    }
}

void Recorder::playNextItem()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        _session->playNextItem();
    }
}

void Recorder::seekToEOP()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        _session->seekToEOP();
    }
}

int Recorder::markItemStart(int* id, bool* isJunk, int64_t* filePosition, int64_t* fileDuration)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->markItemStart(id, isJunk, filePosition, fileDuration);
    }
    
    return -1;
}

int Recorder::clearItem(int id, int index, int64_t* filePosition)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->clearItem(id, index, filePosition);
    }
    
    return -1;
}

int Recorder::markItemStart()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->markItemStart();
    }
    
    return -1;
}

int Recorder::clearItem()
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->clearItem();
    }
    
    return -1;
}

int Recorder::moveItemUp(int id, int index)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->moveItemUp(id, index);
    }
    
    return -1;
}

int Recorder::moveItemDown(int id, int index)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->moveItemDown(id, index);
    }
    
    return -1;
}

int Recorder::disableItem(int id, int index)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->disableItem(id, index);
    }
    
    return -1;
}

int Recorder::enableItem(int id, int index)
{
    SESSION_ACCESS_SECTION();
    
    if (_session)
    {
        return _session->enableItem(id, index);
    }
    
    return -1;
}

RecorderStatus Recorder::getStatus()
{
    ::GeneralStats captureStats;
    _capture->get_general_stats(&captureStats);

    VTRControl* sourceVTRControl = getSourceVTRControl();
    VTRState sourceVTRState = (sourceVTRControl != 0) ? sourceVTRControl->getState() : NOT_CONNECTED_VTR_STATE;
    VTRControl* digibetaVTRControl = getBackupVTRControl();
    VTRState digibetaVTRState = (digibetaVTRControl != 0) ? digibetaVTRControl->getState() : NOT_CONNECTED_VTR_STATE;
    
    
    RecorderStatus status;
    
    bool vtrOK = sourceVTRState != NOT_CONNECTED_VTR_STATE && sourceVTRState != REMOTE_LOCKOUT_VTR_STATE;
    bool tapeOK = sourceVTRState != TAPE_UNTHREADED_VTR_STATE && sourceVTRState != EJECTING_VTR_STATE;
    if (_videotapeBackup)
    {
        vtrOK = vtrOK && digibetaVTRState != NOT_CONNECTED_VTR_STATE && digibetaVTRState != REMOTE_LOCKOUT_VTR_STATE;
        tapeOK = tapeOK && digibetaVTRState != TAPE_UNTHREADED_VTR_STATE && digibetaVTRState != EJECTING_VTR_STATE;
    }
    
    status.recorderName = _name;
    status.databaseOk = RecorderDatabase::getInstance()->haveConnection();
    status.sdiCardOk = captureStats.video_ok && captureStats.audio_ok;
    status.videoOk = captureStats.video_ok;
    status.audioOk = captureStats.audio_ok;
    status.vtrOk = vtrOK;

    bool sessionActive;
    {
        SESSION_ACCESS_SECTION();
        sessionActive = _session != 0;
    }

    status.sourceVTRState = sourceVTRState;
    status.digibetaVTRState = digibetaVTRState;
    
    status.readyToRecord = !sessionActive && 
        status.sdiCardOk && status.videoOk && status.audioOk && 
        status.databaseOk && 
        status.vtrOk && 
        tapeOK;
    
    {
        REPLAY_ACCESS_SECTION();
    
        status.replayActive = _replay != 0;
        status.replayFilename = (_replay != 0) ? _replay->getStatus().filename : "" ;
    }
    
    return status;
}

RecorderSystemStatus Recorder::getSystemStatus()
{
    VTRControl* sourceVTRControl = getSourceVTRControl();
    VTRControl* digibetaVTRControl = getBackupVTRControl();
    
    RecorderSystemStatus status;
    
    status.remDiskSpace = getRemainingDiskSpace();
    {
        LOCK_SECTION(_remDurationMutex);
        status.remDuration = status.remDiskSpace / _remDurationFactor;
        status.remDurationIngestFormat = _remDurationIngestFormat;
    }
    status.sourceVTRState = (sourceVTRControl != 0) ? sourceVTRControl->getState() : NOT_CONNECTED_VTR_STATE;
    status.digibetaVTRState = (digibetaVTRControl != 0) ? digibetaVTRControl->getState() : NOT_CONNECTED_VTR_STATE;
    
    return status;
}

void Recorder::getLastSessionResult(SessionResult* result, string* sourceSpoolNo, string* failureReason)
{
    SESSION_ACCESS_SECTION();

    *result = _lastSessionResult;    
    *sourceSpoolNo = _lastSessionSourceSpoolNo;    
    *failureReason = _lastSessionFailureReason;    
}

int64_t Recorder::getRemainingDiskSpace()
{
    int64_t remDiskSpace = _cache->getDiskSpace() - DISK_SPACE_MARGIN;
    remDiskSpace = (remDiskSpace < 0) ? 0 : remDiskSpace;
    
    return remDiskSpace;
}

RecordingItems* Recorder::getSessionRecordingItems()
{
    SESSION_ACCESS_SECTION();

    if (_session)
    {
        return _session->getRecordingItems();
    }

    return 0;
}

bool Recorder::getSessionComments(string* comments, int* count)
{
    SESSION_ACCESS_SECTION();

    if (_session)
    {
        return _session->getSessionComments(comments, count);
    }

    return false;
}

Cache* Recorder::getCache()
{
    return _cache;
}

::Capture* Recorder::getCapture()
{
    return _capture;
}

VTRControl* Recorder::getSourceVTRControl()
{
    if (_videotapeBackup)
    {
        int deviceCode1 = _vtrControl1->getDeviceCode();
        int deviceCode2 = _vtrControl2->getDeviceCode();
        
        if (isSourceVTR(deviceCode1))
        {
            return _vtrControl1;
        }
        else if (isSourceVTR(deviceCode2))
        {
            return _vtrControl2;
        }
    }
    else
    {
        return _vtrControl1;
    }
    
    // source VTR control is not connected
    return 0;
}

VTRControl* Recorder::getBackupVTRControl()
{
    if (_videotapeBackup)
    {
        int deviceCode1 = _vtrControl1->getDeviceCode();
        int deviceCode2 = _vtrControl2->getDeviceCode();
        
        if (isBackupVTR(deviceCode1))
        {
            return _vtrControl1;
        }
        else if (isBackupVTR(deviceCode2))
        {
            return _vtrControl2;
        }
    }
    
    // backup VTR control is not connected
    return 0;
}

vector<VTRControl*> Recorder::getVTRControls()
{
    vector<VTRControl*> vcs;
    vcs.push_back(_vtrControl1);
    if (_videotapeBackup)
    {
        vcs.push_back(_vtrControl2);
    }
    
    return vcs;
}

RecorderTable* Recorder::getRecorderTable()
{
    return _recorderTable;
}

string Recorder::getName()
{
    return _name;
}

bool Recorder::tapeBackupEnabled()
{
    return _videotapeBackup;
}

bool Recorder::isDigibetaBarcode(string barcode)
{
    REC_ASSERT(_videotapeBackup);
    
    if (!is_valid_barcode(barcode))
    {
        return false;
    }
    
    vector<string>::const_iterator iter;
    for (iter = _digibetaBarcodePrefixes.begin(); iter != _digibetaBarcodePrefixes.end(); iter++)
    {
        if (barcode.compare(0, (*iter).size(), *iter) == 0)
        {
            return true;
        }
    }
    
    return false;
}

bool Recorder::isDigibetaSource(Source* source)
{
    if (!source || source->concreteSources.empty())
    {
        return false;
    }
    
    SourceItem* sourceItem = dynamic_cast<SourceItem*>(source->concreteSources[0]);
    REC_ASSERT(sourceItem);
    
    return sourceItem->format == "BD";
}

bool Recorder::checkMultiItemSupport(Source* source)
{
    return Config::enable_multi_item || source->concreteSources.size() <= 1;
}

bool Recorder::updateSourceAspectRatios(Source* source, vector<string> aspectRatioCodes)
{
    if (aspectRatioCodes.empty())
    {
        // empty means no updates
        return true;
    }
    
    if (aspectRatioCodes.size() != source->concreteSources.size())
    {
        // each source item must have a corresponding aspect ratio code
        return false;
    }
    
    size_t i;
    for (i = 0; i < aspectRatioCodes.size(); i++)
    {
        if (!isValidAspectRatioCode(aspectRatioCodes[i]))
        {
            return false;
        }
    }
    
    for (i = 0; i < source->concreteSources.size(); i++)
    {
        SourceItem* sourceItem = dynamic_cast<SourceItem*>(source->concreteSources[i]);
        REC_ASSERT(sourceItem);
        
        if (sourceItem->aspectRatioCode != aspectRatioCodes[i])
        {
            sourceItem->aspectRatioCode = aspectRatioCodes[i];
            sourceItem->modifiedFlag = true;
        }
    }
    
    return true;
}

bool Recorder::checkSourceAspectRatios(Source* source)
{
    size_t i;
    for (i = 0; i < source->concreteSources.size(); i++)
    {
        SourceItem* sourceItem = dynamic_cast<SourceItem*>(source->concreteSources[i]);
        REC_ASSERT(sourceItem);
        
        if (sourceItem->aspectRatioCode.empty() || !isValidAspectRatioCode(sourceItem->aspectRatioCode))
        {
            return false;
        }
    }
    
    return true;
}

bool Recorder::haveProfile(int id)
{
    LOCK_SECTION(_profileManagerMutex);

    return _profileManager->getProfile(id) != NULL;
}

bool Recorder::getLastSessionProfileCopy(Profile* profileCopy)
{
    LOCK_SECTION(_profileManagerMutex);
    
    Profile* profile = _profileManager->getProfile(_lastSessionProfileId);
    if (!profile)
    {
        return false;
    }
    
    *profileCopy = *profile;
    return true;
}

map<int, Profile> Recorder::getProfileCopies()
{
    LOCK_SECTION(_profileManagerMutex);
    
    const map<int, Profile*>& profiles = _profileManager->getProfiles();
    
    map<int, Profile> profileCopies;
    map<int, Profile*>::const_iterator iter;
    for (iter = profiles.begin(); iter != profiles.end(); iter++)
    {
        profileCopies[iter->first] = *(iter->second);
    }
    
    return profileCopies;
}

bool Recorder::getProfileCopy(int id, Profile* profileCopy)
{
    LOCK_SECTION(_profileManagerMutex);
    
    Profile* profile = _profileManager->getProfile(id);
    if (!profile)
    {
        return false;
    }
    
    *profileCopy = profile;
    return true;
}

bool Recorder::updateProfile(const Profile *updatedProfileCopy)
{
    LOCK_SECTION(_profileManagerMutex);
    
    Profile *profile = _profileManager->getProfile(updatedProfileCopy->getId());
    if (!profile)
    {
        Logging::error("Failed to update unknown profile '%s'\n", updatedProfileCopy->getName().c_str());
        return false;
    }
    
    if (!profile->update(updatedProfileCopy))
    {
        Logging::error("Failed to update profile '%s'\n", updatedProfileCopy->getName().c_str());
        return false;
    }
    
    Logging::info("Updated profile '%s'\n", updatedProfileCopy->getName().c_str());
    
    _profileManager->storeProfiles();
    
    return true;
}

void Recorder::sessionDone()
{
    _sessionDone = true;
}

void Recorder::checkSessionStatus()
{
    if (_sessionDone)
    {
        WRITE_LOCK_SECTION(_sessionRWLock);
        
        if (_session)
        {
            delete _lastSessionRecordingItems;
            _lastSessionRecordingItems = _session->getRecordingItems();
            
            _session->getFinalResult(&_lastSessionResult, &_lastSessionSourceSpoolNo, &_lastSessionFailureReason);
            
            SAFE_DELETE(_sessionThread);
            // don't delete _session which is owned by _sessionThread
            _session = 0;
        }
        
        _sessionDone = false;
    }
}

bool Recorder::isSourceVTR(int deviceTypeCode)
{
    size_t i;
    for (i = 0; i < _sourceVTRDeviceTypeCodes.size(); i++)
    {
        if (deviceTypeCode == _sourceVTRDeviceTypeCodes[i])
        {
            return true;
        }
    }
    
    if (_sourceVTRDeviceTypeCodes.empty())
    {
        for (i = 0; i < _backupVTRDeviceTypeCodes.size(); i++)
        {
            if (deviceTypeCode == _backupVTRDeviceTypeCodes[i])
            {
                return false;
            }
        }
        
        return true;
    }
    
    return false;
}

bool Recorder::isBackupVTR(int deviceTypeCode)
{
    size_t i;
    for (i = 0; i < _backupVTRDeviceTypeCodes.size(); i++)
    {
        if (deviceTypeCode == _backupVTRDeviceTypeCodes[i])
        {
            return true;
        }
    }
    
    if (_backupVTRDeviceTypeCodes.empty())
    {
        for (i = 0; i < _sourceVTRDeviceTypeCodes.size(); i++)
        {
            if (deviceTypeCode == _sourceVTRDeviceTypeCodes[i])
            {
                return false;
            }
        }
        
        return true;
    }
    
    return false;
}

