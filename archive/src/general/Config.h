/*
 * $Id: Config.h,v 1.2 2010/09/01 16:05:22 philipn Exp $
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

#ifndef __RECORDER_CONFIG_H__
#define __RECORDER_CONFIG_H__

#include <string>
#include <map>
#include <vector>

#include "Logging.h"
#include "SerialPort.h"


namespace rec
{


class Config
{
public:
    static bool readFromFile(std::string filename);
    static bool writeToFile(std::string filename);
    static std::string writeToString();
    
    static bool validateRecorderConfig(std::string *error);
    static bool validateTapeExportConfig(std::string *error);

public:
    // common
    
    static std::string log_dir;
    static int keep_logs;
    
    static std::string cache_dir;
    static std::string browse_dir;
    static std::string pse_dir;
    
    static std::string db_host;
    static std::string db_name;
    static std::string db_user;
    static std::string db_password;
    
    static std::string recorder_name;
    static std::string tape_transfer_lock_file;
    
    static std::string player_exe;
    
    
    // infax access
    
    static std::string local_infax_db_host;
    static std::string local_infax_db_name;
    static std::string local_infax_db_user;
    static std::string local_infax_db_password;

    
    // recorder
    
    static LogLevel recorder_log_level;
    static int capture_log_level;
    
    static std::string recorder_www_root;
    static int recorder_www_port;
    static int recorder_www_replay_port;
    static int recorder_www_threads;
    
    static bool videotape_backup;
    static std::vector<std::string> source_vtr;
    static std::vector<std::string> backup_vtr;
    static SerialType recorder_vtr_serial_type_1;
    static SerialType recorder_vtr_serial_type_2;
    static std::string recorder_vtr_device_1;
    static std::string recorder_vtr_device_2;
    
    static bool recorder_server_barcode_scanner;
    static std::string recorder_barcode_fifo;
    
    static std::string profile_directory;
    static std::string profile_filename_suffix;
    
    static int ring_buffer_size;
    
    static int browse_thread_count;
    static int browse_overflow_frames;
    
    static int digibeta_dropout_lower_threshold;
    static int digibeta_dropout_upper_threshold;
    static int digibeta_dropout_store_threshold;
    
    static bool palff_mode;
    static std::vector<int> vitc_lines;
    static std::vector<int> ltc_lines;
    
    static std::vector<std::string> digibeta_barcode_prefixes;

    static int chunking_throttle_fps;

    static bool enable_chunking_junk;
    static bool enable_multi_item;
    
    static int player_source_buffer_size;
    
    static bool read_analogue_ltc;
    static bool read_digital_ltc;
    static int read_audio_track_ltc;
    
    
    // tape export
    
    static LogLevel tape_export_log_level;
    static int tape_log_level;
    
    static std::string tape_export_www_root;
    static int tape_export_www_port;
    static int tape_export_www_threads;
    
    static bool tape_export_server_barcode_scanner;
    static std::string tape_export_barcode_fifo;
    
    static std::string tape_device;
    
    static std::vector<std::string> lto_barcode_prefixes;
    
    static int max_auto_files_on_lto;
    static int max_lto_size_gb;
    
    
    // debug
    
    static bool dbg_av_sync;
    static bool dbg_vitc_failures;
    static bool dbg_vga_replay;
    static bool dbg_force_x11;
    static bool dbg_null_barcode_scanner;
    static bool dbg_store_thread_buffer;
    static bool dbg_serial_port_open;
    static bool dbg_keep_lto_files;
    

private:
    static bool setConfigPairs(std::map<std::string, std::string> &name_values);
    static std::map<std::string, std::string> getConfigPairs();
    
    static bool validateCommon(std::string *error);
};


};


#endif
