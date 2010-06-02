/*
 * $Id: MtEncoder.h,v 1.4 2010/06/02 13:09:53 john_f Exp $
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

#ifndef MtEncoder_h
#define MtEncoder_h

#include <ace/Task.h>

#include "VideoRaster.h"
#include "ffmpeg_encoder.h"

class EncodeFrameBuffer;

class MtEncoder : public ACE_Task<ACE_MT_SYNCH>
{
public:
    MtEncoder(ACE_Thread_Mutex * ff_mutex);
    virtual ~MtEncoder();

    void Init(MaterialResolution::EnumType res, Ingex::VideoRaster::EnumType raster, int num_threads);
    void Encode(EncodeFrameTrack & eft);
    void Close();
    virtual int svc();

private:
    ACE_Thread_Mutex * mpAvcodecMutex; // For AVCodec init
    MaterialResolution::EnumType mRes;
    Ingex::VideoRaster::EnumType mRaster;
    int mNumThreads;
    int mShutdown;
};


#endif // ifndef MtEncoder_h

