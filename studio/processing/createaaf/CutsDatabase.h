#ifndef __CUTS_DATABASE_H__
#define __CUTS_DATABASE_H__


#include <string>
#include <vector>
#include <set>

#include <DataTypes.h>


namespace prodauto
{

class CutsDatabaseEntry
{
public:
    CutsDatabaseEntry() : timecode(0) {}
    
    std::string sourceId;
    Date date;
    int64_t timecode;
};

class CutInfo
{
public:
    CutInfo() : position(0) {}
    
    int64_t position;
    std::string source;
};


class CutsDatabase
{
public:
    CutsDatabase(std::string filename);
    ~CutsDatabase();
    
    std::vector<CutInfo> getCuts(Date creationDate, int64_t startTimecode, 
        std::set<std::string> sources, std::string defaultSource);
    
private:
    void loadEntries(std::string filename);

    std::vector<CutsDatabaseEntry> _entries;
};



};


#endif

