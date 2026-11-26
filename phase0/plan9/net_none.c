/*
 * Sockets where there is no network: goken's Linux-hosted libc (the
 * fast test loop) has no dial or announce.  Every call fails cleanly,
 * which OpenTTD's network code takes as "no network"; select only
 * waits.  On 9front, net_p9.c has the real ones.  Structures are
 * glibc's x86-64 layouts and error numbers Linux's.
 */
#include <u.h>
#include <libc.h>

int	*__errno_location(void);	/* libc_p9.c */
void	c_seterrno(void);

enum
{
	/* Linux errno */
	Ebadf		= 9,
	Enotty		= 25,
	Enosys		= 38,
	Eafnosupport	= 97,

	/* ioctl */
	Fionbio		= 0x5421,

	/* getaddrinfo */
	Eaifail		= -4,
};

static void
seterr(int e)
{
	*__errno_location() = e;
}

/*
 * The socket calls are renamed c_<name> by lower.py, as is close,
 * since net_p9.c's must also release a socket.
 */

int
c_close(int fd)
{
	if(close(fd) < 0){
		c_seterrno();
		return -1;
	}
	return 0;
}

/* FIONBIO succeeds, as for net_p9.c's sockets; nothing else is supported */
int
c_ioctl(int, uvlong req, ...)
{
	if(req == Fionbio)
		return 0;
	seterr(Enotty);
	return -1;
}


int
c_socket(int, int, int)
{
	seterr(Eafnosupport);
	return -1;
}

static int
nosock(void)
{
	seterr(Ebadf);
	return -1;
}

int
c_bind(int, void*, uint)
{
	return nosock();
}

int
c_listen(int, int)
{
	return nosock();
}

int
c_accept(int, void*, void*)
{
	return nosock();
}

int
c_connect(int, void*, uint)
{
	return nosock();
}

vlong
c_send(int, void*, uvlong, int)
{
	return nosock();
}

vlong
c_sendto(int, void*, uvlong, int, void*, uint)
{
	return nosock();
}

vlong
c_recv(int, void*, uvlong, int)
{
	return nosock();
}

vlong
c_recvfrom(int, void*, uvlong, int, void*, void*)
{
	return nosock();
}

int
c_setsockopt(int, int, int, void*, uint)
{
	return nosock();
}

int
c_getsockopt(int, int, int, void*, void*)
{
	return nosock();
}

int
c_getsockname(int, void*, void*)
{
	return nosock();
}

int
c_getpeername(int, void*, void*)
{
	return nosock();
}

int
c_getaddrinfo(char*, char*, void*, void **res)
{
	*res = nil;
	return Eaifail;
}

void
c_freeaddrinfo(void*)
{
}

int
c_getnameinfo(void*, uint, char*, uint, char*, uint, int)
{
	return Eaifail;
}

char*
c_gai_strerror(int)
{
	return "name resolution is not supported";
}

int
c_getifaddrs(void **ifap)
{
	*ifap = nil;
	seterr(Enosys);
	return -1;
}

void
c_freeifaddrs(void*)
{
}

/* glibc struct timeval */
typedef struct Ctimeval Ctimeval;
struct Ctimeval
{
	vlong	sec;
	vlong	usec;
};

int
c_select(int, void *r, void *w, void *e, Ctimeval *tv)
{
	if(r != nil)
		memset(r, 0, 128);	/* fd_set */
	if(w != nil)
		memset(w, 0, 128);
	if(e != nil)
		memset(e, 0, 128);
	if(tv != nil)
		sleep(tv->sec*1000 + tv->usec/1000);
	return 0;
}
