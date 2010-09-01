/*
 * $Id: Profile.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * Read, write and store profiles
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <dirent.h>

#include "Profile.h"
#include "Utilities.h"
#include "Logging.h"
#include "RecorderException.h"

using namespace std;
using namespace rec;


#define DEFAULT_PROFILE_NAME        "Default"



static bool find_value(map<string, string> &config_pairs, string name, string *value)
{
    map<string, string>::const_iterator result = config_pairs.find(name);
    if (result == config_pairs.end())
        return false;

    *value = result->second;
    return true;
}

static int parse_string(map<string, string> &config_pairs, string name, string *value)
{
    string str_value;
    if (!find_value(config_pairs, name, &str_value))
        return 1;
    
    *value = str_value;
    return 0;
}

static int parse_string_array(map<string, string> &config_pairs, string name, vector<string> *value)
{
    string str_value;
    if (!find_value(config_pairs, name, &str_value))
        return 1;
    
    vector<string> ret_value;
    size_t prev_pos = 0;
    size_t pos = 0;
    while ((pos = str_value.find(",", prev_pos)) != string::npos) {
        ret_value.push_back(str_value.substr(prev_pos, pos - prev_pos));
        prev_pos = pos + 1;
    }
    if (prev_pos < str_value.size())
        ret_value.push_back(str_value.substr(prev_pos, str_value.size() - prev_pos));
    
    *value = ret_value;
    return 0;
}

static int parse_int(map<string, string> &config_pairs, string name, int *value)
{
    string str_value;
    if (!find_value(config_pairs, name, &str_value))
        return 1;
    
    int ret_value;
    if (sscanf(str_value.c_str(), "%d", &ret_value) != 1) {
        Logging::error("Profile '%s' value '%s' is not a valid int\n", name.c_str(), str_value.c_str());
        return -1;
    }
    
    *value = ret_value;
    return 0;
}

static int parse_bool(map<string, string> &config_pairs, string name, bool *value)
{
    string str_value;
    if (!find_value(config_pairs, name, &str_value))
        return 1;
    
    if (str_value == "true") {
        *value = true;
        return 0;
    } else if (str_value == "false") {
        *value = false;
        return 0;
    }
    
    Logging::error("Profile '%s' value '%s' is not a valid boolean\n", name.c_str(), str_value.c_str());
    return -1;
}

static bool parse_ingest_format(string code, IngestFormat *ingest_format)
{
    if (code == "MXF_UNC_8") {
        *ingest_format = MXF_UNC_8BIT_INGEST_FORMAT;
        return true;
    }
    else if (code == "MXF_UNC_10") {
        *ingest_format = MXF_UNC_10BIT_INGEST_FORMAT;
        return true;
    }
    else if (code == "MXF_D10_50") {
        *ingest_format = MXF_D10_50_INGEST_FORMAT;
        return true;
    }
    else if (code == "DISABLED") {
        *ingest_format = UNKNOWN_INGEST_FORMAT;
        return true;
    }
    
    return false;
}

static int parse_default_ingest_format(map<string, string> &config_pairs, string name, vector<IngestFormat> *value)
{
    vector<string> str_array_value;
    int result = parse_string_array(config_pairs, name, &str_array_value);
    if (result != 0 || str_array_value.empty()) {
        Logging::error("Profile '%s' value is not a valid default ingest format array: invalid or empty array\n", name.c_str());
        return -1;
    }
    
    vector<IngestFormat> ret_value;
    size_t i;
    for (i = 0; i < str_array_value.size(); i++) {
        IngestFormat ingest_format;
        if (!parse_ingest_format(str_array_value[i], &ingest_format)) {
            Logging::error("Profile '%s' value is not a valid default ingest format array: unknown\n", name.c_str());
            return -1;
        }

        ret_value.push_back(ingest_format);
    }
    
    *value = ret_value;
    return 0;
}

static int parse_ingest_formats(map<string, string> &config_pairs, string prefix, map<string, vector<IngestFormat> > *value)
{
    map<string, vector<IngestFormat> > ret_value;
    map<string, string>::const_iterator iter;
    for (iter = config_pairs.begin(); iter != config_pairs.end(); iter++) {
        if (iter->first.compare(0, prefix.size(), prefix) == 0) {
            string source_format_code = iter->first.substr(prefix.size());
            if (source_format_code.empty() || !ret_value[source_format_code].empty()) {
                Logging::error("Profile '%s' value is not a valid ingest format array: missing or duplicate format code\n",
                               iter->first.c_str());
                return -1;
            }
            
            vector<string> str_array_value;
            int result = parse_string_array(config_pairs, iter->first, &str_array_value);
            if (result != 0 || str_array_value.empty()) {
                Logging::error("Profile '%s' value is not a valid ingest format array: invalid or empty array\n", iter->first.c_str());
                return -1;
            }
            
            size_t i;
            for (i = 0; i < str_array_value.size(); i++) {
                IngestFormat ingest_format;
                if (!parse_ingest_format(str_array_value[i], &ingest_format)) {
                    Logging::error("Profile '%s' value is not a valid ingest format array: unknown format\n", iter->first.c_str());
                    return -1;
                }

                ret_value[source_format_code].push_back(ingest_format);
            }
        }
    }
    
    *value = ret_value;
    return 0;
}

static int parse_primary_timecode(map<string, string> &config_pairs, string name, PrimaryTimecode *primary_timecode)
{
    string str_value;
    if (!find_value(config_pairs, name, &str_value))
        return 1;
    
    if (primary_timecode_from_string(str_value, primary_timecode))
        return 0;
    
    Logging::error("Profile '%s' value '%s' is not a valid primary timecode\n", name.c_str(), str_value.c_str());
    return -1;
}


static string serialize_int(int value)
{
    char buf[16];
    sprintf(buf, "%d", value);
    return buf;
}

static string serialize_bool(bool value)
{
    return (value ? "true" : "false");
}

static string serialize_ingest_format(IngestFormat ingest_format)
{
    switch (ingest_format)
    {
        case MXF_UNC_8BIT_INGEST_FORMAT:
            return "MXF_UNC_8";
        case MXF_UNC_10BIT_INGEST_FORMAT:
            return "MXF_UNC_10";
        case MXF_D10_50_INGEST_FORMAT:
            return "MXF_D10_50";
        case UNKNOWN_INGEST_FORMAT:
        default:
            return "DISABLED";
    }
}

static string serialize_ingest_formats(const map<string, vector<IngestFormat> > &ingest_formats)
{
    string str_value;
    map<string, vector<IngestFormat> >::const_iterator iter;
    for (iter = ingest_formats.begin(); iter != ingest_formats.end(); iter++) {
        str_value += "ingest_format_" + iter->first + " = ";
        size_t i;
        for (i = 0; i < iter->second.size(); i++) {
            if (i != 0)
                str_value += ",";
            str_value += serialize_ingest_format(iter->second[i]);
        }
        str_value += "\n";
    }
    
    return str_value;
}

static string serialize_default_ingest_formats(const vector<IngestFormat> &default_ingest_formats)
{
    string str_value;
    size_t i;
    for (i = 0; i < default_ingest_formats.size(); i++) {
        if (i != 0)
            str_value += ",";
        str_value += serialize_ingest_format(default_ingest_formats[i]);
    }
    
    return str_value;
}


static string create_unique_filename(string directory, string descriptive_name, string filename_suffix)
{
    string name_prefix = "profile_" + descriptive_name;
    size_t i;
    for (i = 0; i < name_prefix.size(); i++) {
        if (name_prefix[i] >= 'A' && name_prefix[i] <= 'Z')
        {
            name_prefix[i] = (name_prefix[i] - 'A') + 'a'; // make lowercase
        }
        else if (name_prefix[i] == ' ')
        {
            name_prefix[i] = '_';
        }
        else if ((name_prefix[i] < 'a' || name_prefix[i] > 'z') &&
                 (name_prefix[i] < '0' || name_prefix[i] > '9') &&
                 name_prefix[i] != '_')
        {
            name_prefix.erase(i, 1);
            i--;
        }
    }
    
    int count = 0;
    string filename;
    do {
        if (count == 0)
            filename = join_path(directory, name_prefix + filename_suffix);
        else
            filename = join_path(directory, name_prefix + "_" + serialize_int(count) + filename_suffix);
        count++;
    } while (file_exists(filename));
    
    return filename;
}


static string read_next_line(FILE* file)
{
    int c;
    string line;

    // move file pointer past the newline characters
    while ((c = fgetc(file)) != EOF && (c == '\r' || c == '\n'))
    {}
    
    while (c != EOF && (c != '\r' && c != '\n')) {
        line += c;
        c = fgetc(file);
    }
    
    return line;
}



Profile::Profile()
{
    browse_enable = false;
    browse_video_bit_rate = 0;
    pse_enable = false;
    num_audio_tracks = 0;
    include_crc32 = false;
    primary_timecode = PRIMARY_TIMECODE_AUTO;
    
    _id = -1;
    _update_count = 0;
    _file_update_count = -1;
    _cached_ingest_format = UNKNOWN_INGEST_FORMAT;
}

Profile::Profile(const Profile *profile)
{
    ingest_formats = profile->ingest_formats;
    default_ingest_format = profile->default_ingest_format;
    browse_enable = profile->browse_enable;
    browse_video_bit_rate = profile->browse_video_bit_rate;
    pse_enable = profile->pse_enable;
    num_audio_tracks = profile->num_audio_tracks;
    include_crc32 = profile->include_crc32;
    primary_timecode = profile->primary_timecode;
    
    _id = profile->_id;
    _name = profile->_name;
    _update_count = profile->_update_count;
    _filename = profile->_filename;
    _file_update_count = profile->_file_update_count;
    _cached_ingest_format = profile->_cached_ingest_format;
}

Profile::~Profile()
{}

Profile* Profile::readFromFile(string filename)
{
    FILE *file = fopen(filename.c_str(), "rb");
    if (!file) {
        Logging::error("Failed to open profile file '%s' for reading: %s\n", filename.c_str(), strerror(errno));
        return 0;
    }
    
    size_t start;
    size_t len;
    string name;
    string value;
    string line;
    bool have_non_space;
    map<string, string> profile_pairs;
    
    while (true) {
        line = read_next_line(file);
        if (line.size() == 0) {
            // done
            break;
        }
        
        // parse name
        start = 0;
        len = 0;   
        have_non_space = false;
        while (start + len < line.size()) {
            if (line[start + len] == '=') {
                break;
            } else if (line[start + len] == '#') {
                // comment line
                len = line.size() - start; // force same as empty line
                break;
            }
            
            have_non_space = have_non_space || !isspace(line[start + len]);
            len++;
        }
        if (start + len >= line.size()) {
            if (have_non_space) {
                fprintf(stderr, "Invalid config line '%s': missing '='\n", line.c_str());
                fclose(file);
                return 0;
            }
            
            // empty or comment line
            continue;
        }
        name = trim_string(line.substr(start, len));
        if (name.size() == 0) {
            fprintf(stderr, "Invalid config line '%s': zero length name\n", line.c_str());
            fclose(file);
            return 0;
        }
        start += len + 1;
        len = 0;

        // parse value        
        while (start + len < line.size()) {
            if (line[start + len] == '#')
                break;
            len++;
        }
        value = trim_string(line.substr(start, len));

        // remove quotes
        if (value.size() >= 2 &&
            value[0] == value[value.size() - 1] &&
            (value[0] == '\'' || value[0] == '"'))
        {
            value = value.substr(1, value.size() - 2);
        }

        // set name/value
        profile_pairs[name] = value;
    }

    fclose(file);


    Profile *new_profile = new Profile();
    new_profile->setProfilePairs(profile_pairs);
    new_profile->_update_count++;
    new_profile->_filename = filename;
    new_profile->_file_update_count = new_profile->_update_count;

    return new_profile;
}

IngestFormat Profile::getIngestFormat(string source_format) const
{
    map<string, vector<IngestFormat> >::const_iterator result = ingest_formats.find(source_format);
    if (result != ingest_formats.end())
    {
        return result->second.front();
    }
    
    return default_ingest_format.front();
}

void Profile::setCachedIngestFormat(string source_format)
{
    _cached_ingest_format = getIngestFormat(source_format);
}

IngestFormat Profile::getIngestFormat() const
{
    return _cached_ingest_format;
}

bool Profile::updateIngestFormat(string source_format_code, IngestFormat format)
{
    map<string, vector<IngestFormat> >::iterator find_result = ingest_formats.find(source_format_code);
    if (find_result == ingest_formats.end())
        return false;
    
    vector<IngestFormat> &current_ingest_format = find_result->second;
    if (!current_ingest_format.empty() && format == current_ingest_format.front())
        return true; // no change

    vector<IngestFormat> updated_ingest_format;
    updated_ingest_format.push_back(format);
    
    size_t i;
    for (i = 0; i < current_ingest_format.size(); i++) {
        if (current_ingest_format[i] == format)
            continue;
        updated_ingest_format.push_back(current_ingest_format[i]);
    }
    
    current_ingest_format = updated_ingest_format;
    _update_count++;
    
    return true;
}

void Profile::updateDefaultIngestFormat(IngestFormat format)
{
    if (!default_ingest_format.empty() && format == default_ingest_format.front())
        return; // no change
    
    vector<IngestFormat> updated_default_ingest_format;
    updated_default_ingest_format.push_back(format);
    
    size_t i;
    for (i = 0; i < default_ingest_format.size(); i++) {
        if (default_ingest_format[i] == format)
            continue;
        updated_default_ingest_format.push_back(default_ingest_format[i]);
    }
    
    default_ingest_format = updated_default_ingest_format;
    _update_count++;
}

bool Profile::update(const Profile *updated_profile)
{
    string error;
    if (!updated_profile->validate(&error)) {
        Logging::error("Updated profile is invalid: %s\n", error.c_str());
        return false;
    }
    
    ingest_formats = updated_profile->ingest_formats;
    default_ingest_format = updated_profile->default_ingest_format;
    browse_enable = updated_profile->browse_enable;
    browse_video_bit_rate = updated_profile->browse_video_bit_rate;
    pse_enable = updated_profile->pse_enable;
    num_audio_tracks = updated_profile->num_audio_tracks;
    include_crc32 = updated_profile->include_crc32;
    primary_timecode = updated_profile->primary_timecode;
    
    _update_count++;
    
    return true;
}

void Profile::defaultInitialize()
{
    ingest_formats["BD"].push_back(MXF_UNC_10BIT_INGEST_FORMAT);
    ingest_formats["BD"].push_back(MXF_UNC_8BIT_INGEST_FORMAT);
    ingest_formats["BD"].push_back(MXF_D10_50_INGEST_FORMAT);
    ingest_formats["BS"].push_back(MXF_UNC_8BIT_INGEST_FORMAT);
    ingest_formats["BS"].push_back(MXF_D10_50_INGEST_FORMAT);
    ingest_formats["BS"].push_back(MXF_UNC_10BIT_INGEST_FORMAT);
    ingest_formats["BX"].push_back(MXF_UNC_8BIT_INGEST_FORMAT);
    ingest_formats["BX"].push_back(MXF_D10_50_INGEST_FORMAT);
    ingest_formats["BX"].push_back(MXF_UNC_10BIT_INGEST_FORMAT);
    ingest_formats["D3"].push_back(MXF_UNC_8BIT_INGEST_FORMAT);
    ingest_formats["D3"].push_back(MXF_UNC_10BIT_INGEST_FORMAT);
    ingest_formats["D3"].push_back(MXF_D10_50_INGEST_FORMAT);
    default_ingest_format.push_back(MXF_UNC_8BIT_INGEST_FORMAT);
    default_ingest_format.push_back(MXF_D10_50_INGEST_FORMAT);
    default_ingest_format.push_back(MXF_UNC_10BIT_INGEST_FORMAT);
    browse_enable = true;
    browse_video_bit_rate = 2700;
    pse_enable = false;
    num_audio_tracks = 4;
    include_crc32 = true;
    primary_timecode = PRIMARY_TIMECODE_AUTO;
}

bool Profile::requireFileWrite() const
{
    return _filename.empty() || _file_update_count != _update_count;
}

bool Profile::writeToFile(string filename)
{
    FILE *file = fopen(filename.c_str(), "wb");
    if (!file) {
        Logging::error("Failed to open profile file '%s' for writing: %s\n", filename.c_str(), strerror(errno));
        return false;
    }

#define CHECK_FPRINTF(cmd) \
    if ((cmd) < 0) { \
        Logging::error("Failed to write to profile file '%s': %s\n", filename.c_str(), strerror(errno)); \
        fclose(file); \
        return false; \
    }
    
    CHECK_FPRINTF(fprintf(file, "profile_name = '%s'\n", _name.c_str()));
    CHECK_FPRINTF(fprintf(file, "%s", serialize_ingest_formats(ingest_formats).c_str()));
    CHECK_FPRINTF(fprintf(file, "default_ingest_format = %s\n", serialize_default_ingest_formats(default_ingest_format).c_str()));
    CHECK_FPRINTF(fprintf(file, "browse_enable = %s\n", serialize_bool(browse_enable).c_str()));
    CHECK_FPRINTF(fprintf(file, "browse_video_bit_rate = %s\n", serialize_int(browse_video_bit_rate).c_str()));
    CHECK_FPRINTF(fprintf(file, "pse_enable = %s\n", serialize_bool(pse_enable).c_str()));
    CHECK_FPRINTF(fprintf(file, "num_audio_tracks = %s\n", serialize_int(num_audio_tracks).c_str()));
    CHECK_FPRINTF(fprintf(file, "include_crc32 = %s\n", serialize_bool(include_crc32).c_str()));
    CHECK_FPRINTF(fprintf(file, "primary_timecode = %s\n", primary_timecode_to_string(primary_timecode).c_str()));
    
    fclose(file);
    
    _filename = filename;
    _file_update_count = _update_count;
    
    return true;
}

bool Profile::validate(string *error) const
{
    if (_name.empty()) {
        *error = "profile name not set";
        return false;
    }
    if (ingest_formats.empty() && default_ingest_format.empty()) {
        *error = "no ingest_formats or default_ingest_format set";
        return false;
    }
    if (browse_enable && browse_video_bit_rate <= 0) {
        *error = "browse_video_bit_rate not set";
        return false;
    }
    
    return true;
}

bool Profile::setProfilePairs(map<string, string> &profile_pairs)
{
#define CHECK_PARSE(cmd) \
    if ((cmd) < 0) \
        return false;

    CHECK_PARSE(parse_string(profile_pairs, "profile_name", &_name));
    CHECK_PARSE(parse_ingest_formats(profile_pairs, "ingest_format_", &ingest_formats));
    CHECK_PARSE(parse_default_ingest_format(profile_pairs, "default_ingest_format", &default_ingest_format));
    CHECK_PARSE(parse_bool(profile_pairs, "browse_enable", &browse_enable));
    CHECK_PARSE(parse_int(profile_pairs, "browse_video_bit_rate", &browse_video_bit_rate));
    CHECK_PARSE(parse_bool(profile_pairs, "pse_enable", &pse_enable));
    CHECK_PARSE(parse_int(profile_pairs, "num_audio_tracks", &num_audio_tracks));
    CHECK_PARSE(parse_bool(profile_pairs, "include_crc32", &include_crc32));
    CHECK_PARSE(parse_primary_timecode(profile_pairs, "primary_timecode", &primary_timecode));
    
    return true;
}



ProfileManager::ProfileManager(string directory, string filename_suffix)
{
    _directory = directory;
    _filename_suffix = filename_suffix;
    _next_profile_id = 1;
    
    DIR *dir_stream;
    struct dirent *entry;
    string filename;
    
    if (directory.empty())
        dir_stream = opendir("./");
    else
        dir_stream = opendir(directory.c_str());
    if (!dir_stream) {
        REC_LOGTHROW(("Failed to open profile directory '%s'", directory.c_str()));
    }
    
    while ((entry = readdir(dir_stream)) != 0) {
        if (strlen(entry->d_name) >= filename_suffix.size() &&
            filename_suffix == &entry->d_name[strlen(entry->d_name) - filename_suffix.size()])
        {
            filename = join_path(directory, entry->d_name);
            
            Profile *profile = Profile::readFromFile(filename);
            if (!profile) {
                REC_LOGTHROW(("Failed to read profile file '%s'", filename.c_str()));
            }
            
            string error;
            if (!profile->validate(&error)) {
                delete profile;
                REC_LOGTHROW(("Profile validation failed for file '%s': %s", entry->d_name, error.c_str()));
            }
            
            if (!addProfile(profile)) {
                delete profile;
                REC_LOGTHROW(("Failed to add profile read from file '%s'", entry->d_name));
            }
            
            Logging::info("Read profile '%s' from file '%s'\n", profile->_name.c_str(), entry->d_name);
        }
    }
    
    closedir(dir_stream);
    
    if (_profileById.empty()) {
        Profile *default_profile = new Profile();
        
        default_profile->defaultInitialize();
        default_profile->_name = DEFAULT_PROFILE_NAME;
        
        addProfile(default_profile);
        Logging::info("Created default profile\n");
        
        storeProfiles();
    }
}

ProfileManager::~ProfileManager()
{
    std::map<int, Profile*>::iterator iter;
    for (iter = _profileById.begin(); iter != _profileById.end(); iter++)
        delete iter->second;
}

Profile* ProfileManager::createProfile(const Profile *templ, std::string name)
{
    Profile *new_profile;
    if (templ)
        new_profile = new Profile(templ);
    else
        new_profile = new Profile();
    new_profile->_name = name;
    
    if (!addProfile(new_profile)) {
        delete new_profile;
        return 0;
    }
    
    return new_profile;
}

void ProfileManager::storeProfiles()
{
    map<int, Profile*>::const_iterator iter;
    for (iter = _profileById.begin(); iter != _profileById.end(); iter++) {
        if (iter->second->requireFileWrite()) {
            if (iter->second->_filename.empty())
                iter->second->writeToFile(create_unique_filename(_directory, iter->second->_name, _filename_suffix));
            else
                iter->second->writeToFile(iter->second->_filename);
        }
    }
}

bool ProfileManager::addProfile(Profile *profile)
{
    if (profile->_name.empty()) {
        Logging::error("Failed to add profile with empty name\n");
        return false;
    }

    if (getProfile(profile->_name)) {
        Logging::error("Failed to add profile with duplicate name '%s'\n", profile->_name.c_str());
        return false;
    }

    _profileById[_next_profile_id] = profile;
    _profileByName[profile->_name] = profile;
    profile->_id = _next_profile_id;
    
    _next_profile_id++;
    
    return true;
}

bool ProfileManager::updateProfileName(Profile *profile, string new_name)
{
    if (profile->_name == new_name)
        return true;
    
    if (getProfile(new_name))
        return false;
    
    profile->_name = new_name;
    profile->_update_count++;
    
    return true;
}

void ProfileManager::incrementProfileUpdateCount(Profile *profile)
{
    profile->_update_count++;
}

Profile* ProfileManager::getProfile(int id) const
{
    map<int, Profile*>::const_iterator result = _profileById.find(id);
    if (result == _profileById.end())
        return 0;
    
    return result->second;
}

Profile* ProfileManager::getProfile(string name) const
{
    map<string, Profile*>::const_iterator result = _profileByName.find(name);
    if (result == _profileByName.end())
        return 0;
    
    return result->second;
}

