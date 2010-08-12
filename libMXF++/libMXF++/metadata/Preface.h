#ifndef __MXFPP_PREFACE_H__
#define __MXFPP_PREFACE_H__



#include <libMXF++/metadata/base/PrefaceBase.h>
#include <libMXF++/metadata/MaterialPackage.h>
#include <libMXF++/metadata/SourcePackage.h>


namespace mxfpp
{


class Preface : public PrefaceBase
{
public:
    friend class MetadataSetFactory<Preface>;

public:
    Preface(HeaderMetadata* headerMetadata);
    virtual ~Preface();


    GenericPackage* findPackage(mxfUMID package_uid) const;
    MaterialPackage* findMaterialPackage() const;
    std::vector<SourcePackage*> findFileSourcePackages() const;

protected:
    Preface(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
