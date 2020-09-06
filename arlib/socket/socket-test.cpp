#include "socket.h"
#include "../test.h"

//TODO: fetch howsmyssl, ensure the only failure is the session cache
//TODO: some of these tests could be made cleaner with runloop::step()

#ifdef ARLIB_TEST
#include "../os.h"

static void socket_test_httpfail(socket* sock, bool xfail, runloop* loop)
{
	test_skip("too slow");
	
	assert(sock);
	
	uintptr_t timer = loop->raw_set_timer_once(8000, bind_lambda([&]() { assert_unreachable(); loop->exit(); }));
	
	cstring http_get =
		"GET / HTTP/1.1\r\n"
		"Host: example.com\r\n" // this is wrong host, so we'll get a 400 or 404 or whatever
		"Connection: close\r\n" // but the response is valid HTTP and that's all we care about
		"\r\n";
	
	assert_eq(sock->send(http_get.bytes()), http_get.length());
	
	uint8_t buf[8];
	size_t n_buf = 0;
	
	bool inside = false; // reject recursion
	bool exited = false; // and excessive calls after loop->exit()
	int exit_calls = 0;
	function<void()> cb = bind_lambda([&]() // extra variable so deleting the socket doesn't delete the handler
		{                                   // please implement coroutines quickly, gcc and clang
			assert(!exited);
			if (exited) assert_lt(exit_calls++, 5);
			assert(!inside);
			inside = true;
			
			if (n_buf < 8)
			{
				int bytes = sock->recv(arrayvieww<uint8_t>(buf).slice(n_buf, n_buf==0 ? 2 : 1));
				
				if (bytes == 0) goto out;
				if (xfail)
				{
					assert_lt(bytes, 0);
					delete sock;
					sock = NULL;
					
					loop->exit();
					exited = true;
				}
				else
				{
					assert_gte(bytes, 0);
					n_buf += bytes;
				}
			}
			else if (n_buf >= 8)
			{
				uint8_t discard[4096];
				
				if (sock->recv(discard) < 0)
				{
					delete sock;
					sock = NULL;
					loop->exit();
					exited = true;
					goto out;
				}
			}
		out:
			inside = false;
		});
	sock->callback(cb);
	
	loop->enter();
	if (!xfail) assert_eq(string(arrayview<uint8_t>(buf)), "HTTP/1.1");
	
	loop->raw_timer_remove(timer);
	
	delete sock;
	sock = NULL;
}

void socket_test_http(socket* sock, runloop* loop) { socket_test_httpfail(sock, false, loop); }
void socket_test_fail(socket* sock, runloop* loop) { socket_test_httpfail(sock, true, loop); }

static void clienttest(cstring target, int port, bool ssl, bool xfail = false, bool unsafe = false)
{
	test_skip("too slow");
	
	autoptr<runloop> loop = runloop::create();
	
	auto creator = socket::create;
	if (ssl)
		creator = socket::create_ssl;
	if (unsafe)
#if defined(ARLIB_SSL_OPENSSL) || defined(ARLIB_SSL_SCHANNEL)
		creator = [](cstring domain, int port, runloop* loop) -> socket*
			{
				return socket::wrap(socket::wrap_ssl_raw_noverify(socket::create(domain, port, loop), loop), loop);
			};
#else
		test_skip_force("unsafe SSL not implemented");
#endif
	socket* sock = creator(target, port, loop);
	assert(sock);
	
	socket_test_httpfail(sock, xfail, loop);
}

//this IP is www.nic.ad.jp, both lookup and ping time for that one are 300ms
test("TCP client with IP",  "runloop", "tcp") { clienttest("192.41.192.145", 80, false); }
test("TCP client with DNS", "dns",     "tcp") { clienttest("www.nic.ad.jp", 80, false); }

test("SSL client",          "tcp",     "ssl") { clienttest("example.com", 443, true); }
test("SSL SNI",             "tcp",     "ssl") { clienttest("git.io", 443, true); }

test("TCP client, disconnecting server", "runloop,dns", "tcp")
{
	test_skip("too slow");
	test_skip_force("30 seconds is too slow");
	
	autoptr<runloop> loop = runloop::create();
	
	uintptr_t timer = loop->raw_set_timer_once(60000, bind_lambda([&]() { loop->exit(); assert_unreachable(); }));
	
	uint64_t start_ms = time_ms_ne();
	
	//need to provoke Shitty Server into actually dropping the connections
	autoptr<socket> sock2;
	uintptr_t timer2 = loop->raw_set_timer_repeat(5000, bind_lambda([&]() { sock2 = socket::create("floating.muncher.se", 9, loop); }));
	
	autoptr<socket> sock = socket::create("floating.muncher.se", 9, loop);
	assert(sock);
	
	sock->callback(bind_lambda([&]()
		{
			uint8_t buf[64];
			int ret = sock->recv(buf);
			assert_lte(ret, 0);
			if (ret < 0) loop->exit();
		}), NULL);
	
	loop->enter();
	
	//test passes if this thing notices that Shitty Server dropped it within 60 seconds
	//Shitty will drop me after 10-20 seconds, depending on when exactly the other sockets connect
	//Arlib will send a keepalive after 30 seconds, which will immediately return RST and return ECONNRESET
	
	//make sure it didn't fail because Shitty Server is down
	assert_gt(time_ms_ne()-start_ms, 29000);
	loop->raw_timer_remove(timer);
	loop->raw_timer_remove(timer2);
}

test("SSL client, bad root", "tcp", "ssl") { clienttest("superfish.badssl.com", 443, true, true); }
test("SSL client, bad name", "tcp", "ssl") { clienttest("wrong.host.badssl.com", 443, true, true); }
test("SSL client, expired",  "tcp", "ssl") { clienttest("expired.badssl.com", 443, true, true); }

test("SSL client, bad root, permissive", "tcp", "ssl") { clienttest("superfish.badssl.com", 443, true, false, true); }
test("SSL client, bad name, permissive", "tcp", "ssl") { clienttest("wrong.host.badssl.com", 443, true, false, true); }
test("SSL client, expired, permissive",  "tcp", "ssl") { clienttest("expired.badssl.com", 443, true, false, true); }

//test("SSL renegotiation", "tcp", "ssl")
//{
//	test_skip("too slow");
//	assert(!"find or create a renegotiating server, then use it");
//	// perhaps BoarSSL could help, or maybe I should add server support to some SSL engines and do some lamehack to renegotiate
//}

//test("","","")
//{
//	test_skip("too slow");
//	assert(!"allow switching SSL engines at runtime");
//}

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
	array<uint8_t> data = sp->serialize(&fd);
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
	array<uint8_t> bytes = recvall(s, 4);
	assert_eq(string(bytes), "HTTP");
	testcall(ser_test(s));
	bytes = recvall(s, 4);
	assert_eq(string(bytes), "/1.1");
}
*/
#endif

test("TCP listen", "tcp", "")
{
	runloop* loop = runloop::global();
	
	uintptr_t timer = loop->raw_set_timer_once(1000, bind_lambda([&]() { loop->exit(); assert_unreachable(); }));
	
	int port = time(NULL)%3600 + 10000; // running tests twice quickly fails due to TIME_WAIT unless port varies
	
	autoptr<socket> a;
	autoptr<socket> b;
	autoptr<socketlisten> lst = socketlisten::create(port, loop, [&](autoptr<socket> s) { b = std::move(s); loop->exit(); });
	
	assert(lst);
	a = socket::create("localhost", port, loop);
	assert(a);
	loop->enter();
	assert(b);
	
	a->send(cstring("hello").bytes());
	b->send(cstring("world").bytes());
	
	// not supposed to call recv outside a runloop, but it works in practice and compactness is better than correctness in tests.
	uint8_t tmp[6] = {0};
	b->recv(tmp);
	assert_eq(cstring((char*)tmp), "hello");
	a->recv(tmp);
	assert_eq(cstring((char*)tmp), "world");
	
	loop->raw_timer_remove(timer);
}
#endif
