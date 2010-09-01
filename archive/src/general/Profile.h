/*
 * $Id: Profile.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#ifndef __RECORDER_PROFILE_H__
#define __RECORDER_PROFILE_H__


#include <string>
#include <vector>
#include <map>

#include "DatabaseThings.h"



namespace rec
{

class ProfileManager;

class Profile
{
public:
    friend class ProfileManager;
    
public:
    static Profile* readFromFile(std::string filename);
    
public:
    Profile();
    Profile(const Profile *profile);
    ~Profile();
    
    int getId() const { return _id; }
    std::string getName() const { return _name; }
    int getUpdateCount() const { return _update_count; }
    
    IngestFormat getIngestFormat(std::string source_format) const;
    
    void setCachedIngestFormat(std::string source_format);
    IngestFormat getIngestFormat() const;
    
    bool updateIngestFormat(std::string source_format_code, IngestFormat format);
    void updateDefaultIngestFormat(IngestFormat format);
    
    bool update(const Profile *updated_profile);

public:
    std::map<std::string, std::vector<IngestFormat> > ingest_formats;
    std::vector<IngestFormat> default_ingest_format;
    bool browse_enable;
    int browse_video_bit_rate;
    bool pse_enable;
    int num_audio_tracks;
    bool include_crc32;
    PrimaryTimecode primary_timecode;
    
private:
    void defaultInitialize();
    
    bool requireFileWrite() const;
    bool writeToFile(std::string filename);
    
    bool validate(std::string *error) const;

    bool setProfilePairs(std::map<std::string, std::string> &name_values);
    
private:
    int _id;
    std::string _name;
    int _update_count;
    std::string _filename;
    int _file_update_count;
    IngestFormat _cached_ingest_format;
};


class ProfileManager
{
public:
    ProfileManager(std::string directory, std::string filename_suffix);
    ~ProfileManager();
    
    Profile* createProfile(const Profile *templ, std::string name);
    
    bool updateProfileName(Profile *profile, std::string new_name);
    void incrementProfileUpdateCount(Profile *profile);
    
    void storeProfiles();
    
    Profile* getProfile(int id) const;
    Profile* getProfile(std::string name) const;
    
    const std::map<int, Profile*>& getProfiles() const { return _profileById; }
    
private:
    bool addProfile(Profile *profile);
    
private:
    std::string _directory;
    std::string _filename_suffix;
    int _next_profile_id;
    std::map<int, Profile*> _profileById;
    std::map<std::string, Profile*> _profileByName;
};


};


#endif
