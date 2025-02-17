#pragma once
#include "global.h"
#include "array.h"
#include "set.h"

// Arlib regexes are same as JS version 5.1 regexes (which are mostly same as C++ regexes), with the following differences:
// - No Unicode, and especially no UTF-16, only bytes. \s matches only ASCII space and some control characters; \u1234 is not implemented.
//     Byte values 128-255 are considered non-space, non-alphanumeric; \D \S \W will match them.
//     You can put literal UTF-8 strings in your regex, or for complex cases, match UTF-8 byte values using \x.
// - No extensions beyond v5.1. The following are absent:
//   - Lookbehind (?<=expr) (?<!expr) (difficult to implement, and rare)
//   - Named capture groups (?<name>expr), \k<name> (hard to represent that return value in C++)
//   - Unicode property groups \p{Sc} (would require large lookup tables, and incompatible with my byte-oriented approach)
//   - Character class expressions [abc--b], [abc&&ace] (useless without \p)
//   - Nesting anything in character sets [\q{abc}] (difficult to implement, near useless without \p)
//   - Probably some more stuff, the spec is increasingly difficult to read
// - Illegal backreferences (backreferences that cannot be defined at this point) are errors, not synonyms for empty string.
// - No exceptions, it just returns false.
// - None of the C++ extensions. No regex traits, no named character classes, collation, or equivalence. [[:digit:]], [[.tilde.]], [[=a=]]
// - None of the browser compatibility extensions; for example, {a} is an error, not literal chars.
// - There may be bugs, of course. I can think of plenty of troublesome edge cases, and it's often difficult to determine the right answer.

// This is fundamentally a backtracking regex engine; as such, the usual caveats about catastrophic backtracking apply.
// However, it optimizes what it can to NFAs, which reduces the asymptote for some regexes.
// For example, (?:a+a+a+a+) will be processed in O(n), not O(n^4), and (?:)+ will be deleted;
//    however, if there are capture groups or other things an NFA can't handle, it will fall back to backtracking.
//    For example, ()+ will crash with a stack overflow.

// If you'd rather not compile the regex every time the expression is reached, you can call this instead.
// This moves the initialization to before main.
#define REGEX(str) (precompiled_regex<decltype([]{ return "" str ""; })>)

class regex {
	enum insntype_t {
		t_jump, // Target is 'data'.
		t_accept, // Doesn't use 'data'. Sets the end of capture group 0.
		t_alternative_first, // Try matching starting from 'data'. If it fails, try from the next insn instead.
		t_alternative_second, // Try matching starting from the next insn. If it fails, try from 'data' instead.
		
		t_byte, // 'data' is index to bytes or dfas, respectively.
		t_dfa_shortest, // Shortest match first.
		t_dfa_longest,
		
		t_capture_start, // 'data' is the capture index. 0 is illegal.
		t_capture_end, // If backtracked, undoes the update of the capture.
		t_capture_discard, // Also undoes if backtracked. Used before | (so (?:(a)|b){2} + "ab" doesn't capture), and after (?!(a)) (it matched, no backtracking).
		t_capture_backref, // Doesn't write the capture list, but still capture related, so it goes in this group.
		
		t_assert_boundary, // These don't use 'data'.
		t_assert_notboundary,
		t_assert_start,
		t_assert_end,
		
		t_lookahead_positive, // Lookahead body is after this instruction, ending with t_accept. 'data' tells where to jump if successful.
		t_lookahead_negative,
	};
	struct insn_t {
		insntype_t type;
		uint32_t data;
	};
	
	struct dfa_t {
		// initial state is 0
		// match is 0x80000000 bit, no more matches is 0x7FFFFFFF
		struct node_t {
			uint32_t next[256];
		};
		array<node_t> transitions;
		uint32_t init_state; // either 0 or 0x80000000, depending on whether empty string matches
	};
	
	uint32_t num_captures;
	array<insn_t> insns;
	array<bitset<256>> bytes; // Simply which bytes are legal at this position. Lots of things compile to this. (Most then become a DFA.)
	array<dfa_t> dfas;
	
	class parser;
	
public:
	regex() { set_fail(); }
	regex(cstring rgx) { parse(rgx); }
	bool parse(cstring rgx);
	operator bool() const
	{
		return num_captures > 0; // a valid regex has at least the \0 capture, but the set_fail one doesn't
	}
	
private:
	
	void reset()
	{
		insns.reset();
		bytes.reset();
		dfas.reset();
	}
	
	void set_fail() // sets current regex to something that can't match anything
	{
		reset();
		num_captures = 0;
		insns.append({ t_byte, 0 });
		bytes.append();
	}
	
	void optimize();
	
	static bool is_alt(insntype_t type)
	{
		return type == t_alternative_first || type == t_alternative_second;
	}
	static bool targets_another_insn(insntype_t type)
	{
		return is_alt(type) || type == t_jump || type == t_lookahead_positive || type == t_lookahead_negative;
	}
	
	struct pair {
		const char * start;
		const char * end;
		cstring str() const { return arrayview<char>(start, end-start); }
		operator cstring() const { return str(); }
	};
public:
	template<size_t N> class match_t {
		static_assert(N >= 1);
		friend class regex;
		pair group[N];
		size_t m_size;
		
	public:
		template<size_t Ni> friend class match_t;
		match_t() { memset(group, 0, sizeof(group)); }
		template<size_t Ni> match_t(const match_t<Ni>& inner)
		{
			static_assert(Ni < N); // if equal, it goes to the implicitly defaulted copy ctor
			memcpy(group, inner.group, sizeof(inner.group));
			memset(group+Ni, 0, sizeof(pair)*(N-Ni));
		}
		
		size_t size() const { return m_size; }
		const pair& operator[](size_t n) const { return group[n]; }
		operator bool() const { return group[0].end; }
		bool operator!() const { return !(bool)*this; }
	};
	
	class matcher;
	
	void match(pair* ret, void** tmp, const char * start, const char * at, const char * end) const;
	void match_alloc(size_t num_captures, pair* ret, void** tmp, const char * start, const char * at, const char * end) const;
	
public:
	
	template<size_t n = 5> match_t<n> match(const char * start, const char * at, const char * end) const
	{
		match_t<n> ret {};
		void* tmp[n];
		if (LIKELY(n >= this->num_captures))
		{
			ret.m_size = this->num_captures;
			match(ret.group, tmp, start, at, end);
		}
		else
		{
			ret.m_size = n;
			match_alloc(n, ret.group, tmp, start, at, end);
		}
		return ret;
	}
	template<size_t n = 5> match_t<n> match(const char * start, const char * end) const { return match<n>(start, start, end); }
	template<size_t n = 5> match_t<n> match(const char * str) const { return match<n>(str, str+strlen(str)); }
	// funny type, to ensure it can't construct a temporary
	template<size_t n = 5> match_t<n> match(cstring& str) const { return match<n>(str.ptr_raw(), str.ptr_raw_end()); }
	
	template<size_t n = 5> match_t<n> search(const char * start, const char * at, const char * end) const
	{
		while (at < end)
		{
			match_t<5> ret = match<n>(start, at, end);
			if (ret)
				return ret;
			at++;
		}
		return {};
	}
	template<size_t n = 5> match_t<n> search(const char * start, const char * end) const { return search<n>(start, start, end); }
	template<size_t n = 5> match_t<n> search(const char * str) const { return search<n>(str, str+strlen(str)); }
	template<size_t n = 5> match_t<n> search(cstring& str) const { return search<n>(str.ptr_raw(), str.ptr_raw_end()); }
	
	string replace(cstring str, const char * replacement) const;
	
public:
	// Prints the compiled regex to stdout. Only usable for debugging.
	void dump() const { dump(insns); }
private:
	void dump(arrayview<insn_t> insns) const;
	static void dump_byte(const bitset<256>& byte);
	static void dump_range(uint8_t first, uint8_t last);
	static void dump_single_char(uint8_t ch);
};

template<typename T> static const regex precompiled_regex { T()() };
