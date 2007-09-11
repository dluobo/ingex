#ifndef __PRODAUTO_INGEX_PLAYER_EXCEPTION_H__
#define __PRODAUTO_INGEX_PLAYER_EXCEPTION_H__


#include <string>


namespace prodauto
{


class IngexPlayerException
{
public:
    IngexPlayerException();
    IngexPlayerException(const char* format, ...);
    virtual ~IngexPlayerException();
    
    std::string getMessage() const;
    
protected:
    std::string _message;
};


};



#endif



