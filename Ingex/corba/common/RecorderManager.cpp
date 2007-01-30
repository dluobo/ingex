/*
 * $Id: RecorderManager.cpp,v 1.2 2007/01/30 12:18:31 john_f Exp $
 *
 * Wrapper for ProdAuto::Recorder.
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

#include "RecorderManager.h"

const ProdAuto::Rational EDIT_RATE = { 25, 1 }; // We assume 25 frames per second


void RecorderManager::Recorder(CORBA::Object_ptr obj)
{
    if (!CORBA::is_nil(obj))
    {
        try
        {
            mRecorder = ProdAuto::Recorder::_narrow(obj);
        }
        catch(const CORBA::Exception &)
        {
            mRecorder = ProdAuto::Recorder::_nil();
        }
    }
    else
    {
        mRecorder = ProdAuto::Recorder::_nil();
    }
}

void RecorderManager::Init()
{
    // Find out how many recording tracks there are
    if (!CORBA::is_nil(mRecorder))
    {
        try
        {
            ProdAuto::TrackList_var track_list;
            track_list = mRecorder->Tracks();
            mTrackCount = track_list->length();
        }
        catch(const CORBA::Exception & e)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Tracks() gave exception: %C\n"), e._name()));
            //mRecorder = ProdAuto::Recorder::_nil();
            mTrackCount = 0;
        }
    }
}

void RecorderManager::AddStatusClient(ProdAuto::StatusClient_ptr client)
{
    if(!CORBA::is_nil(mRecorder))
    {
        try
        {
            mRecorder->AddStatusClient(client);
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("StatusClient attached.\n")));
        }
        catch(const CORBA::Exception &)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to attach StatusClient.\n")));
        }
    }
}

void RecorderManager::RemoveStatusClient(ProdAuto::StatusClient_ptr client)
{
    if (!CORBA::is_nil(mRecorder))
    {
        try
        {
            mRecorder->RemoveStatusClient(client);
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Successfully detached.\n")));
        }
        catch(const CORBA::Exception &e)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to detach: %C\n"), e._name()));
        }
    }
}

void RecorderManager::Start()
{
    if(!CORBA::is_nil(mRecorder.in()))
    {
        ProdAuto::MxfTimecode start_tc;
        start_tc.edit_rate = EDIT_RATE;
        start_tc.samples = 0;
        start_tc.undefined = true;  // Meaning "now"

        ProdAuto::MxfDuration pre_roll;
        pre_roll.edit_rate = EDIT_RATE;
        pre_roll.samples = 5;
        pre_roll.undefined = false;

        ::ProdAuto::BooleanList rec_enable;
        rec_enable.length(mTrackCount);
        for (unsigned int i = 0; i < mTrackCount; ++i)
        {
            rec_enable[i] = 1;
        }

        try
        {
            mRecorder->Start(start_tc, pre_roll, rec_enable, "IngexTestProject");
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Recorder started.\n")));
        }
        catch(const CORBA::Exception & e)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Start() gave exception: %C\n"), e._name()));
            //mRecorder = ProdAuto::Recorder::_nil();
        }
    }
}

void RecorderManager::Stop()
{
    if (! CORBA::is_nil(mRecorder))
    {
        ProdAuto::MxfTimecode stop_tc;
        stop_tc.edit_rate = EDIT_RATE;
        stop_tc.samples = 0;
        stop_tc.undefined = true;  // Meaning "now"

        ProdAuto::MxfDuration post_roll;
        post_roll.edit_rate = EDIT_RATE;
        post_roll.samples = 5;
        post_roll.undefined = false;

        ::ProdAuto::StringList_var files;

        try
        {
            mRecorder->Stop(stop_tc, post_roll, files.out());
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Recorder stopped.\n")));
        }
        catch(const CORBA::Exception & e)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Stop() gave exception: %C\n"), e._name()));
            //mRecorder = ProdAuto::Recorder::_nil();
        }
    }
}

