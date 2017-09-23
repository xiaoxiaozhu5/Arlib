#include "runloop.h"
#include "set.h"

#ifdef __linux__
#include <sys/epoll.h>
#include <unistd.h>

class runloop_linux : public runloop {
public:
	#define RD_EV (EPOLLIN |EPOLLRDHUP|EPOLLHUP|EPOLLERR)
	#define WR_EV (EPOLLOUT|EPOLLRDHUP|EPOLLHUP|EPOLLERR)
	
	int epoll_fd;
	bool exited = false;
	
	struct fd_cbs {
		function<void(uintptr_t)> cb_read;
		function<void(uintptr_t)> cb_write;
	};
	map<int,fd_cbs> fdinfo;
	
	struct timer_cb {
		unsigned id;
		unsigned ms;
		struct timespec next;
		function<bool()> cb;
	};
	//TODO: this should probably be a priority queue instead
	array<timer_cb> timerinfo;
	
	/*private*/ static void timespec_now(struct timespec * ts)
	{
		clock_gettime(CLOCK_MONOTONIC, ts);
	}
	
	/*private*/ static void timespec_add(struct timespec * ts, unsigned ms)
	{
		ts->tv_sec += ms/1000;
		ts->tv_nsec += (ms%1000)*1000000;
		if (ts->tv_nsec > 1000000000)
		{
			ts->tv_sec++;
			ts->tv_nsec -= 1000000000;
		}
	}
	
	//returns milliseconds
	/*private*/ static int64_t timespec_sub(struct timespec * ts1, struct timespec * ts2)
	{
		int64_t ret = (ts1->tv_sec - ts2->tv_sec) * 1000;
		ret += (ts1->tv_nsec - ts2->tv_nsec) / 1000000;
		return ret;
	}
	
	/*private*/ static bool timespec_less(struct timespec * ts1, struct timespec * ts2)
	{
		if (ts1->tv_sec < ts2->tv_sec) return true;
		if (ts1->tv_sec > ts2->tv_sec) return false;
		return (ts1->tv_nsec < ts2->tv_nsec);
	}
	
	
	
	runloop_linux() { epoll_fd = epoll_create1(EPOLL_CLOEXEC); }
	
	uintptr_t set_fd(uintptr_t fd, function<void(uintptr_t)> cb_read, function<void(uintptr_t)> cb_write = NULL)
	{
		fd_cbs& cb = fdinfo.get_create(fd);
		cb.cb_read  = cb_read;
		cb.cb_write = cb_write;
		
		epoll_event ev = {}; // shut up valgrind, I only need events and data.fd, the rest of data will just come back out unchanged
		ev.events = (cb_read ? RD_EV : 0) | (cb_write ? WR_EV : 0);
		ev.data.fd = fd;
		if (ev.events)
		{
			epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev); // one of these two will fail (or do nothing), we'll ignore that
			epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
		}
		else
		{
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
			fdinfo.remove(fd);
		}
		return fd;
	}
	
	
	uintptr_t set_timer_rel(unsigned ms, function<bool()> callback)
	{
		unsigned timer_id = 1;
		for (size_t i=0;i<timerinfo.size();i++)
		{
			if (timerinfo[i].id >= timer_id)
			{
				timer_id = timerinfo[i].id+1;
			}
		}
		
		timer_cb& timer = timerinfo.append();
		timer.id = timer_id;
		timer.ms = ms;
		timer.cb = callback;
		timespec_now(&timer.next);
		timespec_add(&timer.next, ms);
		return -(intptr_t)timer_id;
	}
	
	
	void remove(uintptr_t id)
	{
		intptr_t id_s = id;
		if (id_s >= 0)
		{
			int fd = id_s;
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
			fdinfo.remove(fd);
		}
		else
		{
			unsigned t_id = -id_s;
			for (size_t i=0;i<timerinfo.size();i++)
			{
				if (timerinfo[i].id == t_id)
				{
					timerinfo.remove(i);
					return;
				}
			}
		}
	}
	
	
	/*private*/ void step(bool block)
	{
		struct timespec now;
		timespec_now(&now);
//printf("runloop: time is %lu.%09lu\n", now.tv_sec, now.tv_nsec);
		
		int next = INT_MAX;
		
		for (size_t i=0;i<timerinfo.size();i++)
		{
			timer_cb& timer = timerinfo[i];
//printf("runloop: scheduled event at %lu.%09lu\n", timer.next.tv_sec, timer.next.tv_nsec);
			
			int next_ms = timespec_sub(&timer.next, &now);
			if (next_ms <= 0)
			{
//printf("runloop: calling event scheduled %ims ago\n", -next_ms);
				bool keep = timer.cb();
				if (exited) block = false; // make sure it doesn't block forever if timer callback calls exit()
				if (!keep)
				{
					timerinfo.remove(i);
					i--;
					continue;
				}
				
				timer.next = now;
				timespec_add(&timer.next, timer.ms);
				next_ms = timer.ms;
			}
			
			if (next_ms < next) next = next_ms;
		}
		
		if (next == INT_MAX) next = -1;
		if (!block) next = 0;
		
		
		epoll_event ev[16];
//printf("runloop: waiting %i ms\n", next);
		int nev = epoll_wait(epoll_fd, ev, 16, next);
		for (int i=0;i<nev;i++)
		{
			fd_cbs& cbs = fdinfo[ev[i].data.fd];
			     if ((ev[i].events & RD_EV) && cbs.cb_read)  cbs.cb_read( ev[i].data.fd);
			else if ((ev[i].events & WR_EV) && cbs.cb_write) cbs.cb_write(ev[i].data.fd);
		}
	}
	
	void enter()
	{
		exited = false;
		while (!exited) step(true);
	}
	
	void exit()
	{
		exited = true;
	}
	
	void step()
	{
		step(false);
	}
	
	~runloop_linux() { close(epoll_fd); }
};



namespace {
struct abs_cb {
	function<void()> callback;
	abs_cb(function<void()> callback) : callback(callback) {}
	bool invoke() { callback(); return false; }
};
}
uintptr_t runloop::set_timer_abs(time_t when, function<void()> callback)
{
	int ms = (when-time(NULL))*1000;
	if (ms <= 0) ms = 0;
	return set_timer_rel(ms, bind_ptr_del(&abs_cb::invoke, new abs_cb(callback)));
}

runloop* runloop::create()
{
	return new runloop_linux();
}
#endif

#ifdef ARGUI_NONE
runloop* runloop::global()
{
	//ignore thread safety, this function can only be used from main thread
	static runloop* ret = NULL;
	if (!ret) ret = runloop::create();
	return ret;
}
#endif

#include "test.h"
static int64_t time_ms()
{
	//TODO: non-linux version
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (int64_t)now.tv_sec*1000 + now.tv_nsec/1000000;
}
static void test_runloop(bool is_global)
{
	runloop* loop = (is_global ? runloop::global() : runloop::create());
	
	{
		//must be before the other one, loop->enter() must be called to ensure it doesn't actually run
		loop->remove(loop->set_timer_rel(10, bind_lambda([]()->bool { abort(); return false; })));
	}
	
	{
		//time() is too low resolution, clock() is CLOCK_PROCESS_CPUTIME_ID
		int64_t start = time_ms();
		int64_t end = start;
		loop->set_timer_rel(100, bind_lambda([&]()->bool { end = time_ms(); loop->exit(); return false; }));
		loop->enter();
		
		int64_t ms = end-start;
		assert_msg(ms > 75 && ms < 200, tostring(ms)+" not in range");
	}
	
	if (!is_global) delete loop;
}
test("global runloop")
{
	test_runloop(true);
}
test("private runloop")
{
	test_runloop(false); // it's technically illegal to create a runloop on the main thread, but nobody's gonna notice
}
