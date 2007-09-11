#ifndef __PRODAUTO_MACROS_H__
#define __PRODAUTO_MACROS_H__


#include <macros.h>


#define THROW_EXCEPTION(err) \
    ml_log_error err; \
    ml_log_error("  near %s:%d\n", __FILE__, __LINE__); \
    throw IngexPlayerException err;


#define CHK_OTHROW(cmd) \
    if (!(cmd)) \
    { \
        THROW_EXCEPTION(("'%s' failed\n", #cmd)); \
    }
    
#define CHK_OTHROW_MSG(cmd, err) \
    if (!(cmd)) \
    { \
        THROW_EXCEPTION(err); \
    }
    
    
#define SAFE_DELETE(var) \
    delete *var; \
    *var = 0;

#define SAFE_ARRAY_DELETE(var) \
    delete [] *var; \
    *var = 0;

    
#endif


