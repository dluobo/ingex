/*
 * $Id: SourceSession.h,v 1.1 2006/12/19 16:44:53 john_f Exp $
 *
 * A set of Packages representing live recording / tape sources during a time period
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
 
#ifndef __PRODAUTO_SOURCESESSION_H__
#define __PRODAUTO_SOURCESESSION_H__


#include <string>
#include <vector>


#include "Recorder.h"
#include "Package.h"
#include "DataTypes.h"


namespace prodauto
{


class SourceSession
{
public:
    ~SourceSession();

    static SourceSession* create(Recorder* recorder, Date date);
    static SourceSession* load(Recorder* recorder, Date date);
    
    static bool isInstancePackage(SourceConfig* sourceConfig, SourcePackage* sourcePackage);
    
    
    SourcePackage* getSourcePackage(std::string sourceName);
    
    void deleteFromDatabase();
    
    std::vector<SourcePackage*> sourcePackages;
    
protected:
    SourceSession(Date date);
    
private:
    static std::string getSourcePackageName(std::string sourceName, Date date);

    Date _date;
};



};



#endif

