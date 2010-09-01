/*
 * $Id: Chunking.cpp,v 1.2 2010/09/01 16:05:22 philipn Exp $
 *
 * Chunks an MXF file into MXF, browse copy and PSE report files for each item
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

/*
    Chunking does the same as capture: create an MXF file, browse copy file, 
    timecode text file, information text file and PSE report file for each item.
    The difference is that the source essence data is from a previously created 
    MXF file rather than from captured SDI data.
    
    The original MXF file was written as a "page file" and this reduces the
    disk space requirement from 2 x file size to 2 x page size. Page files are 
    deleted when transferring the data if the remaining disk space is nearing 
    the disk space margin.
    
    The chunking is throttled if a tape transfer is in progress to ensure a
    near maximum tape transfer speed is maintained.
*/

#include <cstdlib>
#include <cerrno>

#include "Chunking.h"
#include "Recorder.h"
#include "RecordingSession.h"
#include "RecorderDatabase.h"
#include "Cache.h"
#include "ArchiveMXFWriter.h"
#include "D10MXFWriter.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Config.h"
#include "Timing.h"
#include "FileLock.h"

#include "video_conversion.h"
#include "PSEReport.h"

using namespace std;
using namespace rec;


// check whether throttling is required every x frames
#define THROTTLE_CHECK_INTERVAL         25


static ArchiveTimecode convert_timecode(rec::Timecode from)
{
    ArchiveTimecode to;
    
    to.dropFrame = false;
    to.hour = from.hour;
    to.min = from.min;
    to.sec = from.sec;
    to.frame = from.frame;
    
    return to;
}


bool Chunking::readyForChunking(RecordingItems* recordingItems)
{
    vector<RecordingItem> items = recordingItems->getItems();
    
    if (items.empty())
    {
        return false;
    }
    
    // seek to start of items not set
    // and then check items not set are disabled
    vector<RecordingItem>::const_iterator iter;
    for (iter = items.begin(); iter != items.end(); iter++)
    {
        if ((*iter).duration < 0)
        {
            break;
        }
    }
    for (; iter != items.end(); iter++)
    {
        if (!(*iter).isDisabled)
        {
            return false;
        }
    }
    
    return true;
}


Chunking::Chunking(RecordingSession* session, RecordingSessionTable* sessionTable, string mxfPageFilename, 
    RecordingItems* recordingItems, bool disablePSE)
: _session(session), _sessionTable(sessionTable), _recordingItems(recordingItems), _disablePSE(disablePSE),
_stop(false), _hasStopped(false), _threadCount(0), _chunkingThrottleFPS(0),
_inputArchiveMXFFile(0), _inputD10MXFFile(0), _inputMXFFile(0),
_hddDest(0), _stereoAudio(0), _yuv420Video(0)
{
    try
    {
        _tapeTransferLockFile = Config::tape_transfer_lock_file;
        _threadCount = Config::browse_thread_count;
        _chunkingThrottleFPS = Config::chunking_throttle_fps;
        
        switch (session->getProfile()->getIngestFormat())
        {
            case MXF_UNC_8BIT_INGEST_FORMAT:
            case MXF_UNC_10BIT_INGEST_FORMAT:
                _inputArchiveMXFFile = new ArchiveMXFFile(mxfPageFilename, MXF_PAGE_FILE_SIZE);
                _inputMXFFile = _inputArchiveMXFFile;
                break;
            case MXF_D10_50_INGEST_FORMAT:
                _inputD10MXFFile = new D10MXFFile(mxfPageFilename, MXF_PAGE_FILE_SIZE,
                                                  session->_recorder->getCache()->getCreatingEventFilename(mxfPageFilename),
                                                  true,
                                                  _session->getProfile()->browse_enable);
                _inputMXFFile = _inputD10MXFFile;
                break;
            case UNKNOWN_INGEST_FORMAT:
                REC_ASSERT(false);
        }
        if (!_inputMXFFile->isComplete())
        {
            REC_LOGTHROW(("Cannot chunk incomplete MXF file '%s'", mxfPageFilename.c_str()));
        }

        
        REC_ASSERT(recordingItems->getItems().size() > 0);
    
        _status.duration = recordingItems->getTotalDuration();
        REC_ASSERT(_status.duration == _inputMXFFile->getDuration());

        
        _stereoAudio = new int16_t[1920 * 2];
        memset(_stereoAudio, 0, 1920 * 2 * sizeof(int16_t));
    
        if (_inputArchiveMXFFile)
        {
            _yuv420Video = new unsigned char[720 * 576 * 3 / 2];
            memset(_yuv420Video, 0, 720 * 576 * 3 / 2);
        }
        
        _status.itemNumber = 1;
        _status.frameNumber = 0;
    }
    catch (...)
    {
        delete _inputMXFFile;
        delete [] _stereoAudio;
        delete [] _yuv420Video;

        throw;
    }
}

Chunking::~Chunking()
{
    delete _inputMXFFile;
    delete [] _stereoAudio;
    delete [] _yuv420Video;
}

void Chunking::start()
{
    GUARD_THREAD_START(_hasStopped);

    vector<RecordingItem> items = _recordingItems->getItems();
    REC_ASSERT(items.size() > 0);
    
    // handle case where all items have been disabled
    if (items.front().isDisabled)
    {
        setChunkingState(CHUNKING_COMPLETED);
        return;
    }
    
    long chunkingThrottleUSec = (long)(40 * THROTTLE_CHECK_INTERVAL * 25.0 / _chunkingThrottleFPS) * MSEC_IN_USEC; 
    
    PSEFailure* pseFailures = 0;
    long numPSEFailures = 0;
    if (!_disablePSE)
    {
        // PSE analysis was done
        numPSEFailures = _inputMXFFile->getPSEFailures(&pseFailures);
    }
    VTRErrorAtPos* vtrPosErrors = 0;
    long numVTRErrors = 0;
    numVTRErrors = _inputMXFFile->getVTRErrors(&vtrPosErrors);
    // note: vtrPosErrors does not include the timecode information and therefore
    // we create a second array of VTRErrors which will have the timecodes
    // filled in
    VTRError* vtrErrors = 0;
    if (numVTRErrors > 0)
    {
        vtrErrors = new VTRError[numVTRErrors];
    }
    DigiBetaDropout* digiBetaDropouts = 0;
    long numDigiBetaDropouts = _inputMXFFile->getDigiBetaDropouts(&digiBetaDropouts);
    
    bool completed = false;
    Cache* cache = _session->_recorder->getCache();
    vector<RecordingItem>::iterator itemIter = items.begin();
    RecordingItem* item = &(*itemIter);
    int itemNumber = 1;
    MXFContentPackage* contentPackage = 0;
    ArchiveMXFContentPackage* archiveContentPackage = 0;
    D10MXFContentPackage* d10ContentPackage = 0;
    int64_t inputFrameNumber = 0;
    int64_t itemFrameNumber = 0;
    MXFWriter* outputMXFFile = 0;
    BrowseEncoder* browseEncoder = 0;
    FILE* timecodeFile = 0;
    long firstPSEFailure = 0;
    long firstVTRError = 0;
    long firstDigiBetaDropout = 0;
    long numItemPSEFailures = 0;
    long numItemVTRErrors = 0;
    long numItemDigiBetaDropouts = 0;
    int64_t diskSpaceCheckInterval = 0;
    Timer throttleTimer;
    bool noMore;
    Timecode vitc;
    Timecode ltc;
    try
    {
        setChunkingState(CHUNKING_IN_PROGRESS);
        
        while (!_stop)
        {
            // prepare stuff if at start of item
            if (itemFrameNumber == 0)
            {
                if (item->isJunk)
                {
                    Logging::info("Skipping junk item %d\n", itemNumber);

                    // update status
                    {
                        LOCK_SECTION(_statusMutex);
                        _status.frameNumber = inputFrameNumber;
                        _status.itemNumber = itemNumber;
                    }
                
                    // skip frames
                    if (!_inputMXFFile->skipFrames(item->duration))
                    {
                        REC_LOGTHROW(("MXF file is missing frames"));
                    }

                    inputFrameNumber += item->duration;
                    itemFrameNumber += item->duration;

                    // update status
                    {
                        LOCK_SECTION(_statusMutex);
                        _status.frameNumber = inputFrameNumber;
                        _status.itemNumber = itemNumber;
                    }

                    
                    // update PSE failure, VTR error and digibeta dropout counts
                    noMore = false;
                    while (!noMore)
                    {
                        noMore = true;
                        if (pseFailures != 0 && firstPSEFailure + numItemPSEFailures < numPSEFailures)
                        {
                            PSEFailure* pseFailure = &pseFailures[firstPSEFailure + numItemPSEFailures];
                            
                            if (pseFailure->position < inputFrameNumber)
                            {
                                numItemPSEFailures++;
                                noMore = false;
                            }
                        }
                        if (vtrErrors != 0 && firstVTRError + numItemVTRErrors < numVTRErrors)
                        {
                            VTRErrorAtPos* vtrPosError = &vtrPosErrors[firstVTRError + numItemVTRErrors];
                            
                            if (vtrPosError->position < inputFrameNumber)
                            {
                                numItemVTRErrors++;
                                noMore = false;
                            }
                        }
                        if (digiBetaDropouts != 0 && firstDigiBetaDropout + numItemDigiBetaDropouts < numDigiBetaDropouts)
                        {
                            DigiBetaDropout* digiBetaDropout = &digiBetaDropouts[firstDigiBetaDropout + numItemDigiBetaDropouts];
                            
                            if (digiBetaDropout->position < inputFrameNumber)
                            {
                                numItemDigiBetaDropouts++;
                                noMore = false;
                            }
                        }
                    }
                    
                    // junk it 
                    _recordingItems->setJunked(item->id);
                    
                    
                    // move on to the next item
    
                    firstPSEFailure += numItemPSEFailures;
                    numItemPSEFailures = 0;
                    firstVTRError += numItemVTRErrors;
                    numItemVTRErrors = 0;
                    firstDigiBetaDropout += numItemDigiBetaDropouts;
                    numItemDigiBetaDropouts = 0;
    
                    itemIter++;
                    if (itemIter == items.end() || (*itemIter).isDisabled)
                    {
                        completed = true;
                        break;
                    }
                    
                    itemFrameNumber = 0;
                    itemNumber++;
                    item = &*itemIter;
                    
                    continue;
                }

                
                Logging::info("Starting chunking of item %d\n", itemNumber);

                if (!_session->_recorder->getCache()->getMultiItemFilenames(_recordingItems->getSource()->barcode, 
                    item->sourceItem->itemNo, &_mxfFilename, &_browseFilename, &_browseTimecodeFilename,
                    &_browseInfoFilename, &_pseFilename, &_eventFilename))
                {
                    REC_LOGTHROW(("Failed to get unique filenames from the cache"));
                }

                _hddDest = _session->addHardDiskDestination(_mxfFilename, _browseFilename, _pseFilename, item->sourceItem, itemNumber);

                
                Rational aspectRatio = Recorder::getRasterAspectRatio(item->sourceItem->aspectRatioCode);
                
                if (_inputArchiveMXFFile)
                {
                    ArchiveMXFWriter* archiveOutputMXFFile = new ArchiveMXFWriter();
                    outputMXFFile = archiveOutputMXFFile;
                    archiveOutputMXFFile->setComponentDepth(_session->getProfile()->getIngestFormat() == MXF_UNC_8BIT_INGEST_FORMAT ? 8 : 10);
                    archiveOutputMXFFile->setAspectRatio(aspectRatio);
                    archiveOutputMXFFile->setNumAudioTracks(_session->getProfile()->num_audio_tracks);
                    archiveOutputMXFFile->setIncludeCRC32(_session->getProfile()->include_crc32);
                    archiveOutputMXFFile->setStartPosition(0);

                    REC_CHECK(archiveOutputMXFFile->createFile(cache->getCompleteCreatingFilename(_mxfFilename)));
                }
                else
                {
                    D10MXFWriter* d10OutputMXFFile = new D10MXFWriter();
                    outputMXFFile = d10OutputMXFFile;
                    d10OutputMXFFile->setAspectRatio(aspectRatio);
                    d10OutputMXFFile->setNumAudioTracks(_session->getProfile()->num_audio_tracks);
                    d10OutputMXFFile->setPrimaryTimecode(_session->getProfile()->primary_timecode);
                    d10OutputMXFFile->setEventFilename(cache->getCompleteCreatingEventFilename(_eventFilename));

                    REC_CHECK(d10OutputMXFFile->createFile(cache->getCompleteCreatingFilename(_mxfFilename)));
                }
                
                diskSpaceCheckInterval = MXF_PAGE_FILE_SIZE / _session->getContentPackageSize();
                REC_ASSERT(diskSpaceCheckInterval > 0);
                
                if (_session->getProfile()->browse_enable)
                {
                    browseEncoder = BrowseEncoder::create(cache->getCompleteBrowseFilename(_browseFilename).c_str(), aspectRatio,
                        _session->getProfile()->browse_video_bit_rate, _threadCount);
                    if (browseEncoder == 0)
                    {
                        REC_LOGTHROW(("Failed to open the browse encoder"));
                    }
                    
                    timecodeFile = fopen(cache->getCompleteBrowseFilename(_browseTimecodeFilename).c_str(), "wb");
                    if (timecodeFile == 0)
                    {
                        REC_LOGTHROW(("Failed to open the timecode file '%s': %s", _browseTimecodeFilename.c_str(), strerror(errno)));
                    }
    
                    _session->writeBrowseInfo(cache->getCompleteBrowseFilename(_browseInfoFilename), item, false);
                }
            }

            // update status
            {
                LOCK_SECTION(_statusMutex);
                _status.frameNumber = inputFrameNumber;
                _status.itemNumber = itemNumber;
            }
            
            
            // read next input frame
            if (!_inputMXFFile->nextFrame(false, contentPackage))
            {
                REC_LOGTHROW(("MXF file is missing frames"));
            }
            
            // check every second if a tape transfer is in progress and if so
            // then throttle the chunking
            if (inputFrameNumber % 25 == 0)
            {
                // if this file is locked then a tape transfer is in progress 
                if (FileLock::isLocked(_tapeTransferLockFile))
                {
                    throttleTimer.sleepRemainder();
                    throttleTimer.start(chunkingThrottleUSec);
                }
            }
            
            // check disk space and truncate the input MXF file is neccessary
            
            if (inputFrameNumber % diskSpaceCheckInterval == 0)
            {
                int64_t diskSpace = _session->_recorder->getRemainingDiskSpace();
                if (diskSpace < DISK_SPACE_MARGIN)
                {
                    _inputMXFFile->forwardTruncate();
                }
            }

            // write frame
            
            if (_inputArchiveMXFFile)
            {
                archiveContentPackage = dynamic_cast<ArchiveMXFContentPackage*>(contentPackage);
                
                REC_ASSERT(_session->getProfile()->num_audio_tracks == archiveContentPackage->getNumAudioTracks());
                outputMXFFile->writeContentPackage(contentPackage);

                if (_session->getProfile()->browse_enable)
                {
                    writeBrowseFrame(archiveContentPackage, browseEncoder, itemFrameNumber);
                    writeTimecodeFrame(archiveContentPackage, itemFrameNumber, timecodeFile);
                }
                
                vitc = archiveContentPackage->getVITC();
                ltc = archiveContentPackage->getLTC();
            }
            else
            {
                d10ContentPackage = dynamic_cast<D10MXFContentPackage*>(contentPackage);
                
                REC_ASSERT(_session->getProfile()->num_audio_tracks == d10ContentPackage->getNumAudioTracks());
                outputMXFFile->writeContentPackage(contentPackage);

                if (_session->getProfile()->browse_enable)
                {
                    writeBrowseFrame(d10ContentPackage, browseEncoder, itemFrameNumber);
                    writeTimecodeFrame(d10ContentPackage, itemFrameNumber, timecodeFile);
                }

                vitc = d10ContentPackage->getVITC();
                ltc = d10ContentPackage->getLTC();
            }
            
            
            // complete PSE failure, VTR error and digibeta dropout for current frame
            
            if (pseFailures != 0 && firstPSEFailure + numItemPSEFailures < numPSEFailures)
            {
                PSEFailure* pseFailure = &pseFailures[firstPSEFailure + numItemPSEFailures];
                
                if (pseFailure->position == inputFrameNumber)
                {
                    pseFailure->vitcTimecode = convert_timecode(vitc);
                    pseFailure->ltcTimecode = convert_timecode(ltc);
                    pseFailure->position = itemFrameNumber; // position is now relative to start of item
                    
                    numItemPSEFailures++;
                }
            }
            if (vtrErrors != 0 && firstVTRError + numItemVTRErrors < numVTRErrors)
            {
                VTRErrorAtPos* vtrPosError = &vtrPosErrors[firstVTRError + numItemVTRErrors];
                VTRError* vtrError = &vtrErrors[firstVTRError + numItemVTRErrors];
                
                if (vtrPosError->position == inputFrameNumber)
                {
                    vtrError->vitcTimecode = convert_timecode(vitc);
                    vtrError->ltcTimecode = convert_timecode(ltc);
                    vtrError->errorCode = vtrPosError->errorCode;
                    
                    numItemVTRErrors++;
                }
            }
            if (digiBetaDropouts != 0 && firstDigiBetaDropout + numItemDigiBetaDropouts < numDigiBetaDropouts)
            {
                DigiBetaDropout* digiBetaDropout = &digiBetaDropouts[firstDigiBetaDropout + numItemDigiBetaDropouts];
                
                if (digiBetaDropout->position == inputFrameNumber)
                {
                    digiBetaDropout->position = itemFrameNumber; // position is now relative to start of item
                    
                    numItemDigiBetaDropouts++;
                }
            }
            
            inputFrameNumber++;
            itemFrameNumber++;
            

            // update status
            {
                LOCK_SECTION(_statusMutex);
                _status.frameNumber = inputFrameNumber;
                _status.itemNumber = itemNumber;
            }
            
            
            // complete the item if we are at the end
            if (itemFrameNumber == item->duration)
            {
                // complete the item
                
                InfaxData infaxData;
                _session->getInfaxData(item, &infaxData);
                
                _hddDest->materialPackageUID = outputMXFFile->getMaterialPackageUID();
                _hddDest->filePackageUID = outputMXFFile->getFileSourcePackageUID();
                _hddDest->tapePackageUID = outputMXFFile->getTapeSourcePackageUID();
                
                REC_CHECK(outputMXFFile->complete(&infaxData,
                    (numItemPSEFailures > 0) ? &pseFailures[firstPSEFailure] : 0, numItemPSEFailures, 
                    (numItemVTRErrors > 0) ? &vtrErrors[firstVTRError] : 0, numItemVTRErrors,
                    (numItemDigiBetaDropouts > 0) ? &digiBetaDropouts[firstDigiBetaDropout] : 0, numItemDigiBetaDropouts));
                    
                delete outputMXFFile;
                outputMXFFile = 0;
                
                if (_session->getProfile()->browse_enable)
                {
                    SAFE_DELETE(browseEncoder);
                    
                    if (timecodeFile != 0)
                    {
                        fclose(timecodeFile);
                    }
                    timecodeFile = 0;
    
                    _session->writeBrowseInfo(cache->getCompleteBrowseFilename(_browseInfoFilename), item, false);
                }
                
                int pseResult = 0;
                if (!_disablePSE)
                {
                    pseResult = writePSEReport(itemFrameNumber, &infaxData, 
                        (numItemPSEFailures > 0) ? &pseFailures[firstPSEFailure] : 0, numItemPSEFailures);
                }


                Logging::info("Completed chunking of item %d\n", itemNumber);


                // update hard disk destination in database
                
                _hddDest->duration = itemFrameNumber;
                _hddDest->pseResult = pseResult;
                _hddDest->size = _session->_recorder->getCache()->getCreatingFileSize(_hddDest->name);
                _hddDest->browseSize = _session->_recorder->getCache()->getBrowseFileSize(_hddDest->browseName);
                _session->updateHardDiskDestination(_hddDest);
                
                
                // update items
                
                _recordingItems->setChunked(item->id, cache->getCompleteCreatingFilename(_mxfFilename));
                
                
                // move on to the next item

                firstPSEFailure += numItemPSEFailures;
                numItemPSEFailures = 0;
                firstVTRError += numItemVTRErrors;
                numItemVTRErrors = 0;
                firstDigiBetaDropout += numItemDigiBetaDropouts;
                numItemDigiBetaDropouts = 0;

                itemIter++;
                if (itemIter == items.end() || (*itemIter).isDisabled)
                {
                    completed = true;
                    break;
                }
                
                itemFrameNumber = 0;
                itemNumber++;
                item = &*itemIter;
            }
        }
        
        
        // check all frames from the input mxf file have been used 
        if (completed && _inputMXFFile->nextFrame(false, contentPackage))
        {
            REC_LOGTHROW(("MXF file has extra frames"));
        }

        
        delete [] vtrErrors;
    }
    catch (...)
    {
        delete outputMXFFile;
        
        if (_session->getProfile()->browse_enable)
        {
            SAFE_DELETE(browseEncoder);
            if (timecodeFile != 0)
            {
                fclose(timecodeFile);
            }
        }

        delete [] vtrErrors;
        
        
        completed = false;
    }
    
    
    if (completed)
    {
        setChunkingState(CHUNKING_COMPLETED);
    }
    else
    {
        setChunkingState(CHUNKING_FAILED);
    }
}

void Chunking::stop()
{
    _stop = true;
}

bool Chunking::hasStopped() const
{
    return _hasStopped;
}

ChunkingStatus Chunking::getStatus()
{
    LOCK_SECTION(_statusMutex);
    return _status;
}

void Chunking::setChunkingState(ChunkingState state)
{
    LOCK_SECTION(_statusMutex);

    _status.state = state;
}

ChunkingState Chunking::getChunkingState()
{
    LOCK_SECTION(_statusMutex);

    return _status.state;
}

void Chunking::writeBrowseFrame(ArchiveMXFContentPackage* contentPackage, BrowseEncoder* browseEncoder,
                                int64_t frameNumber)
{
    // convert audio to 16 bit stereo
    if (contentPackage->getNumAudioTracks() > 0)
    {
        convertAudio(contentPackage->getNumAudioTracks(),
                     contentPackage->getAudio(0), contentPackage->getAudio(1),
                     (uint16_t*)_stereoAudio);
    }
    
    // convert video to yuv420
    uyvy_to_yuv420(720, 576, 0, contentPackage->getVideo8Bit(), _yuv420Video);

    // encode
    REC_CHECK(browseEncoder->encode(_yuv420Video, _stereoAudio, frameNumber));
}
            
void Chunking::writeTimecodeFrame(ArchiveMXFContentPackage* contentPackage, int64_t frameNumber, FILE* timecodeFile)
{
    Timecode ctc, vitc, ltc;

    ctc.hour = frameNumber / (60 * 60 * 25);
    ctc.min = (frameNumber % (60 * 60 * 25)) / (60 * 25);
    ctc.sec = ((frameNumber % (60 * 60 * 25)) % (60 * 25)) / 25;
    ctc.frame = ((frameNumber % (60 * 60 * 25)) % (60 * 25)) % 25;
    
    vitc = contentPackage->getVITC();
    ltc = contentPackage->getLTC();
    
    fprintf(timecodeFile, "C%02d:%02d:%02d:%02d V%02d:%02d:%02d:%02d L%02d:%02d:%02d:%02d\n", 
            ctc.hour, ctc.min, ctc.sec, ctc.frame,
            vitc.hour, vitc.min, vitc.sec, vitc.frame,
            ltc.hour, ltc.min, ltc.sec, ltc.frame);
}
            
void Chunking::writeBrowseFrame(D10MXFContentPackage* contentPackage, BrowseEncoder* browseEncoder, int64_t frameNumber)
{
    // convert audio to 16 bit stereo
    if (contentPackage->getNumAudioTracks() > 0)
    {
        convertAudio(contentPackage->getNumAudioTracks(),
                     contentPackage->getAudio(0), contentPackage->getAudio(1),
                     (uint16_t*)_stereoAudio);
    }

    // encode
    REC_CHECK(browseEncoder->encode(contentPackage->getDecodedVideo(), _stereoAudio, frameNumber));
}
            
void Chunking::writeTimecodeFrame(D10MXFContentPackage* contentPackage, int64_t frameNumber, FILE* timecodeFile)
{
    Timecode ctc, vitc, ltc;
    
    ctc.hour = frameNumber / (60 * 60 * 25);
    ctc.min = (frameNumber % (60 * 60 * 25)) / (60 * 25);
    ctc.sec = ((frameNumber % (60 * 60 * 25)) % (60 * 25)) / 25;
    ctc.frame = ((frameNumber % (60 * 60 * 25)) % (60 * 25)) % 25;
    
    vitc = contentPackage->getVITC();
    ltc = contentPackage->getLTC();
    
    fprintf(timecodeFile, "C%02d:%02d:%02d:%02d V%02d:%02d:%02d:%02d L%02d:%02d:%02d:%02d\n", 
            ctc.hour, ctc.min, ctc.sec, ctc.frame,
            vitc.hour, vitc.min, vitc.sec, vitc.frame,
            ltc.hour, ltc.min, ltc.sec, ltc.frame);
}
            
int Chunking::writePSEReport(int64_t duration, InfaxData* infaxData, PSEFailure* pseFailures, long numPSEFailures)
{
    string filename = _session->_recorder->getCache()->getCompletePSEFilename(_pseFilename);
    
    PSEReport* pseReport = PSEReport::open(filename);
    if (pseReport == 0) 
    {
        REC_LOGTHROW(("Failed to open PSE report file '%s' for writing: %s", filename.c_str()));
    }
    
    try
    {
        bool passed;
        REC_CHECK(pseReport->write(0, duration - 1, "", infaxData, pseFailures, numPSEFailures, &passed));

        delete pseReport;

        return (passed) ? PSE_RESULT_PASSED : PSE_RESULT_FAILED;
    }
    catch (...)
    {
        delete pseReport;
        throw;
    }
}

void Chunking::convertAudio(int numAudioTracks, const unsigned char *inputA1, const unsigned char *inputA2,
                            uint16_t *outputA12)
{
    // convert audio to 16 bit stereo
    int i;
    if (numAudioTracks > 1)
    {
        for (i = 0; i < 1920 * 3; i += 3) 
        {
            *outputA12++ = (((uint16_t)inputA1[i + 2]) << 8) | inputA1[i + 1];
            *outputA12++ = (((uint16_t)inputA2[i + 2]) << 8) | inputA2[i + 1];
        }
    }
    else
    {
        for (i = 0; i < 1920 * 3; i += 3) 
        {
            *outputA12++ = (((uint16_t)inputA1[i + 2]) << 8) | inputA1[i + 1];
            *outputA12++ = 0;
        }
    }
}

