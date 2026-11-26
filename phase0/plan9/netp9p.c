/*
 * The plan9port part of net_p9.c, for the fast test loop on Linux:
 * helpers are threads, a conversation is the socket itself (its
 * "directory" is /dev/fd/n), UDP goes through udpread and udpwrite,
 * and there is no connection server.
 */
#include <u.h>
#include <libc.h>
#include <pthread.h>
#include "net.h"

long	udpread(int, void*, void*, long);	/* libip, <ip.h> */
long	udpwrite(int, void*, void*, long);

static void*
thread(void *v)
{
	Sock *s;

	s = v;
	s->run(s);
	return nil;
}

int
starthelper(Sock *s, void (*run)(Sock*))
{
	pthread_t t;

	s->run = run;
	s->refs++;
	if(pthread_create(&t, nil, thread, s) != 0){
		s->refs--;
		return -1;
	}
	s->helperid = (uvlong)t;
	s->helper = 1;
	return 0;
}

/* cancelled in its blocking call, where it holds no lock */
void
stophelper(Sock *s)
{
	pthread_t t;

	t = (pthread_t)s->helperid;
	pthread_cancel(t);
	pthread_join(t, nil);
	if(!s->done)
		s->refs--;	/* the helper's */
}

void
adoptdir(Sock *s)
{
	snprint(s->dir, sizeof s->dir, "/dev/fd/%d", s->fd);
}

int
netdial(Sock *s, char *addr)
{
	int fd;

	fd = dial(addr, nil, nil, nil);
	if(fd < 0)
		return -1;
	adopt(s, fd);
	s->data = s->fd;
	return 0;
}

/* listen gave the connected socket */
int
netaccept(Sock *s, Call*)
{
	s->data = s->fd;
	return 0;
}

/*
 * Any address is 0.0.0.0, not *: plan9port makes * an IPv6 socket,
 * whose IPv4 peers udpread does not take.
 */
int
udpannounce(Sock *s, char *addr)
{
	char buf[64], *p;
	int fd;

	p = strstr(addr, "!*!");
	if(p != nil){
		snprint(buf, sizeof buf, "%.*s!0.0.0.0!%s", (int)(p - addr), addr, p + 3);
		addr = buf;
	}
	fd = announce(addr, s->dir);
	if(fd < 0)
		return -1;
	adopt(s, fd);
	s->data = s->fd;
	return 0;
}

long
udprecv(Sock *s, uchar *b)
{
	return udpread(s->data, b, b + Udphdr, Mtu);
}

long
udpsend(Sock *s, uchar *b, long n)
{
	return udpwrite(s->data, b, b + Udphdr, n);
}

int
csquery(char*, uchar*)
{
	return 0;
}
