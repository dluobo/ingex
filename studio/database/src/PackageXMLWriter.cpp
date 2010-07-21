/*
 * $Id: PackageXMLWriter.cpp,v 1.2 2010/07/21 16:29:34 john_f Exp $
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

#define __STDC_FORMAT_MACROS    1

#include "PackageXMLWriter.h"
#include "DatabaseCache.h"
#include "DatabaseEnums.h"
#include "MaterialResolution.h"
#include "Logging.h"

using namespace std;
using namespace prodauto;



static const char *DEFAULT_NAMESPACE = "http://bbc.co.uk/rd/ingex";



PackageXMLWriter::PackageXMLWriter(FILE *xml_file, DatabaseCache *db_cache)
: XMLWriter(xml_file)
{
    mDatabaseCache = db_cache;
}

PackageXMLWriter::~PackageXMLWriter()
{
}

void PackageXMLWriter::DeclareDefaultNamespace()
{
    DeclareNamespace(DEFAULT_NAMESPACE, "");
}

void PackageXMLWriter::WriteElementStart(string local_name)
{
    XMLWriter::WriteElementStart(DEFAULT_NAMESPACE, local_name);
}

void PackageXMLWriter::WriteAttribute(string local_name, string value)
{
    XMLWriter::WriteAttribute(DEFAULT_NAMESPACE, local_name, value);
}

void PackageXMLWriter::WriteAttributeStart(string local_name)
{
    XMLWriter::WriteAttributeStart(DEFAULT_NAMESPACE, local_name);
}

void PackageXMLWriter::WriteBoolAttribute(string local_name, bool value)
{
    if (value)
        WriteAttribute(local_name, "true");
    else
        WriteAttribute(local_name, "false");
}

void PackageXMLWriter::WriteIntAttribute(string local_name, int value)
{
    char buffer[32];
    sprintf(buffer, "%d", value);
    WriteAttribute(local_name, buffer);
}

void PackageXMLWriter::WriteLongAttribute(string local_name, long value)
{
    char buffer[32];
    sprintf(buffer, "%ld", value);
    WriteAttribute(local_name, buffer);
}

void PackageXMLWriter::WriteUInt32Attribute(string local_name, uint32_t value)
{
    char buffer[32];
    sprintf(buffer, "%u", value);
    WriteAttribute(local_name, buffer);
}

void PackageXMLWriter::WriteInt64Attribute(string local_name, int64_t value)
{
    char buffer[32];
    sprintf(buffer, "%"PRId64, value);
    WriteAttribute(local_name, buffer);
}

void PackageXMLWriter::WriteRationalAttribute(string local_name, const Rational &value)
{
    char buffer[64];
    sprintf(buffer, "%d/%d", value.numerator, value.denominator);
    WriteAttribute(local_name, buffer);
}

void PackageXMLWriter::WriteUMIDAttribute(std::string local_name, const UMID &value)
{
    char buffer[128];
    sprintf(buffer, "urn:smpte:umid:");
    
    size_t offset = strlen(buffer);
    int i;
    for (i = 0; i < 32; i++) {
        if (i == 0 || i % 4 != 0)
            offset += sprintf(buffer + offset, "%02x", ((unsigned char*)(&value))[i]);
        else
            offset += sprintf(buffer + offset, ".%02x", ((unsigned char*)(&value))[i]);
    }

    WriteAttribute(local_name, buffer);
}

void PackageXMLWriter::WriteTimestampAttribute(std::string local_name, const Timestamp &value)
{
    char buffer[128];
    sprintf(buffer, "%04d-%02u-%02uT%02u:%02u:%02u.%03uZ",
            value.year, value.month, value.day, value.hour, value.min, value.sec, value.qmsec * 4);
    WriteAttribute(local_name, buffer);
}

void PackageXMLWriter::WriteColourAttribute(std::string local_name, int colour)
{
    string colour_symbol;
    switch (colour)
    {
        case USER_COMMENT_WHITE_COLOUR:
            colour_symbol = "White";
            break;
        case USER_COMMENT_RED_COLOUR:
            colour_symbol = "Red";
            break;
        case USER_COMMENT_YELLOW_COLOUR:
            colour_symbol = "Yellow";
            break;
        case USER_COMMENT_GREEN_COLOUR:
            colour_symbol = "Green";
            break;
        case USER_COMMENT_CYAN_COLOUR:
            colour_symbol = "Cyan";
            break;
        case USER_COMMENT_BLUE_COLOUR:
            colour_symbol = "Blue";
            break;
        case USER_COMMENT_MAGENTA_COLOUR:
            colour_symbol = "Magenta";
            break;
        case USER_COMMENT_BLACK_COLOUR:
            colour_symbol = "Black";
            break;
        default:
            // empty string
            Logging::warning("Unknown colour %d\n", colour);
            break;
    }
    WriteAttribute(local_name, colour_symbol);
}

void PackageXMLWriter::WriteOPAttribute(std::string local_name, int op)
{
    string op_symbol;
    switch (op)
    {
        case OperationalPattern::OP_ATOM:
            op_symbol = "OP-Atom";
            break;
        case OperationalPattern::OP_1A:
            op_symbol = "OP-1A";
            break;
        default:
            // empty string
            Logging::warning("Unknown operational pattern %d\n", op);
            break;
    }
    WriteAttribute(local_name, op_symbol);
}

