/*
 * $Id: MXFWriter.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * MXF file writer
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

#ifndef __RECORDER_MXF_WRITER_H__
#define __RECORDER_MXF_WRITER_H__

#include <string>

#include "MXFContentPackage.h"
#include "Types.h"

#include <archive_types.h>


namespace rec
{

class MXFWriter
{
public:
    static bool parseInfaxData(std::string infax_string, InfaxData *infax_data);
    static uint32_t calcCRC32(const unsigned char *data, uint32_t size);

public:
    virtual bool writeContentPackage(const MXFContentPackage *package) = 0;

    virtual bool abort() = 0;
    virtual bool complete(InfaxData *source_infax_data,
                          const PSEFailure *pse_failures, long pse_failure_count,
                          const VTRError *vtr_errors, long vtr_error_count,
                          const DigiBetaDropout *digibeta_dropouts, long digibeta_dropout_count) = 0;

    virtual int64_t getFileSize() = 0;

    virtual UMID getMaterialPackageUID() = 0;
    virtual UMID getFileSourcePackageUID() = 0;
    virtual UMID getTapeSourcePackageUID() = 0;
};



};


#endif
