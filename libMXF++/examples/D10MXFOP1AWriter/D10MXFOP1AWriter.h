/*
 * $Id: D10MXFOP1AWriter.h,v 1.9 2011/11/10 10:44:17 philipn Exp $
 *
 * D10 MXF OP-1A writer
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#ifndef __D10_MXF_OP1A_WRITER_H__
#define __D10_MXF_OP1A_WRITER_H__


#include <string>
#include <vector>
#include <queue>

#include <libMXF++/MXF.h>

#include "D10ContentPackage.h"


class SetWithDuration;

class D10MXFOP1AWriter
{
public:
    typedef enum
    {
        D10_BIT_RATE_30,
        D10_BIT_RATE_40,
        D10_BIT_RATE_50
    } D10BitRate;

    typedef enum
    {
        D10_SAMPLE_RATE_625_50I,
        D10_SAMPLE_RATE_525_60I,
    } D10SampleRate;
    
public:
    static uint32_t GetContentPackageSize(D10SampleRate sample_rate, uint32_t encoded_picture_size);

public:
    D10MXFOP1AWriter();
    ~D10MXFOP1AWriter();
    
    // configure and create file
    
    void SetSampleRate(D10SampleRate sample_rate);                      // default D10_SAMPLE_RATE_625_50I
    void SetAudioChannelCount(uint32_t count);                          // default is 4
    void SetAudioQuantizationBits(uint32_t bits);                       // default is 24; alternative is 16
    void SetAudioSequenceOffset(uint8_t offset);                        // default determined from input
    void SetAspectRatio(mxfRational aspect_ratio);                      // default is 16:9; alternative is 4:3
    void SetStartTimecode(int64_t count, bool drop_frame);              // default 0, false
    void SetBitRate(D10BitRate rate, uint32_t encoded_picture_size);    // default D10_BIT_RATE_50, 250000
    void SetMaterialPackageUID(mxfUMID uid);                            // default generated
    void SetFileSourcePackageUID(mxfUMID uid);                          // default generated
    void SetProductInfo(std::string company_name, std::string product_name, std::string version, mxfUUID product_uid);
                                                                        // default see source
    
    mxfpp::DataModel* CreateDataModel();
    mxfpp::HeaderMetadata* CreateHeaderMetadata();
    void ReserveHeaderMetadataSpace(uint32_t min_bytes);
    
    bool CreateFile(std::string filename);
    bool CreateFile(mxfpp::File **file);
    
    
    // write content package data
    
    // user timecode written to the system item in the essence container
    void SetUserTimecode(Timecode user_timecode);
    // timecode calculated from the start timecode and duration
    Timecode GenerateUserTimecode();
    
    // MPEG video data
    void SetVideo(const unsigned char *data, uint32_t size);
    
    // PCM audio data
    // the number of audio samples must follow the 5-sequence count 1602, 1601, 1602, 1601, 1602 when
    // the sample rate is 60i
    uint32_t GetAudioSampleCount();
    void SetAudio(uint32_t channel, const unsigned char *data, uint32_t size);
    
    void WriteContentPackage();
    
    void WriteContentPackage(const D10ContentPackage *content_package);
    
    
    // file info
    mxfpp::HeaderMetadata* GetHeaderMetadata() const { return mHeaderMetadata; }
    mxfpp::DataModel* GetDataModel() const { return mDataModel; }
    int64_t GetDuration() const { return mDuration; }
    int64_t GetFileSize() const;
    mxfUMID GetMaterialPackageUID() const { return mMaterialPackageUID; }
    mxfUMID GetFileSourcePackageUID() const { return mFileSourcePackageUID; }
    uint32_t GetStoredWidth() const;
    uint32_t GetStoredHeight() const;
    
    
    // optional update of header metadata and complete writing file
    void UpdateStartTimecode(int64_t count);
    void CompleteFile();
    
private:
    void CreateFile();
    uint32_t WriteSystemItem(const D10ContentPackage *content_package);

    void InitAES3Block(DynamicByteArray *aes3_block, uint8_t sequence_index);
    void UpdateAES3Blocks(const D10ContentPackage *content_package);
    void FinalUpdateAES3Blocks();

    uint8_t GetAudioSequenceOffset(const D10ContentPackage *next_content_package);

private:
    D10SampleRate mSampleRate;
    mxfRational mVideoSampleRate;
    uint16_t mRoundedTimecodeBase;
    uint32_t mChannelCount;
    uint32_t mAudioQuantizationBits;
    uint32_t mAudioBytesPerSample;
    uint8_t mAudioSequenceOffset;
    mxfRational mAspectRatio;
    int64_t mStartTimecode;
    bool mDropFrameTimecode;
    D10BitRate mD10BitRate;
    uint32_t mEncodedImageSize;
    uint32_t mMaxEncodedImageSize;
    uint32_t mAES3DataSize;
    uint32_t mReserveMinBytes;
    std::string mCompanyName;
    std::string mProductName;
    std::string mVersionString;
    mxfUUID mProductUID;

    uint32_t mSystemItemSize;
    uint32_t mVideoItemSize;
    uint32_t mAudioItemSize;
    mxfUL mEssenceContainerUL;
    
    mxfUMID mMaterialPackageUID;
    mxfUMID mFileSourcePackageUID;

    mxfpp::File *mMXFFile;
    mxfpp::DataModel *mDataModel;
    mxfpp::HeaderMetadata *mHeaderMetadata;
    mxfpp::IndexTableSegment *mIndexSegment;
    mxfpp::Partition *mHeaderPartition;
    int64_t mHeaderMetadataStartPos;
    int64_t mHeaderMetadataEndPos;
    std::vector<SetWithDuration*> mSetsWithDuration;
    mxfpp::TimecodeComponent *mMaterialPackageTC;
    mxfpp::TimecodeComponent *mFilePackageTC;
    
    D10ContentPackageInt *mContentPackage;
    std::queue<DynamicByteArray*> mAES3Blocks;
    uint32_t mAudioSequence[5];
    uint8_t mAudioSequenceCount;
    uint8_t mAudioSequenceIndex;
    uint32_t mMinAudioSamples;
    uint32_t mMaxAudioSamples;
    uint32_t mMaxFinalPaddingSamples;

    std::vector<D10ContentPackage*> mBufferedContentPackages;

    int64_t mDuration;
};


#endif

