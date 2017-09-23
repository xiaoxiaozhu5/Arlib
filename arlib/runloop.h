#pragma once
#include "global.h"

//A runloop keeps track of a number of file descriptors, calling their handlers whenever the relevant operation is available.
//There are no fairness guarantees. If an event doesn't terminate or unset itself properly, it may inhibit other fds.
//Do not call enter() or step() while inside a callback. However, set_*(), remove() and exit() are fine.
class runloop {
protected:
	runloop() {}
public:
	//The global runloop handles GUI events, in addition to whatever fds it's told to track. Always returns the same object.
	//Don't call from anything other than the main thread.
	static runloop* global();
	
	//Use only one runloop per thread. Don't use on the main thread.
	static runloop* create();
	
#ifndef _WIN32 // fd isn't a defined concept on windows
	//Callback argument is the fd, in case one object maintains multiple fds.
	//A fd can only be used once per runloop. If that fd is already there, it's removed prior to binding the new callbacks.
	//If the new callbacks are both NULL, it's removed. The return value can still safely be passed to remove().
	//If only one callback is provided, events of the other kind are ignored.
	//If both reading and writing is possible, reading takes precedence.
	//If the other side of the fd is closed, it's considered both readable and writable.
	virtual uintptr_t set_fd(uintptr_t fd, function<void(uintptr_t)> cb_read, function<void(uintptr_t)> cb_write = NULL) = 0;
#else
	//TODO: need socket support
#endif
	
	//Runs once.
	uintptr_t set_timer_abs(time_t when, function<void()> callback);
	//Do not remove() the callback from within itself; instead, return false and it won't return.
	//Accuracy is not guaranteed; it may or may not round the timer frequency to something it finds appropriate,
	// in either direction, and may or may not try to 'catch up' if a call is late (or early).
	//Don't use for anything that needs tighter timing than ±1 second.
	virtual uintptr_t set_timer_rel(unsigned ms, function<bool()> callback) = 0;
	
	//Return value from each set_*() is a token which can be used to cancel the event. Only usable before the timer fires.
	virtual void remove(uintptr_t id) = 0;
	
	//Executes the mainloop until ->exit() is called. Recommended for most programs.
	virtual void enter() = 0;
	virtual void exit() = 0;
	
	//Runs until there are no more events to process, then returns. Recommended for high-performance programs like games. Call it frequently.
	virtual void step() = 0;
	
	//Deleting a non-global runloop is fine, but leave the global one alone.
	virtual ~runloop() {}
};
