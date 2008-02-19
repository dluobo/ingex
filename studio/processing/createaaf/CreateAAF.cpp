/*
 * $Id: CreateAAF.cpp,v 1.3 2008/02/19 11:12:11 philipn Exp $
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
 
#include <cassert>

#include <AAFResult.h>
#include <AAFClassDefUIDs.h>
#include <AAFFileKinds.h>
#include <AAFSmartPointer.h>
#include <AAFSmartPointer2.h>
#include <AAFDataDefs.h>
#include <AAFStoredObjectIDs.h>
#include <AAFCompressionDefs.h>
#include <AAFTypeDefUIDs.h>
#include <AAFExtEnum.h>


#include "CreateAAF.h"
#include "CreateAAFException.h"

#include <DatabaseEnums.h>
#include <Logging.h>



using namespace std;
using namespace prodauto;


#define AAF_CHECK(cmd) \
{ \
    HRESULT result = cmd; \
    if (!AAFRESULT_SUCCEEDED(result)) \
    { \
        PA_LOGTHROW(CreateAAFException, ("'%s' failed (result = 0x%x)", #cmd, result)); \
    } \
}

    
// simple helper class to initialize and cleanup AAF library.
struct CAAFInitialize
{
    CAAFInitialize(const char *dllname = NULL)
    {
        HRESULT hr = AAFLoad(dllname);
        if (!AAFRESULT_SUCCEEDED(hr)) 
        {
            PA_LOGTHROW(CreateAAFException, ("Failed to load the AAF library"));
        }
    }

    ~CAAFInitialize()
    {
        AAFUnload();
    }
};

CAAFInitialize aafInit;
    

const aafUID_t NIL_UID = {0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0}};
    
static const aafUID_t kAAFPropID_DIDResolutionID = 
    {0xce2aca4d, 0x51ab, 0x11d3, {0xa0, 0x24, 0x0, 0x60, 0x94, 0xeb, 0x75, 0xcb}};
static const aafUID_t kAAFPropID_DIDFrameSampleSize = 
    {0xce2aca50, 0x51ab, 0x11d3, { 0xa0, 0x24, 0x0, 0x60, 0x94, 0xeb, 0x75, 0xcb}};
static const aafUID_t kAAFPropID_DIDImageSize = 
    {0xce2aca4f, 0x51ab, 0x11d3, {0xa0, 0x24, 0x0, 0x60, 0x94, 0xeb, 0x75, 0xcb}};

static const aafUID_t kAAFPropID_MobAppCode = 
    {0x96c46992, 0x4f62, 0x11d3, {0xa0, 0x22, 0x0, 0x60, 0x94, 0xeb, 0x75, 0xcb}};

static const aafUID_t kAAFCompressionDef_Avid_MJPEG_21 =
    {0x0e040201, 0x0201, 0x0108, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
static const aafUID_t kAAFCompressionDef_Avid_MJPEG_31 =
    {0x0e040201, 0x0201, 0x0106, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
static const aafUID_t kAAFCompressionDef_Avid_MJPEG_101 =
    {0x0e040201, 0x0201, 0x0104, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
static const aafUID_t kAAFCompressionDef_Avid_MJPEG_101m =
    {0x0e040201, 0x0201, 0x0402, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
static const aafUID_t kAAFCompressionDef_Avid_MJPEG_151s =
    {0x0e040201, 0x0201, 0x0202, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
static const aafUID_t kAAFCompressionDef_Avid_MJPEG_201 =
    {0x0e040201, 0x0201, 0x0102, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};


static aafMobID_t convertUMID(prodauto::UMID& umid)
{
    aafMobID_t mobID;
    
    memcpy(&mobID.SMPTELabel, &umid, 12);
    mobID.length = umid.octet12;
    mobID.instanceHigh = umid.octet13;
    mobID.instanceMid = umid.octet14;
    mobID.instanceLow = umid.octet15;

    // TODO: this assumes an AAF SDK UMID
    mobID.material.Data1 = (umid.octet16 << 24) | (umid.octet17 << 16) | (umid.octet18 << 8) | (umid.octet19);
    mobID.material.Data2 = (umid.octet20 << 8) | (umid.octet21);
    mobID.material.Data3 = (umid.octet22 << 8) | (umid.octet23);
    memcpy(&mobID.material.Data4, &umid.octet24, 8); 
    
    return mobID;
}

static aafTimeStamp_t convertTimestamp(prodauto::Timestamp& in)
{
    aafTimeStamp_t out;
    
    out.date.year = in.year;
    out.date.month = in.month;
    out.date.day = in.day;
    out.time.hour = in.hour;
    out.time.minute = in.min;
    out.time.second = in.sec;
    out.time.fraction = in.qmsec * 4 / 10; // MXF is 1/250th second and AAF is 1/100th second
    
    return out;
}

static aafRational_t convertRational(prodauto::Rational& in)
{
    aafRational_t out;
    
    out.numerator = in.numerator;
    out.denominator = in.denominator;
    
    return out;
}

static wchar_t* getSlotName(bool isPicture, bool isTimecode, int count)
{
    static wchar_t wSlotName[13];
    
    if (isPicture)
    {
        swprintf(wSlotName, 11, L"V%d", count);
    }
    else if (!isTimecode)
    {
        swprintf(wSlotName, 11, L"A%d", count);
    }
    else
    {
        swprintf(wSlotName, 12, L"TC%d", count);
    }
    
    return wSlotName;
}


// auto-deletes wide string
class WStringReturn
{
public:
    WStringReturn() : _value(0) {}
    WStringReturn(wchar_t* value) : _value(value) {}
    ~WStringReturn() { delete [] _value; }
    
    operator wchar_t* () { return _value; }
    
private:
    wchar_t* _value;
};

static WStringReturn convertName(string name)
{
    wchar_t* wName = new wchar_t[name.size() + 1];

    mbstowcs(wName, name.c_str(), name.size());
    wName[name.size()] = L'\0';
    
    return wName;
}

static WStringReturn createExtendedClipName(string name, int64_t startPosition)
{
    WStringReturn wName(convertName(name));

    size_t size = wcslen(wName) + 33;
    wchar_t* result = new wchar_t[size];
    
    if (startPosition < 0)
    {
        swprintf(result, size, L"%ls.0", (wchar_t*)wName);
    }
    else
    {
        swprintf(result, size, L"%ls.%02d%02d%02d%02d",
            (wchar_t*)wName,
            (int)(startPosition / (60 * 60 * 25)),
            (int)((startPosition % (60 * 60 * 25)) / (60 * 25)),
            (int)(((startPosition % (60 * 60 * 25)) % (60 * 25)) / 25),
            (int)(((startPosition % (60 * 60 * 25)) % (60 * 25)) % 25));
    }
    
    result[size - 1] = L'\0';
    
    return result;
}




AAFFile::AAFFile(string filename)
{
    IAAFSmartPointer<IAAFTypeDef> pTypeDef;
    IAAFSmartPointer<IAAFPropertyDef> pPropertyDef;

    
    remove(filename.c_str());
    wchar_t wFilename[FILENAME_MAX];
    mbstowcs(wFilename, filename.c_str(), FILENAME_MAX);

    aafProductIdentification_t productInfo;
    aafProductVersion_t ver = {0, 1, 0, 0, kAAFVersionUnknown};
    aafCharacter companyName[] = L"BBC Research";
    aafCharacter productName[] = L"Avid MXF import";
    aafCharacter productVersionString[] = L"Unknown";
    productInfo.companyName = companyName;
    productInfo.productName = productName;
    productInfo.productVersion = &ver;
    productInfo.productVersionString = productVersionString;
    productInfo.productID = NIL_UID;
    productInfo.platform = NULL;

    AAF_CHECK(AAFFileOpenNewModify(wFilename, 0, &productInfo, &pFile));

    try
    {
        AAF_CHECK(pFile->GetHeader(&pHeader));
    
        // Get the AAF Dictionary from the file
        AAF_CHECK(pHeader->GetDictionary(&pDictionary));
    
        // Lookup class definitions for the objects we want to create
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFMob, &pCDMob));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFCompositionMob, &pCDCompositionMob));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFMasterMob, &pCDMasterMob));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFSourceMob, &pCDSourceMob));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFSourceClip, &pCDSourceClip));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFSequence, &pCDSequence));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFSelector, &pCDSelector));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFFiller, &pCDFiller));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFTapeDescriptor, &pCDTapeDescriptor));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFCDCIDescriptor, &pCDCDCIDescriptor));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFDigitalImageDescriptor, &pCDDigitalImageDescriptor));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFPCMDescriptor, &pCDPCMDescriptor));
    
        // Lookup Picture and Sound DataDefinitions
        AAF_CHECK(pDictionary->LookupDataDef(kAAFDataDef_LegacyPicture, &pPictureDef));
        AAF_CHECK(pDictionary->LookupDataDef(kAAFDataDef_LegacySound, &pSoundDef));
    
        // register extension properties
        AAF_CHECK(pDictionary->LookupTypeDef(kAAFTypeID_Int32, &pTypeDef));
        if (pCDDigitalImageDescriptor->LookupPropertyDef(kAAFPropID_DIDResolutionID, &pPropertyDef) != AAFRESULT_SUCCESS)
        {
            AAF_CHECK(pCDDigitalImageDescriptor->RegisterOptionalPropertyDef(kAAFPropID_DIDResolutionID,
                L"ResolutionID", pTypeDef, &pPropertyDef));
        }
        if (pCDDigitalImageDescriptor->LookupPropertyDef(kAAFPropID_DIDFrameSampleSize, &pPropertyDef) != AAFRESULT_SUCCESS)
        {
            AAF_CHECK(pCDDigitalImageDescriptor->RegisterOptionalPropertyDef(kAAFPropID_DIDFrameSampleSize,
                L"FrameSampleSize", pTypeDef, &pPropertyDef));
        }
        if (pCDDigitalImageDescriptor->LookupPropertyDef(kAAFPropID_DIDImageSize, &pPropertyDef) != AAFRESULT_SUCCESS)
        {
            AAF_CHECK(pCDDigitalImageDescriptor->RegisterOptionalPropertyDef(kAAFPropID_DIDImageSize,
                L"ImageSize", pTypeDef, &pPropertyDef));
        }
        
        if (pCDMob->LookupPropertyDef(kAAFPropID_MobAppCode, &pPropertyDef) != AAFRESULT_SUCCESS)
        {
            AAF_CHECK(pCDMob->RegisterOptionalPropertyDef(kAAFPropID_MobAppCode,
                L"AppCode", pTypeDef, &pPropertyDef));
        }
        
    }
    catch (...)
    {    
        if (pFile != 0)
        {
            pFile->Close();
            pFile = IAAFSmartPointer<IAAFFile>(0);
        }
        throw;
    }
}

AAFFile::~AAFFile()
{
    if (pFile != 0)
    {
        pFile->Close();
        pFile = IAAFSmartPointer<IAAFFile>(0);
    }
}

void AAFFile::save()
{
    AAF_CHECK(pFile->Save());
}

void AAFFile::addClip(MaterialPackage* materialPackage, PackageSet& packages)
{
    try
    {
        // create master mob
        mapMasterMob(materialPackage);

        // get packages in material package reference chain
        PackageSet referencedPackages;
        getReferencedPackages(materialPackage, packages, referencedPackages);
        
        // create file and tape source mobs
        PackageSet::const_iterator iter;
        for (iter = referencedPackages.begin(); iter != referencedPackages.end(); iter++)
        {
            Package* package = *iter;
            
            if (package->getType() == MATERIAL_PACKAGE)
            {
                continue;
            }
    
            SourcePackage* sourcePackage = dynamic_cast<SourcePackage*>(package);
            
            if (sourcePackage->descriptor->getType() == FILE_ESSENCE_DESC_TYPE)
            {
                mapFileSourceMob(sourcePackage);
            }
            else if (sourcePackage->descriptor->getType() == TAPE_ESSENCE_DESC_TYPE)
            {
                mapTapeSourceMob(sourcePackage);
            }
            else // LIVE_ESSENCE_DESC_TYPE
            {
                // Avid doesn't support recording descriptors in MXF, so we
                // match what was done in the MXF files and use a tape descriptor
                mapTapeSourceMob(sourcePackage);
            }
        }
    }
    catch (CreateAAFException& ex)
    {
        throw;
    }
    catch (...)
    {
        PA_LOGTHROW(CreateAAFException, ("Failed to add clip to AAF file: unknown exception thrown"));
    }
}

void AAFFile::addMCClip(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, PackageSet& packages,
    vector<CutInfo> sequence)
{
    try
    {
        MaterialPackageSet activeMaterialPackages;
        
        // add clips for each material package that includes a track that will be
        // referenced (active) by the multi-camera clip
        MaterialPackageSet::const_iterator iter;
        for (iter = materialPackages.begin(); iter != materialPackages.end(); iter++)
        {
            MaterialPackage* materialPackage = *iter;

            SourcePackage* fileSourcePackage = 0;
            SourcePackage* sourcePackage = 0;
            Track* sourceTrack = 0;

            vector<Track*>::const_iterator iter2;
            for (iter2 = materialPackage->tracks.begin(); iter2 != materialPackage->tracks.end(); iter2++)
            {
                Track* track = *iter2;
                
                // get the source package and track for this material package track
                if (getSource(track, packages, &fileSourcePackage, &sourcePackage, &sourceTrack))
                {
                    bool donePackage = false;
                    map<uint32_t, MCTrackDef*>::const_iterator iter3;
                    for (iter3 = mcClipDef->trackDefs.begin(); !donePackage && iter3 != mcClipDef->trackDefs.end(); iter3++)
                    {
                        MCTrackDef* trackDef = (*iter3).second;
                        
                        map<uint32_t, MCSelectorDef*>::const_iterator iter4;
                        for (iter4 = trackDef->selectorDefs.begin(); !donePackage && iter4 != trackDef->selectorDefs.end(); iter4++)
                        {
                            MCSelectorDef* selectorDef = (*iter4).second;
                        
                            if (!selectorDef->isConnectedToSource())
                            {
                                continue;
                            }
                            
                            // if the source package and track match the multi-camera selection
                            if (selectorDef->sourceConfig->name.compare(fileSourcePackage->sourceConfigName) == 0 && 
                                sourceTrack->id == selectorDef->sourceTrackID)
                            {
                                if (activeMaterialPackages.insert(materialPackage).second)
                                {
                                    addClip(materialPackage, packages);
                                }
                                donePackage = true;
                            }
                        }
                    }
                }
            }
        }
        
        
        if (activeMaterialPackages.size() > 0)
        {
            // create composition mob collecting together all mob slots from all master mobs
            // and create a multi-camera clip composition mob
            createMCClip(mcClipDef, activeMaterialPackages, packages, sequence);
        }
        
    }
    catch (CreateAAFException& ex)
    {
        throw;
    }
    catch (...)
    {
        PA_LOGTHROW(CreateAAFException, ("Failed to add multi-camera clip to AAF file: unknown exception thrown"));
    }
}
    


void AAFFile::setAppCode(IAAFMob* pMob, aafInt32 value)
{
    IAAFSmartPointer<IAAFObject> pObject;
    IAAFSmartPointer<IAAFPropertyDef> pPropertyDef;
    IAAFSmartPointer<IAAFTypeDef> pTypeDef;
    IAAFSmartPointer<IAAFTypeDefInt> pTypeDefInt;
    IAAFSmartPointer<IAAFPropertyValue> pPropertyValue;
    
    AAF_CHECK(pMob->QueryInterface(IID_IAAFObject, (void**)&pObject));
    
    // Get ResolutionID property and type definition
    AAF_CHECK(pCDMob->LookupPropertyDef(kAAFPropID_MobAppCode, &pPropertyDef));
    AAF_CHECK(pPropertyDef->GetTypeDef(&pTypeDef));
    AAF_CHECK(pTypeDef->QueryInterface(IID_IAAFTypeDefInt, (void**)&pTypeDefInt));
    
    // Try to get prop value. If it doesn't exist, create it.
    HRESULT hr = pObject->GetPropertyValue(pPropertyDef, &pPropertyValue);
    if(hr != AAFRESULT_SUCCESS)
    {
        if(hr == AAFRESULT_PROP_NOT_PRESENT)
        {
            AAF_CHECK(pTypeDefInt->CreateValue(reinterpret_cast<aafMemPtr_t>(&value), 
                sizeof(value), &pPropertyValue));
        }
        else
        {
            AAF_CHECK(hr);
        }
    }
    else
    {
        // Property value exists, modify it.
        AAF_CHECK(pTypeDefInt->SetInteger(pPropertyValue, 
            reinterpret_cast<aafMemPtr_t>(&value), sizeof(value)));
    }
    
    // Set property value.
    AAF_CHECK(pObject->SetPropertyValue(pPropertyDef, pPropertyValue));
}

void AAFFile::setDIDInt32Property(IAAFClassDef* pCDDigitalImageDescriptor, 
    IAAFDigitalImageDescriptor2* pDigitalImageDescriptor2, aafUID_t propertyId, aafInt32 value)
{
    IAAFSmartPointer<IAAFObject> pObject;
    IAAFSmartPointer<IAAFPropertyDef> pPropertyDef;
    IAAFSmartPointer<IAAFTypeDef> pTypeDef;
    IAAFSmartPointer<IAAFTypeDefInt> pTypeDefInt;
    IAAFSmartPointer<IAAFPropertyValue> pPropertyValue;
    
    AAF_CHECK(pDigitalImageDescriptor2->QueryInterface(IID_IAAFObject, (void**)&pObject));
    
    // Get property and type definition
    AAF_CHECK(pCDDigitalImageDescriptor->LookupPropertyDef(propertyId, &pPropertyDef));
    AAF_CHECK(pPropertyDef->GetTypeDef(&pTypeDef));
    AAF_CHECK(pTypeDef->QueryInterface(IID_IAAFTypeDefInt, (void**)&pTypeDefInt));
    
    // Try to get prop value. If it doesn't exist, create it.
    HRESULT hr = pObject->GetPropertyValue(pPropertyDef, &pPropertyValue);
    if (hr != AAFRESULT_SUCCESS)
    {
        if (hr == AAFRESULT_PROP_NOT_PRESENT)
        {
            AAF_CHECK(pTypeDefInt->CreateValue(reinterpret_cast<aafMemPtr_t>(&value), 
                sizeof(value), &pPropertyValue));
        }
        else
        {
            // this will throw an exception
            AAF_CHECK(hr);
        }
    }
    else
    {
        // Property value exists, modify it.
        AAF_CHECK(pTypeDefInt->SetInteger(pPropertyValue, 
            reinterpret_cast<aafMemPtr_t>(&value), sizeof(value)));
    }
    
    // Set property value.
    AAF_CHECK(pObject->SetPropertyValue(pPropertyDef, pPropertyValue));
}

bool AAFFile::haveMob(aafMobID_t mobID)
{
    IAAFSmartPointer<IAAFMob> pMob;
    
    HRESULT hr;
    hr = pHeader->LookupMob(mobID, &pMob);
    if (hr == AAFRESULT_SUCCESS)
    {
        return true;
    }
    else if (hr == AAFRESULT_MOB_NOT_FOUND)
    {
        return false;
    }
    
    // this will throw an exception
    AAF_CHECK(hr);
    
    // only the compiler thinks it could get here
    return false;
}

void AAFFile::getReferencedPackages(Package* topPackage, PackageSet& packages, PackageSet& referencedPackages)
{
    vector<Track*>::const_iterator iter;
    for (iter = topPackage->tracks.begin(); iter != topPackage->tracks.end(); iter++)
    {
        Track* track = *iter;
        
        if (track->sourceClip->sourcePackageUID == g_nullUMID)
        {
            // no reference from this track
            continue;
        }
        
        SourcePackage dummy; // dummy acting as key
        dummy.uid = track->sourceClip->sourcePackageUID;
        
        // if reference package in packages then insert and recurse
        PackageSet::iterator result = packages.find(&dummy);
        if (result != packages.end())
        {
            // insert into referenced packages and recurse if not already present
            if (referencedPackages.insert(*result).second)
            {
                getReferencedPackages(*result, packages, referencedPackages);
            }
        }
        
    }
}

bool AAFFile::getSource(Track* track, PackageSet& packages, SourcePackage** fileSourcePackage,
    SourcePackage** sourcePackage, Track** sourceTrack)
{
    if (track->sourceClip->sourcePackageUID == g_nullUMID)
    {
        return false;
    }
    
    SourcePackage dummy; // dummy acting as key
    dummy.uid = track->sourceClip->sourcePackageUID;

    PackageSet::iterator result = packages.find(&dummy);
    if (result != packages.end())
    {
        Package* package = *result;
        Track* trk = package->getTrack(track->sourceClip->sourceTrackID);
        if (trk == 0)
        {
            return false;
        }
        
        // if the package is a tape or live recording then we take that to be the source
        if (package->getType() == SOURCE_PACKAGE)
        {
            SourcePackage* srcPackage = dynamic_cast<SourcePackage*>(package);
            if (srcPackage->descriptor->getType() == TAPE_ESSENCE_DESC_TYPE ||
                srcPackage->descriptor->getType() == LIVE_ESSENCE_DESC_TYPE)
            {
                *sourcePackage = srcPackage;
                *sourceTrack = trk;
                return true;
            }
            else if (srcPackage->descriptor->getType() == FILE_ESSENCE_DESC_TYPE)
            {
                *fileSourcePackage = srcPackage;
            }
        }
        
        // go further down the reference chain to find the tape/live source
        return getSource(trk, packages, fileSourcePackage, sourcePackage, sourceTrack);
    }
        
    return false;
}



// TODO: handle case where master mobs have different start timecodes
void AAFFile::createMCClip(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, PackageSet& packages,
    vector<CutInfo> sequence)
{
    IAAFSmartPointer<IEnumAAFMobSlots> pEnumMobSlots;
    IAAFSmartPointer<IAAFMob> pCollectionMob;
    IAAFSmartPointer<IAAFMob> pMCMob;
    IAAFSmartPointer<IAAFMob> pSeqMob;
    IAAFSmartPointer<IAAFMob> pMob;
    IAAFSmartPointer<IAAFMob2> pMob2;
    IAAFSmartPointer<IAAFMobSlot> pMobSlot;
    IAAFSmartPointer<IAAFMobSlot> pSequenceMobSlot;
    IAAFSmartPointer<IAAFTimelineMobSlot> pTimelineMobSlot;
    IAAFSmartPointer<IAAFTimelineMobSlot> pCollectionTimelineMobSlot;
    IAAFSmartPointer<IAAFDataDef> pDataDef;
    IAAFSmartPointer<IAAFSourceClip> pSourceClip;
    IAAFSmartPointer<IAAFSequence> pSequence;
    IAAFSmartPointer<IAAFComponent> pComponent;
    IAAFSmartPointer<IAAFSegment> pSegment;
    IAAFSmartPointer<IAAFSelector> pSelector;
    IAAFSmartPointer<IAAFFiller> pFiller;
    aafRational_t editRate;
    int collectionPictureSlotCount = 0;
    int collectionSoundSlotCount = 0;
    int mcPictureSlotCount = 0;
    int mcSoundSlotCount = 0;
    aafUInt32 trackNumber;
    aafBoolean_t isPicture;
    aafBoolean_t isSound;
    aafSourceRef_t sourceRef;
    aafLength_t length; 
    aafSlotID_t collectionSlotID = 1;
    aafSlotID_t mcSlotID = 1;
    bool includeSequence = !sequence.empty();
    aafSlotID_t seqSlotID = 1;
    int seqPictureSlotCount = 0;
    int seqSoundSlotCount = 0;
    Rational editRateForName = {25, 1}; // TODO: don't hard code
    int64_t startPosition = 0;

    // get the start time which we will use to set the multi-camera name and sequence name
    startPosition = getStartTime(*materialPackages.begin(), packages, editRateForName);
    
    // create collection composition mob (Avid application code 5)
    AAF_CHECK(pCDCompositionMob->CreateInstance(IID_IAAFCompositionMob, (IUnknown **)&pCollectionCompositionMob));
    AAF_CHECK(pCollectionCompositionMob->QueryInterface(IID_IAAFMob, (void **)&pCollectionMob));
    setAppCode(pCollectionMob, 5);
    AAF_CHECK(pHeader->AddMob(pCollectionMob));

    // create multi-camera composition mob (Avid application code 4)
    AAF_CHECK(pCDCompositionMob->CreateInstance(IID_IAAFCompositionMob, (IUnknown **)&pClipCompositionMob));
    AAF_CHECK(pClipCompositionMob->QueryInterface(IID_IAAFMob, (void **)&pMCMob));
    AAF_CHECK(pClipCompositionMob->QueryInterface(IID_IAAFMob2, (void **)&pMob2));
    AAF_CHECK(pMob2->SetUsageCode(kAAFUsage_LowerLevel));
    setAppCode(pMCMob, 4);
    AAF_CHECK(pMCMob->SetName(createExtendedClipName(mcClipDef->name, startPosition)));
    AAF_CHECK(pHeader->AddMob(pMCMob));
    
    if (includeSequence)
    {
        // create multi-camera sequence composition mob
        AAF_CHECK(pCDCompositionMob->CreateInstance(IID_IAAFCompositionMob, (IUnknown **)&pSequenceCompositionMob));
        AAF_CHECK(pSequenceCompositionMob->QueryInterface(IID_IAAFMob, (void **)&pSeqMob));
        AAF_CHECK(pSequenceCompositionMob->QueryInterface(IID_IAAFMob2, (void **)&pMob2));
        AAF_CHECK(pMob2->SetUsageCode(kAAFUsage_TopLevel));
        AAF_CHECK(pSeqMob->SetName(createExtendedClipName(mcClipDef->name + "_dc_sequence", startPosition)));
        AAF_CHECK(pHeader->AddMob(pSeqMob));
    }
    
    
    // get the maximum track length
    aafLength_t maxPictureLength = 0;
    map<uint32_t, MCTrackDef*>::const_iterator iter1;
    for (iter1 = mcClipDef->trackDefs.begin(); iter1 != mcClipDef->trackDefs.end(); iter1++)
    {
        MCTrackDef* trackDef = (*iter1).second;
        
        map<uint32_t, MCSelectorDef*>::const_iterator iter2;
        for (iter2 = trackDef->selectorDefs.begin(); iter2 != trackDef->selectorDefs.end(); iter2++)
        {
            MCSelectorDef* selectorDef = (*iter2).second;
        
            if (!selectorDef->isConnectedToSource())
            {
                continue;
            }
            
            bool doneSelectedTrack = false;
            MaterialPackageSet::const_iterator iter3;
            for (iter3 = materialPackages.begin(); !doneSelectedTrack && iter3 != materialPackages.end(); iter3++)
            {
                MaterialPackage* materialPackage = *iter3;
            
                vector<Track*>::const_iterator iter4;
                for (iter4 = materialPackage->tracks.begin(); !doneSelectedTrack && iter4 != materialPackage->tracks.end(); iter4++)
                {
                    Track* track = *iter4;
                    
                    SourcePackage* sourcePackage = 0;
                    SourcePackage* fileSourcePackage = 0;
                    Track* sourceTrack = 0;
        
                    // get the source package and track for this material package track
                    if (getSource(track, packages, &fileSourcePackage, &sourcePackage, &sourceTrack))
                    {
                        // if the source package and track match the mult-camera selection
                        if (selectorDef->sourceConfig->name.compare(fileSourcePackage->sourceConfigName) == 0 && 
                            sourceTrack->id == selectorDef->sourceTrackID)
                        {
                            doneSelectedTrack = true;
                            
                            AAF_CHECK(pHeader->LookupMob(convertUMID(materialPackage->uid), &pMob));
                            AAF_CHECK(pMob->LookupSlot(track->id, &pMobSlot));
                            
                            AAF_CHECK(pMobSlot->GetDataDef(&pDataDef));
                            AAF_CHECK(pDataDef->IsPictureKind(&isPicture));
                            AAF_CHECK(pDataDef->IsSoundKind(&isSound));
                            AAF_CHECK(isPicture == kAAFTrue || isSound == kAAFTrue);
                            
                            // add slot to collection composition Mob
                            
                            // get info from MasterMob slot
                            AAF_CHECK(pMobSlot->QueryInterface(IID_IAAFTimelineMobSlot, (void **)&pTimelineMobSlot));
                            AAF_CHECK(pTimelineMobSlot->GetEditRate(&editRate));
                            AAF_CHECK(pMobSlot->GetSegment(&pSegment));
                            AAF_CHECK(pSegment->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                            AAF_CHECK(pComponent->GetLength(&length));
                            
                            // update the maximum length
                            if (isPicture == kAAFTrue)
                            {
                                maxPictureLength = (length > maxPictureLength) ? length : maxPictureLength;
                            }
                            else
                            {
                                // TODO: don't hard code 25 fps edit rate
                                // round up
                                double factor = 25 * editRate.denominator / (double)(editRate.numerator);
                                aafLength_t pictureLength = (aafLength_t)(length * factor);
                                if (length > (aafLength_t)(pictureLength / factor))
                                {
                                    pictureLength++;
                                }

                                maxPictureLength = (pictureLength > maxPictureLength) ? pictureLength : maxPictureLength;
                            }
                        }
                    }
                }
            }
        }
    }
    
    
    // go through the multi-camera clip definition and add a multi-camera clip slot for tracks present
    // in one of the material packages
    for (iter1 = mcClipDef->trackDefs.begin(); iter1 != mcClipDef->trackDefs.end(); iter1++)
    {
        MCTrackDef* trackDef = (*iter1).second;
        
        map<uint32_t, MCSelectorDef*>::const_iterator iter2;
        for (iter2 = trackDef->selectorDefs.begin(); iter2 != trackDef->selectorDefs.end(); iter2++)
        {
            MCSelectorDef* selectorDef = (*iter2).second;
        
            if (!selectorDef->isConnectedToSource())
            {
                continue;
            }
            
            bool doneSelectedTrack = false;
            MaterialPackageSet::const_iterator iter3;
            for (iter3 = materialPackages.begin(); !doneSelectedTrack && iter3 != materialPackages.end(); iter3++)
            {
                MaterialPackage* materialPackage = *iter3;
            
                vector<Track*>::const_iterator iter4;
                for (iter4 = materialPackage->tracks.begin(); !doneSelectedTrack && iter4 != materialPackage->tracks.end(); iter4++)
                {
                    Track* track = *iter4;
                    
                    SourcePackage* sourcePackage = 0;
                    SourcePackage* fileSourcePackage = 0;
                    Track* sourceTrack = 0;
        
                    // get the source package and track for this material package track
                    if (getSource(track, packages, &fileSourcePackage, &sourcePackage, &sourceTrack))
                    {
                        // if the source package and track match the mult-camera selection
                        if (selectorDef->sourceConfig->name.compare(fileSourcePackage->sourceConfigName) == 0 && 
                            sourceTrack->id == selectorDef->sourceTrackID)
                        {
                            doneSelectedTrack = true;

                            AAF_CHECK(pHeader->LookupMob(convertUMID(materialPackage->uid), &pMob));
                            AAF_CHECK(pMob->LookupSlot(track->id, &pMobSlot));
                            
                            AAF_CHECK(pMobSlot->GetDataDef(&pDataDef));
                            AAF_CHECK(pDataDef->IsPictureKind(&isPicture));
                            AAF_CHECK(pDataDef->IsSoundKind(&isSound));
                            AAF_CHECK(isPicture == kAAFTrue || isSound == kAAFTrue);
                            
                            // add slot to collection composition Mob
                            
                            if (isPicture == kAAFTrue)
                            {
                                collectionPictureSlotCount++;
                                trackNumber = collectionPictureSlotCount;
                            }
                            else
                            {
                                collectionSoundSlotCount++;
                                trackNumber = collectionSoundSlotCount;
                            }
                            
                            // get info from MasterMob slot
                            AAF_CHECK(pMobSlot->QueryInterface(IID_IAAFTimelineMobSlot, (void **)&pTimelineMobSlot));
                            AAF_CHECK(pTimelineMobSlot->GetEditRate(&editRate));
                            AAF_CHECK(pMobSlot->GetSegment(&pSegment));
                            AAF_CHECK(pSegment->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                            AAF_CHECK(pComponent->GetLength(&length));
                            AAF_CHECK(pMob->GetMobID(&sourceRef.sourceID));
                            AAF_CHECK(pMobSlot->GetSlotID(&sourceRef.sourceSlotID));
                            sourceRef.startTime = 0;
                            
                            // create SourceClip referencing MasterMob slot
                            AAF_CHECK(pCDSourceClip->CreateInstance(IID_IAAFSourceClip, (IUnknown **)&pSourceClip));
                            AAF_CHECK(pSourceClip->Initialize(pDataDef, length, sourceRef));
                    
                            // create Sequence containing SourceClip
                            AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                            AAF_CHECK(pCDSequence->CreateInstance(IID_IAAFSequence, (IUnknown **)&pSequence));
                            AAF_CHECK(pSequence->Initialize(pDataDef));
                            AAF_CHECK(pSequence->AppendComponent(pComponent));
                    
                            // append filler if the length < maximum length
                            if (isPicture == kAAFTrue)
                            {
                                if (maxPictureLength > length)
                                {
                                    // add filler
                                    AAF_CHECK(pCDFiller->CreateInstance(IID_IAAFFiller, (IUnknown **)&pFiller));
                                    AAF_CHECK(pFiller->Initialize(pDataDef, maxPictureLength - length));
                                    AAF_CHECK(pFiller->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                                    AAF_CHECK(pSequence->AppendComponent(pComponent));
                                }
                            }
                            else // isSound
                            {
                                // TODO: don't hard code 25 fps edit rate
                                double factor = editRate.numerator / (double)(25 * editRate.denominator);
                                aafLength_t soundMaxLength = (aafLength_t)(maxPictureLength * factor + 0.5);
                                
                                if (soundMaxLength > length)
                                {
                                    // add filler
                                    AAF_CHECK(pCDFiller->CreateInstance(IID_IAAFFiller, (IUnknown **)&pFiller));
                                    AAF_CHECK(pFiller->Initialize(pDataDef, soundMaxLength - length));
                                    AAF_CHECK(pFiller->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                                    AAF_CHECK(pSequence->AppendComponent(pComponent));
                                }
                            }
                            
                            // create TimelineMobSlot with Sequence 
                            AAF_CHECK(pSequence->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                            AAF_CHECK(pCollectionMob->AppendNewTimelineSlot(
                                editRate, pSegment, collectionSlotID, 
                                getSlotName(isPicture == kAAFTrue, false, trackNumber),
                                0, &pCollectionTimelineMobSlot));
                            collectionSlotID++;
                            


                            // add slot or selection to multi-camera clip composition Mob
                            
                            if (isPicture == kAAFTrue)
                            {
                                length = maxPictureLength;
                            }
                            else
                            {
                                // TODO: don't hard code 25 fps edit rate
                                double factor = editRate.numerator / (double)(25 * editRate.denominator);
                                length = (aafLength_t)(maxPictureLength * factor + 0.5);
                            }


                            // create SourceClip referencing the collection MobSlot
                            AAF_CHECK(pCollectionTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
                            AAF_CHECK(pCollectionMob->GetMobID(&sourceRef.sourceID));
                            AAF_CHECK(pMobSlot->GetSlotID(&sourceRef.sourceSlotID));
                            sourceRef.startTime = 0;
                            AAF_CHECK(pCDSourceClip->CreateInstance(IID_IAAFSourceClip, (IUnknown **)&pSourceClip));
                            AAF_CHECK(pSourceClip->Initialize(pDataDef, length, sourceRef));
                            
                            if (AAFRESULT_SUCCEEDED(pMCMob->LookupSlot(trackDef->index, &pMobSlot)))
                            {
                                // get the selector
                                AAF_CHECK(pMobSlot->GetSegment(&pSegment));
                                AAF_CHECK(pSegment->QueryInterface(IID_IAAFSequence, (void **)&pSequence));
                                AAF_CHECK(pSequence->GetComponentAt(0, &pComponent));
                                AAF_CHECK(pComponent->QueryInterface(IID_IAAFSelector, (void **)&pSelector));
                                
                                // append alternate segment
                                AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pSegment));
                                AAF_CHECK(pSelector->AppendAlternateSegment(pSegment));
                            }
                            else
                            {
                                if (isPicture == kAAFTrue)
                                {
                                    mcPictureSlotCount++;
                                    trackNumber = mcPictureSlotCount;
                                }
                                else
                                {
                                    mcSoundSlotCount++;
                                    trackNumber = mcSoundSlotCount;
                                }
                            
                                if (trackDef->selectorDefs.size() == 1)
                                {
                                    // create Sequence containing SourceClip
                                    AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                                    AAF_CHECK(pCDSequence->CreateInstance(IID_IAAFSequence, (IUnknown **)&pSequence));
                                    AAF_CHECK(pSequence->Initialize(pDataDef));
                                    AAF_CHECK(pSequence->AppendComponent(pComponent));
                                }
                                else
                                {
                                    // create selector
                                    AAF_CHECK(pCDSelector->CreateInstance(IID_IAAFSelector, (IUnknown **)&pSelector));
                                
                                    // set selected segment
                                    AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                                    AAF_CHECK(pSelector->SetSelectedSegment(pSegment));
                    
                                    // create Sequence containing Selector
                                    AAF_CHECK(pSelector->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                                    AAF_CHECK(pComponent->SetDataDef(pDataDef));
                                    AAF_CHECK(pComponent->SetLength(length));
                                    AAF_CHECK(pCDSequence->CreateInstance(IID_IAAFSequence, (IUnknown **)&pSequence));
                                    AAF_CHECK(pSequence->Initialize(pDataDef));
                                    AAF_CHECK(pSequence->AppendComponent(pComponent));
                                }
                                
                                // create TimelineMobSlot with Sequence 
                                // Note: the slot name follows the generated trackNumber, which could be
                                // different from the trackDef->number
                                AAF_CHECK(pSequence->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                                AAF_CHECK(pMCMob->AppendNewTimelineSlot(
                                    editRate, pSegment, mcSlotID, 
                                    getSlotName(isPicture == kAAFTrue, false, trackNumber),
                                    0, &pTimelineMobSlot));
                                if (trackDef->number != 0)
                                {
                                    AAF_CHECK(pTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
                                    AAF_CHECK(pMobSlot->SetPhysicalNum(trackDef->number));
                                }
                                mcSlotID++;

                            }
                            
                            
                            // add slot or selection to multi-camera sequence composition Mob
                            
                            if (includeSequence)
                            {
                                if (sequence.size() > 1)
                                {
                                    if (trackDef->selectorDefs.size() > 1 &&
                                        AAFRESULT_SUCCEEDED(pSeqMob->LookupSlot(trackDef->index, &pSequenceMobSlot)))
                                    {
                                        // sequence is present, and therefore we add a selector
                                        
                                        size_t i;
                                        aafLength_t sourceClipLength;
                                        aafLength_t totalSourceClipLength = 0;
                                        CutInfo current;
                                        CutInfo next; 
                                        for (i = 0; i < sequence.size(); i++)
                                        {
                                            if (totalSourceClipLength >= length)
                                            {
                                                // end of sequence
                                                break;
                                            }
                                            
                                            current = sequence[i];
                                            if (isPicture != kAAFTrue)
                                            {
                                                // convert position to audio samples
                                                // TODO: don't hard code 25 fps edit rate
                                                double factor = editRate.numerator / (double)(25 * editRate.denominator);
                                                current.position = (aafLength_t)(current.position * factor + 0.5);
                                            }
                                            if (i != sequence.size() - 1)
                                            {
                                                // calculate length of segment using next cut in sequence
                                                
                                                next = sequence[i + 1];
                                                if (isPicture != kAAFTrue)
                                                {
                                                    // convert position to audio samples
                                                    // TODO: don't hard code 25 fps edit rate
                                                    double factor = editRate.numerator / (double)(25 * editRate.denominator);
                                                    next.position = (aafLength_t)(next.position * factor + 0.5);
                                                }
                                                
                                                // calculate length of segment
                                                if (next.position > length)
                                                {
                                                    // next cut is beyond the end of the sequence so the clip
                                                    // extends to the end
                                                    sourceClipLength = length - totalSourceClipLength;
                                                }
                                                else
                                                {
                                                    // clip ends at the next cut
                                                    sourceClipLength = next.position - current.position;
                                                }
                                            }
                                            else
                                            {
                                                // calculate length of segment using length of total clip
                                                
                                                if (current.position > length)
                                                {
                                                    // cut is beyond end of sequence
                                                    // NOTE: we shouldn't be here because the first cut should be
                                                    // at position 0 and the previous clip will have extended
                                                    // right to the end
                                                    sourceClipLength = length - totalSourceClipLength;
                                                }
                                                else
                                                {
                                                    // clip ends at the start of the next cut
                                                    sourceClipLength = length - current.position;
                                                }
                                            }
                                            
                                            totalSourceClipLength += sourceClipLength;

                                            
                                            // create SourceClip referencing the collection MobSlot
                                            AAF_CHECK(pCollectionTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
                                            AAF_CHECK(pCollectionMob->GetMobID(&sourceRef.sourceID));
                                            AAF_CHECK(pMobSlot->GetSlotID(&sourceRef.sourceSlotID));
                                            sourceRef.startTime = current.position;
                                            AAF_CHECK(pCDSourceClip->CreateInstance(IID_IAAFSourceClip, (IUnknown **)&pSourceClip));
                                            AAF_CHECK(pSourceClip->Initialize(pDataDef, sourceClipLength, sourceRef));
                                            
                                            // get the selector
                                            AAF_CHECK(pSequenceMobSlot->GetSegment(&pSegment));
                                            AAF_CHECK(pSegment->QueryInterface(IID_IAAFSequence, (void **)&pSequence));
                                            AAF_CHECK(pSequence->GetComponentAt(i, &pComponent));
                                            AAF_CHECK(pComponent->QueryInterface(IID_IAAFSelector, (void **)&pSelector));
                                            
                                            if (selectorDef->sourceConfig->name.compare(sequence[i].source) == 0)
                                            {
                                                // set selected segment
                                                AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                                                AAF_CHECK(pSelector->SetSelectedSegment(pSegment));
                                            }
                                            else
                                            {
                                                // append alternate segment
                                                AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pSegment));
                                                AAF_CHECK(pSelector->AppendAlternateSegment(pSegment));
                                            }
                                        }
                                    }
                                    else
                                    {
                                        // sequence not present or only 1 selector is present
                                        
                                        if (isPicture == kAAFTrue)
                                        {
                                            seqPictureSlotCount++;
                                            trackNumber = seqPictureSlotCount;
                                        }
                                        else
                                        {
                                            seqSoundSlotCount++;
                                            trackNumber = seqSoundSlotCount;
                                        }

                                        // create Sequence
                                        AAF_CHECK(pCDSequence->CreateInstance(IID_IAAFSequence, (IUnknown **)&pSequence));
                                        AAF_CHECK(pSequence->Initialize(pDataDef));
                                        
                                        // create TimelineMobSlot with Sequence 
                                        // Note: the slot name follows the generated trackNumber, which could be
                                        // different from the trackDef->number
                                        AAF_CHECK(pSequence->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                                        AAF_CHECK(pSeqMob->AppendNewTimelineSlot(
                                            editRate, pSegment, seqSlotID, 
                                            getSlotName(isPicture == kAAFTrue, false, trackNumber),
                                            0, &pTimelineMobSlot));
                                        if (trackDef->number != 0)
                                        {
                                            AAF_CHECK(pTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
                                            AAF_CHECK(pMobSlot->SetPhysicalNum(trackDef->number));
                                        }
                                        seqSlotID++;

                                        // add Selectors or SourceClip to Sequence
                                        size_t i;
                                        aafLength_t sourceClipLength;
                                        aafLength_t totalSourceClipLength = 0;
                                        CutInfo current;
                                        CutInfo next; 
                                        for (i = 0; i < sequence.size(); i++)
                                        {
                                            if (totalSourceClipLength >= length)
                                            {
                                                // end of sequence
                                                break;
                                            }
                                            
                                            current = sequence[i];
                                            if (isPicture != kAAFTrue)
                                            {
                                                // convert position to audio samples
                                                // TODO: don't hard code 25 fps edit rate
                                                double factor = editRate.numerator / (double)(25 * editRate.denominator);
                                                current.position = (aafLength_t)(current.position * factor + 0.5);
                                            }
                                            if (i != sequence.size() - 1)
                                            {
                                                // calculate length of segment using next cut in sequence
                                                
                                                next = sequence[i + 1];
                                                if (isPicture != kAAFTrue)
                                                {
                                                    // convert position to audio samples
                                                    // TODO: don't hard code 25 fps edit rate
                                                    double factor = editRate.numerator / (double)(25 * editRate.denominator);
                                                    next.position = (aafLength_t)(next.position * factor + 0.5);
                                                }
                                                
                                                // calculate length of segment
                                                if (next.position > length)
                                                {
                                                    // next cut is beyond the end of the sequence so the clip
                                                    // extends to the end
                                                    sourceClipLength = length - totalSourceClipLength;
                                                }
                                                else
                                                {
                                                    // clip ends at the next cut
                                                    sourceClipLength = next.position - current.position;
                                                }
                                            }
                                            else
                                            {
                                                // calculate length of segment using length of total clip
                                                
                                                if (current.position > length)
                                                {
                                                    // cut is beyond end of sequence
                                                    // NOTE: we shouldn't be here because the first cut should be
                                                    // at position 0 and the previous clip will have extended
                                                    // right to the end
                                                    sourceClipLength = length - totalSourceClipLength;
                                                }
                                                else
                                                {
                                                    // clip ends at the start of the next cut
                                                    sourceClipLength = length - current.position;
                                                }
                                            }
                                            totalSourceClipLength += sourceClipLength;
                                            
                                            
                                            // create SourceClip referencing the collection MobSlot
                                            AAF_CHECK(pCollectionTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
                                            AAF_CHECK(pCollectionMob->GetMobID(&sourceRef.sourceID));
                                            AAF_CHECK(pMobSlot->GetSlotID(&sourceRef.sourceSlotID));
                                            sourceRef.startTime = current.position;
                                            AAF_CHECK(pCDSourceClip->CreateInstance(IID_IAAFSourceClip, (IUnknown **)&pSourceClip));
                                            AAF_CHECK(pSourceClip->Initialize(pDataDef, sourceClipLength, sourceRef));
                                            
                                            if (trackDef->selectorDefs.size() > 1)
                                            {
                                                // create selector
                                                AAF_CHECK(pCDSelector->CreateInstance(IID_IAAFSelector, (IUnknown **)&pSelector));
                                                AAF_CHECK(pSelector->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                                                AAF_CHECK(pComponent->SetDataDef(pDataDef));
                                                AAF_CHECK(pComponent->SetLength(sourceClipLength));
                                            
                                                if (selectorDef->sourceConfig->name.compare(sequence[i].source) == 0)
                                                {
                                                    // set selected segment
                                                    AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                                                    AAF_CHECK(pSelector->SetSelectedSegment(pSegment));
                                                }
                                                else
                                                {
                                                    // append alternate segment
                                                    AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pSegment));
                                                    AAF_CHECK(pSelector->AppendAlternateSegment(pSegment));
                                                }
                                            }
                                            else
                                            {
                                                // source clip is appended to sequence
                                                AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                                            }
                            
                                            AAF_CHECK(pSequence->AppendComponent(pComponent));
                                        }
                                    }
                                }
                                
                                // single cut, which should be at position 0, so sequence contains one clip
                                else
                                {
                                    // create SourceClip referencing the collection MobSlot
                                    AAF_CHECK(pCollectionTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
                                    AAF_CHECK(pCollectionMob->GetMobID(&sourceRef.sourceID));
                                    AAF_CHECK(pMobSlot->GetSlotID(&sourceRef.sourceSlotID));
                                    sourceRef.startTime = 0;
                                    AAF_CHECK(pCDSourceClip->CreateInstance(IID_IAAFSourceClip, (IUnknown **)&pSourceClip));
                                    AAF_CHECK(pSourceClip->Initialize(pDataDef, length, sourceRef));
                                    
                                    if (AAFRESULT_SUCCEEDED(pSeqMob->LookupSlot(trackDef->index, &pMobSlot)))
                                    {
                                        // get the selector
                                        AAF_CHECK(pMobSlot->GetSegment(&pSegment));
                                        AAF_CHECK(pSegment->QueryInterface(IID_IAAFSequence, (void **)&pSequence));
                                        AAF_CHECK(pSequence->GetComponentAt(0, &pComponent));
                                        AAF_CHECK(pComponent->QueryInterface(IID_IAAFSelector, (void **)&pSelector));
                                        
                                        // append alternate segment
                                        AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pSegment));
                                        AAF_CHECK(pSelector->AppendAlternateSegment(pSegment));
                                    }
                                    else
                                    {
                                        if (isPicture == kAAFTrue)
                                        {
                                            seqPictureSlotCount++;
                                            trackNumber = seqPictureSlotCount;
                                        }
                                        else
                                        {
                                            seqSoundSlotCount++;
                                            trackNumber = seqSoundSlotCount;
                                        }
                                    
                                        if (trackDef->selectorDefs.size() == 1)
                                        {
                                            // create Sequence containing SourceClip
                                            AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                                            AAF_CHECK(pCDSequence->CreateInstance(IID_IAAFSequence, (IUnknown **)&pSequence));
                                            AAF_CHECK(pSequence->Initialize(pDataDef));
                                            AAF_CHECK(pSequence->AppendComponent(pComponent));
                                        }
                                        else
                                        {
                                            // create selector
                                            AAF_CHECK(pCDSelector->CreateInstance(IID_IAAFSelector, (IUnknown **)&pSelector));
                                        
                                            // set selected segment
                                            AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                                            AAF_CHECK(pSelector->SetSelectedSegment(pSegment));
                            
                                            // create Sequence containing Selector
                                            AAF_CHECK(pSelector->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                                            AAF_CHECK(pComponent->SetDataDef(pDataDef));
                                            AAF_CHECK(pComponent->SetLength(length));
                                            AAF_CHECK(pCDSequence->CreateInstance(IID_IAAFSequence, (IUnknown **)&pSequence));
                                            AAF_CHECK(pSequence->Initialize(pDataDef));
                                            AAF_CHECK(pSequence->AppendComponent(pComponent));
                                        }
                                        
                                        // create TimelineMobSlot with Sequence 
                                        // Note: the slot name follows the generated trackNumber, which could be
                                        // different from the trackDef->number
                                        AAF_CHECK(pSequence->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                                        AAF_CHECK(pSeqMob->AppendNewTimelineSlot(
                                            editRate, pSegment, seqSlotID, 
                                            getSlotName(isPicture == kAAFTrue, false, trackNumber),
                                            0, &pTimelineMobSlot));
                                        if (trackDef->number != 0)
                                        {
                                            AAF_CHECK(pTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
                                            AAF_CHECK(pMobSlot->SetPhysicalNum(trackDef->number));
                                        }
                                        seqSlotID++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            // TODO: log warning if no selected track found?
        }
    }

}

void AAFFile::mapUserComments(Package* package, IAAFMob* mob)
{
    vector<UserComment> userComments = package->getUserComments();
    vector<UserComment>::iterator iter;
    for (iter = userComments.begin(); iter != userComments.end(); iter++)
    {
        UserComment& userComment = *iter;
        
        AAF_CHECK(mob->AppendComment(convertName(userComment.name), convertName(userComment.value))); 
    }
}

void AAFFile::mapMasterMob(MaterialPackage* materialPackage)
{
    IAAFSmartPointer<IAAFMasterMob> pMasterMob;
    IAAFSmartPointer<IAAFMob> pMob;
    IAAFSmartPointer<IAAFSourceClip> pSourceClip;
    IAAFSmartPointer<IAAFTimelineMobSlot> pTimelineMobSlot;
    IAAFSmartPointer<IAAFMobSlot> pMobSlot;
    IAAFSmartPointer<IAAFSegment> pSegment;
    IAAFDataDef* pDataDef = NULL;

    // return if the material package has already been mapped to a mob
    if (haveMob(convertUMID(materialPackage->uid)))
    {
        return;
    }
    
    
    // create master mob    
    AAF_CHECK(pCDMasterMob->CreateInstance(IID_IAAFMasterMob, (IUnknown **)&pMasterMob));
    AAF_CHECK(pMasterMob->QueryInterface(IID_IAAFMob, (void **)&pMob));
    AAF_CHECK(pMob->SetMobID(convertUMID(materialPackage->uid)));
    AAF_CHECK(pMob->SetName(convertName(materialPackage->name)));
    AAF_CHECK(pMob->SetCreateTime(convertTimestamp(materialPackage->creationDate)));
    AAF_CHECK(pHeader->AddMob(pMob));

    // user comments
    mapUserComments(materialPackage, pMob);
    
    // create mob slots
    vector<Track*>::const_iterator iter;
    for (iter = materialPackage->tracks.begin(); iter != materialPackage->tracks.end(); iter++)
    {
        Track* track = *iter;

        if (track->dataDef == PICTURE_DATA_DEFINITION)
        {
            pDataDef = pPictureDef;
        }
        else // SOUND_DATA_DEFINITION
        {
            pDataDef = pSoundDef;
        }
        
        // create a source clip referencing the file source mob
        aafSourceRef_t sourceRef;
        sourceRef.sourceID = convertUMID(track->sourceClip->sourcePackageUID);
        sourceRef.sourceSlotID = track->sourceClip->sourceTrackID;
        sourceRef.startTime = track->sourceClip->position;
        AAF_CHECK(pCDSourceClip->CreateInstance(IID_IAAFSourceClip, (IUnknown **)&pSourceClip));
        AAF_CHECK(pSourceClip->Initialize(pDataDef, track->sourceClip->length, sourceRef));
        AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
        
        // create mob slot        
        AAF_CHECK(pMob->AppendNewTimelineSlot(convertRational(track->editRate), pSegment, track->id,
            convertName(track->name), 0, &pTimelineMobSlot));
        if (track->number != 0)
        {
            AAF_CHECK(pTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
            AAF_CHECK(pMobSlot->SetPhysicalNum(track->number));
        }
    }        
}

    
void AAFFile::mapFileSourceMob(SourcePackage* sourcePackage)
{
    IAAFSmartPointer<IAAFSourceMob> pFileSourceMob;
    IAAFSmartPointer<IAAFMob> pMob;
    IAAFSmartPointer<IAAFSourceClip> pSourceClip;
    IAAFSmartPointer<IAAFSegment> pSegment;
    IAAFSmartPointer<IAAFCDCIDescriptor> pCDCIDescriptor;
    IAAFSmartPointer<IAAFCDCIDescriptor2> pCDCIDescriptor2;
    IAAFSmartPointer<IAAFDigitalImageDescriptor2> pDigitalImageDescriptor2;
    IAAFSmartPointer<IAAFPCMDescriptor> pPCMDescriptor;
    IAAFSmartPointer<IAAFSoundDescriptor> pSoundDescriptor;
    IAAFSmartPointer<IAAFFileDescriptor> pFileDescriptor;
    IAAFSmartPointer<IAAFEssenceDescriptor> pEssenceDescriptor;
    IAAFSmartPointer<IAAFTimelineMobSlot> pTimelineMobSlot;
    IAAFDataDef* pDataDef = NULL;
    aafRational_t audioSamplingRate = {48000, 1};
    // TODO: see below - image size for video with variable size frames
    
    // return if the file source package has already been mapped to a mob
    if (haveMob(convertUMID(sourcePackage->uid)))
    {
        return;
    }
    
    FileEssenceDescriptor* fileDescriptor = dynamic_cast<FileEssenceDescriptor*>(sourcePackage->descriptor);
    if (fileDescriptor == 0)
    {
        PA_LOGTHROW(CreateAAFException, ("File source package has a null file essence descriptor"));
    }
    
    // create file source mob    
    AAF_CHECK(pCDSourceMob->CreateInstance(IID_IAAFSourceMob, (IUnknown **)&pFileSourceMob));
    AAF_CHECK(pFileSourceMob->QueryInterface(IID_IAAFMob, (void **)&pMob));
    AAF_CHECK(pMob->SetMobID(convertUMID(sourcePackage->uid)));
    AAF_CHECK(pMob->SetName(convertName(sourcePackage->name)));
    AAF_CHECK(pMob->SetCreateTime(convertTimestamp(sourcePackage->creationDate)));
    AAF_CHECK(pHeader->AddMob(pMob));

    // user comments
    mapUserComments(sourcePackage, pMob);
    
    // create mob slots
    vector<Track*>::const_iterator iter;
    for (iter = sourcePackage->tracks.begin(); iter != sourcePackage->tracks.end(); iter++)
    {
        Track* track = *iter;

        if (track->dataDef == PICTURE_DATA_DEFINITION)
        {
            pDataDef = pPictureDef;
        }
        else // SOUND_DATA_DEFINITION
        {
            pDataDef = pSoundDef;
        }
        
        // create a source clip referencing the tape source mob
        aafSourceRef_t sourceRef;
        sourceRef.sourceID = convertUMID(track->sourceClip->sourcePackageUID);
        sourceRef.sourceSlotID = track->sourceClip->sourceTrackID;
        sourceRef.startTime = track->sourceClip->position;
        AAF_CHECK(pCDSourceClip->CreateInstance(IID_IAAFSourceClip, (IUnknown **)&pSourceClip));
        AAF_CHECK(pSourceClip->Initialize(pDataDef, track->sourceClip->length, sourceRef));
        AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
        
        // create mob slot. Note: (physical) track number is ignored for file source mobs        
        AAF_CHECK(pMob->AppendNewTimelineSlot(convertRational(track->editRate), pSegment, track->id,
            convertName(track->name), 0, &pTimelineMobSlot));
    
        // create the file descriptor
        if (track->dataDef == PICTURE_DATA_DEFINITION)
        {
            // TODO: match level of support in writeavidmxf
            // only PAL supported for now
            assert(track->editRate.numerator == 25 && track->editRate.denominator == 1);

            if (fileDescriptor->imageAspectRatio.numerator == 0 ||
                fileDescriptor->imageAspectRatio.denominator == 0)
            {
                PA_LOGTHROW(CreateAAFException, ("File descriptor image aspect ratio %d%d is invalid",
                    fileDescriptor->imageAspectRatio.numerator, fileDescriptor->imageAspectRatio.denominator));
            }
            
            AAF_CHECK(pCDCDCIDescriptor->CreateInstance(IID_IAAFCDCIDescriptor2, (IUnknown **)&pCDCIDescriptor));
            AAF_CHECK(pCDCIDescriptor->QueryInterface(IID_IAAFCDCIDescriptor2, (void **)&pCDCIDescriptor2));
            AAF_CHECK(pCDCIDescriptor2->QueryInterface(IID_IAAFDigitalImageDescriptor2, (void **)&pDigitalImageDescriptor2));
            AAF_CHECK(pCDCIDescriptor2->QueryInterface(IID_IAAFFileDescriptor, (void **)&pFileDescriptor));
    
            // TODO: should we use legacy as done when writing the MXF files?
            AAF_CHECK(pCDCIDescriptor2->Initialize());
            if (fileDescriptor->videoResolutionID == DV25_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(2));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_DV_Based_25Mbps_625_50));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(288, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 0, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields)); // non-legacy value
                aafInt32 videoLineMap[2] = {23, 335};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 144000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 144000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x8d);
            }
            else if (fileDescriptor->videoResolutionID == DV50_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_DV_Based_50Mbps_625_50));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(288, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 0, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields)); // non-legacy value
                aafInt32 videoLineMap[2] = {23, 335};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 288000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 288000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x8e);
            }
            else if (fileDescriptor->videoResolutionID == UNC_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(592, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(576, 720, 16, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFMixedFields));
                aafInt32 videoLineMap[2] = {15, 328};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(8192));
                AAF_CHECK(pDigitalImageDescriptor2->SetFieldStartOffset(7680));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 860160); // aligned frame size
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 860160);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0xaa);
            }
            else if (fileDescriptor->videoResolutionID == MJPEG21_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_Avid_MJPEG_21));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(296, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 8, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {15, 328};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 0); // variable frame size
                // TODO: don't have this size
                // setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    // kAAFPropID_DIDImageSize, track->length * 144000); 
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x4c);
            }
            else if (fileDescriptor->videoResolutionID == MJPEG31_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_Avid_MJPEG_31));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(296, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 8, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {15, 328};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 0); // variable frame size
                // TODO: don't have this size
                // setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    // kAAFPropID_DIDImageSize, track->length * 144000); 
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x4d);
            }
            else if (fileDescriptor->videoResolutionID == MJPEG101_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_Avid_MJPEG_101));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(296, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 8, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {15, 328};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 0); // variable frame size
                // TODO: don't have this size
                // setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    // kAAFPropID_DIDImageSize, track->length * 144000); 
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x4b);
            }
            else if (fileDescriptor->videoResolutionID == MJPEG101M_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_Avid_MJPEG_101m));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(296, 288));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 288, 8, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFOneField));
                aafInt32 videoLineMap[1] = {15};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(1, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 0); // variable frame size
                // TODO: don't have this size
                // setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    // kAAFPropID_DIDImageSize, track->length * 144000); 
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x6e);
            }
            else if (fileDescriptor->videoResolutionID == MJPEG151S_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_Avid_MJPEG_151s));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(296, 352));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 352, 8, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFOneField));
                aafInt32 videoLineMap[1] = {15};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(1, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 0); // variable frame size
                // TODO: don't have this size
                // setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    // kAAFPropID_DIDImageSize, track->length * 144000); 
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x4e);
            }
            else if (fileDescriptor->videoResolutionID == MJPEG201_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_Avid_MJPEG_201));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(296, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 8, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {15, 328};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 0); // variable frame size
                // TODO: don't have this size
                // setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    // kAAFPropID_DIDImageSize, track->length * 144000); 
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x52);
            }
            else
            {
                PA_LOGTHROW(CreateAAFException, ("Unsupported resolution id %d", fileDescriptor->videoResolutionID));
            }
            AAF_CHECK(pCDCIDescriptor2->SetComponentWidth(8));
            AAF_CHECK(pCDCIDescriptor2->SetColorSiting(kAAFRec601));
            AAF_CHECK(pCDCIDescriptor2->SetBlackReferenceLevel(16));
            AAF_CHECK(pCDCIDescriptor2->SetWhiteReferenceLevel(235));
            AAF_CHECK(pCDCIDescriptor2->SetColorRange(225));
            AAF_CHECK(pDigitalImageDescriptor2->SetImageAspectRatio(convertRational(fileDescriptor->imageAspectRatio)));
            
            AAF_CHECK(pFileDescriptor->SetLength(track->sourceClip->length));
            AAF_CHECK(pFileDescriptor->SetSampleRate(convertRational(track->editRate)));
            
            AAF_CHECK(pCDCIDescriptor2->QueryInterface(IID_IAAFEssenceDescriptor, (void **)&pEssenceDescriptor));
        }
        else // SOUND_DATA_DEFINITION
        {
            AAF_CHECK(pCDPCMDescriptor->CreateInstance(IID_IAAFPCMDescriptor, (IUnknown **)&pPCMDescriptor));
            AAF_CHECK(pPCMDescriptor->QueryInterface(IID_IAAFSoundDescriptor, (void **)&pSoundDescriptor));
            AAF_CHECK(pPCMDescriptor->QueryInterface(IID_IAAFFileDescriptor, (void **)&pFileDescriptor));
            
            AAF_CHECK(pPCMDescriptor->Initialize());
            AAF_CHECK(pPCMDescriptor->SetBlockAlign((fileDescriptor->audioQuantizationBits + 7) / 8));
            AAF_CHECK(pPCMDescriptor->SetAverageBPS(audioSamplingRate.numerator * ((fileDescriptor->audioQuantizationBits + 7) / 8) / 
                audioSamplingRate.denominator));
            AAF_CHECK(pSoundDescriptor->SetChannelCount(1));
            AAF_CHECK(pSoundDescriptor->SetAudioSamplingRate(audioSamplingRate));
            // TODO: first set this value in MXF - do we know if it is locked or not? 
            // AAF_CHECK(pSoundDescriptor->SetIsLocked(kAAFTrue));
            AAF_CHECK(pSoundDescriptor->SetQuantizationBits(fileDescriptor->audioQuantizationBits));
            
            AAF_CHECK(pFileDescriptor->SetLength(track->sourceClip->length));
            AAF_CHECK(pFileDescriptor->SetSampleRate(convertRational(track->editRate)));
            
            AAF_CHECK(pPCMDescriptor->QueryInterface(IID_IAAFEssenceDescriptor, (void **)&pEssenceDescriptor));
        }

        AAF_CHECK(pFileSourceMob->SetEssenceDescriptor(pEssenceDescriptor));
    }        
}

void AAFFile::mapTapeSourceMob(SourcePackage* sourcePackage)
{
    IAAFSmartPointer<IAAFMob> pMob;
    IAAFSmartPointer<IAAFTapeDescriptor> pTapeDesc;
    IAAFSmartPointer<IAAFEssenceDescriptor> pEssDesc;
    IAAFSmartPointer<IAAFMobSlot> pMobSlot;
    IAAFDataDef* pDataDef = NULL;
    aafUInt32 maxSlotID = 1;
    aafRational_t pictureEditRate = {25, 1}; // if no picture then edit rate is ssumed to be 25/1 
    // TODO: need project edit rate filtered down to here 
    // TODO: we also not drop frame flag indicator if it is NTSC
    
    // return if the tape source package has already been mapped to a mob
    if (haveMob(convertUMID(sourcePackage->uid)))
    {
        return;
    }
    
    // create source mob
    AAF_CHECK(pCDSourceMob->CreateInstance(IID_IAAFSourceMob, (IUnknown **)&pTapeSourceMob));
    AAF_CHECK(pTapeSourceMob->QueryInterface(IID_IAAFMob, (void **)&pMob));
    AAF_CHECK(pMob->SetMobID(convertUMID(sourcePackage->uid)));
    AAF_CHECK(pMob->SetName(convertName(sourcePackage->name)));
    AAF_CHECK(pMob->SetCreateTime(convertTimestamp(sourcePackage->creationDate)));
    AAF_CHECK(pHeader->AddMob(pMob));

    // user comments
    mapUserComments(sourcePackage, pMob);
    
    // append tape descriptor        
    AAF_CHECK(pCDTapeDescriptor->CreateInstance(IID_IAAFTapeDescriptor, (IUnknown **)&pTapeDesc));
    AAF_CHECK(pTapeDesc->QueryInterface(IID_IAAFEssenceDescriptor, (void **)&pEssDesc));
    AAF_CHECK(pTapeSourceMob->SetEssenceDescriptor(pEssDesc));

    // append picture and sound slots
    vector<Track*>::const_iterator iter;
    for (iter = sourcePackage->tracks.begin(); iter != sourcePackage->tracks.end(); iter++)
    {
        Track* track = *iter;
        
        if (track->dataDef == PICTURE_DATA_DEFINITION)
        {
            pDataDef = pPictureDef;
            pictureEditRate = convertRational(track->editRate);
        }
        else // SOUND_DATA_DEFINITION
        {
            pDataDef = pSoundDef;
        }
        

        // create mob slot        
        AAF_CHECK(pTapeSourceMob->AddNilReference(track->id, track->sourceClip->length, 
            pDataDef, convertRational(track->editRate)));
        AAF_CHECK(pMob->LookupSlot(track->id, &pMobSlot));
        AAF_CHECK(pMobSlot->SetName(convertName(track->name)));
        if (track->number != 0)
        {
            AAF_CHECK(pMobSlot->SetPhysicalNum(track->number));
        }
        
        if (track->id > maxSlotID)
        {
            maxSlotID = track->id;
        }
    }
    
    // append timecode slot
    // Note: we assume the same was done in the MXF file
    aafTimecode_t startTimecode;
    startTimecode.startFrame = 0;
    startTimecode.drop = 0; // TODO: need drop frame indicator
    startTimecode.fps = (aafUInt16)(pictureEditRate.numerator / (double)pictureEditRate.denominator + 0.5);
    AAF_CHECK(pTapeSourceMob->AppendTimecodeSlot(pictureEditRate, maxSlotID + 1, startTimecode, 
        120 * 60 * 60 * pictureEditRate.numerator / pictureEditRate.denominator));
    AAF_CHECK(pMob->LookupSlot(maxSlotID + 1, &pMobSlot));
    AAF_CHECK(pMobSlot->SetName(L"TC1"));
    
}



int64_t prodauto::getStartTime(MaterialPackage* materialPackage, PackageSet& packages, Rational editRate)
{
    vector<Track*>::const_iterator iter;
    for (iter = materialPackage->tracks.begin(); iter != materialPackage->tracks.end(); iter++)
    {
        Track* track = *iter;
        
        SourcePackage dummy;
        dummy.uid = track->sourceClip->sourcePackageUID;
        PackageSet::iterator result = packages.find(&dummy);
        if (result != packages.end())
        {
            Package* package = *result;
            
            // were only interested in references to file SourcePackages
            if (package->getType() != SOURCE_PACKAGE || package->tracks.size() == 0)
            {
                continue;
            }
            SourcePackage* sourcePackage = dynamic_cast<SourcePackage*>(package);
            if (sourcePackage->descriptor->getType() != FILE_ESSENCE_DESC_TYPE)
            {
                continue;
            }
            
            // get the startPosition in editRate units
            Track* sourceTrack = *sourcePackage->tracks.begin();
            if (editRate == sourceTrack->editRate)
            {
                return sourceTrack->sourceClip->position;
            }
            else
            {
                double factor = editRate.numerator * sourceTrack->editRate.denominator / 
                    (double)(editRate.denominator * sourceTrack->editRate.numerator);
                return (int64_t)(sourceTrack->sourceClip->position * factor + 0.5);
            }
        }
    }
    
    PA_LOGTHROW(CreateAAFException, ("Could not determine package start time"));
}


