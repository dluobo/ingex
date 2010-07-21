/*
 * $Id: SourceClip.h,v 1.4 2010/07/21 16:29:34 john_f Exp $
 *
 * A Source Clip in a Track referencing a Package or null
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
 
#ifndef __PRODAUTO_SOURCECLIP_H__
#define __PRODAUTO_SOURCECLIP_H__


#include <string>

#include "DatabaseObject.h"
#include "DataTypes.h"
#include "PackageXMLWriter.h"
#include "PackageXMLReader.h"



namespace prodauto
{

class SourceClip : public DatabaseObject
{
public:
    SourceClip();
    virtual ~SourceClip();
    
    virtual std::string toString();

    virtual void cloneInPlace(bool resetLength);
    SourceClip* clone();
    void toXML(PackageXMLWriter *xml_writer);
    void fromXML(PackageXMLReader *xml_reader);

    UMID sourcePackageUID;
    uint32_t sourceTrackID;
    int64_t length;
    int64_t position;
};
    
    
};





#endif

