/*
 * $Id: ForwardTable.cpp,v 1.1 2007/09/11 14:08:33 stuart_hc Exp $
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

#include "ace/Log_Msg.h"
#include "ForwardTable.h"
#include "Forwarder.h"
#include "DateTime.h"

ForwardTable::ForwardTable(unsigned int max_size)
    :max_size_(max_size)
{
    ACE_TRACE(ACE_TEXT ("ForwardTable::ForwardTable()"));
    //ACE_DEBUG((LM_DEBUG, "Max size = %d\n", max_size));
}


ForwardTable::~ForwardTable()
{
    ACE_TRACE(ACE_TEXT ("ForwardTable::~ForwardTable()"));
    //ACE_DEBUG((LM_DEBUG, "ForwardTable::~ForwardTable()\n" ));

    removeAllForwarders();
}


void ForwardTable::removeAllForwarders(void)
{
    ACE_TRACE(ACE_TEXT ("ForwardTable::removeAllForwarders()"));
    ACE_Guard<ACE_Thread_Mutex> guard(this->mutex_);
    std::vector<Forwarder *>::iterator iter = table_.begin();

    while (iter != table_.end()) {
        Forwarder *fwd = *iter;

        if (fwd != NULL) {
            fwd->oustClient();
            fwd->msg_queue()->deactivate();
            table_.erase(iter);
        } else 
            ACE_ERROR((LM_ERROR, "We have a null forwarder\n"));
    }
}


int ForwardTable::addForwarder(Forwarder *f)
{
    ACE_TRACE(ACE_TEXT ("ForwardTable::addForwarder()"));

    std::vector<Forwarder *>::iterator iter;
    iter = findForwarder(f->client());

    ACE_Guard<ACE_Thread_Mutex> guard(this->mutex_);

    if (iter == table_.end()) { // Client is not already subscribed
        ACE_DEBUG((LM_DEBUG, "Client is not already subscribed.\n"));

        if (!isFull())
	{
            table_.push_back(f);
            return 0;
        }
	else
	{
#if 1
		return -1;
#else
	// See if we can oust a lower priority forwarder
            iter = findForwarder(f->clientPriority());
            if (iter == table_.end()) { // No forwarders with lower priority
                return -1;
            } else {    // Replace lower priority forwarder
                Forwarder *fwd = *iter;
                fwd->oustClient();
                fwd->msg_queue()->deactivate();
                table_.erase(iter);
                table_.push_back(f);
                ACE_DEBUG((LM_DEBUG, "An existing client has been ousted "
                                     "by a new client.\n"));
                return 0;
            }           
#endif
        }
    } else {    // Client is already subscribed
        ACE_DEBUG((LM_DEBUG, "Client is already connected.\n"));
        return 1;
    }
}

    
bool ForwardTable::removeForwarder(Forwarder *f)
{
    ACE_TRACE(ACE_TEXT ("ForwardTable::removeForwarder()"));
    ACE_Guard<ACE_Thread_Mutex> guard(this->mutex_);
    std::vector<Forwarder *>::iterator iter;

    iter = findForwarder(f);

    if (iter != table_.end()) {
        Forwarder *f = *iter;
        f->msg_queue()->deactivate();   

        ACE_DEBUG((LM_DEBUG, "Matching client found.\n"));
        ACE_DEBUG((LM_DEBUG, "Erasing Forwarder object from vector.\n"));
        table_.erase(iter);
        ACE_DEBUG((LM_DEBUG, "We now have %d connected monitors.\n", table_.size()));
        return true;
    } else {
        return false;
    }
}

    
bool ForwardTable::removeForwarder(CORBA::Object_ptr client)
{
    ACE_TRACE(ACE_TEXT ("ForwardTable::removeForwarder()"));
    //ACE_DEBUG(( LM_DEBUG, "ForwardTable::removeForwarder(CORBA::Object_ptr)\n" ));

    ACE_Guard<ACE_Thread_Mutex> guard(this->mutex_);
    std::vector<Forwarder *>::iterator iter;

    iter = findForwarder(client);

    if (iter != table_.end()) {
        Forwarder *f = *iter;

        /* If we deactivate the Task's message queue, the svc() method will
         * return, and close() will be called, which will delete the
         * object. close() should not be called by an external agent,
         * because the Task's thread must have finished before the object is
         * deleted. */
        f->msg_queue()->deactivate();

        ACE_DEBUG((LM_DEBUG, "Matching client found.\n"));
        ACE_DEBUG((LM_DEBUG, "Erasing Forwarder object from vector.\n"));
        table_.erase(iter);
        ACE_DEBUG((LM_DEBUG, "We now have %d connected monitors.\n",
                                  table_.size()));
        return true;
    } else {
        return false;
    }
}


void ForwardTable::queueMessage(MessageBlock *m, ACE_Time_Value *timeout)
{
    ACE_TRACE(ACE_TEXT ("ForwardTable::queueMessage()"));
    ACE_DEBUG(( LM_DEBUG, "ForwardTable::queueMessage()\n"));

    ACE_Guard<ACE_Thread_Mutex> guard(this->mutex_);
    std::vector<Forwarder *>::iterator iter;

    /* For each Forwarder, duplicate the message block and put it in the
     * Forwarder's queue. */
    for (iter = table_.begin(); iter != table_.end(); ++iter)
    {
        Forwarder *f = *iter;
        int n;

        /* We use duplicate so that the underlying message block only gets
         * deleted once all threads have dealt with it, i.e., when the
         * reference count is down to 0. */
        MessageBlock *copy = (MessageBlock *) m->duplicate();

        //ACE_DEBUG((LM_DEBUG, "Message block reference count is now %d\n", m->reference_count()));
        //ACE_DEBUG((LM_DEBUG, "Data block reference count is now %d\n", m->data_block()->reference_count()));

        if ((n = f->putq(copy, timeout)) != -1)
        {
            ACE_DEBUG((LM_DEBUG, "There are now %d messages in the queue.\n", n));
        }
        else
        {
            std::string date_time = DateTime::DateTimeNoSeparators();
            ACE_ERROR((LM_ERROR, "%C Couldn't enqueue message: %m\n"
                "  There are %d messages in the queue.\n",
                date_time.c_str(),
                f->msg_queue()->message_count() ));

            copy->release();
        }
    }

    //ACE_DEBUG((LM_DEBUG, "Data block address before release is %@.\n",
    //         m->data_block()));
    //ACE_DEBUG((LM_DEBUG, "Data block points to %@.\n",
    //         ((DataBlock *)m->data_block())->data()));
    //ACE_DEBUG((LM_DEBUG, "Data block base is %@.\n",
    //         m->data_block()->base()));

    /* Use release, not delete.  release decrements the reference count; delete
     * upsets the reference counting mechanism. */
    m->release();

    //ACE_DEBUG((LM_DEBUG, "Message block release completed.\n" ));
}


bool ForwardTable::isFull() const
{
    ACE_TRACE(ACE_TEXT ("ForwardTable::isFull()"));
    return (table_.size() >= max_size_);
}


unsigned int ForwardTable::size() const
{
    ACE_TRACE(ACE_TEXT ("ForwardTable::size()"));
    return table_.size();
}


std::vector<Forwarder *>::iterator ForwardTable::findForwarder(
        CORBA::Object_ptr client)
{
    ACE_TRACE(ACE_TEXT ("ForwardTable::findForwarder()"));
    //ACE_DEBUG(( LM_DEBUG, "ForwardTable::findForwarder(CORBA::Object_ptr)\n" ));
    //ACE_DEBUG(( LM_DEBUG, "Lookng for:\n%C\n", theProxy::instance()->orb_->object_to_string(client) ));

    bool match = false;
    std::vector<Forwarder *>::iterator iter;
    iter = table_.begin();

    while (iter != table_.end()  &&  !match) {
        Forwarder *fwd = *iter;
        ::CORBA::Object_ptr target = fwd->client();
        
        //ACE_DEBUG(( LM_DEBUG, "Checking target:\n%C\n",
        //  theProxy::instance()->orb_->object_to_string(target) ));

        if (client->_is_equivalent(target)) {
            //ACE_DEBUG((LM_DEBUG, "Match found!\n"));
            match = true;
        } else {
            ++iter;
        }
    }

    return iter;
}
    
#if 0
std::vector<Forwarder *>::iterator ForwardTable::findForwarder(
        CORBA::Octet priority)
{
    ACE_TRACE(ACE_TEXT ("ForwardTable::findForwarder()"));

    bool match = false;
    std::vector<Forwarder *>::iterator iter;
    iter = table_.begin();

    while (iter != table_.end()  &&  !match) {
        Forwarder *fwd = *iter;
        ::CORBA::Octet fwd_priority = fwd->clientPriority();
        
        if (priority > fwd_priority) {
            ACE_DEBUG((LM_DEBUG, "Lower priority forwarder found!\n"));
            match = true;
        } else {
            ++iter;
        }
    }

    return iter;
}
#endif


std::vector<Forwarder *>::iterator ForwardTable::findForwarder(Forwarder *f)
{
    ACE_TRACE(ACE_TEXT ("ForwardTable::findForwarder()"));

    bool match = false;
    std::vector<Forwarder *>::iterator iter;
    iter = table_.begin();

    while (iter != table_.end()  &&  !match) {
        Forwarder *fwd = *iter;
        
        if (f == fwd) {
            //ACE_DEBUG((LM_DEBUG, "Match found!\n"));
            match = true;
        } else {
            ++iter;
        }
    }

    return iter;
}
