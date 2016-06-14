#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define socket socket_t
class socket {
protected:
	socket(){}
	int fd; // Used by select().
	
	enum {
		t_tcp,
		t_udp,
		t_tcpssl,
		t_other,
	};
	int type;
	
	//apparently protected becomes private when dealing with another instance of the object, or something
	//probably spec bug, let's just work around it.
	static int get_fd(socket* sock) { return sock->fd; }
	static int get_type(socket* sock) { return sock->type; }
	
public:
	//Returns NULL on connection failure.
	static socket* create(const char * domain, int port);
	//Always succeeds. If the server can't be contacted, returns failure on first write or read.
	static socket* create_async(const char * domain, int port);
	static socket* create_udp(const char * domain, int port);
	
	enum {
		e_closed = -1, // Remote host set the TCP EOF flag.
		e_broken = -2, // Connection was forcibly torn down.
		e_udp_too_big = -3, // Attempted to process an unacceptably large UDP packet.
		e_ssl_failure = -4, // Certificate validation failed, no algorithms in common, or other SSL error.
	};
	
	//Negative means error, see above.
	//Positive is number of bytes handled. Zero means try again, and can be treated as byte count.
	//send() sends all bytes before returning. send1() waits until it can send at least one byte.
	//send0() can send zero, but is fully nonblocking.
	//For UDP sockets, partial reads or writes aren't possible; you always get one or zero packets.
	virtual int recv(uint8_t* data, int len) = 0;
	virtual int send0(const uint8_t* data, int len) = 0;
	virtual int send1(const uint8_t* data, int len) = 0;
	int send(const uint8_t* data, int len)
	{
		int sent = 0;
		while (sent < len)
		{
			int here = send1(data+sent, len-sent);
			if (here<0) return here;
			sent += here;
		}
		return len;
	}
	
	//Convenience functions for handling textual data.
	int recv(char* data, int len) { int ret = recv((uint8_t*)data, len-1); if (ret>=0) data[ret]='\0'; else data[0]='\0'; return ret; }
	int send0(const char * data) { return send0((uint8_t*)data, strlen(data)); }
	int send1(const char * data) { return send1((uint8_t*)data, strlen(data)); }
	int send (const char * data) { return send ((uint8_t*)data, strlen(data)); }
	
	//Returns an index to the sockets array, or negative if timeout expires.
	//Negative timeouts mean wait forever.
	//It's possible that an active socket returns zero bytes.
	//However, this is guaranteed to happen rarely enough that repeatedly select()ing will leave the CPU mostly idle.
	static int select(socket* * socks, int nsocks, int timeout_ms = -1);
	
	virtual ~socket() {}
	
	//Can be used to keep a socket alive across exec().
	//Remember to serialize the SSL socket if this is used.
#ifdef __linux__
	static socket* create_from_fd(int fd);
	int get_fd() { return fd; }
#endif
};

class socketssl : public socket {
protected:
	socketssl(){}
public:
	//If 'permissive' is true, the server certificate won't be verified.
	static socketssl* create(const char * domain, int port, bool permissive=false)
	{
		return socketssl::create(socket::create(domain, port), domain, permissive);
	}
	//On entry, this takes ownership of the connection. Even if connection fails, the socket may not be used anymore.
	//The socket must be a normal TCP socket. UDP and nested SSL is not supported.
	static socketssl* create(socket* parent, const char * domain, bool permissive=false);
	
	
	//Can be used to keep a socket alive across exec().
	//Returns the number of bytes required. Call with data=NULL len=0 to find how many bytes to use.
	//If return value is 0, this SSL implementation doesn't support serialization.
	virtual size_t serialize(uint8_t* data, size_t len) { return 0; }
	static socketssl* unserialize(socket* inner, const uint8_t* data, size_t len);
};
