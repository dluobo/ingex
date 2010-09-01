/*
 * $Id: Config.cpp,v 1.2 2010/09/01 16:05:22 philipn Exp $
 *
 * Read, write and store an application's configuration options
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

#include "Config.h"
#include "Utilities.h"
#include "RecorderException.h"

using namespace std;
using namespace rec;


#define CHECK_PARSE(cmd) \
    if ((cmd) < 0) \
        return false;

#define CHECK_STRING_NOT_SET(var, err) \
    if (var.empty()) { \
        *err = #var " not set"; \
        return false; \
    }

#define CHECK_INT_NOT_SET(var, err) \
    if (var <= 0) { \
        *err = #var " not set"; \
        return false; \
    }

#define CHECK_ARRAY_NOT_SET(var, err) \
    if (var.empty()) { \
        *err = #var " not set"; \
        return false; \
    }



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
        fprintf(stderr, "Config '%s' value '%s' is not a valid int\n", name.c_str(), str_value.c_str());
        return -1;
    }
    
    *value = ret_value;
    return 0;
}

static int parse_int_array(map<string, string> &config_pairs, string name, vector<int> *value)
{
    string str_value;
    if (!find_value(config_pairs, name, &str_value))
        return 1;
    
    vector<int> ret_value;
    int intValue;
    size_t prev_pos = 0;
    size_t pos = 0;
    while ((pos = str_value.find(",", prev_pos)) != string::npos) {
        if (sscanf(str_value.substr(prev_pos, pos - prev_pos).c_str(), "%d", &intValue) != 1) {
            fprintf(stderr, "Config '%s' value '%s' is not a valid int array\n", name.c_str(), str_value.c_str());
            return -1;
        }
        ret_value.push_back(intValue);
        
        prev_pos = pos + 1;
    }
    if (prev_pos < str_value.size()) {
        if (sscanf(str_value.substr(prev_pos, str_value.size() - prev_pos).c_str(), "%d", &intValue) != 1) {
            fprintf(stderr, "Config '%s' value '%s' is not a valid int array\n", name.c_str(), str_value.c_str());
            return -1;
        }
        ret_value.push_back(intValue);
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
    
    fprintf(stderr, "Config '%s' value '%s' is not a valid boolean\n", name.c_str(), str_value.c_str());
    return -1;
}

static int parse_log_level(map<string, string> &config_pairs, string name, LogLevel *value)
{
    string str_value;
    int result = parse_string(config_pairs, name, &str_value);
    if (result != 0)
        return result;
    
    if (str_value == "DEBUG") {
        *value = LOG_LEVEL_DEBUG;
        return 0;
    } else if (str_value == "INFO") {
        *value = LOG_LEVEL_INFO;
        return 0;
    } else if (str_value == "WARNING") {
        *value = LOG_LEVEL_WARNING;
        return 0;
    } else if (str_value == "ERROR") {
        *value = LOG_LEVEL_ERROR;
        return 0;
    }
    
    return -1;
}

static int parse_serial_type(map<string, string> &config_pairs, string name, SerialType *value)
{
    string str_value;
    int result = parse_string(config_pairs, name, &str_value);
    if (result != 0)
        return result;
    
    if (str_value.empty()) {
        *value = TYPE_STD_TTY;
        return 0;
    } else if (str_value == "Blackmagic") {
        *value = TYPE_BLACKMAGIC;
        return 0;
    } else if (str_value == "DVS") {
        *value = TYPE_DVS;
        return 0;
    }
    
    return -1;
}



static string serialize_string_array(const vector<string> &value)
{
    string str_value;
    size_t i;
    for (i = 0; i < value.size(); i++) {
        if (i != 0)
            str_value.append(",");
        str_value.append(value[i]);
    }
    
    return str_value;
}

static string serialize_int(int value)
{
    char buf[16];
    sprintf(buf, "%d", value);
    return buf;
}

static string serialize_int_array(const vector<int> &value)
{
    string str_value;
    char buf[16];
    size_t i;
    for (i = 0; i < value.size(); i++) {
        if (i != 0)
            str_value.append(",");
        sprintf(buf, "%d", value[i]);
        str_value.append(buf);
    }
    
    return str_value;
}

static string serialize_bool(bool value)
{
    return (value ? "true" : "false");
}

static string serialize_log_level(LogLevel level)
{
    switch (level)
    {
        case LOG_LEVEL_ERROR:
            return "ERROR";
        case LOG_LEVEL_WARNING:
            return "WARNING";
        case LOG_LEVEL_INFO:
            return "INFO";
        case LOG_LEVEL_DEBUG:
        default:
            return "DEBUG";
    }
}

static string serialize_serial_type(SerialType type)
{
    switch (type)
    {
        case TYPE_DVS:
            return "DVS";
        case TYPE_BLACKMAGIC:
            return "Blackmagic";
        case TYPE_STD_TTY:
        default:
            return "";
    }
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




string Config::log_dir;
int Config::keep_logs = 0;
string Config::cache_dir;
string Config::browse_dir;
string Config::pse_dir;
string Config::db_host;
string Config::db_name;
string Config::db_user;
string Config::db_password;
string Config::recorder_name;
string Config::tape_transfer_lock_file;
string Config::player_exe;
string Config::local_infax_db_host;
string Config::local_infax_db_name;
string Config::local_infax_db_user;
string Config::local_infax_db_password;
LogLevel Config::recorder_log_level = LOG_LEVEL_DEBUG;
int Config::capture_log_level = 0;
string Config::recorder_www_root;
int Config::recorder_www_port = 0;
int Config::recorder_www_replay_port = 0;
int Config::recorder_www_threads = 0;
bool Config::videotape_backup = false;
vector<string> Config::source_vtr;
vector<string> Config::backup_vtr;
SerialType Config::recorder_vtr_serial_type_1 = TYPE_STD_TTY;
SerialType Config::recorder_vtr_serial_type_2 = TYPE_STD_TTY;
string Config::recorder_vtr_device_1;
string Config::recorder_vtr_device_2;
bool Config::recorder_server_barcode_scanner = false;
string Config::recorder_barcode_fifo;
string Config::profile_directory;
string Config::profile_filename_suffix;
int Config::ring_buffer_size = 0;
int Config::browse_thread_count = 0;
int Config::browse_overflow_frames = 0;
int Config::digibeta_dropout_lower_threshold = 0;
int Config::digibeta_dropout_upper_threshold = 0;
int Config::digibeta_dropout_store_threshold = 0;
bool Config::palff_mode = false;
vector<int> Config::vitc_lines;
vector<int> Config::ltc_lines;
vector<string> Config::digibeta_barcode_prefixes;
int Config::chunking_throttle_fps = 0;
bool Config::enable_chunking_junk = false;
bool Config::enable_multi_item = false;
int Config::player_source_buffer_size = 0;
bool Config::read_analogue_ltc = false;
bool Config::read_digital_ltc = false;
int Config::read_audio_track_ltc = 0;
LogLevel Config::tape_export_log_level = LOG_LEVEL_DEBUG;
int Config::tape_log_level = 0;
string Config::tape_export_www_root;
int Config::tape_export_www_port = 0;
int Config::tape_export_www_threads = 0;
bool Config::tape_export_server_barcode_scanner = false;
string Config::tape_export_barcode_fifo;
string Config::tape_device;
vector<string> Config::lto_barcode_prefixes;
int Config::max_auto_files_on_lto = 0;
int Config::max_lto_size_gb = 0;
bool Config::dbg_av_sync = false;
bool Config::dbg_vitc_failures = false;
bool Config::dbg_vga_replay = false;
bool Config::dbg_force_x11 = false;
bool Config::dbg_null_barcode_scanner = false;
bool Config::dbg_store_thread_buffer = false;
bool Config::dbg_serial_port_open = false;
bool Config::dbg_keep_lto_files = false;


bool Config::readFromFile(string filename)
{
    FILE *file = fopen(filename.c_str(), "rb");
    if (!file) {
        fprintf(stderr, "Failed to open config file '%s' for reading: %s\n", filename.c_str(), strerror(errno));
        return false;
    }
    
    size_t start;
    size_t len;
    string name;
    string value;
    string line;
    bool have_non_space;
    map<string, string> config_pairs;
    
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
                return false;
            }
            
            // empty or comment line
            continue;
        }
        name = trim_string(line.substr(start, len));
        if (name.size() == 0) {
            fprintf(stderr, "Invalid config line '%s': zero length name\n", line.c_str());
            fclose(file);
            return false;
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
        config_pairs[name] = value;
    }

    fclose(file);


    bool result = setConfigPairs(config_pairs);

    return result;
}

bool Config::writeToFile(string filename)
{
    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) {
        fprintf(stderr, "Failed to open config file '%s' for writing: %s\n", filename.c_str(), strerror(errno));
        return false;
    }
    
    map<string, string> config_pairs = getConfigPairs();

    map<string, string>::const_iterator iter;
    for (iter = config_pairs.begin(); iter != config_pairs.end(); iter++) {
        if (fprintf(file, "%s = '%s'\n", iter->first.c_str(), iter->second.c_str()) < 0) {
            fprintf(stderr, "Failed to write to config file '%s': %s\n", filename.c_str(), strerror(errno));
            fclose(file);
            return false;
        }
    }
    
    fclose(file);
    
    return true;
}

string Config::writeToString()
{
    map<string, string> config_pairs = getConfigPairs();

    string result;
    map<string, string>::const_iterator iter;
    for (iter = config_pairs.begin(); iter != config_pairs.end(); iter++)
        result += iter->first + "='" + iter->second + "';";
    
    return result;
}

bool Config::validateRecorderConfig(string *error)
{
    if (!validateCommon(error))
        return false;
    
    CHECK_STRING_NOT_SET(local_infax_db_host, error);
    CHECK_STRING_NOT_SET(local_infax_db_name, error);
    CHECK_STRING_NOT_SET(local_infax_db_user, error);
    CHECK_STRING_NOT_SET(local_infax_db_password, error);
    CHECK_STRING_NOT_SET(recorder_www_root, error);
    CHECK_INT_NOT_SET(recorder_www_port, error);
    CHECK_INT_NOT_SET(recorder_www_threads, error);
    CHECK_ARRAY_NOT_SET(source_vtr, error);
    CHECK_STRING_NOT_SET(recorder_vtr_device_1, error);
    if (videotape_backup) {
        CHECK_ARRAY_NOT_SET(backup_vtr, error);
        CHECK_STRING_NOT_SET(recorder_vtr_device_2, error);
    }
    if (recorder_server_barcode_scanner)
        CHECK_STRING_NOT_SET(recorder_barcode_fifo, error);
    CHECK_STRING_NOT_SET(profile_filename_suffix, error);
    CHECK_INT_NOT_SET(ring_buffer_size, error);
    CHECK_INT_NOT_SET(browse_overflow_frames, error);
    if (digibeta_dropout_lower_threshold >= digibeta_dropout_upper_threshold) {
        *error = "invalid digibeta_dropout_lower_threshold digibeta_dropout_upper_threshold values";
        return false;
    }
    if (palff_mode) {
        CHECK_ARRAY_NOT_SET(vitc_lines, error);
        CHECK_ARRAY_NOT_SET(ltc_lines, error);
    }
    if (videotape_backup)
        CHECK_ARRAY_NOT_SET(digibeta_barcode_prefixes, error);
    CHECK_INT_NOT_SET(chunking_throttle_fps, error);
    if (!read_analogue_ltc && !read_digital_ltc) {
        if (read_audio_track_ltc < 0) {
            *error = "read_audio_track_ltc not set";
            return false;
        }
        if (read_audio_track_ltc >= 8) {
            *error = "invalid read_audio_track_ltc value";
            return false;
        }
    }
    
    
    return true;
}

bool Config::validateTapeExportConfig(string *error)
{
    if (!validateCommon(error))
        return false;
    
    CHECK_STRING_NOT_SET(tape_export_www_root, error);
    CHECK_INT_NOT_SET(tape_export_www_port, error);
    CHECK_INT_NOT_SET(tape_export_www_threads, error);
    if (tape_export_server_barcode_scanner)
        CHECK_STRING_NOT_SET(tape_export_barcode_fifo, error);
    CHECK_STRING_NOT_SET(tape_device, error);
    CHECK_ARRAY_NOT_SET(lto_barcode_prefixes, error);
    CHECK_INT_NOT_SET(max_auto_files_on_lto, error);
    CHECK_INT_NOT_SET(max_lto_size_gb, error);
    
    return true;
}

bool Config::setConfigPairs(map<string, string> &config_pairs)
{
    // common
    CHECK_PARSE(parse_string(config_pairs, "log_dir", &log_dir));
    CHECK_PARSE(parse_int(config_pairs, "keep_logs", &keep_logs));
    CHECK_PARSE(parse_string(config_pairs, "cache_dir", &cache_dir));
    CHECK_PARSE(parse_string(config_pairs, "browse_dir", &browse_dir));
    CHECK_PARSE(parse_string(config_pairs, "pse_dir", &pse_dir));
    CHECK_PARSE(parse_string(config_pairs, "db_host", &db_host));
    CHECK_PARSE(parse_string(config_pairs, "db_name", &db_name));
    CHECK_PARSE(parse_string(config_pairs, "db_user", &db_user));
    CHECK_PARSE(parse_string(config_pairs, "db_password", &db_password));
    CHECK_PARSE(parse_string(config_pairs, "recorder_name", &recorder_name));
    CHECK_PARSE(parse_string(config_pairs, "tape_transfer_lock_file", &tape_transfer_lock_file));
    CHECK_PARSE(parse_string(config_pairs, "player_exe", &player_exe));
    
    // infax access
    CHECK_PARSE(parse_string(config_pairs, "local_infax_db_host", &local_infax_db_host));
    CHECK_PARSE(parse_string(config_pairs, "local_infax_db_name", &local_infax_db_name));
    CHECK_PARSE(parse_string(config_pairs, "local_infax_db_user", &local_infax_db_user));
    CHECK_PARSE(parse_string(config_pairs, "local_infax_db_password", &local_infax_db_password));

    // recorder
    CHECK_PARSE(parse_log_level(config_pairs, "recorder_log_level", &recorder_log_level));
    CHECK_PARSE(parse_int(config_pairs, "capture_log_level", &capture_log_level));
    CHECK_PARSE(parse_string(config_pairs, "recorder_www_root", &recorder_www_root));
    CHECK_PARSE(parse_int(config_pairs, "recorder_www_port", &recorder_www_port));
    CHECK_PARSE(parse_int(config_pairs, "recorder_www_replay_port", &recorder_www_replay_port));
    CHECK_PARSE(parse_int(config_pairs, "recorder_www_threads", &recorder_www_threads));
    CHECK_PARSE(parse_bool(config_pairs, "videotape_backup", &videotape_backup));
    CHECK_PARSE(parse_string_array(config_pairs, "source_vtr", &source_vtr));
    CHECK_PARSE(parse_string_array(config_pairs, "backup_vtr", &backup_vtr));
    CHECK_PARSE(parse_serial_type(config_pairs, "recorder_vtr_serial_type_1", &recorder_vtr_serial_type_1));
    CHECK_PARSE(parse_serial_type(config_pairs, "recorder_vtr_serial_type_2", &recorder_vtr_serial_type_2));
    CHECK_PARSE(parse_string(config_pairs, "recorder_vtr_device_1", &recorder_vtr_device_1));
    CHECK_PARSE(parse_string(config_pairs, "recorder_vtr_device_2", &recorder_vtr_device_2));
    CHECK_PARSE(parse_bool(config_pairs, "recorder_server_barcode_scanner", &recorder_server_barcode_scanner));
    CHECK_PARSE(parse_string(config_pairs, "recorder_barcode_fifo", &recorder_barcode_fifo));
    CHECK_PARSE(parse_string(config_pairs, "profile_directory", &profile_directory));
    CHECK_PARSE(parse_string(config_pairs, "profile_filename_suffix", &profile_filename_suffix));
    CHECK_PARSE(parse_int(config_pairs, "ring_buffer_size", &ring_buffer_size));
    CHECK_PARSE(parse_int(config_pairs, "browse_thread_count", &browse_thread_count));
    CHECK_PARSE(parse_int(config_pairs, "browse_overflow_frames", &browse_overflow_frames));
    CHECK_PARSE(parse_int(config_pairs, "digibeta_dropout_lower_threshold", &digibeta_dropout_lower_threshold));
    CHECK_PARSE(parse_int(config_pairs, "digibeta_dropout_upper_threshold", &digibeta_dropout_upper_threshold));
    CHECK_PARSE(parse_int(config_pairs, "digibeta_dropout_store_threshold", &digibeta_dropout_store_threshold));
    CHECK_PARSE(parse_bool(config_pairs, "palff_mode", &palff_mode));
    CHECK_PARSE(parse_int_array(config_pairs, "vitc_lines", &vitc_lines));
    CHECK_PARSE(parse_int_array(config_pairs, "ltc_lines", &ltc_lines));
    CHECK_PARSE(parse_string_array(config_pairs, "digibeta_barcode_prefixes", &digibeta_barcode_prefixes));
    CHECK_PARSE(parse_int(config_pairs, "chunking_throttle_fps", &chunking_throttle_fps));
    CHECK_PARSE(parse_bool(config_pairs, "enable_chunking_junk", &enable_chunking_junk));
    CHECK_PARSE(parse_bool(config_pairs, "enable_multi_item", &enable_multi_item));
    CHECK_PARSE(parse_int(config_pairs, "player_source_buffer_size", &player_source_buffer_size));
    CHECK_PARSE(parse_bool(config_pairs, "read_analogue_ltc", &read_analogue_ltc));
    CHECK_PARSE(parse_bool(config_pairs, "read_digital_ltc", &read_digital_ltc));
    CHECK_PARSE(parse_int(config_pairs, "read_audio_track_ltc", &read_audio_track_ltc));
    
    // tape export
    CHECK_PARSE(parse_log_level(config_pairs, "tape_export_log_level", &tape_export_log_level));
    CHECK_PARSE(parse_int(config_pairs, "tape_log_level", &tape_log_level));
    CHECK_PARSE(parse_string(config_pairs, "tape_export_www_root", &tape_export_www_root));
    CHECK_PARSE(parse_int(config_pairs, "tape_export_www_port", &tape_export_www_port));
    CHECK_PARSE(parse_int(config_pairs, "tape_export_www_threads", &tape_export_www_threads));
    CHECK_PARSE(parse_bool(config_pairs, "tape_export_server_barcode_scanner", &tape_export_server_barcode_scanner));
    CHECK_PARSE(parse_string(config_pairs, "tape_export_barcode_fifo", &tape_export_barcode_fifo));
    CHECK_PARSE(parse_string(config_pairs, "tape_device", &tape_device));
    CHECK_PARSE(parse_string_array(config_pairs, "lto_barcode_prefixes", &lto_barcode_prefixes));
    CHECK_PARSE(parse_int(config_pairs, "max_auto_files_on_lto", &max_auto_files_on_lto));
    CHECK_PARSE(parse_int(config_pairs, "max_lto_size_gb", &max_lto_size_gb));
    
    // debug
    CHECK_PARSE(parse_bool(config_pairs, "dbg_av_sync", &dbg_av_sync));
    CHECK_PARSE(parse_bool(config_pairs, "dbg_vitc_failures", &dbg_vitc_failures));
    CHECK_PARSE(parse_bool(config_pairs, "dbg_vga_replay", &dbg_vga_replay));
    CHECK_PARSE(parse_bool(config_pairs, "dbg_force_x11", &dbg_force_x11));
    CHECK_PARSE(parse_bool(config_pairs, "dbg_null_barcode_scanner", &dbg_null_barcode_scanner));
    CHECK_PARSE(parse_bool(config_pairs, "dbg_store_thread_buffer", &dbg_store_thread_buffer));
    CHECK_PARSE(parse_bool(config_pairs, "dbg_serial_port_open", &dbg_serial_port_open));
    CHECK_PARSE(parse_bool(config_pairs, "dbg_keep_lto_files", &dbg_keep_lto_files));
    
    return true;
}

map<string, string> Config::getConfigPairs()
{
    map<string, string> config_pairs;
    
    config_pairs["log_dir"] = log_dir;
    config_pairs["keep_logs"] = serialize_int(keep_logs);
    config_pairs["cache_dir"] = cache_dir;
    config_pairs["browse_dir"] = browse_dir;
    config_pairs["pse_dir"] = pse_dir;
    config_pairs["db_host"] = db_host;
    config_pairs["db_name"] = db_name;
    config_pairs["db_user"] = db_user;
    config_pairs["db_password"] = db_password;
    config_pairs["recorder_name"] = recorder_name;
    config_pairs["tape_transfer_lock_file"] = tape_transfer_lock_file;
    config_pairs["player_exe"] = player_exe;
    
    // infax access
    config_pairs["local_infax_db_host"] = local_infax_db_host;
    config_pairs["local_infax_db_name"] = local_infax_db_name;
    config_pairs["local_infax_db_user"] = local_infax_db_user;
    config_pairs["local_infax_db_password"] = local_infax_db_password;

    // recorder
    config_pairs["recorder_log_level"] = serialize_log_level(recorder_log_level);
    config_pairs["capture_log_level"] = serialize_int(capture_log_level);
    config_pairs["recorder_www_root"] = recorder_www_root;
    config_pairs["recorder_www_port"] = serialize_int(recorder_www_port);
    config_pairs["recorder_www_replay_port"] = serialize_int(recorder_www_replay_port);
    config_pairs["recorder_www_threads"] = serialize_int(recorder_www_threads);
    config_pairs["videotape_backup"] = serialize_bool(videotape_backup);
    config_pairs["source_vtr"] = serialize_string_array(source_vtr);
    config_pairs["backup_vtr"] = serialize_string_array(backup_vtr);
    config_pairs["recorder_vtr_serial_type_1"] = serialize_serial_type(recorder_vtr_serial_type_1);
    config_pairs["recorder_vtr_serial_type_2"] = serialize_serial_type(recorder_vtr_serial_type_2);
    config_pairs["recorder_vtr_device_1"] = recorder_vtr_device_1;
    config_pairs["recorder_vtr_device_2"] = recorder_vtr_device_2;
    config_pairs["recorder_server_barcode_scanner"] = serialize_bool(recorder_server_barcode_scanner);
    config_pairs["recorder_barcode_fifo"] = recorder_barcode_fifo;
    config_pairs["profile_directory"] = profile_directory;
    config_pairs["profile_filename_suffix"] = profile_filename_suffix;
    config_pairs["ring_buffer_size"] = serialize_int(ring_buffer_size);
    config_pairs["browse_thread_count"] = serialize_int(browse_thread_count);
    config_pairs["browse_overflow_frames"] = serialize_int(browse_overflow_frames);
    config_pairs["digibeta_dropout_lower_threshold"] = serialize_int(digibeta_dropout_lower_threshold);
    config_pairs["digibeta_dropout_upper_threshold"] = serialize_int(digibeta_dropout_upper_threshold);
    config_pairs["digibeta_dropout_store_threshold"] = serialize_int(digibeta_dropout_store_threshold);
    config_pairs["palff_mode"] = serialize_bool(palff_mode);
    config_pairs["vitc_lines"] = serialize_int_array(vitc_lines);
    config_pairs["ltc_lines"] = serialize_int_array(ltc_lines);
    config_pairs["digibeta_barcode_prefixes"] = serialize_string_array(digibeta_barcode_prefixes);
    config_pairs["chunking_throttle_fps"] = serialize_int(chunking_throttle_fps);
    config_pairs["enable_chunking_junk"] = serialize_bool(enable_chunking_junk);
    config_pairs["enable_multi_item"] = serialize_bool(enable_multi_item);
    config_pairs["player_source_buffer_size"] = serialize_int(player_source_buffer_size);
    config_pairs["read_analogue_ltc"] = serialize_bool(read_analogue_ltc);
    config_pairs["read_digital_ltc"] = serialize_bool(read_digital_ltc);
    config_pairs["read_audio_track_ltc"] = serialize_int(read_audio_track_ltc);
    
    // tape export
    config_pairs["tape_export_log_level"] = serialize_log_level(tape_export_log_level);
    config_pairs["tape_log_level"] = serialize_int(tape_log_level);
    config_pairs["tape_export_www_root"] = tape_export_www_root;
    config_pairs["tape_export_www_port"] = serialize_int(tape_export_www_port);
    config_pairs["tape_export_www_threads"] = serialize_int(tape_export_www_threads);
    config_pairs["tape_export_server_barcode_scanner"] = serialize_bool(tape_export_server_barcode_scanner);
    config_pairs["tape_export_barcode_fifo"] = tape_export_barcode_fifo;
    config_pairs["tape_device"] = tape_device;
    config_pairs["lto_barcode_prefixes"] = serialize_string_array(lto_barcode_prefixes);
    config_pairs["max_auto_files_on_lto"] = serialize_int(max_auto_files_on_lto);
    config_pairs["max_lto_size_gb"] = serialize_int(max_lto_size_gb);
    
    // debug
    config_pairs["dbg_av_sync"] = serialize_bool(dbg_av_sync);
    config_pairs["dbg_vitc_failures"] = serialize_bool(dbg_vitc_failures);
    config_pairs["dbg_vga_replay"] = serialize_bool(dbg_vga_replay);
    config_pairs["dbg_force_x11"] = serialize_bool(dbg_force_x11);
    config_pairs["dbg_null_barcode_scanner"] = serialize_bool(dbg_null_barcode_scanner);
    config_pairs["dbg_store_thread_buffer"] = serialize_bool(dbg_store_thread_buffer);
    config_pairs["dbg_serial_port_open"] = serialize_bool(dbg_serial_port_open);
    config_pairs["dbg_keep_lto_files"] = serialize_bool(dbg_keep_lto_files);
 
    return config_pairs;
}

bool Config::validateCommon(string *error)
{
    CHECK_STRING_NOT_SET(log_dir, error);
    CHECK_INT_NOT_SET(keep_logs, error);
    CHECK_STRING_NOT_SET(cache_dir, error);
    CHECK_STRING_NOT_SET(browse_dir, error);
    CHECK_STRING_NOT_SET(pse_dir, error);
    CHECK_STRING_NOT_SET(db_host, error);
    CHECK_STRING_NOT_SET(db_name, error);
    CHECK_STRING_NOT_SET(db_user, error);
    CHECK_STRING_NOT_SET(db_password, error);
    CHECK_STRING_NOT_SET(recorder_name, error);
    CHECK_STRING_NOT_SET(tape_transfer_lock_file, error);
    CHECK_STRING_NOT_SET(player_exe, error);
    
    return true;
}

