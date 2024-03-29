/*
 * $Id: DatabaseEnums.h,v 1.9 2010/06/02 13:04:26 john_f Exp $
 *
 * Defines enumerated data values matching those in the database
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
 
#ifndef __PRODAUTO_DATABASEENUMS_H__
#define __PRODAUTO_DATABASEENUMS_H__


// These must match the database values!

#define PICTURE_DATA_DEFINITION             1
#define PICTURE_DATA_DEFINITION_NAME        "Video"
#define SOUND_DATA_DEFINITION               2
#define SOUND_DATA_DEFINITION_NAME          "Audio"

#define FILE_ESSENCE_DESC_TYPE              1
#define TAPE_ESSENCE_DESC_TYPE              2
#define LIVE_ESSENCE_DESC_TYPE              3


// Debatable whether source config should have a type.
#define TAPE_SOURCE_CONFIG_TYPE             1
#define LIVE_SOURCE_CONFIG_TYPE             2

#define UNSPECIFIED_TAKE_RESULT             1
#define GOOD_TAKE_RESULT                    2
#define NOT_GOOD_TAKE_RESULT                3

#define RECORDER_PARAM_TYPE_ANY             1

#define LTC_PARAMETER_VALUE                 1
#define VITC_PARAMETER_VALUE                2
#define TIME_PARAMETER_VALUE                3


#define PROXY_STATUS_INCOMPLETE             1
#define PROXY_STATUS_COMPLETE               2
#define PROXY_STATUS_FAILED                 3

#define TRANSCODE_STATUS_NOTSTARTED         1
#define TRANSCODE_STATUS_STARTED            2
#define TRANSCODE_STATUS_COMPLETED          3
#define TRANSCODE_STATUS_FAILED             4
#define TRANSCODE_STATUS_NOTFORTRANSCODE    5
#define TRANSCODE_STATUS_NOTSUPPORTED       6

#define USER_COMMENT_WHITE_COLOUR           1
#define USER_COMMENT_RED_COLOUR             2
#define USER_COMMENT_YELLOW_COLOUR          3
#define USER_COMMENT_GREEN_COLOUR           4
#define USER_COMMENT_CYAN_COLOUR            5
#define USER_COMMENT_BLUE_COLOUR            6
#define USER_COMMENT_MAGENTA_COLOUR         7
#define USER_COMMENT_BLACK_COLOUR           8

#include <string>
#include <map>

/**
Singleton class to provide names corresponding to the various codes.
*/
class DatabaseEnums
{
public:
// methods
    static DatabaseEnums * Instance()
    {
        if (mInstance == 0)  // is it the first call?
        {  
            mInstance = new DatabaseEnums; // create sole instance
        }
        return mInstance;// address of sole instance
    }
    std::string ResolutionName(int resolution);
    std::string FileFormatName(int file_format);
    std::string DataDefName(int data_def);
    std::string TimecodeName(int tc);
protected:
    // Protect constructors to force use of Instance()
    DatabaseEnums();
    DatabaseEnums(const DatabaseEnums &);
    DatabaseEnums & operator= (const DatabaseEnums &);
private:
    // static instance pointer
    static DatabaseEnums * mInstance;
    // local copies of name tables
    std::map<int, std::string> mResolutionNames;
    std::map<int, std::string> mFileFormatNames;
    std::map<int, std::string> mTimecodeNames;
};

#endif

