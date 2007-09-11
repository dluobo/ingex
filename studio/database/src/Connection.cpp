/*
 * $Id: Connection.cpp,v 1.1 2007/09/11 14:08:38 stuart_hc Exp $
 *
 * Wraps a ODBC connection to the database
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
 
#include <cassert>

#include "Connection.h"
#include "Database.h"
#include "DBException.h"
#include "Logging.h"


using namespace std;
using namespace prodauto;


Connection::Connection(Database* database, odbc::Connection* odbcConnection)
: _database(database), _odbcConnection(odbcConnection)
{
    // Note: if the odbConnection had autoCommit set to false, then a rollback
    // will have been done before returning it to the pool and that means 
    // no transaction is in progress at this point and we can safely set 
    // the connection behaviour 
    
    // set the connection to the default behaviour
    odbcConnection->setAutoCommit(false);
    odbcConnection->setTransactionIsolation(odbc::Connection::TRANSACTION_READ_COMMITTED);
 }

Connection::~Connection()
{
    _database->returnConnection(this);
}
    
void Connection::commit()
{
    try
    {
        _odbcConnection->commit();
    }
    catch (...)
    {
        _commitListeners.clear();
        throw;
    }

    // report commit success to objects in transaction
    vector<pair<long, DatabaseObject*> >::const_iterator iter;
    for (iter = _commitListeners.begin(); iter != _commitListeners.end(); iter++)
    {
        (*iter).second->wasCommitted((*iter).first);
    }

    _commitListeners.clear();
}

void Connection::rollback()
{
    try
    {
        _odbcConnection->rollback();
    }
    catch (...)
    {
        _commitListeners.clear();
        throw;
    }

    // objects are not notified of a commit success
    _commitListeners.clear();
}

void Connection::registerCommitListener(long tentativeDatabaseID, DatabaseObject* obj)
{
    _commitListeners.push_back(pair<long, DatabaseObject*>(tentativeDatabaseID, obj));
}

long Connection::getTentativeDatabaseID(DatabaseObject* obj)
{
    vector<pair<long, DatabaseObject*> >::const_iterator iter;
    for (iter = _commitListeners.begin(); iter != _commitListeners.end(); iter++)
    {
        if ((*iter).second == obj)
        {
            return (*iter).first;
        }
    }
    
    PA_LOGTHROW(DBException, ("No tentative database ID available for unregistered object"));
}

