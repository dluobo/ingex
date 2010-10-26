// integer_types.h
// For platform independence without use of ACE headers.

#ifndef integer_types_h
#define integer_types_h

#ifndef INTTYPES_DEFINED
#define INTTYPES_DEFINED

#ifdef _MSC_VER

typedef unsigned char uint8_t;
typedef signed char int8_t;

typedef unsigned short uint16_t;
typedef short int16_t;

typedef unsigned int uint32_t;
typedef int int32_t;

typedef unsigned __int64 uint64_t;
typedef __int64 int64_t;

#ifdef __STDC_FORMAT_MACROS
#define PRId64 "I64d"
#define PRIu64 "I64u"
#endif

#else

#include <inttypes.h>

#endif

#endif //#ifndef INTTYPES_DEFINED

#endif //#ifndef integer_types_h

