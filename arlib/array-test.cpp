#include "array.h"
#include "test.h"

#ifdef ARLIB_TEST
test("array")
{
	{
		array<int> x = { 1, 2, 3 };
		assert_eq(x[0], 1);
		assert_eq(x[1], 2);
		assert_eq(x[2], 3);
	}
}


static string tostring(array<bool> b)
{
	string ret;
	for (size_t i=0;i<b.size();i++) ret += b[i]?"1":"0";
	return ret;
}
static string ones_zeroes(int ones, int zeroes)
{
	string ret;
	for (int i=0;i<ones;i++) ret+="1";
	for (int i=0;i<zeroes;i++) ret+="0";
	return ret;
}
test("array<bool>")
{
	for (int up=0;up<128;up+=13)
	for (int down=0;down<=up;down+=13)
	{
		array<bool> b;
		for (int i=0;i<up;i++)
		{
			assert_eq(b.size(), i);
			b.append(true);
		}
		assert_eq(b.size(), up);
		
		b.resize(down);
		assert_eq(tostring(b), ones_zeroes(down, 0));
		assert_eq(b.size(), down);
		for (int i=0;i<128;i++)
		{
			if (i<down) assert(b[i]);
			else assert(!b[i]);
		}
		assert_eq(b.size(), down);
		
		b.resize(up);
		assert_eq(tostring(b), ones_zeroes(down, up-down));
		assert_eq(b.size(), up);
		for (int i=0;i<128;i++)
		{
			if (i<down) assert(b[i]);
			else assert(!b[i]);
		}
	}
	
	{
		array<bool> b;
		b.resize(8);
		b[0] = true;
		b[1] = true;
		b[2] = true;
		b[3] = true;
		b[4] = true;
		b[5] = true;
		b[6] = true;
		b[7] = true;
		
		b.reset();
		b.resize(16);
	}
}
#endif
