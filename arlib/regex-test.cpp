// to test std::regex too
// all bugs I could find are reported (either by me or someone else), but as of writing, many of them remain unfixed
#if defined(ARLIB_TEST) && 0
#include <regex>

template<typename... Args>
static void testfn_std(const char * re, const char * input, size_t input_len, Args... args)
{
	const char * exp_capture_raw[] = { args... };
	std::string exp_capture;
	for (size_t n=0;n<sizeof...(args);n++)
	{
		if (n) exp_capture += "/";
		exp_capture += (exp_capture_raw[n] ? exp_capture_raw[n] : "(null)");
	}
	if (!exp_capture_raw[0])
		exp_capture = "(fail)"; // std::regex doesn't return capture group count on failure, the match object is empty
	
	std::smatch m;
	std::string in { input, input + input_len };
	bool matched;
	try {
		matched = std::regex_search(in, m, std::regex(std::string(re)), std::regex_constants::match_continuous);
	} catch (const std::exception&) {
		printf("FAIL should compile %s\n", re);
		return;
	}
	
	std::string capture;
	if (!matched)
		capture = "(fail)";
	else
	{
		for (size_t i=0;i<m.size();i++)
		{
			if (i) capture += "/";
			if (m[i].matched)
				capture += m[i].str();
			else
				capture += "(null)";
		}
	}
	
	// no convenient function for this in std::, do it manually
	while (true)
	{
		size_t idx = capture.find('\0');
		if (idx == std::string::npos) break;
		capture.replace(idx, 1, "NUL");
	}
	
	if (capture != exp_capture)
		printf("FAIL %s :: %s :: ret=%s :: exp=%s\n", re, input, capture.c_str(), exp_capture.c_str());
}
static void testfail_std(const char * re)
{
	try {
		std::regex(std::string(re));
		printf("FAIL compile %s\n", re);
	} catch (const std::exception&) {
	}
}
#else
// empty
static void testfn_std(...) {}
static void testfail_std(...) {}
#endif

#include "regex.h"
#include "test.h"

template<size_t n>
static string stringify_match(const regex::match_t<n>& match)
{
	string ret;
	for (size_t i=0;i<match.size();i++)
	{
		if (i) ret += "/";
		auto actual = match[i];
		if (actual.start && actual.end && actual.end >= actual.start)
			ret += cstring(arrayview<char>(actual.start, actual.end-actual.start)).replace(string::nul(), "NUL");
		else if (!actual.start && !actual.end) ret += "(null)";
		else
		{
printf("X %s %s\n", actual.start, actual.end);
			ret += "<ERROR>"; // this means bug somewhere in the matcher, for example forgot to rewind a capture point
		}
	}
	return ret;
}

template<typename... Args>
static void testfn(const char * re, const char * input, size_t input_len, Args... args)
{
	regex rx;
	assert(rx.parse(re));
	regex::match_t<5> result = rx.match(input, input+input_len);
	
	const char * exp_capture_raw[] = { args... };
	string exp_capture;
	for (size_t n=0;n<sizeof...(args);n++)
	{
		if (n) exp_capture += "/";
		exp_capture += (exp_capture_raw[n] ? exp_capture_raw[n] : "(null)");
	}
	
	string capture = stringify_match(result);
	
	test_nothrow
		testctx(re)
			assert_eq(capture, exp_capture);
	
	assert(rx.parse_unoptimized(re));
	regex::match_t<5> result2 = rx.match(input, input+input_len);
	assert_eq(capture, stringify_match(result2));
	
	testfn_std(re, input, input_len, args...);
}
#define test1(exp, input, ...) testcall(testfn(exp, input, sizeof(input)-1, __VA_ARGS__))
#define test1fail(exp) do { regex rg; assert(!rg.parse(exp)); testfail_std(exp); } while(0)
#define test1fail_std(exp, input, ...) do { regex rg; assert(!rg.parse(exp)); testfn_std(exp, input, sizeof(input)-1, __VA_ARGS__); } while(0)

test("regex - basic bytes", "string", "regex")
{
	// . is only tested later
	test1("abc", "abc", "abc");
	test1("abc", "abcd", "abc");
	test1("aaabc", "aaabcd", "aaabc");
	test1("abc", "def", nullptr);
	test1("[Aa]", "A", "A");
	test1("[Aa][Bb][Cc]", "Abc", "Abc");
	test1("[Aa][Bb][Cc]", "bcd", nullptr);
	test1("[abc-]", "b", "b");
	test1("[a-b-c-d]", "c", "c");
	test1("[a-b-c-d]", "-", "-");
	test1("[]", "a", nullptr);
	test1("[^]", "a", "a");
	test1("\xC3[\xB8\x98]", "ø", "ø");
	test1("a\nb", "a\nb", "a\nb");
	test1("a[\n]b", "a\nb", "a\nb");
	test1("[abc]@[def]", "b@d", "b@d"); // try to tickle the unique_bytes cross-chunk case
	test1("a[abc]c", "abc", "abc");
	test1("a[a-z]c", "abc", "abc");
	test1("a[a-zA-Z]c", "aBc", "aBc");
	test1("[\\x00-\\xFF]+", "knäckebröd", "knäckebröd");
	test1fail("a[A-]]c");
	test1fail("[b-a]");
	test1fail("[");
	test1fail("]");
	test1("a\\x62c", "abc", "abc");
	test1("\\xC3\\xB8", "ø", "ø");
	test1("a\\nb", "a\nb", "a\nb");
	test1("a[\\n]b", "a\nb", "a\nb");
	test1("a\\sb", "a\nb", "a\nb");
	test1("a\\Db", "a\nb", "a\nb");
	test1("\\ca", "\x01", "\x01"); // \c is browser only in latest spec, but was present in main on js version 5
	test1("\\cA", "\x01", "\x01");
	test1fail("\\c+");
	test1("\\$", "$", "$");
	test1("a[\\w]c", "abc", "abc");
	test1("a\\wc", "abc", "abc");
	test1("[abc]\\?[def]", "b?d", "b?d");
	test1fail("[\\w-a]");
	test1fail("[a-\\w]");
	test1fail_std("\\Z", "Z", "Z"); // std::regex accepts invalid letter escapes and treats them as identity
	test1fail_std("[\\Z]", "Z", "Z");
	test1("[\\b]", "\b", "\b");
	test1fail("\\");
	test1fail("\\c");
	test1fail("\\x");
	test1fail("\\x0");
	test1("[\\W\\D]", "a", "a");
	test1("\\0", "a", nullptr);
	test1("\\0", "\0", "NUL");
	test1fail("\\00"); // 00 isn't a DecimalIntegerLiteral (they're 0|[1-9][0-9]*), and lookahead can't be digit, so this is illegal
	test1fail("\\01"); // https://262.ecma-international.org/5.1/#sec-15.10.2.11
	test1("[\\0]", "\0", "NUL");
	test1fail("[\\1]"); // no backreferences in 
	test1fail("[\\01]");
	test1fail("[\\00]");
	test1fail("[\\0-\\00]");
}
test("regex - repeats", "string", "regex")
{
	test1("a?a?a?a?a?bc", "aaabcd", "aaabc");
	test1("a?a?a?a?a?bc", "aaaaaabcd", nullptr);
	test1("a?a?a?a?a?", "aaaaaabcd", "aaaaa");
	test1("a??a??a??a??a??", "aaaaaabcd", "");
	test1("a*b*c*", "aaabcccd", "aaabccc");
	test1("a*b*c*?", "aaabcccd", "aaab");
	test1("a+b+c+", "aaabcccd", "aaabccc");
	test1("a+b+c+?", "aaabcccd", "aaabc");
	test1("a?", "aaa", "a");
	test1("a??", "aaa", "");
	test1("a+", "aaa", "aaa");
	test1("a+?", "aaa", "a");
	test1fail("a++");
	test1fail("a{2}{2}");
	test1fail("a???");
	test1("a*", "aaa", "aaa");
	test1("a*?", "aaa", "");
	test1("a*b+c?d", "abcd", "abcd");
	test1("ab*c", "abbc", "abbc");
	test1("a{2,5}bc", "aaaabcd", "aaaabc");
	test1("a{2,5}", "aaaabcd", "aaaa");
	test1("a{2,5}?", "aaaabcd", "aa");
	test1("a{2,5}?b", "aaaabcd", "aaaab");
	test1("a{2,5}?b", "aaaaaabcd", nullptr);
	test1("a{5}", "aaaaaabcd", "aaaaa");
	test1("a{5}?", "aaaaaabcd", "aaaaa"); // lazy quantifier has no effect on fixed-width repeats
	test1("a{1}?", "aaaaaabcd", "a");
	test1fail("a{,5}");
	test1("a{0,5}", "aaa", "aaa");
	test1("a{0,5}", "aaabc", "aaa");
	test1("a{0,5}?", "aaa", "");
	test1("a{0,5}?", "aaabc", "");
	test1("a{0,5}bc", "aaa", nullptr);
	test1("a{0,5}bc", "aaabc", "aaabc");
	test1("a{0,5}?bc", "aaa", nullptr);
	test1("a{0,5}?bc", "aaabc", "aaabc");
	test1("a{3,}?", "aaaaa", "aaa");
	test1("a{3,}", "aaaaa", "aaaaa");
	test1("ax{0}bc", "abc", "abc");
	test1fail("{");
	test1fail("}");
	test1fail("a{");
	test1fail("a{1");
	test1fail("a{1,");
	test1("cd?e?f+g*hi", "cdfffghi", "cdfffghi");
}
test("regex - groups", "string", "regex")
{
	// both capturing and not
	test1("(?:a)b", "ab", "ab");
	test1("(ab)*", "ababababa", "abababab", "ab");
	test1("(ab){3}", "ababababa", "ababab", "ab");
	test1("(ab)c", "abc", "abc", "ab");
	test1("(a){5}", "aaaaaa", "aaaaa", "a");
	test1("([ab])*", "ab", "ab", "b");
	test1("([ab])*", "a", "a", "a");
	test1("([ab])*", "", "", nullptr);
	test1("([ab])+?c", "abc", "abc", "b");
	test1("((.)..)+", "12345678", "123456", "456", "4");
	test1("((.)..)+...", "12345678", "123456", "123", "1");
	test1("((.)..){1,5}", "12345678", "123456", "456", "4");
	test1("((.)..){1,5}...", "12345678", "123456", "123", "1");
	test1("(?:aa)+(?:aaa)+", "aaaaaaaaaa", "aaaaaaaaa"); // the longest legal match isn't necessarily the right one
	test1("a?a?(?:aa)?", "aaa", "aa"); // like the above, but with no + or *
	test1("(((a)))*?a", "aa", "a", nullptr, nullptr, nullptr); // group wasn't touched - should be null
}
test("regex - alternations", "string", "regex")
{
	test1("abc|def", "abcx", "abc");
	test1("abc|def", "defx", "def");
	test1("abc|def", "ghix", nullptr);
	test1("(abc|def)", "abcx", "abc", "abc");
	test1("(abc|def)", "defx", "def", "def");
	test1("(abc|def)", "ghix", nullptr, nullptr);
	test1("(abc|abcd)de", "abcde",  "abcde",  "abc");
	test1("(abc|abcd)de", "abcdde", "abcdde", "abcd");
	test1("(abcd|abc)de", "abcde",  "abcde",  "abc");
	test1("(abcd|abc)de", "abcdde", "abcdde", "abcd");
	test1("(abc|def)(ghi|jkl)", "abcghix", "abcghi", "abc", "ghi");
	test1("(abc|def)(ghi|jkl)", "abcjklx", "abcjkl", "abc", "jkl");
	test1("(abc|def)(ghi|jkl)", "defghix", "defghi", "def", "ghi");
	test1("(abc|def)(ghi|jkl)", "defjklx", "defjkl", "def", "jkl");
	test1("(abc|def)(ghi|jkl)", "abcdef", nullptr, nullptr, nullptr);
	test1("(abc|def)(ghi|jkl)", "abcgkl", nullptr, nullptr, nullptr);
	test1("a|b|cd", "b", "b");
	test1("a|b|cd", "cd", "cd");
	test1("a|b||c", "b", "b");
	test1("a||b|c", "b", "");
	test1("(?:a|b||c)d", "bd", "bd");
	test1("a|", "abc", "a");
	test1("|a", "abc", "");
	test1("aa|a|aaa", "aaa", "aa"); // can't be nfa
	test1("a(?:aa)*|(?:aa)*", "aaaa", "aaa"); // can't be nfa
	test1("a|b|cd", "b", "b");
	test1("a||cd", "cd", ""); // no input string can return matches in non-ascending non-descending order, but still no nfa
	test1("a(?:a||cd)b", "acdb", "acdb");
	test1("q(?:a|b|cd?e?f+g)h", "qcdfffgh", "qcdfffgh");
	test1("a*(?:auth|axolotl|axe)", "aaaxolotl", "aaaxolotl");
	test1("(?:ab?c?)*(?:auth|author|axolotl|axe)", "abaabcaxolotl", "abaabcaxolotl");
	test1(R"(<(@[&!]?\d+|#\d+)>)", "<@12345>", "<@12345>", "@12345"); // caused some trouble with the DFA deduplicator
}
test("regex - backreferences", "string", "regex")
{
	test1("(abc)?\\1", "", "", nullptr);
	test1("(abc)?\\1", "abc", "", nullptr);
	test1("(abc)?\\1", "abcabc", "abcabc", "abc");
	test1("(a)b\\1", "aba", "aba", "a");
	test1fail("(a)|\\4");
	test1fail_std("(a)|\\1", "a", "a", "a"); // std::regex accepts impossible capture groups
	test1("(?:(a)|b)\\1c", "bc", "bc", nullptr);
	test1("((.)\\2){3}", "aabbccddeeff", "aabbcc", "cc", "c");
	test1("((.)\\2){2,4}", "aabbcc", "aabbcc", "cc", "c");
	test1("((.)\\2){2,4}?", "aabbcc", "aabb", "bb", "b");
	test1("(ab){3}\\1", "ababababa", "abababab", "ab");
	test1("(?:a*)a", "aa", "aa");
	test1("(?:a*)b", "ab", "ab");
	test1("(?:ab)*ab", "abab", "abab");
	test1("(?:ab)*aab", "abaab", "abaab");
	test1("(?:|(a?){0,2})\\1b", "a", nullptr, nullptr);
	test1("(?:a|(b))\\1c", "ac", "ac", nullptr);
	test1("(a?){0,2}\\1b", "b", "b", nullptr);
	test1("(a?){0,2}\\1b", "ab", nullptr, nullptr);
	test1("(a?){0,2}\\1b", "aab", "aab", "a");
	test1("(a?){0,2}\\1b", "aaab", "aaab", "a");
	test1("(a?){0,2}\\1b", "aaaab", nullptr, nullptr);
	test1("(a?){0,5}", "aa", "aa", "a");
	test1("(?:(a)|b)\\1", "cd", nullptr, nullptr);
	test1("(?:(a)|b)\\1", "b", "b", nullptr);
	test1("(?:(a)b|aa)\\1", "aaa", "aa", nullptr);
	test1fail("(a)b\\01c");
}
test("regex - groups' trickier behavior", "string", "regex")
{
	// untaken branches in alternations must not capture
	test1("((a)|(b))+", "ab", "ab", "b", nullptr, "b");
	test1("((a)|(b))+", "ba", "ba", "a", "a", nullptr);
	test1("((a)|(b)){2}", "ab", "ab", "b", nullptr, "b");
	test1("((a)|(b)){2}", "ba", "ba", "a", "a", nullptr);
	test1("((a)\\2|(b)\\3){2}", "aabb", "aabb", "bb", nullptr, "b");
	test1("((a)\\2|(b)\\3){2}", "bbaa", "bbaa", "aa", "a", nullptr);
	test1("(?:(a)|(b)|(c)|(d))+", "abcd", "abcd", nullptr, nullptr, nullptr, "d");
	test1("(?:(a)|(b)|(c)|(d))+c", "abc", "abc", nullptr, "b", nullptr, nullptr); // backtracking must restore the capture
	
	// can't perform empty optional captures
	test1("(?:)+e", "e", "e");
	test1("()+e", "e", "e", "");
	test1("(a?)*", "", "", nullptr);
	test1("(a?)+", "", "", "");
	test1("(a?){2,4}b\\1c", "aabc", nullptr, nullptr);
	test1("(a?){3,4}b\\1c", "aabc", "aabc", "");
	test1("((b)?(be)?){0,5}bbee", "bbebbee", "bbebbee", "bbe", "b", "be");
}
test("regex - word and string boundaries", "string", "regex")
{
	test1("^(?:a|ab)*", "aabababaaaabab", "aa"); // matches a twice, and never tries ab without backtracking; regex ends there, so no backtracking needed
	test1("^(?:a|ab)*$", "aabababaaaabab", "aabababaaaabab"); // this, however, needs to backtrack
	test1("^(?:ab|a)*", "aabababaaaabab", "aabababaaaabab"); // this tries ab first and captures everything
	test1("\\b.\\b.\\B", "a+", "a+");
	test1("\\B.\\b.\\b", "+a", "+a");
	test1(".\\b.", "++", nullptr);
	test1(".\\b.", "aa", nullptr);
	test1(".\\b.", "a%", "a%");
	test1(".\\B.", "ab", "ab");
	test1(".\\b.", "ab", nullptr);
	test1(".\\B.", "a%", nullptr);
	test1("\\b.", "+", nullptr);
	test1("\\B.", "a", nullptr);
	test1(".\\b", "+", nullptr);
	test1(".\\B", "a", nullptr);
	test1("\\b", "a", "");
	test1("\\B", "a", nullptr);
	test1("\\b", "%", nullptr);
	test1("\\B", "%", "");
	test1("\\b", "", nullptr);
	test1("\\B", "", "");
	test1fail("\\b+");
	test1fail("^+");
	test1("(?:q^|a|b|cd|e|$)", "cd", "cd");
	test1("(?:a^|b|(?:c|d?e|(?:f|g|hi)j+k)l|mn|o|$)", "gjkl", "gjkl");
	test1("^(ab|()|a|bc)*$", "abc", "abc", "bc", nullptr); // with a capture to ensure it can't become a NFA
	test1("()*?$", "a", nullptr, nullptr);
}
test("regex - lookahead", "string", "regex")
{
	test1("((?=(.b)))a", "ab", "a", "", "ab");
	test1("((?!(.b)))a", "ab", nullptr, nullptr, nullptr);
	test1("((?=(.b)))a", "ac", nullptr, nullptr, nullptr);
	test1("((?!(.b)))a", "ac", "a", "", nullptr);
	test1("(?!(.)\\1)a", "ab", "a", nullptr);
	test1("(?!(.)\\1)a", "aa", nullptr, nullptr);
	test1("(?!(?!(a)))", "a", "", nullptr);
	test1("(?!(?!(a)))", "b", nullptr, nullptr);
	test1fail("(?=a)+");
	test1fail_std("(?!(a))\\1", "a", nullptr, nullptr); // std::regex accepts impossible capture groups
	test1fail_std("(?!(a))\\1", "b", "", nullptr);
	test1fail_std("(?:(a)|\\1b)+", "aabbaab", "aabbaab", nullptr);
	test1fail_std("(?:(a)|\\1b)+", "baabbaab", "baabbaab", nullptr);
	test1("(?=a)abc", "abc", "abc");
	test1("(?=a)b?", "a", "");
	test1("(?!a)bb?", "bb", "bb");
	test1("(?=a)bcd", "a", nullptr); // can't match anything
	test1("(?!a)aa?", "a", nullptr); // can't match anything
	test1("(?=)a", "a", "a"); // empty lookaheads are silly, but syntactically valid
	test1("(?!)a", "a", nullptr);
	test1("(?![])", "", "");
	test1("(?![])", "a", "");
	test1("(?![^])", "", "");
	test1("(?![^])", "a", nullptr);
}
test("regex - the ultimate one", "string", "regex")
{
	// can't NFA this - you can try, but you'll OOM
	test1(".+a.{64}", "b", nullptr);
}
#undef test1
#undef test1fail
test("regex - rarer functions", "string", "regex")
{
	assert_eq((cstring)(regex("bc").search("abc")[0].start), "bc");
	assert_eq(regex("f(oo)").replace("foofoobarfoo", "\\1"), "oooobaroo");
	string nul = string::nul();
	assert(regex("\\0").match(nul));
	assert_eq(regex("\\b").replace("foo bar baz", "-"), "-foo- -bar- -baz-");
	assert_eq(regex("\\b| ").replace("foo bar baz", "-"), "-foo- -bar- -baz-");
	assert_eq(regex(" |\\b").replace("foo bar baz", "-"), "-foo--bar--baz-");
	
	assert_eq(cstring("foo bar baz").csplit(regex("\\b|a")).join(","), "foo, ,b,r, ,b,z");
	assert_eq(cstring("foo bar baz").csplit<1>(regex(" |(?=\\n)")).join(","), "foo,bar baz");
	assert_eq(cstring("foo\nbar baz").csplit<1>(regex(" |(?=\\n)")).join(","), "foo,\nbar baz");
	assert_eq(cstring("").csplit<1>(regex("a")).size(), 1);
	assert_eq(cstring("aabcaada").csplit(regex("a")).join(","), ",,bc,,d,");
	
	cstrnul text = "abc 123";
	auto m = regex("(abc) (123)").match(text); // ensure it doesn't copy 'text' then return a match array full of UAF
	assert_eq(m[1].str(), "abc");
	
	test_nomalloc {
		assert(REGEX("abc").match("abc"));
	};
	
	regex r;
	assert(r.parse("(?:)"));
	assert(r);
	assert(!r.parse("(?:"));
	assert(!r);
	assert(r.parse(""));
	assert(r);
	
	assert_eq(regex::required_substrs("a"), array<string>{"a"});
	assert_eq(regex::required_substrs("abc"), array<string>{"abc"});
	assert_eq(regex::required_substrs("abc.def.ghi"), (array<string>{"abc","def","ghi"}));
	assert_eq(regex::required_substrs("abc(def)ghi"), (array<string>{"abc","def","ghi"}));
	assert_eq(regex::required_substrs("abc+"), (array<string>{"ab"})); // many of these could be improved, but no real point
	assert_eq(regex::required_substrs("abc|abc"), (array<string>{}));
	assert_eq(regex::required_substrs("(abc)+"), (array<string>{}));
}

static void testfn_search(const char * exp, const char * str, int off)
{
	regex_search rs;
	assert(rs.parse(exp));
//rs.dump();
	const char * ret = rs.search(str);
	int off_act = (ret ? ret-str : -1);
	assert_eq(off_act, off);
}
#define test1(exp, input, off) testcall(testfn_search(exp, input, off))
#define test1fail(exp) do { regex_search rs; assert(!rs.parse(exp)); } while(0)
test("regex search", "string", "regex")
{
	test1("a", "walrus", 1);
	test1("[abc]", "walrus", 1);
	test1("bcd", "abcdefg", 1);
	test1("b|bcd|bc", "abcdefg", 1);
	test1("bcde|cd[^]*", "abcdefg", 1);
	test1("bcde|cd[^]*", "abcdZfg", 2);
	test1("bcde|cd[^]*", "abcZefg", -1);
	test1("abc|[^]*", "b", 0);
	test1("a+a+a+", "abaabaaabaaaa", 5);
	test1("a*a*", "abaabaaabaaaa", 0);
	test1("bb", "bababababababababababb", 20);
	test1("a*a*a*bc*c*c*d*e*e*e*", "abaabaaabaaaa", 0);
	test1("a(?:b*b*c*c*|d*d*e*e*|f*f*g*g*|)*h", "haccededh", 1);
	test1("(?:a*a*b*)*c", "abaabaaabaaaac", 0);
	test1("", "walrus", 0);
	test1fail("^");
	test1fail("(a)");
	test1fail("(?=a)");
	test1(R"([^0-9A-Za-z\s\x80-\xBF\xC3-\xFF]|\n\n| {2,}\n|\w+:\S)", "k", -1);
}
