/*
 * $Id: MXFWriter.h,v 1.2 2007/01/30 14:09:48 john_f Exp $
 *
 * Writes essence data to MXF files
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
 
#ifndef __PRODAUTO_MXFWRITER_H__
#define __PRODAUTO_MXFWRITER_H__

#include <map>
#include <vector>
#include <set>

#include <Recorder.h>
#include <SourceSession.h>
#include <Package.h>



// must match declaration in write_avid_mxf.h
typedef struct _AvidClipWriter AvidClipWriter;

namespace prodauto
{

class OutputPackage
{
public:
    OutputPackage();
    ~OutputPackage();
    
    void updateDuration(uint32_t materialTrackID, uint32_t numSamples);
    
    AvidClipWriter* clipWriter;
    
    SourcePackage* sourcePackage;
    std::vector<SourcePackage*> filePackages;
    MaterialPackage* materialPackage;
};
    
class MXFTrackWriter
{
public:
    MXFTrackWriter();
    ~MXFTrackWriter();
    
    AvidClipWriter* clipWriter;
    OutputPackage* outputPackage;
    std::map<uint32_t, uint32_t> inputToMPTrackMap;
};


class MXFWriter
{
public:
    MXFWriter(RecorderInputConfig* inputConfig,
        SourceSession* sourceSession, int resolutionID, Rational imageAspectRatio, 
        uint8_t audioQuantizationBits, uint32_t inputMask, int64_t startPosition, 
        std::string creatingFilePath, std::string destinationFilePath,
        std::string failuresFilePath, std::string filenamePrefix,
        std::vector<UserComment> userComments, std::string avidProjectName);
    ~MXFWriter();
    
    // returns true if the track is present    
    bool trackIsPresent(uint32_t trackID);

    
    void writeSample(uint32_t trackID, uint32_t numSamples, uint8_t* data, uint32_t size);

    // use these 3 functions if you need to write raw data and only at the end
    // indicate the number of samples written
    // e.g. startSampleData() -> writeSampleData() ... -> endSampleData()
    //   Note: you cannot call writeSample() between startSampleData() and endSampleData()
    void startSampleData(uint32_t trackID);    
    void writeSampleData(uint32_t trackID, uint8_t* data, uint32_t size);
    void endSampleData(uint32_t trackID, uint32_t numSamples);    

    void completeAndSaveToDatabase();

    
    // returns true if the file corresponding to the track was successfully completed
    bool wasSuccessfull(uint32_t trackID);
    
    // returns the filename associated with the track
    std::string getFilename(uint32_t trackID);
    
    // adds the creating/destination/failures file path to the filename
    std::string getCreatingFilename(std::string filename);
    std::string getDestinationFilename(std::string filename);
    std::string getFailuresFilename(std::string filename);
    
private:
    OutputPackage* getOutputPackage(SourceSession* sourceSession, std::string sourceName); 

    bool _isComplete;
    std::map<uint32_t, MXFTrackWriter*> _trackWriters;
    std::vector<OutputPackage*> _outputPackages;
    std::set<uint32_t> _sampleDataTracks;
    
    std::string _creatingFilePath;
    std::string _destinationFilePath;
    std::string _failuresFilePath;
    std::string _filenamePrefix;
    
    std::map<UMID, std::string> _filenames;
    std::map<uint32_t, std::string> _filenames2;
    
    std::map<UMID, uint32_t> _filePackageToInputTrackMap;
    std::map<uint32_t, bool> _status;
};


};



#endif 


