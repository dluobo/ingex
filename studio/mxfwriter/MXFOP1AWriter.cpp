/*
 * $Id: MXFOP1AWriter.cpp,v 1.4 2010/10/12 17:44:12 john_f Exp $
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

#include <string.h>
#include <memory>

#include <libMXF++/MXF.h>
#include <D10MXFOP1AWriter.h>

#include "MXFOP1AWriter.h"
#include "MXFWriterException.h"
#include "MaterialResolution.h"

#include <Logging.h>
#include <Utilities.h>

using namespace std;
using namespace prodauto;


#define CHECK(cmd) \
    if (!(cmd)) \
    { \
        PA_LOGTHROW(MXFWriterException, ("'%s' failed\n", #cmd)); \
    }



namespace prodauto
{

class D10MXFOP1AContentPackage : public D10ContentPackage
{
public:
    D10MXFOP1AContentPackage()
    {
        mVideoTrackId = (uint32_t)(-1);
        mVideoSize = 0;
        mGrowingVideoSize = 0;
        mVideo = 0;
        mVideoSet = false;
        memset(mAudioTrackId, (uint32_t)(-1), sizeof(mAudioTrackId));
        mAudioSize = 0;
        mAudioAllocatedSize = 0;
        memset(mGrowingAudioSize, 0, sizeof(mGrowingAudioSize));
        memset(mAudio, 0, sizeof(mAudio));
        memset(mAudioSet, false, sizeof(mAudioSet));
        mNumAudioTracks = 0;
        mNumNullAvidAudioTracks = 0;
    }
    
    virtual ~D10MXFOP1AContentPackage()
    {
        delete [] mVideo;
        
        uint32_t i;
        for (i = 0; i < mNumAudioTracks; i++)
            delete [] mAudio[i];
    }
    
    bool IsComplete() const
    {
        if (!mVideoSet)
            return false;
        
        uint32_t i;
        for (i = 0; i < mNumAudioTracks - mNumNullAvidAudioTracks; i++) {
            if (!mAudioSet[i])
                return false;
        }
        
        return true;
    }
    
    void Reset()
    {
        mVideoSet = false;
        mGrowingVideoSize = 0;
        
        uint32_t i;
        for (i = 0; i < mNumAudioTracks; i++) {
            mAudioSet[i] = false;
            mGrowingAudioSize[i] = 0;
        }
    }
    
    bool IsVideoTrack(uint32_t mp_track_id) const
    {
        return mp_track_id == mVideoTrackId;
    }
    
    int GetAudioTrackIndex(uint32_t mp_track_id) const
    {
        uint32_t i;
        for (i = 0; i < mNumAudioTracks; i++) {
            if (mAudioTrackId[i] == mp_track_id)
                return i;
        }
        
        return -1;
    }
    
public:
    // from D10ContentPackage
    virtual Timecode GetUserTimecode() const { return mUserTimecode; }
    virtual const unsigned char* GetVideo() const { return mVideo; }
    virtual unsigned int GetVideoSize() const { return mVideoSize; }
    virtual uint32_t GetNumAudioTracks() const { return mNumAudioTracks; }
    virtual const unsigned char* GetAudio(int index) const { return mAudio[index]; }
    virtual unsigned int GetAudioSize() const { return mAudioSize; }
    
public:
    ::Timecode mUserTimecode;
    
    uint32_t mVideoTrackId;
    unsigned int mVideoSize;
    unsigned int mGrowingVideoSize;
    unsigned char *mVideo;
    bool mVideoSet;
    
    uint32_t mAudioTrackId[8];
    unsigned int mAudioSize;
    unsigned int mAudioAllocatedSize;
    unsigned int mGrowingAudioSize[8];
    unsigned char *mAudio[8];
    bool mAudioSet[8];
    uint32_t mNumAudioTracks;
    uint32_t mNumNullAvidAudioTracks;
};

};


static mxfRational convert_rational(const prodauto::Rational &in)
{
    mxfRational out;
    
    out.numerator = in.numerator;
    out.denominator = in.denominator;
    
    return out;
}

static mxfUMID convert_umid(const prodauto::UMID &in)
{
    mxfUMID out;
    
    memcpy(&out.octet0, &in.octet0, 32);
    
    return out;
}



MXFOP1AWriter::MXFOP1AWriter()
: MXFWriter()
{
    mPackageGroup = 0;
    mOwnPackageGroup = false;
    mContentPackage = 0;
    mD10Writer = 0;
}

MXFOP1AWriter::~MXFOP1AWriter()
{
    ResetWriter();
    delete mContentPackage;
}

void MXFOP1AWriter::PrepareToWrite(PackageGroup *package_group, bool take_ownership)
{
    PA_ASSERT(package_group->GetOP() == OperationalPattern::OP_1A);
    PA_ASSERT(package_group->GetMaterialResolution() == MaterialResolution::IMX30_MXF_1A ||
              package_group->GetMaterialResolution() == MaterialResolution::IMX40_MXF_1A ||
              package_group->GetMaterialResolution() == MaterialResolution::IMX50_MXF_1A);
    
    ResetWriter();
    
    mPackageGroup = package_group;
    mOwnPackageGroup = take_ownership;
    
    mContentPackage = new D10MXFOP1AContentPackage();


    // get ids of video and audio tracks and count audio tracks
    
    MaterialPackage *material_package = mPackageGroup->GetMaterialPackage();
    size_t i;
    for (i = 0; i < material_package->tracks.size(); i++) {
        Track *track = material_package->tracks[i];
        if (track->dataDef == PICTURE_DATA_DEFINITION) {
            mContentPackage->mVideoTrackId = track->id;
        } else if (track->dataDef == SOUND_DATA_DEFINITION) {
            if (mContentPackage->mNumAudioTracks >= 8) {
                Logging::warning("Skipped audio track because D10 MXF OP-1A only supports max 8\n");
                continue;
            }
            mContentPackage->mAudioTrackId[mContentPackage->mNumAudioTracks] = track->id;
            mContentPackage->mNumAudioTracks++;
        }
    }
    if (mContentPackage->mVideoTrackId == (uint32_t)(-1)) {
        PA_LOGTHROW(ProdAutoException, ("D10 MXF OP-1A requires a video track\n"));
    }
    
    
    // Avid (Media Composer 3.0) doesn't support 2 audio channels (it only shows channel 1) for MPEG 30/40/50
    // and therefore we add 2 channels of silence
    
    if (mContentPackage->mNumAudioTracks == 2) {
        Logging::warning("Adding 2 channels of silence to support Avid import\n");
        int i;
        for (i = 0; i < 2; i++) {
            mContentPackage->mAudioTrackId[mContentPackage->mNumAudioTracks] = (uint32_t)(-1);
            mContentPackage->mNumAudioTracks++;
            mContentPackage->mNumNullAvidAudioTracks++;
        }
    }
    
    
    // get start timecode
    
    int64_t start_timecode = 0;
    Track *track = mPackageGroup->GetFileSourceTrack(mContentPackage->mVideoTrackId);
    if (track)
        start_timecode = track->sourceClip->position;
    
    
    
    // create and initialize D10 writer
    
    SourcePackage *source_package =  mPackageGroup->GetFileSourcePackage();
    FileEssenceDescriptor *file_descriptor = dynamic_cast<FileEssenceDescriptor*>(source_package->descriptor);
    PA_ASSERT(file_descriptor);
    
    mD10Writer = new D10MXFOP1AWriter();

    if (package_group->Is25FPSProject())
        mD10Writer->SetSampleRate(D10MXFOP1AWriter::D10_SAMPLE_RATE_625_50I);
    else
        mD10Writer->SetSampleRate(D10MXFOP1AWriter::D10_SAMPLE_RATE_525_60I);
    mD10Writer->SetAudioChannelCount(mContentPackage->mNumAudioTracks);
    mD10Writer->SetAudioQuantizationBits(file_descriptor->audioQuantizationBits);
    mD10Writer->SetAspectRatio(convert_rational(file_descriptor->imageAspectRatio));
    mD10Writer->SetStartTimecode(start_timecode, false);
    switch (file_descriptor->videoResolutionID)
    {
        case MaterialResolution::IMX30_MXF_1A:
            if (package_group->Is25FPSProject())
                mD10Writer->SetBitRate(D10MXFOP1AWriter::D10_BIT_RATE_30, 150000);
            else
                mD10Writer->SetBitRate(D10MXFOP1AWriter::D10_BIT_RATE_30, 125125);
            break;
        case MaterialResolution::IMX40_MXF_1A:
            if (package_group->Is25FPSProject())
                mD10Writer->SetBitRate(D10MXFOP1AWriter::D10_BIT_RATE_40, 200000);
            else
                mD10Writer->SetBitRate(D10MXFOP1AWriter::D10_BIT_RATE_40, 166833);
            break;
        case MaterialResolution::IMX50_MXF_1A:
            if (package_group->Is25FPSProject())
                mD10Writer->SetBitRate(D10MXFOP1AWriter::D10_BIT_RATE_50, 250000);
            else
                mD10Writer->SetBitRate(D10MXFOP1AWriter::D10_BIT_RATE_50, 208541);
            break;
        default:
            PA_ASSERT(false);
    }
    mD10Writer->SetMaterialPackageUID(convert_umid(material_package->uid));
    mD10Writer->SetFileSourcePackageUID(convert_umid(source_package->uid));
    
    CHECK(mD10Writer->CreateFile(mPackageGroup->GetFileLocation()));
    
    
    // further initialize content package
    
    switch (file_descriptor->videoResolutionID)
    {
        case MaterialResolution::IMX30_MXF_1A:
            if (package_group->Is25FPSProject())
                mContentPackage->mVideoSize = 150000;
            else
                mContentPackage->mVideoSize = 125125;
            break;
        case MaterialResolution::IMX40_MXF_1A:
            if (package_group->Is25FPSProject())
                mContentPackage->mVideoSize = 200000;
            else
                mContentPackage->mVideoSize = 166833;
            break;
        case MaterialResolution::IMX50_MXF_1A:
            if (package_group->Is25FPSProject())
                mContentPackage->mVideoSize = 250000;
            else
                mContentPackage->mVideoSize = 208541;
            break;
        default:
            PA_ASSERT(false);
    }
    mContentPackage->mVideo = new unsigned char[mContentPackage->mVideoSize];
    
    if (mContentPackage->mNumAudioTracks > 0) {
        uint32_t bytes_per_sample = (file_descriptor->audioQuantizationBits + 7) / 8;
        if (package_group->Is25FPSProject())
            mContentPackage->mAudioAllocatedSize = bytes_per_sample * 1920;
        else
            mContentPackage->mAudioAllocatedSize = bytes_per_sample * 1602;
        
        uint32_t i;
        for (i = 0; i < mContentPackage->mNumAudioTracks; i++) {
            mContentPackage->mAudio[i] = new unsigned char[mContentPackage->mAudioAllocatedSize];
            memset(mContentPackage->mAudio[i], 0, mContentPackage->mAudioAllocatedSize);
        }
    }
    
    package_group->UpdateStoredDimensions(mD10Writer->GetStoredWidth(), mD10Writer->GetStoredHeight());
}

void MXFOP1AWriter::WriteSamples(uint32_t mp_track_id, uint32_t num_samples, const uint8_t *data, uint32_t data_size)
{
    if (mContentPackage->IsVideoTrack(mp_track_id)) {
        PA_ASSERT(num_samples == 1);
        PA_ASSERT(data_size == mContentPackage->mVideoSize);
        PA_ASSERT(!mContentPackage->mVideoSet);

        memcpy(mContentPackage->mVideo, data, mContentPackage->mVideoSize);
        mContentPackage->mVideoSet = true;
    } else {
        int audio_index = mContentPackage->GetAudioTrackIndex(mp_track_id);
        if (audio_index >= 0) {
            PA_ASSERT((mPackageGroup->Is25FPSProject() && num_samples == 1920) ||
                      (!mPackageGroup->Is25FPSProject() && (num_samples == 1602 || num_samples == 1601)));
            PA_ASSERT(data_size <= mContentPackage->mAudioAllocatedSize);
            PA_ASSERT(!mContentPackage->mAudioSet[audio_index]);
    
            memcpy(mContentPackage->mAudio[audio_index], data, data_size);
            mContentPackage->mAudioSize = data_size;
            mContentPackage->mAudioSet[audio_index] = true;
        }
    }
    
    if (mContentPackage->IsComplete()) {
        mContentPackage->mUserTimecode = mD10Writer->GenerateUserTimecode();
        mD10Writer->WriteContentPackage(mContentPackage);
        
        mContentPackage->Reset();
    }
}

void MXFOP1AWriter::StartSampleData(uint32_t mp_track_id)
{
    if (mContentPackage->IsVideoTrack(mp_track_id)) {
        PA_ASSERT(!mContentPackage->mVideoSet);
        
        mContentPackage->mGrowingVideoSize = 0;
    } else {
        int audio_index = mContentPackage->GetAudioTrackIndex(mp_track_id);
        if (audio_index >= 0) {
            PA_ASSERT(!mContentPackage->mAudioSet[audio_index]);
            
            mContentPackage->mGrowingAudioSize[audio_index] = 0;
        }
    }
}

void MXFOP1AWriter::WriteSampleData(uint32_t mp_track_id, const uint8_t *data, uint32_t data_size)
{
    if (mContentPackage->IsVideoTrack(mp_track_id)) {
        PA_ASSERT(!mContentPackage->mVideoSet);
        PA_ASSERT(mContentPackage->mGrowingVideoSize + data_size <= mContentPackage->mVideoSize);
        
        memcpy(&mContentPackage->mVideo[mContentPackage->mGrowingVideoSize], data, data_size);
        mContentPackage->mGrowingVideoSize += data_size;
    } else {
        int audio_index = mContentPackage->GetAudioTrackIndex(mp_track_id);
        if (audio_index >= 0) {
            PA_ASSERT(!mContentPackage->mAudioSet[audio_index]);
            PA_ASSERT(mContentPackage->mGrowingAudioSize[audio_index] + data_size <= mContentPackage->mAudioAllocatedSize);
            
            mContentPackage->mGrowingAudioSize[audio_index] = 0;
            mContentPackage->mGrowingAudioSize[audio_index] += data_size;
        }
    }
}

void MXFOP1AWriter::EndSampleData(uint32_t mp_track_id, uint32_t num_samples)
{
    if (mContentPackage->IsVideoTrack(mp_track_id)) {
        PA_ASSERT(num_samples == 1);
        PA_ASSERT(!mContentPackage->mVideoSet);
        PA_ASSERT(mContentPackage->mGrowingVideoSize == mContentPackage->mVideoSize);

        mContentPackage->mVideoSet = true;
        mContentPackage->mGrowingVideoSize = 0;
    } else {
        int audio_index = mContentPackage->GetAudioTrackIndex(mp_track_id);
        if (audio_index >= 0) {
            PA_ASSERT((mPackageGroup->Is25FPSProject() && num_samples == 1920) ||
                      (!mPackageGroup->Is25FPSProject() && (num_samples == 1602 || num_samples == 1601)));
            PA_ASSERT(!mContentPackage->mAudioSet[audio_index]);
            PA_ASSERT(mContentPackage->mGrowingAudioSize[audio_index] > 0);
    
            mContentPackage->mAudioSet[audio_index] = true;
            mContentPackage->mAudioSize = mContentPackage->mGrowingAudioSize[audio_index];
            mContentPackage->mGrowingAudioSize[audio_index] = 0;
        }
    }

    if (mContentPackage->IsComplete()) {
        mContentPackage->mUserTimecode = mD10Writer->GenerateUserTimecode();
        mD10Writer->WriteContentPackage(mContentPackage);
        
        mContentPackage->Reset();
    }
}

void MXFOP1AWriter::CompleteWriting(bool save_to_database)
{
    try
    {
        mPackageGroup->UpdateDuration(mD10Writer->GetDuration());
        
        mD10Writer->CompleteFile();
        delete mD10Writer;
        mD10Writer = 0;
        
        mPackageGroup->RelocateFile(mDestinationDirectory);
        
        if (save_to_database)
            mPackageGroup->SaveToDatabase();
    }
    catch (...)
    {
        try
        {
            delete mD10Writer;
            mD10Writer = 0;
            
            mPackageGroup->RelocateFile(mFailureDirectory);
        }
        catch (...)
        {
            // give up
        }
        
        mD10Writer = 0;

        throw;
    }
}

void MXFOP1AWriter::AbortWriting(bool delete_files)
{
    try
    {
        delete mD10Writer;
        mD10Writer = 0;
        
        if (delete_files)
            mPackageGroup->DeleteFile();
        else
            mPackageGroup->RelocateFile(mFailureDirectory);
    }
    catch (...)
    {
        // give up
    }

    mD10Writer = 0;
}

void MXFOP1AWriter::ResetWriter()
{
    if (mD10Writer) {
        Logging::warning("MXF OP-1A writing was not completed or aborted - aborting without deleting the files\n");
        try
        {
            AbortWriting(false);
        }
        catch (...)
        {
        }
        
        mD10Writer = 0;
    }

    if (mPackageGroup) {
        if (mOwnPackageGroup)
            delete mPackageGroup;
        mPackageGroup = 0;
        mOwnPackageGroup = false;
    }
    
    delete mContentPackage;
    mContentPackage = 0;
}

