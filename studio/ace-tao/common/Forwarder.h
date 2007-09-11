/*
 * $Id: Forwarder.h,v 1.1 2007/09/11 14:08:33 stuart_hc Exp $
 *
 * Send data to an observer.
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
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

#ifndef Forwarder_h
#define Forwarder_h

#include <ace/Task.h>
#include "StatusClientC.h"


/* Forward declarations */
class ForwardTable;
class Package;

/**
This class forwards messages such as status information to a
destination such as a monitoring device.  It is derived from ACE_Task in
order to do the forwarding in a separate thread.
See R&D Tech Note no. 2467.
*/
class Forwarder : public ACE_Task<ACE_MT_SYNCH>
{
    public:
        virtual ~Forwarder(void);

        virtual int open(void *);
        virtual int svc(void);
        virtual int close(u_long = 0);
        virtual CORBA::Object_ptr client(void) const;
        virtual void oustClient(void) = 0;
        
    protected:
        Forwarder(CORBA::Object_ptr client, ForwardTable *, unsigned int queue_depth);
        CORBA::Object_var client_;

    private:
        ForwardTable *fwd_table_;
        unsigned int queue_depth_;

        void selfDestruct(void);
        virtual bool forward(Package *) = 0;
        virtual const char * type() = 0;
};


/**
A Forwarder for status messages.
*/
class StatusForwarder : public Forwarder
{
    public:
        StatusForwarder(CORBA::Object_ptr client, ForwardTable *, unsigned int queue_depth);
        ~StatusForwarder(void);

        virtual void oustClient(void);

    private:
        virtual bool forward(Package *);
        bool PingClient();
        bool GetClientToProcessStatus(const ProdAuto::StatusItem & status);
        const char * type() { return "status"; }
};


#endif  //#ifndef Forwarder_h
