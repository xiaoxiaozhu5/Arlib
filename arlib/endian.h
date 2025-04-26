#pragma once
#include "global.h"
#include "array.h"
#include <stdint.h>

//This file defines:
//readu_{le,be}{8,16,32,64,f32,f64}()
//  Reads and returns an X-endian uintN_t or float/double from the given pointer. Accepts misaligned input.
//  The 8bit ones are trivial, but exist for consistency.
//writeu_{le,be}{8,16,32,64,f32,f64}()
//  The inverse of readu; writes an X-endian uintN_t into the given pointer. Accepts misaligned input.
//pack_{le,be}{8,16,32,64,f32,f64}()
//  Like writeu, but instead of taking a pointer, it returns an sarray<uint8_t,N>.
//readu_{le,be}<T>()
//writeu_{le,be}<T>()
//  Calls the corresponding sized function.

#define END_BIG (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
forceinline uint8_t  readu_le8( const uint8_t* in) { return *in; }
forceinline uint8_t  readu_be8( const uint8_t* in) { return *in; }
forceinline uint16_t readu_le16(const uint8_t* in) { uint16_t ret; memcpy(&ret, in, 2); return END_BIG ? __builtin_bswap16(ret) : ret; }
forceinline uint16_t readu_be16(const uint8_t* in) { uint16_t ret; memcpy(&ret, in, 2); return END_BIG ? ret : __builtin_bswap16(ret); }
forceinline uint32_t readu_le32(const uint8_t* in) { uint32_t ret; memcpy(&ret, in, 4); return END_BIG ? __builtin_bswap32(ret) : ret; }
forceinline uint32_t readu_be32(const uint8_t* in) { uint32_t ret; memcpy(&ret, in, 4); return END_BIG ? ret : __builtin_bswap32(ret); }
forceinline uint64_t readu_le64(const uint8_t* in) { uint64_t ret; memcpy(&ret, in, 8); return END_BIG ? __builtin_bswap64(ret) : ret; }
forceinline uint64_t readu_be64(const uint8_t* in) { uint64_t ret; memcpy(&ret, in, 8); return END_BIG ? ret : __builtin_bswap64(ret); }

forceinline void writeu_le8( uint8_t* target, uint8_t  n) { *target = n; }
forceinline void writeu_be8( uint8_t* target, uint8_t  n) { *target = n; }
forceinline void writeu_le16(uint8_t* target, uint16_t n) { n = END_BIG ? __builtin_bswap16(n) : n; memcpy(target, &n, 2); }
forceinline void writeu_be16(uint8_t* target, uint16_t n) { n = END_BIG ? n : __builtin_bswap16(n); memcpy(target, &n, 2); }
forceinline void writeu_le32(uint8_t* target, uint32_t n) { n = END_BIG ? __builtin_bswap32(n) : n; memcpy(target, &n, 4); }
forceinline void writeu_be32(uint8_t* target, uint32_t n) { n = END_BIG ? n : __builtin_bswap32(n); memcpy(target, &n, 4); }
forceinline void writeu_le64(uint8_t* target, uint64_t n) { n = END_BIG ? __builtin_bswap64(n) : n; memcpy(target, &n, 8); }
forceinline void writeu_be64(uint8_t* target, uint64_t n) { n = END_BIG ? n : __builtin_bswap64(n); memcpy(target, &n, 8); }
#undef END_BIG

forceinline sarray<uint8_t,1> pack_le8( uint8_t  n) { sarray<uint8_t,1> ret; writeu_le8( ret.ptr(), n); return ret; }
forceinline sarray<uint8_t,1> pack_be8( uint8_t  n) { sarray<uint8_t,1> ret; writeu_be8( ret.ptr(), n); return ret; }
forceinline sarray<uint8_t,2> pack_le16(uint16_t n) { sarray<uint8_t,2> ret; writeu_le16(ret.ptr(), n); return ret; }
forceinline sarray<uint8_t,2> pack_be16(uint16_t n) { sarray<uint8_t,2> ret; writeu_be16(ret.ptr(), n); return ret; }
forceinline sarray<uint8_t,4> pack_le32(uint32_t n) { sarray<uint8_t,4> ret; writeu_le32(ret.ptr(), n); return ret; }
forceinline sarray<uint8_t,4> pack_be32(uint32_t n) { sarray<uint8_t,4> ret; writeu_be32(ret.ptr(), n); return ret; }
forceinline sarray<uint8_t,8> pack_le64(uint64_t n) { sarray<uint8_t,8> ret; writeu_le64(ret.ptr(), n); return ret; }
forceinline sarray<uint8_t,8> pack_be64(uint64_t n) { sarray<uint8_t,8> ret; writeu_be64(ret.ptr(), n); return ret; }

forceinline float  readu_lef32(const uint8_t* in) { return transmute<float >(readu_le32(in)); }
forceinline float  readu_bef32(const uint8_t* in) { return transmute<float >(readu_be32(in)); }
forceinline double readu_lef64(const uint8_t* in) { return transmute<double>(readu_le64(in)); }
forceinline double readu_bef64(const uint8_t* in) { return transmute<double>(readu_be64(in)); }
forceinline void writeu_lef32(uint8_t* target, float  n) { writeu_le32(target, transmute<uint32_t>(n)); }
forceinline void writeu_bef32(uint8_t* target, float  n) { writeu_be32(target, transmute<uint32_t>(n)); }
forceinline void writeu_lef64(uint8_t* target, double n) { writeu_le64(target, transmute<uint64_t>(n)); }
forceinline void writeu_bef64(uint8_t* target, double n) { writeu_be64(target, transmute<uint64_t>(n)); }
forceinline sarray<uint8_t,4> pack_lef32(float  n) { return pack_le32(transmute<uint32_t>(n)); }
forceinline sarray<uint8_t,4> pack_bef32(float  n) { return pack_be32(transmute<uint32_t>(n)); }
forceinline sarray<uint8_t,8> pack_lef64(double n) { return pack_le64(transmute<uint64_t>(n)); }
forceinline sarray<uint8_t,8> pack_bef64(double n) { return pack_be64(transmute<uint64_t>(n)); }

template<typename T> forceinline T readu_le(const uint8_t* in)
{
	static_assert(std::is_arithmetic_v<T>);
	if constexpr (std::is_same_v<bool,std::remove_cv_t<T>>) // transmute<bool>(2) is UB, and sizeof(bool) != 1 on PowerPC; override it
		return readu_le8(in);
	else if constexpr (sizeof(T) == 1) return transmute<T>(readu_le8( in));
	else if constexpr (sizeof(T) == 2) return transmute<T>(readu_le16(in));
	else if constexpr (sizeof(T) == 4) return transmute<T>(readu_le32(in));
	else if constexpr (sizeof(T) == 8) return transmute<T>(readu_le64(in));
	else static_assert(sizeof(T) < 0);
}
template<typename T> forceinline T readu_be(const uint8_t* in)
{
	static_assert(std::is_arithmetic_v<T>);
	if constexpr (std::is_same_v<bool,std::remove_cv_t<T>>)
		return readu_be8(in);
	else if constexpr (sizeof(T) == 1) return transmute<T>(readu_be8( in));
	else if constexpr (sizeof(T) == 2) return transmute<T>(readu_be16(in));
	else if constexpr (sizeof(T) == 4) return transmute<T>(readu_be32(in));
	else if constexpr (sizeof(T) == 8) return transmute<T>(readu_be64(in));
	else static_assert(sizeof(T) < 0);
}

template<typename T> forceinline void writeu_le(uint8_t* out, std::type_identity_t<T> val)
{
	static_assert(std::is_arithmetic_v<T>);
	if constexpr (std::is_same_v<bool,std::remove_cv_t<T>>)
		writeu_le8(out, (uint8_t)val);
	else if constexpr (sizeof(T) == 1) writeu_le8( out, transmute<uint8_t >(val));
	else if constexpr (sizeof(T) == 2) writeu_le16(out, transmute<uint16_t>(val));
	else if constexpr (sizeof(T) == 4) writeu_le32(out, transmute<uint32_t>(val));
	else if constexpr (sizeof(T) == 8) writeu_le64(out, transmute<uint64_t>(val));
	else static_assert(sizeof(T) < 0);
}
template<typename T> forceinline void writeu_be(uint8_t* out, std::type_identity_t<T> val)
{
	static_assert(std::is_arithmetic_v<T>);
	if constexpr (std::is_same_v<bool,std::remove_cv_t<T>>)
		writeu_be8(out, (uint8_t)val);
	else if constexpr (sizeof(T) == 1) writeu_be8( out, transmute<uint8_t >(val));
	else if constexpr (sizeof(T) == 2) writeu_be16(out, transmute<uint16_t>(val));
	else if constexpr (sizeof(T) == 4) writeu_be32(out, transmute<uint32_t>(val));
	else if constexpr (sizeof(T) == 8) writeu_be64(out, transmute<uint64_t>(val));
	else static_assert(sizeof(T) < 0);
}
