
#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef BOOL
typedef int BOOL;
#endif

#ifdef _WIN32
#define INT64_FORMAT  "I64i"
#define UINT64_FORMAT "I64u"
#define INT64_HEX_FORMAT  "I64x"
#else
#define INT64_FORMAT  "lli"
#define UINT64_FORMAT "llu"
#define INT64_HEX_FORMAT  "llx"
#endif

#ifndef int64
#if defined(_MSC_VER) || defined(__BORLANDC__)
  typedef __int64 int64;
  typedef unsigned __int64 uint64;
#else
  typedef long long int int64;
  typedef unsigned long long int uint64;
#endif
#endif
