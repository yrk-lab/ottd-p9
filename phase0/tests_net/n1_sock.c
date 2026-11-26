/*
 * n1_sock: the BSD socket calls as OpenTTD's network code makes them,
 * on one machine: a TCP listener polled with select and non-blocking
 * accept, a client connect, data both ways, end of stream, and UDP
 * datagrams between two bound sockets.  Prints what it sees; the
 * output is the same on Linux, through plan9port and on 9front.
 *
 *	n1_sock [ip [port]]	the address to connect to (127.0.0.1)
 */
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static void
fail(const char *what)
{
	printf("FAIL %s (errno %d)\n", what, errno);
	exit(1);
}

/* poll as OpenTTD does: select with no timeout, then sleep a little */
static int
waitready(int fd, int write)
{
	fd_set fds;
	struct timeval tv;
	struct timespec ms = { 0, 1000000 };
	int i;

	for (i = 0; i < 5000; i++) {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		if (select(FD_SETSIZE, write ? NULL : &fds, write ? &fds : NULL, NULL, &tv) < 0)
			fail("select");
		if (FD_ISSET(fd, &fds))
			return 1;
		nanosleep(&ms, NULL);
	}
	return 0;
}

static struct addrinfo *
resolve(const char *host, const char *port, int type, int passive)
{
	struct addrinfo hints, *ai;
	int e;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = type;
	hints.ai_flags = passive ? AI_PASSIVE : 0;
	e = getaddrinfo(host, port, &hints, &ai);
	if (e != 0) {
		printf("FAIL getaddrinfo %s: %s\n", host ? host : "(passive)", gai_strerror(e));
		exit(1);
	}
	return ai;
}

static void
printaddr(const char *what, struct sockaddr *sa, socklen_t len, int withport)
{
	char host[64], serv[16];

	if (getnameinfo(sa, len, host, sizeof host, serv, sizeof serv, NI_NUMERICHOST | NI_NUMERICSERV) != 0)
		fail("getnameinfo");
	if (withport)
		printf("%s %s port %s\n", what, host, serv);
	else
		printf("%s %s\n", what, host);
}

static int
nonblocking(int fd)
{
	int on = 1;

	return ioctl(fd, FIONBIO, &on);
}

static void
tcp(const char *ip, int port)
{
	struct addrinfo *ai;
	struct sockaddr_storage ss;
	socklen_t len;
	char ps[16], buf[64];
	int ls, cs, as, on, n, i;
	ssize_t r;

	snprintf(ps, sizeof ps, "%d", port);

	/* the server, as ListenLoopProc */
	ai = resolve(NULL, ps, SOCK_STREAM, 1);
	printf("passive family %d socktype %d addrlen %d\n", ai->ai_family, ai->ai_socktype, (int)ai->ai_addrlen);
	printaddr("passive", ai->ai_addr, ai->ai_addrlen, 1);
	ls = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (ls < 0)
		fail("socket");
	on = 1;
	if (setsockopt(ls, IPPROTO_TCP, TCP_NODELAY, &on, sizeof on) != 0)
		fail("setsockopt TCP_NODELAY");
	if (setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) != 0)
		fail("setsockopt SO_REUSEADDR");
	if (bind(ls, ai->ai_addr, ai->ai_addrlen) != 0)
		fail("bind");
	if (listen(ls, 1) != 0)
		fail("listen");
	if (nonblocking(ls) != 0)
		fail("ioctl FIONBIO");
	freeaddrinfo(ai);

	len = sizeof ss;
	as = accept(ls, (struct sockaddr *)&ss, &len);
	printf("accept before connect: %d errno %s\n", as, errno == EAGAIN ? "EAGAIN" : "other");

	/* the client, as TCPConnecter */
	ai = resolve(ip, ps, SOCK_STREAM, 0);
	cs = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (cs < 0)
		fail("client socket");
	nonblocking(cs);
	if (connect(cs, ai->ai_addr, ai->ai_addrlen) != 0 && errno != EINPROGRESS)
		fail("connect");
	freeaddrinfo(ai);
	printf("client writable %d\n", waitready(cs, 1));
	len = sizeof on;
	if (getsockopt(cs, SOL_SOCKET, SO_ERROR, &on, &len) != 0)
		fail("getsockopt SO_ERROR");
	printf("SO_ERROR %d\n", on);

	printf("listener readable %d\n", waitready(ls, 0));
	len = sizeof ss;
	as = accept(ls, (struct sockaddr *)&ss, &len);
	if (as < 0)
		fail("accept");
	printf("accepted addrlen %d family %d\n", (int)len, ((struct sockaddr *)&ss)->sa_family);
	printaddr("peer", (struct sockaddr *)&ss, len, 0);
	nonblocking(as);
	len = sizeof ss;
	if (getsockname(as, (struct sockaddr *)&ss, &len) != 0)
		fail("getsockname");
	printaddr("local", (struct sockaddr *)&ss, len, 1);
	len = sizeof ss;
	if (getpeername(cs, (struct sockaddr *)&ss, &len) != 0)
		fail("getpeername");
	printaddr("client's peer", (struct sockaddr *)&ss, len, 1);

	r = recv(as, buf, sizeof buf, 0);
	printf("recv before send: %d errno %s\n", (int)r, errno == EAGAIN ? "EAGAIN" : "other");

	/* client to server, in pieces */
	for (i = 0; i < 3; i++) {
		n = snprintf(buf, sizeof buf, "packet %d;", i);
		if (send(cs, buf, n, 0) != n)
			fail("send");
	}
	n = 0;
	while (n < 27 && waitready(as, 0)) {
		r = recv(as, buf + n, sizeof buf - 1 - n, 0);
		if (r <= 0)
			fail("recv");
		n += r;
	}
	buf[n] = 0;
	printf("server got \"%s\"\n", buf);

	/* server to client */
	if (send(as, "welcome", 7, 0) != 7)
		fail("server send");
	waitready(cs, 0);
	r = recv(cs, buf, sizeof buf - 1, 0);
	buf[r > 0 ? r : 0] = 0;
	printf("client got \"%s\"\n", buf);

	/* the client leaves */
	close(cs);
	printf("server readable after close %d\n", waitready(as, 0));
	r = recv(as, buf, sizeof buf, 0);
	printf("recv after close: %d\n", (int)r);
	close(as);
	close(ls);
}

static void
udp(const char *ip, int port)
{
	struct addrinfo *ai;
	struct sockaddr_storage ss;
	socklen_t len;
	char ps[16], buf[64];
	int a, b, on;
	ssize_t r;

	snprintf(ps, sizeof ps, "%d", port);
	ai = resolve(NULL, ps, SOCK_DGRAM, 1);
	a = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (a < 0 || bind(a, ai->ai_addr, ai->ai_addrlen) != 0)
		fail("udp a");
	freeaddrinfo(ai);
	snprintf(ps, sizeof ps, "%d", port + 1);
	ai = resolve(NULL, ps, SOCK_DGRAM, 1);
	b = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (b < 0 || bind(b, ai->ai_addr, ai->ai_addrlen) != 0)
		fail("udp b");
	freeaddrinfo(ai);
	nonblocking(a);
	nonblocking(b);
	on = 1;
	if (setsockopt(a, SOL_SOCKET, SO_BROADCAST, &on, sizeof on) != 0)
		fail("setsockopt SO_BROADCAST");

	len = sizeof ss;
	r = recvfrom(b, buf, sizeof buf, 0, (struct sockaddr *)&ss, &len);
	printf("recvfrom before sendto: %d errno %s\n", (int)r, errno == EAGAIN ? "EAGAIN" : "other");

	snprintf(ps, sizeof ps, "%d", port + 1);
	ai = resolve(ip, ps, SOCK_DGRAM, 0);
	r = sendto(a, "ping", 4, 0, ai->ai_addr, ai->ai_addrlen);
	freeaddrinfo(ai);
	printf("sendto %d\n", (int)r);
	printf("b readable %d\n", waitready(b, 0));
	len = sizeof ss;
	r = recvfrom(b, buf, sizeof buf - 1, 0, (struct sockaddr *)&ss, &len);
	if (r < 0)
		fail("recvfrom");
	buf[r] = 0;
	printf("b got \"%s\" addrlen %d from port %d\n", buf, (int)len, ntohs(((struct sockaddr_in *)&ss)->sin_port));

	/* and back, to the address it came from */
	r = sendto(b, "pong", 4, 0, (struct sockaddr *)&ss, len);
	waitready(a, 0);
	len = sizeof ss;
	r = recvfrom(a, buf, sizeof buf - 1, 0, (struct sockaddr *)&ss, &len);
	if (r < 0)
		fail("recvfrom a");
	buf[r] = 0;
	printf("a got \"%s\" from port %d\n", buf, ntohs(((struct sockaddr_in *)&ss)->sin_port));
	close(a);
	close(b);
}

int
main(int argc, char **argv)
{
	const char *ip;
	int port;

	ip = argc > 1 ? argv[1] : "127.0.0.1";
	port = argc > 2 ? atoi(argv[2]) : 4730;
	tcp(ip, port);
	udp(ip, port + 10);
	printf("done\n");
	return 0;
}
