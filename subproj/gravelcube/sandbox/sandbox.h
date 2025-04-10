// TODO: check how much of this can be replaced with https://www.kernel.org/doc/html/v5.15/userspace-api/landlock.html

#pragma once
#include "../arlib.h"
#include <errno.h>

//Allows safely executing untrusted code.
//
//Exact rules:
// Other than as allowed by the parent, the child may not read per-user data or system configuration,
//  or write to permanent storage. Internet counts as permanent. The child may find out what OS it's running on.

//Not implemented on Windows. It provides plenty of ways to restrict a process, but
//- All I could find are blacklists, disabling a particular privilege; I want whitelists
//- There are so many resource kinds I can't keep track how to restrict everything, or even list
//    them; it will also fail-open if a new resource kind is introduced
//- Many lockdown functions temporarily disable privileges, rather than completely delete them
//- There's little or no documentation on which privileges are required for the operations I need
//    (Linux doesn't document that very clearly either, but strace exists and the kernel ABI is stable, so it can be reverse engineered)
//      (it occasionally changes between userspace upgrades, forcing a re-RE, but it's a rare and usually easy operation)
//- The lockdown functions are often annoying to call, involving variable-width arrays in
//    structures, and LCIDs that likely vary between reboots
//I cannot trust such a system. I only trust whitelists, like Linux seccomp.
//Even Chrome couldn't find anything comprehensive; they use everything they can find (restricted token, job object, desktop,
// SetProcessMitigationPolicy, firewall, etc), but some operations, such as accessing FAT32 volumes, still pass through.
//It feels like the Windows sandboxing functions are designed for trusted code operating on untrusted data, rather than untrusted code,
// or for memory safe languages, or some similarly inapplicable constraint.
//Since I can't create satisfactory results in such an environment, I won't even try.

//Note that if stdin, stdout or stderr point to the parent process' std{in,out,err}, the child can fstat it.
// If this is considered private information, create a pipe and forward it to the real streams.

#ifdef __linux__
//Currently, both parent and child must be x86_64.

class sandcomm;
struct broker_rsp;
class sandproc {
	fd_t pidfd;
	co_holder brokers;
	
	class filesystem : nocopy {
		enum type_t { ty_error, ty_native, ty_tmp };
		struct mount : nocopy {
			type_t type;
			
			union {
				#define SAND_ERRNO_MASK 0xFFF
				#define SAND_ERRNO_NOISY 0x1000
				int e_error;
				int n_fd;
				//ty_tmp's fd is in tmpfiles, keyed by full path
			};
			int numwrites; // decreased every time a write is done on native, or a file is created on tmp
		};
		//TODO: use an ordered map instead, iterating like this can't be fast
		map<string, mount> mounts;
		map<string, int> tmpfiles;
		map<string, int> mountfds;
		
	public:
		void grant_native_redir(string cpath, string ppath, int max_write = 0);
		void grant_native(string path, int max_write = 0);
		void grant_tmp(string cpath, int max_size);
		void grant_errno(string cpath, int error, bool noisy);
		
		filesystem();
		~filesystem();
		
		function<void(cstring path, bool write)> report_access_violation;
		
		//calls report_access_violation if needed
		int child_file(cstring pathname, int /* broker_req_t */ op, int flags, mode_t mode);
	};
	filesystem fs;
	function<void(int sysno, cstring path)> bad_syscall;
	
	async<void> broker_fn(fd_raw_t sock);
	void send_rsp(int sock, broker_rsp* rsp, int fd);
	
	sandcomm* conn = NULL;
	
	static int preloader_fd();
	
public:
	~sandproc() { terminate(); }
	
	// These functions act like in the process object, except that
	// - if params::fds is smaller than 3, it will be extended with /dev/null, not stdin/stdout/stderr
	// - if fds is more than 3 elements, the extras will be ignored
	// - params::envp is ignored
	// - if no raw_params version
	
	struct params {
		string prog; // If this doesn't contain a slash, find_prog will be called. If it's empty, argv[0] will be used.
		array<string> argv; // If this is empty, prog will be used.
		array<fd_raw_t> fds; // Will be mutated. If shorter than three elements, will be extended with /dev/null.
	};
	
	int create(process::params&& param);
	
	async<int> wait();
	void terminate();
	
	// The following are only available before starting the child.
	
	//If the child process uses Arlib, this allows convenient communication with it.
	//Must be called before starting the child, and exactly once (or never).
	//Like process, the object is not thread safe.
	//Note that if the child is the same as the current program,
	// linked libraries (for example GTK+) must be available, even if they're not used.
	//To avoid that, use a separate binary for the child. If you don't want an on-disk file, use fs_grant_callback and a memfd.
	sandcomm* connect();
	
	void max_cpu(unsigned lim); // in wall clock seconds, default 60
	void max_cpu_frac(float lim); // in core-seconds per wall clock second, default 1
	void max_mem(unsigned lim); // in megabytes, default 1024
	
	//Allows access to a file, or a directory and all of its contents. Usable both before and after launch().
	//Can not be undone, the process may already have opened the file; instead, destroy the process.
	//If 'path' ends with a /, it's assumed to be a directory; it, and every child, are made available.
	//If 'path' does not end with a /, any child returns ENOTDIR.
	//max_write is how many times files may be opened for writing. Zero is allowed. Reads are unlimited.
	//[TODO: verify the above]
	void fs_grant(cstring path, int max_write = 0) { fs_grant_at(path, path, max_write); }
	
	//To allow shuffling the filesystem. For example, /home/user/ can be mounted at /@CWD/.
	void fs_grant_at(cstring real, cstring mount_at, int max_write = 0) { fs.grant_native_redir(mount_at, real, max_write); }
	
	//max_write is how many unique files may exist in the temp directory.
	//Created files will never exist on disk (except maybe in swap).
	void fs_grant_tmp(cstring path, int max_size) { fs.grant_tmp(path, max_size); }
	
	void fs_grant_cwd(int max_write = 0) { fs_grant_cwd(file::cwd(), max_write); }
	void fs_grant_cwd(cstring real, int max_write = 0) { fs_grant_at(real, "/@CWD/", max_write); }
	
	//WARNING: If the parent directory of a fs_hide() target is child-accessible, the hidden directory may be visible too.
	//Do not use as a security mechanism. Use it only as a warning suppression mechanism.
	void fs_hide(cstring path) { fs.grant_errno(path, ENOENT, false); }
	
	//The callback should return a file descriptor, or -1 with errno set.
	//It will never call the access violation callback. Do that yourself.
	//The returned file descriptor must be writable if 'write' is set. If it's not,
	// returning a writable fd will make the sandbox create a new fd corresponding to the same file,
	// but without write access.
	//should_close tells whether the sandbox should close the returned fd for you. Defaults to true.
	void fs_grant_callback(cstring path, function<uintptr_t(cstring path, bool write, bool& should_close)> callback);
	
	//Grants access to some system libraries present in default installations of the operating system,
	// needed to run simple programs.
	//Some programs may require additional files.
	void fs_grant_syslibs(cstring exe = "");
	
	void set_access_violation_cb(function<void(cstring path, bool write)> cb)
	{
		fs.report_access_violation = cb;
	}
	
	// Bad syscalls are rejected with -ENOSYS in the child.
	// Most of the time, the child handles ENOSYS correctly.
	// For more details on what syscall that was, use strace -f. To silence them, upgrade the sandbox.
	// The sandbox implementation does not sanitize the sysno; it can contain nonsense, including negative numbers.
	void set_bad_syscall_cb(function<void(int sysno, cstring detail)> cb)
	{
		bad_syscall = cb;
	}
};

class sandcomm : nomove {
public:
	//For thread safety purposes, the parent and child count as interacting with different objects.
	//However, sandcomm and its sandproc count as the same object.
	
	//If the process isn't in an Arlib sandbox, or parent didn't call connect(), returns NULL. Can only be called once.
	static sandcomm* connect();
	
	//A few binary semaphores ('sem' can be 0-7, inclusive), used to synchronize the two processes.
	//Calling release() on an already-released semaphore is undefined behavior.
	//Multiple threads may be in these functions simultaneously (or in these plus something else),
	// but only one thread per process per semaphore.
	//They start in the locked state.
	void wait(int sem);
	bool try_wait(int sem);
	void release(int sem);
	
	//To allow synchronized(box->sem(1)) {}
	struct semlock_t
	{
		sandcomm* parent; int id;
		void lock() { parent->wait(id); }
		bool try_lock() { return parent->try_wait(id); }
		void unlock() { parent->release(id); }
	};
	semlock_t sem(int id) { semlock_t ret; ret.parent=this; ret.id=id; return ret; }
	
	
	//Clones a file handle into the child. The handle remains open in the parent. The child may get another ID.
	//May block. Obviously, both processes must use the same order.
	//If there are more than 8 handles passed to fd_send() that the child hasn't fetched from fd_recv(), behavior is undefined.
	void fd_send(intptr_t fd); // Parent only.
	intptr_t fd_recv(); // Child only.
	
	//Allocates memory shared between the two processes.
	//Limitations:
	//- Counts as a fd_send/fd_recv pair; must be done in the same order, can overflow
	//- May be expensive, far moreso than normal malloc; prefer reusing allocations if needed (or use stdin/stdout)
	//- Can fail and return NULL, obviously (if parent fails, child will too; parent may succeed if child doesn't)
	//- The returned memory is initially zero initialized; can't resize, you have to copy the data manually
	void* malloc(size_t bytes);
	void free(void* data, size_t bytes); // must be same size as in malloc
	
	//Convenience function, just calls the above.
	template<typename T> T* malloc(size_t count=1) { return (T*)this->malloc(sizeof(T)*count); }
	template<typename T> void free(T* data, size_t count=1) { this->free(data, sizeof(T)*count); }
	
	~sandcomm() {}
};

//Convenience wrapper, launches the current process and calls a function in it.
class sandfunc {
	sandproc proc;
public:
	static void enter(char** argv); // Must be the first thing in main(), before arlib_init(). Does nothing if not launched via sandfunc.
	sandcomm* launch(void(*proc)(sandcomm* comm)); // Not the function<> template, userdata pointers can't be passed around like that.
};
#endif
