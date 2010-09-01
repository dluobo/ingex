/*
 * $Id: PSEReportWrapper.cpp,v 1.2 2010/09/01 16:05:22 philipn Exp $
 *
 * Wraps the PSEReport class and provides it with the info to write the report 
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

#define __STDC_FORMAT_MACROS 1

#include <cstdio>
#include <cerrno>
#include <inttypes.h>

#include <PSEReport.h>

#include "PSEReportWrapper.h"
#include "RecorderException.h"
#include "Logging.h"


using namespace std;
using namespace rec;




PSEReportWrapper::PSEReportWrapper(string filename, ArchiveMXFFile* mxfFile, QCSessionFile* sessionFile)
: _report(0), _mxfFile(mxfFile), _sessionFile(sessionFile)
{
    _report = PSEReport::open(filename);
    if (_report == 0)
    {
        throw RecorderException("Failed to open pse report '%s'\n", filename.c_str());
    }
}

PSEReportWrapper::~PSEReportWrapper()
{
    delete _report;
}

void PSEReportWrapper::write()
{
    const InfaxData* sourceInfaxData = _mxfFile->getSourceInfaxData();
    Mark programmeClip;
    const PSEFailure* firstPSEFailure;
    long numPSEFailures;
    
    if (_sessionFile != 0)
    {
        programmeClip = _sessionFile->getProgrammeClip();
    }
    
    getPSEResultRange(&firstPSEFailure, &numPSEFailures);

    
    // generate report
    
    int64_t firstFrame = (programmeClip.duration > 0) ? programmeClip.position : 0;
    int64_t lastFrame = (programmeClip.duration > 0) ? programmeClip.position + programmeClip.duration - 1 : _mxfFile->getDuration() - 1;
    
    bool dummy;
    REC_CHECK(_report->write(firstFrame, lastFrame, _mxfFile->getFilename(), sourceInfaxData, 
        firstPSEFailure, numPSEFailures, &dummy));
}

bool PSEReportWrapper::getPSEResult()
{
    const PSEFailure* firstPSEFailure;
    long numPSEFailures;
    getPSEResultRange(&firstPSEFailure, &numPSEFailures);
    
    return PSEReport::hasPassed(firstPSEFailure, numPSEFailures);
}

void PSEReportWrapper::getPSEResultRange(PSEFailure const ** firstPSEFailure, long* numPSEFailures)
{
    PSEFailure* pseFailures = 0;
    long intnumPSEFailures = _mxfFile->getPSEFailures(&pseFailures);
    
    Mark programmeClip;
    
    // no failures means it has passed
    if (intnumPSEFailures == 0)
    {
        *firstPSEFailure = 0;
        *numPSEFailures = 0;
        return;
    }
    
    // find PSE failures within clip boundaries
    if (_sessionFile != 0)
    {
        programmeClip = _sessionFile->getProgrammeClip();
    }
    if (programmeClip.duration > 0)
    {
        *numPSEFailures = 0;
        *firstPSEFailure = 0;
        long i;
        for (i = 0; i < intnumPSEFailures; i++)
        {
            const PSEFailure& pseFailure = pseFailures[i];
            
            if (pseFailure.position < programmeClip.position)
            {
                // before start of programme
                continue;
            }
            else if (pseFailure.position > programmeClip.position + programmeClip.duration - 1)
            {
                // after end of programme
                break;
            }
            
            if ((*firstPSEFailure) == 0)
            {
                *firstPSEFailure = &pseFailure;
            }
            (*numPSEFailures)++;
        }
    }
    else
    {
        *firstPSEFailure = &pseFailures[0];
        *numPSEFailures = intnumPSEFailures;
    }
}

