/*
 * $Id: PackageXMLReader.cpp,v 1.1 2010/07/21 16:29:34 john_f Exp $
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

#define __STDC_FORMAT_MACROS 1 // for PRId64 macro

#include <cstdio>

#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMDocument.hpp>

#include <XmlTools.h>

#include "PackageXMLReader.h"
#include "DatabaseCache.h"
#include "DatabaseEnums.h"
#include "MaterialResolution.h"
#include "Logging.h"
#include "ProdAutoException.h"

using namespace std;
using namespace prodauto;



PackageXMLReader::PackageXMLReader(DatabaseCache *db_cache)
{
    mDatabaseCache = db_cache;
    mParser = 0;
    mRootParser = 0;

    try
    {
        XMLPlatformUtils::Initialize();
    }
    catch (...)
    {
        PA_LOGTHROW(ProdAutoException, ("XML toolkit initialization error"));
    }
}

PackageXMLReader::~PackageXMLReader()
{
    Reset();

    // not calling XMLPlatformUtils::Terminate() to allow use of multiple readers
    // from the docs: "The termination call is currently optional, to aid those dynamically loading the parser to clean
    //                 up before exit, or to avoid spurious reports from leak detectors."
}

void PackageXMLReader::Parse(string filename, PackageXMLChildParser *root_parser)
{
    Reset();

    mRootParser = root_parser;
    
    try
    {
        // open parser and parse the document

        mParser = new XercesDOMParser();
        mParser->setValidationScheme(XercesDOMParser::Val_Never);
        mParser->setDoNamespaces(false);
        mParser->setDoSchema(false);
        mParser->setLoadExternalDTD(false);

        // TODO: handle parse errors, e.g. file not exist
        mParser->parse(filename.c_str());

        DOMDocument *doc = mParser->getDocument();
        if (!doc)
            throw ProdAutoException("File does not exist or is not an XML file\n");

        DOMElement *root = doc->getDocumentElement();
        if (!root)
            throw ProdAutoException("Empty XML file\n");

        mElementParseStack.push_back(root);
    }
    catch (...)
    {
        Reset();
        throw;
    }
}

string PackageXMLReader::GetCurrentElementName()
{
    PA_ASSERT(!mElementParseStack.empty());
    DOMElement *element = mElementParseStack.back();

    return StrXml(element->getTagName()).localForm();
}

void PackageXMLReader::ParseChildElements(PackageXMLChildParser *parser)
{
    PA_ASSERT(!mElementParseStack.empty());
    DOMElement *element = mElementParseStack.back();

    DOMNodeList *children = element->getChildNodes();
    const XMLSize_t children_count = children->getLength();
    XMLSize_t i;
    for (i = 0; i < children_count; i++) {
        DOMNode *node = children->item(i);

        if (node->getNodeType() && node->getNodeType() == DOMNode::ELEMENT_NODE) {
            DOMElement *child_element = dynamic_cast<xercesc::DOMElement*>(node);

            mElementParseStack.push_back(child_element);
            parser->ParseXMLChild(this, StrXml(child_element->getTagName()).localForm());
            mElementParseStack.pop_back();
        }
    }
}

bool PackageXMLReader::HaveAttr(string attr_name, bool allow_empty)
{
    PA_ASSERT(!mElementParseStack.empty());
    DOMElement *element = mElementParseStack.back();

    const XMLCh *x_attr = element->getAttribute(Xml(attr_name.c_str()));
    return x_attr && (allow_empty || x_attr[0] != (XMLCh)0);
}

string PackageXMLReader::ParseStringAttr(string attr_name)
{
    PA_ASSERT(!mElementParseStack.empty());
    DOMElement *element = mElementParseStack.back();

    const XMLCh *x_attr = element->getAttribute(Xml(attr_name.c_str()));
    if (!x_attr)
        throw ProdAutoException("Missing attribute '%s'", attr_name.c_str());

    return StrXml(x_attr).localForm();
}

bool PackageXMLReader::ParseBoolAttr(string attr_name)
{
    string attr_str;
    
    attr_str = ParseStringAttr(attr_name);
    
    if (attr_str == "true")
        return true;
    else if (attr_str == "false")
        return false;
    else
        throw ProdAutoException("Invalid bool value '%s'='%s'", attr_name.c_str(), attr_str.c_str());
}

int PackageXMLReader::ParseIntAttr(string attr_name)
{
    string attr_str;
    int value;
    
    attr_str = ParseStringAttr(attr_name);
    
    if (sscanf(attr_str.c_str(), "%d", &value) != 1)
        throw ProdAutoException("Invalid int value '%s'='%s'", attr_name.c_str(), attr_str.c_str());
    
    return value;
}

long PackageXMLReader::ParseLongAttr(string attr_name)
{
    string attr_str;
    long value;
    
    attr_str = ParseStringAttr(attr_name);
    
    if (sscanf(attr_str.c_str(), "%ld", &value) != 1)
        throw ProdAutoException("Invalid int value '%s'='%s'", attr_name.c_str(), attr_str.c_str());
    
    return value;
}

uint32_t PackageXMLReader::ParseUInt32Attr(string attr_name)
{
    string attr_str;
    unsigned int value;
    
    attr_str = ParseStringAttr(attr_name);
    
    if (sscanf(attr_str.c_str(), "%u", &value) != 1)
        throw ProdAutoException("Invalid uint32 value '%s'='%s'", attr_name.c_str(), attr_str.c_str());
    
    return value;
}

int64_t PackageXMLReader::ParseInt64Attr(string attr_name)
{
    string attr_str;
    int64_t value;
    
    attr_str = ParseStringAttr(attr_name);
    
    if (sscanf(attr_str.c_str(), "%"PRId64, &value) != 1)
        throw ProdAutoException("Invalid int64 value '%s'='%s'", attr_name.c_str(), attr_str.c_str());
    
    return value;
}

Rational PackageXMLReader::ParseRationalAttr(string attr_name)
{
    string attr_str;
    Rational value;
    int num, den;
    
    attr_str = ParseStringAttr(attr_name);
    
    if (sscanf(attr_str.c_str(), "%d/%d", &num, &den) != 2)
        throw ProdAutoException("Invalid Rational value '%s'='%s'", attr_name.c_str(), attr_str.c_str());
    
    value.numerator = num;
    value.denominator = den;
    
    return value;
}

UMID PackageXMLReader::ParseUMIDAttr(string attr_name)
{
    string attr_str;
    UMID value;
    unsigned int bytes[32];
    
    attr_str = ParseStringAttr(attr_name);
    
    if (sscanf(attr_str.c_str(), "urn:smpte:umid:"
                                 "%02x%02x%02x%02x.%02x%02x%02x%02x."
                                 "%02x%02x%02x%02x.%02x%02x%02x%02x."
                                 "%02x%02x%02x%02x.%02x%02x%02x%02x."
                                 "%02x%02x%02x%02x.%02x%02x%02x%02x",
               &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5], &bytes[6], &bytes[7],
               &bytes[8], &bytes[9], &bytes[10], &bytes[11], &bytes[12], &bytes[13], &bytes[14], &bytes[15],
               &bytes[16], &bytes[17], &bytes[18], &bytes[19], &bytes[20], &bytes[21], &bytes[22], &bytes[23],
               &bytes[24], &bytes[25], &bytes[26], &bytes[27], &bytes[28], &bytes[29], &bytes[30], &bytes[31]) != 32)
    {
        throw ProdAutoException("Invalid UMID value '%s'='%s'", attr_name.c_str(), attr_str.c_str());
    }
    
    int i;
    for (i = 0; i < 32; i++)
        (&value.octet0)[i] = (uint8_t)bytes[i];
    
    return value;
}

Timestamp PackageXMLReader::ParseTimestampAttr(string attr_name)
{
    string attr_str;
    Timestamp value;
    int year, month, day;
    int hour, min, sec, msec;
    
    attr_str = ParseStringAttr(attr_name);
    
    if (sscanf(attr_str.c_str(), "%04d-%02u-%02uT%02u:%02u:%02u.%03uZ",
               &year, &month, &day, &hour, &min, &sec, &msec) != 7)
    {
        throw ProdAutoException("Invalid Timestamp value '%s'='%s'", attr_name.c_str(), attr_str.c_str());
    }
    
    value.year = year;
    value.month = month;
    value.day = day;
    value.hour = hour;
    value.min = min;
    value.sec = sec;
    value.qmsec = msec / 4;
    
    return value;
}

int PackageXMLReader::ParseColourAttr(string attr_name)
{
    string attr_str;
    
    attr_str = ParseStringAttr(attr_name);
    
    if (attr_str == "White")
        return USER_COMMENT_WHITE_COLOUR;
    else if (attr_str == "Red")
        return USER_COMMENT_RED_COLOUR;
    else if (attr_str == "Yellow")
        return USER_COMMENT_YELLOW_COLOUR;
    else if (attr_str == "Green")
        return USER_COMMENT_GREEN_COLOUR;
    else if (attr_str == "Cyan")
        return USER_COMMENT_CYAN_COLOUR;
    else if (attr_str == "Blue")
        return USER_COMMENT_BLUE_COLOUR;
    else if (attr_str == "Magenta")
        return USER_COMMENT_MAGENTA_COLOUR;
    else if (attr_str == "Black")
        return USER_COMMENT_BLACK_COLOUR;
    
    Logging::warning("Unknown colour '%s'='%s' - defaulting to white\n", attr_name.c_str(), attr_str.c_str());
    return USER_COMMENT_WHITE_COLOUR;
}

int PackageXMLReader::ParseOPAttr(string attr_name)
{
    string attr_str;
    
    attr_str = ParseStringAttr(attr_name);
    
    if (attr_str == "OP-Atom")
        return OperationalPattern::OP_ATOM;
    else if (attr_str == "OP-1A")
        return OperationalPattern::OP_1A;
    
    Logging::warning("Unknown OP '%s'='%s' - defaulting to 0\n", attr_name.c_str(), attr_str.c_str());
    return 0;
}

void PackageXMLReader::Reset()
{
    delete mParser;
    mParser = 0;
    mRootParser = 0;
    mElementParseStack.clear();
}
