#include "global.h"

void malloc_fail(size_t size)
{
	if (size > 0) printf("malloc failed, size %" PRIuPTR "\n", size);
	else puts("malloc failed, size unknown");
	abort();
}

//these are the only libstdc++ functions I use. if I reimplement them, I don't need that library at all,
// saving several hundred kilobytes and a DLL
//(doesn't matter on linux where libstdc++ already exists, also Valgrind hates it so keep them disabled on Linux)
#ifdef _WIN32
#error check what -fno-exceptions does; when exceptions are enabled, libstdc++ is already included, so importing these symbols does no harm
#error also check whether they can be inlined into global.h, like malloc_check
void* operator new(size_t n) { return malloc(n); }
void* operator new[](size_t n) { return malloc(n); }
void operator delete(void * p) { free(p); }
void operator delete[](void * p) { free(p); }
#if __cplusplus >= 201402
void operator delete(void * p, size_t n) { free(p); }
void operator delete[](void * p, size_t n) { free(p); }
#endif
extern "C" void __cxa_pure_virtual();
extern "C" void __cxa_pure_virtual() { puts("__cxa_pure_virtual"); abort(); }
#endif

#include "test.h"

test("bitround", "", "")
{
	assert_eq(bitround((unsigned)1), 1);
	assert_eq(bitround((unsigned)2), 2);
	assert_eq(bitround((unsigned)3), 4);
	assert_eq(bitround((unsigned)4), 4);
	assert_eq(bitround((unsigned)640), 1024);
	assert_eq(bitround((unsigned)0x7FFFFFFF), 0x80000000);
	assert_eq(bitround((unsigned)0x80000000), 0x80000000);
	assert_eq(bitround((signed)1), 1);
	assert_eq(bitround((signed)2), 2);
	assert_eq(bitround((signed)3), 4);
	assert_eq(bitround((signed)4), 4);
	assert_eq(bitround((signed)640), 1024);
	assert_eq(bitround((signed)0x3FFFFFFF), 0x40000000);
	assert_eq(bitround((signed)0x40000000), 0x40000000);
}