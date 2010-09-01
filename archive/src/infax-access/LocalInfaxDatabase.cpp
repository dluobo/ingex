/*
 * $Id: LocalInfaxDatabase.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * Provides access to the local dump of the Infax database
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
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

#include "LocalInfaxDatabase.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"

using namespace std;
using namespace rec;
using namespace pqxx;



static const char* g_loadSourceStmt = "loadsource";
static const char* g_loadSourceSQL = 
" \
    SELECT \
        src_identifier, \
        src_type_id, \
        src_barcode \
    FROM Source \
    WHERE \
        src_barcode = $1 \
";        
        
static const char* g_loadSourceItemsStmt = "loadsourceitems";
static const char* g_loadSourceItemsSQL = 
" \
    SELECT \
        sit_identifier, \
        sit_format, \
        sit_prog_title, \
        sit_episode_title, \
        sit_tx_date, \
        sit_mag_prefix, \
        sit_prog_no, \
        sit_prod_code, \
        sit_spool_status, \
        sit_stock_date, \
        sit_spool_descr, \
        sit_memo, \
        sit_duration, \
        sit_spool_no, \
        sit_acc_no, \
        sit_cat_detail, \
        sit_item_no, \
        sit_aspect_ratio_code \
    FROM SourceItem \
    WHERE \
        sit_source_id = $1 \
    ORDER BY \
        sit_item_no ASC \
";       





LocalInfaxDatabase::LocalInfaxDatabase(string host, string name, string user, string password)
: PostgresDatabase(host, name, user, password)
{
    try
    {
        prepareStatements();
    }
    catch (const exception& ex)
    {
        REC_LOGTHROW(("Failed to open a connection to the recorder database: "
                      "host='%s', name='%s', user='%s':\n%s",
                      host.c_str(), name.c_str(), user.c_str(),
                      ex.what()));
    }
    catch (const RecorderException& ex)
    {
        REC_LOGTHROW(("Failed to open a connection to the recorder database: "
                      "host='%s', name='%s', user='%s':\n%s",
                      host.c_str(), name.c_str(), user.c_str(),
                      ex.getMessage().c_str()));
    }
    catch (...)
    {
        REC_LOGTHROW(("Failed to open a connection to the recorder database: "
                      "host='%s', name='%s', user='%s':\nunknown database exception",
                      host.c_str(), name.c_str(), user.c_str()));
    }
}

LocalInfaxDatabase::~LocalInfaxDatabase()
{
}

void LocalInfaxDatabase::prepareStatements()
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);

        _connection->prepare(g_loadSourceStmt, g_loadSourceSQL)
            ("varchar", prepare::treat_string);
        
        _connection->prepare(g_loadSourceItemsStmt, g_loadSourceItemsSQL)
            ("integer", prepare::treat_direct);
    }
    END_WORK("Prepare database statements")
}

bool LocalInfaxDatabase::haveConnection()
{
    return PostgresDatabase::haveConnection();
}

Source* LocalInfaxDatabase::loadSource(string barcode)
{
    START_WORK
    {
        LOCK_SECTION(_accessMutex);

        // read the Source
        
        work ts(*_connection, "load source");
        result res = ts.prepared(g_loadSourceStmt)(barcode).exec();
        
        if (res.size() == 0)
        {
            Logging::warning("Failed to load source with barcode '%s'\n", barcode.c_str());
            return 0;
        }
        
        if (readLong(res[0][1], -1) != VIDEO_TAPE_SOURCE_TYPE)
        {
            Logging::warning("Unknown source type %d\n", readLong(res[0][1], -1));
            return 0;
        }
        
        auto_ptr<Source> retSource(new Source());
        retSource->databaseId = readLong(res[0][0], -1);
        // res[0][1] "src_type_id" not yet used because the type is assumed to be VIDEO_TAPE_SOURCE_TYPE
        retSource->barcode = readString(res[0][2]);
        
        // read the source items
        retSource->concreteSources = loadSourceItems(ts, retSource->databaseId);
        
        return retSource.release();
    }
    END_WORK("Load source")
}

vector<ConcreteSource*> LocalInfaxDatabase::loadSourceItems(pqxx::work &ts, long databaseId)
{
    START_WORK
    {
        result res = ts.prepared(g_loadSourceItemsStmt)(databaseId).exec();
        
        if (res.size() == 0)
            REC_LOGTHROW(("Failed to load the source items associated with source %ld\n", databaseId));

        AutoPointerVector<ConcreteSource> sourceItems;
        SourceItem *sourceItem;
        
        result::size_type i;
        for (i = 0; i < res.size(); i++)
        {
            sourceItems.get().push_back(new SourceItem());
            sourceItem = dynamic_cast<SourceItem*>(sourceItems.get().back());

            sourceItem->databaseId = readLong(res[i][0], -1);
            sourceItem->format = readString(res[i][1]);
            sourceItem->progTitle = readString(res[i][2]);
            sourceItem->episodeTitle = readString(res[i][3]);
            sourceItem->txDate = readDate(res[i][4], g_nullDate);
            sourceItem->magPrefix = readString(res[i][5]);
            sourceItem->progNo = readString(res[i][6]);
            sourceItem->prodCode = readString(res[i][7]);
            sourceItem->spoolStatus = readString(res[i][8]);
            sourceItem->stockDate = readDate(res[i][9], g_nullDate);
            sourceItem->spoolDescr = readString(res[i][10]);
            sourceItem->memo = readString(res[i][11]);
            sourceItem->duration = readInt64(res[i][12], -1);
            sourceItem->spoolNo = readString(res[i][13]);
            sourceItem->accNo = readString(res[i][14]);
            sourceItem->catDetail = readString(res[i][15]);
            sourceItem->itemNo = readInt(res[i][16], 0);
            sourceItem->aspectRatioCode = readString(res[i][17]);
            // sourceItem->modifiedFlag left at default false
        }
        
        return sourceItems.release();
    }
    END_WORK("Load source items")
}

