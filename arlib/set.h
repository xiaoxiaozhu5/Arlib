#pragma once
#include "global.h"
#include "array.h"
#include "hash.h"
#include "string.h"
#include "stringconv.h"
#include "tuple.h"

template<typename T, typename Thasher = void>
class set {
	template<typename,typename,typename>
	friend class map;
	
	//this is a hashtable, using open addressing and linear probing
	
	enum { i_empty, i_deleted };
	// char can alias anything, so use that for tag type; the above two are the only valid tags
	char& tag(size_t id) { return *(char*)(m_data+id); }
	char tag(size_t id) const { return *(char*)(m_data+id); }
	
	T* m_data; // length is always same as m_valid, no need to duplicate its length
	bitarray m_valid;
	size_t m_used_slots; // number of nodes in m_data that are not i_empty
	size_t m_count; // number of nodes in m_data currently containing valid data
	
	// will spuriously fail during rehash
	//void validate() const
	//{
	//	if (!m_data) return;
	//	
	//	size_t a = 0;
	//	size_t b = 0;
	//	size_t c = 0;
	//	for (size_t n=0;n<m_valid.size();n++)
	//	{
	//		if (m_valid[n]) a++;
	//		else if (tag(n) == i_empty) b++;
	//		else c++;
	//	}
	//	size_t a2 = m_count;
	//	size_t b2 = m_valid.size()-m_used_slots;
	//	size_t c2 = m_used_slots-m_count;
	//	if (a != a2 || b != b2 || c != c2)
	//	{
	//		printf("%lu,%lu %lu,%lu %lu,%lu\n", a,a2, b,b2, c,c2);
	//		*(char*)1 = 1;
	//	}
	//}
	
	void rehash(size_t newsize)
	{
		T* prev_data = m_data;
		bitarray prev_valid = std::move(m_valid);
		
		m_data = xcalloc(newsize, sizeof(T));
		static_assert(sizeof(T) >= 1); // otherwise the tags mess up (zero-size objects are useless in sets, 
		m_valid.reset();               // and I'm not sure if they're expressible in standard C++, but no reason not to check)
		m_valid.resize(newsize);
		
		for (size_t i=0;i<prev_valid.size();i++)
		{
			if (!prev_valid[i]) continue;
			
			size_t pos = find_pos_full<true, false>(prev_data[i]);
			//this is known to not overwrite any existing object; if it does, someone screwed up
			memcpy((void*)&m_data[pos], (void*)&prev_data[i], sizeof(T));
			m_valid[pos] = true;
		}
		free(prev_data);
		m_used_slots = m_count;
	}
	
	// Returns whether it did anything.
	bool grow()
	{
		// half full -> rehash
		if (m_count >= m_valid.size()/2)
			rehash(m_valid.size()*2);
		else if (m_used_slots >= m_valid.size()/4*3)
			rehash(m_valid.size());
		else
			return false;
		return true;
	}
	
	bool slot_empty(size_t pos) const
	{
		return !m_valid[pos];
	}
	
	
	template<typename T2>
	static auto local_hash(const T2& item)
	{
		if constexpr (!std::is_same_v<Thasher, void>)
			return Thasher::hash(item);
		else if constexpr (requires { hash(item); })
			return hash(item);
		else if constexpr (requires { hash((T)item); })
			return hash((T)item);
		else static_assert(sizeof(T2) < 0);
	}
	
	//If the object exists, returns the index where it can be found.
	//If not, and want_empty is true, returns a suitable empty slot to insert it in, or -1 if the object should rehash.
	//If no such object and want_empty is false, returns -1.
	template<bool want_empty, bool want_used = true, typename T2>
	size_t find_pos_full(const T2& item) const
	{
		if (!m_data) return -1;
		
		size_t hashv = hash_shuffle(local_hash<T2>(item));
		size_t i = 0;
		
		size_t emptyslot = -1;
		
		while (true)
		{
			//some hashsets use hashv + i+(i+1)/2 <http://stackoverflow.com/a/15770445>
			//but that only helps on poor hash functions, which mine is not
			size_t pos = (hashv + i) & (m_valid.size()-1);
			if (want_used && m_valid[pos] && m_data[pos] == item)
				return pos;
			if (!m_valid[pos])
			{
				if (emptyslot == (size_t)-1) emptyslot = pos;
				if (tag(pos) == i_empty)
				{
					if (want_empty) return emptyslot;
					else return -1;
				}
			}
			i++;
//if(i > m_valid.size()) *(char*)1=1;
		}
	}
	
	template<typename T2>
	size_t find_pos_const(const T2& item) const
	{
		return find_pos_full<false>(item);
	}
	
	template<typename T2>
	size_t find_pos_insert(const T2& item)
	{
		return find_pos_full<true>(item);
	}
	
	//if the item doesn't exist, NULL
	template<typename T2>
	T* get_or_null(const T2& item) const
	{
		size_t pos = find_pos_const(item);
		if (pos != (size_t)-1) return &m_data[pos];
		else return NULL;
	}
	// returns either false and a normal pointer, or true and an uninitialized pointer
	// in the latter case, it's the caller's responsibility to call placement new
	template<typename T2>
	tuple<bool,T*> get_or_prepare_create(const T2& item, bool known_new = false)
	{
		if (!m_data)
			rehash(m_valid.size());
		
		size_t pos = known_new ? 0 : find_pos_insert(item);
		
		if (known_new || !m_valid[pos])
		{
			if (grow())
				pos = find_pos_insert(item); // recalculate this if grow() moved it
			//do not move grow() earlier; it invalidates references, get_create(item_that_exists) is not allowed to do that
			
			if (tag(pos) == i_empty) m_used_slots++;
			m_valid[pos] = true;
			m_count++;
			return { true, &m_data[pos] };
		}
		return { false, &m_data[pos] };
	}
	
	void construct()
	{
		m_data = NULL;
		m_valid.resize(8);
		m_count = 0;
		m_used_slots = 0;
	}
	void construct(const set& other)
	{
		m_data = xcalloc(other.m_valid.size(), sizeof(T));
		m_valid = other.m_valid;
		m_count = other.m_count;
		m_used_slots = other.m_count;
		
		for (size_t i=0;i<m_valid.size();i++)
		{
			if (m_valid[i])
			{
				new(&m_data[i]) T(other.m_data[i]);
			}
		}
		rehash(m_valid.size());
	}
	void construct(set&& other)
	{
		m_data = std::move(other.m_data);
		m_valid = std::move(other.m_valid);
		m_count = std::move(other.m_count);
		m_used_slots = std::move(other.m_used_slots);
		
		other.construct();
	}
	
	void destruct()
	{
		for (size_t i=0;i<m_valid.size();i++)
		{
			if (m_valid[i])
			{
				m_data[i].~T();
			}
		}
		m_count = 0;
		m_used_slots = 0;
		free(m_data);
		m_valid.reset();
	}
	
public:
	set() { construct(); }
	set(const set& other) { construct(other); }
	set(set&& other) { construct(std::move(other)); }
	set(std::initializer_list<T> c)
	{
		construct();
		for (const T& item : c) add(item);
	}
	set& operator=(const set& other) { destruct(); construct(other); return *this; }
	set& operator=(set&& other) { destruct(); construct(std::move(other)); return *this; }
	~set() { destruct(); }
	
	void add(const T& item)
	{
		auto[create,ptr] = get_or_prepare_create(item);
		if (create) new(ptr) T(item);
	}
	template<typename T2>
	void add(const T2& item)
	{
		auto[create,ptr] = get_or_prepare_create(item);
		if (create) new(ptr) T(item);
	}
	
	template<typename T2>
	bool contains(const T2& item) const
	{
		size_t pos = find_pos_const(item);
		return pos != (size_t)-1;
	}
	
	template<typename T2>
	void remove(const T2& item)
	{
		size_t pos = find_pos_const(item);
		if (pos == (size_t)-1) return;
		
		m_data[pos].~T();
		tag(pos) = i_deleted;
		m_valid[pos] = false;
		m_count--;
		if (m_count < m_valid.size()/4 && m_valid.size() > 8) rehash(m_valid.size()/2);
	}
	
	size_t size() const { return m_count; }
	
	void reset() { destruct(); construct(); }
	
	class iterator {
		friend class set;
		
		const set* parent;
		size_t pos;
		
		void to_valid()
		{
			while (pos < parent->m_valid.size() && !parent->m_valid[pos])
				pos++;
		}
		
		iterator(const set<T,Thasher>* parent, size_t pos) : parent(parent), pos(pos)
		{
			to_valid();
		}
		
	public:
		
		const T& operator*()
		{
			return parent->m_data[pos];
		}
		void operator++()
		{
			pos++;
			to_valid();
		}
		bool operator!=(const end_iterator&)
		{
			return (pos < parent->m_valid.size());
		}
	};
	
	//messing with the set during iteration half-invalidates all iterators
	//a half-invalid iterator may return values you've already seen and may skip values, but will not crash or loop forever
	//exception: you may not dereference a half-invalid iterator, use operator++ first
	//'for (T i : my_set) { my_set.remove(i); }' is safe, but is not guaranteed to remove everything
	iterator begin() const { return iterator(this, 0); }
	end_iterator end() const { return {}; }
	
//string debug_node(int n) { return tostring(n); }
//string debug_node(string& n) { return n; }
//template<typename T2> string debug_node(T2& n) { return "?"; }
//void debug(const char * why)
//{
//puts("---");
//for (size_t i=0;i<m_data.size();i++)
//{
//	printf("%s %lu: valid %d, tag %d, data %s, found slot %lu\n",
//		why, i, (bool)m_valid[i], m_data[i].tag(), (const char*)debug_node(m_data[i].member()), find_pos(m_data[i].member()));
//}
//puts("---");
//}
	
	static constexpr bool serialize_as_array() { return true; }
	template<typename Ts> void serialize(Ts& s)
	{
		if constexpr (Ts::serializing)
		{
			for (const T& child : *this)
				s.item(child);
		}
		else
		{
			while (s.has_item())
			{
				T tmp;
				s.item(tmp);
				add(std::move(tmp));
			}
		}
	}
	
	template<typename Ts> void serialize2(Ts& s)
	{
		if constexpr (Ts::serializing)
		{
			s.enter_array();
			for (const T& child : *this)
				s.item(child);
			s.exit_array();
		}
		else
		{
			SER_ENTER_ARRAY(s)
			{
				T tmp;
				s.item(tmp);
				add(std::move(tmp));
			}
		}
	}
};



template<typename Tkey, typename Tvalue, typename Thasher = void>
class map {
public:
	struct node {
		const Tkey key;
		Tvalue value;
		
		node() : key(), value() {}
		node(const Tkey& key) : key(key), value() {}
		node(const Tkey& key, const Tvalue& value) : key(key), value(value) {}
		//these silly things won't work
		//node(Tkey&& key) : key(std::move(key)), value() {}
		//node(Tkey&& key, const Tvalue& value) : key(std::move(key)), value(value) {}
		//node(const Tkey& key, Tvalue&& value) : key(key), value(std::move(value)) {}
		//node(Tkey&& key, Tvalue&& value) : key(std::move(key)), value(std::move(value)) {}
		//node(node other) : key(other.key), value(other.value) {}
		
		auto hash() const { return ::hash(key); }
		template<typename T>
		bool operator==(const T& other) { return key == other; }
		bool operator==(const node& other) { return key == other.key; }
	};
private:
	class node_hasher_t {
	public:
		static size_t hash(const node& n) { return Thasher::hash(n.key); }
	};
	using node_hasher = std::conditional_t<std::is_same_v<Thasher,void>,void,node_hasher_t>;
	set<node,node_hasher> items;
	
public:
	//map() {}
	//map(const map& other) : items(other.items) {}
	//map(map&& other) : items(std::move(other.items)) {}
	//map& operator=(const map& other) { items = other.items; }
	//map& operator=(map&& other) { items = std::move(other.items); }
	//~map() { destruct(); }
	
	//can't call it set(), conflict with set<>
	Tvalue& insert(const Tkey& key, const Tvalue& value)
	{
		auto[create,ptr] = items.get_or_prepare_create(key);
		if (create) new(ptr) node(key, value);
		else ptr->value = value;
		return ptr->value;
	}
	
	//if nonexistent, null deref (undefined behavior, segfault in practice)
	template<typename Tk2>
	Tvalue& get(const Tk2& key)
	{
		return items.get_or_null(key)->value;
	}
	template<typename Tk2>
	const Tvalue& get(const Tk2& key) const
	{
		return items.get_or_null(key)->value;
	}
	
	//if nonexistent, returns 'def'
	template<typename Tk2>
	Tvalue& get_or(const Tk2& key, Tvalue& def)
	{
		node* ret = items.get_or_null(key);
		if (ret) return ret->value;
		else return def;
	}
	template<typename Tk2>
	const Tvalue& get_or(const Tk2& key, const Tvalue& def) const
	{
		node* ret = items.get_or_null(key);
		if (ret) return ret->value;
		else return def;
	}
	template<typename Tk2>
	Tvalue get_or(const Tk2& key, Tvalue&& def) const
	{
		node* ret = items.get_or_null(key);
		if (ret) return ret->value;
		else return def;
	}
	template<typename Tk2>
	cstrnul get_or(const Tk2& key, const char * def) const requires (std::is_same_v<Tvalue, string>)
	{
		node* ret = items.get_or_null(key);
		if (ret) return ret->value;
		else return def;
	}
	template<typename Tk2>
	cstring get_or(const Tk2& key, cstring def) const requires (std::is_same_v<Tvalue, string>)
	{
		node* ret = items.get_or_null(key);
		if (ret) return ret->value;
		else return def;
	}
	template<typename Tk2>
	Tvalue* get_or_null(const Tk2& key)
	{
		node* ret = items.get_or_null(key);
		if (ret) return &ret->value;
		else return NULL;
	}
	template<typename Tk2>
	const Tvalue* get_or_null(const Tk2& key) const
	{
		node* ret = items.get_or_null(key);
		if (ret) return &ret->value;
		else return NULL;
	}
	Tvalue& get_create(const Tkey& key)
	{
		auto[create,ptr] = items.get_or_prepare_create(key);
		if (create) new(ptr) node(key);
		return ptr->value;
	}
	template<typename Tvc>
	Tvalue& get_create(const Tkey& key, Tvc&& cr) requires (std::is_invocable_r_v<Tvalue, Tvc>)
	{
		auto[create,ptr] = items.get_or_prepare_create(key);
		if (create) new(ptr) node(key, cr());
		return ptr->value;
	}
	Tvalue& get_create(const Tkey& key, const Tvalue& def)
	{
		auto[create,ptr] = items.get_or_prepare_create(key);
		if (create) new(ptr) node(key, def);
		return ptr->value;
	}
	//Tvalue& operator[](const Tkey& key) // C# does this better...
	//{
		//return get(key);
	//}
	
	Tvalue& insert(const Tkey& key)
	{
		return get_create(key);
	}
	
	template<typename Tk2>
	bool contains(const Tk2& item) const
	{
		return items.contains(item);
	}
	
	template<typename Tk2>
	void remove(const Tk2& item)
	{
		items.remove(item);
	}
	
	void reset()
	{
		items.reset();
	}
	
	size_t size() const { return items.size(); }
	
private:
	template<typename Tr, int op>
	class iterator {
		friend class map;
		using Ti = typename set<node,node_hasher>::iterator;
		Ti it;
		iterator(Ti it) : it(it) {}
		
	public:
		Tr& operator*()
		{
			if constexpr (op == 0)
				return const_cast<Tr&>(*it);
			if constexpr (op == 1)
				return (*it).key;
			if constexpr (op == 2)
				return const_cast<Tr&>((*it).value);
		}
		void operator++() { ++it; }
		bool operator!=(const end_iterator& other) { return it != other; }
		
		iterator begin() { return it; }
		end_iterator end() { return {}; }
	};
	
public:
	//messing with the map during iteration half-invalidates all iterators
	//a half-invalid iterator may return values you've already seen and may skip values, but will not crash or loop forever
	//exception: you may not dereference a half-invalid iterator, use operator++ first
	
	iterator<node, 0> begin() { return items.begin(); }
	iterator<const node, 0> begin() const { return items.begin(); }
	end_iterator end() const { return {}; }
	
	iterator<const Tkey, 1> keys() const { return items.begin(); }
	iterator<Tvalue, 2> values() { return items.begin(); }
	iterator<const Tvalue, 2> values() const { return items.begin(); }
	
	template<typename Ts>
	void serialize(Ts& s)
	{
		if constexpr (Ts::serializing)
		{
			for (node& p : *this)
			{
				s.item(tostring(p.key), p.value); // stringconv.h isn't included at this point, but somehow it works
			}
		}
		else
		{
			while (s.has_item())
			{
				Tkey tmpk;
				if (fromstring(s.name(), tmpk))
					s.item(get_create(tmpk));
			}
		}
	}
	
	template<typename Ts> void serialize2(Ts& s)
	{
		if constexpr (Ts::serializing)
		{
			SER_ENTER(s)
			{
				for (node& p : *this)
				{
					s.item(tostring(p.key), p.value); // stringconv.h isn't included at this point, but somehow it works
				}
			}
		}
		else
		{
			SER_ENTER(s)
			{
				Tkey tmpk;
				if (fromstring(s.get_name(), tmpk))
					s.item(get_create(tmpk));
			}
		}
	}
};
