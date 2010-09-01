/*
 * $Id: RecordingSession.cpp,v 1.2 2010/09/01 16:05:22 philipn Exp $
 *
 * Manages a recording session
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
    A recording session starts when the user has selected a source and 
    Digibeta destination and selects to start a session. A recording session 
    ends when the user selects to complete it at the end or abort it at any 
    stage. The system can also abort the session, eg. when the disk space runs 
    out.
    
    The first part of the recording session is the capture from the source tape.
    The capture is started by the user and is stopped either by the user or when
    the source VTR stops.
    
    The second part is the review. A review of a capture of a single-item source
    tape will consist of the user playing back the file and either selecting
    to complete or abort. A review of a multi-item source will start with the 
    user selecting the order, start and duration of each item using the 
    captured file, chunking that file, playing back and finally completing or
    aborting.
*/


// to get the PRI64d etc. macros
#define __STDC_FORMAT_MACROS 1

#include <cerrno>

#include "RecordingSession.h"
#include "Recorder.h"
#include "RecorderDatabase.h"
#include "D10MXFWriter.h"
#include "ArchiveMXFWriter.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Config.h"
#include "Timing.h"


using namespace std;
using namespace rec;

// wait 10 seconds for the SDI signal to stablise when setting VTR in paused state
#define SDI_SIGNAL_PAUSE_WAIT_SEC       10

// wait 2 seconds for the SDI signal to stablise when setting VTR in play state (from paused state)
#define SDI_SIGNAL_PLAY_WAIT_SEC        2

    
static ArchiveTimecode convert_timecode(rec::Timecode rtc)
{
    ArchiveTimecode result;
    
    result.hour = rtc.hour;
    result.min = rtc.min;
    result.sec = rtc.sec;
    result.frame = rtc.frame;
    
    return result;
}

static string get_state_string(SessionState state)
{
    switch (state)
    {
        case NOT_STARTED_SESSION_STATE:
            return "NOT STARTED";
        case READY_SESSION_STATE:
            return "READY";
        case RECORDING_SESSION_STATE:
            return "RECORDING";
        case REVIEWING_SESSION_STATE:
            return "REVIEWING";
        case PREPARE_CHUNKING_SESSION_STATE:
            return "PREPARE CHUNKING";
        case CHUNKING_SESSION_STATE:
            return "CHUNKING";
        case END_SESSION_STATE:
            return "END";
    }
    
    return ""; // keep gcc happy
}

// utility class to set and reset busy state

#define GUARD_BUSY_STATE(bs)      BusyGuard __guard(bs)

class BusyGuard
{
public:
    BusyGuard(bool& busy)
    : _busy(busy)
    {
        _busy = true;
    }
    ~BusyGuard()
    {
        _busy = false;
    }
    
private:
    bool& _busy;
};



RecordingSession::RecordingSession(Recorder* recorder, const Profile *profile, Source* infaxSource,
    string digibetaBarcode, int replayPort, VTRControl* sourceVTRControl, VTRControl* digibetaVTRControl)
: _recorder(recorder), _profile(0), _recordingItems(0), _videotapeBackup(false),
_chunking(0), _digibetaSpoolNo(digibetaBarcode), _sourceVTRControl(sourceVTRControl),
_digibetaVTRControl(digibetaVTRControl),_sessionTable(0), _hddDest(0), _infaxDuration(-1), _sessionState(NOT_STARTED_SESSION_STATE),
_startBusy(false), _stopBusy(false), _chunkBusy(false), _completeBusy(false), _abortBusy(false), _diskSpace(0), 
_vtrErrorCount(0), _digiBetaDropoutEnabled(false), _digiBetaDropoutCount(0), _pseResult(0),
_browseBufferOverflow(false), _duration(-1),
_sessionResult(UNKNOWN_SESSION_RESULT), _stopThread(false), _threadHasStopped(false),
_startCalled(false), _stopCalled(false), _chunkFileCalled(false), _completeCalled(false), _abortCalled(false), 
_sessionCommentsCount(0), _replayPort(replayPort), _replay(0)
{
    REC_CHECK(infaxSource->barcode.size() > 0);
    
    try
    {
        _profile = new Profile(profile);
        _profile->setCachedIngestFormat(infaxSource->getFirstSourceItem()->format);
        REC_ASSERT(_profile->getIngestFormat() != UNKNOWN_INGEST_FORMAT);
        Logging::info("Ingest session profile is '%s'\n", _profile->getName().c_str());
        Logging::info("Ingest format is '%s'\n", ingest_format_to_string(_profile->getIngestFormat(), false).c_str());
        
        _videotapeBackup = Config::videotape_backup;
        if (_videotapeBackup)
        {
            REC_ASSERT(!digibetaBarcode.empty());
            REC_ASSERT(digibetaVTRControl);
        }
        
        _lastVTRErrorLTC.hour = 99;
        _lastVTRErrorVITC.hour = 99;
        
        // initialize capture
        _digiBetaDropoutEnabled = _recorder->isDigibetaSource(infaxSource);
        _recorder->getCapture()->set_digibeta_dropout_enable(_digiBetaDropoutEnabled);
        _recorder->getCapture()->set_browse_enable(_profile->browse_enable);
        _recorder->getCapture()->set_browse_video_bitrate(_profile->browse_video_bit_rate);
        _recorder->getCapture()->set_pse_enable(_profile->pse_enable);
        _recorder->getCapture()->set_num_audio_tracks(_profile->num_audio_tracks);
        _recorder->getCapture()->set_include_crc32(_profile->include_crc32);
        _recorder->getCapture()->set_ingest_format(_profile->getIngestFormat());
        _recorder->getCapture()->set_primary_timecode(_profile->primary_timecode);
        
        
        auto_ptr<Source> source(createRecordingSource(infaxSource));
        RecorderDatabase::getInstance()->saveSource(source.get());
        _recordingItems = new RecordingItems(source.get());
        source.release(); // _recordingItems takes ownership
        
        _infaxDuration = _recordingItems->getTotalInfaxDuration();
        
        if (_recordingItems->haveMultipleItems())
        {
            if (!recorder->getCache()->getMultiItemTemplateFilename(_recordingItems->getSource()->barcode,
                _recordingItems->getItemCount(), &_mxfFilename, &_eventFilename))
            {                             
                REC_LOGTHROW(("Failed to get a unique filenames from the cache for multi-item source"));
            }
        }
        else
        {
            if (!recorder->getCache()->getUniqueFilenames(_recordingItems->getSource()->barcode, _recordingItems->getSource()->databaseId, 
                &_mxfFilename, &_browseFilename, &_browseTimecodeFilename, &_browseInfoFilename, &_pseFilename, &_eventFilename))
            {                             
                REC_LOGTHROW(("Failed to get a unique filenames from the cache"));
            }
        }


        // create session        
        _sessionTable = new RecordingSessionTable();
        _sessionTable->recorder = recorder->getRecorderTable();
        _sessionTable->status = RECORDING_SESSION_STARTED;
        _sessionTable->abortInitiator = -1;
        _sessionTable->source = new RecordingSessionSourceTable();
        _sessionTable->source->source = _recordingItems->getSource();
    
        // save session to database
        RecorderDatabase::getInstance()->saveSession(_sessionTable);
    
        // add hard disk and digibeta destinations
        if (_recordingItems->haveMultipleItems())
        {
            // temporary hard disk destination
            _hddDest = addHardDiskDestination(_mxfFilename, _browseFilename, _pseFilename, 
                _recordingItems->getFirstItem().sourceItem, -1);
        }
        else
        {
            _hddDest = addHardDiskDestination(_mxfFilename, _browseFilename, _pseFilename, 
                _recordingItems->getFirstItem().sourceItem, 1);
        }
        if (_videotapeBackup)
        {
            addDigibetaDestination(digibetaBarcode);
        }


        _diskSpace = _recorder->getRemainingDiskSpace();
        
        _sourceVTRControl->registerListener(this);
        if (_videotapeBackup)
        {
            _digibetaVTRControl->registerListener(this);
        }
    
        _statusMessage = "Ready";
        setSessionState(READY_SESSION_STATE);
        
        
        Logging::info("A new recording session was created for source '%s'",
            _recordingItems->getSource()->barcode.c_str());
        if (_videotapeBackup)
        {
            Logging::infoMore(false, " and digibeta '%s'.", digibetaBarcode.c_str());
        }
        else
        {
            Logging::infoMore(false, ".");
        }
        Logging::infoMore(true, "Filenames are '%s', (browse) '%s', (pse) '%s'\n",  
            _mxfFilename.c_str(), _browseFilename.c_str(), _pseFilename.c_str());
    }
    catch (...)
    {
        delete _profile;
        delete _chunking;
        
        _sourceVTRControl->unregisterListener(this);
        if (_videotapeBackup)
        {
            _digibetaVTRControl->unregisterListener(this);
        }
        
        vector<Destination*>::const_iterator iter;
        for (iter = _destinations.begin(); iter != _destinations.end(); iter++)
        {
            delete *iter;
        }
        delete _sessionTable;
        delete _replay;
        
        throw;
    }
}

RecordingSession::~RecordingSession()
{
    delete _profile;
    delete _chunking;
    
    _sourceVTRControl->unregisterListener(this);
    if (_videotapeBackup)
    {
        _digibetaVTRControl->unregisterListener(this);
    }
    
    vector<Destination*>::const_iterator iter;
    for (iter = _destinations.begin(); iter != _destinations.end(); iter++)
    {
        delete *iter;
    }
    delete _sessionTable;
    delete _replay;
}

void RecordingSession::start()
{
    GUARD_THREAD_START(_threadHasStopped);
    
    Timer controlTimer;
    Timer diskSpaceCheckTimer;
    
    bool startRecording = false;
    bool stopRecording = false;
    bool chunkFile = false;
    bool completeSession = false;
    bool abortSession = false;
    bool abortSessionFromUser = false;
    string abortComments;
    string completeComments;
    bool checkDiskSpace = false;
    
    bool exceptionAbort = false;
    try
    {
        controlTimer.start(10 * MSEC_IN_USEC);
        
        while (!_stopThread)
        {
            // get the control input
            {
                LOCK_SECTION(_controlMutex);
                
                startRecording = _startCalled;
                stopRecording = _stopCalled;
                chunkFile = _chunkFileCalled;
                completeSession = _completeCalled;
                abortSession = _abortCalled;
                abortSessionFromUser = _abortFromUser;
                abortComments = _abortComments;
                completeComments = _completeComments;
            }
            
            // process controls (in order)
            if (abortSession)
            {
                if (abortInternal(abortSessionFromUser, abortComments))
                {
                    // break out of while loop
                    break;
                }
                else
                {
                    Logging::error("Request to abort session failed\n");
                }

                // reset request; no lock needed
                _abortCalled = false;
            }
            else if (completeSession)
            {
                if (completeInternal(completeComments))
                {
                    // break out of while loop
                    break;
                }
                else
                {
                    Logging::error("Request to complete session failed\n");
                }

                // reset request; no lock needed
                _completeCalled = false;
            }
            else if (chunkFile)
            {
                if (!chunkFileInternal())
                {
                    Logging::error("Request to chunk file failed\n");
                }

                // reset request; no lock needed
                _chunkFileCalled = false;
            }
            else if (stopRecording)
            {
                if (stopRecordingInternal())
                {
                    // stop checking disk space no that recording has stopped
                    checkDiskSpace = false;
                }
                else
                {
                    Logging::warning("Request to stop recording failed\n");
                }
                
                // reset request; no lock needed
                _stopCalled = false;
            }
            else if (startRecording)
            {
                if (startRecordingInternal())
                {
                    // check disk space whilst recording
                    checkDiskSpace = true;
                }
                else
                {
                    Logging::warning("Request to start recording failed\n");
                }
                
                // reset request; no lock needed
                _startCalled = false;
            }

            
            // check disk space
            if (checkDiskSpace)
            {
                if (diskSpaceCheckTimer.timeLeft() == 0)
                {
                    _diskSpace = _recorder->getRemainingDiskSpace();
                    
                    if (checkDiskSpace && _diskSpace < DISK_SPACE_MARGIN)
                    {
                        // stop the recording because disk space margin has been breached
                        if (stopRecordingInternal())
                        {
                            checkDiskSpace = false;
                        }
                        else
                        {
                            Logging::warning("Failed to stop recording after disk space margin was breached\n");
                        }
                    }
                    
                    // check disk space every second
                    diskSpaceCheckTimer.start(1 * SEC_IN_USEC);
                }
            }
    
            // don't hog the CPU
            sleep_msec(1);
            
            // check for control input every 10msec
            controlTimer.sleepRemainder();
            controlTimer.start(10 * MSEC_IN_USEC);
        }
    }
    catch (const RecorderException& ex)
    {
        Logging::error("Exception thrown in recorder session control thread: %s\n", ex.getMessage().c_str());
        Logging::errorMore(true, "Aborting session\n");
        exceptionAbort = true;
    }
    catch (...)
    {
        Logging::error("Exception thrown in recorder session control thread - aborting session\n");
        exceptionAbort = true;
    }
    
    // abort if an exception was thrown
    if (exceptionAbort && _recorder->_session != 0)
    {
        try
        {
            abortInternal(false, "System abort");
        }
        catch (const RecorderException& ex)
        {
            Logging::error("Failed to abort session after exception was thrown: %s\n", ex.getMessage().c_str());
        }
        catch (...)
        {
            Logging::error("Failed to abort session after exception was thrown\n");
        }
    }

    
    // signal to the recorder that the session is finished
    setSessionState(END_SESSION_STATE);
    _recorder->sessionDone();
}
    
void RecordingSession::stop()
{
    _stopThread = true;
}
    
bool RecordingSession::hasStopped() const
{
    return _threadHasStopped;
}

void RecordingSession::vtrState(VTRControl* vtrControl, VTRState vtrState, const unsigned char* stateBytes)
{
    SessionState sessionState = getSessionState();
    
    if (sessionState == RECORDING_SESSION_STATE && vtrState != PLAY_VTR_STATE)
    {
        if (vtrState == STOPPED_VTR_STATE || vtrState == PAUSED_VTR_STATE || vtrState == FAST_REWIND_VTR_STATE)
        {
            Logging::warning("Stopping recording because %s VTR %s\n",
                (vtrControl == _sourceVTRControl) ? "Source" : "Backup",
                (vtrState == STOPPED_VTR_STATE) ? "stopped" : 
                    ((vtrState == PAUSED_VTR_STATE) ? "pause" : "fast rewinding"));
            stopRecording();
        }
        else
        {
            // Warning: the recording is not stopped here because it was found that the D3 VTR returned
            // spurious state changes
            Logging::warning("%s VTR state changed whilst recording: state = %s\n",
                (vtrControl == _sourceVTRControl) ? "Source" : "Backup",
                VTRControl::getStateString(vtrState).c_str());
            if (vtrState == OTHER_VTR_STATE)
            {
                Logging::debug("Detailed VTR state: %s\n", VTRControl::stateBytesToString(stateBytes).c_str());
            }
        }
    }
}

void RecordingSession::vtrPlaybackError(VTRControl* vtrControl, int errorCode, Timecode ltc, Timecode vitc)
{
    SessionState sessionState = getSessionState();
    
    if (vtrControl == _sourceVTRControl && sessionState == RECORDING_SESSION_STATE &&
        ltc != _lastVTRErrorLTC && vitc != _lastVTRErrorVITC)
    {
        LOCK_SECTION(_vtrErrorsMutex);
        
        VTRError error;
        error.errorCode = (unsigned char)(errorCode & 0xff);
        error.ltcTimecode = convert_timecode(ltc);
        error.vitcTimecode = convert_timecode(vitc);
        
        _vtrErrors.push_back(error);
        _vtrErrorCount++;
        
        _lastVTRErrorLTC = ltc;
        _lastVTRErrorVITC = vitc;
    }
}



void RecordingSession::startRecording()
{
    LOCK_SECTION(_controlMutex);
    
    _startCalled = true;
}

void RecordingSession::stopRecording()
{
    LOCK_SECTION(_controlMutex);
    
    _stopCalled = true;
}

void RecordingSession::chunkFile()
{
    LOCK_SECTION(_controlMutex);
    
    _chunkFileCalled = true;
}

void RecordingSession::abort(bool fromUser, string comments)
{
    LOCK_SECTION(_controlMutex);
    
    if (!_abortCalled)
    {
        _abortCalled = true;
        _abortFromUser = fromUser;
        _abortComments = comments;
    }
}

bool RecordingSession::abortIfRecording(bool fromUser, string comments)
{
    LOCK_SECTION(_controlMutex);
    
    SessionState sessionState = getSessionState();
    
    if (sessionState == RECORDING_SESSION_STATE && !_abortCalled)
    {
        _abortCalled = true;
        _abortFromUser = fromUser;
        _abortComments = comments;
        return true;
    }
    
    return false;
}

void RecordingSession::complete(string comments)
{
    LOCK_SECTION(_controlMutex);
    
    if (!_completeCalled)
    {
        _completeCalled = true;
        _completeComments = comments;
    }
}

void RecordingSession::setSessionComments(string comments)
{
    LOCK_SECTION(_controlMutex);
    
    if (comments != _sessionComments)
    {
        _sessionComments = comments;
        _sessionCommentsCount++;
    }
}    

void RecordingSession::playItem(int id, int index)
{
    TRY_LOCK_SECTION(_sessionStateChangeMutex);
    if (!HAVE_SECTION_LOCK())
    {
        return;
    }
    
    SessionState sessionState = getSessionState();
    if (sessionState != PREPARE_CHUNKING_SESSION_STATE &&
        sessionState != REVIEWING_SESSION_STATE)
    {
        return;
    }
    
    RecordingItem item;
    if (!_recordingItems->getItem(id, index, &item))
    {
        return;
    }
    
    if (item.isDisabled || item.duration < 0 || item.filename.empty())
    {
        return;
    }
    
    {
        LOCK_SECTION(_replayMutex);
        if (_replay == 0)
        {
            return;
        }

        string filename = _replay->getStatus().filename;

        // start playing if the position is at the start of the item
        if (filename == item.filename)
        {
            _replay->seekPositionOrPlay(item.startPosition);
            return;
        }
    }
    
    // play new file or set file position to start of file
    replayFile(item.filename, item.startPosition, sessionState == PREPARE_CHUNKING_SESSION_STATE);
}

void RecordingSession::playPrevItem()
{
    TRY_LOCK_SECTION(_sessionStateChangeMutex);
    if (!HAVE_SECTION_LOCK())
    {
        return;
    }
    
    SessionState sessionState = getSessionState();
    if (sessionState != PREPARE_CHUNKING_SESSION_STATE &&
        sessionState != REVIEWING_SESSION_STATE)
    {
        return;
    }
    
    
    ConfidenceReplayStatus replayStatus;
    {
        LOCK_SECTION(_replayMutex);

        if (_replay == 0)
        {
            return;
        }

        replayStatus = _replay->getStatus();
    }

    RecordingItem prevItem;
    if (!_recordingItems->getPrevStartOfItem(replayStatus.filename, replayStatus.position, &prevItem))
    {
        return;
    }
    if (prevItem.filename.empty())
    {
        // junk items have empty filenames after chunking
        return;
    }

    replayFile(prevItem.filename, prevItem.startPosition, sessionState == PREPARE_CHUNKING_SESSION_STATE);
}

void RecordingSession::playNextItem()
{
    TRY_LOCK_SECTION(_sessionStateChangeMutex);
    if (!HAVE_SECTION_LOCK())
    {
        return;
    }
    
    SessionState sessionState = getSessionState();
    if (sessionState != PREPARE_CHUNKING_SESSION_STATE &&
        sessionState != REVIEWING_SESSION_STATE)
    {
        return;
    }

    ConfidenceReplayStatus replayStatus;
    {
        LOCK_SECTION(_replayMutex);

        if (_replay == 0)
        {
            return;
        }

        replayStatus = _replay->getStatus();
    }

    RecordingItem nextItem;
    if (!_recordingItems->getNextStartOfItem(replayStatus.filename, replayStatus.position, &nextItem))
    {
        return;
    }
    if (nextItem.filename.empty())
    {
        // junk items have empty filenames after chunking
        return;
    }

    replayFile(nextItem.filename, nextItem.startPosition, sessionState == PREPARE_CHUNKING_SESSION_STATE);
}

void RecordingSession::seekToEOP()
{
    TRY_LOCK_SECTION(_sessionStateChangeMutex);
    if (!HAVE_SECTION_LOCK())
    {
        return;
    }
    
    SessionState sessionState = getSessionState();
    if (sessionState != PREPARE_CHUNKING_SESSION_STATE &&
        sessionState != REVIEWING_SESSION_STATE)
    {
        return;
    }

    ConfidenceReplayStatus replayStatus;
    {
        LOCK_SECTION(_replayMutex);

        if (_replay == 0)
        {
            return;
        }
        
        replayStatus = _replay->getStatus();
    }

    RecordingItem item;
    if (!_recordingItems->getItem(replayStatus.filename, replayStatus.position, &item))
    {
        return;
    }

    if (item.duration > 1 && item.sourceItem->duration > 1)
    {
        LOCK_SECTION(_replayMutex);

        if (_replay == 0)
        {
            return;
        }

        int64_t offset = item.startPosition;
        if (item.duration < item.sourceItem->duration)
        {
            // seek to the end of the item
            offset += item.duration - 1;
        }
        else
        {
            // seek to the end of programme using the infax duration
            offset += item.sourceItem->duration - 1;
        }

        _replay->seek(offset);
    }
}

int RecordingSession::markItemStart(int* id, bool* isJunk, int64_t* filePosition, int64_t* fileDuration)
{
    TRY_LOCK_SECTION(_sessionStateChangeMutex);
    if (!HAVE_SECTION_LOCK())
    {
        return -1;
    }
    
    SessionState sessionState = getSessionState();
    if (sessionState != PREPARE_CHUNKING_SESSION_STATE)
    {
        return -1;
    }
    
    ConfidenceReplayStatus replayStatus;
    {
        LOCK_SECTION(_replayMutex);

        if (_replay == 0)
        {
            return -1;
        }
        
        replayStatus = _replay->getStatus();
    }
    
    int itemId;
    bool itemIsJunk;
    int result = _recordingItems->markItemStart(replayStatus.filename, replayStatus.position, &itemId, &itemIsJunk);
    if (result >= 0)
    {
        LOCK_SECTION(_replayMutex);

        if (_replay != 0)
        {
            _replay->setMark(replayStatus.position, ITEM_START_MARK);
        }
        
        *id = itemId;
        *isJunk = itemIsJunk;
        *filePosition = replayStatus.position;
        *fileDuration = replayStatus.duration;
    }
    
    return result;
}

int RecordingSession::clearItem(int id, int index, int64_t* filePosition)
{
    TRY_LOCK_SECTION(_sessionStateChangeMutex);
    if (!HAVE_SECTION_LOCK())
    {
        return -1;
    }
    
    SessionState sessionState = getSessionState();
    if (sessionState != PREPARE_CHUNKING_SESSION_STATE)
    {
        return -1;
    }
    
    string clearedFilename;
    int64_t clearedPosition;
    int result = _recordingItems->clearItem(id, index, &clearedFilename, &clearedPosition);
    if (result >= 0)
    {
        LOCK_SECTION(_replayMutex);

        if (_replay != 0)
        {
            if (clearedFilename == _replay->getStatus().filename)
            {
                _replay->clearMark(clearedPosition, ITEM_START_MARK);
            }
        }
        
        *filePosition = clearedPosition;
    }
    
    return result;
}

int RecordingSession::markItemStart()
{
    int id;
    bool isJunk;
    int64_t filePosition;
    int64_t fileDuration;
    
    return markItemStart(&id, &isJunk, &filePosition, &fileDuration);
}

int RecordingSession::clearItem()
{
    TRY_LOCK_SECTION(_sessionStateChangeMutex);
    if (!HAVE_SECTION_LOCK())
    {
        return -1;
    }
    
    SessionState sessionState = getSessionState();
    if (sessionState != PREPARE_CHUNKING_SESSION_STATE)
    {
        return -1;
    }
    
    ConfidenceReplayStatus replayStatus;
    {
        LOCK_SECTION(_replayMutex);

        if (_replay == 0)
        {
            return -1;
        }
        
        replayStatus = _replay->getStatus();
    }
    
    RecordingItem item;
    if (!_recordingItems->getItem(replayStatus.filename, replayStatus.position, &item) ||
        item.startPosition - replayStatus.position != 0)
    {
        return -1;
    }
    
    
    string clearedFilename;
    int64_t clearedPosition;
    int result = _recordingItems->clearItem(item.id, item.index, &clearedFilename, &clearedPosition);
    if (result >= 0)
    {
        LOCK_SECTION(_replayMutex);

        _replay->clearMark(clearedPosition, ITEM_START_MARK);
    }
    
    return result;
}

int RecordingSession::moveItemUp(int id, int index)
{
    TRY_LOCK_SECTION(_sessionStateChangeMutex);
    if (!HAVE_SECTION_LOCK())
    {
        return -1;
    }
    
    SessionState sessionState = getSessionState();
    if (sessionState != PREPARE_CHUNKING_SESSION_STATE)
    {
        return -1;
    }
    
    return _recordingItems->moveItemUp(id, index);
}

int RecordingSession::moveItemDown(int id, int index)
{
    TRY_LOCK_SECTION(_sessionStateChangeMutex);
    if (!HAVE_SECTION_LOCK())
    {
        return -1;
    }
    
    SessionState sessionState = getSessionState();
    if (sessionState != PREPARE_CHUNKING_SESSION_STATE)
    {
        return -1;
    }
    
    return _recordingItems->moveItemDown(id, index);
}

int RecordingSession::disableItem(int id, int index)
{
    TRY_LOCK_SECTION(_sessionStateChangeMutex);
    if (!HAVE_SECTION_LOCK())
    {
        return -1;
    }
    
    SessionState sessionState = getSessionState();
    if (sessionState != PREPARE_CHUNKING_SESSION_STATE)
    {
        return -1;
    }
    
    return _recordingItems->disableItem(id, index);
}

int RecordingSession::enableItem(int id, int index)
{
    TRY_LOCK_SECTION(_sessionStateChangeMutex);
    if (!HAVE_SECTION_LOCK())
    {
        return -1;
    }
    
    SessionState sessionState = getSessionState();
    if (sessionState != PREPARE_CHUNKING_SESSION_STATE)
    {
        return -1;
    }
    
    return _recordingItems->enableItem(id, index);
}

SessionState RecordingSession::getState()
{
    return getSessionState();
}

SessionStatus RecordingSession::getStatus(const ConfidenceReplayStatus* replayStatus)
{
    checkChunkingStatus();

    SessionState state = getSessionState();

    SessionStatus status;
    status.state = state;
    status.sourceSpoolNo = _recordingItems->getSourceSpoolNo();
    status.digibetaSpoolNo = _digibetaSpoolNo;
    status.statusMessage = _statusMessage;
    status.itemCount = _recordingItems->getItemCount();
    status.fileFormat = ingest_format_to_string(_profile->getIngestFormat(), false);
    status.filename = strip_path(_mxfFilename);
    status.browseFilename = strip_path(_browseFilename);
    status.vtrErrorCount = _vtrErrorCount;
    status.digiBetaDropoutEnabled = _digiBetaDropoutEnabled;
    status.sessionCommentsCount = _sessionCommentsCount;
    
    
    if (state == READY_SESSION_STATE || 
        state == RECORDING_SESSION_STATE)
    {
        RecordStats stats;
        _recorder->getCapture()->get_record_stats(&stats);
        
        status.vtrState = _sourceVTRControl->getState();
        if (state == RECORDING_SESSION_STATE)
        {
            status.digiBetaDropoutCount = stats.digiBetaDropoutCount;
            status.duration = stats.current_framecount;
            status.startVITC = stats.start_vitc;
            status.startLTC = stats.start_ltc;
            status.currentVITC = stats.current_vitc;
            status.currentLTC = stats.current_ltc;
            status.fileSize = stats.file_size;
            status.browseFileSize = stats.browse_file_size;
            status.browseBufferOverflow = _browseBufferOverflow;
            status.dvsBuffersEmpty = stats.dvs_buffers_empty;
            status.numDVSBuffers = stats.num_dvs_buffers;
            status.captureBufferPos = stats.capture_buffer_pos;
            status.storeBufferPos = stats.store_buffer_pos;
            status.browseBufferPos = stats.browse_buffer_pos;
            status.ringBufferSize = stats.ring_buffer_size;
        }
        else
        {
            status.digiBetaDropoutCount = 0;
            status.duration = 0;
            status.startVITC = g_nullTimecode;
            status.startLTC = g_nullTimecode;
            status.currentVITC = g_nullTimecode;
            status.currentLTC = g_nullTimecode;
            status.fileSize = 0;
            status.browseFileSize = 0;
            status.browseBufferOverflow = false;
            status.dvsBuffersEmpty = 0;
            status.numDVSBuffers = 0;
            status.captureBufferPos = 0;
            status.storeBufferPos = 0;
            status.browseBufferPos = 0;
            status.ringBufferSize = 0;
        }
        status.infaxDuration = _infaxDuration;
        status.diskSpace = _diskSpace;
        status.startBusy = _startBusy;
        status.stopBusy = _stopBusy;
        status.abortBusy = _abortBusy;
    }
    else if (state == REVIEWING_SESSION_STATE ||
        state == PREPARE_CHUNKING_SESSION_STATE ||
        state == CHUNKING_SESSION_STATE)
    {
        if (state == CHUNKING_SESSION_STATE)
        {
            LOCK_SECTION(_chunkingMutex);

            if (_chunking != 0)
            {
                ChunkingStatus chunkingStatus = (dynamic_cast<Chunking*>(_chunking->getWorker()))->getStatus();
                
                status.chunkingItemNumber = chunkingStatus.itemNumber;
                status.chunkingPosition = chunkingStatus.frameNumber;
                status.chunkingDuration = chunkingStatus.duration;
            }
        }
        
        status.digiBetaDropoutCount = _digiBetaDropoutCount;
        status.duration = _duration;
        status.pseResult = _pseResult;
        status.infaxDuration = _infaxDuration;
        status.chunkBusy = _chunkBusy;
        status.completeBusy = _completeBusy;
        
        if (state == PREPARE_CHUNKING_SESSION_STATE)
        {
            status.readyToChunk = Chunking::readyForChunking(_recordingItems);
        }
        
        if (replayStatus->position >= 0)
        {
            RecordingItem item;
            if (_recordingItems->getItem(replayStatus->filename, replayStatus->position, &item))
            {
                status.playingItemId = item.id;
                status.playingItemIndex = item.index;
                status.playingItemPosition = replayStatus->position - item.startPosition;
            }
        }
        
        _recordingItems->getItemsChangeCount(&status.itemClipChangeCount, &status.itemSourceChangeCount);
    }
    
    return status;
}

bool RecordingSession::getSessionComments(string* comments, int* count)
{
    LOCK_SECTION(_controlMutex);
    
    *comments = _sessionComments;
    *count = _sessionCommentsCount;
    return true;
}

void RecordingSession::browseBufferOverflow()
{
    SessionState sessionState = getSessionState();
    if (sessionState == RECORDING_SESSION_STATE)
    {
        _browseBufferOverflow = true;
    }
}

bool RecordingSession::confidenceReplayActive()
{
    LOCK_SECTION(_replayMutex);
    
    return _replay != 0;
}

void RecordingSession::forwardConfidenceReplayControl(string command)
{
    LOCK_SECTION(_replayMutex);

    if (_replay == 0)
    {
        return;
    }
    
    _replay->forwardControl(command);
}

ConfidenceReplayStatus RecordingSession::getConfidenceReplayStatus()
{
    LOCK_SECTION(_replayMutex);

    if (_replay == 0)
    {
        return ConfidenceReplayStatus();
    }
    
    return _replay->getStatus();
}

RecordingItems* RecordingSession::getRecordingItems()
{
    return _recordingItems;
}

void RecordingSession::getFinalResult(SessionResult* result, string* sourceSpoolNo, string* failureReason)
{
    LOCK_SECTION(_sessionStateMutex);
    
    *result = _sessionResult;
    *sourceSpoolNo = _recordingItems->getFirstItem().sourceItem->spoolNo;
    *failureReason = _sessionFailureReason;
}

uint32_t RecordingSession::getContentPackageSize() const
{
    switch (_profile->getIngestFormat())
    {
        case MXF_UNC_8BIT_INGEST_FORMAT:
            return ArchiveMXFWriter::getContentPackageSize(true, _profile->num_audio_tracks, _profile->include_crc32);
        case MXF_UNC_10BIT_INGEST_FORMAT:
            return ArchiveMXFWriter::getContentPackageSize(false, _profile->num_audio_tracks, _profile->include_crc32);
        case MXF_D10_50_INGEST_FORMAT:
            return D10MXFWriter::getContentPackageSize();
        case UNKNOWN_INGEST_FORMAT:
        default:
            REC_ASSERT(false);
    }
}

Source* RecordingSession::createRecordingSource(Source* infaxSource)
{
    // TODO: using clone() might change if infaxSource becomes a different class
    Source* source = infaxSource->clone();
    source->recInstance = 0; // not relevant in infaxSource
    
    return source;
}

void RecordingSession::setSessionState(SessionState state)
{
    LOCK_SECTION(_sessionStateMutex);

    if (_sessionState != state)
    {
        Logging::info("Session state changed to '%s'\n", get_state_string(state).c_str());
    }
    _sessionState = state;
}

void RecordingSession::setSessionState(SessionState state, SessionResult result, string sessionFailureReason)
{
    LOCK_SECTION(_sessionStateMutex);

    if (_sessionState != state)
    {
        Logging::info("Session state changed to '%s'\n", get_state_string(state).c_str());
    }
    _sessionState = state;
    
    _sessionResult = result;
    _sessionFailureReason = sessionFailureReason;
}

SessionState RecordingSession::getSessionState()
{
    LOCK_SECTION(_sessionStateMutex);

    return _sessionState;
}

bool RecordingSession::startRecordingInternal()
{
    LOCK_SECTION(_sessionStateChangeMutex);
    
    GUARD_BUSY_STATE(_startBusy);

    // check session state
    
    SessionState sessionState = getSessionState();
    if (sessionState != READY_SESSION_STATE)
    {
        Logging::debug("Ignoring start record request because not ready\n");
        return false;
    }

    
    // check the VTRs are accessible and that tapes are present
    
    VTRState sourceVTRState = _sourceVTRControl->getState();
    if (sourceVTRState == NOT_CONNECTED_VTR_STATE)
    {
        _statusMessage = "Source VTR disconnected";
        Logging::warning("Attempted to start recording when source VTR is not connected\n");
        return false;
    }
    else if (sourceVTRState == REMOTE_LOCKOUT_VTR_STATE)
    {
        _statusMessage = "Source VTR remote lockout";
        Logging::warning("Attempted to start recording when source VTR is remote locked out\n");
        return false;
    }
    else if (sourceVTRState == TAPE_UNTHREADED_VTR_STATE)
    {
        _statusMessage = "No source Tape";
        Logging::warning("Attempted to start recording without source tape in VTR\n");
        return false;
    }
    
    if (_videotapeBackup)
    {
        VTRState digibetaVTRState = _digibetaVTRControl->getState();
        if (digibetaVTRState == NOT_CONNECTED_VTR_STATE)
        {
            _statusMessage = "Digibeta VTR disconnected";
            Logging::warning("Attempted to start recording when Digibeta VTR is not accessible\n");
            return false;
        }
        else if (digibetaVTRState == REMOTE_LOCKOUT_VTR_STATE)
        {
            _statusMessage = "Digibeta VTR remote lockout";
            Logging::warning("Attempted to start recording when Digibeta VTR is remote locked out\n");
            return false;
        }
        else if (digibetaVTRState == TAPE_UNTHREADED_VTR_STATE)
        {
            _statusMessage = "No Digibeta Tape";
            Logging::warning("Attempted to start recording without Digibeta tape in VTR\n");
            return false;
        }
    }

    
    // check that capture is not busy

    GeneralStats captureStats;
    _recorder->getCapture()->get_general_stats(&captureStats);
    if (captureStats.recording)
    {
        Logging::error("System error: failed to start recording because capture class is busy\n");
        _statusMessage = "System error";

        // abort the capture
        if (_recorder->getCapture()->abort_record())
        {
            Logging::info("Capture was aborted\n");
        }
        else
        {
            // TODO: need to halt the recorder completely?
            Logging::warning("Failed to abort capture\n");
        }
        
        return false;
    }
    
    
    
    Logging::info("Starting recording...\n");

    
    // pause the source VTR if it is not already playing
    if (_sourceVTRControl->getState() != PLAY_VTR_STATE && _sourceVTRControl->getState() != PAUSED_VTR_STATE)
    {
        _statusMessage = "Pause source VTR";
    
        // stop and set standby on
        if (!_sourceVTRControl->stop())
        {
            Logging::warning("Failed to stop source VTR\n");
            _statusMessage = "Stop source VTR failed";
            return false;
        }
        sleep_msec(500);
        if (!_sourceVTRControl->standbyOn())
        {
            Logging::warning("Failed to set source VTR standby on\n");
            _statusMessage = "Standby source VTR failed";
            return false;
        }
        
        // wait max 5 seconds for paused state
        Timer timer;
        timer.start(5 * SEC_IN_USEC);
        VTRState state;
        while ((state = _sourceVTRControl->getState()) != PAUSED_VTR_STATE && timer.timeLeft() > 0)
        {
            sleep_msec(2);
        }
        if (state != PAUSED_VTR_STATE)
        {
            Logging::error("Failed to set the source VTR in paused state within 5 seconds; state != PAUSED_VTR_STATE\n");
            _statusMessage = "Pause source VTR failed";
            return false;
        }

        // give it VTR_SIGNAL_OK_WAIT to stabilise the SDI signal
        sleep_sec(SDI_SIGNAL_PAUSE_WAIT_SEC);
    }
    
    // pause the Digibeta VTR if it is not already recording
    if (_videotapeBackup && _digibetaVTRControl->getState() != RECORDING_VTR_STATE &&
        _digibetaVTRControl->getState() != PAUSED_VTR_STATE)
    {
        _statusMessage = "Pause Digibeta VTR";
    
        // stop and set standby on
        if (!_digibetaVTRControl->stop())
        {
            Logging::warning("Failed to stop Digibeta VTR\n");
            _statusMessage = "Stop Digibeta VTR failed";
            return false;
        }
        sleep_msec(500);
        if (!_digibetaVTRControl->standbyOn())
        {
            Logging::warning("Failed to set Digibeta VTR standby on\n");
            _statusMessage = "Standby Digibeta VTR failed";
            return false;
        }
        
        // wait max 5 seconds for paused state
        Timer timer;
        timer.start(5 * SEC_IN_USEC);
        VTRState state;
        while ((state = _digibetaVTRControl->getState()) != PAUSED_VTR_STATE && timer.timeLeft() > 0)
        {
            sleep_msec(2);
        }
        if (state != PAUSED_VTR_STATE)
        {
            Logging::error("Failed to set the Digibeta VTR in paused state within 5 seconds; state != PAUSED_VTR_STATE\n");
            _statusMessage = "Pause Digibeta VTR failed";
            return false;
        }
    }
    
    // start the source vtr playing
    if (_sourceVTRControl->getState() != PLAY_VTR_STATE)
    {
        _statusMessage = "Play source VTR";
        
        if (!_sourceVTRControl->play())
        {
            Logging::warning("Failed to start source VTR playing\n");
            _statusMessage = "Play source VTR failed";
            return false;
        }
    
        // wait max 5 seconds until it is in play state
        Timer timer;
        timer.start(5 * SEC_IN_USEC);
        VTRState state;
        while ((state = _sourceVTRControl->getState()) != PLAY_VTR_STATE && timer.timeLeft() > 0)
        {
            sleep_msec(2);
        }
        if (state != PLAY_VTR_STATE)
        {
            Logging::error("Failed to start the source VTR playing within 5 seconds; state != PLAY_VTR_STATE\n");
            _statusMessage = "Play source VTR failed";
            return false;
        }
    }

    // give it SDI_SIGNAL_PLAY_WAIT to stabilise the SDI signal for the SDI capture card
    _statusMessage = "Wait for steady SDI";
    sleep_sec(SDI_SIGNAL_PLAY_WAIT_SEC);
    
    
    // check SDI status is ok to start recording
    _recorder->getCapture()->get_general_stats(&captureStats);
    if (!captureStats.video_ok || !captureStats.audio_ok)
    {
        Logging::error("Failed to start recording - SDI signal is bad\n");
        _statusMessage = "SDI signal bad";

        stopVTRs("after start recording failed");
        return false;
    }

    
    // start the Digibeta recording
    if (_videotapeBackup && _digibetaVTRControl->getState() != RECORDING_VTR_STATE)
    {
        _statusMessage = "Record Digibeta VTR";
        
        if (!_digibetaVTRControl->record())
        {
            Logging::warning("Failed to start recording in Digibeta VTR\n");
            _statusMessage = "Digibeta VTR record failed";

            stopVTRs("after start recording failed");
            return false;
        }
    }

    
    // Start recording
    try
    {
        _statusMessage = "Start record";
        
        Rational aspectRatio = Recorder::getRasterAspectRatio(_recordingItems->getFirstItem().sourceItem->aspectRatioCode);
        
        bool result;
        if (_recordingItems->haveMultipleItems())
        {
            // capture multi-item source
            result = _recorder->getCapture()->start_multi_item_record(
                _recorder->getCache()->getCompleteCreatingFilename(_mxfFilename).c_str(),
                _recorder->getCache()->getCompleteCreatingEventFilename(_eventFilename).c_str(),
                &aspectRatio);
        }
        else
        {
            // capture single item source
            result = _recorder->getCapture()->start_record(
                _recorder->getCache()->getCompleteCreatingFilename(_mxfFilename).c_str(),
                _recorder->getCache()->getCompleteBrowseFilename(_browseFilename).c_str(),
                _recorder->getCache()->getCompleteBrowseFilename(_browseTimecodeFilename).c_str(),
                _recorder->getCache()->getCompletePSEFilename(_pseFilename).c_str(),
                _recorder->getCache()->getCompleteCreatingEventFilename(_eventFilename).c_str(),
                &aspectRatio);
        }
        if (!result)
        {
            Logging::error("Failed to start recording\n");
            _statusMessage = "Start record failed";
            
            stopVTRs("after start recording failed");
            return false;
        }
    }
    catch (const RecorderException& ex)
    {
        Logging::error("Failed to start recording: %s\n", ex.getMessage().c_str());
        _statusMessage = "Start record failed";
        
        stopVTRs("after start recording failed");
        return false;
    }
    catch (...)
    {
        Logging::error("Failed to start recording\n");
        _statusMessage = "Start record failed";
        
        stopVTRs("after start recording failed");
        return false;
    }
    
    
    if (_videotapeBackup)
    {
        // check that the Digibeta is recording
        Timer timer;
        timer.start(5 * SEC_IN_USEC);
        VTRState state = _digibetaVTRControl->getState();
        while (state != RECORDING_VTR_STATE && timer.timeLeft() > 0)
        {
            sleep_msec(2);
            state = _digibetaVTRControl->getState();
        }
        if (state != RECORDING_VTR_STATE)
        {
            Logging::error("Failed to start the Digibeta VTR recording within 5 seconds; state != RECORDING_VTR_STATE\n");
            _statusMessage = "Digibeta VTR record failed";
    
            // abort the capture
            try
            {
                if (_recorder->getCapture()->abort_record())
                {
                    Logging::info("Capture was aborted\n");
                }
                else
                {
                    // TODO: need to halt the recorder completely?
                    Logging::warning("Failed to abort capture\n");
                }
            }
            catch (...)
            {
                Logging::warning("Failed to abort capture: exception thrown\n");
            }
            
            stopVTRs("after start recording failed");
            return false;
        }
    }

    
    if (!_recordingItems->haveMultipleItems())
    {
        // write the browse info file
        if (_profile->browse_enable)
        {
            RecordingItem firstItem = _recordingItems->getFirstItem();
            writeBrowseInfo(_recorder->getCache()->getCompleteBrowseFilename(_browseInfoFilename), 
                &firstItem, _browseBufferOverflow);
        }
    }
    
    
    _statusMessage = "Recording";
    setSessionState(RECORDING_SESSION_STATE);

    return true;
}

bool RecordingSession::stopRecordingInternal()
{
    LOCK_SECTION(_sessionStateChangeMutex);
    
    GUARD_BUSY_STATE(_stopBusy);
    
    // check session state
    
    SessionState sessionState = getSessionState();
    if (sessionState != RECORDING_SESSION_STATE)
    {
        Logging::debug("Ignoring stop record request because not recording\n");
        return false;
    }

    
    Logging::info("Stopping recording...\n");
    _statusMessage = "Stop record";
    
    // stop capturing
    try
    {
        LOCK_SECTION(_vtrErrorsMutex);
        
        _vtrErrorCount = _vtrErrors.size();

        bool stopOk;        
        int capturePSEResult;
        if (_recordingItems->haveMultipleItems())
        {
            stopOk = _recorder->getCapture()->stop_multi_item_record(-1, 
                _vtrErrors.size() > 0 ? &_vtrErrors[0] : 0, _vtrErrors.size(),
                &capturePSEResult, &_digiBetaDropoutCount);
        }
        else
        {
            RecordingItem firstItem = _recordingItems->getFirstItem();
            InfaxData sourceInfaxData;
            getInfaxData(&firstItem, &sourceInfaxData);
                
            stopOk = _recorder->getCapture()->stop_record(-1, &sourceInfaxData, 
                _vtrErrors.size() > 0 ? &_vtrErrors[0] : 0, _vtrErrors.size(),
                &capturePSEResult, &_digiBetaDropoutCount);
        }
        if (!stopOk)
        {
            Logging::error("Failed to stop recording\n");
            _statusMessage = "Stop record failed";
            return false;
        }
        if (capturePSEResult == 0)
        {
            _pseResult = PSE_RESULT_PASSED;
        }
        else if (capturePSEResult == 1)
        {
            _pseResult = PSE_RESULT_FAILED;
        }
        else
        {
            _pseResult = 0; // unknown and no PSE report written
        }
        
        
        // update the final status ready for review
        RecordStats stats;
        _recorder->getCapture()->get_record_stats(&stats);
        _duration = stats.current_framecount;
        _materialPackageUID = stats.materialPackageUID;
        _filePackageUID = stats.filePackageUID;
        _tapePackageUID = stats.tapePackageUID;
        _recordingItems->initialise(_recorder->getCache()->getCompleteCreatingFilename(_mxfFilename), _duration);

        // next state
        if (_recordingItems->haveMultipleItems())
        {
            setSessionState(PREPARE_CHUNKING_SESSION_STATE);
        }
        else
        {
            setSessionState(REVIEWING_SESSION_STATE);
        }
    }    
    catch (const RecorderException& ex)
    {
        Logging::error("Failed to stop recording: %s\n", ex.getMessage().c_str());
        _statusMessage = "Stop record failed";
        return false;
    }
    catch (...)
    {
        Logging::error("Failed to stop recording\n");
        _statusMessage = "Stop record failed";
        return false;
    }


    // stop the Digibeta recording
    if (_videotapeBackup && _digibetaVTRControl->getState() == RECORDING_VTR_STATE)
    {
        _statusMessage = "Stop Digibeta VTR";
        
        if (!_digibetaVTRControl->stop())
        {
            Logging::warning("Failed to stop Digibeta VTR recording when stopping the recording\n");
        }
    }
    
    // stop the source VTR playing
    if (_sourceVTRControl->getState() == PLAY_VTR_STATE)
    {
        _statusMessage = "Stop source VTR";
        
        if (!_sourceVTRControl->stop())
        {
            Logging::warning("Failed to stop source VTR playing when stopping the recording\n");
        }
    }
    
    
    _statusMessage = "Start confidence replay";
    
    // start the confidence replay
    if (!replayFile(_recorder->getCache()->getCompleteCreatingFilename(_mxfFilename), 0, _recordingItems->haveMultipleItems()))
    {
        Logging::error("Failed to start confidence replay of '%s'\n", _mxfFilename.c_str());
        _statusMessage = "Start confidence replay failed";
    }
    else
    {
        _statusMessage = "Confidence replay";
    }
    
    return true;
}

bool RecordingSession::chunkFileInternal()
{
    LOCK_SECTION(_sessionStateChangeMutex);
    LOCK_SECTION_2(_chunkingMutex);
    
    GUARD_BUSY_STATE(_chunkBusy);
    
    // check session state
    
    SessionState sessionState = getSessionState();
    if (sessionState != PREPARE_CHUNKING_SESSION_STATE)
    {
        return false;
    }

    
    // lock items
    _recordingItems->lockItems();

    
    // check ready for chunking 
    
    if (!Chunking::readyForChunking(_recordingItems))
    {
        Logging::debug("Attempting to start chunking when not ready\n");
        
        _recordingItems->unlockItems();
        return false;
    }

    
    Logging::info("Starting file chunking...\n");
    
    
    // stop the confidence replay
    
    _statusMessage = "Stop confidence replay";
    try
    {
        LOCK_SECTION(_replayMutex);
        SAFE_DELETE(_replay);
    }
    catch (...)
    {
        Logging::error("Failed to delete the confidence replay\n");
        _replay = 0;
    }
    

    // start the chunking thread
    
    Chunking* worker = 0;
    try
    {
        bool disablePSE = (_pseResult != PSE_RESULT_PASSED && _pseResult != PSE_RESULT_FAILED);
        
        Chunking* worker = new Chunking(this, _sessionTable,
            _recorder->getCache()->getCompleteCreatingFilename(_mxfFilename), _recordingItems, disablePSE);
        _chunking = new Thread(worker, true);
        _chunking->start();
    }
    catch (...)
    {
        if (_chunking == 0)
        {
            delete worker;
        }
        else
        {
            SAFE_DELETE(_chunking);
        }
        Logging::error("Failed to start chunking\n");
        _statusMessage = "Chunk file failed";
        
        abort(false, "File chunking failed to start");
        
        _recordingItems->unlockItems();
        return false;
    }

    
    _statusMessage = "Chunk file";
    setSessionState(CHUNKING_SESSION_STATE);
    
    return true;
}

bool RecordingSession::completeInternal(string comments)
{
    LOCK_SECTION(_sessionStateChangeMutex);
    
    GUARD_BUSY_STATE(_completeBusy);
    
    // check session state
    
    SessionState sessionState = getSessionState();
    if (sessionState != REVIEWING_SESSION_STATE)
    {
        Logging::debug("Ignoring complete record request because not reviewing\n");
        return false;
    }


    Logging::info("Completing session...\n");
    
    
    // stop the confidence replay
    
    _statusMessage = "Stop confidence replay";
    
    try
    {
        LOCK_SECTION(_replayMutex);
        SAFE_DELETE(_replay);
    }
    catch (...)
    {
        Logging::error("Failed to delete the confidence replay\n");
        _replay = 0;
    }
    

    _statusMessage = "Complete session";
    
    try
    {
        // remove the mxf page file and associated session destination
        
        if (_recordingItems->haveMultipleItems())
        {
            vector<RecordingSessionDestinationTable*>::iterator iter1;
            for (iter1 = _sessionTable->destinations.begin(); iter1 != _sessionTable->destinations.end(); iter1++)
            {
                if ((*iter1)->destination->concreteDestination == _hddDest)
                {
                    // delete session destination (note: destination is not ownded)
                    RecorderDatabase::getInstance()->deleteSessionDestination(*iter1);
                    delete *iter1;
                    _sessionTable->destinations.erase(iter1);
                    
                    // erase from _destinations and delete the Destination
                    vector<Destination*>::iterator iter2;
                    for (iter2 = _destinations.begin(); iter2 != _destinations.end(); iter2++)
                    {
                        HardDiskDestination* hddDest = dynamic_cast<HardDiskDestination*>((*iter2)->concreteDestination);
                        if (hddDest == _hddDest)
                        {
                            delete *iter2;
                            _destinations.erase(iter2);
                            _hddDest = 0;
                            break;
                        }
                    }
                    
                    break;
                }
            }

            _recorder->getCache()->removeCreatingItem(_mxfFilename);
        }
        
        
        // update session status
        
        _sessionTable->status = RECORDING_SESSION_COMPLETED;
        _sessionTable->comments = comments.substr(0, 255);
        _sessionTable->totalVTRErrors = _vtrErrorCount;
        _sessionTable->totalDigiBetaDropouts = _digiBetaDropoutCount;
        
        
        // update hard disk destination
        
        if (_recordingItems->haveMultipleItems())
        {
            vector<Destination*>::const_iterator iter;
            for (iter = _destinations.begin(); iter != _destinations.end(); iter++)
            {
                HardDiskDestination* hddDest = dynamic_cast<HardDiskDestination*>((*iter)->concreteDestination);
                if (hddDest == 0)
                {
                    // digibeta destination
                    continue;
                }
                
                _recorder->getCache()->updateCreatingItem(hddDest, _sessionTable);
            }
        }
        else
        {
            RecordStats stats;
            _recorder->getCapture()->get_record_stats(&stats);
            
            _hddDest->duration = stats.current_framecount;
            _hddDest->pseResult = _pseResult;
            _hddDest->size = _recorder->getCache()->getCreatingFileSize(_hddDest->name);
            _hddDest->browseSize = _recorder->getCache()->getBrowseFileSize(_hddDest->browseName);
            _hddDest->materialPackageUID = _materialPackageUID;
            _hddDest->filePackageUID = _filePackageUID;
            _hddDest->tapePackageUID = _tapePackageUID;
    
            updateHardDiskDestination(_hddDest);
        }

        
        // finalise cache items
        
        if (_recordingItems->haveMultipleItems())
        {
            vector<Destination*>::const_iterator iter;
            for (iter = _destinations.begin(); iter != _destinations.end(); iter++)
            {
                HardDiskDestination* hddDest = dynamic_cast<HardDiskDestination*>((*iter)->concreteDestination);
                if (hddDest == 0)
                {
                    // digibeta destination
                    continue;
                }
                
                _recorder->getCache()->finaliseCreatingItem(hddDest);
            }
        }
        else
        {
            _recorder->getCache()->finaliseCreatingItem(_hddDest);
        }

        
        // update the session status 
        
        RecorderDatabase::getInstance()->updateSession(_sessionTable);
        
        
        // rewrite browse info for single item
        
        if (!_recordingItems->haveMultipleItems())
        {
            RecordingItem firstItem = _recordingItems->getFirstItem();
            writeBrowseInfo(_recorder->getCache()->getCompleteBrowseFilename(_browseInfoFilename), 
                &firstItem, _browseBufferOverflow);
        }

        
    }
    catch (...)
    {
        Logging::error("Failed to successfully complete the session - aborting\n");
        _statusMessage = "Session complete failed";
        abort(false, "Failed to complete session successfully");
        return false;
    }
    
    setSessionState(END_SESSION_STATE, COMPLETED_SESSION_RESULT, "");
    _statusMessage = "Session complete";
    
    return true;
}

bool RecordingSession::abortInternal(bool fromUser, string comments)
{
    LOCK_SECTION(_sessionStateChangeMutex);
    
    GUARD_BUSY_STATE(_abortBusy);
    
    // check session state
    
    SessionState sessionState = getSessionState();
    if (sessionState == NOT_STARTED_SESSION_STATE ||
        sessionState == END_SESSION_STATE)
    {
        Logging::debug("Ignoring abort record request because not started or ended session\n");
        return false;
    }

    
    if (fromUser)
    {
        Logging::info("User aborted session...\n");
    }
    else
    {
        Logging::info("System aborted session: %s ...\n", comments.c_str());
    }

    // stop the digibeta vtr recording
    if (_videotapeBackup && sessionState == RECORDING_SESSION_STATE &&
        _digibetaVTRControl->getState() == RECORDING_VTR_STATE)
    {
        _statusMessage = "Stop Digibeta VTR";
        
        Logging::info("Stopping Digibeta for aborted session\n");
        if (!_digibetaVTRControl->stop())
        {
            Logging::warning("Failed to stop Digibeta VTR playing when aborting\n");
        }
    }
    
    // stop the vtr playing
    if (sessionState == RECORDING_SESSION_STATE && _sourceVTRControl->getState() == PLAY_VTR_STATE)
    {
        _statusMessage = "Stop source VTR";
        
        Logging::info("Stopping source for aborted session\n");
        if (!_sourceVTRControl->stop())
        {
            Logging::warning("Failed to stop source VTR playing when aborting\n");
        }
    }
    
    // stop the confidence replay
    try
    {
        _statusMessage = "Stop confidence replay";
        
        LOCK_SECTION(_replayMutex);
        SAFE_DELETE(_replay);
    }
    catch (...)
    {
        Logging::error("Failed to delete confidence replay\n");
        _replay = 0;
    }
    
    
    // stop recording
    try
    {
        if (sessionState == RECORDING_SESSION_STATE)
        {
            // TODO: cannot recover if this fails and the recorder should be stopped! 
            REC_CHECK(_recorder->getCapture()->abort_record());
        }
    }
    catch (...)
    {
        Logging::error("Failed to stop the recording\n");
        return false;
    }

    // stop chunking
    try
    {
        LOCK_SECTION(_chunkingMutex);
        SAFE_DELETE(_chunking);
    }
    catch (...)
    {
        Logging::error("Failed to stop the chunking\n");
        return false;
    }
    
    
    try
    {
        _statusMessage = "Abort session";

        // assume the end of the session even if something fails below
        string failureReason;
        if (fromUser)
        {
            failureReason = "user aborted session";
        }
        else
        {
            failureReason = comments;
        }
        setSessionState(END_SESSION_STATE, FAILED_SESSION_RESULT, failureReason);
    
        // remove items from cache creating directory
        _recorder->getCache()->removeCreatingItems();
    
    
        // remove the session and destinations if the recording wasn't started
        if (sessionState == NOT_STARTED_SESSION_STATE ||
            sessionState == READY_SESSION_STATE)
        {
            RecorderDatabase::getInstance()->deleteSession(_sessionTable);
            SAFE_DELETE(_sessionTable);
            
            vector<Destination*>::const_iterator iter;
            for (iter = _destinations.begin(); iter != _destinations.end(); iter++)
            {
                RecorderDatabase::getInstance()->deleteDestination(*iter);
                delete *iter;
            }
            _destinations.clear();
            
            Logging::info("Deleted session and destinations from the database\n");
        }
        else
        {
            // update the status in the database
            _sessionTable->status = RECORDING_SESSION_ABORTED;
            _sessionTable->abortInitiator = (fromUser) ? USER_ABORT_SESSION : SYSTEM_ABORT_SESSION;
            _sessionTable->comments = comments.substr(0, 255);
            RecorderDatabase::getInstance()->updateSession(_sessionTable);
    
            Logging::info("Updated session status to abort in database and removed cache link from file destination\n");
        }

        Logging::info("Aborted session\n");
        _statusMessage = "Session aborted";
    }
    catch (...)
    {
        Logging::error("Failed to fully abort the session\n");
        _statusMessage = "Session abort failed";
    }
    
    return true;
}


void RecordingSession::writeBrowseInfo(string filename, RecordingItem* item, bool browseBufferOverflow)
{
    FILE* file;
    if ((file = fopen(filename.c_str(), "wb")) == 0)
    {
        Logging::error("Failed to open browse info file '%s': %s\n", filename.c_str(), strerror(errno));
        return;
    }
    
    // Transfer information
    
    fprintf(file, "Transfer Information\n\n");
    
    fprintf(file, "Recorder station: '%s'\n", _recorder->getName().c_str());
    fprintf(file, "Local time: %s\n", get_user_timestamp_string(get_localtime_now()).c_str());
    if (browseBufferOverflow)
    {
        fprintf(file, "ERROR: a browse buffer overflow occurred resulting in an incomplete frame sequence.\n");
    }
    
    
    // source information
    
    fprintf(file, "\n\n\nSource Information\n\n");
    
    vector<AssetProperty> sourceProps = item->sourceItem->getSourceProperties();
    vector<AssetProperty>::const_iterator iter;
    for (iter = sourceProps.begin(); iter != sourceProps.end(); iter++)
    {
        fprintf(file, "%s: %s\n", (*iter).title.c_str(), (*iter).value.c_str()); 
    }
    
    fclose(file);
}

void RecordingSession::getInfaxData(RecordingItem* item, InfaxData* infaxData)
{
    SourceItem* sourceItem = item->sourceItem;

    // initialise and ensure strings are null terminated
    memset((void*)infaxData, 0, sizeof(InfaxData));
    
    strncpy(infaxData->format, sourceItem->format.c_str(), FORMAT_SIZE - 1);
    strncpy(infaxData->progTitle, sourceItem->progTitle.c_str(), PROGTITLE_SIZE - 1);
    strncpy(infaxData->epTitle, sourceItem->episodeTitle.c_str(), EPTITLE_SIZE - 1);
    infaxData->txDate.year = sourceItem->txDate.year;
    infaxData->txDate.month = sourceItem->txDate.month;
    infaxData->txDate.day = sourceItem->txDate.day;
    strncpy(infaxData->magPrefix, sourceItem->magPrefix.c_str(), MAGPREFIX_SIZE - 1);
    strncpy(infaxData->progNo, sourceItem->progNo.c_str(), PROGNO_SIZE - 1);
    strncpy(infaxData->prodCode, sourceItem->prodCode.c_str(), PRODCODE_SIZE - 1);
    strncpy(infaxData->spoolStatus, sourceItem->spoolStatus.c_str(), SPOOLSTATUS_SIZE - 1);
    infaxData->stockDate.year = sourceItem->stockDate.year;
    infaxData->stockDate.month = sourceItem->stockDate.month;
    infaxData->stockDate.day = sourceItem->stockDate.day;
    strncpy(infaxData->spoolDesc, sourceItem->spoolDescr.c_str(), SPOOLDESC_SIZE - 1);
    strncpy(infaxData->memo, sourceItem->memo.c_str(), MEMO_SIZE - 1);
    infaxData->duration = sourceItem->duration / 25; // "/ 25" because infax precision in seconds
    strncpy(infaxData->spoolNo, sourceItem->spoolNo.c_str(), SPOOLNO_SIZE - 1);
    strncpy(infaxData->accNo, sourceItem->accNo.c_str(), ACCNO_SIZE - 1);
    strncpy(infaxData->catDetail, sourceItem->catDetail.c_str(), CATDETAIL_SIZE - 1);
    infaxData->itemNo = sourceItem->itemNo;
}

bool RecordingSession::replayFile(string filename, int64_t startPosition, bool showSecondMarkBar)
{
    if (filename.empty())
    {
        return false;
    }

    LOCK_SECTION(_replayMutex);
    
    // Note: we don't bother marking clip item starts because the system as deployed
    // at I&A will never have items marked after chunking and the file is only
    // opened once before chunking
    
    try
    {
        if (_replay == 0 || _replay->getStatus().filename != filename)
        {
            // first replay or replay of a different file
            SAFE_DELETE(_replay);
        
            string eventFilename = _recorder->getCache()->getCreatingEventFilename(filename);
            
            _replay = new ConfidenceReplay(filename, eventFilename, _replayPort, startPosition, showSecondMarkBar);
            _replay->start();
        }
        else
        {
            // position file at start position
            _replay->seek(startPosition);
        }
        
        return true;
    }
    catch (const RecorderException& ex)
    {
        try { delete _replay; } catch (...){}
        _replay = 0;
        Logging::error("Failed to start confidence replay: %s\n", ex.getMessage().c_str());
        return false;
    }
    catch (...)
    {
        try { delete _replay; } catch (...){}
        _replay = 0;
        Logging::error("Failed to start confidence replay\n");
        return false;
    }
}

void RecordingSession::checkChunkingStatus()
{
    TRY_LOCK_SECTION(_sessionStateChangeMutex);
    if (!HAVE_SECTION_LOCK())
    {
        return;
    }
    
    LOCK_SECTION_2(_chunkingMutex);
    
    if (_chunking == 0 || _chunking->isRunning())
    {
        return;
    }
    
    // abort session if chunking failed
    ChunkingStatus status = (dynamic_cast<Chunking*>(_chunking->getWorker()))->getStatus();
    if (status.state != CHUNKING_COMPLETED)
    {
        Logging::info("Chunking failed\n");
        SAFE_DELETE(_chunking);
        
        abort(false, "Chunking failed");
        return;
    }
    
    
    SAFE_DELETE(_chunking);

    // start the confidence replay
    if (!replayFile(_recordingItems->getFirstItem().filename, 0, false))
    {
        Logging::error("Failed to start confidence replay of '%s'\n", _recordingItems->getFirstItem().filename.c_str());
        _statusMessage = "Start confidence replay failed";
        abort(false, "Failed to start confidence replay");
        return;
    }
    else
    {
        _statusMessage = "Confidence replay";
    }
    
    setSessionState(REVIEWING_SESSION_STATE);
}

HardDiskDestination* RecordingSession::addHardDiskDestination(string mxfFilename,
    string browseFilename, string pseFilename, SourceItem* sourceItem, int ingestItemNo)
{
    //  hard disk destination
    _destinations.push_back(new Destination());
    Destination* dest = _destinations.back();
    dest->sourceId = _recordingItems->getSource()->databaseId;
    dest->sourceItemId = (ingestItemNo > 0) ? sourceItem->databaseId : -1;
    dest->ingestItemNo = ingestItemNo;
    dest->concreteDestination = new HardDiskDestination();
    
    HardDiskDestination* hddDest = dynamic_cast<HardDiskDestination*>(dest->concreteDestination);
    hddDest->ingestFormat = _profile->getIngestFormat();
    hddDest->hostName = get_host_name();
    hddDest->path = _recorder->getCache()->getPath();
    hddDest->name = mxfFilename;
    hddDest->browsePath = _recorder->getCache()->getBrowsePath();
    hddDest->browseName = browseFilename;
    hddDest->psePath = _recorder->getCache()->getPSEPath();
    hddDest->pseName = pseFilename;
    hddDest->pseResult = 0;
    hddDest->cacheId = _recorder->getCache()->getDatabaseId();

    _sessionTable->destinations.push_back(new RecordingSessionDestinationTable());
    RecordingSessionDestinationTable* sessionDestTable = _sessionTable->destinations.back();
    sessionDestTable->destination = dest;

    // save new destination
    RecorderDatabase::getInstance()->saveSessionDestinations(_sessionTable);
    
    // register new hdd destination item with the cache
    _recorder->getCache()->registerCreatingItem(hddDest, _sessionTable, sourceItem, ingestItemNo < 1);
    
    return hddDest;
}

DigibetaDestination* RecordingSession::addDigibetaDestination(string barcode)
{
    REC_ASSERT(_videotapeBackup);
    
    _destinations.push_back(new Destination());
    Destination* dest = _destinations.back();
    dest->barcode = barcode;
    dest->sourceId = _recordingItems->getSource()->databaseId;
    dest->sourceItemId = -1;
    dest->ingestItemNo = -1;
    DigibetaDestination* digibetaDestination = new DigibetaDestination();
    dest->concreteDestination = digibetaDestination;
    
    _sessionTable->destinations.push_back(new RecordingSessionDestinationTable());
    RecordingSessionDestinationTable* sessionDestTable = _sessionTable->destinations.back();
    sessionDestTable->destination = dest;

    // save new destination
    RecorderDatabase::getInstance()->saveSessionDestinations(_sessionTable);
    
    return digibetaDestination;
}

void RecordingSession::updateHardDiskDestination(HardDiskDestination* hddDest)
{
    _recorder->getCache()->updateCreatingItem(hddDest, _sessionTable);
    RecorderDatabase::getInstance()->updateHardDiskDestination(hddDest);
}        

void RecordingSession::stopVTRs(string errorContext)
{
    // stop the VTRs
    if (!_sourceVTRControl->stop())
    {
        Logging::warning("Failed to stop the source VTR %s\n", errorContext.c_str());
    }
    if (_videotapeBackup && !_digibetaVTRControl->stop())
    {
        Logging::warning("Failed to stop the Digibeta VTR %s\n", errorContext.c_str());
    }
}

