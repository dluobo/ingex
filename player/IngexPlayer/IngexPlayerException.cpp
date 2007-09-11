#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>


#include "IngexPlayerException.h"

using namespace std;
using namespace prodauto;


IngexPlayerException::IngexPlayerException()
{}

IngexPlayerException::IngexPlayerException(const char* format, ...)
{
    char message[512];
    
    va_list varg;
    va_start(varg, format);
#if defined(_MSC_VER)
    _vsnprintf(message, 512, format, varg);
#else
    vsnprintf(message, 512, format, varg);
#endif
    va_end(varg);
    
    _message = message;
}


IngexPlayerException::~IngexPlayerException()
{}

string IngexPlayerException::getMessage() const
{
    return _message;
}


