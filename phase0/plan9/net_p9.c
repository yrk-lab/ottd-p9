/*
 * BSD sockets on Plan 9 networking, for OpenTTD's network code: TCP and
 * UDP over IPv4, numeric addresses and /net/cs names.  The program
 * polls: non-blocking accept, recv and recvfrom, and select with no
 * timeout.  Plan 9 blocks instead, so each socket that receives has a
 * helper process that blocks for it (in read, or listen for a
 * listening socket) and leaves what arrives in the socket's buffers;
 * the calls look only at those.  Sends are plain writes.
 *
 * A socket is a network conversation, through dial, announce, listen
 * and accept.  socket() reserves its descriptor (/dev/null) before
 * the conversation exists; the conversation's ctl descriptor is moved
 * onto it once dial or announce made one.
 *
 * Helpers never call malloc: their buffers are allocated with the
 * socket, so a helper killed at close or exit (listening and UDP
 * sockets) holds no lock but its socket's.  A connected socket's
 * reader ends by itself when close shuts the connection, and frees
 * the socket.
 *
 * The system part is in net9f.c on 9front (helpers are rfork(RFMEM)
 * processes, UDP uses /net's header mode) and netp9p.c on Linux with
 * plan9port, the fast test loop (helpers are threads, UDP through
 * udpread and udpwrite); net.h is the interface.  The calls are renamed
 * c_<name> by lower.py --libc, so they never take the place of the C
 * library's own, which plan9port's lib9 uses.  Structures are glibc's
 * x86-64 layouts and error numbers Linux's.
 */
#include <u.h>
#include <libc.h>
#include "net.h"

int	*__errno_location(void);

enum
{
	/* Linux */
	Afunspec	= 0,
	Afinet		= 2,
	Afinet6		= 10,
	Sockstream	= 1,
	Sockdgram	= 2,
	Socktype	= 0xf,		/* without SOCK_NONBLOCK, SOCK_CLOEXEC */
	Ipprototcp	= 6,
	Ipprotoudp	= 17,
	Solsocket	= 1,
	Soerror		= 4,
	Aipassive	= 1,
	Ainumerichost	= 4,
	Fionbio		= 0x5421,

	Ebadf		= 9,
	Eagain		= 11,
	Enomem		= 12,
	Einval		= 22,
	Emfile		= 24,
	Enotty		= 25,
	Epipe		= 32,
	Enotsock	= 88,
	Eopnotsupp	= 95,
	Eafnosupport	= 97,
	Eaddrinuse	= 98,
	Econnrefused	= 111,

	Eainoname	= -2,
	Eaifail		= -4,
	Eaifamily	= -6,
	Eaiservice	= -8,
	Eaimemory	= -10,
};

/* glibc struct sockaddr_in */
typedef struct Csin Csin;
struct Csin
{
	ushort	family;
	uchar	port[2];	/* network order */
	uchar	addr[4];
	uchar	zero[8];
};

/* glibc struct addrinfo */
typedef struct Caddrinfo Caddrinfo;
struct Caddrinfo
{
	int	flags;
	int	family;
	int	socktype;
	int	protocol;
	uint	addrlen;
	Csin	*addr;
	char	*canonname;
	Caddrinfo	*next;
};

/* glibc struct timeval */
typedef struct Ctimeval Ctimeval;
struct Ctimeval
{
	vlong	sec;
	vlong	usec;
};

static Sock	*socks[Maxfd];
static uchar	v4prefix[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };

static void
seterr(int e)
{
	*__errno_location() = e;
}

static Sock*
lookup(int fd)
{
	Sock *s;

	s = nil;
	if(fd >= 0 && fd < Maxfd)
		s = socks[fd];
	if(s == nil)
		seterr(Enotsock);
	return s;
}

/* drop a reference; the last one frees the socket */
static void
release(Sock *s)
{
	int r;

	lock(&s->lk);
	r = --s->refs;
	unlock(&s->lk);
	if(r == 0){
		free(s->buf);
		free(s->obuf);
		free(s->dgram);
		free(s);
	}
}

/*
 * Helpers, started with starthelper (net9f.c, netp9p.c)
 */

/* connected: read into buf until the connection ends */
static void
streamreader(Sock *s)
{
	uchar tmp[8192];
	long n, m;
	int dead;

	for(;;){
		lock(&s->lk);
		m = Nrbuf - s->nbuf;
		dead = s->dead;
		unlock(&s->lk);
		if(dead)
			break;
		if(m == 0){
			sleep(5);	/* full: wait for the program */
			continue;
		}
		if(m > sizeof tmp)
			m = sizeof tmp;
		n = read(s->data, tmp, m);
		lock(&s->lk);
		if(n <= 0)
			s->eof = 1;
		else{
			memmove(s->buf + s->nbuf, tmp, n);
			s->nbuf += n;
		}
		dead = s->dead;
		unlock(&s->lk);
		if(n <= 0 || dead)
			break;
	}
	lock(&s->lk);
	s->done = 1;
	unlock(&s->lk);
	release(s);
}

/* listening: take connections, up to Ncall waiting */
static void
listener(Sock *s)
{
	char dir[Ndir];
	Call *c;
	int fd;

	for(;;){
		fd = listen(s->dir, dir);
		if(fd < 0){
			sleep(100);
			continue;
		}
		lock(&s->lk);
		if(s->ncall < Ncall){
			c = &s->call[(s->call0 + s->ncall) % Ncall];
			c->ctl = fd;
			strecpy(c->dir, c->dir + sizeof c->dir, dir);
			s->ncall++;
			fd = -1;
		}
		unlock(&s->lk);
		if(fd >= 0)
			close(fd);	/* too many waiting: refuse */
	}
}

/* UDP: read datagrams, up to Ndgram waiting */
static void
dgramreader(Sock *s)
{
	uchar b[Udphdr + Mtu];
	Dgram *d;
	long n;

	for(;;){
		n = udprecv(s, b);
		if(n < 0){
			sleep(10);
			continue;
		}
		lock(&s->lk);
		if(s->ndgram < Ndgram){
			d = &s->dgram[(s->dgram0 + s->ndgram) % Ndgram];
			memmove(d->addr, b + 12, 4);	/* raddr, an IPv4 address in IPv6 form */
			memmove(d->port, b + 48, 2);	/* rport */
			d->n = n;
			memmove(d->data, b + Udphdr, n);
			s->ndgram++;
		}
		unlock(&s->lk);
	}
}

/*
 * Addresses
 */

static int
port(Csin *sin)
{
	return sin->port[0]<<8 | sin->port[1];
}

static void
setport(Csin *sin, int p)
{
	sin->port[0] = p>>8;
	sin->port[1] = p;
}

/* a.b.c.d: 0 if s is not one */
static int
parseip4(char *s, uchar *a)
{
	int i;
	long v;
	char *e;

	for(i = 0; i < 4; i++){
		if(*s < '0' || *s > '9')
			return 0;
		v = strtol(s, &e, 10);
		if(v > 255 || (i < 3 && *e != '.') || (i == 3 && *e != 0))
			return 0;
		a[i] = v;
		s = e + 1;
	}
	return 1;
}

/* a Plan 9 system name as an IPv4 address: *, ::, ::ffff:a.b.c.d, a.b.c.d */
static void
sysaddr(char *sys, uchar *a)
{
	char *p;

	memset(a, 0, 4);
	if(sys == nil)
		return;
	p = strrchr(sys, ':');
	parseip4(p != nil ? p + 1 : sys, a);
}

static char*
ipstr(char *buf, int n, uchar *a)
{
	if(a[0] == 0 && a[1] == 0 && a[2] == 0 && a[3] == 0)
		snprint(buf, n, "*");
	else
		snprint(buf, n, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
	return buf;
}

/* tcp!a.b.c.d!port */
static char*
netaddr(char *buf, int n, char *net, Csin *sin)
{
	char ip[32];

	snprint(buf, n, "%s!%s!%d", net, ipstr(ip, sizeof ip, sin->addr), port(sin));
	return buf;
}

static void
fillsin(void *sa, void *lenp, uchar *addr, int p)
{
	Csin sin;

	if(sa == nil)
		return;
	memset(&sin, 0, sizeof sin);
	sin.family = Afinet;
	memmove(sin.addr, addr, 4);
	setport(&sin, p);
	memmove(sa, &sin, sizeof sin);
	if(lenp != nil)
		*(uint*)lenp = sizeof sin;
}

/* the conversation's ctl (or the socket) becomes the program's descriptor */
void
adopt(Sock *s, int fd)
{
	if(fd != s->fd){
		dup(fd, s->fd);
		close(fd);
	}
	adoptdir(s);
}

static Sock*
newsock(int fd, int type)
{
	Sock *s;

	s = mallocz(sizeof *s, 1);
	if(s == nil)
		return nil;
	s->fd = fd;
	s->data = -1;
	s->type = type;
	s->refs = 1;
	socks[fd] = s;
	return s;
}

/*
 * The calls
 */

int
c_socket(int family, int type, int)
{
	int fd;

	if(family != Afinet){
		seterr(Eafnosupport);
		return -1;
	}
	type &= Socktype;
	if(type != Sockstream && type != Sockdgram){
		seterr(Einval);
		return -1;
	}
	fd = open("/dev/null", ORDWR);
	if(fd < 0){
		seterr(Emfile);
		return -1;
	}
	if(fd >= Maxfd || newsock(fd, type) == nil){
		close(fd);
		seterr(Emfile);
		return -1;
	}
	return fd;
}

/* UDP: announce now, receive from here on */
static int
udpbind(Sock *s)
{
	char addr[64];

	if(udpannounce(s, netaddr(addr, sizeof addr, "udp", (Csin*)s->local)) < 0){
		seterr(Eaddrinuse);
		return -1;
	}
	s->dgram = mallocz(Ndgram * sizeof(Dgram), 1);
	if(s->dgram == nil){
		seterr(Enomem);
		return -1;
	}
	s->state = Sdgram;
	if(starthelper(s, dgramreader) < 0){
		seterr(Enomem);
		return -1;
	}
	return 0;
}

/* bind, listen and accept are Plan 9 libc names too */
int
c_bind(int fd, void *sa, uint len)
{
	Sock *s;
	Csin *sin;

	s = lookup(fd);
	if(s == nil)
		return -1;
	sin = sa;
	if(len < sizeof *sin || sin->family != Afinet){
		seterr(Eafnosupport);
		return -1;
	}
	if(s->bound || s->state != Snew){
		seterr(Einval);
		return -1;
	}
	memmove(s->local, sin, sizeof *sin);
	s->bound = 1;
	if(s->type == Sockdgram)
		return udpbind(s);
	return 0;
}

int
c_listen(int fd, int)
{
	Sock *s;
	char addr[64];
	int ctl;

	s = lookup(fd);
	if(s == nil)
		return -1;
	if(s->type != Sockstream || s->state != Snew){
		seterr(Eopnotsupp);
		return -1;
	}
	ctl = announce(netaddr(addr, sizeof addr, "tcp", (Csin*)s->local), s->dir);
	if(ctl < 0){
		seterr(Eaddrinuse);
		return -1;
	}
	adopt(s, ctl);
	s->state = Slistening;
	if(starthelper(s, listener) < 0){
		seterr(Enomem);
		return -1;
	}
	return 0;
}

static int
startreader(Sock *s)
{
	s->buf = malloc(Nrbuf);
	s->obuf = malloc(Nobuf);
	if(s->buf == nil || s->obuf == nil)
		return -1;
	s->state = Sconnected;
	return starthelper(s, streamreader);
}

int
c_accept(int fd, void *sa, void *lenp)
{
	Sock *s, *n;
	Call c;
	NetConnInfo *ni;
	uchar a[4];

	s = lookup(fd);
	if(s == nil)
		return -1;
	if(s->state != Slistening){
		seterr(Einval);
		return -1;
	}
	netflush();
	lock(&s->lk);
	if(s->ncall == 0){
		unlock(&s->lk);
		seterr(Eagain);
		return -1;
	}
	c = s->call[s->call0];
	s->call0 = (s->call0 + 1) % Ncall;
	s->ncall--;
	unlock(&s->lk);

	if(c.ctl >= Maxfd || (n = newsock(c.ctl, Sockstream)) == nil){
		close(c.ctl);
		seterr(Emfile);
		return -1;
	}
	strecpy(n->dir, n->dir + sizeof n->dir, c.dir);
	if(netaccept(n, &c) < 0){
		socks[n->fd] = nil;
		release(n);
		close(c.ctl);
		seterr(Econnrefused);
		return -1;
	}
	if(startreader(n) < 0){
		seterr(Enomem);
		return -1;
	}
	ni = getnetconninfo(n->dir, -1);
	memset(a, 0, sizeof a);
	sysaddr(ni != nil ? ni->rsys : nil, a);
	fillsin(sa, lenp, a, ni != nil ? atoi(ni->rserv) : 0);
	if(ni != nil)
		freenetconninfo(ni);
	return n->fd;
}

/* connect blocks, as dial does */
int
c_connect(int fd, void *sa, uint len)
{
	Sock *s;
	Csin *sin;
	char addr[64];

	s = lookup(fd);
	if(s == nil)
		return -1;
	sin = sa;
	if(len < sizeof *sin || sin->family != Afinet){
		seterr(Eafnosupport);
		return -1;
	}
	if(s->type != Sockstream || s->state != Snew){
		seterr(Eopnotsupp);
		return -1;
	}
	if(netdial(s, netaddr(addr, sizeof addr, "tcp", sin)) < 0){
		seterr(Econnrefused);
		return -1;
	}
	if(startreader(s) < 0){
		seterr(Enomem);
		return -1;
	}
	return 0;
}

/*
 * Sending is batched: each of the program's send calls would be a TCP
 * write, and OpenTTD sends a packet per call, so a tick's packets
 * (a frame and its sync, say) could arrive in separate segments and
 * be acted on apart: a client that saw the frame before the sync
 * missed every sync and never acknowledged one, and the server
 * dropped it (issue #7).  What is sent waits in the socket's buffer
 * until it is full or the program next selects, receives, accepts,
 * closes or sleeps (nanosleep, os_p9.c): one write a batch, no delay.
 */
static void
flush(Sock *s)
{
	if(s->nobuf == 0)
		return;
	if(!s->oerr && write(s->data, s->obuf, s->nobuf) != s->nobuf)
		s->oerr = 1;
	s->nobuf = 0;
}

void
netflush(void)
{
	int fd;
	Sock *s;

	for(fd = 0; fd < Maxfd; fd++){
		s = socks[fd];
		if(s != nil && s->nobuf > 0)
			flush(s);
	}
}

vlong
c_send(int fd, void *buf, uvlong n, int)
{
	Sock *s;

	s = lookup(fd);
	if(s == nil)
		return -1;
	if(s->state != Sconnected || s->oerr){
		seterr(Epipe);
		return -1;
	}
	if(s->nobuf + n > Nobuf)
		flush(s);
	if(n >= Nobuf){
		if(write(s->data, buf, n) != n){
			s->oerr = 1;
			seterr(Epipe);
			return -1;
		}
		return n;
	}
	memmove(s->obuf + s->nobuf, buf, n);
	s->nobuf += n;
	return n;
}

vlong
c_recv(int fd, void *buf, uvlong n, int)
{
	Sock *s;
	long m;

	s = lookup(fd);
	if(s == nil)
		return -1;
	if(s->state != Sconnected){
		seterr(Einval);
		return -1;
	}
	netflush();
	lock(&s->lk);
	if(s->nbuf == 0){
		m = s->eof ? 0 : -1;
		unlock(&s->lk);
		if(m < 0)
			seterr(Eagain);
		return m;
	}
	m = s->nbuf;
	if(m > n)
		m = n;
	memmove(buf, s->buf, m);
	s->nbuf -= m;
	memmove(s->buf, s->buf + m, s->nbuf);
	unlock(&s->lk);
	return m;
}

vlong
c_sendto(int fd, void *buf, uvlong n, int flags, void *sa, uint len)
{
	Sock *s;
	Csin *sin, any;
	uchar b[Udphdr + Mtu];
	long r;

	s = lookup(fd);
	if(s == nil)
		return -1;
	if(s->type == Sockstream)
		return c_send(fd, buf, n, flags);
	sin = sa;
	if(sin == nil || len < sizeof *sin || sin->family != Afinet){
		seterr(Eafnosupport);
		return -1;
	}
	if(n > Mtu){
		seterr(Einval);
		return -1;
	}
	if(s->state == Snew){
		/* not bound: any port */
		memset(&any, 0, sizeof any);
		any.family = Afinet;
		memmove(s->local, &any, sizeof any);
		s->bound = 1;
		if(udpbind(s) < 0)
			return -1;
	}
	memset(b, 0, Udphdr);
	memmove(b, v4prefix, 12);
	memmove(b + 12, sin->addr, 4);
	memmove(b + 48, sin->port, 2);
	memmove(b + Udphdr, buf, n);
	r = udpsend(s, b, n);
	if(r < 0){
		seterr(Einval);
		return -1;
	}
	return r;
}

vlong
c_recvfrom(int fd, void *buf, uvlong n, int flags, void *sa, void *lenp)
{
	Sock *s;
	Dgram *d;
	long m;

	s = lookup(fd);
	if(s == nil)
		return -1;
	if(s->type == Sockstream)
		return c_recv(fd, buf, n, flags);
	if(s->state != Sdgram){
		seterr(Eagain);
		return -1;
	}
	lock(&s->lk);
	if(s->ndgram == 0){
		unlock(&s->lk);
		seterr(Eagain);
		return -1;
	}
	d = &s->dgram[s->dgram0];
	m = d->n;
	if(m > n)
		m = n;	/* the rest is lost, as with recvfrom */
	memmove(buf, d->data, m);
	fillsin(sa, lenp, d->addr, d->port[0]<<8 | d->port[1]);
	s->dgram0 = (s->dgram0 + 1) % Ndgram;
	s->ndgram--;
	unlock(&s->lk);
	return m;
}

/* no options: TCP_NODELAY, SO_REUSEADDR, SO_BROADCAST... are accepted */
int
c_setsockopt(int fd, int, int, void*, uint)
{
	return lookup(fd) == nil ? -1 : 0;
}

int
c_getsockopt(int fd, int level, int opt, void *v, void *lenp)
{
	if(lookup(fd) == nil)
		return -1;
	if(v != nil && lenp != nil && *(uint*)lenp >= 4){
		*(int*)v = 0;
		if(level == Solsocket && opt == Soerror)
			*(int*)v = 0;	/* connect has finished, or failed */
		*(uint*)lenp = 4;
	}
	return 0;
}

static int
sockname(int fd, void *sa, void *lenp, int remote)
{
	Sock *s;
	NetConnInfo *ni;
	uchar a[4];
	int p;

	s = lookup(fd);
	if(s == nil)
		return -1;
	if(s->state == Snew){
		if(remote){
			seterr(Einval);
			return -1;
		}
		fillsin(sa, lenp, ((Csin*)s->local)->addr, port((Csin*)s->local));
		return 0;
	}
	ni = getnetconninfo(s->dir, -1);
	if(ni == nil){
		seterr(Einval);
		return -1;
	}
	if(remote){
		sysaddr(ni->rsys, a);
		p = atoi(ni->rserv);
	}else{
		sysaddr(ni->lsys, a);
		p = atoi(ni->lserv);
	}
	freenetconninfo(ni);
	fillsin(sa, lenp, a, p);
	return 0;
}

int
c_getsockname(int fd, void *sa, void *lenp)
{
	return sockname(fd, sa, lenp, 0);
}

int
c_getpeername(int fd, void *sa, void *lenp)
{
	return sockname(fd, sa, lenp, 1);
}

/* close(2): a socket's helper and buffers go with it */
static void
sockclose(Sock *s)
{
	flush(s);
	socks[s->fd] = nil;
	lock(&s->lk);
	s->dead = 1;
	unlock(&s->lk);
	if(s->helper)
		stophelper(s);
	if(s->data >= 0 && s->data != s->fd)
		close(s->data);
	release(s);
}

int
c_close(int fd)
{
	if(fd >= 0 && fd < Maxfd && socks[fd] != nil)
		sockclose(socks[fd]);
	if(close(fd) < 0){
		seterr(Ebadf);
		return -1;
	}
	return 0;
}

/* FIONBIO: sockets never block */
int
c_ioctl(int, uvlong req, ...)
{
	if(req == Fionbio)
		return 0;
	seterr(Enotty);
	return -1;
}

static int
readable(Sock *s)
{
	int r;

	lock(&s->lk);
	switch(s->state){
	case Sconnected:
		r = s->nbuf > 0 || s->eof;
		break;
	case Slistening:
		r = s->ncall > 0;
		break;
	case Sdgram:
		r = s->ndgram > 0;
		break;
	default:
		r = 0;
		break;
	}
	unlock(&s->lk);
	return r;
}

static int
writable(Sock *s)
{
	return s->state == Sconnected || s->type == Sockdgram;
}

/*
 * select over glibc fd_sets (1024 bits).  Only sockets are ever
 * ready: the program polls standard input with it too (the dedicated
 * server's console), and a read there would block.
 */
int
c_select(int n, uvlong *r, uvlong *w, uvlong *e, Ctimeval *tv)
{
	uvlong rin[Maxfd/64], win[Maxfd/64], bit;
	vlong deadline;
	Sock *s;
	int fd, cnt, i;

	if(n > Maxfd)
		n = Maxfd;
	netflush();
	for(i = 0; i < Maxfd/64; i++){
		rin[i] = r != nil ? r[i] : 0;
		win[i] = w != nil ? w[i] : 0;
	}
	if(e != nil)
		memset(e, 0, sizeof rin);
	deadline = 0;
	if(tv != nil)
		deadline = nsec() + tv->sec*1000000000LL + tv->usec*1000LL;
	for(;;){
		if(r != nil)
			memset(r, 0, sizeof rin);
		if(w != nil)
			memset(w, 0, sizeof win);
		cnt = 0;
		for(fd = 0; fd < n; fd++){
			bit = 1ULL << (fd % 64);
			if(!((rin[fd/64] | win[fd/64]) & bit))
				continue;
			s = socks[fd];
			if(s == nil)
				continue;
			if((rin[fd/64] & bit) && readable(s)){
				r[fd/64] |= bit;
				cnt++;
			}
			if((win[fd/64] & bit) && writable(s)){
				w[fd/64] |= bit;
				cnt++;
			}
		}
		if(cnt > 0 || (tv != nil && nsec() >= deadline))
			return cnt;
		sleep(1);
	}
}

/*
 * Names
 */

int
c_getaddrinfo(char *node, char *service, Caddrinfo *hints, Caddrinfo **res)
{
	Caddrinfo *ai;
	Csin *sin;
	uchar a[4];
	int family, type, flags, p;
	char *e;

	*res = nil;
	family = hints != nil ? hints->family : Afunspec;
	type = hints != nil ? hints->socktype : 0;
	flags = hints != nil ? hints->flags : 0;
	if(family != Afunspec && family != Afinet)
		return Eaifamily;
	p = 0;
	if(service != nil && *service != 0){
		p = strtol(service, &e, 10);
		if(*e != 0 || p < 0 || p > 65535)
			return Eaiservice;
	}
	memset(a, 0, sizeof a);
	if(node == nil || *node == 0){
		if(!(flags & Aipassive)){
			a[0] = 127;
			a[3] = 1;
		}
	}else if(!parseip4(node, a)){
		if(flags & Ainumerichost)
			return Eainoname;
		if(strcmp(node, "localhost") == 0){
			a[0] = 127;
			a[3] = 1;
		}else if(!csquery(node, a))
			return Eainoname;
	}
	if(type == 0)
		type = Sockstream;
	ai = mallocz(sizeof *ai + sizeof *sin, 1);
	if(ai == nil)
		return Eaimemory;
	sin = (Csin*)(ai + 1);
	sin->family = Afinet;
	memmove(sin->addr, a, 4);
	setport(sin, p);
	ai->family = Afinet;
	ai->socktype = type;
	ai->protocol = type == Sockstream ? Ipprototcp : Ipprotoudp;
	ai->addrlen = sizeof *sin;
	ai->addr = sin;
	*res = ai;
	return 0;
}

void
c_freeaddrinfo(Caddrinfo *ai)
{
	Caddrinfo *next;

	for(; ai != nil; ai = next){
		next = ai->next;
		free(ai);
	}
}

/* numeric only, as NI_NUMERICHOST|NI_NUMERICSERV */
int
c_getnameinfo(Csin *sin, uint, char *host, uint hostlen, char *serv, uint servlen, int)
{
	if(sin == nil)
		return Eaifamily;
	if(sin->family == Afinet6){
		if(host != nil && hostlen > 0)
			snprint(host, hostlen, "::");
	}else if(sin->family == Afinet){
		if(host != nil && hostlen > 0)
			snprint(host, hostlen, "%d.%d.%d.%d", sin->addr[0], sin->addr[1], sin->addr[2], sin->addr[3]);
	}else
		return Eaifamily;
	if(serv != nil && servlen > 0)
		snprint(serv, servlen, "%d", port(sin));
	return 0;
}

char*
c_gai_strerror(int e)
{
	switch(e){
	case Eainoname:
		return "Name or service not known";
	case Eaifamily:
		return "ai_family not supported";
	case Eaiservice:
		return "Servname not supported for ai_socktype";
	case Eaimemory:
		return "Memory allocation failure";
	}
	return "Non-recoverable failure in name resolution";
}

/* no interfaces listed: OpenTTD finds no broadcast addresses */
int
c_getifaddrs(void **ifap)
{
	*ifap = nil;
	return 0;
}

void
c_freeifaddrs(void*)
{
}
