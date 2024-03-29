/*
 * $Id: IngexMXFInfo.cpp,v 1.7 2011/01/10 16:59:19 john_f Exp $
 *
 * Extract information from Ingex MXF files.
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
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

// TODO:
// Missing info:
//   tape source package spoolNumber
// Source code improvements:
//   add avid extensions data model header file to AvidHeaderMetadata
//   move Avid user comments and attributes to libMXF


#include <cstdio>
#include <cstring>
#include <cassert>
#include <memory>

#include <libMXF++/MXF.h>
#include <libMXF++/extensions/TaggedValue.h>

#include <mxf/mxf_avid.h>

#include <Package.h>
#include <MaterialResolution.h>
#include <Logging.h>
#include <ProdAutoException.h>

#include "IngexMXFInfo.h"

using namespace mxfpp;
using namespace std;



#define ASSERT_CHECK(cmd) \
    if (!(cmd)) { \
        prodauto::Logging::error("'%s' failed, at %s:%d\n", #cmd, __FILE__, __LINE__); \
        throw FAILED; \
    }

// these colors match the colors defined in studio/database/src/DatabaseEnums.h and
// AvidRGBColor in libMXF/examples/writeavidmxf/package_definitions.h
static const RGBColor RGB_COLORS[] =
{
    {65534, 65535, 65535}, // white
    {41471, 12134, 6564 }, // red
    {58981, 58981, 6553 }, // yellow
    {13107, 52428, 13107}, // green
    {13107, 52428, 52428}, // cyan
    {13107, 13107, 52428}, // blue
    {52428, 13107, 52428}, // magenta
    {0    , 0    , 0    }  // black
};

#define RGB_COLORS_SIZE     (sizeof(RGB_COLORS) / sizeof(RGBColor))


static const char *ERROR_STRINGS[] =
{
    "success",
    "general error",
    "could not open for reading",
    "no header partition found",
    "file is not OP-Atom",
    "error reading header metadata",
    "unknown project edit rate",
};

#define ERROR_STRINGS_SIZE  (sizeof(ERROR_STRINGS) / sizeof(char*))


typedef struct
{
    const mxfUL container_label;
    int ingex_resolution_id;
} ECLabelToIngexResolutionMap;

static const ECLabelToIngexResolutionMap EC_LABEL_RESOLUTION_ATOM_MAP[] =
{
    {MXF_EC_L(D10_50_625_50_defined_template),      MaterialResolution::IMX50_MXF_ATOM},
    {MXF_EC_L(D10_50_625_50_extended_template),     MaterialResolution::IMX50_MXF_ATOM},
    {MXF_EC_L(D10_50_625_50_picture_only),          MaterialResolution::IMX50_MXF_ATOM},
    {MXF_EC_L(D10_50_525_60_defined_template),      MaterialResolution::IMX50_MXF_ATOM},
    {MXF_EC_L(D10_50_525_60_extended_template),     MaterialResolution::IMX50_MXF_ATOM},
    {MXF_EC_L(D10_50_525_60_picture_only),          MaterialResolution::IMX50_MXF_ATOM},
    
    {MXF_EC_L(D10_40_625_50_defined_template),      MaterialResolution::IMX40_MXF_ATOM},
    {MXF_EC_L(D10_40_625_50_extended_template),     MaterialResolution::IMX40_MXF_ATOM},
    {MXF_EC_L(D10_40_625_50_picture_only),          MaterialResolution::IMX40_MXF_ATOM},
    {MXF_EC_L(D10_40_525_60_defined_template),      MaterialResolution::IMX40_MXF_ATOM},
    {MXF_EC_L(D10_40_525_60_extended_template),     MaterialResolution::IMX40_MXF_ATOM},
    {MXF_EC_L(D10_40_525_60_picture_only),          MaterialResolution::IMX40_MXF_ATOM},

    {MXF_EC_L(D10_30_625_50_defined_template),      MaterialResolution::IMX30_MXF_ATOM},
    {MXF_EC_L(D10_30_625_50_extended_template),     MaterialResolution::IMX30_MXF_ATOM},
    {MXF_EC_L(D10_30_625_50_picture_only),          MaterialResolution::IMX30_MXF_ATOM},
    {MXF_EC_L(D10_30_525_60_defined_template),      MaterialResolution::IMX30_MXF_ATOM},
    {MXF_EC_L(D10_30_525_60_extended_template),     MaterialResolution::IMX30_MXF_ATOM},
    {MXF_EC_L(D10_30_525_60_picture_only),          MaterialResolution::IMX30_MXF_ATOM},

    {MXF_EC_L(IECDV_25_525_60_ClipWrapped),         MaterialResolution::DV25_MXF_ATOM},
    {MXF_EC_L(IECDV_25_525_60_FrameWrapped),        MaterialResolution::DV25_MXF_ATOM},
    {MXF_EC_L(IECDV_25_625_50_ClipWrapped),         MaterialResolution::DV25_MXF_ATOM},
    {MXF_EC_L(IECDV_25_625_50_FrameWrapped),        MaterialResolution::DV25_MXF_ATOM},
    {MXF_EC_L(DVBased_25_525_60_ClipWrapped),       MaterialResolution::DV25_MXF_ATOM},
    {MXF_EC_L(DVBased_25_525_60_FrameWrapped),      MaterialResolution::DV25_MXF_ATOM},
    {MXF_EC_L(DVBased_25_625_50_ClipWrapped),       MaterialResolution::DV25_MXF_ATOM},
    {MXF_EC_L(DVBased_25_625_50_FrameWrapped),      MaterialResolution::DV25_MXF_ATOM},

    {MXF_EC_L(DVBased_50_525_60_ClipWrapped),       MaterialResolution::DV50_MXF_ATOM},
    {MXF_EC_L(DVBased_50_525_60_FrameWrapped),      MaterialResolution::DV50_MXF_ATOM},
    {MXF_EC_L(DVBased_50_625_50_ClipWrapped),       MaterialResolution::DV50_MXF_ATOM},
    {MXF_EC_L(DVBased_50_625_50_FrameWrapped),      MaterialResolution::DV50_MXF_ATOM},

    {MXF_EC_L(DVBased_100_720_50_P_ClipWrapped),    MaterialResolution::DV100_MXF_ATOM},
    {MXF_EC_L(DVBased_100_1080_50_I_ClipWrapped),   MaterialResolution::DV100_MXF_ATOM},
    
    {MXF_EC_L(HD_Unc_1080_50i_422_ClipWrapped),     MaterialResolution::UNC_MXF_ATOM},
    {MXF_EC_L(SD_Unc_625_50i_422_135_ClipWrapped),  MaterialResolution::UNC_MXF_ATOM},
    
    {MXF_EC_L(DNxHD1080i185ClipWrapped),            MaterialResolution::DNX185I_MXF_ATOM},
    
    {MXF_EC_L(DNxHD1080p185ClipWrapped),            MaterialResolution::DNX185P_MXF_ATOM},
    {MXF_EC_L(DNxHD720p185ClipWrapped),             MaterialResolution::DNX185P_MXF_ATOM},
    
    {MXF_EC_L(DNxHD1080i120ClipWrapped),            MaterialResolution::DNX120I_MXF_ATOM},
    
    {MXF_EC_L(DNxHD1080p120ClipWrapped),            MaterialResolution::DNX120P_MXF_ATOM},
    {MXF_EC_L(DNxHD720p120ClipWrapped),             MaterialResolution::DNX120P_MXF_ATOM},
    
    {MXF_EC_L(DNxHD1080p36ClipWrapped),             MaterialResolution::DNX36P_MXF_ATOM},
};

#define EC_LABEL_RESOLUTION_ATOM_MAP_SIZE    (sizeof(EC_LABEL_RESOLUTION_ATOM_MAP) / sizeof(ECLabelToIngexResolutionMap))


typedef struct
{
    const mxfUL compression_label;
    int ingex_resolution_id;
} CompLabelToIngexResolutionMap;

static const CompLabelToIngexResolutionMap COMP_LABEL_RESOLUTION_ATOM_MAP[] =
{
    {MXF_CMDEF_L(AvidMJPEG21_PAL),                     MaterialResolution::MJPEG21_MXF_ATOM},
    {MXF_CMDEF_L(AvidMJPEG21_NTSC),                    MaterialResolution::MJPEG21_MXF_ATOM},
    
    {MXF_CMDEF_L(AvidMJPEG31_PAL),                     MaterialResolution::MJPEG31_MXF_ATOM},
    {MXF_CMDEF_L(AvidMJPEG31_NTSC),                    MaterialResolution::MJPEG31_MXF_ATOM},
    
    {MXF_CMDEF_L(AvidMJPEG101_PAL),                    MaterialResolution::MJPEG101_MXF_ATOM},
    {MXF_CMDEF_L(AvidMJPEG101_NTSC),                   MaterialResolution::MJPEG101_MXF_ATOM},
    
    {MXF_CMDEF_L(AvidMJPEG101m_PAL),                   MaterialResolution::MJPEG101M_MXF_ATOM},
    {MXF_CMDEF_L(AvidMJPEG101m_NTSC),                  MaterialResolution::MJPEG101M_MXF_ATOM},
    
    {MXF_CMDEF_L(AvidMJPEG151s_PAL),                   MaterialResolution::MJPEG151S_MXF_ATOM},
    {MXF_CMDEF_L(AvidMJPEG151s_NTSC),                  MaterialResolution::MJPEG151S_MXF_ATOM},
    
    {MXF_CMDEF_L(AvidMJPEG201_PAL),                    MaterialResolution::MJPEG201_MXF_ATOM},
    {MXF_CMDEF_L(AvidMJPEG201_NTSC),                   MaterialResolution::MJPEG201_MXF_ATOM},
};

#define COMP_LABEL_RESOLUTION_ATOM_MAP_SIZE  (sizeof(COMP_LABEL_RESOLUTION_ATOM_MAP) / sizeof(CompLabelToIngexResolutionMap))


typedef struct
{
    int32_t avid_resolution_id;
    int ingex_resolution_id;
} ResolutionMap;

static const ResolutionMap RESOLUTION_ATOM_MAP[] =
{
    {g_AvidMJPEG21_ResolutionID,                    MaterialResolution::MJPEG21_MXF_ATOM},
    {g_AvidMJPEG31_ResolutionID,                    MaterialResolution::MJPEG31_MXF_ATOM},
    {g_AvidMJPEG101_ResolutionID,                   MaterialResolution::MJPEG101_MXF_ATOM},
    {g_AvidMJPEG101m_ResolutionID,                  MaterialResolution::MJPEG101M_MXF_ATOM},
    {g_AvidMJPEG151s_ResolutionID,                  MaterialResolution::MJPEG151S_MXF_ATOM},
    {g_AvidMJPEG201_ResolutionID,                   MaterialResolution::MJPEG201_MXF_ATOM},
};

#define RESOLUTION_ATOM_MAP_SIZE     (sizeof(RESOLUTION_ATOM_MAP) / sizeof(ResolutionMap))




void convert_umid(mxfUMID mxf_umid, prodauto::UMID *umid)
{
    memcpy(umid, &mxf_umid, sizeof(*umid));
}

void convert_timestamp(mxfTimestamp mxf_timestamp, prodauto::Timestamp *timestamp)
{
    memcpy(timestamp, &mxf_timestamp, sizeof(*timestamp));
}

void convert_rational(mxfRational mxf_rational, prodauto::Rational *rational)
{
    rational->numerator = mxf_rational.numerator;
    rational->denominator = mxf_rational.denominator;
}

void convert_data_def(mxfUL data_def_ul, int *data_def)
{
    if (mxf_is_picture(&data_def_ul))
        *data_def = PICTURE_DATA_DEFINITION;
    else if (mxf_is_sound(&data_def_ul))
        *data_def = SOUND_DATA_DEFINITION;
    else
        *data_def = 0;
}

void convert_color(const RGBColor *color, int *db_color)
{
    size_t i;
    float diff[RGB_COLORS_SIZE];
    float min_diff = -1;
    int min_diff_index = 0;
    
    assert(USER_COMMENT_BLACK_COLOUR - USER_COMMENT_WHITE_COLOUR == RGB_COLORS_SIZE - 1);
    
    // choose the color that has minimum difference to a known color
    
    for (i = 0; i < RGB_COLORS_SIZE; i++) {
        diff[i] = ((float)(color->red - RGB_COLORS[i].red) * (float)(color->red - RGB_COLORS[i].red) +
                   (float)(color->green - RGB_COLORS[i].green) * (float)(color->green - RGB_COLORS[i].green) +
                   (float)(color->blue - RGB_COLORS[i].blue) * (float)(color->blue - RGB_COLORS[i].blue)) / 3.0;
    }
    
    for (i = 0; i < RGB_COLORS_SIZE; i++) {
        if (min_diff < 0 || diff[i] < min_diff) {
            min_diff = diff[i];
            min_diff_index = i;
        }
    }
    
    *db_color = min_diff_index + USER_COMMENT_WHITE_COLOUR;
}




IngexMXFInfo::ReadResult IngexMXFInfo::read(string filename, IngexMXFInfo **info)
{
    ReadResult result = SUCCESS;
    File *mxf_file = 0;
    Partition *header_partition = 0;
    AvidHeaderMetadata *header_metadata = 0;
    DataModel *data_model = 0;
    mxfKey key;
    uint8_t llen;
    uint64_t len;
    vector<mxfUL> container_labels;

    try
    {
        // open the file
        try
        {
            mxf_file = File::openRead(filename);
        }
        catch (...)
        {
            throw FILE_OPEN_ERROR;
        }
        
        // read the header partition pack and check the operational pattern
        header_partition = Partition::findAndReadHeaderPartition(mxf_file);
        if (!header_partition)
            throw NO_HEADER_PARTITION;

        if (!is_op_atom(header_partition->getOperationalPattern()))
            throw NOT_OP_ATOM;

        // essence container label
        container_labels = header_partition->getEssenceContainers();
        ASSERT_CHECK(container_labels.size() == 1);

        // initialise data model with Avid extensions
        data_model = new DataModel();
        header_metadata = new AvidHeaderMetadata(data_model);
        TaggedValue::registerObjectFactory(header_metadata);

        // move to the header metadata and read it
        mxf_file->readNextNonFillerKL(&key, &llen, &len);
        if (!mxf_is_header_metadata(&key))
            throw HEADER_ERROR;
        header_metadata->read(mxf_file, header_partition, &key, llen, len);
        
        
        result = read(filename, &container_labels[0], header_metadata, data_model, info);
    }
    catch (ReadResult &error)
    {
        result = error;
    }
    catch (...)
    {
        result = FAILED;
    }

    delete header_partition;
    delete header_metadata;
    delete data_model;
    delete mxf_file;
    
    return result;
}

IngexMXFInfo::ReadResult IngexMXFInfo::read(string filename, mxfUL *essence_container_label,
                                            HeaderMetadata *header_metadata, DataModel *data_model,
                                            IngexMXFInfo **info)
{
    ReadResult result = SUCCESS;

    try
    {
        *info = new IngexMXFInfo(filename, essence_container_label, header_metadata, data_model);
    }
    catch (ReadResult &error)
    {
        result = error;
    }
    catch (...)
    {
        result = FAILED;
    }

    return result;
}

string IngexMXFInfo::errorToString(ReadResult result)
{
    size_t index = (size_t)(-1 * (int)result);
    PA_ASSERT(index < ERROR_STRINGS_SIZE);
    
    return ERROR_STRINGS[index];
}



IngexMXFInfo::IngexMXFInfo(string filename, mxfUL *essence_container_label, HeaderMetadata *header_metadata,
                           DataModel *data_model)
{
    _filename = filename;
    _data_model = data_model;
    _package_group = 0;
    
    
    try
    {
        Preface *preface = header_metadata->getPreface();
        ContentStorage *content = preface->getContentStorage();
        vector<GenericPackage*> packages = content->getPackages();

        // determine the project edit rate
        prodauto::Rational project_edit_rate;
        if (preface->haveItem(&MXF_ITEM_K(Preface, ProjectEditRate))) {
            convert_rational(preface->getRationalItem(&MXF_ITEM_K(Preface, ProjectEditRate)), &project_edit_rate);
        } else {
            // get the project edit rate from a video track in the material package or tape source package
            project_edit_rate = prodauto::g_palEditRate; // default to PAL if no video tracks present
            vector<GenericPackage*>::iterator package_iter;
            for (package_iter = packages.begin(); package_iter != packages.end(); package_iter++) {
                if (extractProjectEditRate(*package_iter, &project_edit_rate))
                    break;
            }
        }
        
        if (project_edit_rate != prodauto::g_palEditRate
            && project_edit_rate == prodauto::g_ntscEditRate)
        {
            throw UNKNOWN_PROJECT_EDIT_RATE;
        }
        
        _package_group = new prodauto::PackageGroup(project_edit_rate, OperationalPattern::OP_ATOM);
        
        
        // extract package info
        vector<GenericPackage*>::iterator package_iter;
        for (package_iter = packages.begin(); package_iter != packages.end(); package_iter++)
            extractPackageInfo(*package_iter, essence_container_label);
    }
    catch (...)
    {
        delete _package_group;
        throw;
    }
}

IngexMXFInfo::~IngexMXFInfo()
{
    delete _package_group;
}

bool IngexMXFInfo::extractProjectEditRate(mxfpp::GenericPackage *mxf_package, prodauto::Rational *project_edit_rate)
{
    // check that it is a material or tape source package
    MaterialPackage *mp = dynamic_cast<MaterialPackage*>(mxf_package);
    SourcePackage *sp = dynamic_cast<SourcePackage*>(mxf_package);
    if (!mp && !sp) {
        return false;
    } else if (sp) {
        if (!sp->haveDescriptor() ||
            !_data_model->isSubclassOf(sp->getDescriptor(), &MXF_SET_K(TapeDescriptor)))
        {
            return false;
        }
    }

    vector<GenericTrack*> mxf_tracks = mxf_package->getTracks();
    Track *mxf_track;
    Sequence *mxf_sequence;
    int data_def;
    vector<GenericTrack*>::iterator track_iter;
    for (track_iter = mxf_tracks.begin(); track_iter != mxf_tracks.end(); track_iter++) {
        // a timeline track
        mxf_track = dynamic_cast<Track*>(*track_iter);
        if (!mxf_track)
            continue;

        // containing a sequence
        mxf_sequence = dynamic_cast<Sequence*>(mxf_track->getSequence());
        if (!mxf_sequence)
            continue;

        // which is picture
        convert_data_def(mxf_sequence->getDataDefinition(), &data_def);
        if (data_def != PICTURE_DATA_DEFINITION)
            continue;
        
        convert_rational(mxf_track->getEditRate(), project_edit_rate);
        return true;
    }
    
    return false;
}

void IngexMXFInfo::extractPackageInfo(GenericPackage *mxf_package, mxfUL *essence_container_label)
{
    MaterialPackage *mp = dynamic_cast<MaterialPackage*>(mxf_package);
    SourcePackage *sp = dynamic_cast<SourcePackage*>(mxf_package);
    bool is_mp = false;
    bool is_tape_sp = false;
    bool is_file_sp = false;
    prodauto::Package *db_package;

    // find out what type of package it is
    if (mp) {
        is_mp = true;
    } else if (sp) {
        if (!sp->haveDescriptor())
            return;
        
        if (_data_model->isSubclassOf(sp->getDescriptor(), &MXF_SET_K(TapeDescriptor)))
            is_tape_sp = true;
        else if (_data_model->isSubclassOf(sp->getDescriptor(), &MXF_SET_K(FileDescriptor)))
            is_file_sp = true;
        else
            return;
    }
    
    if (is_mp) {
        _package_group->SetMaterialPackage(new prodauto::MaterialPackage());
        db_package = _package_group->GetMaterialPackage();
    } else if (is_file_sp) {
        _package_group->AppendFileSourcePackage(new prodauto::SourcePackage());
        db_package = getCurrentFileSourcePackage();
    } else {
        _package_group->SetTapeSourcePackage(new prodauto::SourcePackage(), true);
        db_package = _package_group->GetTapeSourcePackage();
    }
    
    // extract general package info
    
    convert_umid(mxf_package->getPackageUID(), &db_package->uid);
    if (mxf_package->haveName())
        db_package->name = mxf_package->getName();
    convert_timestamp(mxf_package->getPackageCreationDate(), &db_package->creationDate);

    vector<TaggedValue*> attributes = getPackageAttributes(mxf_package);
    db_package->projectName = getStringTaggedValue(attributes, "_PJ");

    vector<TaggedValue*> comments = getPackageComments(mxf_package);
    vector<TaggedValue*>::iterator comment_iter;
    for (comment_iter = comments.begin(); comment_iter != comments.end(); comment_iter++)
        db_package->addUserComment((*comment_iter)->getName(), (*comment_iter)->getStringValue(),
                                   STATIC_COMMENT_POSITION, 0);

    
    vector<GenericTrack*> mxf_tracks = mxf_package->getTracks();
    Track *mxf_track;
    prodauto::Track *track;
    Sequence *mxf_sequence;
    int data_def;
    vector<StructuralComponent*> mxf_components;
    SourceClip *mxf_source_clip;
    vector<GenericTrack*>::iterator track_iter;
    for (track_iter = mxf_tracks.begin(); track_iter != mxf_tracks.end(); track_iter++) {
        
        // a timeline track
        mxf_track = dynamic_cast<Track*>(*track_iter);
        if (!mxf_track)
            continue;

        // containing a sequence referencing a single source clip
        mxf_sequence = dynamic_cast<Sequence*>(mxf_track->getSequence());
        if (!mxf_sequence)
            continue;
        mxf_components = mxf_sequence->getStructuralComponents();
        if (mxf_components.size() != 1)
            continue;
        mxf_source_clip = dynamic_cast<SourceClip*>(mxf_components.front());
        if (!mxf_source_clip)
            continue;

        // which is either sound or picture        
        convert_data_def(mxf_sequence->getDataDefinition(), &data_def);
        if (data_def == 0)
            continue;

        // map to prodauto data structures
        
        mxf_track = dynamic_cast<Track*>(*track_iter);
        db_package->tracks.push_back(new prodauto::Track());
        track = db_package->tracks.back();
        
        ASSERT_CHECK(mxf_track->haveTrackID());
        track->id = mxf_track->getTrackID();
        track->number = mxf_track->getTrackNumber();
        if (mxf_track->haveTrackName())
            track->name = mxf_track->getTrackName();
        track->origin = mxf_track->getOrigin();
        convert_rational(mxf_track->getEditRate(), &track->editRate);
        track->dataDef = data_def;
        
        track->sourceClip = new prodauto::SourceClip();
        convert_umid(mxf_source_clip->getSourcePackageID(), &track->sourceClip->sourcePackageUID);
        track->sourceClip->sourceTrackID = mxf_source_clip->getSourceTrackID();
        track->sourceClip->length = mxf_source_clip->getDuration();
        track->sourceClip->position = mxf_source_clip->getStartPosition();
    }
    
    // extract info specific to package type
    if (is_mp)
        extractMaterialPackageInfo(mp);
    else if (is_file_sp)
        extractFileSourcePackageInfo(sp, essence_container_label);
    else if (is_tape_sp)
        extractTapeSourcePackageInfo(sp);
}

void IngexMXFInfo::extractMaterialPackageInfo(MaterialPackage *mxf_package)
{
    vector<GenericTrack*> mxf_tracks;
    EventTrack *mxf_event_track;
    Sequence *mxf_sequence;
    DMSegment *mxf_dm_segment;
    vector<StructuralComponent*> mxf_components;
    mxfUL mxf_data_def;
    string comment;
    int64_t position;
    RGBColor mxf_color;
    int color;
    
    // op atom
    _package_group->GetMaterialPackage()->op = OperationalPattern::OP_ATOM;
    
    // extract positioned user comments from the DM event track
    
    mxf_tracks = mxf_package->getTracks();
    vector<GenericTrack*>::iterator track_iter;    
    for (track_iter = mxf_tracks.begin(); track_iter != mxf_tracks.end(); track_iter++) {
        
        // an event track
        mxf_event_track = dynamic_cast<EventTrack*>(*track_iter);
        if (!mxf_event_track)
            continue;

        // containing a descriptive metadata sequence
        mxf_sequence = dynamic_cast<Sequence*>(mxf_event_track->getSequence());
        if (!mxf_sequence)
            continue;
        mxf_data_def = mxf_sequence->getDataDefinition();
        if (!mxf_is_descriptive_metadata(&mxf_data_def))
            continue;

        // extract dm segments and map to database structures
        mxf_components = mxf_sequence->getStructuralComponents();
        vector<StructuralComponent*>::iterator comp_iter;
        for (comp_iter = mxf_components.begin(); comp_iter != mxf_components.end(); comp_iter++) {
            
            // DMSegment
            mxf_dm_segment = dynamic_cast<DMSegment*>(*comp_iter);
            if (!mxf_dm_segment)
                continue;

            // map to database user comment
            
            position = mxf_dm_segment->getEventStartPosition();
            if (mxf_dm_segment->haveEventComment())
                comment = mxf_dm_segment->getEventComment();
            else
                comment = "";
            if (mxf_dm_segment->haveItem(&MXF_ITEM_K(DMSegment, CommentMarkerColor))) {
                MXFPP_CHECK(mxf_avid_get_rgb_color_item(mxf_dm_segment->getCMetadataSet(),
                                                        &MXF_ITEM_K(DMSegment, CommentMarkerColor), &mxf_color));
                convert_color(&mxf_color, &color);
            } else {
                color = USER_COMMENT_RED_COLOUR;
            }

            _package_group->GetMaterialPackage()->addUserComment(POSITIONED_COMMENT_NAME, comment, position, color);
        }
    }
}

void IngexMXFInfo::extractFileSourcePackageInfo(SourcePackage *mxf_package, mxfUL *essence_container_label)
{
    prodauto::FileEssenceDescriptor *descriptor;
    FileDescriptor *mxf_descriptor;
    GenericPictureEssenceDescriptor *mxf_picture_descriptor;
    GenericSoundEssenceDescriptor *mxf_sound_descriptor;
    int32_t avid_resolution_id = 0;
    mxfUL picture_essence_coding;
    
    memset(&picture_essence_coding, 0, sizeof(picture_essence_coding));

    mxf_descriptor = dynamic_cast<FileDescriptor*>(mxf_package->getDescriptor());
    
    descriptor = new prodauto::FileEssenceDescriptor();
    getCurrentFileSourcePackage()->descriptor = descriptor;
    
    descriptor->fileLocation = _filename;
    descriptor->fileFormat = FileFormat::MXF;
    
    vector<prodauto::UserComment> source_comments =
        _package_group->GetMaterialPackage()->getUserComments(AVID_UC_SOURCE_NAME);
    if (!source_comments.empty())
        getCurrentFileSourcePackage()->sourceConfigName = source_comments[0].value;
    else
        prodauto::Logging::warning("Missing Ingex '%s' (source config name) user comment\n", AVID_UC_SOURCE_NAME);
    
    mxf_picture_descriptor = dynamic_cast<GenericPictureEssenceDescriptor*>(mxf_descriptor);
    mxf_sound_descriptor = dynamic_cast<GenericSoundEssenceDescriptor*>(mxf_descriptor);

    if (mxf_picture_descriptor) {
        if (mxf_descriptor->haveItem(&MXF_ITEM_K(GenericPictureEssenceDescriptor, ResolutionID)))
            avid_resolution_id = mxf_descriptor->getInt32Item(&MXF_ITEM_K(GenericPictureEssenceDescriptor,
                                                              ResolutionID));
        if (mxf_picture_descriptor->havePictureEssenceCoding())
            picture_essence_coding = mxf_picture_descriptor->getPictureEssenceCoding();
        
        descriptor->videoResolutionID = getVideoResolutionId(essence_container_label, &picture_essence_coding,
                                                             avid_resolution_id);
        
        if (mxf_picture_descriptor->haveAspectRatio())
            convert_rational(mxf_picture_descriptor->getAspectRatio(), &descriptor->imageAspectRatio);
        
        if (mxf_picture_descriptor->haveStoredWidth())
            descriptor->storedWidth = mxf_picture_descriptor->getStoredWidth();
        else
            descriptor->storedWidth = 0;
        if (mxf_picture_descriptor->haveStoredHeight())
            descriptor->storedHeight = mxf_picture_descriptor->getStoredHeight();
        else
            descriptor->storedHeight = 0;
        
    } else if (mxf_sound_descriptor) {
        if (mxf_sound_descriptor->haveQuantizationBits())
            descriptor->audioQuantizationBits = mxf_sound_descriptor->getQuantizationBits();
    }
}

void IngexMXFInfo::extractTapeSourcePackageInfo(SourcePackage *mxf_package)
{
    prodauto::TapeEssenceDescriptor *descriptor;

    descriptor = new prodauto::TapeEssenceDescriptor();
    _package_group->GetTapeSourcePackage()->descriptor = descriptor;
}

vector<TaggedValue*> IngexMXFInfo::getPackageAttributes(GenericPackage *mxf_package)
{
    vector<TaggedValue*> result;

    if (!mxf_package->haveItem(&MXF_ITEM_K(GenericPackage, MobAttributeList)))
        return result;
    
    auto_ptr<ObjectIterator> iter(mxf_package->getStrongRefArrayItem(&MXF_ITEM_K(GenericPackage, MobAttributeList)));
    while (iter->next()) {
        MXFPP_CHECK(dynamic_cast<TaggedValue*>(iter->get()));
        result.push_back(dynamic_cast<TaggedValue*>(iter->get()));
    }
    
    return result;
}

vector<TaggedValue*> IngexMXFInfo::getPackageComments(GenericPackage *mxf_package)
{
    vector<TaggedValue*> result;

    if (!mxf_package->haveItem(&MXF_ITEM_K(GenericPackage, UserComments)))
        return result;
    
    auto_ptr<ObjectIterator> iter(mxf_package->getStrongRefArrayItem(&MXF_ITEM_K(GenericPackage, UserComments)));
    while (iter->next()) {
        MXFPP_CHECK(dynamic_cast<TaggedValue*>(iter->get()));
        result.push_back(dynamic_cast<TaggedValue*>(iter->get()));
    }
    
    return result;
}

string IngexMXFInfo::getStringTaggedValue(vector<TaggedValue*>& tagged_values, string name)
{
    vector<TaggedValue*>::iterator iter;
    for (iter = tagged_values.begin(); iter != tagged_values.end(); iter++) {
        if ((*iter)->getName() == name) {
            if ((*iter)->isStringValue())
                return (*iter)->getStringValue();
            else
                return "";
        }
    }
    
    return "";
}

int IngexMXFInfo::getVideoResolutionId(mxfUL *container_label, mxfUL *picture_essence_coding,
                                       int32_t avid_resolution_id)
{
    size_t i;

    if (mxf_equals_ul_mod_regver(container_label, &MXF_EC_L(AvidMJPEGClipWrapped))) {
        if (avid_resolution_id >= 0) {
            for (i = 0; i < RESOLUTION_ATOM_MAP_SIZE; i++) {
                if (avid_resolution_id == RESOLUTION_ATOM_MAP[i].avid_resolution_id)
                    return RESOLUTION_ATOM_MAP[i].ingex_resolution_id;
            }
        }

        for (i = 0; i < COMP_LABEL_RESOLUTION_ATOM_MAP_SIZE; i++) {
            if (mxf_equals_ul(picture_essence_coding, &COMP_LABEL_RESOLUTION_ATOM_MAP[i].compression_label))
                return COMP_LABEL_RESOLUTION_ATOM_MAP[i].ingex_resolution_id;
        }
    } else {
        // Note: using mxf_equals_ul_mod_regver function below because the Avid labels sometimes have a
        // different registry version byte
        for (i = 0; i < EC_LABEL_RESOLUTION_ATOM_MAP_SIZE; i++) {
            if (mxf_equals_ul_mod_regver(container_label, &EC_LABEL_RESOLUTION_ATOM_MAP[i].container_label))
                return EC_LABEL_RESOLUTION_ATOM_MAP[i].ingex_resolution_id;
        }
    }

    return 0;
}

prodauto::SourcePackage* IngexMXFInfo::getCurrentFileSourcePackage()
{
    return _package_group->GetFileSourcePackages().back();
}

