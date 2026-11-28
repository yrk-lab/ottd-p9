/*
 * The 9front part of net_p9.c: helpers are processes sharing memory
 * (thread9f.c's p9proc), UDP is /net's header mode, names go to /net/cs.
 */
#include <u.h>
#include <libc.h>
#include "net.h"

static int	pids[Maxfd];	/* helpers, by socket descriptor */

/* at exit, stop the helpers, which would outlive the program */
static void
stopall(void)
{
	int i;

	for(i = 0; i < Maxfd; i++)
		if(pids[i] > 0)
			postnote(PNPROC, pids[i], "kill");
}

int	p9proc(void (*)(void*), void*);	/* thread9f.c */
void	tsetname(char*);

static void
sockrun(void *a)
{
	Sock *s;

	s = a;
	tsetname("net");
	s->run(s);
}

/* a process sharing memory (thread9f.c's p9proc: on a stack in it) */
int
starthelper(Sock *s, void (*run)(Sock*))
{
	static int once;
	int pid;

	if(!once){
		atexit(stopall);
		once = 1;
	}
	s->run = run;
	s->refs++;
	pid = p9proc(sockrun, s);
	if(pid < 0){
		s->refs--;
		return -1;
	}
	s->helperid = pid;
	s->helper = 1;
	pids[s->fd] = pid;
	return 0;
}

/*
 * A reader ends on the close and drops its reference: "close" is
 * TCP's graceful close (FIN, as close(2) does), which also ends the
 * reader's read.  Others are killed and keep theirs: the socket stays
 * allocated, as the helper may run a moment longer.
 */
void
stophelper(Sock *s)
{
	if(s->state == Sconnected)
		write(s->fd, "close", 5);
	else
		postnote(PNPROC, s->helperid, "kill");
	pids[s->fd] = 0;
}

/* the directory is the conversation's: it does not change with the descriptor */
void
adoptdir(Sock*)
{
}

int
netdial(Sock *s, char *addr)
{
	int ctl;

	ctl = -1;
	s->data = dial(addr, nil, s->dir, &ctl);
	if(s->data < 0)
		return -1;
	adopt(s, ctl);
	return 0;
}

int
netaccept(Sock *s, Call *c)
{
	s->data = accept(c->ctl, c->dir);
	return s->data < 0 ? -1 : 0;
}

/* announce, with a header before each datagram */
int
udpannounce(Sock *s, char *addr)
{
	char data[Ndir+8];
	int ctl;

	ctl = announce(addr, s->dir);
	if(ctl < 0)
		return -1;
	snprint(data, sizeof data, "%s/data", s->dir);
	if(write(ctl, "headers", 7) != 7 || (s->data = open(data, ORDWR)) < 0){
		close(ctl);
		return -1;
	}
	adopt(s, ctl);
	return 0;
}

long
udprecv(Sock *s, uchar *b)
{
	return read(s->data, b, Udphdr + Mtu) - Udphdr;
}

long
udpsend(Sock *s, uchar *b, long n)
{
	n = write(s->data, b, Udphdr + n);
	return n < 0 ? -1 : n - Udphdr;
}

/* host as an IPv4 address, through the connection server */
int
csquery(char *host, uchar *a)
{
	char buf[128], *p, *e;
	int fd, n, i;
	long v;

	fd = open("/net/cs", ORDWR);
	if(fd < 0)
		return 0;
	snprint(buf, sizeof buf, "tcp!%s!1", host);
	if(write(fd, buf, strlen(buf)) < 0){
		close(fd);
		return 0;
	}
	seek(fd, 0, 0);
	while((n = read(fd, buf, sizeof buf - 1)) > 0){
		/* /net/tcp/clone a.b.c.d!1 */
		buf[n] = 0;
		p = strchr(buf, ' ');
		if(p == nil)
			continue;
		p++;
		for(i = 0; i < 4; i++){
			v = strtol(p, &e, 10);
			if(e == p || v > 255 || *e != (i < 3 ? '.' : '!'))
				break;
			a[i] = v;
			p = e + 1;
		}
		if(i == 4){
			close(fd);
			return 1;
		}
	}
	close(fd);
	return 0;
}
