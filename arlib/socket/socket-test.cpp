#include "socket.h"
#include "../test.h"

//TODO: fetch howsmyssl, ensure the only failure is the session cache

#ifdef ARLIB_TEST

static void clienttest(cstring target, int port, bool ssl, bool xfail = false)
{
	if (ssl) socket::wrap_ssl(NULL, "", NULL); // bearssl takes forever to initialize, do it outside the runloop check
	
	test_skip("too slow");
	
	autoptr<runloop> loop = runloop::create();
	bool timeout = false;
	loop->set_timer_rel(8000, bind_lambda([&]()->bool { timeout = true; loop->exit(); return false; }));
	
	autoptr<socket> sock = (ssl ? socket::create_ssl : socket::create)(target, port, loop);
	assert(sock);
	
	//ugly, but the alternative is nesting lambdas forever or busywait. I need a way to break it anyways
	function<void(socket*)> break_runloop = bind_lambda([&](socket*) { loop->exit(); });
	
	cstring http_get =
		"GET / HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Connection: close\r\n"
		"\r\n";
	
	assert_eq(sock->send(http_get.bytes()), http_get.length());
	
	sock->callback(loop, break_runloop, NULL);
	
	uint8_t buf[4];
	size_t n_buf = 0;
	while (n_buf < 4)
	{
		testcall(loop->enter());
		assert(!timeout);
		
		int bytes = sock->recv(arrayvieww<byte>(buf).skip(n_buf));
		if (xfail)
		{
			if (bytes == 0) continue;
			assert_lt(bytes, 0);
			return;
		}
		assert_gte(bytes, 0);
		n_buf += bytes;
	}
	
	assert_eq(string(arrayview<byte>(buf)), "HTTP");
}

test("TCP client with IP") { clienttest("192.41.192.145", 80, false); } // www.nic.ad.jp
test("TCP client with DNS") { clienttest("www.nic.ad.jp", 80, false); } // both lookup and ping time for that one are 300ms
test("SSL client") { clienttest("google.com", 443, true); }
test("SSL SNI") { clienttest("git.io", 443, true); }

//TODO: test permissiveness for those
test("SSL client, bad root") { clienttest("superfish.badssl.com", 443, true, true); }
test("SSL client, bad name") { clienttest("wrong.host.badssl.com", 443, true, true); }
test("SSL client, expired") { clienttest("expired.badssl.com", 443, true, true); }

#ifdef ARLIB_SSL_BEARSSL
/*
static void ser_test(autoptr<socketssl>& s)
{
//int fd;
//s->serialize(&fd);
	
	socketssl* sp = s.release();
	assert(sp);
	assert(!s);
	int fd;
	array<byte> data = sp->serialize(&fd);
	assert(data);
	s = socketssl::deserialize(fd, data);
	assert(s);
}

test("SSL serialization")
{
	test_skip("too slow");
	autoptr<socketssl> s = socketssl::create("google.com", 443);
	testcall(ser_test(s));
	s->send("GET / HTTP/1.1\n");
	testcall(ser_test(s));
	s->send("Host: google.com\nConnection: close\n\n");
	testcall(ser_test(s));
	array<byte> bytes = recvall(s, 4);
	assert_eq(string(bytes), "HTTP");
	testcall(ser_test(s));
	bytes = recvall(s, 4);
	assert_eq(string(bytes), "/1.1");
}
*/
#endif
#endif
