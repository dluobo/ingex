/*
 * $Id: create_aaf.cpp,v 1.1 2007/09/11 14:08:44 stuart_hc Exp $
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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
static const char* g_databaseUserName = "bamzooki";
static const char* g_databasePassword = "bamzooki";

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


static void parseDateAndTimecode(string dateAndTimecodeStr, Date* date, int64_t* timecode)
{
    int data[7];
    
    if (sscanf(dateAndTimecodeStr.c_str(), "%u-%u-%uS%u:%u:%u:%u", &data[0], &data[1],
        &data[2], &data[3], &data[4], &data[5], &data[6]) != 7)
    {
        throw "Invalid date and timecode string";
    }
    
    date->year = data[0];
    date->month = data[1];
    date->day = data[2];
    *timecode = data[3] * 60 * 60 * 25 +
        data[4] * 60 * 25 + 
        data[5] * 25 + 
        data[6];
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


static string dateAndTimecodeString(Date date, int64_t timecode)
{
    char dateAndTimecodeStr[48];
    
#if defined(_MSC_VER)    
    _snprintf(
#else
    snprintf(
#endif
        dateAndTimecodeStr, 48, "%04d%02u%02u%02u%02u%02u%02u",
        date.year, date.month, date.day, 
        (int)(timecode / (60 * 60 * 25)), 
        (int)((timecode % (60 * 60 * 25)) / (60 * 25)),
        (int)(((timecode % (60 * 60 * 25)) % (60 * 25)) / 25),
        (int)(((timecode % (60 * 60 * 25)) % (60 * 25)) % 25));
        
    return dateAndTimecodeStr;
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

static string createDateAndTimecodeSuffix(Date fromDate, int64_t fromTimecode, Date toDate, int64_t toTimecode)
{
    stringstream suffix;
    
    suffix << dateAndTimecodeString(fromDate, fromTimecode) << "_" << dateAndTimecodeString(toDate, toTimecode);
        
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

static string createUniqueFilename(string prefix)
{
    string result;
    int count = 0;
    struct stat statBuf;
    
    while (count < 10000) // more than 10000 files with the same name - you must be joking!
    {
        stringstream filename;
        
        if (count == 0)
        {
            filename << prefix << ".aaf";
        }
        else
        {
            filename << prefix << "_" << count << ".aaf";
        }
        
        if (stat(filename.str().c_str(), &statBuf) != 0)
        {
            // file does not exist - use it
            result = filename.str();
            break;
        }
        
        count++;
    }
    if (result.size() == 0)
    {
        throw "Failed to create unique filename";
    }
    
    return result;
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
    fprintf(stderr, "  -h, --help                     Display this usage message\n");
    fprintf(stderr, "  -v, --verbose                  Print status messages\n");
    fprintf(stderr, "  -p, --prefix <filepath>        Filename prefix used for AAF files (default '%s')\n", g_filenamePrefix);
    fprintf(stderr, "  -r --resolution <id>           Video resolution identifier as listed in the database\n"); 
    fprintf(stderr, "                                     (default is %d for DV-50)\n", DV50_MATERIAL_RESOLUTION);
    fprintf(stderr, "  -g, --group                    Create AAF file with all clips included\n");
    fprintf(stderr, "  -o, --grouponly                Only create AAF file with all clips included\n");
    fprintf(stderr, "      --no-ts-suffix             Don't use a timestamp for a group only file\n");
    fprintf(stderr, "  -m, --multicam                 Also create multi-camera clips\n");
    fprintf(stderr, "  -f, --from <date>S<timecode>   Includes clips created >= date and start timecode\n");
    fprintf(stderr, "  -t, --to <date>S<timecode>     Includes clips created < date and start timecode\n");
    fprintf(stderr, "  --tag <name>=<value>           Includes clips with user comment tag <name> == <value>\n");
    fprintf(stderr, "  -d, --dns <string>             Database DNS (default '%s')\n", g_dns);
    fprintf(stderr, "  -u, --dbuser <string>          Database user name (default '%s')\n", g_databaseUserName);
    fprintf(stderr, "  --dbpassword <string>          Database user password (default ***)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Notes:\n");
    fprintf(stderr, "* --from and --to form is 'yyyy-mm-ddShh:mm:ss:ff'\n");
    fprintf(stderr, "* The default for --from is the start of today\n"); 
    fprintf(stderr, "* The default for --to is the start of tommorrow\n");
    fprintf(stderr, "* If --tag is used then --to and --from are ignored\n");
}


int main(int argc, const char* argv[])
{
    int cmdlnIndex = 1;
    string filenamePrefix = g_filenamePrefix;
    bool createGroup = false;
    bool createGroupOnly = false;
    bool createMultiCam = false;
    Date fromDate;
    Date toDate;
    int64_t fromTimecode = 0;
    int64_t toTimecode = 0;
    string dns = g_dns;
    string dbUserName = g_databaseUserName;
    string dbPassword = g_databasePassword;
    string tagName;
    string tagValue;
    string suffix;
    vector<string> filenames;
    int videoResolutionID = DV50_MATERIAL_RESOLUTION;
    bool verbose = false;
    bool noTSSuffix = false;
    
    Timestamp t = generateTimestampStartToday();
    fromDate.year = t.year;
    fromDate.month = t.month;
    fromDate.day = t.day;
    t = generateTimestampStartTomorrow();
    toDate.year = t.year;
    toDate.month = t.month;
    toDate.day = t.day;
    
    
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
            else if (strcmp(argv[cmdlnIndex], "-v") == 0 ||
                strcmp(argv[cmdlnIndex], "--verbose") == 0)
            {
                verbose = true;
                cmdlnIndex++;
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
            else if (strcmp(argv[cmdlnIndex], "--no-ts-suffix") == 0)
            {
                noTSSuffix = true;
                cmdlnIndex++;
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
                parseDateAndTimecode(argv[cmdlnIndex + 1], &fromDate, &fromTimecode);
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
                parseDateAndTimecode(argv[cmdlnIndex + 1], &toDate, &toTimecode);
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
        suffix = createDateAndTimecodeSuffix(fromDate, fromTimecode, toDate, toTimecode);
    }
    
    try
    {
        Database::initialise(dns, dbUserName, dbPassword, 1, 3);
        if (verbose)
        {
            printf("Initialised the database connection\n");
        }
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
            if (verbose)
            {
                printf("Loaded %d clips from the database based on the tag %s=%s\n", material.topPackages.size(),
                    tagName.c_str(), tagValue.c_str());
            }
        }
        else
        {
            Timestamp startDayFrom = g_nullTimestamp;
            startDayFrom.year = fromDate.year;
            startDayFrom.month = fromDate.month;
            startDayFrom.day = fromDate.day;
            Timestamp endDayTo = g_nullTimestamp;
            endDayTo.year = toDate.year;
            endDayTo.month = toDate.month;
            endDayTo.day = toDate.day;
            endDayTo.hour = 23;
            endDayTo.min = 59;
            endDayTo.sec = 59;
            endDayTo.qmsec = 249;
            
            // load all material created >= (start of the day) from and < (end of the day) to
            database->loadMaterial(startDayFrom, endDayTo, &material.topPackages, &material.packages);

            if (verbose)
            {
                printf("Loaded %d clips from the database based on the dates alone\n", material.topPackages.size());
            }
            
            // remove all packages that are < fromTimecode (at fromDate) and > toTimecode (at toDate)
            int removedCount = 0;
            Rational palEditRate = {25, 1};
            MaterialPackageSet::iterator iter1 = material.topPackages.begin();
            while (iter1 != material.topPackages.end())
            {
                MaterialPackage* topPackage = *iter1;
                
                if (topPackage->creationDate.year == fromDate.year && 
                    topPackage->creationDate.month == fromDate.month && 
                    topPackage->creationDate.day == fromDate.day &&
                    getStartTime(topPackage, material.packages, palEditRate) < fromTimecode)
                {
                    material.topPackages.erase(iter1);
                    removedCount++;
                }
                else if (topPackage->creationDate.year == toDate.year && 
                    topPackage->creationDate.month == toDate.month && 
                    topPackage->creationDate.day == toDate.day &&
                    getStartTime(topPackage, material.packages, palEditRate) >= toTimecode)
                {
                    material.topPackages.erase(iter1);
                    removedCount++;
                }
                
                iter1++;
            }

            if (verbose)
            {
                if (removedCount > 0)
                {
                    printf("Removed %d clips from those loaded that did not match the start timecode range\n", removedCount);
                }
                else
                {
                    printf("All clips match the start timecode range\n");
                }
            }
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
    int removedCount = 0;
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
                    removedCount++;
                    break;
                }
            }

        }
        iter1++;
    }
    
    if (verbose)
    {
        if (removedCount > 0)
        {
            printf("Removed %d clips from those loaded that did not match the video resolution\n", removedCount);
        }
        else
        {
            printf("All clips either match the video resolution or are audio only\n");
        }
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
                if (createGroupOnly && noTSSuffix)
                {
                    aafGroup = auto_ptr<AAFFile>(new AAFFile(
                        addFilename(filenames, createUniqueFilename(filenamePrefix))));
                }
                else if (createMultiCam)
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

