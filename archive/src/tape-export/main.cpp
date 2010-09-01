/*
 * $Id: main.cpp,v 1.2 2010/09/01 16:05:23 philipn Exp $
 *
 * Tape export main function
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "logF.h"

#include "TapeExport.h"
#include "Config.h"
#include "RecorderDatabase.h"
#include "BarcodeScanner.h"
#include "BarcodeScannerFIFO.h"
#include "HTTPTapeExport.h"
#include "HTTPServer.h"
#include "RecorderException.h"
#include "Logging.h"
#include "MXFCommon.h"
#include "Utilities.h"
#include "Timing.h"
#include "version.h"

using namespace std;
using namespace rec;


static const char* g_tapeExportLogPrefix = "tape_export_";
static const char* g_tapeExportLogSuffix = ".log";
static const char* g_tapeLogPrefix = "tape_";
static const char* g_tapeLogSuffix = ".log";



// from version.h

string rec::get_version()
{
    return "0.9.6";
}

string rec::get_build_date()
{
    return __DATE__;
}


static bool check_absolute_directory_exists(string path, string descriptiveName)
{
    if (!directory_path_is_absolute(path))
    {
        fprintf(stderr, "%s directory '%s' is not an absolute path\n", descriptiveName.c_str(), path.c_str());
        return false;
    }
    else if (!directory_exists(path))
    {
        fprintf(stderr, "%s directory '%s' does not exist\n", descriptiveName.c_str(), path.c_str());
        return false;
    }
    
    return true;
}


static void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s [options]\n", cmd);
    fprintf(stderr, "Options: (* means is required)\n");
    fprintf(stderr, "  -h, --help                   Display this usage message\n");
    fprintf(stderr, "  -v | --version               Display the version and exit\n");
    fprintf(stderr, "* -c | --config <name>         The configuration filename\n");
}

int main(int argc, const char** argv)
{
    int cmdlnIndex = 1;
    string configFilename;
    string recorderName;
    string logDir;
    LogLevel logLevel;
    string cacheDir;
    string pseDir;
    string dbHost;
    string dbName;
    string dbUser;
    string dbPassword;
    string documentRoot;
    int port;
    string tapeDeviceName;
    string scannerFIFO;
    int numHTTPThreads;
    int keepLogs;
    bool haveBarcodeScanner;
    bool debugBarcodeScanner = false;

    
    // parse command line arguments
    while (cmdlnIndex < argc)
    {
        if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
            strcmp(argv[cmdlnIndex], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdlnIndex], "-v") == 0 ||
            strcmp(argv[cmdlnIndex], "--version") == 0)
        {
            printf("%s (build %s)\n", get_version().c_str(), get_build_date().c_str());
            return 0;
        }
        else if (strcmp(argv[cmdlnIndex], "-c") == 0 ||
            strcmp(argv[cmdlnIndex], "--config") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            configFilename = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else
        {
            usage(argv[0]);
            fprintf(stderr, "Unknown argument '%s'\n", argv[cmdlnIndex]);
            return 1;
        }
    }    
    
    if (configFilename.empty())
    {
        usage(argv[0]);
        fprintf(stderr, "Missing --config argument\n");
        return 1;
    }
    
    // read and validate config
    
    if (!Config::readFromFile(configFilename))
    {
        fprintf(stderr, "Failed to read configuration from '%s'\n", configFilename.c_str());
        return 1;
    }

    if (Config::recorder_name.empty())
    {
        string hostName = get_host_name();
        if (hostName.empty())
        {
            fprintf(stderr, "Failed to get the hostname to set the default recorder name: %s\n", strerror(errno));
            return 1;
        }
        Config::recorder_name = hostName;
    }
    
    string configError;
    if (!Config::validateTapeExportConfig(&configError))
    {
        fprintf(stderr, "Invalid configuration: %s\n", configError.c_str());
        return 1;
    }
    
    
    // read config values and do further checks

    logDir = Config::log_dir;
    if (!directory_exists(logDir))
    {
        fprintf(stderr, "Log directory '%s' does not exist\n", logDir.c_str());
        return 1;
    }

    logLevel = Config::tape_export_log_level;
    
    cacheDir = Config::cache_dir;
    if (!check_absolute_directory_exists(cacheDir, "Cache"))
    {
        return 1;
    }
    
    dbHost = Config::db_host;
    dbName = Config::db_name;
    dbUser = Config::db_user;
    dbPassword = Config::db_password;
    
    recorderName = Config::recorder_name;
    
    documentRoot = Config::recorder_www_root;
    port = Config::recorder_www_port;
    numHTTPThreads = Config::recorder_www_threads;
    
    tapeDeviceName = Config::tape_device;
    
    haveBarcodeScanner = Config::tape_export_server_barcode_scanner;
    if (haveBarcodeScanner)
    {
        debugBarcodeScanner = Config::dbg_null_barcode_scanner;
        scannerFIFO = Config::tape_export_barcode_fifo;
        if (!file_exists(scannerFIFO))
        {
            fprintf(stderr, "Scanner FIFO file '%s' does not exist\n", scannerFIFO.c_str());
            return 1;
        }
    }
    
    keepLogs = Config::keep_logs;

    
    // clear old log files
    clear_old_log_files(logDir, g_tapeExportLogPrefix, g_tapeExportLogSuffix, keepLogs);
    clear_old_log_files(logDir, g_tapeLogPrefix, g_tapeLogSuffix, keepLogs);
    
    
    // initialise logging to file and console
    Date logFileStartDate = get_todays_date();
    string tapeExportLogFilename = join_path(logDir, add_timestamp_to_filename(g_tapeExportLogPrefix, g_tapeExportLogSuffix));
    string tapeLogFilename = join_path(logDir, add_timestamp_to_filename(g_tapeLogPrefix, g_tapeLogSuffix));
    try
    {
        Logging::initialise(new DualLogging(tapeExportLogFilename, logLevel));
        openLogFile(tapeLogFilename.c_str());
    }
    catch (const RecorderException& ex)
    {
        fprintf(stderr, "Failed to initialise logging: Recorder exception: %s\n", ex.getMessage().c_str());
        return 1;
    }
    catch (...)
    {
        fprintf(stderr, "Failed to initialise logging: Unknown exception\n");
        return 1;
    }

    Logging::info("Starting tape export for recorder '%s'...\n", recorderName.c_str());
    Logging::info("Version: %s (build %s)\n", get_version().c_str(), get_build_date().c_str());
    Logging::info("Configuration: %s\n", Config::writeToString().c_str());

    
    // redirect libMXF log messages
    redirect_mxf_logging();
    
    
    // initialise the database
    try
    {
        PostgresRecorderDatabase::initialise(dbHost, dbName, dbUser, dbPassword);
    }
    catch (const RecorderException& ex)
    {
        fprintf(stderr, "Failed to initialise the postgres database: Recorder exception: %s\n", ex.getMessage().c_str());
        return 1;
    }

    // open the scanner - either using exclusive device access or a fifo written to by an another process
    BarcodeScanner* scanner = 0;
    if (haveBarcodeScanner)
    {
        try
        {
            if (debugBarcodeScanner)
            {
                scanner = new BarcodeScanner();
                Logging::warning("Using null barcode scanner device\n");
            }
            else if (scannerFIFO.size() > 0)
            {
                scanner = new BarcodeScannerFIFO(scannerFIFO);
            }
            else
            {
                scanner = new BarcodeScannerDevice();
            }
        }
        catch (const RecorderException& ex)
        {
            fprintf(stderr, "Recorder exception: %s\n", ex.getMessage().c_str());
            return 1;
        }
    }
    
    // start
    string newTapeExportLogFilename;
    string newTapeLogFilename;
    try
    {
        // add an alias to the pse directory so that we can html link to files
        vector<pair<string, string> > pseAlias;
        pseAlias.push_back(pair<string, string>(HTTPTapeExport::getPSEReportsURL(), pseDir));
        HTTPServer httpServer(port, documentRoot, pseAlias, numHTTPThreads);
        
        TapeExport tapeExport(tapeDeviceName, recorderName, cacheDir);
        HTTPTapeExport httpTapeExport(&httpServer, &tapeExport, scanner);
        
        httpServer.start();
            
        while (true)
        {
            // sleep 5 minutes
            sleep_sec(5 * 60);
            
            
            // check if the log files must be moved to next day
            
            Date dateNow = get_todays_date();
            if (dateNow != logFileStartDate)
            {
                string newTapeExportLogFilename = join_path(logDir, add_timestamp_to_filename(g_tapeExportLogPrefix, g_tapeExportLogSuffix));
                if (newTapeExportLogFilename != tapeExportLogFilename)
                {
                    FileLogging* fileLogging = dynamic_cast<FileLogging*>(Logging::getInstance());
                    if (fileLogging != 0)
                    {
                        Logging::info("Reopening log file '%s'\n", newTapeExportLogFilename.c_str());
                        if (fileLogging->reopenLogFile(newTapeExportLogFilename))
                        {
                            Logging::info("Reopened log file from '%s'\n", tapeExportLogFilename.c_str());
                            tapeExportLogFilename = newTapeExportLogFilename;
                            
                            clear_old_log_files(logDir, g_tapeExportLogPrefix, g_tapeExportLogSuffix, keepLogs);    
    
                            Logging::info("Tape export for recorder '%s'\n", recorderName.c_str());
                            Logging::info("Version: %s (build %s)\n", get_version().c_str(), get_build_date().c_str());
                            Logging::info("Configuration: %s\n", Config::writeToString().c_str());
                        }
                        else
                        {
                            Logging::error("Failed to reopen log file '%s'\n", newTapeExportLogFilename.c_str());
                        }
                    }
                }
            
                string newTapeLogFilename = join_path(logDir, add_timestamp_to_filename(g_tapeLogPrefix, g_tapeLogSuffix));
                if (newTapeLogFilename != tapeLogFilename)
                {
                    logTF("Reopening log file '%s'\n", newTapeLogFilename.c_str());
                    if (reopenLogFile(newTapeLogFilename.c_str()))
                    {
                        logTF("Reopened log file from '%s'\n", newTapeLogFilename.c_str());
                        tapeLogFilename = newTapeLogFilename;
    
                        clear_old_log_files(logDir, g_tapeLogPrefix, g_tapeLogSuffix, keepLogs);    
                    }
                    else
                    {
                        logTF("ERROR: Failed to reopen log file '%s'\n", newTapeLogFilename.c_str());
                    }
                }
                
                logFileStartDate = dateNow; // whatever happens we try only once per day
            }
        }
    }
    catch (const RecorderException& ex)
    {
        fprintf(stderr, "Recorder exception: %s\n", ex.getMessage().c_str());
        return 1;
    }

    delete scanner;
    
    PostgresRecorderDatabase::close();
    
    return 0;
}


