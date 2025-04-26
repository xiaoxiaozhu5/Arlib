#pragma once
#include "global.h"
#include <time.h>
#ifdef __linux__
#include "file.h"
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#endif
// This is basically a grab-bag of assorted rarely used tools.

#if defined(_WIN32)
#define DLLEXPORT extern "C" __declspec(dllexport)
#elif defined(__GNUC__)
#define DLLEXPORT extern "C" __attribute__((__visibility__("default")))
#endif

#ifdef __i386__
// needs some extra shenanigans to kill the stdcall @12 suffix
#define DLLEXPORT_STDCALL(ret, name, args) \
	__asm__(".pushsection .drectve; .ascii \" -export:" #name "\"; .popsection"); \
	extern "C" ret __stdcall name args __asm__("_" #name); \
	extern "C" ret __stdcall name args
#else
#define DLLEXPORT_STDCALL(ret, name, args) DLLEXPORT ret __stdcall name args
#endif

class dylib : nocopy {
	void* handle = NULL;
	
public:
	dylib() = default;
	dylib(const char * filename) { init(filename); }
	
	// Does not permit being called on an initialized object. Deinit it first.
	bool init(const char * filename);
	bool inited() { return handle; }
	
	// Will return NULL if initialization failed.
	void* sym_ptr(const char * name);
	// Separate function because
	//  The ISO C standard does not require that pointers to functions can be cast back and forth to pointers to data.
	//  -- POSIX dlsym, http://pubs.opengroup.org/onlinepubs/009695399/functions/dlsym.html#tag_03_112_08
	// But the trivial implementation works on everything I'm aware of.
	// (C spec does guarantee you can cast every function pointer to any other.)
	funcptr sym_func(const char * name)
	{
		static_assert(sizeof(void*) == sizeof(funcptr));
		return (funcptr)sym_ptr(name);
	}
	template<typename T>
	auto sym(const char * name)
	{
		static_assert(std::is_function_v<T> || std::is_pointer_v<T>);
		if constexpr (std::is_function_v<T>) return (T*)sym_func(name);
		else if constexpr (std::is_function_v<std::remove_pointer_t<T>>) return (T)sym_func(name);
		else return (T)sym_ptr(name);
	}
	template<auto T>
	auto sym(const char * name)
	{
		return sym<decltype(T)>(name);
	}
	// Usage recommendation:
	// IDirect3D9* ret = d3d9.sym<Direct3DCreate9>("Direct3DCreate9")(D3D_SDK_VERSION);
	// auto Direct3DCreate9 = d3d9.sym< ::Direct3DCreate9>("Direct3DCreate9");
	// If you need multiple symbols, using DECL_DYLIB_T will save you a bit of typing.
	
	//Fetches multiple symbols. 'names' must be a NUL-separated list of names, terminated with a blank one.
	// This is easiest done by using multiple NUL-terminated strings, and let compiler append another NUL.
	// For example libc.sym_multi(whatever, "isalpha\0isdigit\0isalnum\0")
	//Returns whether all of them were successfully fetched. If not (even if init failed), failures are NULL; all are still attempted.
	//Not recommended for public use; DECL_DYLIB_T is a lot more convenient.
	bool sym_multi(funcptr* out, const char * names);
	
	void deinit();
	~dylib() { deinit(); }
};

#define DECL_DYLIB_MEMB(prefix, name) decltype(::prefix##name)* name;
#define DECL_DYLIB_RESET(prefix, name) name = nullptr;
#define DECL_DYLIB_NAME(prefix, name) #prefix #name "\0"
#define DECL_DYLIB_PREFIX_T(name, prefix, ...) \
	class name { \
	public: \
		PPFOREACH_A(DECL_DYLIB_MEMB, prefix, __VA_ARGS__); \
		\
		name() { PPFOREACH_A(DECL_DYLIB_RESET, prefix, __VA_ARGS__) } \
		name(const char * filename) { init(filename); } \
		bool init(const char * filename) /* TODO: what if 'init' is also one of the imported symbols? */ \
		{ \
			_internal_dylib.init(filename); \
			/* call this even on failure, to ensure members are nulled */ \
			return _internal_dylib.sym_multi((funcptr*)this, PPFOREACH_A(DECL_DYLIB_NAME, prefix, __VA_ARGS__)); \
		} \
		bool inited() { return _internal_dylib.inited(); } \
	private: \
		dylib _internal_dylib; \
	}
//The intended usecase of the prefixed one is a DLL exporting multiple functions, for example isalpha, isdigit, and isalnum.
//DECL_DYLIB_PREFIX_T(is_t, is, alpha, digit, alnum);
//is_t is("libc.so.6");
//if (is.alpha('a')) {}
//If your functions don't have any plausible prefix, or you want to include the prefix, feel free to instead use
#define DECL_DYLIB_T(name, ...) DECL_DYLIB_PREFIX_T(name, , __VA_ARGS__)
//which is just the above without the second argument.

// This one is like the above two, but caller is expected to initialize it,
//  probably because caller needs it initialized via something other than a dylib.
// raw_names() and raw_target() are valid arguments to dylib::sym_multi(), but caller is welcome to pass them somewhere else.
#define DECL_DYLIB_RAW_T(name, prefix, ...) \
	class name { \
	public: \
		PPFOREACH_A(DECL_DYLIB_MEMB, prefix, __VA_ARGS__); \
		const char * raw_names() { return PPFOREACH_A(DECL_DYLIB_NAME, prefix, __VA_ARGS__); } \
		funcptr * raw_target() { return (funcptr*)this; } \
	}


void debug_log_exedir(); // Opens the debug log at the executable's directory. Will affect all applicable of the following.
const uint8_t * debug_build_id(); // Returns a 16-byte build ID.
void debug_log_exe_version(const char * friendly_name); // Prints build ID and some other stuff to the debug log.
bool debug_break(const char * text); // If the process is run under a debugger, this triggers a breakpoint. Returns whether it did anything.
void debug_log(const char * text); // This prints to stderr and a nearby file. The string should have a \n suffix.
void debug_warn(const char * text); // Triggers a breakpoint if possible. If not, calls debug_log().
[[noreturn]] void debug_fatal(const char * text); // Same as debug_warn(), then terminates the program.
void debug_log_stack(const char * text); // These three are same as the above, but also print a stack trace, if possible.
void debug_warn_stack(const char * text);
[[noreturn]] void debug_fatal_stack(const char * text);
void debug_install_crash_handler(); // Calls debug_fatal_stack() if a fatal signal is received.


class timer {
	uint64_t start; // If the performance counter counts nanoseconds, this will take about 584 years to overflow.
	
public:
#ifdef __unix__
	static forceinline uint64_t get_raw()
	{
		struct timespec tp;
		clock_gettime(CLOCK_MONOTONIC, &tp); // CLOCK_MONOTONIC_RAW makes more sense per docs, but just about everything recommends MONOTONIC
		return (uint64_t)tp.tv_sec*(uint64_t)1000000000 + tp.tv_nsec; // even vdso - it implements MONOTONIC, but not M_RAW
	}
	static uint64_t us_to_raw(uint64_t us) { return us*1000; }
	static uint64_t raw_to_us(uint64_t count) { return count/1000; }
	static uint64_t raw_to_ms(uint64_t count) { return count/1000000; }
#else
	static uint64_t get_raw();
	static uint64_t raw_to_us(uint64_t count);
	static uint64_t raw_to_ms(uint64_t count);
	static uint64_t us_to_raw(uint64_t us);
#endif
	
private:
	uint64_t counter() { return get_raw() - start; }
	uint64_t counter_reset()
	{
		uint64_t prev = start;
		start = get_raw();
		return start - prev;
	}
	
public:
	timer() { reset(); }
	void reset() { start = get_raw(); }
	uint64_t us() { return raw_to_us(counter()); }
	uint64_t ms() { return raw_to_ms(counter()); }
	uint64_t us_reset() { return raw_to_us(counter_reset()); }
	uint64_t ms_reset() { return raw_to_ms(counter_reset()); }
};

class benchmark {
	uint64_t timer_start;
	uint64_t timer_stop;
public:
	uint64_t iterations;
private:
	
	bool next_cycle()
	{
		uint64_t t = timer::get_raw();
		if (t > timer_stop)
		{
			timer_stop = t;
			return false;
		}
		return true;
	}
	
public:
	
	benchmark() { reset(); }
	benchmark(uint32_t us) { reset(us); }
	
	forceinline operator bool()
	{
		iterations++;
		if (UNLIKELY(!(iterations & (iterations-1)))) // for really fast-running things, fetching the time is a bottleneck; do it rarely
		{
			if (!next_cycle())
				return false;
		}
		return true;
	}
	
	double per_second()
	{
		return (double)iterations * 1000000 / timer::raw_to_us(timer_stop - timer_start);
	}
	
	void reset(uint32_t new_us = 500000)
	{
		iterations = -1; // since the last operator bool() increments the iteration count but does not enter the loop body
		timer_start = timer::get_raw();
		timer_stop = timer_start + timer::us_to_raw(new_us);
	}
	
	// This is mostly the same as the global launder function, but guarantees that the input is
	//  computed for every iteration and not optimized out. A function call is a sufficient compiler
	//  barrier most of the time, but it's available if needed (for example if the function got inlined).
	template<typename T> static T launder(T v)
	{
		__asm__ volatile("" : "+g"(v));
		return v;
	}
};

// An instruction counter, well, counts the number of instructions executed by the calling thread since object creation.
// While actual time spent is the measure you'd actually want to optimize, it's a very noisy number, affected by system load,
//   system load a second ago (affects current cpu frequency), CPU model, scheduler whims,
//   kernel state (mmap takes different time depending on how much RAM is free/cached/whatever), and just about everything else.
// The instruction counter is much more stable; not perfect, but around one part per million if running the same program twice.
//   (The remaining noise is probably mostly due to ASLR messing with malloc, or foreign threads causing mutex spinning or something.)
// Can return zero, if the operation isn't supported on this OS or hardware (for example, Intel e-cores don't support them).
// Zero itself is not a possible return value if the object is functional, since fetching the counter takes a few instructions.
// Do not use this object together with the benchmark object - its number of iterations can vary by factors of two.
// (It is, however, okay to divide instruction count with benchmark's iteration count.)
class insn_counter {
#ifdef __linux__
	fd_t fd;
	
	static int perf_event_open(const struct perf_event_attr * hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags)
	{
		return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
	}
public:
	insn_counter()
	{
		struct perf_event_attr pe = {
			.type = PERF_TYPE_HARDWARE,
			.size = sizeof(pe),
			.config = PERF_COUNT_HW_INSTRUCTIONS,
			.exclude_kernel = true,
			.exclude_hv = true,
			.exclude_idle = true, // not sure what instruction count while idle means, but let's omit it
		};
		fd = perf_event_open(&pe, 0, -1, -1, PERF_FLAG_FD_CLOEXEC);
	}
	operator bool() { return fd.valid(); }
	
	uint64_t get()
	{
		// https://man7.org/linux/man-pages/man2/perf_event_open.2.html cap_user_rdpmc contains an example of skipping the read() syscall
		// but it's incomplete (missing setup, refers to undefined functions), uses the deprecated cap_usr_time name,
		// doesn't mention how it interacts with cpu migration, and contains a seqlock that means a potentially-noisy race condition
		// let's just stick to plain old reads
		uint64_t ret = 0;
		read(fd, &ret, sizeof(ret)); // if perf_event_open fails, read(-1, ...) will leave the buffer unchanged at zero
		return ret;
	}
	
	// Or you can just subtract the previous get() return value.
	void reset()
	{
		ioctl(fd, PERF_EVENT_IOC_RESET, 0);
	}
#else
public:
	// the windows equivalent is EnableThreadProfiling()
	// but the documentation is awful, and googling it suggests it doesn't work at all
	// (perf_event_open's docs are a rambly infodump with way too few examples, but at least there is some information)
	insn_counter() {}
	operator bool() { return false; }
	uint64_t get() { return 0; }
	void reset() {}
#endif
};

#ifdef _WIN32
#define gmtime_r(a,b) (*(b) = *gmtime(a)) // this is safe, gmtime() returns a thread local
time_t timegm(struct tm * t); // similar to mktime, but UTC timezone
#endif
