#pragma once
#include "global.h"
#include "array.h"
#include "random.h"

// Hash values are guaranteed stable within the process, but nothing else. Do not persist them outside the process.
// They are allowed to change along with the build target, Arlib version, build time, kernel version, etc.
// They are only expected to be unique, not high entropy; entropy can be improved with random_t::oracle.
// Don't rely on them for any security-related purpose either.

template<typename T>
auto hash(T val) requires (std::is_integral_v<T>)
{
	if constexpr (sizeof(T) == 8)
		return (uint64_t)val;
	else
		return (uint32_t)val;
}
template<typename T>
auto hash(const T& val) requires requires { val.hash(); }
{
	return val.hash();
}
size_t hash(const uint8_t * val, size_t n);
static inline size_t hash(const char * val)
{
	return hash((uint8_t*)val, strlen(val));
}
static inline size_t hash(const bytesr& val)
{
	return hash(val.ptr(), val.size());
}
static inline size_t hash(const bytesw& val)
{
	return hash(val.ptr(), val.size());
}
static inline size_t hash(const bytearray& val)
{
	return hash(val.ptr(), val.size());
}

class hash_combiner {
public:
	// same algorithm and numbers as python tuple hash, except I removed the last few steps
	static const size_t first = (sizeof(size_t) > 4 ? 2870177450012600261u : 374761393u);
	static size_t combine(size_t prev, size_t hash)
	{
		if constexpr (sizeof(size_t) > 4)
		{
			size_t val = prev;
			val += hash * 14029467366897019727u;
			val = ((val << 31) | (val >> 33));
			val *= 11400714785074694791u;
			return val;
		}
		else
		{
			size_t val = prev;
			val += hash * 2246822519u;
			val = ((val << 13) | (val >> 19));
			val *= 2654435761u;
			return val;
		}
	}
};

class pointer_hasher {
	pointer_hasher() = delete;
public:
	template<typename T>
	static size_t hash(T* ptr) { return (uintptr_t)ptr; }
};
class arrayview_hasher {
	arrayview_hasher() = delete;
public:
	template<typename T>
	static size_t hash(arrayview<T> arr) requires (std::is_integral_v<T>) { return ::hash(arr.template transmute<uint8_t>()); }
	template<typename T>
	static size_t hash(arrayview<T> arr) requires (!std::is_integral_v<T>)
	{
		size_t ret = hash_combiner::first;
		for (const T& elem : arr)
			ret = hash_combiner::combine(ret, ::hash(elem));
		return ret;
	}
};
