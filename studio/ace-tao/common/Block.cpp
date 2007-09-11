/*
 * $Id: Block.cpp,v 1.1 2007/09/11 14:08:32 stuart_hc Exp $
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

#include <ace/Log_Msg.h>

#include "Block.h"


DataBlock::DataBlock(Package *p)
    :ACE_Data_Block(sizeof(Package *), 
     ACE_Message_Block::MB_DATA, 0, 0, new Lock(), 0, 0), data_(p)
{
    ACE_DEBUG(( LM_DEBUG, "DataBlock::DataBlock()\n" ));
}


DataBlock::~DataBlock()
{
    ACE_DEBUG((LM_DEBUG, "DataBlock::~DataBlock()\n"));
    ((Lock *) locking_strategy())->destroy();
    delete data_;
}


Package *DataBlock::data() 
{
    return this->data_; 
}


DataBlock::Lock::Lock()
{
    ACE_TRACE(ACE_TEXT ("DataBlock::Lock::Lock()"));
}


DataBlock::Lock::~Lock()
{
    ACE_TRACE(ACE_TEXT ("DataBlock::Lock::~Lock()"));
}


int DataBlock::Lock::destroy()
{
    ACE_TRACE(ACE_TEXT ("DataBlock::Lock::destroy()"));
    delete this;
    return 0;
}


MessageBlock::MessageBlock(Package *p)
    :ACE_Message_Block(new DataBlock(p))
{
    ACE_TRACE(ACE_TEXT ("MessageBlock::MessageBlock()"));
    ACE_DEBUG(( LM_DEBUG, "MessageBlock::MessageBlock()\n" ));
}


MessageBlock::~MessageBlock()
{
    ACE_TRACE(ACE_TEXT ("MessageBlock::~MessageBlock()"));
    ACE_DEBUG((LM_DEBUG, "MessageBlock::~MessageBlock()\n"));
}
