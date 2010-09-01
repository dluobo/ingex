/*
 * $Id: ArchiveMXFWriter.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * Archive MXF file writer
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

#include "ArchiveMXFWriter.h"
#include "ArchiveMXFContentPackage.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Logging.h"

using namespace std;
using namespace rec;



static ArchiveTimecode convert_timecode(Timecode from)
{
    ArchiveTimecode to;
    
    to.dropFrame = false;
    to.hour = from.hour;
    to.min = from.min;
    to.sec = from.sec;
    to.frame = from.frame;
    
    return to;
}

static UMID convert_umid(mxfUMID from)
{
    UMID to;
    
    memcpy(to.bytes, &from.octet0, 32);
    
    return to;
}

static mxfRational convert_rational(Rational from)
{
    mxfRational to;
    
    to.numerator = from.numerator;
    to.denominator = from.denominator;
    
    return to;
}



uint32_t rec::ArchiveMXFWriter::getContentPackageSize(bool component_depth_8bit, int num_audio_tracks,
                                                      bool include_crc32)
{
    return (uint32_t)(get_archive_mxf_content_package_size(component_depth_8bit, num_audio_tracks, include_crc32));
}


rec::ArchiveMXFWriter::ArchiveMXFWriter()
{
    _componentDepth = 8;
    _aspectRatio = Rational(4, 3);
    _numAudioTracks = 4;
    _includeCRC32 = true;
    _startPosition = 0;
    
    _writer = 0;
}

rec::ArchiveMXFWriter::~ArchiveMXFWriter()
{
    if (_writer)
        abort_archive_mxf_file(&_writer);
}

void rec::ArchiveMXFWriter::setComponentDepth(uint32_t depth)
{
    REC_ASSERT(depth == 8 || depth == 10);
    _componentDepth = depth;
}

void rec::ArchiveMXFWriter::setAspectRatio(Rational aspect_ratio)
{
    REC_ASSERT(aspect_ratio == Rational(4, 3) || aspect_ratio == Rational(16, 9));
    _aspectRatio = aspect_ratio;
}

void rec::ArchiveMXFWriter::setNumAudioTracks(int count)
{
    REC_ASSERT(count >= 0 && count <= 8);
    _numAudioTracks = count;
}

void rec::ArchiveMXFWriter::setIncludeCRC32(bool enable)
{
    _includeCRC32 = enable;
}

void rec::ArchiveMXFWriter::setStartPosition(int64_t position)
{
    _startPosition = position;
}

bool rec::ArchiveMXFWriter::createFile(string filename)
{
    REC_ASSERT(!_writer);
    
    mxfRational mxfAspectRatio = convert_rational(_aspectRatio);
    
    return prepare_archive_mxf_file(filename.c_str(), _componentDepth == 8, &mxfAspectRatio,
                                    _numAudioTracks, _includeCRC32, _startPosition, true,
                                    &_writer);
}

bool rec::ArchiveMXFWriter::createFile(MXFFile **mxfFile, std::string filename)
{
    REC_ASSERT(!_writer);
    
    mxfRational mxfAspectRatio = convert_rational(_aspectRatio);
    
    return prepare_archive_mxf_file_2(mxfFile, filename.c_str(), _componentDepth == 8, &mxfAspectRatio,
                                    _numAudioTracks, _includeCRC32, _startPosition, true,
                                    &_writer);
}

bool rec::ArchiveMXFWriter::writeContentPackage(const MXFContentPackage *content_package)
{
    REC_ASSERT(_writer);

    const ArchiveMXFContentPackage *arch_content_package =
        dynamic_cast<const ArchiveMXFContentPackage*>(content_package);
    REC_ASSERT(arch_content_package);
    
    REC_ASSERT(_numAudioTracks == arch_content_package->getNumAudioTracks());
    REC_ASSERT((!_includeCRC32 && arch_content_package->getNumCRC32() == 0) ||
               (_includeCRC32 && arch_content_package->getNumCRC32() == 1 + _numAudioTracks));

    try
    {
        ArchiveTimecode mxfLTC = convert_timecode(arch_content_package->getLTC());
        ArchiveTimecode mxfVITC = convert_timecode(arch_content_package->getVITC());
        REC_CHECK(write_system_item(_writer, mxfVITC, mxfLTC,
                                    arch_content_package->getCRC32(), arch_content_package->getNumCRC32()));
        
        if (_componentDepth == 10) {
            REC_CHECK(write_video_frame(_writer, arch_content_package->getVideo(), arch_content_package->getVideoSize()));
        } else {
            REC_CHECK(write_video_frame(_writer, arch_content_package->getVideo8Bit(), arch_content_package->getVideo8BitSize()));
        }
        
        int i;
        for (i = 0; i < arch_content_package->getNumAudioTracks(); i++)
            REC_CHECK(write_audio_frame(_writer, arch_content_package->getAudio(i), arch_content_package->getAudioSize()));
        
        _timecodeBreakHelper.ProcessTimecode(arch_content_package->haveLTC(),
                                             timecode_to_position(arch_content_package->getLTC()),
                                             arch_content_package->haveVITC(),
                                             timecode_to_position(arch_content_package->getVITC()));
    }
    catch (...)
    {
        return false;
    }
    
    return true;
}

bool rec::ArchiveMXFWriter::abort()
{
    REC_ASSERT(_writer);
    
    try
    {
        REC_CHECK(abort_archive_mxf_file(&_writer));
    }
    catch (...)
    {
        return false;
    }
    
    return true;
}

bool rec::ArchiveMXFWriter::complete(InfaxData *source_infax_data,
                                     const PSEFailure *pse_failures, long pse_failure_count,
                                     const VTRError *vtr_errors, long vtr_error_count,
                                     const DigiBetaDropout *digibeta_dropouts, long digibeta_dropout_count)
{
    REC_ASSERT(_writer);
    
    try
    {
        vector<TimecodeBreak>& timecode_breaks = _timecodeBreakHelper.GetBreaks();

        REC_CHECK(complete_archive_mxf_file(&_writer, source_infax_data,
                                            pse_failures, pse_failure_count,
                                            vtr_errors, vtr_error_count,
                                            digibeta_dropouts, digibeta_dropout_count,
                                            &timecode_breaks[0], (long)timecode_breaks.size()));
    }
    catch (...)
    {
        return false;
    }
    
    return true;
}

int64_t rec::ArchiveMXFWriter::getFileSize()
{
    REC_ASSERT(_writer);
    
    return get_archive_mxf_file_size(_writer);
}

UMID rec::ArchiveMXFWriter::getMaterialPackageUID()
{
    REC_ASSERT(_writer);
    
    return convert_umid(get_material_package_uid(_writer));
}

UMID rec::ArchiveMXFWriter::getFileSourcePackageUID()
{
    REC_ASSERT(_writer);

    return convert_umid(get_file_package_uid(_writer));
}

UMID rec::ArchiveMXFWriter::getTapeSourcePackageUID()
{
    REC_ASSERT(_writer);

    return convert_umid(get_tape_package_uid(_writer));
}

