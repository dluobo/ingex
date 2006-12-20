/*
 * $Id: ForwardTable.h,v 1.1 2006/12/20 12:28:29 john_f Exp $
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

#ifndef ForwardTable_h
#define ForwardTable_h

#include <ace/Task.h>

#include <StatusClientC.h>
#include <DataSourceC.h>

#include "Block.h"
#include "Forwarder.h"

#include <vector>

// Forward declaration of Forwarder class
class Forwarder;

class ForwardTable
{
    public:
        ForwardTable(unsigned int max_size);
        ~ForwardTable();

        int addForwarder(Forwarder *);
        bool removeForwarder(Forwarder *);
        bool removeForwarder(CORBA::Object_ptr client);
        void queueMessage(MessageBlock *, ACE_Time_Value *timeout);
        unsigned int size(void) const;

    private:
        std::vector<Forwarder *> table_;
        ACE_Thread_Mutex mutex_;
        unsigned int max_size_;

        bool isFull(void) const;
        void removeAllForwarders(void);

        std::vector<Forwarder *>::iterator findForwarder(CORBA::Object_ptr client);
        //std::vector<Forwarder *>::iterator findForwarder(CORBA::Octet priority);
        std::vector<Forwarder *>::iterator findForwarder(Forwarder *);
};


#endif  /* ForwardTable_h */
