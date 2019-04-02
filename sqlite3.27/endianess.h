#ifndef ENDIANESS_H
#define ENDIANESS_H

#ifdef WIN32
#define __LITTLE_ENDIAN  1234
#define __BIG_ENDIAN     4321
#define __BYTE_ORDER   __LITTLE_ENDIAN
#else
#ifndef __BYTE_ORDER
// on android we avoid the inclusion of htonx functions disabling the processing of _SYS_ENDIAN_H_
#define _SYS_ENDIAN_H_
#define _LITTLE_ENDIAN   1234
#define _BIG_ENDIAN      4321
#include <machine/endian.h>
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#define __BIG_ENDIAN    _BIG_ENDIAN
#define __BYTE_ORDER    _BYTE_ORDER
#endif
#endif

#ifndef __BYTE_ORDER
#error "__BYTE_ORDER not defined"
#endif
#ifndef __BIG_ENDIAN
#error "__BIG_ENDIAN not defined"
#endif
#ifndef __LITTLE_ENDIAN
#error "__LITTLE_ENDIAN not defined"
#endif
#if __BIG_ENDIAN == __LITTLE_ENDIAN
#error "__BIG_ENDIAN == __LITTLE_ENDIAN"
#endif

#endif // ENDIANESS_H
