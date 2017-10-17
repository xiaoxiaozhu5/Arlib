#ifdef ARLIB_SOCKET
#pragma once
#include "socket.h"
#include "bytepipe.h"

class WebSocket : nocopy {
	runloop* loop;
	autoptr<socket> sock;
	bytepipe tosend; // used to avoid sending stuff before the handshake finishes, server throws Protocol Error if I try
	
	array<byte> msg;
	
	bool inHandshake;
	bool gotFirstLine; // HTTP/1.1 101 Switching Protocols
	//TODO: Sec-WebSocket-Accept https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers
	
	function<void(string)> cb_str;
	function<void(arrayview<byte>)> cb_bin;
	function<void()> cb_error;
	
	void activity(socket*);
	
	void send(arrayview<byte> message, bool binary);
	
	void cancel() { sock = NULL; cb_error(); }
	
public:
	WebSocket(runloop* loop) : loop(loop) {}
	void connect(cstring target, arrayview<string> headers = NULL);
	
	void send(arrayview<byte> message) { send(message, true); }
	void send(cstring message) { send(message.bytes(), false); }
	
	//It's fine to call both of those if you need to keep text/binary data apart. If you don't, both will go to the same one.
	//Do this before connect(), or you may miss events, for example if the target is unparseable.
	void callback(function<void(string         )> cb_str, function<void()> cb_error) { this->cb_str = cb_str; this->cb_error = cb_error; }
	void callback(function<void(arrayview<byte>)> cb_bin, function<void()> cb_error) { this->cb_bin = cb_bin; this->cb_error = cb_error; }
	
	bool isOpen() { return sock; }
	void reset() { sock = NULL; msg.reset(); }
	
	operator bool() { return isOpen(); }
	
	//If this key is returned, call .recv(). May not necessarily return anything.
	void monitor(socket::monitor& mon, void* key) { if (sock) mon.add(sock, key, true, false); }
};

#endif
