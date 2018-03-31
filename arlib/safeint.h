#pragma once
#include "global.h"
#include <limits>

#define ALLOPER(x) \
	x(+, +=) x(-, -=) x(*, *=) x(/, /=) x(%, %=) \
	x(|, |=) x(^, ^=) x(<<, <<=) x(>>, >>=)

//Overflow-safe integer class.
//If an operation would overflow, it gets a special sentinel value instead, and all further operations return that.
template<typename T> class safeint {
	T data;
	
#define HANDLE_BASE(stype, utype) \
	static inline bool addov_i(utype a, utype b, utype* c) \
	{ \
		*c = a+b; /* rely on unsigned overflow wrapping */ \
		return *c < a; \
	} \
	static inline bool subov_i(utype a, utype b, utype* c) \
	{ \
		*c = a-b; \
		return (b > a); \
	} \
	static inline bool mulov_i(utype a, utype b, utype* c) \
	{ \
		*c = a*b; \
		return a!=0 && *c/a!=b; \
	} \
	static inline bool lslov_i(utype a, utype b, utype* c) \
	{ \
		if (b >= sizeof(utype)*8) return true; \
		*c = a<<b; \
		return (*c>>b != a); \
	} \
	static inline bool addov_i(stype a, stype b, stype* c) \
	{ \
		*c = (utype)a+(utype)b; \
		return (a>0 && b>0 && *c<0) || (a<0 && b<0 && *c>0); \
	} \
	static inline bool subov_i(stype a, stype b, stype* c) \
	{ \
		*c = (utype)a-(utype)b; \
		return (a>0 && b<0 && *c<0) || (a<0 && b>0 && *c>0); \
	} \
	static inline bool mulov_i(stype a, stype b, stype* c) \
	{ \
		stype min = std::numeric_limits<stype>::min(); \
		stype max = std::numeric_limits<stype>::max(); \
		/* not sure if this can be simplified */ \
		if (a<0 && b<0 && max/a > b) return true; \
		if (a<0 && b>0 && min/a < b) return true; \
		if (a>0 && b<0 && min/a > b) return true; \
		if (a>0 && b>0 && max/a < b) return true; \
		*c = a*b; \
		return false; \
	} \
	static inline bool lslov_i(stype a, stype b, stype* c) \
	{ \
		if (b<0 || a<0) return true; \
		if (b >= (stype)sizeof(stype)*8) return true; \
		*c = (a << b); \
		return (*c>>b != a); \
	}
	HANDLE_BASE(signed int,       unsigned int)
	HANDLE_BASE(signed long,      unsigned long)
	HANDLE_BASE(signed long long, unsigned long long)
	
#define HANDLE_EXT(name, op, type, ext) \
	static inline bool name(type a, type b, type* c) \
	{ \
		if (sizeof(type) == sizeof(ext)) return name((ext)a, (ext)b, (ext*)c); \
		if (1 op 8 == 256) /* op is operator<< */ \
		{ \
			/* can go bigger than twice the base type's size */ \
			if (a < 0 || b < 0) return true; \
			if (b >= (type)sizeof(type)*8) return true; \
		} \
		if ((ext)a op (ext)b != (type)(a op b)) return true; \
		*c = (ext)a op (ext)b; \
		return false; \
	}
HANDLE_EXT(addov_i, +, signed short, signed int)
HANDLE_EXT(addov_i, +, signed char,  signed int)
HANDLE_EXT(addov_i, +, unsigned short, unsigned int)
HANDLE_EXT(addov_i, +, unsigned char,  unsigned int)
HANDLE_EXT(subov_i, -, signed short, signed int)
HANDLE_EXT(subov_i, -, signed char,  signed int)
HANDLE_EXT(subov_i, -, unsigned short, unsigned int)
HANDLE_EXT(subov_i, -, unsigned char,  unsigned int)
HANDLE_EXT(mulov_i, *, signed short, signed int)
HANDLE_EXT(mulov_i, *, signed char,  signed int)
HANDLE_EXT(mulov_i, *, unsigned short, unsigned int)
HANDLE_EXT(mulov_i, *, unsigned char,  unsigned int)
HANDLE_EXT(lslov_i,<<, signed short, signed int)
HANDLE_EXT(lslov_i,<<, signed char,  signed int)
HANDLE_EXT(lslov_i,<<, unsigned short, unsigned int)
HANDLE_EXT(lslov_i,<<, unsigned char,  unsigned int)
#undef HANDLE_EXT
	
public:
#ifndef __has_builtin
#define __has_builtin(x) false
#endif
#if __has_builtin(__builtin_add_overflow) || __GNUC__ >= 5
#define addov_i __builtin_add_overflow
#define subov_i __builtin_sub_overflow
#define mulov_i __builtin_mul_overflow
#endif
	//Returns true if the result overflowed, otherwise false.
	//lslov<uint32_t>(0, 32) returns overflow, since 0<<32 is undefined behavior.
	//lslov<int32_t>(-1, 1) returns overflow for the same reason.
	//If overflow, *c is undefined.
	static bool addov(T a, T b, T* c) { return addov_i(a, b, c); }
	static bool subov(T a, T b, T* c) { return subov_i(a, b, c); }
	static bool mulov(T a, T b, T* c) { return mulov_i(a, b, c); }
	static bool lslov(T a, T b, T* c) { return lslov_i(a, b, c); }
#undef addov_i
#undef subov_i
#undef mulov_i
	
	//Should give results identical to the above.
	static bool addov_nobuiltin(T a, T b, T* c) { return addov_i(a, b, c); }
	static bool subov_nobuiltin(T a, T b, T* c) { return subov_i(a, b, c); }
	static bool mulov_nobuiltin(T a, T b, T* c) { return mulov_i(a, b, c); }
	static bool lslov_nobuiltin(T a, T b, T* c) { return lslov_i(a, b, c); }
	
#define addov_i you screwed up
	
	static const T invalid = T(-1)<0 ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
	//static const T min = T(-1)<0 ? invalid+1 : 0;
	//static const T max = T(-1)<0 ? -min : T(-2);
	
	safeint() : data(0) {}
	template<typename U> safeint(U val) { if (T(val)==val) data=val; else data=invalid; }
	bool valid() { return data!=invalid; }
	T val() { return data; }
	
	safeint<T>& operator=(safeint<T> i) { data=i.val(); return *this; }
	
	safeint<T> operator++(int) { safeint<T> r = *this; *this+=1; return r; }
	safeint<T> operator--(int) { safeint<T> r = *this; *this-=1; return r; }
	safeint<T>& operator++() { *this+=1; return *this; }
	safeint<T>& operator--() { *this+=1; return *this; }
	
#define OP(op, ope) \
	safeint<T>& operator ope(safeint<T> i) { *this = *this op i; return *this; }
ALLOPER(OP)
#undef OP
	
	
	safeint<T> operator+(safeint<T> b)
	{
		if (!valid() || !b.valid()) return invalid;
		T ret;
		if (addov(val(), b.val(), &ret)) return invalid;
		else return ret;
	}
	safeint<T> operator-(safeint<T> b)
	{
		if (!valid() || !b.valid()) return invalid;
		T ret;
		if (subov(val(), b.val(), &ret)) return invalid;
		else return ret;
	}
	safeint<T> operator*(safeint<T> b)
	{
		if (!valid() || !b.valid()) return invalid;
		T ret;
		if (mulov(val(), b.val(), &ret)) return invalid;
		else return ret;
	}
	safeint<T> operator/(safeint<T> b)
	{
		if (!valid() || !b.valid()) return invalid;
		//overflows in division throw SIGFPE rather than truncating
		return val()/b.val();
	}
	safeint<T> operator%(safeint<T> b)
	{
		if (!valid() || !b.valid()) return invalid;
		//like division, overflow doesn't truncate
		return val()%b.val();
	}
	safeint<T> operator&(safeint<T> b)
	{
		if (!valid() || !b.valid()) return invalid;
		//can't overflow
		return val()&b.val();
	}
	safeint<T> operator|(safeint<T> b)
	{
		if (!valid() || !b.valid()) return invalid;
		//can't overflow (okay, it can become ::invalid, but that's fine)
		return val()|b.val();
	}
	safeint<T> operator^(safeint<T> b)
	{
		if (!valid() || !b.valid()) return invalid;
		//can't overflow
		return val()^b.val();
	}
	safeint<T> operator<<(safeint<T> b)
	{
		if (!valid() || !b.valid()) return invalid;
		T ret;
		if (lslov(val(), b.val(), &ret)) return invalid;
		else return ret;
	}
	safeint<T> operator>>(safeint<T> b)
	{
		if (!valid() || !b.valid()) return invalid;
		//can't overflow
		return val()>>b.val();
	}
	
	bool operator==(safeint<T> b)
	{
		if (!valid() || !b.valid()) return false;
		return val()==b.val();
	}
	bool operator!=(safeint<T> b)
	{
		if (!valid() || !b.valid()) return true; // match NaN
		return val()!=b.val();
	}
	bool operator<(safeint<T> b)
	{
		if (!valid() || !b.valid()) return false;
		return val()<b.val();
	}
	bool operator<=(safeint<T> b)
	{
		if (!valid() || !b.valid()) return false;
		return val()<=b.val();
	}
	bool operator>(safeint<T> b)
	{
		if (!valid() || !b.valid()) return false;
		return val()>b.val();
	}
	bool operator>=(safeint<T> b)
	{
		if (!valid() || !b.valid()) return false;
		return val()>=b.val();
	}
};

#define OP(op, ope) \
	template<typename T, typename U> safeint<T> operator op(U a, safeint<T> b) { return safeint<T>(a) op b; } \
	template<typename T, typename U> safeint<T> operator op(safeint<T> a, U b) { return b op safeint<T>(a); }
ALLOPER(OP)
OP(==, _)
OP(!=, _)
OP(<, _)
OP(<=, _)
OP(>, _)
OP(>=, _)
#undef OP

#undef ALLOPER
