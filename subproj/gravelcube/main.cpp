#include "arlib.h"
#include "sandbox/sandbox.h"

int main(int argc, char** argv)
{
	argparse args;
	bool verbose = false;
	args.add('v', "verbose", &verbose);
	array<string> child;
	args.add_early_stop(&child);
	args.parse(argv);
	
	sandproc ch(runloop::global());
	ch.set_stdout(process::output::create_stdout());
	ch.set_stderr(process::output::create_stderr());
	ch.fs_grant_syslibs(child[0]);
	ch.fs_grant_cwd(100);
	
	//for gcc
	ch.fs_grant("/usr/bin/make");
	ch.fs_grant("/usr/bin/gcc");
	ch.fs_grant("/usr/bin/g++");
	ch.fs_grant("/usr/bin/as");
	ch.fs_grant("/usr/bin/ld");
	ch.fs_grant("/usr/lib/");
	ch.fs_hide("/usr/gnu/");
	ch.fs_grant("/lib/");
	ch.fs_hide("/usr/");
	ch.fs_hide("/usr/local/include/");
	ch.fs_grant("/usr/include/");
	ch.fs_hide("/usr/x86_64-linux-gnu/");
	ch.fs_hide("/usr/bin/gnm");
	ch.fs_hide("/bin/gnm");
	ch.fs_grant("/usr/bin/nm");
	ch.fs_hide("/usr/bin/gstrip");
	ch.fs_hide("/bin/gstrip");
	ch.fs_grant("/usr/bin/strip");
	ch.fs_hide("/usr/bin/uname");
	ch.fs_grant("/bin/uname");
	ch.fs_grant("/usr/bin/objdump");
	ch.fs_hide("/usr/bin/grep");
	ch.fs_grant("/bin/grep");
	
	if (verbose)
	{
		array<string> sysnames;
		for (cstring line : file::readallt("/usr/include/x86_64-linux-gnu/asm/unistd_64.h").split("\n"))
		{
			if (line.startswith("#define __NR_"))
			{
				array<cstring> parts = line.csplit<2>(" ");
				int no;
				if (parts.size() == 3 && fromstring(parts[2], no) && no >= 0 && no < 1024)
				{
					sysnames.reserve(no+1);
					sysnames[no] = parts[1].substr(strlen("__NR_"), ~0);
				}
			}
		}
		
		ch.set_bad_syscall_cb([&sysnames](int sysno, cstring detail){
			string sysname;
			if (sysno >= 0 && (unsigned)sysno < sysnames.size()) sysname = sysnames[sysno];
			puts((string)"gravelcube: denied syscall " + (sysname ? sysname : tostring(sysno)) + " " + detail);
		});
		ch.set_access_violation_cb([](cstring path, bool write){
			puts((string)"gravelcube: denied " + (write ? "writing " : "reading ") + path);
		});
	}
	
	int ret;
	ch.onexit([&](int lstatus) { ret = lstatus; runloop::global()->exit(); });
	
	if (!ch.launch(child[0], child.skip(1)))
	{
		// TODO: failing launches occasionally don't print launch failed
		// I have not been able to determine the cause; it disappears as soon as I look at it
		puts("launch failed");
		return 1;
	}
	
	runloop::global()->enter();
	return ret;
}
