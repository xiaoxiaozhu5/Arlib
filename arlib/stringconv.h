#pragma once
#include "global.h"
#include "string.h"
#include <stdio.h>

inline string tostring(string s) { return s; }
inline string tostring(cstring s) { return s; }
inline string tostring(const char * s) { return s; }
inline string tostring(int val) { char ret[16]; sprintf(ret, "%i", val); return ret; }
inline string tostring(unsigned int val) { char ret[16]; sprintf(ret, "%u", val); return ret; }
#ifdef _WIN32
inline string tostring(size_t val) { char ret[16]; sprintf(ret, "%Iu", val); return ret; }
#else
inline string tostring(size_t val) { char ret[16]; sprintf(ret, "%zu", val); return ret; }
#endif
inline string tostring(float val) { char ret[32]; sprintf(ret, "%f", val); return ret; }
inline string tostring(time_t val) { char ret[32]; sprintf(ret, "%" PRIi64, (int64_t)val); return ret; }

template<typename T> inline T fromstring(cstring s);
template<> inline string fromstring<string>(cstring s) { return s; }
template<> inline cstring fromstring<cstring>(cstring s) { return s; }
//no const char *, their lifetime is unknowable

template<> inline int fromstring<int>(cstring s) { return strtol(s, NULL, 0); }
template<> inline long int fromstring<long int>(cstring s) { return strtol(s, NULL, 0); }
template<> inline unsigned int fromstring<unsigned int>(cstring s) { return strtoul(s, NULL, 0); }
template<> inline float fromstring<float>(cstring s) { return (float)strtod(s, NULL); }

template<> inline char fromstring<char>(cstring s) { return s[0]; }
template<> inline bool fromstring<bool>(cstring s) { return s=="true"; }
