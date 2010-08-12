/*
 * $Id: Preface.cpp,v 1.2 2010/08/12 16:25:39 john_f Exp $
 *
 * 
 *
 * Copyright (C) 2008  Philip de Nier <philipn@users.sourceforge.net>
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

#include <libMXF++/MXF.h>


using namespace std;
using namespace mxfpp;



Preface::Preface(HeaderMetadata* headerMetadata)
: PrefaceBase(headerMetadata)
{}

Preface::Preface(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet)
: PrefaceBase(headerMetadata, cMetadataSet)
{}

Preface::~Preface()
{}

GenericPackage* Preface::findPackage(mxfUMID package_uid) const
{
    vector<GenericPackage*> packages = getContentStorage()->getPackages();
    mxfUMID uid;
    size_t i;
    for (i = 0; i < packages.size(); i++) {
        uid = packages[i]->getPackageUID();
        if (mxf_equals_umid(&package_uid, &uid))
            return packages[i];
    }

    return 0;
}

MaterialPackage* Preface::findMaterialPackage() const
{
    vector<GenericPackage*> packages = getContentStorage()->getPackages();
    MaterialPackage *material_package;
    size_t i;
    for (i = 0; i < packages.size(); i++) {
        material_package = dynamic_cast<MaterialPackage*>(packages[i]);
        if (material_package)
            return material_package;
    }

    return 0;
}

vector<SourcePackage*> Preface::findFileSourcePackages() const
{
    vector<GenericPackage*> packages = getContentStorage()->getPackages();
    vector<SourcePackage*> file_packages;
    SourcePackage *file_package;
    size_t i;
    for (i = 0; i < packages.size(); i++) {
        file_package = dynamic_cast<SourcePackage*>(packages[i]);
        if (!file_package ||
            !file_package->haveDescriptor() || !dynamic_cast<FileDescriptor*>(file_package->getDescriptor()))
        {
            continue;
        }

        file_packages.push_back(file_package);
    }

    return file_packages;
}

