#ifdef __linux__
#include "sandbox.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <sys/wait.h>

#include "internal.h"

//TODO: when (if) AT_BENEATH is merged, hand out fds to the mount points to the child and allow openat(O_RDONLY|AT_BENEATH)
//do not allow O_RDWR, max_write is mandatory
//this will improve performance by not involving broker for the vast majority of open()s
//it's still open/openat/sigreturn, but it's way better than open/sendto/recvfrom/openat/sendmsg/sigreturn

void sandproc::filesystem::grant_native_redir(string cpath, string ppath, int max_write)
{
	if (ppath[0] != '/') abort();
	mount& m = mounts.insert(cpath);
	
	string cend = cpath.rsplit<1>("/")[1];
	array<string> pp = ppath.rsplit<1>("/");
	string pend = pp[1];
	if (cend != pend) abort();
	
	string mntpath = pp[0]+"/"; // if ppath is "/", pp[0] is empty, so append a /
	
	m.type = ty_native;
	if (!mountfds.contains(mntpath))
	{
		// open all permitted paths eagerly; juggling them at br_open is a timing leak
		// (not sure if kernel is constant-time, but at least I am)
		int fd = open(mntpath, O_DIRECTORY|O_PATH);
		if (fd < 0)
		{
			goto fail;
		}
		mountfds.insert(mntpath, fd);
	}
	m.n_fd = mountfds.get(mntpath);
	m.numwrites = max_write;
	
	if (m.n_fd < 0)
	{
	fail:
		//TODO: report error to parent-process caller
		m.type = ty_error;
		m.e_error = ENOENT;
	}
}
void sandproc::filesystem::grant_native(string path, int max_write)
{
	grant_native_redir(path, path, max_write);
}
void sandproc::filesystem::grant_tmp(string cpath, int max_size)
{
	mount& m = mounts.insert(cpath);
	
	m.type = ty_tmp;
	m.numwrites = max_size;
}
void sandproc::filesystem::grant_errno(string cpath, int error, bool noisy)
{
	mount& m = mounts.insert(cpath);
	
	m.type = ty_error;
	m.e_error = error | (noisy ? SAND_ERRNO_NOISY : 0);
}

sandproc::filesystem::filesystem()
{
	grant_errno("/", EACCES, true);
}

sandproc::filesystem::~filesystem()
{
	for (auto& m : tmpfiles)
	{
		close(m.value);
	}
	for (auto& pair : mountfds)
	{
		close(pair.value);
	}
}

//calls report_access_violation if needed
int sandproc::filesystem::child_file(cstring pathname, int op_, int flags, mode_t mode)
{
	broker_req_t op = (broker_req_t)op_;
	bool is_write;
	if (op == br_open)
	{
		//block unfamiliar or unusable flags
		//intentionally rejected flags: O_DIRECT, O_DSYNC, O_PATH, O_SYNC, __O_TMPFILE
		if (flags & ~(O_ACCMODE|O_APPEND|O_ASYNC|O_CLOEXEC|O_CREAT|O_DIRECTORY|O_EXCL|
					  O_LARGEFILE|O_NOATIME|O_NOCTTY|O_NOFOLLOW|O_NONBLOCK|O_TRUNC))
		{
			errno = EINVAL;
			return -1;
		}
		
		if ((flags&O_ACCMODE) == O_ACCMODE)
		{
			errno = EINVAL;
			return -1;
		}
		
		is_write = ((flags&O_ACCMODE) != O_RDONLY) || (flags&O_CREAT) || (flags&O_TRUNC);
	}
	else if (op == br_unlink) is_write = true;
	else if (op == br_access) is_write = false;
	else
	{
		errno = EINVAL;
		return -1;
	}
	
	if (pathname[0] != '/' ||
		pathname.contains("/./") || pathname.contains("/../") ||
		pathname.endswith("/.") || pathname.endswith("/.."))
	{
		report_access_violation(pathname, is_write);
		errno = EACCES;
		return -1;
	}
	while (pathname.endswith("/")) pathname = pathname.substr(0, ~1);
	
	bool exact_path = false;
	
//puts("open "+pathname);
	mount* m = NULL;
	size_t mlen = 0;
	for (auto& miter : mounts)
	{
//puts("  mount "+miter.key);
		if (miter.key[0] != '/') abort();
		if (miter.key.length() <= mlen) continue;
		bool use;
		if (miter.key.endswith("/"))
		{
			if ((pathname+"/") == miter.key)
			{
				exact_path = true;
				use = true;
			}
			else use = (pathname.startswith(miter.key));
		}
		else
		{
			use = (pathname == miter.key); // file mount
		}
		if (use)
		{
//puts("    yes");
			m = &miter.value;
			mlen = miter.key.length();
			while (miter.key[mlen-1]!='/') mlen--;
		}
	}
//puts("  "+pathname+" "+tostring(mlen));
	if (!mlen) abort();
	
	
	switch (m->type)
	{
	case ty_error:
	{
		if (m->e_error & SAND_ERRNO_NOISY)
		{
			report_access_violation(pathname, is_write);
		}
		errno = m->e_error&SAND_ERRNO_MASK;
		return -1;
	}
	
	case ty_native:
	{
		if (is_write)
		{
			if (m->numwrites == 0)
			{
				report_access_violation(pathname, true);
				errno = EACCES;
				return -1;
			}
			m->numwrites--;
		}
		
		cstring relpath;
		if (mlen > pathname.length()) relpath = "."; // open("/usr/include/") when that's a mountpoint
		else relpath = pathname.substr(mlen, ~0);
		
		if (op == br_open) return openat(m->n_fd, relpath.c_str(), flags|O_CLOEXEC|O_NOCTTY, mode);
		if (op == br_unlink) return unlinkat(m->n_fd, relpath.c_str(), 0);
		if (op == br_access) return faccessat(m->n_fd, relpath.c_str(), flags, 0);
		abort();
	}
	case ty_tmp:
	{
		if (op == br_open)
		{
			int fd = tmpfiles.get_or(pathname, -1);
			if (fd < 0 && is_write)
			{
				//ignore mode, it gets 777
				if (m->numwrites == 0)
				{
					report_access_violation(pathname, true);
					errno = ENOMEM;
					return -1;
				}
				m->numwrites--;
				
				fd = memfd_create(pathname.c_str(), MFD_CLOEXEC);
				tmpfiles.insert(pathname, fd);
			}
			//unshare file position
			return open("/proc/self/fd/"+tostring(fd), (flags|O_CLOEXEC|O_NOCTTY)&~(O_EXCL|O_CREAT));
		}
		if (op == br_unlink)
		{
			int fd = tmpfiles.get_or(pathname, -1);
			if (fd >= 0)
			{
				tmpfiles.remove(pathname);
				close(fd);
				return 0;
			}
			else
			{
				errno = ENOENT;
				return -1;
			}
		}
		if (op == br_access)
		{
			if (tmpfiles.contains(pathname) || exact_path)
			{
				return 0;
			}
			else
			{
				errno = ENOENT;
				return -1;
			}
		}
		abort();
	}
	default: abort();
	}
}



void sandproc::send_rsp(int sock, broker_rsp* rsp, int fd)
{
	if (send_fd(sock, rsp, sizeof(*rsp), MSG_DONTWAIT|MSG_NOSIGNAL|MSG_EOR, fd) <= 0)
	{
		//full buffer or otherwise failed send means misbehaving child or child exited
		//in the former case, kill; in the latter, killing what's dead is harmless
		//(okay, it could hit if a threaded child calls open() and exit() simultaneously,
		//  but that's a badly designed child. and killing what's dead is still harmless.)
		terminate();
	}
}

async<void> sandproc::broker_fn(fd_raw_t sock)
{
	while (true)
	{
		co_await runloop2::await_read(sock);
		struct broker_req req;
		ssize_t req_sz = recv(sock, &req, sizeof(req), MSG_DONTWAIT);
		if (req_sz==-1 && errno==EAGAIN) continue;
		else if (req_sz == 0)
		{
			// closed socket? child probably exited
			// ideally, we'd check if that was the last one and terminate child, but no real point
			co_return;
		}
		else if (req_sz != sizeof(req))
		{
			terminate(); // no mis-sized messages allowed
			co_return;
		}
		req.path[sizeof(req.path)-1] = '\0'; // ensure the string is nul terminated
		
		broker_rsp rsp = { req.type };
		int fd = -1;
		bool close_fd = true;
		
		switch (req.type)
		{
		case br_nop:
		{
			continue; // continue so send_rsp doesn't run
		}
		case br_ping:
		{
			break; // pings return only { br_ping }, which we already have
		}
		case br_open:
		case br_unlink:
		case br_access:
		{
			fd = fs.child_file(req.path, req.type, req.flags[0], req.flags[1]);
			close_fd = true;
	//puts((string)req.path+" "+tostring(req.type)+": "+tostring(fd)+" "+tostring(errno));
			if (fd < 0) rsp.err = errno;
			break;
		}
		case br_fork:
		{
			int socks[2];
			if (socketpair(AF_UNIX, SOCK_SEQPACKET|SOCK_CLOEXEC, 0, socks) < 0)
			{
				socks[0] = -1;
				socks[1] = -1;
			}
			
			fd = socks[1];
			close_fd = true;
			brokers.add(broker_fn(socks[0]));
			break;
		}
		case br_bad_sys:
		{
			bad_syscall(req.flags[0], req.path);
			continue;
		}
		default:
			terminate(); // invalid request means child is doing something stupid
			co_return;
		}
		send_rsp(sock, &rsp, fd);
		if (fd > 0 && close_fd) close(fd);
	}
}

void sandproc::fs_grant_syslibs(cstring exe)
{
	fs.grant_native("/lib64/ld-linux-x86-64.so.2");
	fs.grant_native("/usr/lib/x86_64-linux-gnu/libstdc++.so.6");
	fs.grant_native("/lib/x86_64-linux-gnu/libdl.so.2");
	fs.grant_native("/lib/x86_64-linux-gnu/libpthread.so.0");
	fs.grant_native("/lib/x86_64-linux-gnu/libc.so.6");
	fs.grant_native("/lib/x86_64-linux-gnu/libm.so.6");
	fs.grant_native("/lib/x86_64-linux-gnu/libgcc_s.so.1");
	fs.grant_native("/lib/x86_64-linux-gnu/libselinux.so.1");
	fs.grant_native("/lib/x86_64-linux-gnu/libpcre.so.3"); // kinda specialized, but part of the base install so it's fine
	fs.grant_native("/bin/sh");
	fs.grant_native("/dev/urandom");
	fs.grant_errno("/dev/", EACCES, false);
	fs.grant_errno("/etc/ld.so.nohwcap", ENOENT, false);
	fs.grant_errno("/etc/ld.so.preload", ENOENT, false);
	fs.grant_errno("/etc/suid-debug", ENOENT, false);
	fs.grant_errno("/usr/share/locale/", ENOENT, false);
	fs.grant_errno("/usr/share/locale-langpack/", ENOENT, false);
	fs.grant_errno("/usr/lib/locale/", ENOENT, false);
	fs.grant_errno("/lib/x86_64-linux-gnu/glibc-hwcaps/", ENOENT, false);
	fs.grant_errno("/lib/x86_64-linux-gnu/tls/", ENOENT, false);
	fs.grant_errno("/lib/x86_64-linux-gnu/x86_64/", ENOENT, false);
	fs.grant_native("/etc/ld.so.cache");
	fs.grant_tmp("/tmp/", 100);
	if (exe)
	{
		string exepath = process::find_prog(exe);
		if (exepath) fs.grant_native(file::realpath(exepath));
	}
}
#endif
