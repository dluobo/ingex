/*
 * $Id: DigiBetaDropoutDetector.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * C++ wrapper for the DigiBeta dropout detector
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

#include <cassert>

#include "DigiBetaDropoutDetector.h"

#include "YUV_frame.h"
#include "digibeta_dropout.h"

using namespace std;



DigiBetaDropoutDetector::DigiBetaDropoutDetector(int width, int height,
    int lowerThreshold, int upperThreshold, int storeThreshold)
{
    assert(upperThreshold - lowerThreshold > 0);
    
    _width = width;
    _height = height;
    _lowerThreshold = lowerThreshold;
    _upperThreshold = upperThreshold;
    _normalizedStoreThreshold = (int32_t)(100 * (storeThreshold - lowerThreshold) /
                                            (float)(upperThreshold - lowerThreshold));
    _position = 0;
    
    _workspace = new int[width * height];
}

DigiBetaDropoutDetector::~DigiBetaDropoutDetector()
{
    delete [] _workspace;
}

void DigiBetaDropoutDetector::processPicture(unsigned char *data, unsigned int dataSize)
{
    YUV_frame frame;
    
    assert(dataSize == _width * _height * 2);
    
    YUV_frame_from_buffer(&frame, data, _width, _height, UYVY);
    
    dropout_result result[2];
    result[0] = digibeta_dropout(&frame, 0, 0, _workspace); // field 1
    result[1] = digibeta_dropout(&frame, 0, 1, _workspace); // field 2
    
    int32_t maxNormalizedStrength;
    if (result[0].strength > result[1].strength)
    {
        maxNormalizedStrength = (int32_t)(100 * (result[0].strength - _lowerThreshold) /
                                            (float)(_upperThreshold - _lowerThreshold));
    }
    else
    {
        maxNormalizedStrength = (int32_t)(100 * (result[1].strength - _lowerThreshold) /
                                            (float)(_upperThreshold - _lowerThreshold));
    }
    
    if (maxNormalizedStrength >= _normalizedStoreThreshold)
    {
        DigiBetaDropout dropout;
        dropout.position = _position;
        dropout.strength = maxNormalizedStrength;
        
        _dropouts.push_back(dropout);
    }
    
    _position++;
}

