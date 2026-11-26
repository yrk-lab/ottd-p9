/*
 * The POSIX/Linux operating-system calls OpenTTD (and libc++) make, on
 * Plan 9: files with POSIX flags, process, time, user and system
 * information, randomness, sleeping.  Signals, backtraces, dynamic
 * loading and sockets are accepted and fail cleanly for now (sockets:
 * Plan 9 dial/announce later).  Structures are glibc's x86-64 layouts and
 * error numbers Linux's, as the translated code was compiled against.
 * Names that Plan 9 libc also has, with another meaning or ABI, are
 * renamed by lower.py --libc to c_<name>.
 */
#include <u.h>
#include <libc.h>

int	*__errno_location(void);	/* libc_p9.c */
void	c_seterrno(void);
char	*strerror_r(int, char*, uvlong);

enum
{
	/* Linux open(2) flags */
	Oaccmode	= 03,
	Ocreat		= 0100,
	Oexcl		= 0200,
	Otrunc		= 01000,
	Oappend		= 02000,

	/* Linux errno */
	Ebadf		= 9,
	Eexist		= 17,
	Einval		= 22,
	Enotty		= 25,
	Enosys		= 38,
	Eafnosupport	= 97,

	/* getaddrinfo */
	Eaifail		= -4,

	/* sysconf */
	Scclktck	= 2,
	Scpagesize	= 30,
	Scnprocconf	= 83,
	Scnproconln	= 84,
};

static void
seterr(int e)
{
	*__errno_location() = e;
}

/*
 * getuser, sysname and truerand as 9front's libc has them, from the
 * same /dev files, with fallbacks: goken's Linux-hosted libc (the fast
 * test loop) has none of them.
 */
static char*
devfile(char *path, char *env, char *dflt, char *buf, int n)
{
	int fd, r;
	char *e;

	fd = open(path, OREAD);
	if(fd >= 0){
		r = read(fd, buf, n - 1);
		close(fd);
		if(r > 0){
			buf[r] = 0;
			return buf;
		}
	}
	e = getenv(env);
	strecpy(buf, buf + n, e != nil ? e : dflt);
	free(e);
	return buf;
}

static char*
user(void)
{
	static char buf[64];

	return devfile("/dev/user", "user", "none", buf, sizeof buf);
}

static char*
sysnam(void)
{
	static char buf[64];

	return devfile("/dev/sysname", "sysname", "plan9", buf, sizeof buf);
}

static ulong
random32(void)
{
	static int fd = -2;
	ulong r;

	if(fd == -2)
		fd = open("/dev/random", OREAD);
	if(fd >= 0 && read(fd, &r, sizeof r) == sizeof r)
		return r;
	return nsec() * 1103515245 + 12345;
}

/*
 * Files.  Plan 9's read and write return a 32-bit long; these return
 * the LP64 ssize_t.  POSIX open flags are mapped to Plan 9 modes; a new
 * file is made with create.
 */

int
c_open(char *path, int flags, ...)
{
	va_list arg;
	int fd, mode, perm;
	Dir *d;

	perm = 0666;
	if(flags & Ocreat){
		va_start(arg, flags);
		perm = va_arg(arg, int);
		va_end(arg);
	}
	mode = flags & Oaccmode;	/* 0 1 2 are OREAD OWRITE ORDWR */
	if(flags & Otrunc)
		mode |= OTRUNC;
	fd = -1;
	if(flags & Ocreat){
		d = dirstat(path);
		if(d != nil){
			free(d);
			if(flags & Oexcl){
				seterr(Eexist);
				return -1;
			}
			fd = open(path, mode);
		}else
			fd = create(path, mode, perm & 0777);
	}else
		fd = open(path, mode);
	if(fd < 0){
		c_seterrno();
		return -1;
	}
	if(flags & Oappend)
		seek(fd, 0, 2);
	return fd;
}

vlong
c_read(int fd, void *buf, uvlong n)
{
	long r;

	r = read(fd, buf, n);
	if(r < 0)
		c_seterrno();
	return r;
}

vlong
c_write(int fd, void *buf, uvlong n)
{
	long r;

	r = write(fd, buf, n);
	if(r < 0)
		c_seterrno();
	return r;
}

vlong
c_lseek(int fd, vlong off, int whence)
{
	vlong r;

	r = seek(fd, off, whence);	/* SEEK_SET, CUR, END are 0, 1, 2 too */
	if(r < 0)
		c_seterrno();
	return r;
}

/* POSIX dup(fd): the lowest free descriptor */
int
c_dup(int fd)
{
	return dup(fd, -1);
}

int
dup2(int old, int new)
{
	return dup(old, new);
}

int
fdatasync(int)
{
	return 0;
}

int
ioctl(int, uvlong, ...)
{
	seterr(Enotty);
	return -1;
}

int
statvfs(char*, void*)
{
	seterr(Enosys);
	return -1;
}

/* a terminal is the console */
int
c_isatty(int fd)
{
	Dir *d;
	int r;

	d = dirfstat(fd);
	if(d == nil)
		return 0;
	r = strcmp(d->name, "cons") == 0;
	free(d);
	return r;
}

char*
c_strerror(int e)
{
	static char buf[64];

	return strerror_r(e, buf, sizeof buf);
}

/*
 * Process.
 */

void
_exit(int status)
{
	char buf[16];

	if(status == 0)
		_exits(nil);
	snprint(buf, sizeof buf, "%d", status);
	_exits(buf);
}

int
execvp(char *file, char **argv)
{
	char buf[256];

	if(strchr(file, '/') == nil){
		snprint(buf, sizeof buf, "/bin/%s", file);
		file = buf;
	}
	exec(file, argv);
	c_seterrno();
	return -1;
}

/* Plan 9 has no numeric user ids */
uint
getuid(void)
{
	return 0;
}

/* glibc struct passwd */
typedef struct Cpasswd Cpasswd;
struct Cpasswd
{
	char	*name;
	char	*passwd;
	uint	uid;
	uint	gid;
	char	*gecos;
	char	*dir;
	char	*shell;
};

Cpasswd*
getpwuid(uint)
{
	static Cpasswd pw;
	static char home[256];
	char *h;

	h = getenv("home");
	strecpy(home, home + sizeof home, h != nil ? h : "/");
	free(h);
	pw.name = user();
	pw.passwd = "";
	pw.gecos = pw.name;
	pw.dir = home;
	pw.shell = "/bin/rc";
	return &pw;
}

/* glibc struct utsname: six 65-byte strings */
int
uname(char *u)
{
	char *s;

	memset(u, 0, 6*65);
	strecpy(u, u + 65, "Plan 9");
	s = sysnam();
	strecpy(u + 65, u + 2*65, s);
	strecpy(u + 2*65, u + 3*65, "9front");
	strecpy(u + 4*65, u + 5*65, "amd64");
	return 0;
}

vlong
sysconf(int name)
{
	switch(name){
	case Scnprocconf:
	case Scnproconln:
		return 1;
	case Scpagesize:
		return 4096;
	case Scclktck:
		return 100;
	}
	seterr(Einval);
	return -1;
}

vlong
getrandom(void *buf, uvlong n, uint)
{
	uchar *p;
	ulong r;
	uvlong i;

	p = buf;
	for(i = 0; i < n; i++){
		if(i % 4 == 0)
			r = random32();
		p[i] = r;
		r >>= 8;
	}
	return n;
}

/*
 * Time.  Plan 9's time() returns a 32-bit long: this one is the LP64
 * time_t.
 */

typedef struct Ctimespec Ctimespec;
struct Ctimespec
{
	vlong	sec;
	vlong	nsec;
};

vlong
c_time(vlong *t)
{
	vlong now;

	now = nsec() / 1000000000LL;
	if(t != nil)
		*t = now;
	return now;
}

int
nanosleep(Ctimespec *req, Ctimespec *rem)
{
	sleep(req->sec*1000 + req->nsec/1000000);
	if(rem != nil){
		rem->sec = 0;
		rem->nsec = 0;
	}
	return 0;
}

/* glibc struct tm (libc_p9.c has the same) */
typedef struct Ctm Ctm;
struct Ctm
{
	int	sec;
	int	min;
	int	hour;
	int	mday;
	int	mon;
	int	year;
	int	wday;
	int	yday;
	int	isdst;
	int	pad;
	vlong	gmtoff;
	char	*zone;
};

static Ctm*
fromtm(Tm *t, Ctm *c)
{
	static char zone[16];

	c->sec = t->sec;
	c->min = t->min;
	c->hour = t->hour;
	c->mday = t->mday;
	c->mon = t->mon;
	c->year = t->year;
	c->wday = t->wday;
	c->yday = t->yday;
	c->isdst = 0;
	c->gmtoff = t->tzoff;
	strecpy(zone, zone + sizeof zone, t->zone);
	c->zone = zone;
	return c;
}

Ctm*
gmtime_r(vlong *t, Ctm *c)
{
	return fromtm(gmtime(*t), c);
}

Ctm*
localtime_r(vlong *t, Ctm *c)
{
	return fromtm(localtime(*t), c);
}

/*
 * Signals: accepted and ignored for now (notes later).
 */

void*
signal(int, void*)
{
	return nil;	/* SIG_DFL */
}

int
sigaction(int, void*, void *old)
{
	if(old != nil)
		memset(old, 0, 152);	/* glibc struct sigaction */
	return 0;
}

int
sigprocmask(int, void*, void *old)
{
	if(old != nil)
		memset(old, 0, 128);	/* sigset_t */
	return 0;
}

int
sigemptyset(void *set)
{
	memset(set, 0, 128);
	return 0;
}

int
sigaddset(void*, int)
{
	return 0;
}

char*
strsignal(int sig)
{
	static char buf[32];

	snprint(buf, sizeof buf, "Signal %d", sig);
	return buf;
}

/*
 * No backtraces, no dynamic loading.
 */

int
backtrace(void**, int)
{
	return 0;
}

char**
backtrace_symbols(void**, int)
{
	return nil;
}

void*
dlopen(char*, int)
{
	return nil;
}

char*
dlerror(void)
{
	return "dynamic loading is not supported";
}

void*
dlsym(void*, char*)
{
	return nil;
}

int
dlclose(void*)
{
	return 0;
}

/*
 * Sockets: none yet.  OpenTTD's network code treats these failures as
 * "no network".  select only waits.
 */

int
socket(int, int, int)
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

/* bind, listen and accept are Plan 9 libc names too: lower.py renames them */
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
connect(int, void*, uint)
{
	return nosock();
}

vlong
send(int, void*, uvlong, int)
{
	return nosock();
}

vlong
sendto(int, void*, uvlong, int, void*, uint)
{
	return nosock();
}

vlong
recv(int, void*, uvlong, int)
{
	return nosock();
}

vlong
recvfrom(int, void*, uvlong, int, void*, void*)
{
	return nosock();
}

int
setsockopt(int, int, int, void*, uint)
{
	return nosock();
}

int
getsockopt(int, int, int, void*, void*)
{
	return nosock();
}

int
getsockname(int, void*, void*)
{
	return nosock();
}

int
getpeername(int, void*, void*)
{
	return nosock();
}

int
getaddrinfo(char*, char*, void*, void **res)
{
	*res = nil;
	return Eaifail;
}

void
freeaddrinfo(void*)
{
}

int
getnameinfo(void*, uint, char*, uint, char*, uint, int)
{
	return Eaifail;
}

char*
gai_strerror(int)
{
	return "name resolution is not supported";
}

int
getifaddrs(void **ifap)
{
	*ifap = nil;
	seterr(Enosys);
	return -1;
}

void
freeifaddrs(void*)
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
select(int, void *r, void *w, void *e, Ctimeval *tv)
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
