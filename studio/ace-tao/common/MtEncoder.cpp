/*
 * $Id: MtEncoder.cpp,v 1.9 2011/04/18 09:34:14 john_f Exp $
 *
 * Video encoder using multiple threads.
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
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

#include "ElapsedTimeReporter.h"
#include "DateTime.h"
#include "Block.h"
#include "Package.h"
#include "EncodeFrameBuffer.h"
#include "MtEncoder.h"

const int DEFAULT_NUM_THREADS = 3;

MtEncoder::MtEncoder(ACE_Thread_Mutex * mutex)
: ACE_Task<ACE_MT_SYNCH>(),
  mpAvcodecMutex(mutex), mShutdown(0)
{
}

MtEncoder::~MtEncoder()
{
    wait();
}

void MtEncoder::Init(MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster, int num_threads)
{
    mRes = res;
    mRaster = raster;
    if (num_threads < 1)
    {
        num_threads = DEFAULT_NUM_THREADS;
    }
    mNumThreads = num_threads;
    this->activate(THR_NEW_LWP | THR_JOINABLE | THR_INHERIT_SCHED, num_threads);
}

void MtEncoder::Encode(EncodeFrameTrack * eft)
{
    //ACE_DEBUG((LM_INFO, ACE_TEXT("MtEncoder queueing frame %d\n"),
    //    eft->FrameIndex()));
    FramePackage * fp = new FramePackage(eft);
    MessageBlock * mb = new MessageBlock(fp);
    putq(mb);
}

void MtEncoder::Close()
{
    // There must be a better way, but ...
    for (int i = 0; i < mNumThreads; ++i)
    {
        ACE_Message_Block * mb = new ACE_Message_Block();
        mb->msg_type(ACE_Message_Block::MB_HANGUP);
        putq(mb);
    }
}

int MtEncoder::svc()
{
    //ACE_DEBUG((LM_INFO, ACE_TEXT("MtEncoder::svc()\n")));

    // Init ffmpeg encoder
    ffmpeg_encoder_t * enc = 0;
    {
        // Prevent "insufficient thread locking around avcodec_open/close()"
        ACE_Guard<ACE_Thread_Mutex> guard(*mpAvcodecMutex);

        enc = ffmpeg_encoder_init(mRes, mRaster, 0);
    }

    while (!mShutdown)
    {
        ACE_Message_Block * mb;
        int n = getq(mb);

        if (n == -1)
        {
            // could be terminated due to task shutdown;
            mShutdown = 1;
        }
        else if (ACE_Message_Block::MB_DATA != mb->msg_type())
        {
            // shutdown message
            mShutdown = 1;
            mb->release();
        }
        else
        {
            // Extract frame data.
            DataBlock * db = dynamic_cast<DataBlock *>(mb->data_block());
            Package * p = db->data();
            FramePackage * fp = dynamic_cast<FramePackage *>(p);
            EncodeFrameTrack * eft = fp->VideoData();
            uint8_t * p_input_video = (uint8_t *)eft->Data();

            // Encode the frame (if input data is still in memory).
            uint8_t * p_enc_video = 0;
            size_t size_enc_video = 0;
            if (eft->Valid())
            {
                //ElapsedTimeReporter er(200000, "%s Mt encode started,", DateTime::Timecode().c_str());

                //ACE_DEBUG((LM_INFO, ACE_TEXT("MtEncoder encoding frame %d\n"),
                //    eft.FrameIndex()));
                size_enc_video = ffmpeg_encoder_encode(enc, p_input_video, &p_enc_video);
            }
            else
            {
                //ACE_DEBUG((LM_INFO, ACE_TEXT("MtEncoder skipping frame %d!\n"),
                //    eft.FrameIndex()));
            }

            // Check again the input data is still in memory
            if (eft->Valid())
            {
                // Replace input data with coded data
                eft->Init(p_enc_video, size_enc_video, 1, true, false, true, eft->FrameIndex(), 0);
            }
            else
            {
                // Mark as coded, zero size and in error
                eft->Init(0, 0, 0, false, false, true, eft->FrameIndex(), 0);
                eft->Error(true);
                ACE_DEBUG((LM_ERROR, ACE_TEXT("MtEncoder missed frame   %d!\n"),
                    eft->FrameIndex()));
            }

            mb->release();
        }
    }

    // Close ffmpeg encoder
    {
        // Prevent "insufficient thread locking around avcodec_open/close()"
        ACE_Guard<ACE_Thread_Mutex> guard(*mpAvcodecMutex);

        ffmpeg_encoder_close(enc);
    }

    ACE_DEBUG(( LM_DEBUG, "MtEncoder::svc exiting\n" ));

    return 0;
}


