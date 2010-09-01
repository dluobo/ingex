/*
 * $Id: VTRControl.h,v 1.2 2010/09/01 16:05:23 philipn Exp $
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
 
#ifndef __RECORDER_VTR_CONTROL_H__
#define __RECORDER_VTR_CONTROL_H__


#include <vector>
#include <set>

#include "SonyProtocol.h"
#include "Threads.h"
#include "Types.h"


#define NUM_STATE_BYTES         4



namespace rec
{


typedef enum 
{
    NOT_CONNECTED_VTR_STATE = 0,    // no vtr connected
    REMOTE_LOCKOUT_VTR_STATE,       // vtr is not listening to remote calls
    TAPE_UNTHREADED_VTR_STATE,      // tape is unthreaded
    STOPPED_VTR_STATE,              // tape threaded but not in standby
    PAUSED_VTR_STATE,               // tape threaded in standby (paused)
    PLAY_VTR_STATE,                 // playing
    FAST_FORWARD_VTR_STATE,         // fast forward
    FAST_REWIND_VTR_STATE,          // fast rewind  
    EJECTING_VTR_STATE,             // tape is ejecting
    RECORDING_VTR_STATE,            // vtr is recording
    SEEKING_VTR_STATE,              // seeking
    JOG_VTR_STATE,                  // jog
    OTHER_VTR_STATE                 // TODO
} VTRState;

typedef enum
{
    UNKNOWN_DEVICE_TYPE = 0,            // don't know - have a look at the device code
    AJ_D350_525LINE_DEVICE_TYPE,        // TODO: D350 should be D360 for NTSC?
    AJ_D350_625LINE_DEVICE_TYPE,
    BVW_10_525LINE_DEVICE_TYPE,
    BVW_10_625LINE_DEVICE_TYPE,
    BVW_11_525LINE_DEVICE_TYPE,
    BVW_11_625LINE_DEVICE_TYPE,
    BVW_15_525LINE_DEVICE_TYPE,
    BVW_15_625LINE_DEVICE_TYPE,
    BVW_35_525LINE_DEVICE_TYPE,
    BVW_35_625LINE_DEVICE_TYPE,
    BVW_40_525LINE_DEVICE_TYPE,
    BVW_40_625LINE_DEVICE_TYPE,
    BVW_50_525LINE_DEVICE_TYPE,
    BVW_50_625LINE_DEVICE_TYPE,
    BVW_60_525LINE_DEVICE_TYPE,
    BVW_60_625LINE_DEVICE_TYPE,
    BVW_65_525LINE_DEVICE_TYPE,
    BVW_65_625LINE_DEVICE_TYPE,
    BVW_95_525LINE_DEVICE_TYPE,
    BVW_95_625LINE_DEVICE_TYPE,
    BVW_96_525LINE_DEVICE_TYPE,
    BVW_96_625LINE_DEVICE_TYPE,
    BVW_70_525LINE_DEVICE_TYPE,
    BVW_70_625LINE_DEVICE_TYPE,
    BVW_75_525LINE_DEVICE_TYPE,
    BVW_75_625LINE_DEVICE_TYPE,
    BVW_D75_525LINE_DEVICE_TYPE,
    BVW_D75_625LINE_DEVICE_TYPE,
    BVW_D265_DEVICE_TYPE,
    BVW_9000_525LINE_DEVICE_TYPE,
    BVW_9000_625LINE_DEVICE_TYPE,
    BVW_35PM_DEVICE_TYPE,
    BVW_65PM_DEVICE_TYPE,
    BVW_95PM_DEVICE_TYPE,
    BVW_75PM_DEVICE_TYPE,
    BVW_85P_DEVICE_TYPE,
    BVW_70S_DEVICE_TYPE,
    BVW_75S_DEVICE_TYPE,
    WBR_700_DEVICE_TYPE,
    DVW_A500_525LINE_DEVICE_TYPE,
    DVW_A500_625LINE_DEVICE_TYPE,
    DVW_A510_525LINE_DEVICE_TYPE,
    DVW_A510_625LINE_DEVICE_TYPE,
    DVW_CA510_525LINE_DEVICE_TYPE,
    DVW_CA510_625LINE_DEVICE_TYPE,
    DVW_500_525LINE_DEVICE_TYPE,
    DVW_500_625LINE_DEVICE_TYPE,
    DVW_510_525LINE_DEVICE_TYPE,
    DVW_510_625LINE_DEVICE_TYPE,
    DVW_250_525LINE_DEVICE_TYPE,
    DVW_250_625LINE_DEVICE_TYPE,
    DVW_2000_525LINE_DEVICE_TYPE,
    DVW_2000_625LINE_DEVICE_TYPE,
    DVW_M2000_525LINE_DEVICE_TYPE,
    DVW_M2000_625LINE_DEVICE_TYPE,
    DNW_30_525LINE_DEVICE_TYPE,
    DNW_30_625LINE_DEVICE_TYPE,
    DNW_A30_525LINE_DEVICE_TYPE,
    DNW_A30_625LINE_DEVICE_TYPE,
    DNW_A45_A50_525LINE_DEVICE_TYPE,
    DNW_A45_A50_625LINE_DEVICE_TYPE,
    DNW_65_525LINE_DEVICE_TYPE,
    DNW_65_625LINE_DEVICE_TYPE,
    DNW_A65_525LINE_DEVICE_TYPE,
    DNW_A65_625LINE_DEVICE_TYPE,
    DNW_75_525LINE_DEVICE_TYPE,
    DNW_75_625LINE_DEVICE_TYPE,
    DNW_A75_525LINE_DEVICE_TYPE,
    DNW_A75_625LINE_DEVICE_TYPE,
    DNW_A100_525LINE_DEVICE_TYPE,
    DNW_A100_625LINE_DEVICE_TYPE,
    DNW_A25_A25WS_525LINE_DEVICE_TYPE,
    DNW_A25_A25WS_625LINE_DEVICE_TYPE,
    DNW_A28_525LINE_DEVICE_TYPE,
    DNW_A28_625LINE_DEVICE_TYPE,
    DNW_A220_R_525LINE_DEVICE_TYPE,
    DNW_A220_R_625LINE_DEVICE_TYPE,
    DNW_A220_L_525LINE_DEVICE_TYPE,
    DNW_A220_L_625LINE_DEVICE_TYPE,
    MSW_2000_525LINE_DEVICE_TYPE,
    MSW_2000_625LINE_DEVICE_TYPE,
    MSW_A2000_525LINE_DEVICE_TYPE,
    MSW_A2000_625LINE_DEVICE_TYPE,
    MSW_M2000_M2000E_525LINE_DEVICE_TYPE,
    MSW_M2000_M2000E_625LINE_DEVICE_TYPE,
    MSW_M2100_M2100E_525LINE_DEVICE_TYPE,
    MSW_M2100_M2100E_625LINE_DEVICE_TYPE,
    HDW_F500_30FRAME_DEVICE_TYPE,
    HDW_F500_25FRAME_DEVICE_TYPE,
    HDW_F500_24FRAME_DEVICE_TYPE,
    HDW_250_DEVICE_TYPE,
    HDW_2000_D2000_M2000_S2000_5994HZ_DEVICE_TYPE,
    HDW_2000_D2000_M2000_S2000_50HZ_DEVICE_TYPE,
    HDW_A2100_M2100_5994HZ_DEVICE_TYPE,
    HDW_A2100_M2100_50HZ_DEVICE_TYPE,
    HDW_S280_30FRAME_DEVICE_TYPE,
    HDW_S280_25FRAME_DEVICE_TYPE,
    HDW_S280_24FRAME_DEVICE_TYPE,
    J1_J2_J3_J10_10SDI_30_30SDI_525LINE_DEVICE_TYPE,
    J1_J2_J3_J10_10SDI_30_30SDI_625LINE_DEVICE_TYPE,
    J_H3_30FRAME_DEVICE_TYPE,
    J_H3_25FRAME_DEVICE_TYPE,
    J_H3_24FRAME_DEVICE_TYPE,
    SRW_5000_30FRAME_DEVICE_TYPE,
    SRW_5000_25FRAME_DEVICE_TYPE,
    SRW_5000_24FRAME_DEVICE_TYPE,
    SRW_5500_30FRAME_DEVICE_TYPE,
    SRW_5500_25FRAME_DEVICE_TYPE,
    SRW_5500_24FRAME_DEVICE_TYPE,
} DeviceType;


class VTRControl;

class VTRControlListener
{
public:
    virtual ~VTRControlListener() {}
    
    virtual void vtrDeviceType(VTRControl* vtrControl, int deviceTypeCode, DeviceType deviceType) {}
    virtual void vtrState(VTRControl* vtrControl, VTRState state, const unsigned char* stateBytes) {};
    virtual void vtrPlaybackError(VTRControl* vtrControl, int errorCode, Timecode ltc, Timecode vitc) {};
    virtual void vtrVITC(VTRControl* vtrControl, Timecode vitc) {};
    virtual void vtrLTC(VTRControl* vtrControl, Timecode ltc) {};
};


class VTRControl
{
public:
    virtual ~VTRControl() {}
    
    // listeners
    virtual void registerListener(VTRControlListener* listener) = 0;
    virtual void unregisterListener(VTRControlListener* listener) = 0;

    virtual void pollExtState(bool poll) = 0;
    virtual void pollVitc(bool poll) = 0;
    virtual void pollLtc(bool poll) = 0;

    // device info
    virtual int getDeviceCode() = 0;
    virtual DeviceType getDeviceType() = 0;
    virtual bool isD3VTR() = 0;
    virtual bool isNonD3VTR() = 0;
    
    // control functions
    virtual bool play() = 0;
    virtual bool stop() = 0;
    virtual bool record() = 0;
    virtual bool fastForward() = 0;
    virtual bool fastRewind() = 0;
    virtual bool standbyOn() = 0;
    virtual bool standbyOff() = 0;
    virtual bool seekToTimecode(Timecode timecode) = 0;
    virtual bool eject() = 0;

    // state functions
    virtual VTRState getState() = 0;
    virtual void getDetailedState(VTRState* state, unsigned char* stateBytes) = 0;


    // utilities
    static std::string getStateString(VTRState state);
    static std::string errorCodeToString(int errorCode);
    static std::string getDeviceTypeString(DeviceType deviceType);
    static int deviceTypeToCode(DeviceType type);
    static std::string stateBytesToString(const unsigned char* stateBytes);
};



class VTRStatePoller;

class DefaultVTRControl : public VTRControl
{
public:
    friend class VTRStatePoller;
    
public:
    DefaultVTRControl(std::string deviceName, SerialType = TYPE_STD_TTY);
    ~DefaultVTRControl();

    // from VTRControl
    virtual void registerListener(VTRControlListener* listener);
    virtual void unregisterListener(VTRControlListener* listener);
    virtual void pollExtState(bool poll);
    virtual void pollVitc(bool poll);
    virtual void pollLtc(bool poll);
    virtual int getDeviceCode();
    virtual DeviceType getDeviceType();
    virtual bool isD3VTR();
    virtual bool isNonD3VTR();
    virtual bool play();
    virtual bool stop();
    virtual bool record();
    virtual bool fastForward();
    virtual bool fastRewind();
    virtual bool standbyOn();
    virtual bool standbyOff();
    virtual bool seekToTimecode(Timecode timecode);
    virtual bool eject();
    virtual VTRState getState();
    virtual void getDetailedState(VTRState* state, unsigned char* stateBytes);

    
private:
    bool openPort(std::string deviceName);
    bool reopenPort();
    bool portIsOpen();
    std::string getSerialPortName();

    void updateDeviceType(int deviceTypeCode, DeviceType deviceType);
    void updateState(VTRState state, const unsigned char* stateBytes);
    void updatePlaybackError(int errorCode, Timecode ltc, Timecode vitc);
    void updateVitc(Timecode vitc);
    void updateLtc(Timecode ltc);
    
    bool readDeviceType(int* deviceTypeCode);
    bool readState(VTRState* state, unsigned char* stateBytes);
    bool readExtState(int* errorCode);
    bool readTimecodes(Timecode* vitc, Timecode* ltc);
    bool readVitc(Timecode* vitc);
    bool readLtc(Timecode* ltc);

    void getStateFromData(const unsigned char* data, VTRState* state);
    void getExtStateFromData(const unsigned char* data, int* errorCode);
    void getTimecodeFromData(const unsigned char* data, Timecode* timecode);


    bool _debugSerialPortOpen;
    
    Thread* _stateThread;

    std::string _serialPortDeviceName;
    SerialType _serialPortDeviceType;
    std::string _actualSerialPortDeviceName;
    SonyProtocol* _protocol;
    Mutex _serialMutex;
    
    Mutex _listenerMutex;
    typedef struct
    {
        VTRControlListener* listener;
        int deviceTypeCode;
        DeviceType deviceType;
        VTRState state;
        int errorCode;
        Timecode ltcAtError;
        Timecode vitcAtError;
        Timecode vitc;
        Timecode ltc;
    } ListenerInfo;
    std::vector<ListenerInfo> _listeners;
    
    Mutex _stateMutex;
    VTRState _state;
    unsigned char _stateBytes[NUM_STATE_BYTES];
    
    Mutex _deviceTypeMutex;
    int _deviceTypeCode;
    DeviceType _deviceType;

private:
    static void addSerialPort(std::string deviceName);
    static void removeSerialPort(std::string deviceName);
    static bool serialPortIsAvailable(std::string deviceName);
    
    static std::set<std::string> _serialPortInUse;
    static Mutex _serialPortInUseMutex;
};






};




#endif


