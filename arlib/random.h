#pragma once
#include "global.h"
#include "thread/atomic.h"
#ifdef __linux__
#include <unistd.h>
#endif

#if defined(_WIN32)
#include <windows.h> // ntsecapi.h doesn't include its dependencies properly
#include <ntsecapi.h>
#endif

// Yields cryptographically secure random numbers. Max size is 256. Despite the return value, it always succeeds.
static bool rand_secure(void* out, size_t n)
{
#if defined(__linux__)
	return getentropy(out, n) == 0; // can't fail unless kernel < 3.17 (oct 2014), bad pointer, size > 256, or strange seccomp/ptrace/etc
#elif defined(_WIN32)
	// documented on msdn as having no import library, but works in mingw
	// msdn doesn't claim it's cryptographically secure, but everything else that talks about it treats it as such
	// msdn also says I should use CryptGenRandom, but that requires creating a random number provider and offers no clear benefits
	// BCryptGenRandom exists, but doesn't help until 7+, is in a rarer DLL than RtlGenRandom, and offers no clear benefits either
	return RtlGenRandom(out, n);
#else
	#error unsupported
#endif
}

template<bool is_atomic>
class random_base_t : nocopy {
public:
	// This is a random oracle. Given any ctr, it returns a random number.
	// There's a bunch of alternate keys available at https://squaresrng.wixsite.com/rand
	// but there's no real reason to use something else, unless you need a different sequence and want to start the counter at zero.
	// It is not cryptographically secure.
	forceinline static uint32_t oracle32(uint64_t ctr, uint64_t key = 0xc58efd154ce32f6d)
	{
		// this is based on the Squares RNG https://arxiv.org/abs/2004.06278
		// __builtin_bswap64 is better than rotating by 32, allowing me to remove a few rounds
		// with one round less, it passes the BigCrush test suite, and PractRand up to 16TB; the extra round is needed for 32T
		// if it needs a name, it's the Serauqs RNG (it's just Squares backwards, to represent the bswaps putting things backwards)
		uint64_t x = ctr * key;
		uint64_t y = x;
		uint64_t z = y + key;
		x = __builtin_bswap64(x*x + y);
		x = __builtin_bswap64(x*x + z);
		return __builtin_bswap64(x*x + y);
	}
	// Like the above, but it returns a 64bit number.
	// They are different random oracles; it's safe to mix them (unless you use the same counter and the same key - that case is untested).
	forceinline static uint64_t oracle64(uint64_t ctr, uint64_t key = 0xfcbd6e154bf53ed9)
	{
		// tests fail immediately if the last shift is replaced with a bswap
		uint64_t x = ctr * key;
		uint64_t y = x;
		uint64_t z = y + key;
		x = __builtin_bswap64(x*x + y);
		x = __builtin_bswap64(x*x + z);
		uint64_t t = x*x + y;
		x = __builtin_bswap64(t);
		return t ^ ((x*x + z) >> 32);
	}
private:
	// Hoisting the multiplication like this saves one multiplication per invocation, saving some time.
	// The drawback is the key becomes hardcoded, and the same between the two, but no big deal.
	static constexpr uint64_t standard_key = 0xc58efd154ce32f6d;
	uint64_t state;
	forceinline uint64_t next_state()
	{
		if (is_atomic)
			return lock_incr<lock_loose>(&state) * standard_key; // I think atomic add is slower than atomic increment + 64bit mul
		else
			return state += standard_key;
	}
	forceinline static uint32_t oracle32_keyed(uint64_t ctr_keyed)
	{
		uint64_t x = ctr_keyed;
		uint64_t y = x;
		uint64_t z = y + standard_key;
		x = __builtin_bswap64(x*x + y);
		x = __builtin_bswap64(x*x + z);
		return __builtin_bswap64(x*x + y);
	}
	forceinline static uint64_t oracle64_keyed(uint64_t ctr_keyed)
	{
		uint64_t x = ctr_keyed;
		uint64_t y = x;
		uint64_t z = y + standard_key;
		x = __builtin_bswap64(x*x + y);
		x = __builtin_bswap64(x*x + z);
		uint64_t t = x*x + y;
		x = __builtin_bswap64(t);
		return t ^ ((x*x + z) >> 32);
	}
public:
	
	// CSPRNG is overkill, but the alternative is time, which has a few drawbacks of its own. Better overkill than underkill.
	void seed() { rand_secure(&state, sizeof(state)); }
	void seed(uint64_t num) { state = oracle64(num); } // randomize input, so consecutive seeds don't return almost-same sequence
	
	forceinline uint32_t rand32() { return oracle32_keyed(next_state()); }
	forceinline uint64_t rand64() { return oracle64_keyed(next_state()); }
	uint32_t rand_mod(uint32_t limit);
	uint64_t rand_mod(uint64_t limit);
	
	uint32_t operator()(uint32_t mod) { return rand_mod(mod); }
	uint64_t operator()(uint64_t mod) { return rand_mod(mod); }
	// gives bad answers for negative input, but there are no good answers for that
	uint32_t operator()(int mod) { return rand_mod((uint32_t)mod); }
};
class random_t : public random_base_t<false> {
public:
	random_t() { seed(); }
	random_t(uint64_t num) { seed(num); } // Not recommended unless you need predictable output.
};
#ifdef ARLIB_THREAD
extern random_base_t<true> g_rand; // g_rand is thread safe, random_t is not
#else
extern random_base_t<false> g_rand;
#endif
