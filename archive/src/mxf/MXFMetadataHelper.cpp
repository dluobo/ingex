/*
 * $Id: MXFMetadataHelper.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * MXF metadata helper functions
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

#define __STDC_FORMAT_MACROS    1

#include <cctype>

#include <libMXF++/MXF.h>

#include "MXFMetadataHelper.h"
#include "MXFCommon.h"
#include "RecorderException.h"
#include "Logging.h"

using namespace std;
using namespace rec;
using namespace mxfpp;



// values used to identify fields that are not present
#define INVALID_MONTH_VALUE             99
#define INVALID_DURATION_VALUE          (-1)
#define INVALID_ITEM_NO                 0


static const mxfRational EDIT_RATE = {25, 1};
static const int64_t TAPE_LEN = 120 * 25 * 60 * 60;



static string get_track_name(string prefix, int number)
{
    char buf[16];
    sprintf(buf, "%d", number);
    return prefix.append(buf);
}

static bool is_empty_string(const char *str, int including_space)
{
    const char *str_ptr = str;
    
    if (!str)
        return true;
    
    while (*str_ptr != '\0') {
        if (!including_space || !isspace(*str_ptr))
            return false;
        str_ptr++;
    }
    
    return true;
}



void MXFMetadataHelper::RegisterDataModelExtensions(DataModel *data_model)
{
    register_archive_extensions(data_model);
}


MXFMetadataHelper::MXFMetadataHelper(DataModel *data_model, HeaderMetadata *header_metadata)
{
    mDataModel = data_model;
    mHeaderMetadata = header_metadata;
}

MXFMetadataHelper::~MXFMetadataHelper()
{
}

void MXFMetadataHelper::SetMaterialPackageName(string name)
{
    MaterialPackage *material_package = GetMaterialPackage();
    if (!material_package) {
        Logging::warning("Can't set material package name because no material package found\n");
        return;
    }
    
    material_package->setName(name);
}

SourcePackage* MXFMetadataHelper::AddTapeSourcePackage(string name)
{
    SourcePackage *tape_package = GetTapeSourcePackage();
    if (!tape_package) {
        mxfUMID tape_package_uid;
        mxfTimestamp now;
        size_t num_video_tracks = 0, num_audio_tracks = 0;
        uint32_t tape_track_id, tape_video_track_id = 0, tape_audio_track_id = 0;
        size_t i;
        
        mxf_get_timestamp_now(&now);
        mxf_generate_umid(&tape_package_uid);
        
        
        // get the file source package and count the number of video and audio tracks
        
        SourcePackage *file_package = GetFileSourcePackage();
        REC_ASSERT(file_package);
        vector<GenericTrack*> file_package_tracks = file_package->getTracks();
        for (i = 0; i < file_package_tracks.size(); i++) {
            mxfUL data_def = file_package_tracks[i]->getSequence()->getDataDefinition();
            if (mxf_is_picture(&data_def))
                num_video_tracks++;
            else if (mxf_is_sound(&data_def))
                num_audio_tracks++;
        }
        
        
        // create the tape source package
        
        tape_package = new SourcePackage(mHeaderMetadata);
        mHeaderMetadata->getPreface()->getContentStorage()->appendPackages(tape_package);
        tape_package->setPackageUID(tape_package_uid);
        tape_package->setPackageCreationDate(now);
        tape_package->setPackageModifiedDate(now);
        if (!name.empty())
            tape_package->setName(name);
        
        // Preface - ContentStorage - tape SourcePackage - Timeline Tracks
        // video track followed by audio tracks
        for (i = 0; i < num_video_tracks + num_audio_tracks; i++) {
            bool is_picture = (i < num_video_tracks);
            
            // Preface - ContentStorage - tape SourcePackage - Timeline Track
            Track *track = new Track(mHeaderMetadata);
            tape_package->appendTracks(track);
            track->setTrackName(is_picture ? get_track_name("V", i + 1) :
                                             get_track_name("A", i - num_video_tracks + 1));
            track->setTrackID(i + 1);
            track->setTrackNumber(0);
            track->setEditRate(EDIT_RATE);
            track->setOrigin(0);
            
            // Preface - ContentStorage - SourcePackage - Timeline Track - Sequence
            Sequence *sequence = new Sequence(mHeaderMetadata);
            track->setSequence(sequence);
            sequence->setDataDefinition(is_picture ? MXF_DDEF_L(Picture) : MXF_DDEF_L(Sound));
            sequence->setDuration(TAPE_LEN);
            
            // Preface - ContentStorage - SourcePackage - Timeline Track - Sequence - SourceClip
            SourceClip *source_clip = new SourceClip(mHeaderMetadata);
            sequence->appendStructuralComponents(source_clip);
            source_clip->setDataDefinition(is_picture ? MXF_DDEF_L(Picture) : MXF_DDEF_L(Sound));
            source_clip->setDuration(TAPE_LEN);
            source_clip->setStartPosition(0);
            source_clip->setSourceTrackID(0);
            source_clip->setSourcePackageID(g_Null_UMID);
        }
        
        // Preface - ContentStorage - tape SourcePackage - timecode Timeline Track
        Track *timecode_track = new Track(mHeaderMetadata);
        tape_package->appendTracks(timecode_track);
        timecode_track->setTrackName("TC1");
        timecode_track->setTrackID(num_video_tracks + num_audio_tracks + 1);
        timecode_track->setTrackNumber(0);
        timecode_track->setEditRate(EDIT_RATE);
        timecode_track->setOrigin(0);
        
        // Preface - ContentStorage - tape SourcePackage - timecode Timeline Track - Sequence
        Sequence *sequence = new Sequence(mHeaderMetadata);
        timecode_track->setSequence(sequence);
        sequence->setDataDefinition(MXF_DDEF_L(Timecode));
        sequence->setDuration(TAPE_LEN);
        
        // Preface - ContentStorage - tape SourcePackage - Timecode Track - TimecodeComponent
        TimecodeComponent *timecode_component = new TimecodeComponent(mHeaderMetadata);
        sequence->appendStructuralComponents(timecode_component);
        timecode_component->setDataDefinition(MXF_DDEF_L(Timecode));
        timecode_component->setDuration(TAPE_LEN);
        timecode_component->setRoundedTimecodeBase(25);
        timecode_component->setDropFrame(false);
        timecode_component->setStartTimecode(0);
        
        // Preface - ContentStorage - tape SourcePackage - TapeDescriptor
        GenericDescriptor *tape_descriptor = dynamic_cast<GenericDescriptor*>(
                                                mHeaderMetadata->createAndWrap(&MXF_SET_K(TapeDescriptor)));
        tape_package->setDescriptor(tape_descriptor);
        
        
        // connect the file source package to the tape source package
        
        for (i = 0; i < file_package_tracks.size(); i++) {
            Sequence *sequence = dynamic_cast<Sequence*>(file_package_tracks[i]->getSequence());
            REC_CHECK(sequence);
            mxfUL data_def = sequence->getDataDefinition();
            if (mxf_is_picture(&data_def) || mxf_is_sound(&data_def)) {
                if (mxf_is_picture(&data_def)) {
                    if (tape_video_track_id == 0)
                        tape_video_track_id = 1;
                    else
                        tape_video_track_id++;
                    tape_track_id = tape_video_track_id;
                } else {
                    if (tape_audio_track_id == 0)
                        tape_audio_track_id = num_video_tracks + 1;
                    else
                        tape_audio_track_id++;
                    tape_track_id = tape_audio_track_id;
                }
                
                REC_CHECK(sequence->getStructuralComponents().size() == 1);
                SourceClip *source_clip = dynamic_cast<SourceClip*>(sequence->getStructuralComponents()[0]);
                REC_CHECK(source_clip);
                
                source_clip->setSourcePackageID(tape_package_uid);
                source_clip->setSourceTrackID(tape_track_id);
            }
        }
    }
    
    return tape_package;
}

void MXFMetadataHelper::SetTapeSourceInfaxMetadata(InfaxData *infax_data)
{
    string tape_package_name;
    if (!is_empty_string(infax_data->spoolNo, true))
        tape_package_name = infax_data->spoolNo;
    
    SourcePackage *tape_package = GetTapeSourcePackage();
    if (!tape_package)
        tape_package = AddTapeSourcePackage(tape_package_name);
    
    SetInfaxMetadata(tape_package, infax_data);
}

void MXFMetadataHelper::AddInfaxMetadataAsComments(InfaxData *infax_data)
{
    MaterialPackage *material_package = GetMaterialPackage();
    REC_ASSERT(material_package);
    
    AddInfaxMetadataAsComments(material_package, infax_data);
}

MaterialPackage* MXFMetadataHelper::GetMaterialPackage()
{
    vector<GenericPackage*> packages = mHeaderMetadata->getPreface()->getContentStorage()->getPackages();
    
    size_t i;
    for (i = 0; i < packages.size(); i++) {
        MaterialPackage *material_package = dynamic_cast<MaterialPackage*>(packages[i]);
        if (material_package)
            return material_package;
    }
    
    return 0;
}

SourcePackage* MXFMetadataHelper::GetFileSourcePackage()
{
    vector<GenericPackage*> packages = mHeaderMetadata->getPreface()->getContentStorage()->getPackages();
    
    size_t i;
    for (i = 0; i < packages.size(); i++) {
        SourcePackage *source_package = dynamic_cast<SourcePackage*>(packages[i]);
        if (source_package &&
            source_package->haveDescriptor() &&
            dynamic_cast<FileDescriptor*>(source_package->getDescriptor()))
        {
            return source_package;
        }
    }
    
    return 0;
}

SourcePackage* MXFMetadataHelper::GetTapeSourcePackage()
{
    vector<GenericPackage*> packages = mHeaderMetadata->getPreface()->getContentStorage()->getPackages();
    
    size_t i;
    for (i = 0; i < packages.size(); i++) {
        SourcePackage *source_package = dynamic_cast<SourcePackage*>(packages[i]);
        if (source_package &&
            source_package->haveDescriptor() &&
            mDataModel->isSubclassOf(source_package->getDescriptor(), &MXF_SET_K(TapeDescriptor)))
        {
            return source_package;
        }
    }
    
    return 0;
}

void MXFMetadataHelper::SetArchiveDMScheme()
{
    // note: DMSchemes is a required property even if it is empty
    vector<mxfUL> dm_schemes = mHeaderMetadata->getPreface()->getDMSchemes();
    
    size_t i;
    for (i = 0; i < dm_schemes.size(); i++) {
        if (mxf_equals_ul(&dm_schemes[i], &MXF_DM_L(APP_PreservationDescriptiveScheme)))
            return;
    }
    
    mHeaderMetadata->getPreface()->appendDMSchemes(MXF_DM_L(APP_PreservationDescriptiveScheme));
}

void MXFMetadataHelper::SetInfaxMetadata(GenericPackage *package, InfaxData *infax_data)
{
    // set DM Scheme for Archive DM Scheme
    
    SetArchiveDMScheme();
    
    
    // find existing Infax framework
    
    DMFramework *infax_framework = 0;
    vector<GenericTrack*> tracks = package->getTracks();
    size_t i;
    for (i = 0; i < tracks.size(); i++) {
        if (!dynamic_cast<StaticTrack*>(tracks[i]))
            continue;
        
        Sequence *sequence = dynamic_cast<Sequence*>(tracks[i]->getSequence());
        if (!sequence || sequence->getStructuralComponents().size() != 1)
            continue;
        
        DMSegment *dm_segment = dynamic_cast<DMSegment*>(sequence->getStructuralComponents()[0]);
        if (!dm_segment || !dm_segment->haveDMFramework())
            continue;
        
        if (mDataModel->isSubclassOf(dm_segment->getDMFramework(), &MXF_SET_K(APP_InfaxFramework))) {
            infax_framework = dm_segment->getDMFramework();
            break;
        }
    }
    
    
    // create track with infax framework if necessary
    
    if (!infax_framework) {
        // Preface - ContentStorage - Package - DM Track
        StaticTrack *dm_track = new StaticTrack(mHeaderMetadata);
        package->appendTracks(dm_track);
        dm_track->setTrackName("DM1");
        dm_track->setTrackID(100);
        dm_track->setTrackNumber(0);
        
        // Preface - ContentStorage - Package - DM Track - Sequence
        Sequence *sequence = new Sequence(mHeaderMetadata);
        dm_track->setSequence(sequence);
        sequence->setDataDefinition(MXF_DDEF_L(DescriptiveMetadata));
        
        // Preface - ContentStorage - Package - DM Track - Sequence - DMSegment
        DMSegment *dm_segment = new DMSegment(mHeaderMetadata);
        sequence->appendStructuralComponents(dm_segment);
        dm_segment->setDataDefinition(MXF_DDEF_L(DescriptiveMetadata));
        
        // Preface - ContentStorage - Package - DM Track - Sequence - DMSegment - DMFramework
        infax_framework = dynamic_cast<DMFramework*>(mHeaderMetadata->createAndWrap(&MXF_SET_K(APP_InfaxFramework)));
        dm_segment->setDMFramework(infax_framework);
    }
 
    // set properties
    // TODO: create class for handling Infax data in the recorder
    
    mxfTimestamp date_only;
    
#define SET_INFAX_STRING_ITEM(name, member, size, is_symbol) \
    if (!is_empty_string(infax_data->member, is_symbol)) { \
        infax_framework->setFixedSizeStringItem(&MXF_ITEM_K(APP_InfaxFramework, name), \
                                                infax_data->member, \
                                                size); \
    } else if (infax_framework->haveItem(&MXF_ITEM_K(APP_InfaxFramework, name))) { \
        infax_framework->removeItem(&MXF_ITEM_K(APP_InfaxFramework, name)); \
    }
    
    SET_INFAX_STRING_ITEM(APP_Format, format, FORMAT_SIZE, true);
    SET_INFAX_STRING_ITEM(APP_ProgrammeTitle, progTitle, PROGTITLE_SIZE, false);
    SET_INFAX_STRING_ITEM(APP_EpisodeTitle, epTitle, EPTITLE_SIZE, false);
    SET_INFAX_STRING_ITEM(APP_MagazinePrefix, magPrefix, MAGPREFIX_SIZE, true);
    SET_INFAX_STRING_ITEM(APP_ProgrammeNumber, progNo, PROGNO_SIZE, true);
    SET_INFAX_STRING_ITEM(APP_ProductionCode, prodCode, PRODCODE_SIZE, true);
    SET_INFAX_STRING_ITEM(APP_SpoolStatus, spoolStatus, SPOOLSTATUS_SIZE, true);
    SET_INFAX_STRING_ITEM(APP_SpoolDescriptor, spoolDesc, SPOOLDESC_SIZE, false);
    SET_INFAX_STRING_ITEM(APP_Memo, memo, MEMO_SIZE, false);
    SET_INFAX_STRING_ITEM(APP_SpoolNumber, spoolNo, SPOOLNO_SIZE, true);
    SET_INFAX_STRING_ITEM(APP_AccessionNumber, accNo, ACCNO_SIZE, true);
    SET_INFAX_STRING_ITEM(APP_CatalogueDetail, catDetail, CATDETAIL_SIZE, true);


#define SET_INFAX_DATE_ITEM(name, member) \
    if (infax_data->member.month != INVALID_MONTH_VALUE) { \
        /* only the date part is relevant */ \
        date_only = infax_data->member; \
        date_only.hour = 0; \
        date_only.min = 0; \
        date_only.sec = 0; \
        date_only.qmsec = 0; \
        infax_framework->setTimestampItem(&MXF_ITEM_K(APP_InfaxFramework, name), date_only); \
    } else if (infax_framework->haveItem(&MXF_ITEM_K(APP_InfaxFramework, name))) { \
        infax_framework->removeItem(&MXF_ITEM_K(APP_InfaxFramework, name)); \
    }
    
    SET_INFAX_DATE_ITEM(APP_TransmissionDate, txDate);
    SET_INFAX_DATE_ITEM(APP_StockDate, stockDate);


#define SET_INFAX_SIMPLE_ITEM(name, member, invalid_value, method) \
    if (infax_data->member != invalid_value) { \
        infax_framework->method(&MXF_ITEM_K(APP_InfaxFramework, name), infax_data->member); \
    } else if (infax_framework->haveItem(&MXF_ITEM_K(APP_InfaxFramework, name))) { \
        infax_framework->removeItem(&MXF_ITEM_K(APP_InfaxFramework, name)); \
    }
    
    SET_INFAX_SIMPLE_ITEM(APP_Duration, duration, INVALID_DURATION_VALUE, setInt64Item);
    SET_INFAX_SIMPLE_ITEM(APP_ItemNumber, itemNo, INVALID_ITEM_NO, setUInt32Item);
}

void MXFMetadataHelper::AddInfaxMetadataAsComments(GenericPackage *package, InfaxData *infax_data)
{
    // set DM Scheme for Archive DM Scheme
    
    SetArchiveDMScheme();

    
    // set properties as user comments
    
    char buffer[64];
    
#define ADD_INFAX_STRING_COMMENT(name, member, size, is_symbol) \
    if (!is_empty_string(infax_data->member, is_symbol)) { \
        package->attachAvidUserComment("_BBC_" #name, infax_data->member); \
    }
    
    ADD_INFAX_STRING_COMMENT(APP_Format, format, FORMAT_SIZE, true);
    ADD_INFAX_STRING_COMMENT(APP_ProgrammeTitle, progTitle, PROGTITLE_SIZE, false);
    ADD_INFAX_STRING_COMMENT(APP_EpisodeTitle, epTitle, EPTITLE_SIZE, false);
    ADD_INFAX_STRING_COMMENT(APP_MagazinePrefix, magPrefix, MAGPREFIX_SIZE, true);
    ADD_INFAX_STRING_COMMENT(APP_ProgrammeNumber, progNo, PROGNO_SIZE, true);
    ADD_INFAX_STRING_COMMENT(APP_ProductionCode, prodCode, PRODCODE_SIZE, true);
    ADD_INFAX_STRING_COMMENT(APP_SpoolStatus, spoolStatus, SPOOLSTATUS_SIZE, true);
    ADD_INFAX_STRING_COMMENT(APP_SpoolDescriptor, spoolDesc, SPOOLDESC_SIZE, false);
    ADD_INFAX_STRING_COMMENT(APP_Memo, memo, MEMO_SIZE, false);
    ADD_INFAX_STRING_COMMENT(APP_SpoolNumber, spoolNo, SPOOLNO_SIZE, true);
    ADD_INFAX_STRING_COMMENT(APP_AccessionNumber, accNo, ACCNO_SIZE, true);
    ADD_INFAX_STRING_COMMENT(APP_CatalogueDetail, catDetail, CATDETAIL_SIZE, true);


#define ADD_INFAX_DATE_COMMENT(name, member) \
    if (infax_data->member.month != INVALID_MONTH_VALUE) { \
        /* only the date part is relevant */ \
        sprintf(buffer, "%04d-%02d-%02d", infax_data->member.year, infax_data->member.month, infax_data->member.day); \
        package->attachAvidUserComment("_BBC_" #name, buffer); \
    }
    
    ADD_INFAX_DATE_COMMENT(APP_TransmissionDate, txDate);
    ADD_INFAX_DATE_COMMENT(APP_StockDate, stockDate);


#define ADD_INFAX_INTEGER_COMMENT(name, member, invalid_value, format) \
    if (infax_data->member != invalid_value) { \
        sprintf(buffer, format, infax_data->member); \
        package->attachAvidUserComment("_BBC_" #name, buffer); \
    }
    
    ADD_INFAX_INTEGER_COMMENT(APP_Duration, duration, INVALID_DURATION_VALUE, "%"PRIi64);
    ADD_INFAX_INTEGER_COMMENT(APP_ItemNumber, itemNo, INVALID_ITEM_NO, "%u");
}

