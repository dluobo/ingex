/*
 * $Id: PackageXMLReader.h,v 1.1 2010/07/21 16:29:34 john_f Exp $
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

#ifndef __PRODAUTO_PACKAGE_XML_READER_H__
#define __PRODAUTO_PACKAGE_XML_READER_H__


#include <string>
#include <vector>

#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/dom/DOMElement.hpp>

#include "DataTypes.h"



namespace prodauto
{


class DatabaseCache;


class PackageXMLReader;

class PackageXMLChildParser
{
public:
    virtual void ParseXMLChild(PackageXMLReader *reader, std::string name) = 0;
};


class PackageXMLReader
{
public:
    PackageXMLReader(DatabaseCache *db_cache);
    virtual ~PackageXMLReader();

    void Parse(std::string filename, PackageXMLChildParser *root_parser);

public:
    std::string GetCurrentElementName();

    void ParseChildElements(PackageXMLChildParser *parser);

    bool HaveAttr(std::string attr_name, bool allow_empty);
    std::string ParseStringAttr(std::string attr_name);
    bool ParseBoolAttr(std::string attr_name);
    int ParseIntAttr(std::string attr_name);
    long ParseLongAttr(std::string attr_name);
    uint32_t ParseUInt32Attr(std::string attr_name);
    int64_t ParseInt64Attr(std::string attr_name);
    Rational ParseRationalAttr(std::string attr_name);
    UMID ParseUMIDAttr(std::string attr_name);
    Timestamp ParseTimestampAttr(std::string attr_name);
    int ParseColourAttr(std::string attr_name);
    int ParseOPAttr(std::string attr_name);

    bool HaveDatabaseCache() { return mDatabaseCache != 0; }
    DatabaseCache* GetDatabaseCache() { return mDatabaseCache; }

protected:
    DatabaseCache *mDatabaseCache;
    xercesc::XercesDOMParser *mParser;
    PackageXMLChildParser *mRootParser;
    std::vector<xercesc::DOMElement*> mElementParseStack;

private:
    void Reset();
};


};



#endif