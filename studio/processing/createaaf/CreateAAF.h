/*
 * $Id: CreateAAF.h,v 1.2 2008/02/06 16:59:12 john_f Exp $
 *
 * AAF file for defining clips, multi-camera clips, etc
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
 
#ifndef __PRODAUTO_CREATEAAF_H__
#define __PRODAUTO_CREATEAAF_H__



#include <DataTypes.h>
#include <Package.h>
#include <MCClipDef.h>
#include <SourceConfig.h>
#include "CutsDatabase.h"


namespace prodauto
{

// putting AAF headers in here to avoid conflicts with standard sql typedefs (eg. TCHAR)
#include <AAF.h>
#include <AAFSmartPointer.h>
#include <AAFTypes.h>


class AAFFile
{
public:
    AAFFile(std::string filename);
    ~AAFFile();

    // must call this to save the data to the file
    void save();
    
    // add a single clip corresponding to the material package
    void addClip(MaterialPackage* materialPackage, PackageSet& packages);

    // add a multi-camera clip and cut sequence
    void addMCClip(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, PackageSet& packages,
        std::vector<CutInfo> sequence);
    
    
private:
    void setAppCode(IAAFMob* pMob, aafInt32 value);
    void setDIDInt32Property(IAAFClassDef* pCDDigitalImageDescriptor, 
        IAAFDigitalImageDescriptor2* pDigitalImageDescriptor2, aafUID_t propertyId, aafInt32 value);
    bool haveMob(aafMobID_t mobID);        
    void getReferencedPackages(Package* topPackage, PackageSet& packages, PackageSet& referencedPackages);
    bool getSource(Track* track, PackageSet& packages, SourcePackage** fileSourcePackage, 
        SourcePackage** sourcePackage, Track** sourceTrack);

    void createMCClip(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, PackageSet& packages,
        std::vector<CutInfo> sequence);
    
    void mapUserComments(Package* package, IAAFMob* mob);
    void mapMasterMob(MaterialPackage* materialPackage);
    void mapFileSourceMob(SourcePackage* sourcePackage);
    void mapTapeSourceMob(SourcePackage* sourcePackage);

    
    IAAFSmartPointer<IAAFFile> pFile;
    IAAFSmartPointer<IAAFHeader> pHeader;
    IAAFSmartPointer<IAAFDictionary> pDictionary;
    IAAFSmartPointer<IAAFCompositionMob> pCollectionCompositionMob;
    IAAFSmartPointer<IAAFCompositionMob> pClipCompositionMob;
    IAAFSmartPointer<IAAFCompositionMob> pSequenceCompositionMob;
    IAAFSmartPointer<IAAFSourceMob> pTapeSourceMob;
    IAAFSmartPointer<IAAFClassDef> pCDMob;
    IAAFSmartPointer<IAAFClassDef> pCDCompositionMob;
    IAAFSmartPointer<IAAFClassDef> pCDMasterMob;
    IAAFSmartPointer<IAAFClassDef> pCDSourceMob;
    IAAFSmartPointer<IAAFClassDef> pCDSourceClip;
    IAAFSmartPointer<IAAFClassDef> pCDSequence;
    IAAFSmartPointer<IAAFClassDef> pCDSelector;
    IAAFSmartPointer<IAAFClassDef> pCDFiller;
    IAAFSmartPointer<IAAFClassDef> pCDTapeDescriptor;
    IAAFSmartPointer<IAAFClassDef> pCDCDCIDescriptor;
    IAAFSmartPointer<IAAFClassDef> pCDDigitalImageDescriptor;
    IAAFSmartPointer<IAAFClassDef> pCDPCMDescriptor;
    IAAFSmartPointer<IAAFDataDef> pPictureDef;
    IAAFSmartPointer<IAAFDataDef> pSoundDef;
};


// get the startTime in the file SourcePackage referenced by the materialPackage
int64_t getStartTime(MaterialPackage* materialPackage, PackageSet& packages, Rational editRate);


    
};



#endif


