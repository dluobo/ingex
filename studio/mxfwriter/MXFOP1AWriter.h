/*
 * $Id: MXFOP1AWriter.h,v 1.1 2010/03/30 08:30:21 john_f Exp $
 *
 * MXF OP-1A writer
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

#ifndef __PRODAUTO_MXF_OP1A_WRITER_H__
#define __PRODAUTO_MXF_OP1A_WRITER_H__


#include <string>
#include <vector>
#include <map>

#include "MXFWriter.h"

class D10MXFOP1AWriter;


namespace prodauto
{

class D10MXFOP1AContentPackage;

class MXFOP1AWriter : public MXFWriter
{
public:
    MXFOP1AWriter();
    virtual ~MXFOP1AWriter();

public:
    // from MXFWriter
    
    virtual void PrepareToWrite(PackageGroup *package_group, bool take_ownership = false);
    
    virtual void WriteSamples(uint32_t mp_track_id, uint32_t num_samples, const uint8_t *data, uint32_t data_size);
    virtual void StartSampleData(uint32_t mp_track_id);
    virtual void WriteSampleData(uint32_t mp_track_id, const uint8_t *data, uint32_t data_size);
    virtual void EndSampleData(uint32_t mp_track_id, uint32_t num_samples);
    
    virtual void CompleteWriting(bool save_to_database);
    virtual void AbortWriting(bool delete_files);
    
private:
    void ResetWriter();
    
private:
    PackageGroup *mPackageGroup;
    bool mOwnPackageGroup;

    D10MXFOP1AContentPackage *mContentPackage;
    
    D10MXFOP1AWriter *mD10Writer;
};


};


#endif

