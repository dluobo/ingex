/*
 * $Id: create_aaf.cpp,v 1.2 2007/02/08 11:02:38 philipn Exp $
 *
 * Creates AAF files with clips extracted from the database
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <sstream>

#include "CreateAAF.h"
#include "CreateAAFException.h"
#include <Database.h>
#include <MCClipDef.h>
#include <Utilities.h>
#include <DBException.h>

using namespace std;
using namespace prodauto;



static const char* g_dns = "prodautodb";
static const char* g_databaseUserName = "";
static const char* g_databasePassword = "";

static const char* g_filenamePrefix = "pilot";


// utility class to clean-up Package pointers
class MaterialHolder
{
public:
    ~MaterialHolder()
    {
        PackageSet::iterator iter;
        for (iter = packages.begin(); iter != packages.end(); iter++)
        {
            delete *iter;
        }
        
        // topPackages only holds references so don't delete
    }
    
    MaterialPackageSet topPackages; 
    PackageSet packages;
};


static Timestamp parseTimestamp(string timestampStr)
{
    int data[6];
    
    if (strchr(timestampStr.c_str(), 'L') != NULL)
    {
        if (sscanf(timestampStr.c_str(), "%u-%u-%uL%u:%u:%u", &data[0], &data[1],
            &data[2], &data[3], &data[4], &data[5]) != 6)
        {
            throw "Invalid timestamp string";
        }
        
        // convert broken down time structure to calendar time
        struct tm localTime;
        time_t t;
        localTime.tm_year = data[0] - 1900;
        localTime.tm_mon = data[1] - 1;
        localTime.tm_mday = data[2];
        localTime.tm_hour = data[3];
        localTime.tm_min = data[4];
        localTime.tm_sec = data[5];
        localTime.tm_isdst = -1;
        if ((t = mktime(&localTime)) == (time_t)(-1))
        {
            throw "Failed to convert timestamp to calendar time";
        }
        
        
        // convert calendar time to UTC time
        struct tm gmt;
        memset(&gmt, 0, sizeof(struct tm));
#if defined(_MSVC)
        // TODO: need thread-safe (reentrant) version
        const struct tm* gmtPtr = gmtime(&t);
        if (gmtPtr == NULL)
        {
            throw "Failed to convert calendar time to UTC";
        }
        gmt = *gmtPtr;
#else
        if (gmtime_r(&t, &gmt) == NULL)
        {
            throw "Failed to convert calendar time to UTC";
        }
#endif
        Timestamp timestamp;
        timestamp.year = gmt.tm_year + 1900;
        timestamp.month = gmt.tm_mon + 1;
        timestamp.day = gmt.tm_mday;
        timestamp.hour = gmt.tm_hour;
        timestamp.min = gmt.tm_min;
        timestamp.sec = gmt.tm_sec;
        timestamp.qmsec = 0;
        
        return timestamp;
    }
    else
    {
        if (sscanf(timestampStr.c_str(), "%u-%u-%uU%u:%u:%u", &data[0], &data[1],
            &data[2], &data[3], &data[4], &data[5]) != 6)
        {
            throw "Invalid timestamp string";
        }
        
        Timestamp timestamp;
        timestamp.year = data[0];
        timestamp.month = data[1];
        timestamp.day = data[2];
        timestamp.hour = data[3];
        timestamp.min = data[4];
        timestamp.sec = data[5];
        timestamp.qmsec = 0;
        
        return timestamp;
    }        

}

static void parseTag(string tag, string& tagName, string& tagValue)
{
    size_t index;
    if ((index = tag.find("=")) != string::npos && index + 1 != tag.size())
    {
        tagName = tag.substr(0, index);
        tagValue = tag.substr(index + 1, tag.size() - index);
    }
    else
    {
        throw "Failed to parse tag";
    }
}


static string timestampString(Timestamp timestamp)
{
    char timestampStr[22];
    
#if defined(_MSC_VER)    
    _snprintf(
#else
    snprintf(
#endif
        timestampStr, 22, "%04d%02u%02u%02u%02u%02u",
        timestamp.year, timestamp.month, timestamp.day, 
        timestamp.hour, timestamp.min, timestamp.sec);
        
    return timestampStr;
}

static string createTimestampSuffix(Timestamp from, Timestamp to)
{
    stringstream suffix;
    
    suffix << timestampString(from) << "_" << timestampString(to);
        
    return suffix.str();
}

static string createTagSuffix()
{
    return timestampString(generateTimestampNow());
}

static string createSingleClipFilename(string prefix, string suffix, int index)
{
    stringstream filename;
    
    filename << prefix << "_" << suffix << "_" << index << ".aaf";
        
    return filename.str();
}

static string createMCClipFilename(string prefix, int64_t startTime, string suffix, int index)
{
    stringstream filename;
    
    filename << prefix << "_mc_" << startTime << "_"  << suffix << "_" << index << ".aaf";
        
    return filename.str();
}

static string createGroupSingleFilename(string prefix, string suffix)
{
    stringstream filename;
    
    filename << prefix << "_g_" << suffix << ".aaf";
        
    return filename.str();
}

static string createGroupMCFilename(string prefix, string suffix)
{
    stringstream filename;
    
    filename << prefix << "_g_mc_" << suffix << ".aaf";
        
    return filename.str();
}

static string addFilename(vector<string>& filenames, string filename)
{
    filenames.push_back(filename);
    return filename;
}

static void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s <<options>>\n", cmd);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help                 Display this usage message\n");
    fprintf(stderr, "  -p, --prefix <filepath>    Filename prefix used for AAF files (default '%s')\n", g_filenamePrefix);
    fprintf(stderr, "  -r --resolution <id>       Video resolution identifier as listed in the database\n"); 
    fprintf(stderr, "                             (default is %d for DV-50)\n", DV50_MATERIAL_RESOLUTION);
    fprintf(stderr, "  -g, --group                Create AAF file with all clips included\n");
    fprintf(stderr, "  -o, --grouponly            Only create AAF file with all clips included\n");
    fprintf(stderr, "  -m, --multicam             Also create multi-camera clips\n");
    fprintf(stderr, "  -f, --from <timestamp>     Includes clips created >= timestamp\n");
    fprintf(stderr, "  -t, --to <timestamp>       Includes clips created < timestamp\n");
    fprintf(stderr, "  --tag <name>=<value>       Includes clips with user comment tag <name> == <value>\n");
    fprintf(stderr, "  -d, --dns <string>         Database DNS (default '%s')\n", g_dns);
    fprintf(stderr, "  -u, --dbuser <string>      Database user name (default '%s')\n", g_databaseUserName);
    fprintf(stderr, "  --dbpassword <string>      Database user password (default ***)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Notes:\n");
    fprintf(stderr, "* Timestamps are of the form yyyy-mm-ddXhh:mm:ss, where X is L for local time and U for UTC\n");
    fprintf(stderr, "* Timestamps are always stored in UTC\n");
    fprintf(stderr, "* Timestamps are for the creation date and not the start times of the clips\n");
    fprintf(stderr, "* The default for --from is the start of today at 00:00:00\n"); 
    fprintf(stderr, "* The default for --to is the start of tommorrow at 00:00:00\n");
    fprintf(stderr, "* If --tag is used then --to and --from are ignored\n");
}


int main(int argc, const char* argv[])
{
    int cmdlnIndex = 1;
    string filenamePrefix = g_filenamePrefix;
    bool createGroup = false;
    bool createGroupOnly = false;
    bool createMultiCam = false;
    Timestamp from = generateTimestampStartToday();
    Timestamp to = generateTimestampStartTomorrow();
    string dns = g_dns;
    string dbUserName = g_databaseUserName;
    string dbPassword = g_databasePassword;
    string tagName;
    string tagValue;
    string suffix;
    vector<string> filenames;
    int videoResolutionID = DV50_MATERIAL_RESOLUTION;
    
    
    try
    {
        while (cmdlnIndex < argc)
        {
            if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
                strcmp(argv[cmdlnIndex], "--help") == 0)
            {
                usage(argv[0]);
                return 0;
            }
            else if (strcmp(argv[cmdlnIndex], "-p") == 0 ||
                strcmp(argv[cmdlnIndex], "--prefix") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                filenamePrefix = argv[cmdlnIndex + 1];
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "-r") == 0 ||
                strcmp(argv[cmdlnIndex], "--resolution") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for --resolution\n");
                    return 1;
                }
                if (sscanf(argv[cmdlnIndex + 1], "%d\n", &videoResolutionID) != 1)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Invalid argument for --resolution\n");
                    return 1;
                }
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "-g") == 0 ||
                strcmp(argv[cmdlnIndex], "--group") == 0)
            {
                createGroup = true;
                cmdlnIndex += 1;
            }
            else if (strcmp(argv[cmdlnIndex], "-o") == 0 ||
                strcmp(argv[cmdlnIndex], "--grouponly") == 0)
            {
                createGroupOnly = true;
                createGroup = true;
                cmdlnIndex += 1;
            }
            else if (strcmp(argv[cmdlnIndex], "-m") == 0 ||
                strcmp(argv[cmdlnIndex], "--multicam") == 0)
            {
                createMultiCam = true;
                cmdlnIndex += 1;
            }
            else if (strcmp(argv[cmdlnIndex], "-f") == 0 ||
                strcmp(argv[cmdlnIndex], "--from") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                from = parseTimestamp(argv[cmdlnIndex + 1]);
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "-t") == 0 ||
                strcmp(argv[cmdlnIndex], "--to") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                to = parseTimestamp(argv[cmdlnIndex + 1]);
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "--tag") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                parseTag(argv[cmdlnIndex + 1], tagName, tagValue);
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "-d") == 0 ||
                strcmp(argv[cmdlnIndex], "--dns") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                dns = argv[cmdlnIndex + 1];
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "-u") == 0 ||
                strcmp(argv[cmdlnIndex], "--dbuser") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                dbUserName = argv[cmdlnIndex + 1];
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "--dbpassword") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                dbPassword = argv[cmdlnIndex + 1];
                cmdlnIndex += 2;
            }
            else
            {
                usage(argv[0]);
                fprintf(stderr, "Unknown argument '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
        }
    }
    catch (const char*& ex)
    {
        usage(argv[0]);
        fprintf(stderr, "\nFailed to parse arguments:\n%s\n", ex);
        return 1;
    }
    catch (...)
    {
        usage(argv[0]);
        fprintf(stderr, "\nFailed to parse arguments:\nUnknown exception thrown\n");
        return 1;
    }

    if (tagName.size() > 0)
    {
        suffix = createTagSuffix();
    }
    else
    {
        suffix = createTimestampSuffix(from, to);
    }
    
    try
    {
        Database::initialise(dns, dbUserName, dbPassword, 1, 3);
    }
    catch (DBException& ex)
    {
        fprintf(stderr, "Failed to connect to database:\n  %s\n", ex.getMessage().c_str());
        return 1;
    }


    // load the material    
    Database* database = Database::getInstance();
    auto_ptr<AAFFile> aafGroup;
    MaterialHolder material;
    try
    {
        if (tagName.size() != 0)
        {
            // load all material with tagged value name = tagName and value = tagValue
            database->loadMaterial(tagName.c_str(), tagValue.c_str(), &material.topPackages, &material.packages);
        }
        else
        {
            // load all material created >= from and < to
            database->loadMaterial(from, to, &material.topPackages, &material.packages);
        }
    }
    catch (const char*& ex)
    {
        fprintf(stderr, "\nFailed to create:\n%s\n", ex);
        Database::close();
        return 1;
    }
    catch (ProdAutoException& ex)
    {
        fprintf(stderr, "\nFailed to create:\n%s\n", ex.getMessage().c_str());
        Database::close();    
        return 1;
    }
    catch (...)
    {
        fprintf(stderr, "\nFailed to create:\nUnknown exception thrown\n");
        Database::close();    
        return 1;
    }

    // go through the material package -> file package and remove any that reference
    // a file package with a non-zero videoResolutionID and !=  targetVideoResolutionID
    MaterialPackageSet::iterator iter1 = material.topPackages.begin();
    while (iter1 != material.topPackages.end())
    {
        MaterialPackage* topPackage = *iter1;
        
        vector<Track*>::const_iterator iter2;
        for (iter2 = topPackage->tracks.begin(); iter2 != topPackage->tracks.end(); iter2++)
        {
            Track* track = *iter2;
            
            SourcePackage dummy;
            dummy.uid = track->sourceClip->sourcePackageUID;
            PackageSet::iterator result = material.packages.find(&dummy);
            if (result != material.packages.end())
            {
                Package* package = *result;
                
                if (package->getType() != SOURCE_PACKAGE || package->tracks.size() == 0)
                {
                    continue;
                }
                SourcePackage* sourcePackage = dynamic_cast<SourcePackage*>(package);
                if (sourcePackage->descriptor->getType() != FILE_ESSENCE_DESC_TYPE)
                {
                    continue;
                }
                
                FileEssenceDescriptor* fileDescriptor = dynamic_cast<FileEssenceDescriptor*>(
                    sourcePackage->descriptor);
                if (fileDescriptor->videoResolutionID != 0 && 
                    fileDescriptor->videoResolutionID != videoResolutionID)
                {
                    // material package has wrong video resolution
                    material.topPackages.erase(iter1);
                    break;
                }
            }

        }
        iter1++;
    }
        
    
    
    // create AAF file if there is material    
    if (material.topPackages.size() == 0)
    {
        fprintf(stderr, "No packages found\n");
    }
    else
    {
        try
        {
            // create single source clips
            if (createGroup)
            {
                if (createMultiCam)
                {
                    aafGroup = auto_ptr<AAFFile>(new AAFFile(
                        addFilename(filenames, createGroupMCFilename(filenamePrefix, suffix))));
                }
                else
                {
                    aafGroup = auto_ptr<AAFFile>(new AAFFile(
                        addFilename(filenames, createGroupSingleFilename(filenamePrefix, suffix))));
                }
            }
            MaterialPackageSet::iterator iter1;
            int index = 0;
            for (iter1 = material.topPackages.begin(); iter1 != material.topPackages.end(); iter1++)
            {
                MaterialPackage* topPackage = *iter1;
                
                if (!createGroupOnly)
                {
                    AAFFile aafFile(
                        addFilename(filenames, createSingleClipFilename(filenamePrefix, suffix, index++)));
                    aafFile.addClip(topPackage, material.packages);
                    aafFile.save();
                }
    
                if (createGroup)
                {
                    aafGroup->addClip(topPackage, material.packages);
                }
            }
            
            
            // create multi-camera clips
            // mult-camera clips group packages together which have the same start time and creation date
            
            if (createMultiCam)
            {
                // load multi-camera clip definitions
                VectorGuard<MCClipDef> mcClipDefs;
                mcClipDefs.get() = database->loadAllMultiCameraClipDefs();
                
                MaterialPackageSet donePackages;
                for (iter1 = material.topPackages.begin(); iter1 != material.topPackages.end(); iter1++)
                {
                    MaterialPackage* topPackage1 = *iter1;
        
                    if (!donePackages.insert(topPackage1).second)
                    {
                        // already done this package
                        continue;
                    }
                    
                    MaterialPackageSet materialPackages;
                    materialPackages.insert(topPackage1);
                    
                    // add material to group that has same start time and creation date
                    Rational palEditRate = {25, 1}; // any rate will do
                    MaterialPackageSet::iterator iter2;
                    for (iter2 = iter1, iter2++; iter2 != material.topPackages.end(); iter2++)
                    {
                        MaterialPackage* topPackage2 = *iter2;
                        
                        if (topPackage2->creationDate.year == topPackage1->creationDate.year &&
                            topPackage2->creationDate.month == topPackage1->creationDate.month &&
                            topPackage2->creationDate.day == topPackage1->creationDate.day &&
                            getStartTime(topPackage1, material.packages, palEditRate) == 
                                getStartTime(topPackage2, material.packages, palEditRate))
                        {
                            materialPackages.insert(topPackage2);
                            donePackages.insert(topPackage2);
                        }
                    }
                    
                    if (!createGroupOnly)
                    {
                        vector<MCClipDef*>::iterator iter3;
                        int index = 0;
                        for (iter3 = mcClipDefs.get().begin(); iter3 != mcClipDefs.get().end(); iter3++)
                        {
                            MCClipDef* mcClipDef = *iter3;
                            
                            AAFFile aafFile(
                                addFilename(filenames, createMCClipFilename(filenamePrefix, 
                                    getStartTime(topPackage1, material.packages, palEditRate), suffix, index++)));
                            aafFile.addMCClip(mcClipDef, materialPackages, material.packages);
                            aafFile.save();
                        }
                    }
                    
                    if (createGroup)
                    {
                        vector<MCClipDef*>::iterator iter3;
                        for (iter3 = mcClipDefs.get().begin(); iter3 != mcClipDefs.get().end(); iter3++)
                        {
                            MCClipDef* mcClipDef = *iter3;
                            
                            aafGroup->addMCClip(mcClipDef, materialPackages, material.packages);
                        }
                    }                            
                }
            }
            
            if (createGroup)
            {
                aafGroup->save();
                delete aafGroup.release();
            }
            
            // output list of filenames to stdout
            vector<string>::const_iterator filenamesIter;
            for (filenamesIter = filenames.begin(); filenamesIter != filenames.end(); filenamesIter++)
            {
                printf("%s\n", (*filenamesIter).c_str());
            }
        }
        catch (const char*& ex)
        {
            fprintf(stderr, "\nFailed to create:\n%s\n", ex);
            Database::close();
            return 1;
        }
        catch (ProdAutoException& ex)
        {
            fprintf(stderr, "\nFailed to create:\n%s\n", ex.getMessage().c_str());
            Database::close();
            return 1;
        }
        catch (...)
        {
            fprintf(stderr, "\nFailed to create:\nUnknown exception thrown\n");
            Database::close();
            return 1;
        }
    }
    
    Database::close();
    return 0;
}

