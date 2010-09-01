/*
 * $Id: test_ltc_decoder.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * Test the LTC audio signal decoder
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
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

#include "LTCDecoder.h"
#include "ArchiveMXFFile.h"

using namespace rec;



static void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s <audio track num> <mxf filename>\n", cmd);
}

int main(int argc, const char **argv)
{
    int audioTrack;
    const char* mxfFilename;
    
    if (argc != 3)
    {
        usage(argv[0]);
        return 1;
    }
    if (sscanf(argv[1], "%d", &audioTrack) != 1 || audioTrack < 0 || audioTrack >= 8)
    {
        usage(argv[0]);
        fprintf(stderr, "Invalid audio track value '%s'\n", argv[1]);
        return 1;
    }
    mxfFilename = argv[2];
    
    
    ArchiveMXFFile mxfFile(mxfFilename);
    
    unsigned char buffer[1920];
    long frameCount = 0;
    Timecode decodedLTC;
    LTCDecoder decoder;
    int j;
    MXFContentPackage* contentPackage;
    ArchiveMXFContentPackage* archiveContentPackage;
    Timecode ltc, prevLTC;
    
    while (mxfFile.nextFrame(false, contentPackage))
    {
        archiveContentPackage = dynamic_cast<ArchiveMXFContentPackage*>(contentPackage);
        
        // crude 24-bit to 8-bit conversion
        for (j = 0; j < 1920; j++)
        {
            buffer[j] = 128 + (char)archiveContentPackage->getAudioBuffer(audioTrack)[j * 3 + 2];
        }
        
        decoder.processFrame(buffer, 1920);

        
        ltc = archiveContentPackage->getLTC();
        
        if (!decoder.havePrevFrameTimecode() && !decoder.haveThisFrameTimecode())
        {
            printf("NOT AVAILABLE: %ld: ltc=%02u:%02u:%02u:%02u\n", frameCount,
                   ltc.hour, ltc.min, ltc.sec, ltc.frame);
        }
        else
        {
            decodedLTC = decoder.getTimecode();
            
            if (decoder.havePrevFrameTimecode())
            {
                if (prevLTC != decodedLTC)
                {
                    printf("MISMATCH: ");
                }
                printf("(P) %ld: ltc=%02u:%02u:%02u:%02u, decoded ltc=%02u:%02u:%02u:%02u\n", frameCount - 1,
                       prevLTC.hour, prevLTC.min, prevLTC.sec, prevLTC.frame,
                       decodedLTC.hour, decodedLTC.min, decodedLTC.sec, decodedLTC.frame);
            }
            else
            {
                if (ltc != decodedLTC)
                {
                    printf("MISMATCH: ");
                }
                printf("(T) %ld: ltc=%02u:%02u:%02u:%02u, decoded ltc=%02u:%02u:%02u:%02u\n", frameCount,
                       ltc.hour, ltc.min, ltc.sec, ltc.frame,
                       decodedLTC.hour, decodedLTC.min, decodedLTC.sec, decodedLTC.frame);
            }
        }
        
        prevLTC = ltc;
        
        frameCount++;
    }
    
    
    return 0;
}
