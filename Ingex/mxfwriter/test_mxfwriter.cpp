/*
 * $Id: test_mxfwriter.cpp,v 1.1 2006/12/20 14:51:07 john_f Exp $
 *
 * Tests the MXF writer
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
#include <stdarg.h>
#include <assert.h>
#include <pthread.h>

#include <sstream>


#include "MXFWriter.h"
#include "MXFWriterException.h"
#include "MXFUtils.h"
#include <Database.h>
#include <Recorder.h>
#include <SourceSession.h>
#include <Utilities.h>
#include <DBException.h>
#include <Logging.h>


using namespace std;
using namespace prodauto;


//#define TEST_WRITE_SAMPLE_DATA      1


#define NUM_RECORD_THREADS          4
#define NUM_FRAMES                  5
#define NUM_RECORDS                 1

//#define RECORDER_NAME               "Studio A"
#define RECORDER_NAME               "KW-A44"
#define NUM_RECORDER_INPUTS         4

//#define CREATING_FILE_PATH          "creating"
//#define DESTINATION_FILE_PATH       "destination"
//#define FAILURES_FILE_PATH          "failures"
#define CREATING_FILE_PATH          ""
#define DESTINATION_FILE_PATH       ""
#define FAILURES_FILE_PATH          ""




typedef struct
{
    int64_t startPosition;
    string creatingFilePath;
    string destinationFilePath;
    string failuresFilePath;
    string filenamePrefix;
    Recorder* recorder; 
    RecorderInputConfig* recorderInputConfig;
    SourceSession* sourceSession;
    vector<UserComment> userComments;
    string avidProjectName;
} RecordData;

void* start_record_routine(void* data)
{
    RecordData* recordData = (RecordData*)data;

    // dummy essence data
    uint8_t uncData[720 * 576 * 2];
    uint32_t uncFrameSize = 720 * 576 * 2;
    memset(uncData, 0, uncFrameSize);
    uint8_t pcmData[1920 * 2]; // 16-bit
    uint32_t pcmFrameSize = 1920;
    memset(pcmData, 0, pcmFrameSize * 2);
    
    printf("Starting... (%s)\n", recordData->filenamePrefix.c_str());
    
    try
    {
        // create writer for input (mask all except track 1,2,3)
        Rational imageAspectRatio = {16, 9};
        auto_ptr<MXFWriter> writer(new MXFWriter(
            recordData->recorderInputConfig, recordData->sourceSession, 
            UNC_MATERIAL_RESOLUTION, imageAspectRatio,
            16, 0xffffffff, recordData->startPosition, recordData->creatingFilePath,
            recordData->destinationFilePath, recordData->failuresFilePath,
            recordData->filenamePrefix, recordData->userComments, 
            recordData->avidProjectName));
    
        int i;
        int trackIndex;
        
#if defined(TEST_WRITE_SAMPLE_DATA)
        // start the sample data for PCM
        vector<RecorderInputTrackConfig*>::const_iterator trackIter;
        for (trackIndex = 1, trackIter = recordData->recorderInputConfig->trackConfigs.begin();
            trackIter != recordData->recorderInputConfig->trackConfigs.end();
            trackIndex++, trackIter++)
        {
            RecorderInputTrackConfig* trackConfig = *trackIter;
            
            if (trackConfig->isConnectedToSource())
            {
                SourceTrackConfig* sourceTrackConfig = 
                    trackConfig->sourceConfig->getTrackConfig(trackConfig->sourceTrackID);
                if (sourceTrackConfig->dataDef == SOUND_DATA_DEFINITION)
                {
                    // start write pcm audio data
                    writer->startSampleData(trackIndex);
                }
            }
        }
#endif
        // write NUM_FRAMES of uncompressed video and audio
        for (i = 0; i < NUM_FRAMES; i++)
        {
            vector<RecorderInputTrackConfig*>::const_iterator trackIter;
            for (trackIndex = 1, trackIter = recordData->recorderInputConfig->trackConfigs.begin();
                trackIter != recordData->recorderInputConfig->trackConfigs.end();
                trackIndex++, trackIter++)
            {
                RecorderInputTrackConfig* trackConfig = *trackIter;
                
                if (trackConfig->isConnectedToSource())
                {
                    SourceTrackConfig* sourceTrackConfig = 
                        trackConfig->sourceConfig->getTrackConfig(trackConfig->sourceTrackID);
                    if (sourceTrackConfig->dataDef == SOUND_DATA_DEFINITION)
                    {
#if defined(TEST_WRITE_SAMPLE_DATA)
                        // write pcm audio data
                        writer->writeSampleData(trackIndex, pcmData, pcmFrameSize * 2);
#else
                        writer->writeSample(trackIndex, pcmFrameSize, pcmData, pcmFrameSize * 2);
#endif
                    }
                    else // PICTURE_DATA_DEFINITION
                    {
                        // write uncompressed video data
                        writer->writeSample(trackIndex, 1, uncData, uncFrameSize);
                    }
                }
            }
    
        }

#if defined(TEST_WRITE_SAMPLE_DATA)
        // end the sample data for PCM
        vector<RecorderInputTrackConfig*>::const_iterator trackIter;
        for (trackIndex = 1, trackIter = recordData->recorderInputConfig->trackConfigs.begin();
            trackIter != recordData->recorderInputConfig->trackConfigs.end();
            trackIndex++, trackIter++)
        {
            RecorderInputTrackConfig* trackConfig = *trackIter;
            
            if (trackConfig->isConnectedToSource())
            {
                SourceTrackConfig* sourceTrackConfig = 
                    trackConfig->sourceConfig->getTrackConfig(trackConfig->sourceTrackID);
                if (sourceTrackConfig->dataDef == SOUND_DATA_DEFINITION)
                {
                    // end write pcm audio data
                    writer->endSampleData(trackIndex, pcmFrameSize * NUM_FRAMES);
                }
            }
        }
#endif        

        // complete the writing and save packages to database
        writer->completeAndSaveToDatabase();
    }
    catch (const ProdAutoException& ex)
    {
        fprintf(stderr, "Failed to record (%s):\n  %s\n", recordData->filenamePrefix.c_str(), 
            ex.getMessage().c_str());
        pthread_exit((void *) 0);
        return NULL;
    }
    catch (...)
    {
        fprintf(stderr, "Unknown exception thrown (%s)\n", recordData->filenamePrefix.c_str());
        pthread_exit((void *) 0);
        return NULL;
    }
    
    printf("Successfully completed and saved (%s)\n", recordData->filenamePrefix.c_str());
    
    pthread_exit((void *) 0);
}


static void usage(const char* prog)
{
    fprintf(stderr, "%s <filename prefix>\n", prog);
}


int main(int argc, const char* argv[])
{
    int j;
    int k;
    
    
    // set logging
    Logging::initialise(LOG_LEVEL_DEBUG);
    
    // connect the MXF logging facility
    connectLibMXFLogging();
    
    if (argc != 2)
    {
        usage(argv[0]);
        return 1;
    }

    const char* filenamePrefix = argv[1];
    
    try
    {
        Database::initialise("prodautodb", "ingex", "ingex", 1, 2 * NUM_RECORD_THREADS);
    }
    catch (const DBException& ex)
    {
        fprintf(stderr, "Failed to connect to database:\n  %s\n", ex.getMessage().c_str());
        return 1;
    }

    // load the recorder configuration
    auto_ptr<Recorder> recorder;
    try
    {
        Database* database = Database::getInstance();
        recorder = auto_ptr<Recorder>(database->loadRecorder(RECORDER_NAME));
        if (!recorder->hasConfig())
        {
            fprintf(stderr, "Recorder '%s' has null config\n", RECORDER_NAME);
            Database::close();
            return 1;
        }
    }
    catch (const DBException& ex)
    {
        fprintf(stderr, "Failed to load recorder '%s':\n  %s\n", RECORDER_NAME, ex.getMessage().c_str());
        Database::close();
        return 1;
    }


    // load the source session
    auto_ptr<SourceSession> sourceSession;
    try
    {
        SourceSession* tmpSourceSession;
        Date now = generateDateNow();
        
        if ((tmpSourceSession = SourceSession::load(recorder.get(), now)) == 0)
        {
            printf("Warning: creating a source session...\n");
            tmpSourceSession = SourceSession::create(recorder.get(), now);
            sourceSession = auto_ptr<SourceSession>(tmpSourceSession);
        }
        else
        {
            sourceSession = auto_ptr<SourceSession>(tmpSourceSession);
        }
    }
    catch (const DBException& ex)
    {
        fprintf(stderr, "Failed to load source session:\n  %s\n", ex.getMessage().c_str());
        Database::close();
        return 1;
    }

    
    RecorderInputConfig* inputConfig[NUM_RECORDER_INPUTS];
    for (k = 0; k < NUM_RECORDER_INPUTS; k++)
    {
        if ((inputConfig[k] = recorder.get()->getConfig()->getInputConfig(k + 1)) == 0)
        {
            fprintf(stderr, "Unknown recorder input config %d\n", k + 1);
            Database::close();
            return 1;
        }
    }
    
    // get the start position (ie. time now) for the "recording"
    Timestamp now = generateTimestampNow();
    int64_t startPosition = now.hour * 60 * 60 * 25 +
        now.min * 60 * 25 +
        now.sec * 25 +
        now.qmsec * 4 * 25 / 1000;
        
    for (j = 0; j < NUM_RECORDS; j++)
    {
        printf("** j == %d\n", j);
        
        try
        {
            pthread_t thread[NUM_RECORD_THREADS];
            RecordData recordData[NUM_RECORD_THREADS];
            pthread_attr_t attr;
            int i, rc, status;
            
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
            
            for (i = 0; i < NUM_RECORD_THREADS; i++)
            {
                stringstream prefix;
                prefix << filenamePrefix << "_" << i; 
                
                recordData[i].startPosition = startPosition;
                recordData[i].creatingFilePath = CREATING_FILE_PATH;
                recordData[i].destinationFilePath = DESTINATION_FILE_PATH;
                recordData[i].failuresFilePath = FAILURES_FILE_PATH;
                recordData[i].filenamePrefix = prefix.str();
                recordData[i].recorder = recorder.get();
                recordData[i].recorderInputConfig = inputConfig[i % NUM_RECORDER_INPUTS];
                recordData[i].sourceSession = sourceSession.get();
                recordData[i].userComments.push_back(UserComment(AVID_UC_COMMENTS_NAME, 
                    "a test file"));
                recordData[i].userComments.push_back(UserComment(AVID_UC_DESCRIPTION_NAME, 
                    "an mxfwriter test file produced by test_mxfwriter"));
                recordData[i].avidProjectName = "testproject";
                
                if (pthread_create(&thread[i], &attr, start_record_routine, (void *)(&recordData[i])))
                {
                    fprintf(stderr, "Failed to create record thread\n");
                    Database::close();
                    pthread_exit(NULL);
                    return 1;
                }
            }
            
            for (i = 0; i < NUM_RECORD_THREADS; i++)
            {
                rc = pthread_join(thread[i], (void **)&status);
                if (rc)
                {
                    fprintf(stderr, "Return code from pthread_join() is %d\n", rc);
                }
            }
            
            startPosition += NUM_FRAMES;
        }
        catch (...)
        {
            fprintf(stderr, "Unknown exception thrown\n");
            Database::close();
            pthread_exit(NULL);
            return 1;
        }
    
    }
        
    Database::close();
    
    pthread_exit(NULL);
    return 0;
}

