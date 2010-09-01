/*
 * $Id: MXFEventFile.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#include <cstdlib>

#include <memory>

#include <libMXF++/MXF.h>

#include <archive_mxf_info_lib.h>

#include "MXFEventFile.h"
#include "MXFMetadataHelper.h"
#include "RecorderException.h"
#include "Logging.h"

using namespace std;
using namespace rec;
using namespace mxfpp;



MXFEventFile::MXFEventFile()
{
    mPSEFailures = 0;
    mNumPSEFailures = 0;
    mVTRErrors = 0;
    mNumVTRErrors = 0;
    mDigiBetaDropouts = 0;
    mNumDigiBetaDropouts = 0;
    mTimecodeBreaks = 0;
    mNumTimecodeBreaks = 0;
}

MXFEventFile::~MXFEventFile()
{
    if (mPSEFailures)
        free(mPSEFailures);
    if (mVTRErrors)
        free(mVTRErrors);
    if (mDigiBetaDropouts)
        free(mDigiBetaDropouts);
    if (mTimecodeBreaks)
        free(mTimecodeBreaks);
}

bool MXFEventFile::Read(string filename)
{
    if (mPSEFailures) {
        free(mPSEFailures);
        mPSEFailures = 0;
        mNumPSEFailures = 0;
    }
    if (mVTRErrors) {
        free(mVTRErrors);
        mVTRErrors = 0;
        mNumVTRErrors = 0;
    }
    if (mDigiBetaDropouts) {
        free(mDigiBetaDropouts);
        mDigiBetaDropouts = 0;
        mNumDigiBetaDropouts = 0;
    }
    if (mTimecodeBreaks) {
        free(mTimecodeBreaks);
        mTimecodeBreaks = 0;
        mNumTimecodeBreaks = 0;
    }
    
    
    File *mxf_file = 0;
    DataModel *data_model = 0;
    HeaderMetadata *header_metadata = 0;
    mxfKey key;
    uint8_t llen;
    uint64_t len;
    try
    {
        mxf_file = File::openRead(filename);
        
        data_model = new DataModel();
        MXFMetadataHelper::RegisterDataModelExtensions(data_model);
        header_metadata = new HeaderMetadata(data_model);
        
        auto_ptr<Partition> header_partition(Partition::findAndReadHeaderPartition(mxf_file));
        if (header_partition.get() == 0)
            throw "Failed to find header partition";
        
        mxf_file->readNextNonFillerKL(&key, &llen, &len);
        REC_CHECK(mxf_is_header_metadata(&key));
        
        header_metadata->read(mxf_file, header_partition.get(), &key, llen, len);
        
        REC_CHECK(archive_mxf_get_pse_failures(header_metadata->getCHeaderMetadata(),
                                               &mPSEFailures, &mNumPSEFailures));
        REC_CHECK(archive_mxf_get_vtr_errors(header_metadata->getCHeaderMetadata(),
                                             &mVTRErrors, &mNumVTRErrors));
        REC_CHECK(archive_mxf_get_digibeta_dropouts(header_metadata->getCHeaderMetadata(),
                                                    &mDigiBetaDropouts, &mNumDigiBetaDropouts));
        REC_CHECK(archive_mxf_get_timecode_breaks(header_metadata->getCHeaderMetadata(),
                                                  &mTimecodeBreaks, &mNumTimecodeBreaks));

        delete mxf_file;
        mxf_file = 0;
        delete header_metadata;
        header_metadata = 0;
        delete data_model;
        data_model = 0;
        
        return true;
    }
    catch (...)
    {
        delete mxf_file;
        delete header_metadata;
        delete data_model;
        
        return false;
    }
}

