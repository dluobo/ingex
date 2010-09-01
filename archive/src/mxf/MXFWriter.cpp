/*
 * $Id: MXFWriter.cpp,v 1.1 2010/09/01 16:05:22 philipn Exp $
 *
 * MXF file writer
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

#include "MXFWriter.h"

#include <write_archive_mxf.h>

using namespace std;
using namespace rec;



bool MXFWriter::parseInfaxData(string infax_string, InfaxData *infax_data)
{
    return parse_infax_data(infax_string.c_str(), infax_data, true);
}

uint32_t MXFWriter::calcCRC32(const unsigned char *data, uint32_t size)
{
    return calc_crc32(data, size);
}

