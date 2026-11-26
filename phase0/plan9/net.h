/*
 * What net_p9.c, the BSD socket calls on Plan 9 networking, shares
 * with its system part: net9f.c on 9front, netp9p.c on plan9port.
 * Include after <u.h> and <libc.h>.
 */

enum
{
	Maxfd		= 1024,		/* FD_SETSIZE */
	Ndir		= 40,		/* NETPATHLEN */
	Nrbuf		= 64*1024,	/* stream receive buffer */
	Ncall		= 8,		/* pending connections */
	Ndgram		= 64,		/* pending datagrams */
	Mtu		= 2048,		/* largest datagram kept */
	Udphdr		= 52,		/* raddr laddr ifcaddr [16], rport lport [2] */

	/* Sock.state */
	Snew		= 0,
	Sconnected,
	Slistening,
	Sdgram,
};

typedef struct Call Call;
typedef struct Dgram Dgram;
typedef struct Sock Sock;

/* a connection to accept */
struct Call
{
	int	ctl;
	char	dir[Ndir];
};

struct Dgram
{
	uchar	addr[4];
	uchar	port[2];
	int	n;
	uchar	data[Mtu];
};

struct Sock
{
	Lock	lk;
	int	fd;		/* the descriptor the program has: ctl */
	int	data;		/* data descriptor, or -1 */
	int	type;		/* Sockstream, Sockdgram */
	int	state;
	char	dir[Ndir];
	uchar	local[16];	/* from bind: a glibc sockaddr_in */
	int	bound;

	/* filled by the helper */
	uchar	*buf;		/* Sconnected: received bytes */
	long	nbuf;
	int	eof;
	Call	call[Ncall];	/* Slistening: connections to accept */
	int	call0;
	int	ncall;
	Dgram	*dgram;		/* Sdgram: datagrams, [Ndgram] */
	int	dgram0;
	int	ndgram;

	int	refs;		/* the program's, the helper's */
	int	dead;		/* closed by the program */
	int	helper;		/* has one */
	int	done;		/* the helper has dropped its reference */
	uvlong	helperid;	/* its process or thread */
	void	(*run)(Sock*);
};

/* net9f.c, netp9p.c */
int	starthelper(Sock*, void (*)(Sock*));
void	stophelper(Sock*);
void	adoptdir(Sock*);
int	netdial(Sock*, char*);
int	netaccept(Sock*, Call*);
int	udpannounce(Sock*, char*);
long	udprecv(Sock*, uchar*);
long	udpsend(Sock*, uchar*, long);
int	csquery(char*, uchar*);

/* net_p9.c */
void	adopt(Sock*, int);
