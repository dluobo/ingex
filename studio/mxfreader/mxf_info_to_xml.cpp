/*
 * $Id: mxf_info_to_xml.cpp,v 1.2 2010/07/21 16:29:34 john_f Exp $
 *
 * Read MXF files and dump metadata to XML file
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

#include <string>
#include <vector>

#include "IngexMXFInfo.h"
#include "ProdAutoException.h"

using namespace std;



class Clip
{
public:
    Clip(const IngexMXFInfo *info)
    {
        mPackageGroup = info->getPackageGroup()->Clone();
    }
    ~Clip()
    {
        delete mPackageGroup;
    }

    bool IsSameClip(const IngexMXFInfo *info)
    {
        return info->getPackageGroup()->GetMaterialPackage()->uid == mPackageGroup->GetMaterialPackage()->uid;
    }

    void MergeIn(const IngexMXFInfo *info)
    {
        PA_ASSERT(IsSameClip(info));
        // assert that either both have the same tape source package or both have no tape source package
        PA_ASSERT((!info->getPackageGroup()->GetTapeSourcePackage() && !mPackageGroup->GetTapeSourcePackage()) ||
                  (info->getPackageGroup()->GetTapeSourcePackage() && mPackageGroup->GetTapeSourcePackage()));
        PA_ASSERT(!info->getPackageGroup()->GetTapeSourcePackage() ||
                  info->getPackageGroup()->GetTapeSourcePackage()->uid == mPackageGroup->GetTapeSourcePackage()->uid);

        // append clone of file source packages not already present
        vector<prodauto::SourcePackage*> &from_file_packages = info->getPackageGroup()->GetFileSourcePackages();
        vector<prodauto::SourcePackage*> &to_file_packages = mPackageGroup->GetFileSourcePackages();
        size_t i, j;
        for (i = 0; i < from_file_packages.size(); i++) {
            for (j = 0; j < to_file_packages.size(); j++) {
                if (from_file_packages[i]->uid == to_file_packages[j]->uid)
                    break; // duplicate
            }

            if (j == to_file_packages.size()) {
                mPackageGroup->AppendFileSourcePackage(
                    dynamic_cast<prodauto::SourcePackage*>(from_file_packages[i]->clone()));
            } else {
                printf("Warning: ignoring duplicate file '%s'\n", info->getFilename().c_str());
            }
        }
    }

    bool IsComplete()
    {
        prodauto::MaterialPackage *material_package = mPackageGroup->GetMaterialPackage();
        size_t i;
        for (i = 0; i < material_package->tracks.size(); i++) {
            if (!mPackageGroup->HaveFileSourcePackage(material_package->tracks[i]->id))
                return false;
        }

        return true;
    }

    vector<uint32_t> GetMissingTracks()
    {
        vector<uint32_t> missing_tracks;

        prodauto::MaterialPackage *material_package = mPackageGroup->GetMaterialPackage();
        size_t i;
        for (i = 0; i < material_package->tracks.size(); i++) {
            if (!mPackageGroup->HaveFileSourcePackage(material_package->tracks[i]->id))
                missing_tracks.push_back(material_package->tracks[i]->id);
        }

        return missing_tracks;
    }
    
    string GetAFilename()
    {
        string audio_filename;
        
        prodauto::MaterialPackage *material_package = mPackageGroup->GetMaterialPackage();
        size_t i;
        for (i = 0; i < material_package->tracks.size(); i++) {
            if (mPackageGroup->HaveFileSourcePackage(material_package->tracks[i]->id)) {
                if (material_package->tracks[i]->dataDef == PICTURE_DATA_DEFINITION)
                    return mPackageGroup->GetFileLocation(material_package->tracks[i]->id);
                else if (audio_filename.empty())
                    audio_filename = mPackageGroup->GetFileLocation(material_package->tracks[i]->id);
            }
        }
        
        return audio_filename;
    }

    prodauto::MaterialPackage* GetMaterialPackage() { return mPackageGroup->GetMaterialPackage(); }
    prodauto::PackageGroup* GetPackageGroup() { return mPackageGroup; }

private:
    prodauto::PackageGroup *mPackageGroup;
};




static string get_xml_filename(string output_dir, string mxf_filepath)
{
    string xml_name;
    size_t sep_index;
    if ((sep_index = mxf_filepath.rfind("/")) != string::npos)
        xml_name = mxf_filepath.substr(sep_index + 1);
    else
        xml_name = mxf_filepath;
    xml_name.append(".xml");

    if (output_dir.empty())
        return xml_name;
    else if (output_dir[output_dir.size() - 1] == '/')
        return output_dir + xml_name;
    else
        return output_dir + '/' + xml_name;
}

static void print_usage(const char *cmd)
{
    fprintf(stderr, "%s [options] <filenames>\n", cmd);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -h | --help            Show this usage message and exit\n");
    fprintf(stderr, "    --output <dir>         Output directory (default .)\n");
    fprintf(stderr, "    --accept-incomplete    Accept incomplete set of files for a clip\n");
}

int main(int argc, const char **argv)
{
    const char *output_dirname = "./";
    vector<string> filenames;
    bool accept_incomplete = false;
    int cmdln_index;

    // parse the command line arguments

    for (cmdln_index = 1; cmdln_index < argc; cmdln_index++) {
        if (strcmp(argv[cmdln_index], "-h") == 0 ||
            strcmp(argv[cmdln_index], "--help") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdln_index], "--output") == 0)
        {
            if (cmdln_index + 1 >= argc) {
                print_usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdln_index]);
                return 1;
            }
            output_dirname = argv[cmdln_index + 1];
            cmdln_index++;
        }
        else if (strcmp(argv[cmdln_index], "--accept-incomplete") == 0)
        {
            accept_incomplete = true;
        }
        else
        {
            break;
        }
    }
    
    if (cmdln_index == argc) {
        print_usage(argv[0]);
        if (argc != 1)
            fprintf(stderr, "Missing <filenames>\n");
        return 1;
    }

    for (; cmdln_index < argc; cmdln_index++)
        filenames.push_back(argv[cmdln_index]);


    size_t f, i, c, t;
    
    // read package info from each file

    vector<IngexMXFInfo*> mxf_infos;
    for (f = 0; f < filenames.size(); f++) {
        IngexMXFInfo *info;
        IngexMXFInfo::ReadResult result = IngexMXFInfo::read(filenames[f], &info);
        if (result != IngexMXFInfo::SUCCESS) {
            fprintf(stderr, "Failed to read package info from file '%s': %s\n",
                    filenames[f].c_str(), IngexMXFInfo::errorToString(result).c_str());
            return 1;
        }

        mxf_infos.push_back(info);
    }


    // group files into clips

    vector<Clip*> clips;
    for (i = 0; i < mxf_infos.size(); i++) {
        for (c = 0; c < clips.size(); c++) {
            if (clips[c]->IsSameClip(mxf_infos[i])) {
                clips[c]->MergeIn(mxf_infos[i]);
                break;
            }
        }
        if (c == clips.size())
            clips.push_back(new Clip(mxf_infos[i]));
    }


    // check clips are complete

    for (c = 0; c < clips.size(); c++) {
        if (!clips[c]->IsComplete()) {
            vector<uint32_t> missing_tracks = clips[c]->GetMissingTracks();
            fprintf(stderr, "Clip file set '%s' is incomplete: missing material package tracks ",
                    clips[c]->GetMaterialPackage()->name.c_str());
            for (t = 0; t < missing_tracks.size(); t++) {
                if (t != 0)
                    fprintf(stderr, ", ");
                fprintf(stderr, "%d", missing_tracks[t]);
            }
            fprintf(stderr, "\n");
            if (!accept_incomplete) {
                fprintf(stderr, "Failed because clip file set is incomplete\n");
                return 1;
            }
        }
    }


    // save clips to xml

    int error_count = 0;
    
    for (c = 0; c < clips.size(); c++) {
        try
        {
            string a_filename = clips[c]->GetAFilename();
            if (a_filename.empty()) {
                fprintf(stderr, "Error: no file location present in PackageGroup\n");
                error_count++;
                continue;
            }
            
            clips[c]->GetPackageGroup()->SaveToFile(get_xml_filename(output_dirname, a_filename), 0);
        }
        catch (const prodauto::ProdAutoException &ex)
        {
            fprintf(stderr, "Error: failed to save metadata to file: %s\n", ex.getMessage().c_str());
            error_count++;
        }
        catch (...)
        {
            fprintf(stderr, "Error: failed to save metadata to file\n");
            error_count++;
        }
    }

    printf("%zd files processed\n", filenames.size());
    printf("%zd clips processed\n", clips.size());
    printf("%d clips resulted in an error\n", error_count);


    // clean up

    for (i = 0; i < mxf_infos.size(); i++)
        delete mxf_infos[i];
    for (c = 0; c < clips.size(); c++)
        delete clips[c];


    return 0;
}

