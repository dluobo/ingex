/*
 * $Id: VTRControl.cpp,v 1.2 2010/09/01 16:05:23 philipn Exp $
 *
 * Used to control a VTR and monitor it's state
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
 
#include "VTRControl.h"
#include "RecorderException.h"
#include "Logging.h"
#include "Timing.h"
#include "Config.h"
#include "Utilities.h"


using namespace std;
using namespace rec;


// time between retrying to open the serial port
#define REOPEN_PORT_RETRY_SEC           2

// number of usecs before a failure to poll the VTR state is determined to be a connection failure
#define CONNECTION_FAIL_PERIOD          (1 * 1000 * 1000)

// number of usecs before a failure to poll the VTR state results in the port being reopened
// NOTE: REOPEN_PORT_FAIL_PERIOD must be > CONNECTION_FAIL_PERIOD
#define REOPEN_PORT_FAIL_PERIOD         (5 * 1000 * 1000)

// range of ports checked when the deviceName has the %d suffix; ttyUSB0..ttyUSB3, or ttyS0..ttyS3
#define MAX_PORT_DEVICE_NUMBER          4



typedef struct
{
    DeviceType type;
    int code;
    const char* name;
} VTRDeviceInfo;

// NOTE: this table is indexed using DeviceType and needs to be kept in sync
static const VTRDeviceInfo g_vtrDeviceInfo[] = 
{
    {UNKNOWN_DEVICE_TYPE,                       0x0000, "Unknown"},
    
    // Panasonic VTRs
    {AJ_D350_525LINE_DEVICE_TYPE,               0xF019, "AJ-D350 525 line"},
    {AJ_D350_625LINE_DEVICE_TYPE,               0xF119, "AJ-D350 625 line"},
    
    // Sony VTRs
    {BVW_10_525LINE_DEVICE_TYPE,                0x2000, "BVW-10 525 line"},
    {BVW_10_625LINE_DEVICE_TYPE,                0x2100, "BVW-10 625 line"},
    {BVW_11_525LINE_DEVICE_TYPE,                0x2002, "BVW-11 525 line"},
    {BVW_11_625LINE_DEVICE_TYPE,                0x2102, "BVW-11 625 line"},
    {BVW_15_525LINE_DEVICE_TYPE,                0x2003, "BVW-15 525 line"},
    {BVW_15_625LINE_DEVICE_TYPE,                0x2103, "BVW-15 625 line"},
    {BVW_35_525LINE_DEVICE_TYPE,                0x2010, "BVW-35 525 line"},
    {BVW_35_625LINE_DEVICE_TYPE,                0x2110, "BVW-35 625 line"},
    {BVW_40_525LINE_DEVICE_TYPE,                0x2001, "BVW-40 525 line"},
    {BVW_40_625LINE_DEVICE_TYPE,                0x2101, "BVW-40 625 line"},
    {BVW_50_525LINE_DEVICE_TYPE,                0x2030, "BVW-50 525 line"},
    {BVW_50_625LINE_DEVICE_TYPE,                0x2130, "BVW-50 625 line"},
    {BVW_60_525LINE_DEVICE_TYPE,                0x2020, "BVW-60 525 line"},
    {BVW_60_625LINE_DEVICE_TYPE,                0x2120, "BVW-60 625 line"},
    {BVW_65_525LINE_DEVICE_TYPE,                0x2021, "BVW-65 525 line"},
    {BVW_65_625LINE_DEVICE_TYPE,                0x2121, "BVW-65 625 line"},
    {BVW_95_525LINE_DEVICE_TYPE,                0x2022, "BVW-95 525 line"},
    {BVW_95_625LINE_DEVICE_TYPE,                0x2122, "BVW-95 625 line"},
    {BVW_96_525LINE_DEVICE_TYPE,                0x2023, "BVW-96 525 line"},
    {BVW_96_625LINE_DEVICE_TYPE,                0x2123, "BVW-96 625 line"},
    {BVW_70_525LINE_DEVICE_TYPE,                0x2024, "BVW-70 525 line"},
    {BVW_70_625LINE_DEVICE_TYPE,                0x2124, "BVW-70 625 line"},
    {BVW_75_525LINE_DEVICE_TYPE,                0x2025, "BVW-75 525 line"},
    {BVW_75_625LINE_DEVICE_TYPE,                0x2125, "BVW-75 625 line"},
    {BVW_D75_525LINE_DEVICE_TYPE,               0x2046, "BVW-D75 525 line"},
    {BVW_D75_625LINE_DEVICE_TYPE,               0x2146, "BVW-D75 625 line"},
    {BVW_D265_DEVICE_TYPE,                      0x2045, "BVW-D265"},
    {BVW_9000_525LINE_DEVICE_TYPE,              0x2047, "BVW-9000 525 line"},
    {BVW_9000_625LINE_DEVICE_TYPE,              0x2147, "BVW-9000 625 line"},
    {BVW_35PM_DEVICE_TYPE,                      0x2018, "BVW-35PM"},
    {BVW_65PM_DEVICE_TYPE,                      0x2029, "BVW-65PM"},
    {BVW_95PM_DEVICE_TYPE,                      0x202A, "BVW-95PM"},
    {BVW_75PM_DEVICE_TYPE,                      0x202D, "BVW-75PM"},
    {BVW_85P_DEVICE_TYPE,                       0x2126, "BVW-85P"},
    {BVW_70S_DEVICE_TYPE,                       0x212C, "BVW-70S"},
    {BVW_75S_DEVICE_TYPE,                       0x212D, "BVW-75S"},
    {WBR_700_DEVICE_TYPE,                       0x212F, "WBR-700"},
    {DVW_A500_525LINE_DEVICE_TYPE,              0xB000, "DVW-A500 525 line"},
    {DVW_A500_625LINE_DEVICE_TYPE,              0xB100, "DVW-A500 625 line"},
    {DVW_A510_525LINE_DEVICE_TYPE,              0xB001, "DVW-A510 525 line"},
    {DVW_A510_625LINE_DEVICE_TYPE,              0xB101, "DVW-A510 625 line"},
    {DVW_CA510_525LINE_DEVICE_TYPE,             0xB003, "DVW-CA510 525 line"},
    {DVW_CA510_625LINE_DEVICE_TYPE,             0xB103, "DVW-CA510 625 line"},
    {DVW_500_525LINE_DEVICE_TYPE,               0xB010, "DVW-500 525 line"},
    {DVW_500_625LINE_DEVICE_TYPE,               0xB110, "DVW-500 625 line"},
    {DVW_510_525LINE_DEVICE_TYPE,               0xB011, "DVW-510 525 line"},
    {DVW_510_625LINE_DEVICE_TYPE,               0xB111, "DVW-510 625 line"},
    {DVW_250_525LINE_DEVICE_TYPE,               0xB030, "DVW-250 525 line"},
    {DVW_250_625LINE_DEVICE_TYPE,               0xB130, "DVW-250 625 line"},
    {DVW_2000_525LINE_DEVICE_TYPE,              0xB014, "DVW-2000 525 line"},
    {DVW_2000_625LINE_DEVICE_TYPE,              0xB114, "DVW-2000 625 line"},
    {DVW_M2000_525LINE_DEVICE_TYPE,             0xB004, "DVW-M2000 525 line"},
    {DVW_M2000_625LINE_DEVICE_TYPE,             0xB104, "DVW-M2000 625 line"},
    {DNW_30_525LINE_DEVICE_TYPE,                0xB049, "DNW-30 525 line"},
    {DNW_30_625LINE_DEVICE_TYPE,                0xB149, "DNW-30 625 line"},
    {DNW_A30_525LINE_DEVICE_TYPE,               0xB048, "DNW-A30 525 line"},
    {DNW_A30_625LINE_DEVICE_TYPE,               0xB148, "DNW-A30 625 line"},
    {DNW_A45_A50_525LINE_DEVICE_TYPE,           0xB045, "DNW-A45-A50 525 line"},
    {DNW_A45_A50_625LINE_DEVICE_TYPE,           0xB145, "DNW-A45-A50 625 line"},
    {DNW_65_525LINE_DEVICE_TYPE,                0xB04F, "DNW-65 525 line"},
    {DNW_65_625LINE_DEVICE_TYPE,                0xB14F, "DNW-65 625 line"},
    {DNW_A65_525LINE_DEVICE_TYPE,               0xB047, "DNW-A65 525 line"},
    {DNW_A65_625LINE_DEVICE_TYPE,               0xB147, "DNW-A65 625 line"},
    {DNW_75_525LINE_DEVICE_TYPE,                0xB04E, "DNW-75 525 line"},
    {DNW_75_625LINE_DEVICE_TYPE,                0xB14E, "DNW-75 625 line"},
    {DNW_A75_525LINE_DEVICE_TYPE,               0xB046, "DNW-A75 525 line"},
    {DNW_A75_625LINE_DEVICE_TYPE,               0xB146, "DNW-A75 625 line"},
    {DNW_A100_525LINE_DEVICE_TYPE,              0xB041, "DNW-A100 525 line"},
    {DNW_A100_625LINE_DEVICE_TYPE,              0xB141, "DNW-A100 625 line"},
    {DNW_A25_A25WS_525LINE_DEVICE_TYPE,         0xB04B, "DNW-A25/A25WS 525 line"},
    {DNW_A25_A25WS_625LINE_DEVICE_TYPE,         0xB14B, "DNW-A25/A25WS 625 line"},
    {DNW_A28_525LINE_DEVICE_TYPE,               0xB04D, "DNW-A28 525 line"},
    {DNW_A28_625LINE_DEVICE_TYPE,               0xB14D, "DNW-A28 625 line"},
    {DNW_A220_R_525LINE_DEVICE_TYPE,            0xB04A, "DNW-A220/R 525 line"},
    {DNW_A220_R_625LINE_DEVICE_TYPE,            0xB14A, "DNW-A220/R 625 line"},
    {DNW_A220_L_525LINE_DEVICE_TYPE,            0xB04C, "DNW-A220/L 525 line"},
    {DNW_A220_L_625LINE_DEVICE_TYPE,            0xB14C, "DNW-A220/L 625 line"},
    {MSW_2000_525LINE_DEVICE_TYPE,              0xB062, "MSW-2000 525 line"},
    {MSW_2000_625LINE_DEVICE_TYPE,              0xB162, "MSW-2000 625 line"},
    {MSW_A2000_525LINE_DEVICE_TYPE,             0xB061, "MSW-A2000 525 line"},
    {MSW_A2000_625LINE_DEVICE_TYPE,             0xB161, "MSW-A2000 625 line"},
    {MSW_M2000_M2000E_525LINE_DEVICE_TYPE,      0xB060, "MSW-M2000/M2000E 525 line"},
    {MSW_M2000_M2000E_625LINE_DEVICE_TYPE,      0xB160, "MSW-M2000/M2000E 625 line"},
    {MSW_M2100_M2100E_525LINE_DEVICE_TYPE,      0xB063, "MSW-M2100/M2100E 525 line"},
    {MSW_M2100_M2100E_625LINE_DEVICE_TYPE,      0xB163, "MSW-M2100/M2100E 625 line"},
    {HDW_F500_30FRAME_DEVICE_TYPE,              0x20E0, "HDW-F500_30 frame"},
    {HDW_F500_25FRAME_DEVICE_TYPE,              0x21E0, "HDW-F500_25 frame"},
    {HDW_F500_24FRAME_DEVICE_TYPE,              0x22E0, "HDW-F500_24 frame"},
    {HDW_250_DEVICE_TYPE,                       0x20E1, "HDW-250"},
    {HDW_2000_D2000_M2000_S2000_5994HZ_DEVICE_TYPE,
                                                0x20E2, "HDW-2000/D2000/M2000/S2000 59.94Hz"},
    {HDW_2000_D2000_M2000_S2000_50HZ_DEVICE_TYPE,
                                                0x21E2, "HDW-2000/D2000/M2000/S2000 50Hz"},
    {HDW_A2100_M2100_5994HZ_DEVICE_TYPE,        0x20E3, "HDW-A2100/M2100 59.94Hz"},
    {HDW_A2100_M2100_50HZ_DEVICE_TYPE,          0x21E3, "HDW-A2100/M2100 50Hz"},
    {HDW_S280_30FRAME_DEVICE_TYPE,              0x20E5, "HDW-S280 30 frame"},
    {HDW_S280_25FRAME_DEVICE_TYPE,              0x21E5, "HDW-S280 25 frame"},
    {HDW_S280_24FRAME_DEVICE_TYPE,              0x22E5, "HDW-S280 24 frame"},
    {J1_J2_J3_J10_10SDI_30_30SDI_525LINE_DEVICE_TYPE,
                                                0xB070, "J-series 525 line"},
    {J1_J2_J3_J10_10SDI_30_30SDI_625LINE_DEVICE_TYPE,
                                                0xB170, "J-series 625 line"},
    {J_H3_30FRAME_DEVICE_TYPE,                  0x20E4, "J-H3 30 frame"},
    {J_H3_25FRAME_DEVICE_TYPE,                  0x21E4, "J-H3 25 frame"},
    {J_H3_24FRAME_DEVICE_TYPE,                  0x22E4, "J-H3 24 frame"},
    {SRW_5000_30FRAME_DEVICE_TYPE,              0x20A0, "SRW-5000 30 frame"},
    {SRW_5000_25FRAME_DEVICE_TYPE,              0x21A0, "SRW-5000 25 frame"},
    {SRW_5000_24FRAME_DEVICE_TYPE,              0x22A0, "SRW-5000 24 frame"},
    {SRW_5500_30FRAME_DEVICE_TYPE,              0x20A1, "SRW-5500 30 frame"},
    {SRW_5500_25FRAME_DEVICE_TYPE,              0x21A1, "SRW-5500 25 frame"},
    {SRW_5500_24FRAME_DEVICE_TYPE,              0x22A1, "SRW-5500 24 frame"},
};


// Note: Tape Trouble and Hard Error could be specific to the D3 VTR
static const char* g_vtrStateBitStrings[NUM_STATE_BYTES][8] = 
{
    {"X", "X", "Tape Unthread", "Servo Ref Missing", "?Tape Trouble", "?Hard Error", "X", "Local"},
    {"Standby", "X", "Stop", "Eject", "Rewind", "Fast Fwd", "Record", "Play"},
    {"Servo Lock", "TSO Mode", "Shuttle", "Jog", "Var", "Tape Dir", "Still", "Cue Up"},
    {"Auto Mode", "Freeze On", "X", "CF Mode", "A Out", "A In", "Out", "In"}
};



string VTRControl::getStateString(VTRState state)
{
    switch (state)
    {
        case NOT_CONNECTED_VTR_STATE:
            return "Not connected state";
        case REMOTE_LOCKOUT_VTR_STATE:
            return "Remote lockout state";
        case TAPE_UNTHREADED_VTR_STATE:
            return "Tape unthreaded state";
        case STOPPED_VTR_STATE:
            return "Stopped state";
        case PAUSED_VTR_STATE:
            return "Paused state";
        case PLAY_VTR_STATE:
            return "play state";
        case FAST_FORWARD_VTR_STATE:
            return "Fast forward state";
        case FAST_REWIND_VTR_STATE:
            return "Fast rewind state";
        case EJECTING_VTR_STATE:
            return "Ejecting state";
        case RECORDING_VTR_STATE:
            return "Recording state";
        case SEEKING_VTR_STATE:
            return "Seeking state";
        case JOG_VTR_STATE:
            return "Jog state";
        case OTHER_VTR_STATE:
        default:
            return "Unknown state";
    }
}

string VTRControl::errorCodeToString(int errorCode)
{
    int videoNibble = (errorCode & 0x00000007);
    int audioNibble = (errorCode & 0x00000070) >> 4;

    string result;

    switch (videoNibble)
    {
        case 0: 
            result = "Video good, ";
            break;
        case 1:
            result = "Video almost good, ";
            break;
        case 2:
            result = "Video status cannot be determined, ";
            break;
        case 3:
            result = "Video status unclear, ";
            break;
        default: // case 4
            result = "Video not good, ";
    }

    switch (audioNibble)
    {
        case 0: 
            result += "Audio good";
            break;
        case 1:
            result += "Audio almost good";
            break;
        case 2:
            result += "Audio status cannot be determined";
            break;
        case 3:
            result += "Audio status unclear";
            break;
        default: // case 4
            result += "Audio not good";
    }

    return result;
}

string VTRControl::getDeviceTypeString(DeviceType type)
{
    REC_ASSERT((size_t)type < sizeof(g_vtrDeviceInfo) / sizeof(VTRDeviceInfo));
    
    return g_vtrDeviceInfo[type].name;
}

int VTRControl::deviceTypeToCode(DeviceType type)
{
    REC_ASSERT((size_t)type < sizeof(g_vtrDeviceInfo) / sizeof(VTRDeviceInfo));
    
    return g_vtrDeviceInfo[type].code;
}

string VTRControl::stateBytesToString(const unsigned char* stateBytes)
{
    string result;
    
    char buf[32];
    sprintf(buf, "(%02x %02x %02x %02x) ", 
        stateBytes[0], stateBytes[1], stateBytes[2], stateBytes[3]);
    result = buf;
    
    unsigned char mask;
    int i, j;
    bool first = true;
    for (i = 0; i < 4; i++)
    {
        mask = 0x80;
        for (j = 0; j < 8; j++)
        {
            if (stateBytes[i] & mask)
            {
                if (!first)
                {
                    result += ",";
                }
                first = false;
                result += g_vtrStateBitStrings[i][j];
            }
            mask >>= 1;
        }
    }

    return result;
}


namespace rec
{
    
class VTRStatePoller : public ThreadWorker
{
public:
    VTRStatePoller(DefaultVTRControl* vtrControl)
    : _vtrControl(vtrControl), _hasStopped(true), _stop(false),
    _pollExtState(false), _pollVitc(false),
    _pollLtc(false), _minPollInterval(10)
    {}
    virtual ~VTRStatePoller()
    {}

    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped); 

        Timer pollFailureTimer;
        VTRState state;
        unsigned char stateBytes[NUM_STATE_BYTES];
        bool haveDeviceType = false;
        int deviceTypeCode;
        int errorCode;
        bool requireReopenPort = false;
        bool logConnection = true;
        
        while (!_stop)
        {
            // reopen the serial port if it isn't open
            if (!_vtrControl->portIsOpen() || requireReopenPort)
            {
                sleep_sec(REOPEN_PORT_RETRY_SEC);

                if (!_vtrControl->reopenPort())
                {
                    // try again
                    continue;
                }
                
                logConnection = true;
                requireReopenPort = false;
            }

            // poll state
            if (_vtrControl->readState(&state, stateBytes))
            {
                pollFailureTimer.reset();
                
                // get the device type before sending state updates
                if (!haveDeviceType)
                {
                    if (_vtrControl->readDeviceType(&deviceTypeCode))
                    {
                        haveDeviceType = true;
                        size_t i;
                        bool knownDeviceType = false;
                        for (i = 0; i < sizeof(g_vtrDeviceInfo) / sizeof(VTRDeviceInfo); i++)
                        {
                            if (deviceTypeCode == g_vtrDeviceInfo[i].code)
                            {
                                knownDeviceType = true;
                                _vtrControl->updateDeviceType(deviceTypeCode, g_vtrDeviceInfo[i].type);
                                break;
                            }
                        }
                        if (!knownDeviceType)
                        {
                            _vtrControl->updateDeviceType(deviceTypeCode, UNKNOWN_DEVICE_TYPE);
                        }
                    }
                }
                
                _vtrControl->updateState(state, stateBytes);

                
                if (logConnection)
                {
                    Logging::info("Connected to VTR '%s' via port '%s'\n", 
                        VTRControl::getDeviceTypeString(_vtrControl->getDeviceType()).c_str(),
                        _vtrControl->getSerialPortName().c_str());
                    logConnection = false;
                }
            }
            else
            {
                haveDeviceType = false; // force a re-check of the device type
                
                if (!pollFailureTimer.isRunning())
                {
                    // start the timer
                    pollFailureTimer.start();
                }
                else
                {
                    if (pollFailureTimer.elapsedTime() > REOPEN_PORT_FAIL_PERIOD)
                    {
                        // if state polling is still failing after REOPEN_PORT_FAIL_PERIOD then we 
                        // close the port and set state to "not connected"
                        
                        state = NOT_CONNECTED_VTR_STATE;
                        memset(stateBytes, 0, NUM_STATE_BYTES);
                        _vtrControl->updateState(state, stateBytes);
                        
                        requireReopenPort = true;
                    }
                    else if (pollFailureTimer.elapsedTime() > CONNECTION_FAIL_PERIOD)
                    {
                        // if state polling is still failing after CONNECTION_FAIL_PERIOD then we set 
                        // state to "not connected" 
                        
                        if (_vtrControl->getState() != NOT_CONNECTED_VTR_STATE)
                        {
                            Logging::warning("Connection lost to VTR '%s' via port '%s'\n", 
                                VTRControl::getDeviceTypeString(_vtrControl->getDeviceType()).c_str(),
                                _vtrControl->getSerialPortName().c_str());
                            logConnection = true;
                        }
                        state = NOT_CONNECTED_VTR_STATE;
                        memset(stateBytes, 0, NUM_STATE_BYTES);
                        _vtrControl->updateState(state, stateBytes);

                        // don't reset the timer because REOPEN_PORT_FAIL_PERIOD > CONNECTION_FAIL_PERIOD
                    }
                }
                
                // don't hog the CPU
                sleep_msec(2 * _minPollInterval);
                continue;
            }

            
            errorCode = 0;
            
            // poll extended state for VTR errors
            if (_pollExtState)
            {
                if (_vtrControl->readExtState(&errorCode))
                {
                    if (errorCode)
                    {
                        Timecode ltc, vitc;
                        _vtrControl->readLtc(&ltc);
                        _vtrControl->readVitc(&vitc);

                        _vtrControl->updateLtc(ltc);
                        _vtrControl->updateVitc(vitc);

                        _vtrControl->updatePlaybackError(errorCode, ltc, vitc);
                    }
                }
            }

            // poll timecodes
            if (_pollVitc && !errorCode) 
            {
                Timecode vitc;
                if (_vtrControl->readVitc(&vitc))
                {
                    _vtrControl->updateVitc(vitc);
                }
            }
            if (_pollLtc && !errorCode)
            {
                Timecode ltc;
                if (_vtrControl->readLtc(&ltc))
                {
                    _vtrControl->updateLtc(ltc);
                }
            }

            // sleep to give other threads a chance to operate the vtr
            sleep_msec(_minPollInterval);
        }

        _hasStopped = true;
    }
    
    virtual void stop()
    {
        _stop = true;
    }
    
    virtual bool hasStopped() const
    {
        return _hasStopped;
    }

    
    void pollExtState(bool poll)
    {
        _pollExtState = poll;
    }

    void pollVitc(bool poll)
    {
        _pollVitc = poll;
    }

    void pollLtc(bool poll)
    {
        _pollLtc = poll;
    }

    void minPollInterval(int interval)
    {
        _minPollInterval = interval;
    }
    
private:
    DefaultVTRControl* _vtrControl;
    bool _hasStopped;
    bool _stop;
    bool _pollExtState;
    bool _pollVitc;
    bool _pollLtc;
    int _minPollInterval;
};

}; // namespace rec





set<string> DefaultVTRControl::_serialPortInUse;
Mutex DefaultVTRControl::_serialPortInUseMutex;



void DefaultVTRControl::addSerialPort(string deviceName)
{
    if (!deviceName.empty())
    {
        _serialPortInUse.insert(deviceName);
    }
}

void DefaultVTRControl::removeSerialPort(string deviceName)
{
    _serialPortInUse.erase(deviceName);
}

bool DefaultVTRControl::serialPortIsAvailable(string deviceName)
{
    return _serialPortInUse.find(deviceName) == _serialPortInUse.end();
}



DefaultVTRControl::DefaultVTRControl(string deviceName, SerialType type)
: _debugSerialPortOpen(false), _stateThread(0), _serialPortDeviceName(deviceName), _serialPortDeviceType(type),
  _protocol(0), _state(NOT_CONNECTED_VTR_STATE), _deviceTypeCode(0), _deviceType(UNKNOWN_DEVICE_TYPE)
{
    memset(_stateBytes, 0, NUM_STATE_BYTES);

    _debugSerialPortOpen = Config::dbg_serial_port_open;
    
    // use reopen function to open the port 
    if (!reopenPort())
    {
        Logging::warning("Failed to open serial port '%s' at startup - will try again later\n", deviceName.c_str());
    }
    
    // create the state poll thread
    _stateThread = new Thread(new VTRStatePoller(this), true);
    _stateThread->start();
}

DefaultVTRControl::~DefaultVTRControl()
{
    delete _stateThread;
    
    delete _protocol;

    {    
        LOCK_SECTION(_serialPortInUseMutex);
        removeSerialPort(_actualSerialPortDeviceName);
    }
}

void DefaultVTRControl::registerListener(VTRControlListener* listener)
{
    LOCK_SECTION(_listenerMutex);

    ListenerInfo info;
    info.listener = listener;
    info.state = NOT_CONNECTED_VTR_STATE;
    info.errorCode = 0x00;
    info.ltcAtError.hour = 99;
    info.vitcAtError.hour = 99;
    info.vitc.hour = 99;
    info.ltc.hour = 99;
    
    // send device type straight away
    {
        LOCK_SECTION(_deviceTypeMutex);
        info.deviceTypeCode = _deviceTypeCode;
        info.deviceType = _deviceType;
    }
    listener->vtrDeviceType(this, info.deviceTypeCode, info.deviceType);

    _listeners.push_back(info);
}

void DefaultVTRControl::unregisterListener(VTRControlListener* listener)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<ListenerInfo>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if ((*iter).listener == listener)
        {
            _listeners.erase(iter);
            return;
        }
    }
}

void DefaultVTRControl::pollExtState(bool poll)
{
    VTRStatePoller* poller;
    poller = dynamic_cast<VTRStatePoller*>(_stateThread->getWorker());
    poller->pollExtState(poll);
}

void DefaultVTRControl::pollVitc(bool poll)
{
    VTRStatePoller* poller;
    poller = dynamic_cast<VTRStatePoller*>(_stateThread->getWorker());
    poller->pollVitc(poll);
}

void DefaultVTRControl::pollLtc(bool poll)
{
    VTRStatePoller*  poller;
    poller = dynamic_cast<VTRStatePoller*>(_stateThread->getWorker());
    poller->pollLtc(poll);
}

int DefaultVTRControl::getDeviceCode()
{
    return _deviceTypeCode;
}

DeviceType DefaultVTRControl::getDeviceType()
{
    return _deviceType;
}

bool DefaultVTRControl::isD3VTR()
{
    return _deviceType == AJ_D350_525LINE_DEVICE_TYPE || _deviceType == AJ_D350_625LINE_DEVICE_TYPE;
}

bool DefaultVTRControl::isNonD3VTR()
{
    return !isD3VTR() && _deviceType != 0x00;
}

bool DefaultVTRControl::play()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x01, "Play") == 0;
}

bool DefaultVTRControl::stop()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x00, "Stop") == 0;
}

bool DefaultVTRControl::record()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x02, "Record") == 0;
}

bool DefaultVTRControl::fastForward()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x10, "Fast Forward") == 0;
}

bool DefaultVTRControl::fastRewind()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x20, "Fast Rewind") == 0;
}

bool DefaultVTRControl::standbyOn()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x05, "Standby On") == 0;
}

bool DefaultVTRControl::standbyOff()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x04, "Standby Off") == 0;
}

bool DefaultVTRControl::seekToTimecode(Timecode timecode)
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    unsigned char data[4];
    data[0] = ((timecode.frame / 10) << 4) | (timecode.frame % 10);
    data[1] = ((timecode.sec / 10) << 4) | (timecode.sec % 10);
    data[2] = ((timecode.min / 10) << 4) | (timecode.min % 10);
    data[3] = ((timecode.hour / 10) << 4) | (timecode.hour % 10);
    
    CommandBlock request;
    request.setCMDs(0x20, 0x31);
    request.setData(data, 4);
 
    return _protocol->simpleCommand(&request, "Seek Timecode") == 0;
}

bool DefaultVTRControl::eject()
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    return _protocol->simpleCommand(0x20, 0x0F, "Eject") == 0;
}

VTRState DefaultVTRControl::getState()
{
    LOCK_SECTION(_stateMutex);
    
    return _state;
}

void DefaultVTRControl::getDetailedState(VTRState* state, unsigned char* stateBytes)
{
    LOCK_SECTION(_stateMutex);
    
    *state = _state;
    memcpy(stateBytes, _stateBytes, NUM_STATE_BYTES);
}


// caller must have lock on _serialMutex and _serialPortInUseMutex
bool DefaultVTRControl::openPort(string deviceName)
{
    if (!serialPortIsAvailable(deviceName))
    {
        return false;
    }
    
    try
    {
        if (_debugSerialPortOpen)
        {
            Logging::debug("Opening VTR serial port '%s'...\n", deviceName.c_str());
        }

        _protocol = SonyProtocol::open(deviceName, _serialPortDeviceType);
        if (_protocol == 0)
        {
            if (_debugSerialPortOpen)
            {
                Logging::debug("Failed to open serial port '%s'\n", deviceName.c_str());
            }
            return false;
        }
        
        _protocol->flush();

        _actualSerialPortDeviceName = deviceName;
        addSerialPort(_actualSerialPortDeviceName);

        if (_debugSerialPortOpen)
        {
            Logging::debug("Successfully opened VTR serial port '%s'\n", deviceName.c_str());
        }

        return true;
    }
    catch (...)
    {
        SAFE_DELETE(_protocol);
        return false;
    }
}

bool DefaultVTRControl::reopenPort()
{
    LOCK_SECTION(_serialMutex);
    LOCK_SECTION_2(_serialPortInUseMutex);
    
    // close port first
    string prevActualDeviceName = _actualSerialPortDeviceName;
    if (_protocol != 0)
    {
        removeSerialPort(_actualSerialPortDeviceName);
        _actualSerialPortDeviceName = "";
        SAFE_DELETE(_protocol);
    }

    if (_serialPortDeviceName.size() >= 2 && 
        _serialPortDeviceName.compare(_serialPortDeviceName.size() - 2, 2, "%d") == 0)
    {
        // try MAX_PORT_DEVICE_NUMBER devices, substituting %d with the number 0..MAX_PORT_DEVICE_NUMBER
        // try the previously opened port last
        // DVS ports are 1 to 4, all others are 0 to 3

        int i;
        string actualDeviceName;
        for (i = 0; i < MAX_PORT_DEVICE_NUMBER; i++)
        {
            char deviceNumberBase = _serialPortDeviceType == TYPE_DVS ? '1' : '0';
            actualDeviceName = _serialPortDeviceName.substr(0, _serialPortDeviceName.size() - 2);
            actualDeviceName.append(1, (char)(deviceNumberBase + i));
            if (actualDeviceName != prevActualDeviceName &&
                openPort(actualDeviceName))
            {
                return true;
            }
        }
 
        // try the previously opened port
        if (!prevActualDeviceName.empty())
        {
            return openPort(prevActualDeviceName);
        }
        
        return false;
    }
    
    return openPort(_serialPortDeviceName);
}

bool DefaultVTRControl::portIsOpen()
{
    return _protocol != 0;
}

string DefaultVTRControl::getSerialPortName()
{
    return _actualSerialPortDeviceName;
}

void DefaultVTRControl::updateDeviceType(int deviceTypeCode, DeviceType deviceType)
{
    {
        LOCK_SECTION(_deviceTypeMutex);
        _deviceTypeCode = deviceTypeCode;
        _deviceType = deviceType;
    }
    
    {
        LOCK_SECTION(_listenerMutex);
        
        vector<ListenerInfo>::iterator iter;
        for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
        {
            if ((*iter).deviceTypeCode != deviceTypeCode)
            {
                (*iter).listener->vtrDeviceType(this, deviceTypeCode, deviceType);
                (*iter).deviceTypeCode = deviceTypeCode;
                (*iter).deviceType = deviceType;
            }
        }
    }    
}

void DefaultVTRControl::updateState(VTRState state, const unsigned char* stateBytes)
{
    {
        LOCK_SECTION(_stateMutex);
        _state = state;
        memcpy(_stateBytes, stateBytes, NUM_STATE_BYTES);
    }
    
    {
        LOCK_SECTION(_listenerMutex);
        
        vector<ListenerInfo>::iterator iter;
        for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
        {
            if ((*iter).state != state)
            {
                (*iter).listener->vtrState(this, state, stateBytes);
                (*iter).state = state;
            }
        }
    }    
}

void DefaultVTRControl::updatePlaybackError(int errorCode, Timecode ltc, Timecode vitc)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<ListenerInfo>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if ((*iter).errorCode != errorCode ||
            (*iter).ltcAtError != ltc || (*iter).vitcAtError != vitc)
        {
            (*iter).listener->vtrPlaybackError(this, errorCode, ltc, vitc);
            (*iter).errorCode = errorCode;
            (*iter).ltcAtError = ltc;
            (*iter).vitcAtError = vitc;
        }
    }
}

void DefaultVTRControl::updateVitc(Timecode vitc)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<ListenerInfo>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if ((*iter).vitc != vitc)
        {
            (*iter).listener->vtrVITC(this, vitc);
            (*iter).vitc = vitc;
        }
    }
}

void DefaultVTRControl::updateLtc(Timecode ltc)
{
    LOCK_SECTION(_listenerMutex);
    
    vector<ListenerInfo>::iterator iter;
    for (iter = _listeners.begin(); iter != _listeners.end(); iter++)
    {
        if ((*iter).ltc != ltc)
        {
            (*iter).listener->vtrLTC(this, ltc);
            (*iter).ltc = ltc;
        }
    }
}

bool DefaultVTRControl::readDeviceType(int* deviceTypeCode)
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    CommandBlock request;
    CommandBlock response;
    
    // issue command
    request.setCMDs(0x00, 0x11);
    int result = _protocol->issueCommand(&request, &response, "Device Type");
    if (result != 0)
    {
        return false;
    }
    
    // check response
    if (response.getDataCount() != 2 || !response.checkCMDs(0x10, 0x11))
    {
        Logging::error("Invalid device type response from VTR\n");
        return false;
    }

    *deviceTypeCode = (((int)response.getData()[0]) << 8) | (int)response.getData()[1];
    return true;
}

bool DefaultVTRControl::readState(VTRState* state, unsigned char* stateBytes)
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    CommandBlock request;
    CommandBlock response;
    
    // issue command
    request.setCMDs(0x60, 0x20);
    const unsigned char requestData[] = {0x04};
    request.setData(requestData, sizeof(requestData));
    int result = _protocol->issueCommand(&request, &response, "State");
    if (result != 0)
    {
        return false;
    }
    
    // check response
    if (response.getDataCount() != NUM_STATE_BYTES || !response.checkCMDs(0x70, 0x20))
    {
        Logging::error("Invalid state response from VTR\n");
        return false;
    }

    getStateFromData(response.getData(), state);
    memcpy(stateBytes, response.getData(), NUM_STATE_BYTES);
    return true;
}

bool DefaultVTRControl::readExtState(int* errorCode)
{
    LOCK_SECTION(_serialMutex);

    *errorCode = 0;
    
    if (_protocol == 0)
    {
        return false;
    }

    CommandBlock request;
    CommandBlock response;
    
    // issue command
    request.setCMDs(0x60, 0x21);
    const unsigned char requestData[] = {0x07};
    request.setData(requestData, sizeof(requestData));
    int result = _protocol->issueCommand(&request, &response, "Ext State");
    if (result != 0)
    {
        return false;
    }
    
    // check response
    if (response.getDataCount() != 13 || !response.checkCMDs(0x70, 0x21))
    {
        Logging::error("Invalid extended state response from VTR\n");
        return false;
    }

    getExtStateFromData(response.getData(), errorCode);

    return true;
}

bool DefaultVTRControl::readTimecodes(Timecode* vitc, Timecode* ltc)
{
    if (!readVitc(vitc) || !readLtc(ltc))
    {
        return false;
    }

    return true;
}

bool DefaultVTRControl::readVitc(Timecode* vitc)
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    CommandBlock request;
    CommandBlock response;
    
    // issue command
    request.setCMDs(0x60, 0x0C);
    const unsigned char requestData[] = {0x02};
    request.setData(requestData, sizeof(requestData));
    int result = _protocol->issueCommand(&request, &response, "VITC");
    if (result != 0)
    {
        return false;
    }
    
    // check response
    if (response.getDataCount() != 4 || response.getCMD1() != 0x70)
    {
        Logging::error("Invalid VITC response from VTR\n");
        return false;
    }
    // CMD2: timecode type indicator: ltc=0x04, vitc=0x06, hold=0x16, corrected ltc=0x14

    getTimecodeFromData(response.getData(), vitc);
    return true;
}

bool DefaultVTRControl::readLtc(Timecode* ltc)
{
    LOCK_SECTION(_serialMutex);

    if (_protocol == 0)
    {
        return false;
    }

    CommandBlock request;
    CommandBlock response;
    
    // issue command
    request.setCMDs(0x60, 0x0C);
    const unsigned char requestData[] = {0x01};
    request.setData(requestData, sizeof(requestData));
    int result = _protocol->issueCommand(&request, &response, "LTC");
    if (result != 0)
    {
        return false;
    }
    
    // check response
    if (response.getDataCount() != 4 || response.getCMD1() != 0x70)
    {
        Logging::error("Invalid VITC response from VTR\n");
        return false;
    }
    // CMD2: timecode type indicator: ltc=0x04, vitc=0x06, hold=0x16, corrected ltc=0x14

    getTimecodeFromData(response.getData(), ltc);
    return true;
}

void DefaultVTRControl::getStateFromData(const unsigned char* data, VTRState* state)
{
    if (data[0] & 0x01)
    {
        *state = REMOTE_LOCKOUT_VTR_STATE;
        return;
    }

    if (data[0] & 0x20)
    {
        *state = TAPE_UNTHREADED_VTR_STATE;
        return;
    }

    // check before play state because 0x01 is also set when recording
    if (data[1] & 0x02)
    {
        *state = RECORDING_VTR_STATE;
        return;
    }

    if (data[1] & 0x01)
    {
        *state = PLAY_VTR_STATE;
        return;
    }

    if (data[1] & 0x04)
    {
        *state = FAST_FORWARD_VTR_STATE;
        return;
    }

    if (data[1] & 0x08)
    {
        *state = FAST_REWIND_VTR_STATE;
        return;
    }

    if (data[1] & 0x10)
    {
        *state = EJECTING_VTR_STATE;
        return;
    }

    // tape is stopped and standby is on = paused
    if ((data[1] & 0x20) && (data[1] & 0x80)) 
    {
        *state = PAUSED_VTR_STATE;
        return;
    }

    // standby is off = stopped
    if (!(data[1] & 0x80))
    {
        *state = STOPPED_VTR_STATE;
        return;
    }

    if (data[2] & 0x20)
    {
        *state = SEEKING_VTR_STATE;
        return;
    }

    if (data[2] & 0x10)
    {
        *state = JOG_VTR_STATE;
        return;
    }

    // TODO
    *state = OTHER_VTR_STATE;
}

void DefaultVTRControl::getExtStateFromData(const unsigned char* data, int* errorCode)
{
    // The Sony 9-pin protocol specification states that bit 3 and bit 7 indicate the validity of the error level
    // The validity bit is set to 1 when the error level is a valid value
    // However, the D3 protocol specification has this bit undefined and the VTR doesn't set this bit to 1
    
    int mask = 0x77;
    if (_deviceType != AJ_D350_625LINE_DEVICE_TYPE && _deviceType != AJ_D350_525LINE_DEVICE_TYPE)
    {
        if (!(data[8] & 0x80))
        {
            mask &= 0x0f;
        }
        if (!(data[8] & 0x08))
        {
            mask &= 0xf0;
        }
    }
        
    *errorCode = data[8] & mask;
}

void DefaultVTRControl::getTimecodeFromData(const unsigned char* data, Timecode* timecode)
{
    timecode->frame = ((data[0] & 0x30) >> 4) * 10 + (data[0] & 0x0F);
    timecode->sec   = ((data[1] & 0x70) >> 4) * 10 + (data[1] & 0x0F);
    timecode->min   = ((data[2] & 0x70) >> 4) * 10 + (data[2] & 0x0F);
    timecode->hour  = ((data[3] & 0x30) >> 4) * 10 + (data[3] & 0x0F);
}


