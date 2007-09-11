/*
 * $Id: DatabaseEnums.h,v 1.1 2007/09/11 14:08:38 stuart_hc Exp $
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


// these must match the database values

#define PICTURE_DATA_DEFINITION             1
#define SOUND_DATA_DEFINITION               2

#define UNSPECIFIED_FILE_FORMAT_TYPE        1
#define MXF_FILE_FORMAT_TYPE                2

#define FILE_ESSENCE_DESC_TYPE              1
#define TAPE_ESSENCE_DESC_TYPE              2
#define LIVE_ESSENCE_DESC_TYPE              3

#define UNC_MATERIAL_RESOLUTION             1
#define DV25_MATERIAL_RESOLUTION            2
#define DV50_MATERIAL_RESOLUTION            3
#define MJPEG21_MATERIAL_RESOLUTION         4
#define MJPEG31_MATERIAL_RESOLUTION         5
#define MJPEG101_MATERIAL_RESOLUTION        6
#define MJPEG151S_MATERIAL_RESOLUTION       7
#define MJPEG201_MATERIAL_RESOLUTION        8
#define MJPEG101M_MATERIAL_RESOLUTION       9
#define IMX30_MATERIAL_RESOLUTION           10
#define IMX40_MATERIAL_RESOLUTION           11
#define IMX50_MATERIAL_RESOLUTION           12

#define DVD_MATERIAL_RESOLUTION             20
#define MOV_MATERIAL_RESOLUTION             21

#define TAPE_SOURCE_CONFIG_TYPE             1 
#define LIVE_SOURCE_CONFIG_TYPE             2 

#define UNSPECIFIED_TAKE_RESULT             1
#define GOOD_TAKE_RESULT                    2
#define NOT_GOOD_TAKE_RESULT                3

#define RECORDER_PARAM_TYPE_ANY             1

#define PROXY_STATUS_INCOMPLETE             1
#define PROXY_STATUS_COMPLETE               2
#define PROXY_STATUS_FAILED                 3

#define TRANSCODE_STATUS_NOTSTARTED         1
#define TRANSCODE_STATUS_STARTED            2
#define TRANSCODE_STATUS_COMPLETED          3
#define TRANSCODE_STATUS_FAILED             4
#define TRANSCODE_STATUS_NOTFORTRANSCODE    5
#define TRANSCODE_STATUS_NOTSUPPORTED       6


#endif

