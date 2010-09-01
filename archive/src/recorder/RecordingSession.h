/*
 * $Id: RecordingSession.h,v 1.2 2010/09/01 16:05:22 philipn Exp $
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
 
#ifndef __RECORDER_RECORDING_SESSION_H__
#define __RECORDER_RECORDING_SESSION_H__

#include "DatabaseThings.h"
#include "Threads.h"
#include "ConfidenceReplay.h"
#include "VTRControl.h"
#include "RecordingItems.h"
#include "Chunking.h"
#include "Profile.h"

#include <archive_types.h>


namespace rec
{

typedef enum
{
    UNKNOWN_SESSION_RESULT = 0,
    COMPLETED_SESSION_RESULT,
    FAILED_SESSION_RESULT
} SessionResult;
    
typedef enum
{
    NOT_STARTED_SESSION_STATE = 0,
    READY_SESSION_STATE,
    RECORDING_SESSION_STATE,
    REVIEWING_SESSION_STATE,
    PREPARE_CHUNKING_SESSION_STATE,
    CHUNKING_SESSION_STATE,
    END_SESSION_STATE
} SessionState;


class SessionStatus
{
public:
    SessionStatus()
    : state(NOT_STARTED_SESSION_STATE), vtrErrorCount(0),
    digiBetaDropoutEnabled(false), digiBetaDropoutCount(0), duration(-1),
    abortBusy(false), browseBufferOverflow(false), itemCount(-1), sessionCommentsCount(0),
    vtrState(NOT_CONNECTED_VTR_STATE), infaxDuration(-1), fileSize(-1), diskSpace(-1), 
    browseFileSize(-1), startBusy(false), stopBusy(false),
    dvsBuffersEmpty(0), numDVSBuffers(0), captureBufferPos(0), storeBufferPos(0), browseBufferPos(0), ringBufferSize(0),
    pseResult(0), chunkBusy(false), completeBusy(false), 
    playingItemId(-1), playingItemIndex(-1), playingItemPosition(-1), 
    itemClipChangeCount(0), itemSourceChangeCount(0),
    chunkingItemNumber(-1), chunkingPosition(-1), chunkingDuration(-1), readyToChunk(false)
    {}
    

    // both Record and Review states
    
    SessionState state;
    std::string sourceSpoolNo;
    std::string digibetaSpoolNo;
    std::string statusMessage;
    long vtrErrorCount;
    bool digiBetaDropoutEnabled;
    long digiBetaDropoutCount;
    int64_t duration;
    std::string fileFormat;
    std::string filename;
    std::string browseFilename;
    bool abortBusy;
    bool browseBufferOverflow;
    int itemCount;
    int sessionCommentsCount;
    
    // Record only
    VTRState vtrState;
    Timecode startVITC;
    Timecode startLTC;
    Timecode currentVITC;
    Timecode currentLTC;
    int64_t infaxDuration;
    int64_t fileSize;
    int64_t diskSpace;
    int64_t browseFileSize;
    bool startBusy;
    bool stopBusy;
    int dvsBuffersEmpty;
    int numDVSBuffers;
    int captureBufferPos;
    int storeBufferPos;
    int browseBufferPos;
    int ringBufferSize;
    
    // Review only
    int pseResult;
    bool chunkBusy;
    bool completeBusy;
    int playingItemId;
    int playingItemIndex;
    int64_t playingItemPosition;
    int itemClipChangeCount;
    int itemSourceChangeCount;
    int chunkingItemNumber;
    int64_t chunkingPosition;
    int64_t chunkingDuration;
    bool readyToChunk;
};



class Recorder;

class RecordingSession : public ThreadWorker, public VTRControlListener
{
public:
    friend class Recorder;
    friend class Chunking;
    
public:
    RecordingSession(Recorder* recorder, const Profile *profile, Source* infaxSource, std::string digibetaBarcode,
        int replayPort, VTRControl* sourceVTRControl, VTRControl* digibetaVTRControl);
    virtual ~RecordingSession();
    
    // from ThreadWorker    
    virtual void start();
    virtual void stop();
    virtual bool hasStopped() const;
    
    // from VTRControlListener    
    virtual void vtrState(VTRControl* vtrControl, VTRState state, const unsigned char* stateBytes);
    virtual void vtrPlaybackError(VTRControl* vtrControl, int errorCode, Timecode ltc, Timecode vitc);

    void startRecording();
    void stopRecording();
    void chunkFile();
    void abort(bool fromUser, std::string comments);
    bool abortIfRecording(bool fromUser, std::string comments);
    void complete(std::string comments);
    void setSessionComments(std::string comments);
    
    void playItem(int id, int index);
    void playPrevItem();
    void playNextItem();
    void seekToEOP();
    int markItemStart(int* id, bool* isJunk, int64_t* filePosition, int64_t* fileDuration);
    int clearItem(int id, int index, int64_t* filePosition);
    int markItemStart();
    int clearItem();
    int moveItemUp(int id, int index);
    int moveItemDown(int id, int index);
    int disableItem(int id, int index);
    int enableItem(int id, int index);
    
    SessionState getState();
    SessionStatus getStatus(const ConfidenceReplayStatus* replayStatus);
    bool getSessionComments(std::string* comments, int* count);
    
    void browseBufferOverflow();
    
    bool confidenceReplayActive();
    void forwardConfidenceReplayControl(std::string command);
    ConfidenceReplayStatus getConfidenceReplayStatus();

    RecordingItems* getRecordingItems();
    
    void getFinalResult(SessionResult* result, std::string* sourceSpoolNo, std::string* failureReason);
    
    uint32_t getContentPackageSize() const;
    IngestFormat getIngestFormat() const { return _profile->getIngestFormat(); }
    
private:
    Source* createRecordingSource(Source* infaxSource);
    
    void setSessionState(SessionState state);
    void setSessionState(SessionState state, SessionResult result, std::string sessionFailureReason);
    SessionState getSessionState();
    
    bool startRecordingInternal();
    bool stopRecordingInternal();
    bool chunkFileInternal();
    bool abortInternal(bool fromUser, std::string comments);
    bool completeInternal(std::string comments);
    
    void writeBrowseInfo(std::string filename, RecordingItem* item, bool browseBufferOverflow);
    void getInfaxData(RecordingItem* item, InfaxData* infaxData);
    
    bool replayFile(std::string filename, int64_t startPosition, bool showSecondMarkBar);
    
    void checkChunkingStatus();
    
    HardDiskDestination* addHardDiskDestination(std::string mxfFilename,
        std::string browseFilename, std::string pseFilename, SourceItem* sourceItem, int ingestItemNo);
    DigibetaDestination* addDigibetaDestination(std::string barcode);
    void updateHardDiskDestination(HardDiskDestination* hddDest);
    
    void stopVTRs(std::string errorContext);

    const Profile* getProfile() const { return _profile; }
    
private:
    Recorder* _recorder;
    Profile* _profile;
    RecordingItems* _recordingItems;
    
    bool _videotapeBackup;
    
    Mutex _chunkingMutex;
    Thread* _chunking;

    std::vector<Destination*> _destinations;
    std::string _digibetaSpoolNo;
    
    VTRControl* _sourceVTRControl;
    VTRControl* _digibetaVTRControl;

    RecordingSessionTable* _sessionTable;
    HardDiskDestination* _hddDest;
    
    int64_t _infaxDuration;
    
    std::string _mxfFilename;
    std::string _browseFilename;
    std::string _browseTimecodeFilename;
    std::string _browseInfoFilename;
    std::string _pseFilename;
    std::string _eventFilename;
    
    // session status
    Mutex _sessionStateMutex;
    Mutex _sessionStateChangeMutex;
    SessionState _sessionState;
    bool _startBusy;
    bool _stopBusy;
    bool _chunkBusy;
    bool _completeBusy;
    bool _abortBusy;
    int64_t _diskSpace;
    std::string _statusMessage;
    long _vtrErrorCount;
    bool _digiBetaDropoutEnabled;
    long _digiBetaDropoutCount;
    int _pseResult;
    bool _browseBufferOverflow;
    int64_t _duration;
    UMID _materialPackageUID;
    UMID _filePackageUID;
    UMID _tapePackageUID;
    SessionResult _sessionResult;
    std::string _sessionFailureReason;
    
    // session thread
    bool _stopThread;
    bool _threadHasStopped;
    
    // session control
    Mutex _controlMutex;    
    bool _startCalled;
    bool _stopCalled;
    bool _chunkFileCalled;
    bool _completeCalled;
    bool _abortCalled;
    bool _abortFromUser;
    std::string _abortComments;
    std::string _completeComments;
    std::string _sessionComments;
    int _sessionCommentsCount;
    
    // confidence replay
    int _replayPort;
    ConfidenceReplay* _replay;
    Mutex _replayMutex;
    
    // vtr errors
    std::vector<VTRError> _vtrErrors;
    Timecode _lastVTRErrorLTC;
    Timecode _lastVTRErrorVITC;
    Mutex _vtrErrorsMutex;
};



};





#endif

