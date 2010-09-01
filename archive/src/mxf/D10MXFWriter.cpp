/*
 * $Id: D10MXFWriter.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * SMPTE D-10 (S386M) MXF file writer
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

#include <memory>

#include <libMXF++/MXF.h>
#include <D10MXFOP1AWriter.h>

#include "D10MXFWriter.h"
#include "D10MXFContentPackage.h"
#include "MXFMetadataHelper.h"
#include "MXFCommon.h"
#include "MXFEvents.h"
#include "Utilities.h"
#include "RecorderException.h"
#include "Logging.h"

using namespace std;
using namespace rec;



// the maximum number of VTR errors in which a check is made that a timeode can be
// converted to a position. If not, then no errors are stored and a warning is given
#define MAX_VTR_ERROR_CHECK     100

// maximum number of primary timecode breaks recorded in the file
#define MAX_PRIMARY_TIMECODE_BREAKS     500



static mxfRational convert_rational(Rational from)
{
    mxfRational to;

    to.numerator = from.numerator;
    to.denominator = from.denominator;

    return to;
}

static ::Timecode convert_timecode(rec::Timecode from)
{
    ::Timecode to;

    to.hour = from.hour;
    to.min = from.min;
    to.sec = from.sec;
    to.frame = from.frame;

    return to;
}

static ArchiveTimecode convert_timecode_archive(rec::Timecode from)
{
    ArchiveTimecode to;

    to.hour = from.hour;
    to.min = from.min;
    to.sec = from.sec;
    to.frame = from.frame;

    return to;
}

static rec::UMID convert_umid(::mxfUMID from)
{
    rec::UMID to;

    memcpy(to.bytes, &from.octet0, sizeof(to.bytes));

    return to;
}



class D10ContentPackageMapper : public D10ContentPackage
{
public:
    D10ContentPackageMapper(const D10MXFContentPackage *content_package)
    {
        _contentPackage = content_package;
    }
    virtual ~D10ContentPackageMapper()
    {}

    virtual ::Timecode GetUserTimecode() const
    {
        return convert_timecode(_contentPackage->getLTC());
    }

    virtual const unsigned char* GetVideo() const
    {
        return _contentPackage->getVideo();
    }

    virtual unsigned int GetVideoSize() const
    {
        return _contentPackage->getVideoSize();
    }

    virtual uint32_t GetNumAudioTracks() const
    {
        return _contentPackage->getNumAudioTracks();
    }

    virtual const unsigned char* GetAudio(int index) const
    {
        return _contentPackage->getAudio(index);
    }

    virtual unsigned int GetAudioSize() const
    {
        return _contentPackage->getAudioSize();
    }

private:
    const D10MXFContentPackage *_contentPackage;
};



uint32_t D10MXFWriter::getContentPackageSize()
{
    // TODO: support other variants
    return D10MXFOP1AWriter::GetContentPackageSize(D10MXFOP1AWriter::D10_SAMPLE_RATE_625_50I, 250000);
}


D10MXFWriter::D10MXFWriter()
{
    _aspectRatio = Rational(4, 3);
    _numAudioTracks = 4;
    _writer = 0;
    _primaryTimecode = PRIMARY_TIMECODE_AUTO;
    initialise_timecode_index(&_vitcTimecodeIndex, 512);
    initialise_timecode_index(&_ltcTimecodeIndex, 512);
}

D10MXFWriter::~D10MXFWriter()
{
    delete _writer;
    clear_timecode_index(&_vitcTimecodeIndex);
    clear_timecode_index(&_ltcTimecodeIndex);
}

void D10MXFWriter::setAspectRatio(Rational aspect_ratio)
{
    REC_ASSERT(aspect_ratio == Rational(4, 3) || aspect_ratio == Rational(16, 9));
    _aspectRatio = aspect_ratio;
}

void D10MXFWriter::setNumAudioTracks(int count)
{
    REC_ASSERT(count >= 0 && count <= 8);
    _numAudioTracks = count;
}

void D10MXFWriter::setPrimaryTimecode(PrimaryTimecode primary_timecode)
{
    _primaryTimecode = primary_timecode;
}

void D10MXFWriter::setEventFilename(string filename)
{
    _eventFilename = filename;
}

bool D10MXFWriter::createFile(string filename)
{
    REC_ASSERT(!_writer);

    try
    {
        createWriter();

        _writer->CreateFile(filename);
    }
    catch (...)
    {
        delete _writer;
        _writer = 0;

        return false;
    }

    return true;
}

bool D10MXFWriter::createFile(MXFFile **mxfFile, string filename)
{
    REC_ASSERT(!_writer);

    (void)filename;

    mxfpp::File *file = 0;
    try
    {
        createWriter();

        file = new mxfpp::File(*mxfFile);
        _writer->CreateFile(&file);
    }
    catch (...)
    {
        delete file;

        delete _writer;
        _writer = 0;

        return false;
    }

    *mxfFile = 0;
    return true;
}

bool D10MXFWriter::writeContentPackage(const MXFContentPackage *content_package)
{
    REC_ASSERT(_writer);

    const D10MXFContentPackage *d10_content_package =
        dynamic_cast<const D10MXFContentPackage*>(content_package);
    REC_ASSERT(d10_content_package);

    REC_ASSERT(_numAudioTracks == d10_content_package->getNumAudioTracks());

    try
    {
        D10ContentPackageMapper content_package_mapper(d10_content_package);
        _writer->WriteContentPackage(&content_package_mapper);

        ArchiveTimecode archive_vitc = convert_timecode_archive(d10_content_package->getVITC());
        ArchiveTimecode archive_ltc = convert_timecode_archive(d10_content_package->getLTC());
        REC_CHECK(add_timecode_to_index(&_vitcTimecodeIndex, &archive_vitc));
        REC_CHECK(add_timecode_to_index(&_ltcTimecodeIndex, &archive_ltc));

        _timecodeBreakHelper.ProcessTimecode(d10_content_package->haveLTC(),
                                             timecode_to_position(d10_content_package->getLTC()),
                                             d10_content_package->haveVITC(),
                                             timecode_to_position(d10_content_package->getVITC()));
    }
    catch (...)
    {
        return false;
    }

    return true;
}

bool D10MXFWriter::abort()
{
    REC_ASSERT(_writer);

    try
    {
        delete _writer;
        _writer = 0;
    }
    catch (...)
    {
        return false;
    }

    return true;
}

bool D10MXFWriter::complete(InfaxData *source_infax_data,
                            const PSEFailure *pse_failures, long pse_failure_count,
                            const VTRError *vtr_errors, long vtr_error_count,
                            const DigiBetaDropout *digibeta_dropouts, long digibeta_dropout_count)
{
    REC_ASSERT(_writer);

    try
    {
        MXFMetadataHelper helper(_writer->GetDataModel(), _writer->GetHeaderMetadata());
        // We write the Infax metadata as user comments (Tagged Values), and don't add a tape source package,
        // because Avid fails to read the file when an Infax Framework or tape Source Package are present.
        // An alternative would be to add an Avid meta-dictionary to the file, but this could cause problems in
        // other applications that import IMX
        helper.AddInfaxMetadataAsComments(source_infax_data);

        _writer->UpdateStartTimecode(_timecodeBreakHelper.GetStartTimecode(_primaryTimecode));

        _writer->CompleteFile();
        delete _writer;
        _writer = 0;

        // Events are written to a separate file for the same reason as explained above
        if (_eventFilename.empty()) {
            Logging::warning("Not writing event file because filename is empty\n");
        } else {
            auto_ptr<MXFEventFileWriter> event_file(MXFEventFileWriter::Open(_eventFilename));

            writePSEFailureEvents(event_file.get(), pse_failures, pse_failure_count);
            writeVTRErrorEvents(event_file.get(), vtr_errors, vtr_error_count);
            writeDigiBetaDropoutEvents(event_file.get(), digibeta_dropouts, digibeta_dropout_count);
            _timecodeBreakHelper.WriteTimecodeBreakEvents(event_file.get());

            event_file->Save();
        }
    }
    catch (...)
    {
        return false;
    }

    return true;
}

int64_t D10MXFWriter::getFileSize()
{
    REC_ASSERT(_writer);

    return _writer->GetFileSize();
}

UMID D10MXFWriter::getMaterialPackageUID()
{
    REC_ASSERT(_writer);

    return convert_umid(_writer->GetMaterialPackageUID());
}

UMID D10MXFWriter::getFileSourcePackageUID()
{
    REC_ASSERT(_writer);

    return convert_umid(_writer->GetFileSourcePackageUID());
}

UMID D10MXFWriter::getTapeSourcePackageUID()
{
    REC_ASSERT(_writer);

    // TODO
    return UMID();
}

void D10MXFWriter::createWriter()
{
    mxfRational mxfAspectRatio = convert_rational(_aspectRatio);

    // TODO: support other variants
    _writer = new D10MXFOP1AWriter();
    _writer->SetSampleRate(D10MXFOP1AWriter::D10_SAMPLE_RATE_625_50I);
    _writer->SetAudioChannelCount(_numAudioTracks);
    _writer->SetAudioQuantizationBits(24);
    _writer->SetAspectRatio(mxfAspectRatio);
    _writer->SetStartTimecode(0, false); // start timecode will be updated when completing
    _writer->SetBitRate(D10MXFOP1AWriter::D10_BIT_RATE_50, 250000);
    _writer->SetProductInfo(MXF_IDENT_COMPANY_NAME, MXF_IDENT_PRODUCT_NAME, MXF_IDENT_VERSION_STRING, MXF_IDENT_PRODUCT_UID);
    _writer->ReserveHeaderMetadataSpace(8192);

    MXFMetadataHelper::RegisterDataModelExtensions(_writer->CreateDataModel());
}

void D10MXFWriter::writePSEFailureEvents(MXFEventFileWriter *event_file, const PSEFailure *pse_failures,
                                         long pse_failure_count)
{
    if (pse_failure_count == 0)
        return;

    PSEAnalysisFrameworkEvent::Register(event_file);

    event_file->StartNewEventTrack(PSEAnalysisFrameworkEvent::GetTrackId(),
                                   PSEAnalysisFrameworkEvent::GetTrackName());

    PSEAnalysisFrameworkEvent *event = PSEAnalysisFrameworkEvent::Create(event_file);

    long i;
    for (i = 0; i < pse_failure_count; i++) {
        if (i != 0)
            event->CreateNewInstance();

        event->SetRedFlash(pse_failures[i].redFlash);
        event->SetSpatialPattern(pse_failures[i].spatialPattern);
        event->SetLuminanceFlash(pse_failures[i].luminanceFlash);
        event->SetExtendedFailure(pse_failures[i].extendedFailure);
        event_file->AddEvent(pse_failures[i].position, 1, event);
    }
}

void D10MXFWriter::writeVTRErrorEvents(MXFEventFileWriter *event_file, const VTRError *vtr_errors,
                                       long vtr_error_count)
{
    if (vtr_error_count == 0)
        return;

    bool vitc_index_is_null = is_null_timecode_index(&_vitcTimecodeIndex);
    bool ltc_index_is_null = is_null_timecode_index(&_ltcTimecodeIndex);

    if (vitc_index_is_null && ltc_index_is_null) {
        Logging::warning("SDI LTC and VITC indexes are both null - not recording any vtr errors\n");
        return;
    }


    // check we can at least find one VTR error position in the first MAX_VTR_ERROR_CHECK
    // if we can't then somethng is badly wrong with the RS-232 or SDI links and we decide to store nothing

    TimecodeIndexSearcher vitc_index_searcher;
    TimecodeIndexSearcher ltc_index_searcher;
    initialise_timecode_index_searcher(&_vitcTimecodeIndex, &vitc_index_searcher);
    initialise_timecode_index_searcher(&_ltcTimecodeIndex, &ltc_index_searcher);

    bool located_vtr_error = false;
    const VTRError *vtr_error;
    int64_t error_position;
    bool matched_ltc;
    long i;
    for (i = 0; i < MAX_VTR_ERROR_CHECK && i < vtr_error_count; i++) {
        vtr_error = (const VTRError*)&vtr_errors[i];

        if (findVTRErrorPosition(vitc_index_is_null ? 0 : &vitc_index_searcher,
                                 ltc_index_is_null ? 0 : &ltc_index_searcher,
                                 &vtr_errors[i],
                                 &error_position, &matched_ltc))
        {
            located_vtr_error = true;
            break;
        }
    }
    if (!located_vtr_error) {
        Logging::warning("Failed to find the position of at least one VTR error in first %d "
                         "- not recording any errors", MAX_VTR_ERROR_CHECK);
        return;
    }



    initialise_timecode_index_searcher(&_vitcTimecodeIndex, &vitc_index_searcher);
    initialise_timecode_index_searcher(&_ltcTimecodeIndex, &ltc_index_searcher);

    VTRReplayErrorFrameworkEvent::Register(event_file);

    event_file->StartNewEventTrack(VTRReplayErrorFrameworkEvent::GetTrackId(),
                                   VTRReplayErrorFrameworkEvent::GetTrackName());

    VTRReplayErrorFrameworkEvent *event = VTRReplayErrorFrameworkEvent::Create(event_file);

    located_vtr_error = false;
    for (i = 0; i < vtr_error_count; i++) {

        if (!findVTRErrorPosition(vitc_index_is_null ? 0 : &vitc_index_searcher,
                                  ltc_index_is_null ? 0 : &ltc_index_searcher,
                                  &vtr_errors[i],
                                  &error_position, &matched_ltc))
        {
            continue;
        }

        if (located_vtr_error)
            event->CreateNewInstance();
        else
            located_vtr_error = true;

        event->SetErrorCode(vtr_errors[i].errorCode);
        event_file->AddEvent(error_position, 1, event);
    }
}

void D10MXFWriter::writeDigiBetaDropoutEvents(MXFEventFileWriter *event_file, const DigiBetaDropout *digibeta_dropouts,
                                              long digibeta_dropout_count)
{
    if (digibeta_dropout_count == 0)
        return;

    DigiBetaDropoutFrameworkEvent::Register(event_file);

    event_file->StartNewEventTrack(DigiBetaDropoutFrameworkEvent::GetTrackId(),
                                   DigiBetaDropoutFrameworkEvent::GetTrackName());

    DigiBetaDropoutFrameworkEvent *event = DigiBetaDropoutFrameworkEvent::Create(event_file);

    long i;
    for (i = 0; i < digibeta_dropout_count; i++) {
        if (i != 0)
            event->CreateNewInstance();

        event->SetStrength(digibeta_dropouts[i].strength);
        event_file->AddEvent(digibeta_dropouts[i].position, 1, event);
    }
}

bool D10MXFWriter::findVTRErrorPosition(TimecodeIndexSearcher *vitc_index_searcher,
                                        TimecodeIndexSearcher *ltc_index_searcher,
                                        const VTRError *vtr_error,
                                        int64_t *error_position, bool *matched_ltc)
{
    if (!vitc_index_searcher) {
        if (find_position(ltc_index_searcher, &vtr_error->ltcTimecode, error_position)) {
            *matched_ltc = true;
            return true;
        }
    } else if (!ltc_index_searcher) {
        if (find_position(vitc_index_searcher, &vtr_error->vitcTimecode, error_position)) {
            *matched_ltc = false;
            return true;
        }
    } else {
        if (find_position(ltc_index_searcher, &vtr_error->ltcTimecode, error_position)) {
            *matched_ltc = true;
            return true;
        } else if (find_position(vitc_index_searcher, &vtr_error->vitcTimecode, error_position)) {
            *matched_ltc = false;
            return true;
        }
    }

    return false;
}

