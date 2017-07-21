#include "string.h"
#include "test.h"

void string::resize(uint32_t newlen)
{
	switch (!inlined()<<1 | (newlen>max_inline))
	{
	case 0: // small->small
		{
			m_inline[newlen] = '\0';
			m_inline_len = max_inline-newlen;
		}
		break;
	case 1: // small->big
		{
			uint8_t* newptr = malloc(bytes_for(newlen));
			memcpy(newptr, m_inline, max_inline);
			newptr[newlen] = '\0';
			m_data = newptr;
			m_len = newlen;
			m_nul = true;
			
			m_inline_len = -1;
		}
		break;
	case 2: // big->small
		{
			uint8_t* oldptr = m_data;
			memcpy(m_inline, oldptr, newlen);
			free(oldptr);
			m_inline[newlen] = '\0';
			m_inline_len = max_inline-newlen;
		}
		break;
	case 3: // big->big
		{
			m_data = realloc(m_data, bytes_for(newlen));
			m_data[newlen] = '\0';
			m_len = newlen;
		}
		break;
	}
}

void string::init_from(arrayview<byte> data)
{
	const uint8_t * str = data.ptr();
	uint32_t len = data.size();
	
	if (len <= max_inline)
	{
		memcpy(m_inline, str, len);
		m_inline[len] = '\0';
		m_inline_len = max_inline-len;
	}
	else
	{
		m_inline_len = -1;
		
		m_data = malloc(bytes_for(len));
		memcpy(m_data, str, len);
		m_data[len]='\0';
		
		m_len = len;
		m_nul = true;
	}
}

void string::replace_set(int32_t pos, int32_t len, cstring newdat)
{
	//if newdat is a cstring backed by this, modifying this invalidates that string, so it's illegal
	//if newdat equals this, then the memmoves will mess things up
	if (this == &newdat)
	{
		string copy = newdat;
		replace_set(pos, len, copy);
		return;
	}
	
	uint32_t prevlength = length();
	uint32_t newlength = newdat.length();
	
	if (newlength < prevlength)
	{
		memmove(ptr()+pos+newlength, ptr()+pos+len, prevlength-len-pos);
		resize(prevlength - len + newlength);
	}
	if (newlength > prevlength)
	{
		resize(prevlength - len + newlength);
		memmove(ptr()+pos+newlength, ptr()+pos+len, prevlength-len-pos);
	}
	
	memcpy(ptr()+pos, newdat.ptr(), newlength);
}

string cstring::replace(cstring in, cstring out)
{
	size_t outlen = length();
	
	if (in.length() != out.length())
	{
		const uint8_t* haystack = ptr();
		const uint8_t* haystackend = ptr()+length();
		while (true)
		{
			haystack = (uint8_t*)memmem(haystack, haystackend-haystack, in.ptr(), in.length());
			if (!haystack) break;
			
			haystack += in.length();
			outlen += out.length(); // outlen-inlen is type uint - bad idea
			outlen -= in.length();
		}
	}
	
	string ret;
	uint8_t* retptr = ret.construct(outlen).ptr();
	
	const uint8_t* prev = ptr();
	const uint8_t* myend = ptr()+length();
	while (true)
	{
		const uint8_t* match = (uint8_t*)memmem(prev, myend-prev, in.ptr(), in.length());
		if (!match) break;
		
		memcpy(retptr, prev, match-prev);
		retptr += match-prev;
		prev = match + in.length();
		
		memcpy(retptr, out.ptr(), out.length());
		retptr += out.length();
	}
	memcpy(retptr, prev, myend-prev);
	
	return ret;
}

array<cstring> cstring::csplit(cstring sep, size_t limit) const
{
	array<cstring> ret;
	const uint8_t * data = ptr();
	const uint8_t * dataend = ptr()+length();
	
	while (ret.size() < limit)
	{
		const uint8_t * next = (uint8_t*)memmem(data, dataend-data, sep.ptr(), sep.length());
		if (!next) break;
		ret.append(arrayview<uint8_t>(data, next-data));
		data = next+sep.length();
	}
	ret.append(arrayview<uint8_t>(data, dataend-data));
	return ret;
}

array<cstring> cstring::crsplit(cstring sep, size_t limit) const
{
	array<cstring> ret;
	const uint8_t * datastart = ptr();
	const uint8_t * data = ptr()+length();
	
	const uint8_t * sepp = sep.ptr();
	size_t sepl = sep.length();
	
	while (ret.size() < limit)
	{
		if (datastart+sepl > data) break;
		const uint8_t * next = data-sepl;
		while (memcmp(next, sepp, sepl)!=0)
		{
			if (datastart==next) goto done;
			next--;
		}
		ret.insert(0, arrayview<uint8_t>(next+sepl, data-(next+sepl)));
		data = next;
	}
done:
	ret.insert(0, arrayview<uint8_t>(datastart, data-datastart));
	return ret;
}

string string::codepoint(uint32_t cp)
{
	string ret;
	if (cp<=0x7F)
	{
		ret[0] = cp;
	}
	else if (cp<=0x07FF)
	{
		ret[0] = (((cp>> 6)     )|0xC0);
		ret[1] = (((cp    )&0x3F)|0x80);
	}
	else if (cp>=0xD800 && cp<=0xDFFF) return "\xEF\xBF\xBD";
	else if (cp<=0xFFFF)
	{
		ret[0] = (((cp>>12)&0x0F)|0xE0);
		ret[1] = (((cp>>6 )&0x3F)|0x80);
		ret[2] = (((cp    )&0x3F)|0x80);
	}
	else if (cp<=0x10FFFF)
	{
		ret[0] = (((cp>>18)&0x07)|0xF0);
		ret[1] = (((cp>>12)&0x3F)|0x80);
		ret[2] = (((cp>>6 )&0x3F)|0x80);
		ret[3] = (((cp    )&0x3F)|0x80);
	}
	else return "\xEF\xBF\xBD";
	return ret;
}

#define X 0xFFFD
static const uint16_t windows1252tab[32]={
	//00 to 7F map to themselves
	0x20AC, X,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
	0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, X,      0x017D, X,     
	X,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, X,      0x017E, 0x0178,
	//A0 to FF map to themselves
};
#undef X

static string fromlatin1(cstring in, bool windows1252)
{
	string out;
	for (int i=0;in[i];i++)
	{
		uint8_t ch = in[i];
		if (ch < 0x80) out += ch;
		else if (ch < 0xA0 && windows1252) out += string::codepoint(windows1252tab[ch-0x80]);
		else if (ch < 0xA0) out+="\xEF\xBF\xBD";
		else out += string::codepoint(ch);
	}
	return out;
}

string cstring::fromlatin1()      const { return ::fromlatin1(*this, false); }
string cstring::fromwindows1252() const { return ::fromlatin1(*this, true); }

test()
{
	{
		const char * g = "hi";
		
		string a = g;
		a[2]='!';
		string b = a;
		assert_eq(b, "hi!");
		a[3]='!';
		assert_eq(a, "hi!!");
		assert_eq(b, "hi!");
		a = b;
		assert_eq(a, "hi!");
		assert_eq(b, "hi!");
		
		
		//a.replace(1,1, "ello");
		//assert_eq(a, "hello!");
		//assert_eq(a.substr(1,3), "el");
		//a.replace(1,4, "i");
		//assert_eq(a, "hi!");
		//a.replace(1,2, "ey");
		//assert_eq(a, "hey");
		//
		//assert_eq(a.substr(2,2), "");
	}
	
	{
		//ensure it works properly when going across the inline-outline border
		string a = "123456789012345";
		a += "678";
		assert_eq(a, "123456789012345678");
		a += (const char*)a;
		string b = a;
		assert_eq(a, "123456789012345678123456789012345678");
		assert_eq(a.substr(1,3), "23");
		assert_eq(b, "123456789012345678123456789012345678");
		assert_eq(a.substr(1,21), "23456789012345678123");
		assert_eq(a.substr(1,~1), "2345678901234567812345678901234567");
		assert_eq(a.substr(2,2), "");
		assert_eq(a.substr(22,22), "");
		//a.replace(1,5, "-");
		//assert_eq(a, "1-789012345678123456789012345678");
		//a.replace(4,20, "-");
		//assert_eq(a, "1-78-12345678");
	}
	
	{
		string a = "12345678";
		a += a;
		a += a;
		string b = a;
		a = "";
		assert_eq(b, "12345678123456781234567812345678");
	}
	
	{
		string a = "1abc1de1fgh1";
		assert_eq(a.replace("1", ""), "abcdefgh");
		assert_eq(a.replace("1", "@"), "@abc@de@fgh@");
		assert_eq(a.replace("1", "@@"), "@@abc@@de@@fgh@@");
	}
	
	{
		//this has thrown valgrind errors due to derpy allocations
		string a = "abcdefghijklmnopqrstuvwxyz";
		string b = a; // needs an extra reference
		a += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		assert_eq(a, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
		assert_eq(b, "abcdefghijklmnopqrstuvwxyz");
	}
	
	{
		string a = "aaaaaaaaaaaaaaaa";
		a[0] = 'b';
		assert_eq(a, "baaaaaaaaaaaaaaa");
	}
	
	{
		arrayview<byte> a((uint8_t*)"123", 3);
		string b = "["+string(a)+"]";
		string c = "["+cstring(a)+"]";
		assert_eq(b, "[123]");
		assert_eq(c, "[123]");
	}
	
	{
		string a;
		a = "192.168.0.1";
		assert_eq(a.split(".").join("/"), "192/168/0/1");
		assert_eq(a.split<1>(".").join("/"), "192/168.0.1");
		assert_eq(a.rsplit(".").join("/"), "192/168/0/1");
		assert_eq(a.rsplit<1>(".").join("/"), "192.168.0/1");
		
		a = "baaaaaaaaaaaaaaa";
		assert_eq(a.split("a").join("."), "b...............");
		assert_eq(a.split("aa").join("."), "b.......a");
		assert_eq(a.split<1>("aa").join("."), "b.aaaaaaaaaaaaa");
		assert_eq(a.split<1>("x").join("."), "baaaaaaaaaaaaaaa");
		
		a = "aaaaaaaaaaaaaaab";
		assert_eq(a.split("a").join("."), "...............b");
		assert_eq(a.split("aa").join("."), ".......ab");
		assert_eq(a.split<1>("aa").join("."), ".aaaaaaaaaaaaab");
		assert_eq(a.split<1>("x").join("."), "aaaaaaaaaaaaaaab");
		
		a = "baaaaaaaaaaaaaaa";
		assert_eq(a.rsplit("a").join("."), "b...............");
		assert_eq(a.rsplit("aa").join("."), "ba.......");
		assert_eq(a.rsplit<1>("aa").join("."), "baaaaaaaaaaaaa.");
		assert_eq(a.rsplit<1>("x").join("."), "baaaaaaaaaaaaaaa");
		
		a = "aaaaaaaaaaaaaaab";
		assert_eq(a.rsplit("a").join("."), "...............b");
		assert_eq(a.rsplit("aa").join("."), "a.......b");
		assert_eq(a.rsplit<1>("aa").join("."), "aaaaaaaaaaaaa.b");
		assert_eq(a.rsplit<1>("x").join("."), "aaaaaaaaaaaaaaab");
	}
	
	{
		uint8_t in[] = { 0xF8, 0x80 };
		cstring instr = arrayview<byte>(in);
		assert_eq(instr.fromlatin1(), u8"ø\uFFFD");
		assert_eq(instr.fromwindows1252(), "ø€");
	}
}
