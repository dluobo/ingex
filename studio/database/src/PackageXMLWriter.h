/*
 * $Id: PackageXMLWriter.h,v 1.1 2010/06/02 13:04:40 john_f Exp $
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
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

#ifndef __PRODAUTO_PACKAGE_XML_WRITER_H__
#define __PRODAUTO_PACKAGE_XML_WRITER_H__


#include <XMLWriter.h>

#include "DataTypes.h"



namespace prodauto
{


class PackageXMLWriter : public XMLWriter
{
public:
    PackageXMLWriter(FILE *mxf_file);
    virtual ~PackageXMLWriter();
    
    void DeclareDefaultNamespace();

    void WriteElementStart(std::string local_name);
    void WriteAttribute(std::string local_name, std::string value);
    void WriteAttributeStart(std::string local_name);
    
    void WriteBoolAttribute(std::string local_name, bool value);
    void WriteIntAttribute(std::string local_name, int value);
    void WriteUInt32Attribute(std::string local_name, uint32_t value);
    void WriteInt64Attribute(std::string local_name, int64_t value);
    void WriteRationalAttribute(std::string local_name, const Rational &value);
    void WriteUMIDAttribute(std::string local_name, const UMID &value);
    void WriteTimestampAttribute(std::string local_name, const Timestamp &value);
    void WriteColourAttribute(std::string local_name, int colour);
    void WriteOPAttribute(std::string local_name, int op);
};


};



#endif
