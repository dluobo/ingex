/*
 * $Id: SerialPort.cpp,v 1.1 2008/04/18 16:03:29 john_f Exp $
 *
 * Serial communication port.
 *
 * Copyright (C) 2008  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "SerialPort.h"

bool SerialPort::Connect(const std::string & port)
{
    return (-1 != mDeviceConnector.connect(mSerialDevice, ACE_DEV_Addr(ACE_TEXT(port.c_str()))));
}

void SerialPort::BaudRate(int baud_rate)
{
	ACE_TTY_IO::Serial_Params myparams;
	myparams.baudrate = baud_rate;
	myparams.parityenb = false;
	myparams.databits = 8;
	myparams.stopbits = 1;
	myparams.readtimeoutmsec = 100;
	myparams.ctsenb = 0;
	myparams.rcvenb = true;
	mSerialDevice.control(ACE_TTY_IO::SETPARAMS, &myparams);
}

int SerialPort::Send(const void * buf, size_t n, const ACE_Time_Value * timeout)
{
    // NB. ACE_DEV_IO coes not support timeout
    return mSerialDevice.send(buf, n);
}

int SerialPort::Recv(void * buf, size_t n, const ACE_Time_Value * timeout)
{
    // NB. ACE_DEV_IO coes not support timeout
    return mSerialDevice.recv(buf, n);
}

SerialPort::~SerialPort()
{
}