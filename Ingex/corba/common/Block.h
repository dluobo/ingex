/*
 * $Id: Block.h,v 1.1 2006/12/20 12:28:28 john_f Exp $
 *
 * Extended ACE_Data_Block to carry arbitrary data.
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

#ifndef Block_h
#define Block_h

#include "ace/Task.h"
#include "ace/Lock_Adapter_T.h"

#include "ace/Thread_Mutex.h"

#include "Package.h"

/**
For passing data to subtitle/status forwarders.
See R&D Tech Note no. 2467.
See also ACE Tutorial no. 13.
The Lock class provides the locking strategy used during release() etc.
*/
class DataBlock : public ACE_Data_Block
{
    public: 
        DataBlock(Package *);
        ~DataBlock();
        Package *data(void);
        
    protected:
        Package *data_;

        class Lock : public ACE_Lock_Adapter<ACE_Thread_Mutex> 
        {
            public:
                Lock(void);
                ~Lock(void);
                int destroy(void);
        };
};

#if 1
/**
For passing data to forwarders.
See R&D Tech Note no. 2467.
*/
class MessageBlock : public ACE_Message_Block
{
    public:
        MessageBlock(Package *);
        ~MessageBlock(void);
};
#endif

#endif  //#ifndef Block_h
