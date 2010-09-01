/*
 * $Id: DatabaseThings.cpp,v 1.2 2010/09/01 16:05:22 philipn Exp $
 *
 * Database enums and classes matching the things stored in the database
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
 
/*
    Note: The SourceItem::getSourceProperties() and 
    SourceItem::parseSourceProperties() methods (and related parse_... functions)
    were originally used when updating the source info using the web browser client.
    This functionality is no longer available in the web browser client.
*/


#include <set>

#include "DatabaseThings.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"

using namespace std;
using namespace rec;


const char* g_stringPropertyType = "string";
const char* g_datePropertyType = "date";
const char* g_lengthPropertyType = "length";
const char* g_uint32PropertyType = "uint32";


bool parse_string(AssetPropertySource* assetSource, int id, string name, bool checkNotEmpty, int maxSize, 
    string* outValue, string* errMessage)
{
    if (!assetSource->haveValue(id, name))
    {
        return true;
    }
    
    string inValue = assetSource->getValue(id, name);
    
    if ((int)inValue.size() > maxSize)
    {
        *errMessage = "String field '";
        *errMessage += name + "' (" + int_to_string(id) + ") size " + 
            int_to_string((int)inValue.size()) + " exceeds maximum " +
            int_to_string(maxSize);
        return false;
    }
    if (checkNotEmpty && inValue.size() == 0)
    {
        *errMessage = "String field '";
        *errMessage += name + "' (" + int_to_string(id) + ") has zero size";
        return false;
    }
    
    *outValue = inValue;
    
    return true;
}

bool parse_date(AssetPropertySource* assetSource, int id, string name,
    Date* outValue, string* errMessage)
{
    if (!assetSource->haveValue(id, name))
    {
        return true;
    }
    
    string inValue = assetSource->getValue(id, name);
    
    int year = 0;
    int month = 0;
    int day = 0;
    if ((sscanf(inValue.c_str(), "%d-%d-%d", &year, &month, &day) != 3) ||
        year < 0 ||
        month < 0 || month > 12 ||
        day < 0 || day > 31)
    {
        *errMessage = "Date field '";
        *errMessage += name + "' (" + int_to_string(id) + ") value is invalid (yyyy-mm-dd)";
        return false;
    }
    
    outValue->year = year;
    outValue->month = month;
    outValue->day = day;
    
    return true;
}

bool parse_duration(AssetPropertySource* assetSource, int id, string name, 
    int64_t* outValue, string* errMessage)
{
    if (!assetSource->haveValue(id, name))
    {
        return true;
    }
    
    string inValue = assetSource->getValue(id, name);
    
    int hour = 0;
    int min = 0;
    int sec = 0;
    int frame = 0;
    if ((sscanf(inValue.c_str(), "%d:%d:%d:%d", &hour, &min, &sec, &frame) != 4) ||
        hour < 0 ||
        min < 0 || min > 59 ||
        sec < 0 || sec > 59 || 
        frame < 0 || frame > 25)
    {
        *errMessage = "Duration field '";
        *errMessage += name + "' (" + int_to_string(id) + ") value is invalid (hh:mm:ss:ff)";
        return false;
    }
    
    *outValue = ((int64_t)hour) * 60 * 60 * 25 +
        ((int64_t)min) * 60 * 25 + 
        ((int64_t)sec) * 25 + 
        ((int64_t)frame);
        
    return true;        
}

bool parse_spool_number(AssetPropertySource* assetSource, int id, string name,
    string* outValue, string* errMessage)
{
    if (!assetSource->haveValue(id, name))
    {
        *errMessage = "Spool number field '";
        *errMessage += name + "' (" + int_to_string(id) + ") value has zero size";
        return false;
    }
    
    string inValue = assetSource->getValue(id, name);
    
    // spool number must be 3 letters (padded to right with spaces) followed by
    // 6 digits
    int i;
    // 3 letters
    for (i = 0; i < 3; i++)
    {
        if (inValue.at(i) == ' ')
        {
            // check space padding
            break;
        }
        else if (inValue.at(i) < 'A' || inValue.at(i) > 'Z')
        {
            *errMessage = "Spool number field '";
            *errMessage += name + "' (" + int_to_string(id) + ") is invalid - "
                "non-letter (capital) character found";
            return false;
        }
    }
    // space padding
    for (; i < 3; i++)
    {
        if (inValue.at(i) != ' ')
        {
            *errMessage = "Spool number field '";
            *errMessage += name + "' (" + int_to_string(id) + ") is invalid - "
                "space padding broken";
            return false;
        }
    }
    // 6 digits
    for (; i < 9; i++)
    {
        if (inValue.at(i) < '0' || inValue.at(i) > '9')
        {
            *errMessage = "Spool number field '";
            *errMessage += name + "' (" + int_to_string(id) + ") is invalid - "
                "non-digit character found";
            return false;
        }
    }
    
    *outValue = inValue;
    
    return true;
}

bool parse_uint32(AssetPropertySource* assetSource, int id, string name, uint32_t* outValue, string* errMessage)
{
    if (!assetSource->haveValue(id, name))
    {
        return true;
    }
    
    string inValue = assetSource->getValue(id, name);
    
    uint32_t value = 0;
    if (sscanf(inValue.c_str(), "%u", &value) != 1 ||
        value > 0xffff)
    {
        *errMessage = "UInt16 field '";
        *errMessage += name + "' (" + int_to_string(id) + ") value is invalid";
        return false;
    }
    
    *outValue = value;
    
    return true;
}



static string get_duration_string(int64_t value)
{
    char buf[50];
    int hour;
    int min;
    int sec;
    int frame;
    
    if (value >= 0)
    {
        hour = (int)(value / (60 * 60 * 25));
        min = (int)((value % (60 * 60 * 25)) / (60 * 25));
        sec = (int)(((value % (60 * 60 * 25)) % (60 * 25)) / 25);
        frame = (int)(((value % (60 * 60 * 25)) % (60 * 25)) % 25);
    }
    else
    {
        hour = 0;
        min = 0;
        sec = 0;
        frame = 0;
    }
    
    sprintf(buf, "%02d:%02d:%02d:%02d", hour, min, sec, frame);
    
    return buf;
}



string rec::ingest_format_to_string(IngestFormat format, bool unknownIsDisabled)
{
    switch (format)
    {
        case MXF_UNC_8BIT_INGEST_FORMAT:
            return "MXF Unc. 8-bit";
        case MXF_UNC_10BIT_INGEST_FORMAT:
            return "MXF Unc. 10-bit";
        case MXF_D10_50_INGEST_FORMAT:
            return "MXF D-10 50Mbps";
        case UNKNOWN_INGEST_FORMAT:
        default:
            if (unknownIsDisabled)
            {
                return "Disabled";
            }
            else
            {
                return "";
            }
    }
}



AssetProperty::AssetProperty()
{}

AssetProperty::AssetProperty(int id_, string name_, string title_, string value_, std::string type_, 
    int maxSize_, bool editable_)
: id(id_), name(name_), title(title_), value(value_), type(type_), maxSize(maxSize_), editable(editable_) 
{}

AssetProperty::AssetProperty(const AssetProperty& a)
: id(a.id), name(a.name), title(a.title), value(a.value), type(a.type), maxSize(a.maxSize), editable(a.editable)
{}

AssetProperty::~AssetProperty()
{}



DatabaseTable::DatabaseTable()
: databaseId(-1)
{
}

DatabaseTable::~DatabaseTable()
{
}


Source::Source()
: DatabaseTable(), recInstance(-1)
{
}

Source::~Source()
{
    vector<ConcreteSource*>::const_iterator iter;
    for (iter = concreteSources.begin(); iter != concreteSources.end(); iter++)
    {
        delete *iter;
    }
}

SourceItem* Source::getSourceItem(uint32_t itemNo)
{
    vector<ConcreteSource*>::const_iterator iter;
    for (iter = concreteSources.begin(); iter != concreteSources.end(); iter++)
    {
        SourceItem* sourceItem = dynamic_cast<SourceItem*>(*iter);
        
        if (sourceItem != 0 && sourceItem->itemNo == itemNo)
        {
            return sourceItem;
        }
    }
    
    return 0;
}

SourceItem* Source::getFirstSourceItem()
{
    if (concreteSources.empty())
    {
        return 0;
    }
    
    return dynamic_cast<SourceItem*>(concreteSources.front());
}

Source* Source::clone()
{
    Source *clonedSource = new Source();
    
    clonedSource->barcode = barcode;
    clonedSource->recInstance = recInstance;

    size_t i;
    for (i = 0; i < concreteSources.size(); i++)
    {
        SourceItem *sourceItem = dynamic_cast<SourceItem*>(concreteSources[i]);
        REC_ASSERT(sourceItem);
        
        SourceItem *clonedSourceItem = new SourceItem();
        *clonedSourceItem = *sourceItem;
        clonedSource->concreteSources.push_back(clonedSourceItem);
    }
    
    return clonedSource;
}

Destination::Destination()
: DatabaseTable(), concreteDestination(0), sourceId(-1), sourceItemId(-1), ingestItemNo(-1)
{
}

Destination::~Destination()
{
    delete concreteDestination;
}


SourceItem::SourceItem()
: ConcreteSource(), duration(-1), itemNo(0), modifiedFlag(false)
{
}

SourceItem::~SourceItem()
{
}

vector<AssetProperty> SourceItem::getSourceProperties()
{
    vector<AssetProperty> result;
    
    result.push_back(AssetProperty(1, "format", "Format", format, g_stringPropertyType, 6, false));
    result.push_back(AssetProperty(2, "progtitle", "Programme Title", progTitle, g_stringPropertyType, 72, false));
    result.push_back(AssetProperty(3, "episodetitle", "Episode Title", episodeTitle, g_stringPropertyType, 144, false));
    result.push_back(AssetProperty(4, "txdate", "Tx Date", txDate.toString(), g_datePropertyType, 0, false));
    result.push_back(AssetProperty(5, "magprefix", "Magazine Prefix", magPrefix, g_stringPropertyType, 1, true));
    result.push_back(AssetProperty(6, "progno", "Programme No.", progNo, g_stringPropertyType, 8, true));
    result.push_back(AssetProperty(7, "prodcode", "Production Code", prodCode, g_stringPropertyType, 2, true));
    result.push_back(AssetProperty(8, "spoolstatus", "Spool Status", spoolStatus, g_stringPropertyType, 1, true));
    result.push_back(AssetProperty(9, "stockdate", "Stock Date", stockDate.toString(), g_datePropertyType, 0, true));
    result.push_back(AssetProperty(10, "spooldescr", "Spool Description", spoolDescr, g_stringPropertyType, 29, true));
    result.push_back(AssetProperty(11, "duration", "Duration", get_duration_string(duration), g_lengthPropertyType, 0, true));
    result.push_back(AssetProperty(12, "spoolno", "Spool No.", spoolNo, g_stringPropertyType, 14, false));
    result.push_back(AssetProperty(13, "accno", "Accession No.", accNo, g_stringPropertyType, 14, true));
    result.push_back(AssetProperty(14, "catdetail", "Catalogue Detail", catDetail, g_stringPropertyType, 10, true));
    result.push_back(AssetProperty(15, "memo", "Memo", memo, g_stringPropertyType, 120, true));
    result.push_back(AssetProperty(16, "itemno", "Item No.", int64_to_string(itemNo), g_uint32PropertyType, 0, true));
    result.push_back(AssetProperty(17, "aspectratiocode", "Aspect Ratio Code", aspectRatioCode, g_stringPropertyType, 8, true));
    
    return result;
}

bool SourceItem::parseSourceProperties(AssetPropertySource* assetSource, string* errMessage)
{
    // ignore format, progTitle, episodeTitle, txDate, spoolNo which are not editable
    
    string in_magPrefix;
    string in_progNo;
    string in_prodCode;
    string in_spoolStatus;
    Date in_stockDate;
    string in_spoolDescr;
    string in_memo;
    int64_t in_duration;
    string in_accNo;
    string in_catDetail;
    uint32_t in_itemNo;
    string in_aspectRatioCode;
    
    // parse editable values if present
    
    if (!parse_string(assetSource, 5, "magprefix", false, 1, &in_magPrefix, errMessage))
    {
        return false;
    }
    if (!parse_string(assetSource, 6, "progno", false, 8, &in_progNo, errMessage))
    {
        return false;
    }
    if (!parse_string(assetSource, 7, "prodcode", false, 2, &in_prodCode, errMessage))
    {
        return false;
    }
    if (!parse_string(assetSource, 8, "spoolstatus", false, 1, &in_spoolStatus, errMessage))
    {
        return false;
    }
    if (!parse_date(assetSource, 9, "stockdate", &in_stockDate, errMessage))
    {
        return false;
    }
    if (!parse_string(assetSource, 10, "spooldescr", false, 29, &in_spoolDescr, errMessage))
    {
        return false;
    }
    if (!parse_duration(assetSource, 11, "duration", &in_duration, errMessage))
    {
        return false;
    }
    if (!parse_string(assetSource, 13, "accno", false, 14, &in_accNo, errMessage))
    {
        return false;
    }
    if (!parse_string(assetSource, 14, "catdetail", false, 10, &in_catDetail, errMessage))
    {
        return false;
    }
    if (!parse_string(assetSource, 15, "memo", false, 120, &in_memo, errMessage))
    {
        return false;
    }
    if (!parse_uint32(assetSource, 16, "itemno", &in_itemNo, errMessage))
    {
        return false;
    }
    if (!parse_string(assetSource, 17, "aspectratiocode", false, 8, &in_aspectRatioCode, errMessage))
    {
        return false;
    }

    if (in_magPrefix != magPrefix ||
        in_progNo != progNo ||
        in_prodCode != prodCode ||
        in_spoolStatus != spoolStatus ||
        in_stockDate != stockDate ||
        in_spoolDescr != spoolDescr ||
        in_duration != duration ||
        in_accNo != accNo ||
        in_catDetail != catDetail ||
        in_memo != memo ||
        in_itemNo != itemNo ||
        in_aspectRatioCode != aspectRatioCode)
    {
        modifiedFlag = true;

        progNo = in_progNo;
        prodCode = in_prodCode;
        spoolStatus = in_spoolStatus;
        stockDate = in_stockDate;
        spoolDescr = in_spoolDescr;
        duration = in_duration;
        accNo = in_accNo;
        catDetail = in_catDetail;
        memo = in_memo;
        itemNo = in_itemNo;
        aspectRatioCode = in_aspectRatioCode;
    }
    
    return true;
}


HardDiskDestination::HardDiskDestination()
: ConcreteDestination(), ingestFormat(UNKNOWN_INGEST_FORMAT), pseResult(0), size(-1), duration(-1),
browseSize(-1), cacheId(-1)
{
}

HardDiskDestination::~HardDiskDestination()
{
}

vector<AssetProperty> HardDiskDestination::getDestinationProperties()
{
    // TODO
    return vector<AssetProperty>();
}

DigibetaDestination::DigibetaDestination()
: ConcreteDestination()
{
}

DigibetaDestination::~DigibetaDestination()
{
}

vector<AssetProperty> DigibetaDestination::getDestinationProperties()
{
    // TODO
    return vector<AssetProperty>();
}


RecorderTable::RecorderTable()
: DatabaseTable()
{}

RecorderTable::~RecorderTable()
{}


RecordingSessionSourceTable::RecordingSessionSourceTable()
: DatabaseTable(), source(0)
{
    // source not owned
}

RecordingSessionSourceTable::~RecordingSessionSourceTable()
{
}


RecordingSessionDestinationTable::RecordingSessionDestinationTable()
: DatabaseTable(), destination(0)
{
}

RecordingSessionDestinationTable::~RecordingSessionDestinationTable()
{
    // destination not owned
}


RecordingSessionTable::RecordingSessionTable()
: DatabaseTable(), recorder(0), status(0), abortInitiator(0), totalVTRErrors(0), totalDigiBetaDropouts(0),
source(0)
{
}

RecordingSessionTable::~RecordingSessionTable()
{
    delete source;
    
    vector<RecordingSessionDestinationTable*>::const_iterator iter2;
    for (iter2 = destinations.begin(); iter2 != destinations.end(); iter2++)
    {
        delete *iter2;
    }
    
    // recorder is not owned
}



CacheTable::CacheTable()
: DatabaseTable(), recorderId(-1)
{}

CacheTable::~CacheTable()
{}


CacheItem::CacheItem()
: ingestFormat(UNKNOWN_INGEST_FORMAT), hdDestinationId(-1), pseResult(0), size(-1), duration(-1),
sessionId(-1), sourceItemNo(0)
{
}

CacheItem::~CacheItem()
{
}


InfaxExportTable::InfaxExportTable()
: DatabaseTable(), sourceItemNo(0)
{
}
InfaxExportTable::~InfaxExportTable()
{
}


LTOFileTable::LTOFileTable()
: DatabaseTable(), status(-1), size(-1), realDuration(-1), duration(-1), itemNo(0),
recordingSessionId(-1), ingestFormat(UNKNOWN_INGEST_FORMAT), sourceItemNo(0)
{
}

LTOFileTable::~LTOFileTable()
{
}


LTOTable::LTOTable()
: DatabaseTable()
{
}

LTOTable::~LTOTable()
{
    vector<LTOFileTable*>::const_iterator iter;
    for (iter = ltoFiles.begin(); iter != ltoFiles.end(); iter++)
    {
        delete *iter;
    }
}


LTOTransferSessionTable::LTOTransferSessionTable()
: DatabaseTable(), recorderId(-1), status(-1), abortInitiator(-1), lto(0)
{
}

LTOTransferSessionTable::~LTOTransferSessionTable()
{
    delete lto;
}

