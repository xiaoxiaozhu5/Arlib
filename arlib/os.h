#pragma once
#include "global.h"

#ifdef __unix__
#define DYLIB_EXT ".so"
#define DYLIB_MAKE_NAME(name) "lib" name DYLIB_EXT
#endif
#ifdef _WIN32
#define DYLIB_EXT ".dll"
#define DYLIB_MAKE_NAME(name) name DYLIB_EXT
#endif

class dylib : nocopy {
	void* handle;
	
public:
	dylib() { handle=NULL; }
	dylib(const char * filename) { handle=NULL; init(filename); }
	
	//owned tells whether the DLL was loaded before calling this
	//this is an atomic operation; if multiple threads call dylib::create for the same file, only one will get owned==true
	//if called multiple times, unloads the previous dylib
	bool init(const char * filename, bool * owned = NULL);
	void* sym_ptr(const char * name);
	funcptr sym_func(const char * name);
	template<typename T> T sym(const char * name) { return (T)sym_func(name); }
	
	//Fetches multiple symbols. 'names' is expected to be a NUL-separated list of names, terminated with a blank one.
	// (This is easiest done by using multiple NUL-terminated strings. The compiler appends another NUL.)
	//Returns whether all of them were successfully fetched. Failures are NULL.
	bool sym_multi(funcptr* out, const char * names);
	
	void deinit();
	~dylib() { deinit(); }
};

//If the program is run under a debugger, this triggers a breakpoint. If not, ignored.
void debug_or_ignore();
//If the program is run under a debugger, this triggers a breakpoint. If not, the program silently exits.
void debug_or_exit();
//If the program is run under a debugger, this triggers a breakpoint. If not, the program crashes.
void debug_or_abort();

//Returns time since an undefined point in time, in microseconds. The epoch may vary across machines or reboots.
uint64_t perfcounter();
