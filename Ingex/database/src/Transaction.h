/*
 * $Id: Transaction.h,v 1.1 2006/12/19 16:44:53 john_f Exp $
 *
 * A database transaction
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
 
#ifndef __PRODAUTO_TRANSACTION_H__
#define __PRODAUTO_TRANSACTION_H__


#include "Connection.h"


namespace prodauto
{

class Database;


class Transaction : public Connection
{
public:
    friend class Database;
    
public:
    virtual ~Transaction();
    
    virtual void commit();
    virtual void rollback();
    
    void commitTransaction();

protected:
    Transaction(Database* database, odbc::Connection* odbcConnection);

};



};









#endif

