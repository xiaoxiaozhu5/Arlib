#pragma once
#include "global.h"

template<typename T>
typename std::enable_if<std::is_integral<T>::value, size_t>::type hash(T val)
{
	return val;
}
template<typename T>
typename std::enable_if<std::is_class<T>::value, size_t>::type hash(T val)
{
	return val.hash();
}
static inline size_t hash(const char * val, size_t n)
{
	size_t hash = 5381;
	while (n--)
	{
		hash = hash*31 ^ *val;
		val++;
	}
	return hash;
}
static inline size_t hash(const char * val)
{
	return hash(val, strlen(val));
}

//these two are reversible, but there's no reversal because why should I
inline uint32_t hash_shuffle(uint32_t val)
{
	//https://code.google.com/p/smhasher/wiki/MurmurHash3
	val ^= val >> 16;
	val *= 0x85ebca6b;
	val ^= val >> 13;
	val *= 0xc2b2ae35;
	val ^= val >> 16;
	return val;
}

inline uint64_t hash_shuffle(uint64_t val)
{
	//http://zimbry.blogspot.se/2011/09/better-bit-mixing-improving-on.html Mix13
	val ^= val >> 30;
	val *= 0xbf58476d1ce4e5b9;
	val ^= val >> 27;
	val *= 0x94d049bb133111eb;
	val ^= val >> 31;
	return val;
}
