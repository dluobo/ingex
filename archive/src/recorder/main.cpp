/*
 * $Id: main.cpp,v 1.2 2010/09/01 16:05:22 philipn Exp $
 *
 * Recorder application main function
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

#include <curl/curl.h>

#include "logF.h"

#include "Recorder.h"
#include "BarcodeScanner.h"
#include "BarcodeScannerFIFO.h"
#include "HTTPRecorder.h"
#include "HTTPVTRControl.h"
#include "HTTPServer.h"
#include "RecorderException.h"
#include "RecorderDatabase.h"
#include "InfaxAccess.h"
#include "Logging.h"
#include "MXFCommon.h"
#include "Utilities.h"
#include "Config.h"
#include "version.h"

using namespace std;
using namespace rec;


static const char* g_recorderLogPrefix = "recorder_";
static const char* g_recorderLogSuffix = ".log";
static const char* g_captureLogPrefix = "capture_";
static const char* g_captureLogSuffix = ".log";



// from version.h

string rec::get_version()
{
    return "0.11.0";
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
    fprintf(stderr, "  -h, --help                   Display this usage message and exit\n");
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
    string browseDir;
    string pseDir;
    string dbHost;
    string dbName;
    string dbUser;
    string dbPassword;
    string documentRoot;
    int port;
    int replayPort;
    string vtrSerialDeviceName1;
    string vtrSerialDeviceName2;
    string scannerFIFO;
    int realUserId = getuid();
    int numHTTPThreads;
    int keepLogs;
    bool haveBarcodeScanner;
    bool debugBarcodeScanner = false;
    
#if !defined(ENABLE_DEBUG)
    if (realUserId == 0)
    {
        fprintf(stderr, "ERROR: Running recorder with root user as real user is not permitted\n");
        return 1;
    }
    if (geteuid() != 0)
    {
        fprintf(stderr, "ERROR: Running recorder with root user not the effective user is not permitted\n");
        fprintf(stderr, "Change the ownership of '%s' to root and set the set-user-id bit\n", argv[0]);
        return 1;
    }
#endif
    
    
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


    // drop root privileges by setting the effective user to the real user
    if (seteuid(realUserId) != 0)
    {
        fprintf(stderr, "Failed to set effective user: %s\n", strerror(errno));
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
    if (!Config::validateRecorderConfig(&configError))
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

    logLevel = Config::recorder_log_level;
    
    cacheDir = Config::cache_dir;
    if (!check_absolute_directory_exists(cacheDir, "Cache"))
    {
        return 1;
    }
    
    browseDir = Config::browse_dir;
    if (!check_absolute_directory_exists(browseDir, "Browse"))
    {
        return 1;
    }
    
    pseDir = Config::pse_dir;
    if (!check_absolute_directory_exists(pseDir, "PSE"))
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
    replayPort = Config::recorder_www_replay_port;
    numHTTPThreads = Config::recorder_www_threads;
    
    vtrSerialDeviceName1 = Config::recorder_vtr_device_1;
    vtrSerialDeviceName2 = Config::recorder_vtr_device_2;

    haveBarcodeScanner = Config::recorder_server_barcode_scanner;
    if (haveBarcodeScanner)
    {
        debugBarcodeScanner = Config::dbg_null_barcode_scanner;
        scannerFIFO = Config::recorder_barcode_fifo;
        if (!file_exists(scannerFIFO))
        {
            fprintf(stderr, "Scanner FIFO file '%s' does not exist\n", scannerFIFO.c_str());
            return 1;
        }
    }
    
    keepLogs = Config::keep_logs;
    

    
    // initialise the curl library
    curl_global_init(CURL_GLOBAL_ALL);
    
    
    // clear old log files
    clear_old_log_files(logDir, g_recorderLogPrefix, g_recorderLogSuffix, keepLogs);
    clear_old_log_files(logDir, g_captureLogPrefix, g_captureLogSuffix, keepLogs);

    
    // initialise logging to file and console
    Date logFileStartDate = get_todays_date();
    string recorderLogFilename = join_path(logDir, add_timestamp_to_filename(g_recorderLogPrefix, g_recorderLogSuffix));
    string captureLogFilename = join_path(logDir, add_timestamp_to_filename(g_captureLogPrefix, g_captureLogSuffix));
    try
    {
        Logging::initialise(new DualLogging(recorderLogFilename, logLevel));
        openLogFile(captureLogFilename.c_str());
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

    Logging::info("Starting recorder '%s'...\n", recorderName.c_str());
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
        Logging::error("Failed to initialise the postgres database: Recorder exception: %s\n", ex.getMessage().c_str());
        return 1;
    }
    
    // initialize the Infax access
    try
    {
        InfaxAccess::initialise();
    }
    catch (const RecorderException& ex)
    {
        Logging::error("Failed to initialise Infax access: Recorder exception: %s\n", ex.getMessage().c_str());
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
            Logging::error("Recorder exception: %s\n", ex.getMessage().c_str());
            return 1;
        }
    }
    
    // start
    try
    {
        // add an alias to the pse directory so that we can html link to files
        vector<pair<string, string> > pseAlias;
        pseAlias.push_back(pair<string, string>(HTTPRecorder::getPSEReportsURL(), pseDir));
        HTTPServer httpServer(port, documentRoot, pseAlias, numHTTPThreads);

        Recorder recorder(recorderName, cacheDir, browseDir, pseDir, replayPort, 
            vtrSerialDeviceName1, vtrSerialDeviceName2);

#if !defined(ENABLE_DEBUG)
        // drop root privileges permanently now that the recorder has initialized
        if (seteuid(0) != 0)
        {
            throw RecorderException("Failed to set effective user to root user: %s", strerror(errno));
        }
        if (setuid(realUserId) != 0)
        {
            throw RecorderException("Failed to set user: %s", strerror(errno));
        }
        if (setuid(0) != -1)
        {
            throw RecorderException("Failed to drop privileges permanently: root privilege could be restored");
        }
#endif
        
        HTTPRecorder httpRec(&httpServer, &recorder, scanner);
        HTTPVTRControl httpVTRControl(&httpServer, recorder.getVTRControls());

        httpServer.start();
        
        
        while (true)
        {
            // sleep 5 minutes
            sleep_sec(5 * 60);

            
            // check if the log files must be moved to next day
            
            Date dateNow = get_todays_date();
            if (logFileStartDate != dateNow)
            {
                string newRecorderLogFilename = join_path(logDir, add_timestamp_to_filename(g_recorderLogPrefix, g_recorderLogSuffix));
                if (newRecorderLogFilename != recorderLogFilename)
                {
                    FileLogging* fileLogging = dynamic_cast<FileLogging*>(Logging::getInstance());
                    if (fileLogging != 0)
                    {
                        Logging::info("Reopening log file '%s'\n", newRecorderLogFilename.c_str());
                        if (fileLogging->reopenLogFile(newRecorderLogFilename))
                        {
                            Logging::info("Reopened log file from '%s'\n", recorderLogFilename.c_str());
                            recorderLogFilename = newRecorderLogFilename;
                            
                            clear_old_log_files(logDir, g_recorderLogPrefix, g_recorderLogSuffix, keepLogs);    
    
                            Logging::info("Recorder '%s'\n", recorderName.c_str());
                            Logging::info("Version: %s (build %s)\n", get_version().c_str(), get_build_date().c_str());
                            Logging::info("Configuration: %s\n", Config::writeToString().c_str());
                        }
                        else
                        {
                            Logging::error("Failed to reopen log file '%s'\n", newRecorderLogFilename.c_str());
                        }
                    }
                }
                
                string newCaptureLogFilename = join_path(logDir, add_timestamp_to_filename(g_captureLogPrefix, g_captureLogSuffix));
                if (newCaptureLogFilename != captureLogFilename)
                {
                    logTF("Reopening log file '%s'\n", newCaptureLogFilename.c_str());
                    if (reopenLogFile(newCaptureLogFilename.c_str()))
                    {
                        logTF("Reopened log file from '%s'\n", newCaptureLogFilename.c_str());
                        captureLogFilename = newCaptureLogFilename;
                        
                        clear_old_log_files(logDir, g_captureLogPrefix, g_captureLogSuffix, keepLogs);
                    }
                    else
                    {
                        logTF("ERROR: Failed to reopen log file '%s'\n", newCaptureLogFilename.c_str());
                    }
                }

                logFileStartDate = dateNow; // whatever happens we try only once per day
            }
        }
    }
    catch (const RecorderException& ex)
    {
        Logging::error("Recorder exception: %s\n", ex.getMessage().c_str());
        return 1;
    }
    
    delete scanner;
    
    RecorderDatabase::close();

    curl_global_cleanup();
    
    return 0;
}


