/*
 * $Id: Forwarder.cpp,v 1.1 2006/12/20 12:28:29 john_f Exp $
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

#include <ace/Log_Msg.h>

#include "CorbaUtil.h"
#include "Logfile.h"
#include "DateTime.h"

#include "Forwarder.h"
#include "ForwardTable.h"
#include "Block.h"

#include "StatusClientC.h"

#include <sstream>


Forwarder::Forwarder (CORBA::Object_ptr client, ForwardTable *ft, unsigned int queue_depth) 
    :ACE_Task<ACE_MT_SYNCH>(), client_(client),
     fwd_table_(ft), queue_depth_(queue_depth) 
{
    ACE_TRACE(ACE_TEXT ("Forwarder::Forwarder()"));
    //ACE_DEBUG((LM_DEBUG, "Max queue = %d\n", queue_depth_));  
}


Forwarder::~Forwarder(void)
{
    ACE_TRACE(ACE_TEXT ("Forwarder::~Forwarder()"));
}


int Forwarder::open(void *)
{
    ACE_TRACE(ACE_TEXT ("Forwarder::open()"));

    water_marks(ACE_IO_Cntl_Msg::SET_HWM, 
            (size_t) (queue_depth_ * sizeof(Package *) - 1));
    ACE_DEBUG((LM_DEBUG, "High water mark is %d bytes.\n", 
                msg_queue()->high_water_mark()));

    // Activate task
    activate();
    return 0;
}


// The method that executes in a separate thread
int Forwarder::svc(void)
{
// Start debug logfile for this thread.
// We only do this for debug level > 2 because you will
// get a great many of these files, particularly in the Proxy.
#if 0
    if(Options::Instance()->DebugLevel() > 2)
    {
        Logfile::Open("Fwd");
        char * s = CorbaUtil::Instance()->ObjectToString(client_.in());
        ACE_DEBUG(( LM_NOTICE, "%C Forwarder::svc thread started at %C\n"
            "Sending to:\n%C\n\n",
            type(),
            Timestamp::TimeDate(buf), s ));
        CORBA::string_free(s);
    }
#endif

    ACE_Message_Block *message;
    //MessageBlock *message_block;
    DataBlock *data_block;
    Package *data;
    bool success = false;
    int n;
    
    while ((n = this->getq(message)) != -1) {
        //ACE_DEBUG((LM_DEBUG, "We have pulled a message from the queue. "
        //           "There are %d messages remaining in the queue.\n", n));

        //message_block = ACE_dynamic_cast(MessageBlock *, message);
        data_block = ACE_dynamic_cast(DataBlock *, message->data_block());
        data = data_block->data();

        success = forward(data);
        
        //ACE_DEBUG((LM_DEBUG, "Releasing message block.\n"));
        message->release();
        
        if (!success) {
            selfDestruct();
            this->flush();
        }
    }

    const char * date_time = DateTime::DateTimeNoSeparators();
    ACE_DEBUG((LM_NOTICE, "Exited message loop at %C\n",
        date_time ));
    delete [] date_time;

    return 0;
}


/* close should not be called by an external agent (which begs the 
 * question of why it is not private in the first place!). */
int Forwarder::close(u_long) 
{
    ACE_TRACE(ACE_TEXT ("Forwarder::close()"));
    ACE_DEBUG(( LM_DEBUG, "Forwarder::close()\n" ));
    delete this;
    return 0;
}


::CORBA::Object_ptr Forwarder::client(void)  const
{
    ACE_TRACE(ACE_TEXT ("Forwarder::client()"));
    return client_.in();
}


void Forwarder::selfDestruct(void)
{
    ACE_TRACE(ACE_TEXT ("Forwarder::selfDestruct()"));
    if (fwd_table_->removeForwarder(this))
    {
        const char * date_time = DateTime::DateTimeNoSeparators();
        ACE_DEBUG((LM_INFO, "Due to a problem sending data, Forwarder "
                            "self-destructed at %C.\n",
                            date_time ));
        delete [] date_time;
    }   
}

#if 0
CORBA::Octet Forwarder::clientPriority() const
{
    ACE_TRACE(ACE_TEXT ("Forwarder::clientPriority()"));
    return priority_;
}
#endif





StatusForwarder::StatusForwarder(CORBA::Object_ptr client, ForwardTable *ft, unsigned int queue_depth)
    :Forwarder(client, ft, queue_depth)
{
    ACE_TRACE(ACE_TEXT ("StatusForwarder::StatusForwarder()"));
}


bool StatusForwarder::forward(Package *p)
{
    StatusPackage * sp = ACE_dynamic_cast(StatusPackage *, p);

    if (sp) {
        if(sp->IsEmpty()) {
            ACE_DEBUG(( LM_DEBUG, "Forwarder pinging status client\n" ));
            return PingClient();
        } else {
            ACE_DEBUG(( LM_DEBUG, "Forwarding status (%C = %C)\n",
		sp->status().name.in(), sp->status().value.in() ));
            return GetClientToProcessStatus(sp->status());
        }
    } else {
        ACE_ERROR_RETURN((LM_WARNING, "Package could not be cast to a StatusPackage.\n"),
                          false);
    }
}

void StatusForwarder::oustClient()
{
    ACE_TRACE(ACE_TEXT ("StatusForwarder::oustClient()"));

    ::ProdAuto::StatusClient_var sc = 
            ::ProdAuto::StatusClient::_narrow(client_.in());

    if (CORBA::is_nil(sc.in()))
    {
        try
        {
            sc->HandleStatusOust();
        }
        catch (const CORBA::Exception &)
        {
            // Never mind.
        }
    }

    return;
}

bool StatusForwarder::PingClient()
{
    ACE_TRACE(ACE_TEXT ("StatusForwarder::PingClient()"));

    ::ProdAuto::StatusClient_var sc;
    try
    {
        sc = ::ProdAuto::StatusClient::_narrow(client_.in());
    }
    catch(CORBA::Exception & e)
    {
        ACE_DEBUG((LM_ERROR, "Exception from StatusClient::_narrow: %C\n", e._name() ));
        return false;
    }

    if (CORBA::is_nil(sc.in()))
    {
        ACE_ERROR_RETURN((LM_WARNING, "Client is not a status client.\n"), false);
    }

    bool done = false;
    bool success = false;
    for (int retry = 0; !done; ++retry)
    {
        try
        {
            sc->Ping();
            success = true;
            done = true;
        }
        catch(CORBA::COMM_FAILURE & e)
        {
            ACE_DEBUG((LM_ERROR, "Exception from ping: %C\n", e._name() ));
            if(retry < 1)
            {
                // allow a retry
            }
            else
            {
                done = true;
            }
        }
        catch(CORBA::Exception & e)
        {
            ACE_DEBUG((LM_ERROR, "Exception from ping: %C\n", e._name() ));
            done = true;
        }
    }

    return success;
}

/**
This is a wrapper for ProcessStatus operation on the StatusClient.
Returns true if the operation succeeded.
*/
bool StatusForwarder::GetClientToProcessStatus(const ProdAuto::StatusItem & status)
{
    ::ProdAuto::StatusClient_var sc;
    try
    {
        sc = ::ProdAuto::StatusClient::_narrow(client_.in());
    }
    catch(CORBA::Exception & e)
    {
        ACE_DEBUG((LM_ERROR, "Exception from StatusClient::_narrow: %C\n", e._name() ));
        return false;
    }

    if (CORBA::is_nil(sc.in()))
    {
        ACE_ERROR_RETURN((LM_WARNING, "Client is not a status client.\n"), false);
    }

    bool done = false;
    bool success = false;
    for (int retry = 0; !done; ++retry)
    {
        try
        {
            sc->ProcessStatus(status);
            success = true;
            done = true;
        }
        catch(CORBA::COMM_FAILURE & e)
        {
            ACE_DEBUG((LM_ERROR, "Exception from processStatus: %C\n", e._name() ));
            if(retry < 1)
            {
                // allow a retry
            }
            else
            {
                done = true;
            }
        }
        catch(CORBA::Exception & e)
        {
            ACE_DEBUG((LM_ERROR, "Exception from processStatus: %C\n", e._name() ));
            done = true;
        }
    }

    return success;
}
