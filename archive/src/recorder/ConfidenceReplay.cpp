/*
 * $Id: ConfidenceReplay.cpp,v 1.2 2010/09/01 16:05:22 philipn Exp $
 *
 * Runs and provides access to a player running in a separate process
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
    The player is run in a separate process. Player control and getting player
    state information is done using HTTP requests.
*/
 
#define __STDC_FORMAT_MACROS 1

#include <cstdlib>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>
#include <sys/wait.h>

#include <curl/curl.h>

#include "ConfidenceReplay.h"
#include "PlayerState.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Config.h"
#include "Timing.h"



using namespace std;
using namespace rec;


#define COMMANDS_BUFFER_SIZE                20

// maximum data size that will be accepted for the player state http response
#define MAX_PLAYER_STATE_RESPONSE_SIZE      4096

// poll the player state every 1/5 second
#define PLAYER_STATE_POLL_INTERVAL          (200 * MSEC_IN_USEC)

// player mark types
#define ALL_MARK_TYPE               0xffffffff
#define VTR_ERROR_MARK_TYPE         0x00010000
#define PSE_FAILURE_MARK_TYPE       0x00020000
#define DIGIBETA_DROPOUT_MARK_TYPE  0x00040000
#define TIMECODE_BREAK_MARK_TYPE    0x00080000


// TODO: make ConfidenceReplay a singelton because of these global variables

static pid_t g_childProcessPid = 0;
static int g_signalForChild = 0;
static bool g_signalledChild = false;

static void signal_child(int signum)
{
    g_signalledChild = true;
    g_signalForChild = signum;
    kill(g_childProcessPid, signum);
}



// Utility class to set and ensure the signals are returned to their original state

#define GUARD_SIGNAL(idx, signum, handler)      SignalGuard __signalGuard##idx(signum, handler);
#define GUARD_SIGNAL_BLOCK(idx, signum)         SignalGuard __signalGuardBlock##idx(signum);

class SignalGuard
{
public:
    SignalGuard(int signum)
    : _signum(signum), _isSet(false), _isBlocked(false)
    {
        // block the signal
        sigset_t blockedSet;
        sigemptyset(&blockedSet);
        sigaddset(&blockedSet, signum);
        if (sigprocmask(SIG_BLOCK, &blockedSet, &_originalBlockedSet) < 0)
        {
            Logging::error("Failed to block signal: %s\n", strerror(errno));
        }
        else
        {
            _isBlocked = true;
        }
    }
    
    SignalGuard(int signum, sighandler_t handler)
    : _signum(signum), _isSet(false), _isBlocked(false)
    {
        // install new handler
        if ((_originalHandler = signal(signum, handler)) == SIG_ERR)
        {
            Logging::error("Failed to install signal handler: %s\n", strerror(errno));
        }
        else
        {
            _isSet = true;
        }
    }
    
    ~SignalGuard()
    {
        if (_isSet)
        {
            if (signal(_signum, _originalHandler) == SIG_ERR)
            {
                Logging::error("Failed to restore original signal handler: %s\n", strerror(errno));
            }
        }
        else if (_isBlocked)
        {
            if (sigprocmask(SIG_BLOCK, &_originalBlockedSet, NULL) < 0)
            {
                Logging::error("Failed to un-block signal: %s\n", strerror(errno));
            }
        }
    }
    
private:
    int _signum;
    bool _isSet;
    bool _isBlocked;
    sighandler_t _originalHandler;
    sigset_t _originalBlockedSet;
};
    

static string int_to_hex_string(int value)
{
    char buffer[32];
    sprintf(buffer, "0x%08x", value);
    return buffer;
}


class ConfidenceReplayWorker : public ThreadWorker
{
public:    
    ConfidenceReplayWorker(string filename, string eventFilename,
                           int httpAccessPort, int64_t startPosition, bool showSecondMarkBar)
    : _hasStopped(false), _filename(filename), _eventFilename(eventFilename), _httpAccessPort(httpAccessPort),
    _startPosition(startPosition), _showSecondMarkBar(showSecondMarkBar), _playerPid(-1)
    {
        _playerExe = Config::player_exe;
        _dbgVGAReplay = Config::dbg_vga_replay;
        _dbgForceX11 = Config::dbg_force_x11;
        _srcBufferSize = Config::player_source_buffer_size;
        
        _curl = curl_easy_init();
        if (!_curl)
        {
            REC_LOGTHROW(("Failed to initialise curl easy"));
        }
    }
        
    virtual ~ConfidenceReplayWorker()
    {
        curl_easy_cleanup(_curl);
    }
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        // check whether the event filename exists
        string existingEventFilename;
        if (file_exists(_eventFilename))
        {
            existingEventFilename = _eventFilename;
        }
        
        _playerPid = fork();
        if (_playerPid < 0)
        {
            Logging::error("Failed to fork for confidence replay: %s\n", strerror(errno));
            _playerPid = -1;
            return;
        }
        
        if (_playerPid == 0) // child
        {
            vector<string> args;
            args.push_back(_playerExe);  // the executable name
            args.push_back("-m"); // input MXF file
                args.push_back(_filename);
            args.push_back("--audio-mon"); // show audio levels for maximum 8 audio streams
                args.push_back("8");
            args.push_back("--show-tc"); // show VITC
                args.push_back("VITC.0");
            args.push_back("--mark-vtr-errors"); // mark VTR errors on the progress bar
            args.push_back("--vtr-error-level"); // initially show all VTR errors
                args.push_back("1");
            args.push_back("--mark-digi-dropouts"); // mark digibeta dropouts on the progress bar
            args.push_back("--mark-tc-breaks"); // mark timecode breaks on the progress bar
            args.push_back("--config-marks"); // show item start, vtr error and digibeta dropout mark types
                args.push_back("9,itemstart,red"
                               ":17,vtrerror,yellow"
                               ":19,digidropout,light-grey"
                               ":20,tcbreak,green");
            args.push_back("--vitc-read"); // read VITC from lines 19 and 21
                args.push_back("19,21");
            args.push_back("--qc-control"); // use the QC control input mapping
            args.push_back("--disable-console-mon"); // disable console monitoring of the player
            args.push_back("--log-file");  // no logging
                args.push_back("/dev/null");
            args.push_back("--http-access");  // port for sending control commands
                args.push_back(int_to_string(_httpAccessPort));
            args.push_back("--disable-shuttle");  // recorder uses the jog-shuttle control
            args.push_back("--start");  // start playing from this position
                args.push_back(int64_to_string(_startPosition));
            
            if (_srcBufferSize > 0)
            {
                args.push_back("--src-buf");  // source buffer
                    args.push_back(int_to_string(_srcBufferSize));
            }

            if (_showSecondMarkBar)
            {
                // mask out VTR, DigiBeta dropout and timecode break marks on the first progress bar
                // mask out everything but VTR, DigiBeta dropout and timecode break marks on the second progress bar
                args.push_back("--mark-filter");
                    args.push_back(int_to_hex_string(ALL_MARK_TYPE &
                                                    ~VTR_ERROR_MARK_TYPE &
                                                    ~DIGIBETA_DROPOUT_MARK_TYPE &
                                                    ~TIMECODE_BREAK_MARK_TYPE));
                args.push_back("--mark-filter");
                    args.push_back(int_to_hex_string(VTR_ERROR_MARK_TYPE |
                                                     DIGIBETA_DROPOUT_MARK_TYPE |
                                                     TIMECODE_BREAK_MARK_TYPE));
                args.push_back("--pb-mark-mask");
                    args.push_back(int_to_hex_string(ALL_MARK_TYPE &
                                                    ~VTR_ERROR_MARK_TYPE &
                                                    ~DIGIBETA_DROPOUT_MARK_TYPE &
                                                    ~TIMECODE_BREAK_MARK_TYPE));
                args.push_back("--pb-mark-mask");
                    args.push_back(int_to_hex_string(VTR_ERROR_MARK_TYPE |
                                                     DIGIBETA_DROPOUT_MARK_TYPE |
                                                     TIMECODE_BREAK_MARK_TYPE));
            }
            else
            {
                // mask out everything but VTR, DigiBeta dropout and timecode break marks
                args.push_back("--mark-filter");
                    args.push_back(int_to_hex_string(VTR_ERROR_MARK_TYPE |
                                                     DIGIBETA_DROPOUT_MARK_TYPE |
                                                     TIMECODE_BREAK_MARK_TYPE));
            }
            
            if (_dbgVGAReplay)  // output to the VGA display
            {
                if (_dbgForceX11)
                {
                    args.push_back("--x11"); // force use of X11 player sink
                }
            }
            else
            {
                args.push_back("--dvs"); // output to SDI
            }
            
            // add event file
            if (!existingEventFilename.empty())
            {
                args.push_back("-m");
                    args.push_back(existingEventFilename);
            }
            
            const char* c_args[64];
            size_t i;
            for (i = 0; i < args.size(); i++)
            {
                c_args[i] = args[i].c_str();
            }
            c_args[i] = NULL;
            
            execv(_playerExe.c_str(), const_cast<char* const*>(c_args));

            Logging::error("Failed to execute player command for confidence replay: %s\n", strerror(errno));
            exit(1);
        }
        else // parent
        {
            int childResult;
            int childStatus;
            { // scope where signals handling is modified
                
                // globals used by signal_child function and later on
                g_childProcessPid = _playerPid;
                g_signalledChild = false;
                g_signalForChild = 0;
                
                // send signals through to child and handle through child return status
                GUARD_SIGNAL(1, SIGINT, signal_child);
                GUARD_SIGNAL(2, SIGTERM, signal_child);
                GUARD_SIGNAL(3, SIGQUIT, signal_child);
                GUARD_SIGNAL(4, SIGHUP, signal_child);
                
                // block sigchild because it is handled through child return status
                GUARD_SIGNAL_BLOCK(1, SIGCHLD);
            
                // wait for child to exit
                Timer timer;
                while ((childResult = waitpid(_playerPid, &childStatus, WNOHANG)) == 0)
                {
                    string command;
                    {
                        LOCK_SECTION(_commandMutex);
                        
                        if (_commands.size() > 0)
                        {
                            command = _commands.back();
                            _commands.pop_back();
                        }
                    }
                    
                    if (command.size() > 0)
                    {
                        sendCommand(command);
                    }
                    else
                    {
                        // don't hog the CPU - sleep at least 500usec
                        sleep_usec(500);
                        timer.sleepRemainder();
                    }
                    
                    // check for commands or child exit every 5 msec
                    timer.start(5 * MSEC_IN_USEC);
                }
            }

            _playerPid = -1;
            
            if (childResult < 0) // error
            {
                Logging::error("Failed to wait for confidence replay child process: %s\n", strerror(errno));
            }
            else // child returned status
            {
                Logging::debug("Confidence replay child process returned status %d\n", childStatus);
                
                // if a signal was sent to the child through the signal handlers then send it to this process
                if (g_signalledChild)
                {
                    Logging::info("Termination signal received through confidence replay child\n");
                    kill(0, g_signalForChild);
                }
            }
        }
        
    }
    
    virtual void stop()
    {
        if (_playerPid > 0)
        {
            Logging::info("Stopping the confidence replay process\n");
            
            sendCommand("stop");
            
            // wait maximum 2 seconds for the player to stop
            int count = 200;
            while (!_hasStopped && count > 0)
            {
                sleep_msec(10);
                count--;
            }
            
            // if not stopped then terminate using a signal
            if (!_hasStopped)
            {
                Logging::warning("Failed to stop the confidence replay process - sending SIGKILL signal\n");
                
                if (kill(_playerPid, SIGKILL) < 0)
                {
                    Logging::error("Failed to kill (SIGKILL) child process running player: %s\n", strerror(errno));
                }
            }
        }
    }
    
    virtual bool hasStopped() const
    {
        return _hasStopped;
    }
    
    
    void forwardControl(string command)
    {
        LOCK_SECTION(_commandMutex);
        
        if (_commands.size() >= COMMANDS_BUFFER_SIZE)
        {
            // ignore the request
            return;
        }
        
        if (command.find("next-show-marks") == 0)
        {
            // modify next-show-marks command to select the progress bar with the VTR errors and DigiBeta dropouts
            if (_showSecondMarkBar)
            {
                _commands.push_back("next-show-marks?sel=1");
            }
            else
            {
                _commands.push_back("next-show-marks?sel=0");
            }
        }
        else
        {
            _commands.push_back(command);
        }
    }
    
    void sendCommand(string command)
    {
        // send command through to the player
        
        string url;
        url = "http://localhost:" + int_to_string(_httpAccessPort) + 
            "/player/control/" + command;
        
        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 1);
        curl_easy_setopt(_curl, CURLOPT_ERRORBUFFER, errorBuffer);
        
        curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());
        CURLcode result = curl_easy_perform(_curl);
        if (result != 0)
        {
            Logging::warning("Failed to send replay command '%s': %s\n", url.c_str(), errorBuffer);
        }
    }
    
private:
    string _playerExe;
    bool _dbgVGAReplay;
    bool _dbgForceX11;
    int _srcBufferSize;
    
    bool _hasStopped;
    string _filename;
    string _eventFilename;
    int _httpAccessPort;
    int64_t _startPosition;
    bool _showSecondMarkBar;
    pid_t _playerPid;
    
    vector<string> _commands;
    Mutex _commandMutex;
    
    CURL* _curl;
};



class StateMonitorWorker : public ThreadWorker
{
public:    
    StateMonitorWorker( int httpAccessPort)
    : _stop(false), _hasStopped(false), _httpAccessPort(httpAccessPort),
    _duration(-1), _position(-1), _vtrErrorLevel(-1), _markFilter(-1)
    {
        _curl = curl_easy_init();
        if (!_curl)
        {
            REC_LOGTHROW(("Failed to initialise curl easy"));
        }
    }
        
    virtual ~StateMonitorWorker()
    {
        curl_easy_cleanup(_curl);
    }
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        Timer timer;

        while (!_stop)
        {
            timer.start(PLAYER_STATE_POLL_INTERVAL);
            
            getPlayerState();
            
            timer.sleepRemainder();
        }
    }
    
    virtual void stop()
    {
        _stop = true;
    }
    
    virtual bool hasStopped() const
    {
        return _hasStopped;
    }
    

    void getStatus(ConfidenceReplayStatus* status)
    {
        LOCK_SECTION(_stateMutex);
        
        status->duration = _duration;
        status->position = _position;
        status->vtrErrorLevel = _vtrErrorLevel;
        status->markFilter = _markFilter;
    }
    

private:    
    static size_t responseHandler(void* ptr, size_t size, size_t nmemb, void* stream)
    {
        StateMonitorWorker* monitorWorker = static_cast<StateMonitorWorker*>(stream);
        
        if (monitorWorker->_stateResponse.size() <= MAX_PLAYER_STATE_RESPONSE_SIZE)
        {
            monitorWorker->_stateResponse.append(static_cast<char*>(ptr), size * nmemb);
        }
        
        return size * nmemb;
    }
    
    void getPlayerState()
    {
        string url;
        url = "http://localhost:" + int_to_string(_httpAccessPort) + 
            "/player/state.txt";
        
        char errorBuffer[CURL_ERROR_SIZE];
        curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 1);
        curl_easy_setopt(_curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, responseHandler);
        curl_easy_setopt(_curl, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());
        
        _stateResponse.clear();
        _stateResponse.reserve(512);
        
        CURLcode result = curl_easy_perform(_curl);
        if (result != 0)
        {
            // assume we failed to get player state because the player has not yet started or
            // has been closed
            return;
        }

        
        if (!_playerState.parse(_stateResponse))
        {
            Logging::warning("Failed to parse player state http response\n");
            return;
        }
        
        int64_t position;
        int64_t startOffset;
        int64_t length;
        int vtrErrorLevel;
        int numMarkSelections;
        unsigned int markFilter;
        
        if (!_playerState.getInt64("length", &length))
        {
            Logging::warning("Failed to parse 'length' from player state\n");
            return;
        }
        if (!_playerState.getInt64("startOffset", &startOffset))
        {
            Logging::warning("Failed to parse 'startOffset' from player state\n");
            return;
        }
        if (!_playerState.getInt64("position", &position))
        {
            Logging::warning("Failed to parse 'position' from player state\n");
            return;
        }
        if (!_playerState.getInt("vtrErrorLevel", &vtrErrorLevel))
        {
            Logging::warning("Failed to parse 'vtrErrorLevel' from player state\n");
            return;
        }

        // the VTR error and DigiBeta dropouts are on the second progress bar if numMarkSelections == 2, else
        // it is on the first
        if (!_playerState.getInt("numMarkSelections", &numMarkSelections))
        {
            Logging::warning("Failed to parse 'numMarkSelections' from player state\n");
            return;
        }
        if (numMarkSelections == 0)
        {
            // player is still starting up
            markFilter = (unsigned int)(-1);
        }
        else if (numMarkSelections == 2)
        {
            if (!_playerState.getUInt("markFilter_1", &markFilter))
            {
                Logging::warning("Failed to parse 'markFilter_1' from player state\n");
                return;
            }
        }
        else
        {
            if (!_playerState.getUInt("markFilter_0", &markFilter))
            {
                Logging::warning("Failed to parse 'markFilter_0' from player state\n");
                return;
            }
        }

        
        {
            LOCK_SECTION(_stateMutex);
            
            _duration = length;
            _position = startOffset + position;
            _vtrErrorLevel = vtrErrorLevel;
            _markFilter = markFilter;
        }
    }

    
    bool _stop;
    bool _hasStopped;
    int _httpAccessPort;
    
    Mutex _stateMutex;
    string _stateResponse;
    PlayerState _playerState;
    int64_t _duration;
    int64_t _position;
    int _vtrErrorLevel;
    unsigned int _markFilter;
    
    CURL* _curl;
};



ConfidenceReplay::ConfidenceReplay(string filename, string eventFilename,
                                   int httpAccessPort, int64_t startPosition, bool showSecondMarkBar)
: Thread(new ConfidenceReplayWorker(filename, eventFilename, httpAccessPort, startPosition, showSecondMarkBar), true), _filename(filename), _stateMonitor(0)
{
    _stateMonitor = new Thread(new StateMonitorWorker(httpAccessPort), true);
    _stateMonitor->start();
}

ConfidenceReplay::~ConfidenceReplay()
{
    stop();
    
    delete _stateMonitor;
}

void ConfidenceReplay::forwardControl(string command)
{
    if (!_worker->hasStopped())
    {
        ConfidenceReplayWorker* confReplayWorker = dynamic_cast<ConfidenceReplayWorker*>(_worker);
        confReplayWorker->forwardControl(command);
    }
}
    
void ConfidenceReplay::setMark(int64_t position, int type)
{
    char buffer[128];
    sprintf(buffer, "mark-position?position=%"PRId64"&type=%d&toggle=false", position, type);
    
    forwardControl(buffer);
}

void ConfidenceReplay::clearMark(int64_t position, int type)
{
    char buffer[128];
    sprintf(buffer, "clear-mark-position?position=%"PRId64"&type=%d", position, type);
    
    forwardControl(buffer);
}

void ConfidenceReplay::seek(int64_t position)
{
    char buffer[128];
    sprintf(buffer, "seek?offset=%"PRId64"&whence=SEEK_SET&unit=FRAME_PLAY_UNIT&pause=true", position);
    
    forwardControl(buffer);
}

void ConfidenceReplay::play()
{
    forwardControl("play");
}

void ConfidenceReplay::seekPositionOrPlay(int64_t position)
{
    if (getStatus().position == position)
    {
        play();
    }
    else
    {
        seek(position);
    }
}

ConfidenceReplayStatus ConfidenceReplay::getStatus()
{
    ConfidenceReplayStatus status;
    StateMonitorWorker* monitorWorker = dynamic_cast<StateMonitorWorker*>(_stateMonitor->getWorker());
    
    status.filename = _filename;
    monitorWorker->getStatus(&status);
    
    return status;
}

