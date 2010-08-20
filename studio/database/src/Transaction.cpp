/*
 * $Id: Transaction.cpp,v 1.4 2010/08/20 16:12:51 john_f Exp $
 *
 * A database transaction
 *
 * Copyright (C) 2009 British Broadcasting Corporation
 * All Rights Reserved
 *
 * Author: Philip de Nier
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
 
 
#include "Transaction.h"
#include "Database.h"
#include "DBException.h"
#include "Logging.h"

using namespace std;
using namespace prodauto;
using namespace pqxx;



ConnectionReturner::ConnectionReturner(Database *database, pqxx::connection *conn)
{
    _database = database;
    _conn = conn;
}

ConnectionReturner::~ConnectionReturner()
{
    // The destructor order is: pqxx::work -> ConnectionReturner -> namedclass
    //    (namedclass is last because it is declared a virtual base class)
    // and therefore the connection is returned here to the Database connection pool after the
    // pqxx::work superclass of Transaction has destructed
    _database->returnConnection(this);
}



Transaction::Transaction(Database *database, connection *conn, string name)
: namedclass("Transaction"), ConnectionReturner(database, conn), work(*conn, name)
{
}

Transaction::~Transaction() throw()
{
    // work::abort() will be called in work destructor if transaction was started but not committed
}

void Transaction::commit()
{
    try
    {
        work::commit();
    }
    catch (...)
    {
        _commitListeners.clear();
        throw;
    }

    // report successful commit to objects in transaction
    vector<pair<long, DatabaseObject*> >::const_iterator iter;
    for (iter = _commitListeners.begin(); iter != _commitListeners.end(); iter++)
        iter->second->wasCommitted(iter->first);

    _commitListeners.clear();
}

void Transaction::abort()
{
    work::abort();
}

void Transaction::registerCommitListener(long tentative_database_id, DatabaseObject *obj)
{
    _commitListeners.push_back(make_pair(tentative_database_id, obj));
}

long Transaction::tentativeDatabaseID(DatabaseObject *obj)
{
    vector<pair<long, DatabaseObject*> >::const_iterator iter;
    for (iter = _commitListeners.begin(); iter != _commitListeners.end(); iter++) {
        if (iter->second == obj)
            return iter->first;
    }
    
    PA_LOGTHROW(DBException, ("No tentative database ID available for unregistered commit listener object"));
}

