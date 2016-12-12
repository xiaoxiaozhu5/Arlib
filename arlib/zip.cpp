#define timegm timegm_goaway
#include "zip.h"
#include "test.h"
#include "crc32.h"
#include "endian.h"
#define MINIZ_HEADER_FILE_ONLY
#include "deps/miniz.c"
#include <time.h>
#undef timegm

//similar to mktime, but UTC timezone
//from http://stackoverflow.com/a/11324281
static time_t timegm(register struct tm * t)
/* struct tm to seconds since Unix epoch */
{
    register long year;
    register time_t result;
#define MONTHSPERYEAR   12      /* months per calendar year */
    static const int cumdays[MONTHSPERYEAR] =
        { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

    /*@ +matchanyintegral @*/
    year = 1900 + t->tm_year + t->tm_mon / MONTHSPERYEAR;
    result = (year - 1970) * 365 + cumdays[t->tm_mon % MONTHSPERYEAR];
    result += (year - 1968) / 4;
    result -= (year - 1900) / 100;
    result += (year - 1600) / 400;
    if ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0) &&
        (t->tm_mon % MONTHSPERYEAR) < 2)
        result--;
    result += t->tm_mday - 1;
    result *= 24;
    result += t->tm_hour;
    result *= 60;
    result += t->tm_min;
    result *= 60;
    result += t->tm_sec;
    if (t->tm_isdst == 1)
        result -= 3600;
    /*@ -matchanyintegral @*/
    return (result);
}

#ifdef _WIN32 // surprisingly, this is safe - gmtime() returns a thread local
#define gmtime_r(a,b) (*(b)=gmtime(a))
#endif

static time_t fromdosdate(uint32_t date)
{
	if (!date) return 0;
	
	struct tm tp = {
		/*tm_sec*/  (int)((date>>0)&31)<<1,
		/*tm_min*/  (int)(date>>5)&63,
		/*tm_hour*/ (int)(date>>11)&31,
		/*tm_mday*/ (int)(date>>16>>0)&31,
		/*tm_mon*/  (int)((date>>16>>5)&15) - 1,
		/*tm_year*/ (int)(date>>16>>9) + 1980 - 1900,
		/*tm_wday*/ 0,
		/*tm_yday*/ 0,
		/*tm_is_dst*/ false,
	};
	return timegm(&tp);
}

static uint32_t todosdate(time_t date)
{
	if (!date) return 0;
	
	struct tm tp;
	gmtime_r(&date, &tp);
	
	return (tp.tm_year-1980+1900)<<9<<16 |
	       (tp.tm_mon+1)<<5<<16 |
	       tp.tm_mday<<0<<16 |
	       tp.tm_hour<<11 |
	       tp.tm_min<<5 |
	       tp.tm_sec>>1;
}

class zip {
	struct locfhead {
		litend<uint32_t> signature;
		static const uint32_t signature_expected = 0x04034B50;
		litend<uint16_t> vermin;
		litend<uint16_t> bitflags;
		litend<uint16_t> compmethod;
		//litend<uint16_t> modtime;
		//litend<uint16_t> moddate; // merging these
		litend<uint32_t> moddate;
		litend<uint32_t> crc32;
		litend<uint32_t> size_comp;
		litend<uint32_t> size_decomp;
		litend<uint16_t> len_fname;
		litend<uint16_t> len_exfield;
	};
	static_assert(sizeof(locfhead)==30);
	
	struct centdirrec {
		litend<uint32_t> signature;
		static const uint32_t signature_expected = 0x02014B50;
		litend<uint16_t> verused;
		litend<uint16_t> vermin;
		litend<uint16_t> bitflags;
		litend<uint16_t> compmethod;
		//litend<uint16_t> modtime;
		//litend<uint16_t> moddate; // merging these
		litend<uint32_t> moddate;
		litend<uint32_t> crc32;
		litend<uint32_t> size_comp;
		litend<uint32_t> size_decomp;
		litend<uint16_t> len_fname;
		litend<uint16_t> len_exfield;
		litend<uint16_t> len_fcomment;
		litend<uint16_t> disknr;
		litend<uint16_t> attr_int;
		litend<uint32_t> attr_ext;
		litend<uint32_t> header_start;
	};
	static_assert(sizeof(centdirrec)==46);
	
	struct endofcdr {
		litend<uint32_t> signature;
		static const uint32_t signature_expected = 0x06054B50;
		litend<uint16_t> diskid_this;
		litend<uint16_t> diskid_cdrst;
		litend<uint16_t> numfiles_thisdisk;
		litend<uint16_t> numfiles;
		litend<uint32_t> cdrsize;
		litend<uint32_t> cdrstart_fromdisk;
		litend<uint16_t> zipcommentlen;
	};
	static_assert(sizeof(endofcdr)==22);
	
	
	endofcdr* getendofcdr(arrayview<byte> data)
	{
		for (size_t commentlen=0;commentlen<65536;commentlen++)
		{
			if (data.size() < sizeof(endofcdr)+commentlen) return NULL;
			
			size_t trystart = data.size()-sizeof(endofcdr)-commentlen;
			endofcdr* ret = (endofcdr*)data.slice(trystart, sizeof(endofcdr)).ptr();
			if (ret->signature == ret->signature_expected)
			{
				if (ret->diskid_this != 0) return NULL;
				return ret;
			}
		}
		
		return NULL;
	}
	
	centdirrec* getcdr(arrayview<byte> data, endofcdr* end)
	{
		if (end->cdrstart_fromdisk+sizeof(centdirrec) > data.size()) return NULL;
		centdirrec* ret = (centdirrec*)data.slice(end->cdrstart_fromdisk, sizeof(centdirrec)).ptr();
		if (ret->signature != ret->signature_expected) return NULL;
		return ret;
	}
	
	centdirrec* nextcdr(arrayview<byte> data, centdirrec* cdr)
	{
		size_t start = (uint8_t*)cdr - data.ptr();
		size_t len = sizeof(centdirrec) + cdr->len_fname + cdr->len_exfield + cdr->len_fcomment;
		if (start+len+sizeof(centdirrec) > data.size()) return NULL;
		
		centdirrec* next = (centdirrec*)data.slice(start+len, sizeof(centdirrec)).ptr();
		if (next->signature != next->signature_expected) return NULL;
		return next;
	}
	
	locfhead* geth(arrayview<byte> data, centdirrec* cdr)
	{
		if (cdr->header_start+sizeof(locfhead) > data.size()) return NULL;
		locfhead* ret = (locfhead*)data.slice(cdr->header_start, sizeof(locfhead)).ptr();
		if (ret->signature != ret->signature_expected) return NULL;
		return ret;
	}
	
	arrayview<byte> fh_fname(arrayview<byte> data, locfhead* fh)
	{
		size_t start = (uint8_t*)fh - data.ptr();
		if (start + sizeof(locfhead) + fh->len_fname > data.size()) return NULL;
		
		return data.slice(start+sizeof(locfhead), fh->len_fname);
	}
	
	arrayview<byte> fh_data(arrayview<byte> data, locfhead* fh)
	{
		size_t start = (uint8_t*)fh - data.ptr();
		size_t len = sizeof(locfhead) + fh->len_fname + fh->len_exfield;
		if (start+len+sizeof(locfhead) > data.size()) return NULL;
		
		return data.slice(start+len, fh->size_comp);
	}
	
	
	array<string> filenames;
	struct file {
		//would've put filenames here too, but then I can't return them in files()
		uint32_t decomplen;
		uint16_t method;
		array<byte> data;
		uint32_t crc32;
		uint32_t dosdate;
	};
	array<file> filedat;
	
	
public:
	zip() {}
	zip(arrayview<byte> data)
	{
		init(data);
	}
	
	bool init(arrayview<byte> data)
	{
		filenames.reset();
		filedat.reset();
		
		endofcdr* eod = getendofcdr(data);
		if (!eod) return false;
		
		centdirrec* cdr = getcdr(data, eod);
		while (cdr)
		{
			locfhead* fh = geth(data, cdr);
			if (!fh) return false;
			
			filenames.append(fh_fname(data, fh));
			file f = { fh->size_decomp, fh->compmethod, fh_data(data, fh), fh->crc32, fh->moddate };
			filedat.append(f);
			
			cdr = nextcdr(data, cdr);
		}
		
		return true;
	}
	
	//Invalidated whenever the file list changes.
	arrayview<string> files()
	{
		return filenames;
	}
	
private:
	size_t find_file(cstring name)
	{
		for (size_t i=0;i<filenames.size();i++)
		{
			if (filenames[i]==name) return i;
		}
		return (size_t)-1;
	}
	
	array<byte> unpackfiledat(file& f)
	{
		switch (f.method)
		{
			case 0:
			{
				return f.data;
			}
			case 8:
			{
				array<byte> ret;
				ret.resize(f.decomplen);
				size_t actualsize = tinfl_decompress_mem_to_mem(ret.ptr(), ret.size(), f.data.ptr(), f.data.size(), 0);
				if (actualsize != f.decomplen) return NULL;
				return ret;
			}
			default:
			{
				return NULL;
			}
		}
	}
public:
	
	array<byte> read(cstring name, time_t * time = NULL)
	{
		size_t i = find_file(name);
		if (i==(size_t)-1) return NULL;
		
		file& f = filedat[i];
		array<byte> ret = unpackfiledat(f);
		
		//APPNOTE.TXT specifies some bizarre generator constant, 0xdebb20e3
		//no idea how to use that, the normal crc32 (0xedb88320) works fine
		if (crc32(ret) != f.crc32) return NULL;
		if (time) *time = fromdosdate(f.dosdate);
		return ret;
	}
	
	//Writing a blank array deletes the file.
	void write(cstring name, arrayview<byte> data, time_t date = 0)
	{
		size_t i = find_file(name);
		if (!data)
		{
			if (i==(size_t)-1) return;
			else
			{
				filenames.remove(i);
				filedat.remove(i);
			}
			return;
		}
		
		if (i==(size_t)-1)
		{
			i = filenames.size();
			filenames.append(name);
		}
		file& f = filedat[i];
		f.decomplen = data.size();
		f.crc32 = crc32(data);
		if (date) f.dosdate = todosdate(date); // else leave unchanged, or leave as 0
		
		array<byte> comp;
		comp.resize(data.size());
		size_t complen = tdefl_compress_mem_to_mem(comp.ptr(), comp.size(), data.ptr(), data.size(), TDEFL_DEFAULT_MAX_PROBES);
		if (complen != 0 && complen < data.size())
		{
			comp.resize(complen);
			f.method = 8;
			f.data = std::move(comp);
		}
		else
		{
			f.method = 0;
			f.data = data;
		}
	}
	
private:
	static int fileminver(file& f)
	{
		if (f.method == 8) return 20;
		return 10;
	}
	
	static bool strascii(cstring s)
	{
		for (size_t i=0;i<s.length();i++)
		{
			if (s[i]>=128 || s[i]<0) return false;
		}
		return true;
	}
	
public:
	array<byte> pack()
	{
		array<byte> ret;
		
		array<size_t> headerstarts;
		for (size_t i=0;i<filenames.size();i++)
		{
			headerstarts.append(ret.size());
			
			file& f = filedat[i];
			locfhead h = {
				/*signature*/   locfhead::signature_expected,
				/*vermin*/      fileminver(f), // also contains host OS, but not important. and even if it was, pointless field
				/*bitflags*/    strascii(filenames[i]) ? 0 : 1<<11, // UTF-8 filenames
				/*compmethod*/  f.method,
				/*modtime*/     //merged
				/*moddate*/     f.dosdate,
				/*crc32*/       f.crc32,
				/*size_comp*/   f.data.size(),
				/*size_decomp*/ f.decomplen,
				/*len_fname*/   filenames[i].length(),
				/*len_exfield*/ 0,
			};
			arrayview<byte> hb((uint8_t*)&h, sizeof(h));
			ret += hb;
			ret += filenames[i].bytes();
			ret += f.data;
		}
		
		size_t cdrstart = ret.size();
		for (size_t i=0;i<filenames.size();i++)
		{
			file& f = filedat[i];
			centdirrec cdr = {
				/*signature*/   centdirrec::signature_expected,
				/*verused*/      63, // don't think anything really cares about this, just use latest
				/*vermin*/       fileminver(f),
				/*bitflags*/     strascii(filenames[i]) ? 0 : 1<<11,
				/*compmethod*/   f.method,
				/*modtime*/      //merged
				/*moddate*/      f.dosdate,
				/*crc32*/        f.crc32,
				/*size_comp*/    f.data.size(),
				/*size_decomp*/  f.decomplen,
				/*len_fname*/    filenames[i].length(),
				/*len_exfield*/  0,
				/*len_fcomment*/ 0,
				/*disknr*/       0,
				/*attr_int*/     0,
				/*attr_ext*/     0, // APPNOTE.TXT doesn't document this, other packers I checked are huge mazes. just gonna ignore it
				/*header_start*/ headerstarts[i],
			};
			arrayview<byte> cdrb((uint8_t*)&cdr, sizeof(cdr));
			ret += cdrb;
			ret += filenames[i].bytes();
		}
		size_t cdrend = ret.size();
		
		endofcdr eod = {
			/*signature*/         endofcdr::signature_expected,
			/*diskid_this*/       0,
			/*diskid_cdrst*/      0,
			/*numfiles_thisdisk*/ filenames.size(),
			/*numfiles*/          filenames.size(),
			/*cdrsize*/           cdrend-cdrstart,
			/*cdrstart_fromdisk*/ cdrstart,
			/*zipcommentlen*/     0,
		};
		arrayview<byte> eodb((uint8_t*)&eod, sizeof(eod));
		ret += eodb;
		
		return ret;
	}
};

#ifdef ARLIB_TEST
const uint8_t zipbytes[433] = {
	0x50,0x4B,0x03,0x04,0x0A,0x03,0x00,0x00,0x00,0x00,0x2B,0xA5,0x8A,0x49,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x65,0x6D,
	0x70,0x74,0x79,0x2E,0x74,0x78,0x74,0x50,0x4B,0x03,0x04,0x0A,0x03,0x00,0x00,0x00,
	0x00,0x25,0xA5,0x8A,0x49,0x85,0x11,0x4A,0x0D,0x0B,0x00,0x00,0x00,0x0B,0x00,0x00,
	0x00,0x09,0x00,0x00,0x00,0x68,0x65,0x6C,0x6C,0x6F,0x2E,0x74,0x78,0x74,0x68,0x65,
	0x6C,0x6C,0x6F,0x20,0x77,0x6F,0x72,0x6C,0x64,0x50,0x4B,0x03,0x04,0x14,0x03,0x00,
	0x00,0x08,0x00,0x30,0xA5,0x8A,0x49,0x80,0x7B,0x90,0xA9,0x76,0x00,0x00,0x00,0xDD,
	0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x69,0x6E,0x6E,0x65,0x72,0x2E,0x7A,0x69,0x70,
	0x75,0x8D,0x3D,0x0E,0x40,0x40,0x10,0x85,0x97,0x45,0x82,0x28,0xDC,0x40,0x23,0x0A,
	0x89,0x2B,0xA8,0xD1,0x68,0xF5,0x24,0x8A,0xF5,0x13,0xD9,0x04,0x1D,0x85,0xC6,0x39,
	0xB8,0xA7,0xC1,0x6E,0x6C,0xB2,0xF1,0x26,0x93,0x99,0x64,0xBE,0x37,0x2F,0xCF,0xB0,
	0x66,0x61,0x04,0x0A,0xCF,0x3D,0x41,0x82,0x4C,0xE8,0xAA,0xE9,0xE9,0x1C,0xD1,0x89,
	0x7E,0x98,0x0F,0xD8,0xE6,0xA6,0x8E,0x0D,0xBB,0xCD,0xB0,0xBA,0x22,0xA4,0xBB,0xB1,
	0x67,0xF1,0xC6,0x6E,0x20,0x65,0x9E,0x29,0x6A,0x8C,0xFF,0x5E,0x73,0x79,0xCB,0xB1,
	0x22,0x31,0x88,0xD9,0xE4,0x28,0xC9,0x16,0xF0,0xE0,0xD7,0xA6,0x1B,0xF7,0x41,0x85,
	0x6A,0x61,0x16,0x0F,0x76,0x01,0x50,0x4B,0x01,0x02,0x3F,0x03,0x0A,0x03,0x00,0x00,
	0x00,0x00,0x2B,0xA5,0x8A,0x49,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x80,0xA4,0x81,
	0x00,0x00,0x00,0x00,0x65,0x6D,0x70,0x74,0x79,0x2E,0x74,0x78,0x74,0x50,0x4B,0x01,
	0x02,0x3F,0x03,0x0A,0x03,0x00,0x00,0x00,0x00,0x25,0xA5,0x8A,0x49,0x85,0x11,0x4A,
	0x0D,0x0B,0x00,0x00,0x00,0x0B,0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x20,0x80,0xA4,0x81,0x27,0x00,0x00,0x00,0x68,0x65,0x6C,0x6C,0x6F,
	0x2E,0x74,0x78,0x74,0x50,0x4B,0x01,0x02,0x3F,0x03,0x14,0x03,0x00,0x00,0x08,0x00,
	0x30,0xA5,0x8A,0x49,0x80,0x7B,0x90,0xA9,0x76,0x00,0x00,0x00,0xDD,0x00,0x00,0x00,
	0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x80,0xA4,0x81,0x59,0x00,
	0x00,0x00,0x69,0x6E,0x6E,0x65,0x72,0x2E,0x7A,0x69,0x70,0x50,0x4B,0x05,0x06,0x00,
	0x00,0x00,0x00,0x03,0x00,0x03,0x00,0xA5,0x00,0x00,0x00,0xF6,0x00,0x00,0x00,0x00,
	0x00
};

template<typename T, typename U> bool member(arrayview<T> data, U item)
{
	for (size_t i=0;i<data.size();i++)
	{
		if (data[i] == item) return true;
	}
	return false;
}

test("timestamp conversion")
{
	time_t unix = 1481402470; // 2016-12-10 20:41:10
	uint32_t dos = 0x498AA525;
	assert_eq(fromdosdate(dos), unix);
	assert_eq(todosdate(unix), dos);
	
	//check 10000 random timestamps to ensure the conversion is lossless
	for (int i=0;i<10000;i++)
	{
		//pick a random unix timestamp in a suitable range
		//generation is in the unix domain, it's easier
		time_t unix_min = 631152000; // 1990-01-01 00:00:00
		time_t unix_max = 1577836800; // 2020-01-01 00:00:00
		time_t unix = unix_min + rand()%(unix_max-unix_min); // 100 million
		
		time_t dos = todosdate(unix);
		assert_eq(fromdosdate(dos), unix&~1);
	}
}

test("ZIP reading")
{
	zip z;
	assert(z.init(arrayview<byte>(zipbytes, sizeof(zipbytes))));
	
	arrayview<string> files = z.files();
	assert_eq(files.size(), 3);
	assert(member(files, "empty.txt"));
	assert(member(files, "hello.txt"));
	assert(member(files, "inner.zip"));
	
	time_t t;
	assert_eq(z.read("empty.txt").size(), 0);
	assert_eq(string(z.read("hello.txt", &t)), "hello world");
	assert_eq(t, 1481402470);
	
	zip z2;
	assert(z2.init(z.read("inner.zip")));
	
	files = z2.files();
	assert_eq(files.size(), 2);
	assert(member(files, "empty.txt"));
	assert(member(files, "hello.txt"));
	
	assert_eq(z2.read("empty.txt").size(), 0);
	assert_eq(string(z2.read("hello.txt")), "hello world");
}

static arrayview<byte> sb(const char * str) { return arrayview<byte>((uint8_t*)str, strlen(str)); }
test("ZIP writing")
{
	zip z;
	assert(z.init(arrayview<byte>(zipbytes, sizeof(zipbytes))));
	
	array<byte> zb = z.pack();
	assert_eq(zb.size(), sizeof(zipbytes));
	
	//puts("");
	//puts("---------");
	//for (int i=0;i<zb.size();i++)
	//{
	//	if (zb[i] == zipbytes[i]) printf("%.2X ",zb[i]);
	//	else printf("(%.2X|%.2X) ",zipbytes[i],zb[i]);
	//}
	//puts("");
	//puts("---------");
	
	z.write("hello.txt", sb("Hello World"));
	z.write("hello2.txt", sb("test"), 1000000000);
	z.write("empty.txt", sb(""));
	z.write("empty2.txt", sb(""));
	
	zip z2;
	assert(z2.init(z.pack()));
	
	arrayview<string> files = z2.files();
	assert_eq(files.size(), 3);
	assert(member(files, "hello2.txt"));
	assert(member(files, "hello.txt"));
	assert(member(files, "inner.zip"));
	
	time_t t;
	assert_eq(string(z2.read("hello.txt", &t)), "Hello World");
	assert_eq(t, 1481402470); // if timestamp isn't set in the call, don't update it
	assert_eq(string(z2.read("hello2.txt", &t)), "test");
	assert_eq(t, 1000000000);
	
	zip z3; // no initing
	array<byte> nuls;
	nuls.resize(65536);
	z3.write("nul.bin", nuls);
	
	array<byte> nulsc = z3.pack();
	assert(nulsc.size() < 1024);
	zip z4;
	assert(z4.init(nulsc));
	array<byte> nulsdc = z4.read("nul.bin");
	for (int i=0;i<65536;i++) assert_eq(nulsdc[i], 0);
}
#endif
