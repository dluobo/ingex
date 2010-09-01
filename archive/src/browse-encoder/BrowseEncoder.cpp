/*
 * $Id: BrowseEncoder.cpp,v 1.2 2010/09/01 16:05:22 philipn Exp $
 *
 * C++ wrapper for browse_encoder
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

#include "BrowseEncoder.h"
#include "logF.h"

using namespace std;



BrowseEncoder* BrowseEncoder::create(const char *filename, rec::Rational aspectRatio, uint32_t kbit_rate, int thread_count)
{
    browse_encoder_t* encoder = browse_encoder_init(filename, aspectRatio.numerator, aspectRatio.denominator, kbit_rate, thread_count);
    if (encoder == 0)
    {
        return 0;
    }
    
    return new BrowseEncoder(encoder);
}

BrowseEncoder::BrowseEncoder(browse_encoder_t* encoder)
: _encoder(encoder)
{
}

BrowseEncoder::~BrowseEncoder()
{
    if (browse_encoder_close(_encoder) != 0)
    {
        logFF("Failed to complete browse file\n");
    }
    else
    {
        logTF("Browse file completed\n");
    }
}

bool BrowseEncoder::encode(const uint8_t *p_video, const int16_t *p_audio, int32_t frame_number)
{
    return browse_encoder_encode(_encoder, p_video, p_audio, frame_number) == 0;
}

