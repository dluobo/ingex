/*
 * $Id: MXFCommon.h,v 1.1 2010/09/01 16:05:22 philipn Exp $
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

#ifndef __RECORDER_MXF_COMMON_H__
#define __RECORDER_MXF_COMMON_H__


#include <mxf/mxf.h>

namespace mxfpp
{
    class DataModel;
};


// declare the BBC Archive MXF extensions

#define MXF_LABEL(d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13, d14, d15) \
    {d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13, d14, d15}

#define MXF_SET_DEFINITION(parentName, name, label) \
    static const mxfUL MXF_SET_K(name) = label;
    
#define MXF_ITEM_DEFINITION(setName, name, label, localTag, typeId, isRequired) \
    static const mxfUL MXF_ITEM_K(setName, name) = label;

#include <bbc_archive_extensions_data_model.h>


// BBC Archive descriptive scheme label

static const mxfUL MXF_DM_L(APP_PreservationDescriptiveScheme) =
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01, 0x0D, 0x04, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00};


// MXF Identification property values
    
static const char * const MXF_IDENT_COMPANY_NAME   = "BBC";
static const char * const MXF_IDENT_PRODUCT_NAME   = "Ingex Archive";
static const char * const MXF_IDENT_VERSION_STRING = "1.1";
static const mxfUUID MXF_IDENT_PRODUCT_UID  =
    {0x75, 0x2e, 0xf1, 0x5e, 0xde, 0x4b, 0x46, 0xe2, 0xb1, 0x11, 0x6e, 0x5a, 0x74, 0x7a, 0xc8, 0x41};



namespace rec
{

void redirect_mxf_logging();

void register_archive_extensions(mxfpp::DataModel *data_model);


};


#endif

