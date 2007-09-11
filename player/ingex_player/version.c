#include "version.h"



const char* get_player_version()
{
    static const char* playerVersion = "version 0.91";
    
    return playerVersion;
}

const char* get_player_build_timestamp()
{
    static const char* playerBuildTimestamp = __DATE__ " " __TIME__;
    
    return playerBuildTimestamp;
}



