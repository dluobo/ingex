/*
 * $Id: D10MXFFrameWriter.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * SMPTE D-10 (S386M) MXF frame writer
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
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

#ifndef __RECORDER_D10_MXF_FRAME_WRITER_H__
#define __RECORDER_D10_MXF_FRAME_WRITER_H__

#include "FrameWriter.h"
#include "D10MXFWriter.h"
#include "FFmpegEncoder.h"


namespace rec
{

class D10MXFFrameWriter : public FrameWriter
{
public:
    D10MXFFrameWriter(int element_size, bool palff_mode, D10MXFWriter *writer);
    virtual ~D10MXFFrameWriter();
    
public:
    // from FrameWriter
    virtual void writeFrame();
    
private:
    D10MXFWriter *_writer;
    FFmpegEncoder *_encoder;
};



};


#endif
