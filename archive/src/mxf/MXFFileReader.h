/*
 * $Id: MXFFileReader.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * MXF file reader
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

#ifndef __RECORDER_MXF_FILE_READER_H__
#define __RECORDER_MXF_FILE_READER_H__


#include <string>

#include <archive_types.h>

#include "Types.h"
#include "MXFContentPackage.h"


namespace rec
{


class MXFFileReader
{
public:
    MXFFileReader(std::string filename);
    virtual ~MXFFileReader();
    
    std::string getFilename() const { return _filename; }
    bool isComplete() const { return _isComplete; }
    
    int64_t getDuration() const { return _duration; }
    Rational getAspectRatio() const { return _aspectRatio; }

    const InfaxData* getSourceInfaxData() const { return &_sourceInfaxData; }
    const InfaxData* getLTOInfaxData() const { return &_ltoInfaxData; }
 
public:
    virtual long getPSEFailures(PSEFailure **failures) const = 0;
    virtual long getVTRErrors(VTRErrorAtPos **errors) const = 0;
    virtual long getDigiBetaDropouts(DigiBetaDropout **digiBetaDropouts) const = 0;
    virtual long getTimecodeBreaks(TimecodeBreak **timecodeBreaks) const = 0;
    
    virtual bool nextFrame(bool ignoreAVEssenceData, MXFContentPackage *&contentPackage) = 0;
    virtual bool skipFrames(int64_t count) = 0;
    virtual bool readFrame(int64_t position, bool ignoreAVEssenceData, MXFContentPackage *&contentPackage) = 0;
    virtual void forwardTruncate() = 0;

protected:
    std::string _filename;
    bool _isComplete;
    int64_t _duration;
    Rational _aspectRatio;

    InfaxData _sourceInfaxData;
    InfaxData _ltoInfaxData;
};


};



#endif
