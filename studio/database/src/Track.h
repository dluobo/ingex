/*
 * $Id: Track.h,v 1.5 2010/07/21 16:29:34 john_f Exp $
 *
 * A Track in a Package
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
 
#ifndef __PRODAUTO_TRACK_H__
#define __PRODAUTO_TRACK_H__


#include <string>
#include <vector>

#include "DatabaseObject.h"
#include "DataTypes.h"
#include "SourceClip.h"
#include "PackageXMLWriter.h"
#include "PackageXMLReader.h"



namespace prodauto
{

class Track : public DatabaseObject, protected PackageXMLChildParser
{
public:
    Track();
    virtual ~Track();
    
    virtual std::string toString();

    virtual void cloneInPlace(bool resetLengths);
    Track* clone();
    void toXML(PackageXMLWriter *xml_writer);
    void fromXML(PackageXMLReader *xml_reader);
    
    uint32_t id;
    uint32_t number;
    std::string name;
    Rational editRate;
    int64_t origin;
    int dataDef;
    SourceClip* sourceClip;
    
protected:
    // from PackageXMLChildParser
    virtual void ParseXMLChild(PackageXMLReader *xml_reader, std::string name);
};
    
    
};





#endif


