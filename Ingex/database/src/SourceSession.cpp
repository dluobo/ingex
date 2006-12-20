/*
 * $Id: SourceSession.cpp,v 1.1 2006/12/20 14:37:03 john_f Exp $
 *
 * A set of Packages representing live recording / tape sources during a time period
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
 
#include <cassert>
#include <map>

#include "SourceSession.h"
#include "Database.h"
#include "Utilities.h"
#include "DBException.h"
#include "Logging.h"


using namespace std;
using namespace prodauto;



SourceSession* SourceSession::create(Recorder* recorder, Date date)
{
    if (!recorder->hasConfig())
    {
        PA_LOGTHROW(DBException, ("Failed to create source session for recorder with no "
            "configuration selected"));
    }
    
    Database* database = Database::getInstance();
    auto_ptr<SourceSession> sourceSession(new SourceSession(date));
    SourcePackage* sourcePackage;
    Track* track;
    SourceClip* sourceClip;
    TapeEssenceDescriptor* tapeEssDesc;
    LiveEssenceDescriptor* liveEssDesc;
    map<SourceConfig*, SourcePackage*> processedSourceConfigs;

    
    try
    {
        vector<RecorderInputConfig*>::const_iterator iter1;
        for (iter1 = recorder->getConfig()->recorderInputConfigs.begin(); 
            iter1 != recorder->getConfig()->recorderInputConfigs.end(); iter1++)
        {
            RecorderInputConfig* inputConfig = *iter1;
            
            vector<RecorderInputTrackConfig*>::const_iterator iter2;
            for (iter2 = inputConfig->trackConfigs.begin(); iter2 != inputConfig->trackConfigs.end(); iter2++)
            {
                RecorderInputTrackConfig* trackConfig = *iter2;
                
                if (!trackConfig->isConnectedToSource())
                {
                    // recorder track is not connected to a source
                    continue;
                }

                SourceConfig* sourceConfig = trackConfig->sourceConfig;
                
                // check that the recorder config track is connected to a source config track
                if (sourceConfig->getTrackConfig(trackConfig->sourceTrackID) == 0)
                {
                    PA_LOGTHROW(DBException, ("Recorder track config references non-existing source track config"));
                }
                
                // create new source package if source config not already processed
                if (processedSourceConfigs.find(sourceConfig) == processedSourceConfigs.end())
                {
                    // create new source package
                    
                    sourcePackage = new SourcePackage();
                    sourceSession->sourcePackages.push_back(sourcePackage);
                    processedSourceConfigs.insert(pair<SourceConfig*, SourcePackage*>(sourceConfig, sourcePackage));

                    sourcePackage->uid = generateUMID();
                    sourcePackage->name = getSourcePackageName(sourceConfig->name, date);
                    sourcePackage->creationDate = generateTimestampNow();
                    if (sourceConfig->type == TAPE_SOURCE_CONFIG_TYPE)
                    {
                        tapeEssDesc = new TapeEssenceDescriptor();
                        sourcePackage->descriptor = tapeEssDesc;
                        tapeEssDesc->spoolNumber = sourceConfig->spoolNumber;
                    }
                    else // LIVE_SOURCE_CONFIG_TYPE
                    {
                        liveEssDesc = new LiveEssenceDescriptor();
                        sourcePackage->descriptor = liveEssDesc;
                        liveEssDesc->recordingLocation = sourceConfig->recordingLocation;
                    }
                    
                    
                    // create source package tracks
                    
                    vector<SourceTrackConfig*>::const_iterator iter3;
                    for (iter3 = sourceConfig->trackConfigs.begin(); iter3 != sourceConfig->trackConfigs.end(); iter3++)
                    {
                        SourceTrackConfig* sourceTrackConfig = *iter3;
                
                        track = new Track();
                        sourcePackage->tracks.push_back(track);
                        
                        track->id = sourceTrackConfig->id;
                        track->number = sourceTrackConfig->number;
                        track->name = sourceTrackConfig->name;
                        track->dataDef = sourceTrackConfig->dataDef;
                        track->editRate = sourceTrackConfig->editRate;
                        sourceClip = new SourceClip();
                        track->sourceClip = sourceClip;
                        
                        sourceClip->sourcePackageUID = g_nullUMID;
                        sourceClip->sourceTrackID = 0;
                        sourceClip->length = sourceTrackConfig->length;
                        sourceClip->position = 0;
                    }
                }
            }
        }

        // save source packages to the database
        
        auto_ptr<Transaction> transaction(database->getTransaction());
        vector<SourcePackage*>::const_iterator iter;
        for (iter = sourceSession->sourcePackages.begin(); iter != sourceSession->sourcePackages.end(); iter++)
        {
            database->savePackage(*iter, transaction.get());
        }
        transaction->commitTransaction();
    }
    catch (DBException& ex)
    {
        PA_LOGTHROW(DBException, ("Failed to create source session:\n%s", ex.getMessage().c_str()));
    }
    
    return sourceSession.release();
}

SourceSession* SourceSession::load(Recorder* recorder, Date date)
{
    if (!recorder->hasConfig())
    {
        PA_LOGTHROW(DBException, ("Failed to create source session for recorder with no "
            "configuration selected"));
    }
    
    Database* database = Database::getInstance();
    auto_ptr<SourceSession> sourceSession(new SourceSession(date));
    long count = 0;
    
    try
    {
        vector<SourceConfig*>::const_iterator iter;
        for (iter = recorder->getConfig()->sourceConfigs.begin(); 
            iter != recorder->getConfig()->sourceConfigs.end(); iter++)
        {
            SourcePackage* sourcePackage = database->loadSourcePackage(getSourcePackageName((*iter)->name, date));
            if (sourcePackage == 0)
            {
                if (count > 0)
                {
                    PA_LOGTHROW(DBException, ("One or more, but not all, of the source packages in "
                        "the source session is missing"));
                }
                return 0; // no exception 
            }
            sourceSession->sourcePackages.push_back(sourcePackage);
            count++;
        }
    }
    catch (DBException& ex)
    {
        PA_LOGTHROW(DBException, ("Failed to load source session:\n%s", ex.getMessage().c_str()));
    }
    
    return sourceSession.release();
}

bool SourceSession::isInstancePackage(SourceConfig* sourceConfig, SourcePackage* sourcePackage)
{
    // the source package name is sufixed with " - yyyy-mm-dd"
    size_t index = sourcePackage->name.rfind("-");
    if (index != string::npos && index > 0)
    {
        index = sourcePackage->name.rfind("-", index - 1);
        if (index != string::npos && index > 0)
        {
            index = sourcePackage->name.rfind("-", index - 1);
            if (index != string::npos && index > 1)
            {
                if (sourceConfig->name.compare(sourcePackage->name.substr(0, index - 1)) == 0)
                {
                    return true;
                }
            }
        }
    }
    
    return false;
}


string SourceSession::getSourcePackageName(string sourceName, Date date)
{
    string sourcePackageName = sourceName;
    sourcePackageName.append(" - ");
    sourcePackageName.append(getDateString(date));
    
    return sourcePackageName;
}

SourceSession::SourceSession(Date date)
: _date(date)
{}

SourceSession::~SourceSession()
{
    vector<SourcePackage*>::const_iterator iter;
    for (iter = sourcePackages.begin(); iter != sourcePackages.end(); iter++)
    {
        delete *iter;
    }
}

SourcePackage* SourceSession::getSourcePackage(std::string sourceName)
{
    string sourcePackageName = getSourcePackageName(sourceName, _date);
    
    vector<SourcePackage*>::const_iterator iter;
    for (iter = sourcePackages.begin(); iter != sourcePackages.end(); iter++)
    {
        if ((*iter)->name.compare(sourcePackageName) == 0)
        {
            return *iter;
        }
    }
    
    return 0;
}

void SourceSession::deleteFromDatabase()
{
    Database* database = Database::getInstance();

    // delete all from database or none if anything fails    
    auto_ptr<Transaction> transaction(database->getTransaction());
    vector<SourcePackage*>::iterator iter;
    for (iter = sourcePackages.begin(); iter != sourcePackages.end(); iter++)
    {
        database->deletePackage(*iter, transaction.get());
    }
    transaction->commitTransaction();

    // deletion successful so delete objects
    for (iter = sourcePackages.begin(); iter != sourcePackages.end(); iter++)
    {
        delete *iter;
    }
    sourcePackages.clear();
}
    

