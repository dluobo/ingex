/*
 * $Id: SerialPort.cpp,v 1.2 2010/09/01 16:05:23 philipn Exp $
 *
 * Provides serial port reading and writing
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
 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <cerrno>
#include <ctime>
#include <cstring>

#include "SerialPort.h"
#include "RecorderException.h"
#include "Logging.h"
#include "Config.h"


using namespace std;
using namespace rec;



SerialPort* SerialPort::open(string deviceName, BaudRate baudRate, CharacterSize charSize,
        StopBits stopBits, Parity parity, SerialType type)
{
    int fd = -1;
    sv_handle * sv = NULL;

    switch (type)
    {
    case TYPE_STD_TTY:

        // open the serial device
        fd = ::open(deviceName.c_str(), O_RDWR | O_NONBLOCK | O_NOCTTY);

        if (fd < 0)
        {
            if (Config::dbg_serial_port_open)
            {
                Logging::debug("Failed to open serial device '%s': %s\n", deviceName.c_str(), strerror(errno));
            }
            return 0;
        }
        else
        {
            // set the new serial settings
            struct termios newTermios;
            bzero(&newTermios, sizeof(newTermios));

            // non-canonical read mode
            newTermios.c_lflag = 0;

            // ignore modem control lines and enable the receiver
            newTermios.c_cflag |= CLOCAL | CREAD;

            // baud rate
            newTermios.c_cflag |= baudRate;

            // character size
            newTermios.c_cflag |= charSize;

            // stop bits
            switch (stopBits)
            {
                case STOP_BITS_1:
                    // nothing
                    break;
                case STOP_BITS_2:
                    newTermios.c_cflag |= CSTOPB;
                    break;
            }

            // parity
            switch (parity)
            {
                case PARITY_EVEN:
                    newTermios.c_cflag |= PARENB;
                    newTermios.c_iflag |= INPCK;
                    break;
                case PARITY_ODD:
                    newTermios.c_cflag |= PARENB | PARODD;
                    newTermios.c_iflag |= INPCK;
                    break;
                case PARITY_NONE:
                    newTermios.c_iflag |= IGNPAR;
                    break;
            }

            // read returns immediately with available characters
            newTermios.c_cc[VMIN] = 0;
            newTermios.c_cc[VTIME] = 0;

            // flush input and output buffers
            tcflush(fd, TCIOFLUSH);

            // set new
            if (tcsetattr(fd, TCSANOW, &newTermios) < 0)
            {
                Logging::error("Failed to set new serial settings for '%s': %s\n", deviceName.c_str(), strerror(errno));
                close(fd);
                return 0;
            }
            else
            {
                // check settings could be completed
                struct termios setTermios;
                tcgetattr(fd, &setTermios);
                if (memcmp(&newTermios.c_iflag, &setTermios.c_iflag, sizeof(newTermios.c_iflag)) != 0 ||
                    memcmp(&newTermios.c_oflag, &setTermios.c_oflag, sizeof(newTermios.c_oflag)) != 0 ||
                    memcmp(&newTermios.c_cflag, &setTermios.c_cflag, sizeof(newTermios.c_cflag)) != 0 ||
                    memcmp(&newTermios.c_lflag, &setTermios.c_lflag, sizeof(newTermios.c_lflag)) != 0 ||
                    memcmp(&newTermios.c_cc, &setTermios.c_cc, sizeof(newTermios.c_cc)) != 0)
                {
                    Logging::error("New serial settings could not be set for '%s'\n", deviceName.c_str());
                    close(fd);
                    return 0;
                }
            }
        }

        break;

    case TYPE_BLACKMAGIC:

        // Supports the RS422 port on a Blackmagic DeckLink card. This port is used like a standard
        // tty device, but its driver is pretty minimal and the port speed, character size, parity and
        // stop bits can't be changed or read without an error.

        // Check that the requested serial settings match the fixed settings for the Blackmagic driver
        if (baudRate == BAUD_38400  && charSize == CHAR_SIZE_8  && stopBits == STOP_BITS_1  && parity == PARITY_ODD)
        {
            // open the serial device
            fd = ::open(deviceName.c_str(), O_RDWR | O_NONBLOCK | O_NOCTTY);

            if (fd < 0)
            {
                if (Config::dbg_serial_port_open)
                {
                    Logging::debug("Failed to open serial device '%s': %s\n", deviceName.c_str(), strerror(errno));
                }
                return 0;
            }
        }
        else
        {
            Logging::error("Serial settings incompatible with '%s'\n", deviceName.c_str());
            return 0;
        }

        break;

    case TYPE_DVS:
        int sv_ret;
        char setup[64];
        int opentype, flags;

        opentype = SV_OPENTYPE_RS422A | SV_OPENTYPE_RS422B | SV_OPENTYPE_RS422C | SV_OPENTYPE_RS422D;

        sprintf(setup, "PCI,card=0,channel=0");

        sv_ret = sv_openex(&sv,
                        setup,
                        SV_OPENPROGRAM_APPLICATION,
                        opentype,
                        0,
                        0);

        if (sv_ret == SV_ERROR_WRONGMODE)
        {
            // Multichannel mode not on so doesn't like "channel=0"
            sprintf(setup, "PCI,card=0");

            sv_ret = sv_openex(&sv, setup,
                            SV_OPENPROGRAM_APPLICATION,
                            opentype,
                            0,
                            0);
        }

        if (sv_ret != SV_OK)
        {
            Logging::error("Failed to open DVS device: %s\n", sv_geterrortext(sv_ret));
            return false;
        }

        // DVS ports 0, 1, 2 and 3 are supported, they are labelled rmt1, rmt2, rmt3 and rmt4.
        // According to the DVS SDK manual, port 0 is a master port and 1, 2 & 3 are slave ports.
        // They only appear to communicate with a VTR if they are in the slave configuration,
        // so port 0 is swapped.
        switch (deviceName[deviceName.size() - 1])
        {
        case '1':
            fd = 0;
            flags = SV_RS422_PINOUT_SWAPPED;
            break;

        case '2':
            fd = 1;
            flags = SV_RS422_PINOUT_NORMAL;
            break;

        case '3':
            fd = 2;
            flags = SV_RS422_PINOUT_NORMAL;
            break;

        case '4':
            fd = 3;
            flags = SV_RS422_PINOUT_NORMAL;
            break;

        default:
            sv_close(sv);
            return 0;
        }

        sv_ret = sv_rs422_open(sv, fd, 38400, flags);

        if (sv_ret != SV_OK)
        {
            Logging::error("Failed to open DVS RS422 port %s: %s\n", deviceName.c_str(), sv_geterrortext(sv_ret));
            sv_close(sv);
            return 0;
        }

        break;
    }

    return new SerialPort(deviceName, fd, type, sv);
}

SerialPort::SerialPort(string deviceName, int fd, SerialType type, sv_handle * sv)
: _deviceName(deviceName), _fd(fd), _type(type), _sv(sv)
{
}

SerialPort::~SerialPort()
{
    switch (_type)
    {
    case TYPE_STD_TTY:
    case TYPE_BLACKMAGIC:
        if (_fd >= 0)
        {
            close(_fd);
        }

        break;

    case TYPE_DVS:

        if (_fd >= 0 && _fd <= 3)
        {
            sv_rs422_close(_sv, _fd);
        }

        if (_sv)
        {
            sv_close(_sv);
        }

        break;
    }
}

// TODO: there should be some inter symbol timeout as well after the first
// symbol has been read before the timeoutMSec
int SerialPort::read(unsigned char* buffer, int count, long timeoutMSec)
{
    LOCK_SECTION(_accessMutex);

    fd_set set;
    struct timeval now, tv;
    struct timespec ts;
    int result;
    int retval = 0;
    ssize_t numRead;
    int totalRead = 0;
    
    switch (_type)
    {
    case TYPE_STD_TTY:
    case TYPE_BLACKMAGIC:
        while (true)
        {
            FD_ZERO(&set);
            FD_SET(_fd, &set);

            tv.tv_sec = timeoutMSec / 1000;
            tv.tv_usec = (timeoutMSec % 1000) * 1000;

            result = select(_fd + 1, &set, NULL, NULL, &tv);
            if (result < 0)
            {
                if (errno == EINTR)
                {
                    // select was interrupted by a signal - retry
                    continue;
                }

                // select error
                Logging::error("Failed to do a select on the serial port: %s\n", strerror(errno));
                retval = -1;
                break;
            }
            else if (result == 0)
            {
                // select timeout
                retval = -2;
                break;
            }

            numRead = ::read(_fd, &buffer[totalRead], count - totalRead);
            if (numRead < 0)
            {
                Logging::error("Failed to read from serial port: %s\n", strerror(errno));
                retval = -1;
                break;
            }

            totalRead += numRead;
            if (totalRead == count)
            {
                break;
            }
        }
        break;

    case TYPE_DVS:

        // Create 1ms timeout
        ts.tv_sec = 0;
        ts.tv_nsec = 1000000;

        // Create timeout target
        gettimeofday(&tv, 0);
        tv.tv_usec += (timeoutMSec % 1000) * 1000;
        tv.tv_sec += (tv.tv_usec / 1000000);
        tv.tv_usec %= 1000000;
        tv.tv_sec += timeoutMSec / 1000;

        while (true)
        {
            retval = sv_rs422_rw(_sv, _fd, FALSE, (char *) &buffer[totalRead], count - totalRead, (int *) &numRead, 0);

            if (retval != SV_OK)
            {
                Logging::error("Failed to read from DVS serial port %s: %s\n", _deviceName.c_str(), sv_geterrortext(retval));
                retval = -1;
                break;
            }

            totalRead += numRead;

            if (totalRead == count)
            {
                break;
            }

            // Check if we have tried for long enough, wait 1ms if not
            gettimeofday(&now, 0);

            if (now.tv_sec > tv.tv_sec || (now.tv_sec == tv.tv_sec && now.tv_usec >= tv.tv_usec))
            {
                // Timeout
                retval = -2;
                break;
            }

            if (nanosleep(&ts, NULL) < 0)
            {
                if (errno == EINTR)
                {
                    // sleep was interrupted by a signal - retry
                    continue;
                }

                // sleep error
                Logging::error("Failed to sleep waiting for the DVS serial port: %s\n", strerror(errno));
                retval = -1;
                break;
            }
        }
        break;
    }

    return retval;
}

int SerialPort::write(const unsigned char* buffer, int count)
{
    LOCK_SECTION(_accessMutex);

    ssize_t result;
    int retval = 0, nwrite = 0, sv_ret;

    switch (_type)
    {
    case TYPE_STD_TTY:
    case TYPE_BLACKMAGIC:
        while (true)
        {
            result = ::write(_fd, buffer, count);
            if (result < 0)
            {
                if (errno == EAGAIN || errno == EINTR)
                {
                    // try again if write was blocked or interrupted by a signal
                    continue;
                }

                Logging::error("Failed to write to serial port: %s\n", strerror(errno));
                retval = -1;
            }
            else if (result != count)
            {
                Logging::error("Failed to write %d bytes to serial port; only %d bytes were written\n", count, result);
                retval = -1;
            }
            
            break;
        }
        break;

    case TYPE_DVS:

        sv_ret = sv_rs422_rw(_sv, _fd, TRUE, (char *) buffer, count, &nwrite, 0);

        if (sv_ret != SV_OK)
        {
            Logging::error("Failed to write to DVS serial port %s: %s\n", _deviceName.c_str(), sv_geterrortext(sv_ret));
        }
        else if (nwrite != count)
        {
            Logging::error("Failed to write %d bytes to DVS serial port; only %d bytes were written\n", count, nwrite);
            retval = -1;
        }

        break;
    }

    return retval;
}

void SerialPort::flush(bool read, bool write)
{
    LOCK_SECTION(_accessMutex);
    
    switch (_type)
    {
    case TYPE_STD_TTY:
    case TYPE_BLACKMAGIC:
        if (read && !write)
        {
            if (tcflush(_fd, TCIFLUSH) != 0)
            {
                Logging::error("Failed to flush serial port '%s' input\n", _deviceName.c_str());
            }
        }
        else if (write && !read)
        {
            if (tcflush(_fd, TCOFLUSH) != 0)
            {
                Logging::error("Failed to flush serial port '%s' output\n", _deviceName.c_str());
            }
        }
        else // read && write
        {
            if (tcflush(_fd, TCIOFLUSH) != 0)
            {
                Logging::error("Failed to flush serial port '%s' input and output\n", _deviceName.c_str());
            }
        }
        break;

    case TYPE_DVS:
        // The DVS card does not provide a way to flush any internal buffers. We assume that
        // data written has already gone, so a write flush is not required. For read, we simply
        // read until no bytes are returned.

        if (read)
        {
            char buffer;
            int sv_ret, numRead;

            do
            {
                sv_ret = sv_rs422_rw(_sv, _fd, FALSE, &buffer, 1, &numRead, 0);
            }
            while (sv_ret == SV_OK && numRead == 1);


            if (sv_ret != SV_OK)
            {
                Logging::error("Failed to flush DVS serial port %s: %s\n", _deviceName.c_str(), sv_geterrortext(sv_ret));
            }
        }

        break;
    }
}

