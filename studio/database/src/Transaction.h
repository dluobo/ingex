/*
 * $Id: Transaction.h,v 1.4 2010/09/28 08:11:20 john_f Exp $
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
 
#ifndef __PRODAUTO_TRANSACTION_H__
#define __PRODAUTO_TRANSACTION_H__


#include <string>
#include <vector>

#include <pqxx/pqxx>

#include "DatabaseObject.h"


namespace prodauto
{

class Database;


class ConnectionReturner
{
public:
    friend class Database;
    
public:
    ConnectionReturner(Database *database, pqxx::connection *conn);
    virtual ~ConnectionReturner();

protected:
    Database *_database;
    pqxx::connection *_conn;
};


class Transaction : public ConnectionReturner, public pqxx::work
{
/* ConnectionReturner is the leftmost base class and so is destructed after pqxx::work */
public:
    Transaction(Database *database, pqxx::connection *conn, std::string name);
    virtual ~Transaction() throw();
    
    void commit(); // overrides pqxx::work::commit()
    void abort(); // overrides pqxx::work::abort()
    
    void registerCommitListener(long tentative_database_id, DatabaseObject *obj);
    long tentativeDatabaseID(DatabaseObject *obj);
    
private:
    std::vector<std::pair<long, DatabaseObject*> > _commitListeners;
};



};


#endif

