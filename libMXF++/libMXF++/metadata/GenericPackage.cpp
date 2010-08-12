/*
 * $Id: GenericPackage.cpp,v 1.2 2010/08/12 16:25:39 john_f Exp $
 *
 * 
 *
 * Copyright (C) 2008  Philip de Nier <philipn@users.sourceforge.net>
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

#include <libMXF++/MXF.h>


using namespace std;
using namespace mxfpp;



GenericPackage::GenericPackage(HeaderMetadata* headerMetadata)
: GenericPackageBase(headerMetadata)
{}

GenericPackage::GenericPackage(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: GenericPackageBase(headerMetadata, cMetadataSet)
{}

GenericPackage::~GenericPackage()
{}

GenericTrack* GenericPackage::findTrack(uint32_t trackId) const
{
    vector<GenericTrack*> tracks = getTracks();
    size_t i;
    for (i = 0; i < tracks.size(); i++) {
        if (tracks[i]->haveTrackID() && tracks[i]->getTrackID() == trackId)
            return tracks[i];
    }

    return 0;
}

