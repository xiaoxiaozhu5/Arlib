#include "file.h"

#ifdef __unix__
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>

#if defined(__x86_64__) || defined(__i386__)
static const long pagesize = 4096;
#else
#warning better hardcode the page size
static const long pagesize = sysconf(_SC_PAGESIZE);
#endif

fd_t fd_t::create_devnull()
{
	return ::open("/dev/null", O_RDWR|O_CLOEXEC);
}

namespace {
	class file_unix : public file::impl {
	public:
		int fd;
		
		file_unix(int fd) : fd(fd) {}
		
		size_t size() override
		{
			return lseek(fd, 0, SEEK_END);
		}
		bool resize(size_t newsize) override
		{
			return (ftruncate(this->fd, newsize) == 0);
		}
		
		size_t pread(arrayvieww<uint8_t> target, size_t start) override
		{
			size_t ret = ::pread(fd, target.ptr(), target.size(), start);
			if (ret<0) return 0;
			else return ret;
		}
		bool pwrite(arrayview<uint8_t> data, size_t start) override
		{
			size_t ret = ::pwrite(fd, data.ptr(), data.size(), start);
			if (ret<0) return 0;
			else return ret;
		}
		
		/*private*/ arrayvieww<uint8_t> mmap(bool writable, size_t start, size_t len)
		{
			//TODO: for small things (64KB? 1MB?), use malloc, it's faster
			//http://lkml.iu.edu/hypermail/linux/kernel/0004.0/0728.html
			size_t offset = start % pagesize;
			void* data = ::mmap(NULL, len+offset, writable ? PROT_WRITE|PROT_READ : PROT_READ, MAP_SHARED, this->fd, start-offset);
			if (data == MAP_FAILED) return NULL;
			return arrayvieww<uint8_t>((uint8_t*)data+offset, len);
		}
		
		arrayview<uint8_t> mmap(size_t start, size_t len) override { return mmap(false, start, len); }
		void unmap(arrayview<uint8_t> data) override
		{
			size_t offset = (uintptr_t)data.ptr() % pagesize;
			munmap((char*)data.ptr()-offset, data.size()+offset);
		}
		
		arrayvieww<uint8_t> mmapw(size_t start, size_t len) override { return mmap(true, start, len); }
		bool unmapw(arrayvieww<uint8_t> data) override
		{
			unmap(data);
			// manpage documents no errors for the case where file writing fails, gotta assume it never does
			return true;
		}
		
		~file_unix() { close(fd); }
	};
}

file::impl* file::open_impl_fs(cstring filename, mode m)
{
	static const int flags[] = { O_RDONLY, O_RDWR|O_CREAT, O_RDWR, O_RDWR|O_CREAT|O_TRUNC, O_RDWR|O_CREAT|O_EXCL };
	int fd = ::open(filename.c_str(), flags[m]|O_CLOEXEC, 0644);
	if (fd<0) return NULL;
	
	struct stat st;
	fstat(fd, &st);
	if (!S_ISREG(st.st_mode)) // no opening directories
	{
		::close(fd);
		return NULL;
	}
	
	return new file_unix(fd);
}

bool file::mkdir_fs(cstring filename)
{
	int ret = ::mkdir(filename.c_str(), 0755);
	if (ret==0) return true;
	if (ret==-1 && errno==EEXIST)
	{
		struct stat st;
		if (lstat(filename.c_str(), &st) < 0) return false;
		return (S_ISDIR(st.st_mode));
	}
	return false;
}

bool file::unlink_fs(cstring filename)
{
	int ret = ::unlink(filename.c_str());
	return ret==0 || (ret==-1 && errno==ENOENT);
}

array<string> file::listdir(cstring path)
{
	DIR* dir = opendir(path.c_str());
	if (!dir) return NULL;
	
	array<string> ret;
	dirent* ent;
	while ((ent = readdir(dir)))
	{
		// don't use strcmp, its constant overhead is suboptimal at best
		// (it's inlined in gcc -O2, but not -Os. And not in clang. And it's inlined poorly.)
		if (UNLIKELY(ent->d_name[0] == '.'))
		{
			if (ent->d_name[1] == '\0') continue;
			if (ent->d_name[1] == '.' && ent->d_name[2] == '\0') continue;
		}
		
		string childpath = path + ent->d_name;
		if (ent->d_type == DT_UNKNOWN)
		{
			struct stat st;
			stat(childpath, &st);
			if (S_ISDIR(st.st_mode)) childpath += "/";
		}
		else if (ent->d_type == DT_DIR) childpath += "/";
		ret.append(std::move(childpath));
	}
	closedir(dir);
	return ret;
}

string file::readlink(cstrnul path)
{
	char buf[256];
	
	ssize_t r = ::readlink(path, buf, sizeof(buf));
	if (r <= 0) return "";
	
	if ((size_t)r < sizeof(buf)-1)
	{
		buf[r] = '\0'; // readlink doesn't nul terminate
		return buf;
	}
	else
	{
		size_t buflen = sizeof(buf);
		char* ptr = NULL;
	again:
		buflen *= 2;
		ptr = xrealloc(ptr, buflen);
		
		ssize_t r = ::readlink(path, ptr, buflen);
		if (r <= 0) return "";
		if ((size_t)r >= buflen-1) goto again;
		
		ptr[r] = '\0';
		return string::create_usurp(ptr);
	}
}


/*
static char* g_cwd;
cstring file::cwd() { return g_cwd; }

void arlib_init_file()
{
	string cwd = string::create_usurp(getcwd(NULL, 0));
	if (!cwd.endswith("/")) cwd += "/";
	g_cwd = strdup(cwd);
	
	//char * cwd_init_tmp=getcwd(NULL, 0);
	//char * cwdend=strrchr(cwd_init_tmp, '/');
	//if (!cwdend) cwd_init="/";
	//else if (cwdend[1]=='/') cwd_init=cwd_init_tmp;
	//else
	//{
	//	size_t cwdlen=strlen(cwd_init_tmp);
	//	char * cwd_init_fixed=malloc(cwdlen+1+1);
	//	memcpy(cwd_init_fixed, cwd_init_tmp, cwdlen);
	//	cwd_init_fixed[cwdlen+0]='/';
	//	cwd_init_fixed[cwdlen+1]='\0';
	//	cwd_init=cwd_init_fixed;
	//	free(cwd_init_tmp);
	//}
	
//	//disable cwd
//	//try a couple of useless directories and hope one of them works
//	//this seems to be the best one:
//	//- even root can't create files here
//	//- it contains no files with a plausible name on a standard Ubuntu box
//	//    (I have an ath9k-phy0, and a bunch of stuff with colons, nothing will ever want those filenames)
//	//- a wild write will not do anything dangerous except turn on some lamps
//	!chdir("/sys/class/leds/") ||
//		//the rest are in case it's not accessible (weird chroot? not linux?), so try some random things
//		!chdir("/sys/") ||
//		!chdir("/dev/") ||
//		!chdir("/home/") ||
//		!chdir("/tmp/") ||
//		!chdir("/");
//	cwd_bogus = getcwd(NULL, 0); // POSIX does not specify getcwd(NULL), it's Linux-specific
}
*/



bool file2::openat(int dirfd, cstrnul filename, mode m)
{
	reset();
	static const int flags[] = {
		O_RDONLY              |O_CLOEXEC, // m_read
		O_RDWR|O_CREAT        |O_CLOEXEC, // m_write
		O_RDWR                |O_CLOEXEC, // m_wr_existing
		O_RDWR|O_CREAT|O_TRUNC|O_CLOEXEC, // m_replace
		O_RDWR|O_CREAT|O_EXCL |O_CLOEXEC  // m_create_excl
	};
	fd = ::openat(dirfd, filename, flags[m&7], 0644);
	if (!fd.valid())
		return false;
	
	if (m & m_exclusive)
	{
		int op;
		if ((m&7) == m_read)
			op = LOCK_SH|LOCK_NB;
		else
			op = LOCK_EX|LOCK_NB;
		if (flock(fd, op) < 0)
			close();
	}
	
	return true;
}

bool file2::open(cstrnul filename, mode m)
{
	return openat(AT_FDCWD, filename, m);
}

#define SMALL_MAP 16384
void file2::mmap_t::map(bytesr& by, file2& src, bool writable, bool small)
{
	by = nullptr;
	
	size_t size = src.size();
	if (!size) return;
	
	uint8_t * data;
	if (small && size < SMALL_MAP)
	{
		data = xmalloc(size);
		if (src.read(bytesw(data, size)) != size)
		{
			free(data);
			return;
		}
	}
	else
	{
		data = (uint8_t*)::mmap(NULL, size, PROT_READ | (writable * PROT_WRITE), MAP_SHARED, src.fd, 0);
		if (data == MAP_FAILED)
			return;
	}
	
	by = { data, size };
}
void file2::mmap_t::unmap(bytesr& by, bool small)
{
	size_t size = by.size();
	if (size == 0)
		{}
	else if (small && size < SMALL_MAP)
		free((uint8_t*)by.ptr());
	else
		munmap((uint8_t*)by.ptr(), by.size());
	by = nullptr;
}


directory::directory(cstrnul path)
{
	if (file::path_corrupt(path))
		return;
	fd = open(path, O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_DIRECTORY);
	iter_next(true);
}
bool directory::dent_is_dir()
{
	dirent64* dent_raw = current_dirent64();
	if (dent_raw->d_type == DT_DIR)
		return true;
	if (dent_raw->d_type == DT_REG)
		return false;
	struct stat st;
	if (fstatat(fd, dent_raw->d_name, &st, 0) < 0)
		return false;
	return S_ISDIR(st.st_mode);
}
void directory::iter_next(bool initial)
{
	if (initial)
		goto initial;
again:
	buf_at += current_dirent64()->d_reclen;
	if (buf_at == buf_len)
	{
	initial:
		buf_at = 0;
		int len = getdents64(fd, buf, sizeof(buf));
		if (len < 0)
			len = 0;
		buf_len = len;
		if (len <= 0)
			return;
	}
	const char * name = current_dirent64()->d_name;
	if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
		goto again;
}
file2 directory::open_file_inner(const char * path, file2::mode m)
{
	file2 ret;
	ret.openat(fd, path, m);
	return ret;
}
directory directory::open_dir_inner(const char * path)
{
	return directory(openat(fd, path, O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_DIRECTORY));
}
#endif
