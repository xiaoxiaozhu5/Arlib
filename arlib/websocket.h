#include "socket.h"

class websocket {
	socketbuf sock;
	mksocket_t cb_mksock = socket2::create_sslmaybe;
	
public:
	enum {
		t_cont = 0,
		t_text = 1,
		t_binary = 2,
		
		t_close = 8,
		t_ping = 9,
		t_pong = 10,
	};
	
	void wrap_socks(mksocket_t cb) { cb_mksock = cb; }
	async<bool> connect(cstring target, arrayview<string> headers = nullptr);
	operator bool() { return sock; }
	
	void reset() { sock = nullptr; }
	
	// The returned bytesr is valid until next function call on this object.
	// If all is true, type can be 0-15. If false, only 0-7.
	async<bytesr> msg(int* type = nullptr, bool all = false);
	void send(bytesr by, int type);
	void send(bytesr by) { send(by, t_binary); }
	void send(cstring text) { send(text.bytes(), t_text); }
	async<bool> await_send() { return sock.await_send(); }
};
