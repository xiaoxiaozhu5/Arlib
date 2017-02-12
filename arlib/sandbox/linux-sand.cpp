#include "sandbox.h"
#include "../process.h"
#include "../test.h"

#ifdef __linux__
//Features needed:
//- seccomp for easy cases (read(2) is fine, reboot(2) is not), ptrace for tricky stuff (open(2) is sometimes allowed)
//- Parent process needs to handle ptrace events; do this on a separate thread
//- permit() needs to be thread safe; the stdio functions don't need anything
//- Make sure ptrace/waitpid don't interact in bad ways, maybe process needs a "I killed the child for you" function?
//- Do not require any privileges
//- Must be able to run multiple sandboxes at once
//Independent process:
//- Assume hostile code immediately after exec()
//- Must be able to open files needed for initialization
//- Cannot change this program
//Arlib process:
//- Support only same process
//- Can change the program, for example adding a static void sandbox::enter()
// - Can add an extra command line argument, can add something funny to fd 3
//- Can assume non-hostility until said call (but it's better not to)
//- Must be able to create events and shared memory
// - whether the process is same or another one
//- They don't need to share an API
// - but they should share the BPF rules
//  - I'll implement a 'connect to parent' operation via open("/#!arlib", O_WRONLY|O_RDWR),
//    ptrace will intercept this and replace it with fd 3, which parent set up

#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

bool sandproc::launch_impl(cstring path, arrayview<string> args)
{
	this->preexec = bind_this(&sandproc::preexec_fn);
	bool ret = process::launch_impl(path, args);
	if (!ret) return false;
	ptrace(PTRACE_SETOPTIONS, this->pid, NULL, (uintptr_t)(PTRACE_O_EXITKILL | PTRACE_O_TRACESECCOMP));
	ptrace(PTRACE_CONT, this->pid, NULL, (uintptr_t)0);
	return true;
}

void sandproc::preexec_fn(execparm* params)
{
	if (conn)
	{
		params->nfds_keep = 4;
		//dup2(conn->?, 3);
	}
	
	ptrace(PTRACE_TRACEME);
	puts("AAAAAAAAAAA");
	
	//FIXME: seccomp script
	//note that ftruncate() must be rejected on kernel < 3.17 (including Ubuntu 14.04), to ensure sandcomm::shalloc can't SIGBUS
	//linkat() must also be restricted, to ensure open(O_TRUNC) and truncate() can't be used
}

//https://github.com/nelhage/ministrace/commit/0a1ab993f4763e9fb77d9a89e763a403d80de1ff

//void* sandproc::ptrace_func_c(void* arg) { ((sandproc*)arg)->ptrace_func(); return NULL; }
//void sandproc::ptrace_func()
//{
//	//glibc does
//	//void* stack = mmap(NULL, 256*1024, size, prot, MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0);
//	//clone(flags=(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGNAL |
//	//             CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID | CLONE_SYSVSEM | 0)
//}

sandproc::~sandproc() { delete this->conn; }

//static void sandtest_fn(sandcomm* comm)
//{
	//int* i = comm->malloc<int>();
	//comm->wait(0);
	//(*i)++;
	//comm->release(1);
//}

test()
{
	test_skip("not implemented");
	{
		sandproc p;
		bool ok = p.launch("/bin/true");
		assert(ok);
		int status;
		p.wait(&status);
		assert_eq(status, 0);
	}
	
	{
		sandproc p[5];
		for (int i=0;i<5;i++) assert(p[i].launch("/bin/sleep", "5"));
		for (int i=0;i<5;i++)
		{
			int ret;
			p[i].wait(&ret);
			assert_eq(ret, 0);
		}
	}
	
	//{
		//sandproc p;
		//p.error();
		//p.permit("/etc/not_passwd");
		//p.permit("/etc/passw");
		//p.permit("/etc/passwd/");
		//p.permit("/etc/passwd_");
		//assert(p.launch("/bin/cat", "/etc/passwd"));
		//p.wait();
		//assert_eq(p.read(), "");
		//assert(p.error() != "");
	//}
	
	//{
		//sandproc p;
		//p.error();
		//p.permit("/etc/passwd");
		//assert(p.launch("/bin/cat", "/etc/passwd"));
		//p.wait();
		//assert(p.read() != "");
		//assert_eq(p.error(), "");
	//}
	
	//{
		//TODO: call sandfunc::enter in test.cpp
		//sandfunc f;
		//sandcomm* comm = f.launch(sandtest_fn);
		//int* i = comm->malloc<int>();
		//assert(i);
		//*i = 1;
		//comm->release(0);
		//comm->wait(1);
		//assert_eq(*i, 2);
		//comm->free(i);
	//}
	
}
#endif
