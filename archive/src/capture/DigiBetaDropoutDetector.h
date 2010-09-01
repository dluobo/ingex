/*
 * $Id: DigiBetaDropoutDetector.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#ifndef __RECORDER_DIGIBETA_DROPOUT_DETECTOR_H__
#define __RECORDER_DIGIBETA_DROPOUT_DETECTOR_H__


#include <vector>

#include <archive_types.h>


class DigiBetaDropoutDetector
{
public:
    DigiBetaDropoutDetector(int width, int height,
        int lowerThreshold, int upperThreshold, int storeThreshold);
    ~DigiBetaDropoutDetector();
    
    void processPicture(unsigned char *data, unsigned int dataSize);


    std::vector<DigiBetaDropout>& getDropouts() { return _dropouts; }
    size_t getNumDropouts() { return _dropouts.size(); }

private:
    unsigned int _width;
    unsigned int _height;
    int _lowerThreshold;
    int _upperThreshold;
    int _normalizedStoreThreshold;
    int *_workspace;
    int64_t _position;
    std::vector<DigiBetaDropout> _dropouts;
};



#endif

