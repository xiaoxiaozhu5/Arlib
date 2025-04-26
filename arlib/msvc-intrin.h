#pragma once
// Makes a few GCC builtins compile under MSVC. Performance is not guaranteed.
#ifdef _MSC_VER
#define __attribute__(x) __attribute_unwrap x
#define __attribute_unwrap(x) __attribute_##x // __attribute_target("sse2") isn't a preprocessing token, but it works anyways
#define __attribute_target(x) // can't do anything with those string literals, but msvc has no equivalent of this attribute anyways
// attribute(packed) is unimplemented, msvc requires pragma packed

#define __builtin_bswap16 _byteswap_ushort
#define __builtin_bswap32 _byteswap_ulong
#define __builtin_bswap64 _byteswap_uint64

#define __ORDER_LITTLE_ENDIAN__ 1234
#define __ORDER_BIG_ENDIAN__ 4321
#if defined(_M_IX86) || defined(_M_AMD64) || defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ALPHA) || defined(_M_IA64)
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#elif defined(_M_PPC)
#define __BYTE_ORDER__ __ORDER_BIG_ENDIAN__
#endif

#endif
