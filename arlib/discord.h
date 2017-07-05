#pragma once
#include "global.h"
#include "string.h"
#include "array.h"
#include "websocket.h"
#include "http.h"
#include "json.h"
#include "linq.h"

class Discord {
	struct i_role;
	struct i_user;
	struct i_channel;
	struct i_guild;
	
public:
	bool connect_bot(cstring token);
	
	//Discord is a complex protocol. On activity, or after a 1000 millisecond timeout, call process().
	void monitor(socket::monitor& mon, void* key)
	{
		m_ws.monitor(mon, key); // pun not intended
		m_http.monitor(mon, key);
	}
	//Only nonblocking is available.
	void process() { while (process(false)) {} }
	
	class Role;
	class User;
	class Channel;
	class Guild;
	friend class Role;
	friend class User;
	friend class Channel;
	friend class Guild;
	
	class Role {
		Discord* m_parent;
		cstring m_id;
		friend class Discord;
		//friend class Role;
		friend class User;
		friend class Channel;
		friend class Guild;
		
		i_role& impl() { return m_parent->roles[m_id]; }
		Role(Discord* parent, cstring id) : m_parent(parent), m_id(id) {}
		
	public:
		Role() : m_parent(NULL) {}
		Role(const Role& other) : m_parent(other.m_parent), m_id(other.m_id) {}
		
		string id() { return m_id; }
		string name() { return m_parent->roles[m_id].name; }
		Guild guild() { return Guild(m_parent, impl().guild); }
		set<User> users()
		{
			cstring guild_id = impl().guild;
			return m_parent->users
				.where([&](const map<string,i_user>::node& user)->bool { return user.value.roles.contains(m_id); })
				.select([&](const map<string,i_user>::node& user)->User { return User(m_parent, user.key, guild_id); })
				;
		}
		
		operator bool() { return m_parent; }
		bool operator==(const Role& other) { return m_parent==other.m_parent && m_id==other.m_id; }
		
		size_t hash() const { return m_id.hash(); }
	};
	class User {
		Discord* m_parent;
		cstring m_id;
		cstring m_guild_id; // for fetching nick
		
		friend class Discord;
		friend class Role;
		//friend class User;
		friend class Channel;
		friend class Guild;
		
		i_user& impl() { return m_parent->users[m_id]; }
		User(Discord* parent, cstring id, cstring guild_id) : m_parent(parent), m_id(id), m_guild_id(guild_id) {}
		
	public:
		User() : m_parent(NULL) {}
		User(const User& other) : m_parent(other.m_parent), m_id(other.m_id) {}
		
		cstring id() { return m_id; }
		cstring nick()
		{
			string guild_nick = impl().nicks.get_or(m_guild_id, "");
			if (guild_nick) return guild_nick;
			else return impl().username;
		}
		cstring account() { return impl().username+"#"+impl().discriminator; }
		cstring highlight() { return "<@"+m_id+">"; }
		
		array<Role> roles();
		array<Role> roles(Guild guild);
		bool has_role(Role role)
		{
			return impl().roles.contains(role.m_id);
		}
		void set_role(Role role, bool present)
		{
			m_parent->http(present ? "PUT" : "DELETE", "/guilds/"+role.impl().guild+"/members/"+m_id+"/roles/"+role.m_id);
		}
		
		bool partial() { return !impl().discriminator; }
		
		void send(cstring text);
		
		operator bool() { return m_parent; }
		bool operator==(const User& other) { return m_parent==other.m_parent && m_id==other.m_id; }
		
		size_t hash() const { return m_id.hash(); }
	};
	class Channel {
		Discord* m_parent;
		cstring m_id;
		friend class Discord;
		friend class Role;
		friend class User;
		//friend class Channel;
		friend class Guild;
		
		i_channel& impl() { return m_parent->channels[m_id]; }
		Channel(Discord* parent, cstring id) : m_parent(parent), m_id(id) {}
		
	public:
		Channel() : m_parent(NULL) {}
		Channel(const Channel& other) : m_parent(other.m_parent), m_id(other.m_id) {}
		
		cstring id() { return m_id; }
		cstring name() { return impl().name; }
		Guild guild() { return Guild(m_parent, impl().guild); }
		
		void message(cstring text)
		{
			JSON json;
			json["content"] = text;
			m_parent->http("/channels/"+m_id+"/messages", json);
		}
		void busy()
		{
			m_parent->http("POST", "/channels/"+m_id+"/typing");
		}
		
		operator bool() { return m_parent; }
		bool operator==(const Channel& other) { return m_parent==other.m_parent && m_id==other.m_id; }
		
		size_t hash() const { return m_id.hash(); }
	};
	class Guild { // apparently Guild is to the API what Server is to the end user
		Discord* m_parent;
		cstring m_id;
		friend class Discord;
		friend class Role;
		friend class User;
		friend class Channel;
		//friend class Guild;
		
		i_guild& impl() { return m_parent->guilds[m_id]; }
		Guild(Discord* parent, cstring id) : m_parent(parent), m_id(id) {}
		
	public:
		Guild() : m_parent(NULL) {}
		Guild(const Guild& other) : m_parent(other.m_parent), m_id(other.m_id) {}
		
		cstring id() { return m_id; }
		
		set<Role> roles() { return impl().roles.select([&](cstring r){ return Role(m_parent, r); }); }
		set<User> users() { return impl().users.select([&](cstring u){ return User(m_parent, u, m_id); }); }
		set<Channel> channels() { return impl().channels.select([&](cstring c){ return Channel(m_parent, c); }); }
		
		Role role(cstring name)
		{
			cstring id = impl().roles.first([&](cstring r)->bool { return m_parent->roles[r].name == name; });
			if (id) return Role(m_parent, id);
			else return Role();
		}
		Channel channel(cstring name)
		{
			cstring id = impl().channels.first([&](cstring c)->bool { return m_parent->channels[c].name == name; });
			if (id) return Channel(m_parent, id);
			else return Channel();
		}
		
		operator bool() { return m_parent; }
		bool operator==(const Guild& other) { return m_parent==other.m_parent && m_id==other.m_id; }
		
		size_t hash() const { return m_id.hash(); }
	};
	
	User self() { return User(this, my_user, ""); }
	
	function<void(Channel chan, User user, cstring message)> on_msg;
	function<void(Guild guild, User self)> on_guild_enter;
	function<void(Guild guild, User user)> on_join;
	
private:
	bool process(bool block);
	
	WebSocket m_ws; // don't use these directly, dangerous!
	HTTP m_http;
	array<function<void(HTTP::rsp)>> m_http_reqs; // TODO: kinda annoying, do I make it all synchronous?
	
	bool connect();
	void connect_cb(HTTP::rsp r);
	
	bool bot;
	string token;
	
	time_t ratelimit = 0;
	
	time_t keepalive_next = 0;
	int keepalive_ms;
	
	int guilds_to_join;
	
	string resume;
	uint64_t sequence;
	
	struct i_role {
		string name;
		string guild;
	};
	map<string,i_role> roles;
	
	struct i_channel {
		string name;
		string guild;
		set<string> users;
	};
	map<string,i_channel> channels;
	
	struct i_user {
		string username; // Kieran
		string discriminator; // 4697 (it's split for some reason)
		map<string,string> nicks; // varies per guild
		set<string> roles; // also per-guild, but roles are unique across discord so excess roles don't do any harm
	};
	map<string,i_user> users;
	string my_user;
	
	struct i_guild {
		string name;
		set<string> users;
		set<string> roles;
		set<string> channels;
	};
	map<string,i_guild> guilds;
	
	void headers(array<string>& h);
	
	void http(HTTP::req r, function<void(HTTP::rsp)> callback = NULL);
	void http(cstring url, function<void(HTTP::rsp)> callback = NULL)
	{
		HTTP::req r(url);
		http(r, callback);
	}
	void http(cstring url, JSON& post, function<void(HTTP::rsp)> callback = NULL)
	{
		HTTP::req r(url);
		r.postdata = post.serialize().bytes();
		http(r, callback);
	}
	void http(cstring method, cstring url, JSON& post, function<void(HTTP::rsp)> callback = NULL)
	{
		HTTP::req r(url);
		r.method = method;
		r.postdata = post.serialize().bytes();
		http(r, callback);
	}
	void http(cstring method, cstring url, function<void(HTTP::rsp)> callback = NULL)
	{
		HTTP::req r(url);
		r.method = method;
		http(r, callback);
	}
	
	void http_process();
	
	void http_await()
	{
		while (m_http_reqs)
		{
			m_http.await();
			http_process();
		}
	}
	
	void send(JSON& json)
	{
		string msg = json.serialize();
		datelog("<< "+msg);
		m_ws.send(msg);
	}
	
	//takes a User object, with ["id"]
	void set_user_inner(JSON& json);
	//takes a Guild Member object, with ["user"] and ["roles"]
	void set_user(cstring guild_id, JSON& json);
	
	void del_user(cstring guild_id, cstring user_id);
	
	void datelog(cstring text);
	
public:
void debug();
};
