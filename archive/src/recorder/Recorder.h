/*
 * $Id: Recorder.h,v 1.2 2010/09/01 16:05:22 philipn Exp $
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
 
#ifndef __RECORDER_RECORDER_H__
#define __RECORDER_RECORDER_H__


#include <vector>

// TODO: capture.h should have these
#include <pthread.h>
#include <stdio.h>
#include <capture.h>

#include "Cache.h"
#include "RecordingSession.h"
#include "Threads.h"
#include "VTRControl.h"
#include "JogShuttleControl.h"
#include "Profile.h"

// return failure codes for startNewSession
#define SESSION_IN_PROGRESS_FAILURE         1
#define VIDEO_SIGNAL_BAD_FAILURE            2
#define AUDIO_SIGNAL_BAD_FAILURE            3
#define SOURCE_VTR_CONNECT_FAILURE          4
#define SOURCE_VTR_REMOTE_LOCKOUT_FAILURE   5
#define NO_SOURCE_TAPE                      6
#define DIGIBETA_VTR_CONNECT_FAILURE        7
#define DIGIBETA_VTR_REMOTE_LOCKOUT_FAILURE 8
#define NO_DIGIBETA_TAPE                    9
#define DISK_SPACE_FAILURE                  10
#define MULTI_ITEM_MXF_EXISTS_FAILURE       11
#define MULTI_ITEM_NOT_ENABLED_FAILURE      12
#define INVALID_ASPECT_RATIO_FAILURE        13
#define UNKNOWN_PROFILE_FAILURE             14
#define DISABLED_INGEST_FORMAT_FAILURE      15
#define MULTI_ITEM_INGEST_FORMAT_FAILURE    16
#define INTERNAL_FAILURE                    20

// return failure codes for replayFile
#define SESSION_IN_PROGRESS_REPLAY_FAILURE  1
#define UNKNOWN_FILE_REPLAY_FAILURE         2
#define INTERNAL_REPLAY_FAILURE             20


// size of page files used to record multi-item tapes; equivalent to approx. 1 min recording
#define MXF_PAGE_FILE_SIZE                  (1313622000LL)

// more than 2 * MXF_PAGE_FILE_SIZE + approx 10 seconds space required to start or continue an ingest session 
#define DISK_SPACE_MARGIN                   (2 * MXF_PAGE_FILE_SIZE + 218937000LL)


namespace rec
{

class RecorderStatus
{
public:
    RecorderStatus() : databaseOk(false), sdiCardOk(false), videoOk(false), audioOk(false), 
        readyToRecord(false),
        sourceVTRState(NOT_CONNECTED_VTR_STATE), digibetaVTRState(NOT_CONNECTED_VTR_STATE),
        replayActive(false) {}
    
    std::string recorderName;
    bool databaseOk;
    bool sdiCardOk;
    bool videoOk;
    bool audioOk;
    bool vtrOk;
    bool readyToRecord;
    VTRState sourceVTRState;
    VTRState digibetaVTRState;
    bool replayActive;
    std::string replayFilename;
};

class RecorderSystemStatus
{
public:
    RecorderSystemStatus() : remDiskSpace(0), remDuration(0), remDurationIngestFormat(UNKNOWN_INGEST_FORMAT),
        sourceVTRState(NOT_CONNECTED_VTR_STATE), digibetaVTRState(NOT_CONNECTED_VTR_STATE) {}
    
    int64_t remDiskSpace;
    int64_t remDuration;
    IngestFormat remDurationIngestFormat;
    VTRState sourceVTRState;
    VTRState digibetaVTRState;
};



class Recorder : public CaptureListener, public VTRControlListener
{
public:
    static bool isValidAspectRatioCode(std::string aspectRatioCode);
    static Rational getRasterAspectRatio(std::string aspectRatioCode);

    
public:
    friend class RecordingSession;
    
public:
    Recorder(std::string name, std::string cacheDirectory, std::string browseDirectory, std::string pseDirectory,
        int replayPort, std::string vtrSerialDeviceName1, std::string vtrSerialDeviceName2);
    virtual ~Recorder();
    
    // from CaptureListener
    virtual void sdiStatusChanged(bool videoOK, bool audioOK, bool dltcOk, bool vitcOk, bool recordingOK);
    virtual void sdiCaptureDroppedFrame();
    virtual void mxfBufferOverflow();
    virtual void browseBufferOverflow();
    
    // from VTRControlListener
    virtual void vtrDeviceType(VTRControl* vtrControl, int deviceTypeCode, DeviceType deviceType);
    
    // returns 0 when successful; else see failure codes above
    int startNewSession(int profileId, Source* source, std::string digibetaBarcode, RecordingSession** session);
    bool haveSession();

    void startRecording();
    void stopRecording();
    void chunkFile();
    void completeSession(std::string comments);
    void abortSession(bool fromUser, std::string comments);

    void setSessionComments(std::string comments);

    SessionState getSessionState();
    SessionStatus getSessionStatus(const ConfidenceReplayStatus* replayStatus);
    RecordingItems* getSessionRecordingItems();
    bool getSessionComments(std::string* comments, int* count);
    
    bool confidenceReplayActive();
    void forwardConfidenceReplayControl(std::string command);
    ConfidenceReplayStatus getConfidenceReplayStatus();
    
    int replayFile(std::string filename);
    
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

    
    RecorderStatus getStatus();
    RecorderSystemStatus getSystemStatus();
    void getLastSessionResult(SessionResult* result, std::string* sourceSpoolNo, std::string* failureReason);
    
    int64_t getRemainingDiskSpace();
    
    Cache* getCache();
    ::Capture* getCapture();
    VTRControl* getSourceVTRControl();
    VTRControl* getBackupVTRControl();
    std::vector<VTRControl*> getVTRControls();
    
    RecorderTable* getRecorderTable();
    std::string getName();
    
    bool tapeBackupEnabled();
    
    bool isDigibetaBarcode(string barcode);
    
    bool isDigibetaSource(Source* source);
    
    bool checkMultiItemSupport(Source* source);
    
    bool updateSourceAspectRatios(Source* source, std::vector<string> aspectRatioCodes);
    bool checkSourceAspectRatios(Source* source);
    
    bool haveProfile(int id);
    bool getLastSessionProfileCopy(Profile* profileCopy);
    std::map<int, Profile> getProfileCopies();
    bool getProfileCopy(int id, Profile* profileCopy);
    
    bool updateProfile(const Profile *updatedProfileCopy);
    
private:
    void sessionDone(); // called by RecordingSession
    void checkSessionStatus();
    
    bool isSourceVTR(int deviceTypeCode);
    bool isBackupVTR(int deviceTypeCode);
    
private:
    Mutex _profileManagerMutex;
    ProfileManager *_profileManager;
    int _lastSessionProfileId;
    
    std::string _name;
    bool _videotapeBackup;
    std::vector<int> _sourceVTRDeviceTypeCodes;
    std::vector<int> _backupVTRDeviceTypeCodes;
    std::vector<std::string> _digibetaBarcodePrefixes;
    int _replayPort;

    Mutex _remDurationMutex;
    uint32_t _remDurationFactor;
    IngestFormat _remDurationIngestFormat;
    
    ::Capture* _capture;
    
    VTRControl* _vtrControl1;
    VTRControl* _vtrControl2;

    ReadWriteLock _sessionRWLock;
    bool _sessionDone;
    RecordingSession* _session;
    Thread* _sessionThread;

    // recording items is kept until the next session starts, which should be enough time
    // to avoid users accessing the data from a session after it gets deleted
    RecordingItems* _lastSessionRecordingItems;
    SessionResult _lastSessionResult;
    std::string _lastSessionSourceSpoolNo;
    std::string _lastSessionFailureReason;

    Cache* _cache;
    
    RecorderTable* _recorderTable;
    
    Mutex _replayMutex;
    ConfidenceReplay* _replay;
    
    JogShuttleControl* _jogShuttleControl;
};


};







#endif

