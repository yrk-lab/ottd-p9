/*
 * ISO C library calls for translated C programs, on top of Plan 9 libc.
 *
 * The programs were compiled by clang for x86_64 Linux, so they expect
 * the LP64 C ABI: long and size_t are 64 bits.  On Plan 9 amd64 long is
 * 32 bits, and there is no printf or FILE.  lower.py --libc renames the
 * affected names (its RENAMED list) to c_name, and this file provides
 * them, using only calls found in both 9front's and goken's libc.
 *
 * printf is implemented here instead of being translated to print(2):
 * Plan 9's %x is signed, and 'u' is a flag in 9front but a verb in goken,
 * so integer conversions are done by hand.  Floating point is passed to
 * print with the same flags, width and precision.
 */
#include <u.h>
#include <libc.h>

typedef struct CFile CFile;
typedef struct Spec Spec;

enum
{
	Bufsize	= 8192,
	Nbuf	= 512,		/* conversion buffer for one number */
};

struct CFile
{
	int	fd;
	int	wr;		/* opened for writing */
	int	eof;
	int	n;		/* bytes in buf */
	int	p;		/* read position in buf */
	uchar	buf[Bufsize];
	char	*mem;		/* snprintf: write here instead, at most memsize-1 bytes */
	long	memsize;
	long	memlen;
};

/* printf conversion flags */
enum
{
	Fleft	= 1<<0,		/* - */
	Fsign	= 1<<1,		/* + */
	Fspace	= 1<<2,		/* space */
	Fsharp	= 1<<3,		/* # */
	Fzero	= 1<<4,		/* 0 */
};

/* printf argument sizes */
enum
{
	Sint,
	Sshort,		/* h */
	Schar,		/* hh */
	Svlong,		/* l ll z j t q: 64 bits under LP64 */
};

struct Spec
{
	int	flags;
	int	width;
	int	prec;		/* -1 if none */
	int	size;
	int	verb;
};

static CFile	cin = { 0, 0 };
static CFile	cout = { 1, 1 };
static CFile	cerr = { 2, 1 };

CFile	*c_stdin = &cin;
CFile	*c_stdout = &cout;
CFile	*c_stderr = &cerr;

static int
cflush(CFile *f)
{
	int n;

	if(!f->wr || f->n == 0)
		return 0;
	n = f->n;
	f->n = 0;
	if(write(f->fd, f->buf, n) != n)
		return -1;
	return 0;
}

/*
 * stderr is unbuffered, and flushes stdout first so that
 * interleaved output keeps its order.
 */
static int
cwrite(CFile *f, void *v, long n)
{
	uchar *s;
	long m;

	s = v;
	if(f->mem != nil){
		m = f->memsize - 1 - f->memlen;
		if(m > n)
			m = n;
		if(m > 0){
			memmove(f->mem + f->memlen, s, m);
			f->memlen += m;
		}
		return 0;
	}
	if(f == &cerr){
		cflush(&cout);
		if(write(2, s, n) != n)
			return -1;
		return 0;
	}
	while(n > 0){
		m = Bufsize - f->n;
		if(m > n)
			m = n;
		memmove(f->buf + f->n, s, m);
		f->n += m;
		s += m;
		n -= m;
		if(f->n == Bufsize && cflush(f) < 0)
			return -1;
	}
	return 0;
}

static int
cgetc(CFile *f)
{
	if(f->p >= f->n){
		if(f->eof)
			return -1;
		if(f == &cin)
			cflush(&cout);
		f->n = read(f->fd, f->buf, Bufsize);
		f->p = 0;
		if(f->n <= 0){
			f->n = 0;
			f->eof = 1;
			return -1;
		}
	}
	return f->buf[f->p++];
}

/*
 * printf
 */

static void
pad(CFile *f, int c, int n)
{
	char b[64];
	int m;

	memset(b, c, sizeof b);
	while(n > 0){
		m = n;
		if(m > sizeof b)
			m = sizeof b;
		cwrite(f, b, m);
		n -= m;
	}
}

/* write prefix and body, padded to sp->width; return the length written */
static int
field(CFile *f, Spec *sp, char *pre, char *s, int len, int zeropad)
{
	int npre, n;

	npre = strlen(pre);
	n = npre + len;
	if(!(sp->flags & Fleft) && !zeropad)
		pad(f, ' ', sp->width - n);
	cwrite(f, pre, npre);
	if(!(sp->flags & Fleft) && zeropad)
		pad(f, '0', sp->width - n);
	cwrite(f, s, len);
	if(sp->flags & Fleft)
		pad(f, ' ', sp->width - n);
	if(n < sp->width)
		return sp->width;
	return n;
}

/* parse the conversion after a '%'; return the character after it */
static char*
parsespec(char *fmt, Spec *sp, va_list *arg)
{
	sp->flags = 0;
	for(;; fmt++){
		switch(*fmt){
		case '-':
			sp->flags |= Fleft;
			continue;
		case '+':
			sp->flags |= Fsign;
			continue;
		case ' ':
			sp->flags |= Fspace;
			continue;
		case '#':
			sp->flags |= Fsharp;
			continue;
		case '0':
			sp->flags |= Fzero;
			continue;
		}
		break;
	}

	sp->width = 0;
	if(*fmt == '*'){
		fmt++;
		sp->width = va_arg(*arg, int);
		if(sp->width < 0){
			sp->flags |= Fleft;
			sp->width = -sp->width;
		}
	}else
		while(*fmt >= '0' && *fmt <= '9')
			sp->width = sp->width*10 + *fmt++ - '0';

	sp->prec = -1;
	if(*fmt == '.'){
		fmt++;
		sp->prec = 0;
		if(*fmt == '*'){
			fmt++;
			sp->prec = va_arg(*arg, int);
			if(sp->prec < 0)
				sp->prec = -1;
		}else
			while(*fmt >= '0' && *fmt <= '9')
				sp->prec = sp->prec*10 + *fmt++ - '0';
	}

	sp->size = Sint;
	for(;; fmt++){
		switch(*fmt){
		case 'h':
			if(sp->size == Sshort)
				sp->size = Schar;
			else
				sp->size = Sshort;
			continue;
		case 'l':
		case 'z':
		case 'j':
		case 't':
		case 'q':
			sp->size = Svlong;
			continue;
		case 'L':	/* long double: treated as double */
			continue;
		}
		break;
	}
	sp->verb = *fmt;
	if(*fmt != 0)
		fmt++;
	return fmt;
}

static int
fmtint(CFile *f, Spec *sp, char *pre, uvlong u, int base)
{
	char buf[Nbuf], *s, *e, *digits;

	digits = "0123456789abcdef";
	if(sp->verb == 'X')
		digits = "0123456789ABCDEF";
	e = buf + sizeof buf;
	s = e;
	while(u != 0){
		*--s = digits[u % base];
		u /= base;
	}
	if(sp->prec < 0 && s == e)
		*--s = '0';
	while(e - s < sp->prec)
		*--s = '0';
	if((sp->flags & Fsharp) && base == 8 && *s != '0')
		*--s = '0';
	return field(f, sp, pre, s, e - s, (sp->flags & Fzero) && sp->prec < 0);
}

static int
fmtsigned(CFile *f, Spec *sp, va_list *arg)
{
	vlong v;
	char *pre;

	if(sp->size == Svlong)
		v = va_arg(*arg, vlong);
	else
		v = va_arg(*arg, int);
	if(sp->size == Sshort)
		v = (short)v;
	else if(sp->size == Schar)
		v = (schar)v;

	pre = "";
	if(v < 0)
		pre = "-";
	else if(sp->flags & Fsign)
		pre = "+";
	else if(sp->flags & Fspace)
		pre = " ";
	if(v < 0)
		return fmtint(f, sp, pre, -(uvlong)v, 10);
	return fmtint(f, sp, pre, v, 10);
}

static int
fmtunsigned(CFile *f, Spec *sp, va_list *arg)
{
	uvlong u;
	char *pre;
	int base;

	if(sp->size == Svlong)
		u = va_arg(*arg, uvlong);
	else
		u = va_arg(*arg, uint);
	if(sp->size == Sshort)
		u = (ushort)u;
	else if(sp->size == Schar)
		u = (uchar)u;

	pre = "";
	switch(sp->verb){
	case 'o':
		base = 8;
		break;
	case 'u':
		base = 10;
		break;
	default:
		base = 16;
		if((sp->flags & Fsharp) && u != 0)
			pre = sp->verb == 'X' ? "0X" : "0x";
		break;
	}
	return fmtint(f, sp, pre, u, base);
}

/* glibc style: 0x... or (nil) */
static int
fmtptr(CFile *f, Spec *sp, va_list *arg)
{
	uvlong u;

	u = (uvlong)va_arg(*arg, void*);
	if(u == 0)
		return field(f, sp, "", "(nil)", 5, 0);
	sp->verb = 'x';
	sp->prec = -1;
	return fmtint(f, sp, "0x", u, 16);
}

static int
fmtstr(CFile *f, Spec *sp, va_list *arg)
{
	char *s;
	int n;

	s = va_arg(*arg, char*);
	if(s == nil)
		s = "(null)";
	for(n = 0; s[n] != 0; n++)
		if(sp->prec >= 0 && n >= sp->prec)
			break;
	return field(f, sp, "", s, n, 0);
}

static int
fmtchar(CFile *f, Spec *sp, va_list *arg)
{
	char c;

	c = va_arg(*arg, int);
	return field(f, sp, "", &c, 1, 0);
}

/* floating point: rebuild the conversion and let print(2) do it */
static int
fmtflt(CFile *f, Spec *sp, va_list *arg)
{
	char pf[32], buf[Nbuf], *p, *e;
	double d;
	int n;

	d = va_arg(*arg, double);
	p = pf;
	e = pf + sizeof pf;
	*p++ = '%';
	if(sp->flags & Fleft)
		*p++ = '-';
	if(sp->flags & Fsign)
		*p++ = '+';
	if(sp->flags & Fspace)
		*p++ = ' ';
	if(sp->flags & Fsharp)
		*p++ = '#';
	if(sp->flags & Fzero)
		*p++ = '0';
	*p = 0;
	if(sp->width > 0)
		p = seprint(p, e, "%d", sp->width);
	if(sp->prec >= 0)
		p = seprint(p, e, ".%d", sp->prec);
	if(sp->verb == 'F')
		*p++ = 'f';
	else
		*p++ = sp->verb;
	*p = 0;

	n = snprint(buf, sizeof buf, pf, d);
	cwrite(f, buf, n);
	return n;
}

static int
cvfprintf(CFile *f, char *fmt, va_list arg)
{
	Spec spec;
	char *s;
	int total;

	total = 0;
	for(;;){
		for(s = fmt; *s != 0 && *s != '%'; s++)
			;
		if(s > fmt){
			cwrite(f, fmt, s - fmt);
			total += s - fmt;
		}
		if(*s == 0)
			break;
		fmt = parsespec(s + 1, &spec, &arg);
		switch(spec.verb){
		case 0:
			return total;
		case 'd':
		case 'i':
			total += fmtsigned(f, &spec, &arg);
			break;
		case 'u':
		case 'o':
		case 'x':
		case 'X':
			total += fmtunsigned(f, &spec, &arg);
			break;
		case 'p':
			total += fmtptr(f, &spec, &arg);
			break;
		case 's':
			total += fmtstr(f, &spec, &arg);
			break;
		case 'c':
			total += fmtchar(f, &spec, &arg);
			break;
		case 'f':
		case 'F':
		case 'e':
		case 'E':
		case 'g':
		case 'G':
			total += fmtflt(f, &spec, &arg);
			break;
		case 'n':
			*va_arg(arg, int*) = total;
			break;
		default:	/* %% and unknown verbs */
			cwrite(f, "%", 1);
			total++;
			break;
		}
	}
	return total;
}

int
c_printf(char *fmt, ...)
{
	va_list arg;
	int n;

	va_start(arg, fmt);
	n = cvfprintf(c_stdout, fmt, arg);
	va_end(arg);
	return n;
}

int
c_fprintf(CFile *f, char *fmt, ...)
{
	va_list arg;
	int n;

	va_start(arg, fmt);
	n = cvfprintf(f, fmt, arg);
	va_end(arg);
	return n;
}

/*
 * The v*printf functions are called by translated code, where a va_list
 * is the x86-64 ABI's: a pointer to a 24-byte __va_list_tag.  Translated
 * va_start (shim/stdarg.h) keeps the Plan 9 va_list in its first word.
 */

int
c_vprintf(char *fmt, void *ap)
{
	return cvfprintf(c_stdout, fmt, *(va_list*)ap);
}

int
c_vfprintf(CFile *f, char *fmt, void *ap)
{
	return cvfprintf(f, fmt, *(va_list*)ap);
}

static int
cvsnprintf(char *buf, uvlong size, char *fmt, va_list arg)
{
	CFile f;
	int n;

	f.fd = -1;
	f.wr = 1;
	f.n = 0;
	f.mem = buf;
	f.memsize = size;
	f.memlen = 0;
	n = cvfprintf(&f, fmt, arg);
	if(size > 0)
		buf[f.memlen] = 0;
	return n;
}

int
c_vsnprintf(char *buf, uvlong size, char *fmt, void *ap)
{
	return cvsnprintf(buf, size, fmt, *(va_list*)ap);
}

int
c_snprintf(char *buf, uvlong size, char *fmt, ...)
{
	va_list arg;
	int n;

	va_start(arg, fmt);
	n = cvsnprintf(buf, size, fmt, arg);
	va_end(arg);
	return n;
}

/*
 * stdio
 */

int
c_putc(int c, CFile *f)
{
	uchar b;

	b = c;
	cwrite(f, &b, 1);
	return b;
}

int
c_fputc(int c, CFile *f)
{
	return c_putc(c, f);
}

int
c_putchar(int c)
{
	return c_putc(c, c_stdout);
}

int
c_fputs(char *s, CFile *f)
{
	cwrite(f, s, strlen(s));
	return 0;
}

int
c_puts(char *s)
{
	c_fputs(s, c_stdout);
	return c_putc('\n', c_stdout);
}

uvlong
c_fwrite(void *p, uvlong size, uvlong n, CFile *f)
{
	if(cwrite(f, p, size*n) < 0)
		return 0;
	return n;
}

int
c_fflush(CFile *f)
{
	if(f == nil)
		f = c_stdout;
	return cflush(f);
}

CFile*
c_fopen(char *path, char *mode)
{
	CFile *f;
	int fd;

	switch(mode[0]){
	case 'r':
		fd = open(path, OREAD);
		break;
	case 'w':
		fd = create(path, OWRITE, 0666);
		break;
	default:	/* 'a' */
		fd = open(path, OWRITE);
		if(fd >= 0)
			seek(fd, 0, 2);
		break;
	}
	if(fd < 0)
		return nil;
	f = mallocz(sizeof *f, 1);
	if(f == nil){
		close(fd);
		return nil;
	}
	f->fd = fd;
	f->wr = mode[0] != 'r';
	return f;
}

int
c_fclose(CFile *f)
{
	int r;

	r = cflush(f);
	close(f->fd);
	if(f != &cin && f != &cout && f != &cerr)
		free(f);
	return r;
}

int
c_getc(CFile *f)
{
	return cgetc(f);
}

int
c_fgetc(CFile *f)
{
	return cgetc(f);
}

int
c_getchar(void)
{
	return cgetc(c_stdin);
}

char*
c_fgets(char *s, int n, CFile *f)
{
	int i, c;

	for(i = 0; i < n-1; ){
		c = cgetc(f);
		if(c < 0)
			break;
		s[i++] = c;
		if(c == '\n')
			break;
	}
	if(i == 0)
		return nil;
	s[i] = 0;
	return s;
}

void
c_perror(char *s)
{
	cflush(&cout);
	if(s != nil && *s != 0)
		fprint(2, "%s: %r\n", s);
	else
		fprint(2, "%r\n");
}

/*
 * errno.  Plan 9 has none; glibc's <errno.h> makes errno *__errno_location(),
 * and translated code compares it with Linux's values.
 */

enum
{
	Erange	= 34,	/* Linux ERANGE */
};

static int	cerrno;

int*
__errno_location(void)
{
	return &cerrno;
}

/*
 * strtol and friends.  Plan 9's strtol returns a 32-bit long, and none of
 * them report overflow through errno, which libc++ (std::stoi) relies on.
 * glibc 2.38 headers redirect them to __isoc23_ names in C23 mode.
 */

static int
digit(int c, int base)
{
	int v;

	if(c >= '0' && c <= '9')
		v = c - '0';
	else if(c >= 'a' && c <= 'z')
		v = c - 'a' + 10;
	else if(c >= 'A' && c <= 'Z')
		v = c - 'A' + 10;
	else
		return -1;
	if(v >= base)
		return -1;
	return v;
}

/*
 * Convert s in base (0: by prefix) to an unsigned magnitude, at most max;
 * set *neg.  On overflow return max and set errno.  *e is the end, or s
 * if there are no digits.
 */
static uvlong
cstrtou(char *s, char **e, int base, uvlong max, int *neg)
{
	char *p, *start;
	uvlong n;
	int v, over;

	p = s;
	while(*p == ' ' || (*p >= '\t' && *p <= '\r'))
		p++;
	*neg = 0;
	if(*p == '-' || *p == '+')
		*neg = *p++ == '-';
	if((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && digit(p[2], 16) >= 0){
		p += 2;
		base = 16;
	}else if(base == 0)
		base = *p == '0' ? 8 : 10;

	start = p;
	n = 0;
	over = 0;
	for(; (v = digit(*p, base)) >= 0; p++){
		if(n > (max - v) / base)
			over = 1;
		else
			n = n*base + v;
	}
	if(e != nil)
		*e = p == start ? s : p;
	if(over){
		cerrno = Erange;
		return max;
	}
	return n;
}

static vlong
cstrtoll(char *s, char **e, int base)
{
	uvlong n, max;
	int neg;

	max = (uvlong)1 << 63;	/* the magnitude of the most negative value */
	n = cstrtou(s, e, base, max, &neg);
	if(!neg && n == max){
		cerrno = Erange;
		return max - 1;
	}
	if(neg)
		return -n;
	return n;
}

static uvlong
cstrtoull(char *s, char **e, int base)
{
	uvlong n;
	int neg;

	n = cstrtou(s, e, base, ~(uvlong)0, &neg);
	if(neg)
		return -n;
	return n;
}

vlong
c_strtol(char *s, char **e, int base)
{
	return cstrtoll(s, e, base);
}

uvlong
c_strtoul(char *s, char **e, int base)
{
	return cstrtoull(s, e, base);
}

vlong
c_atol(char *s)
{
	return cstrtoll(s, nil, 10);
}

vlong
__isoc23_strtol(char *s, char **e, int base)
{
	return cstrtoll(s, e, base);
}

vlong
__isoc23_strtoll(char *s, char **e, int base)
{
	return cstrtoll(s, e, base);
}

uvlong
__isoc23_strtoul(char *s, char **e, int base)
{
	return cstrtoull(s, e, base);
}

uvlong
__isoc23_strtoull(char *s, char **e, int base)
{
	return cstrtoull(s, e, base);
}

vlong
c_labs(vlong v)
{
	if(v < 0)
		return -v;
	return v;
}

uvlong
c_strlen(char *s)
{
	return strlen(s);
}

/*
 * POSIX file system calls used by libc++'s std::filesystem, on Plan 9's
 * dirstat, dirread, dirwstat, create and remove.  The structures are
 * glibc's x86-64 layouts and errno values are Linux's, since that is
 * what the translated code was compiled against.  Plan 9 has no
 * symbolic links: lstat is stat.
 */

enum
{
	Eio		= 5,
	Enoent		= 2,
	Eacces		= 13,
	Eexist		= 17,
	Exdev		= 18,
	Enotdir		= 20,
	Eisdir		= 21,
	Einval		= 22,
	Enotempty	= 39,

	Sifdir		= 0040000,
	Sifreg		= 0100000,
	Dtdir		= 4,
	Dtreg		= 8,

	Pathmax		= 4096,
};

typedef struct Ctimespec Ctimespec;
typedef struct Cstat Cstat;
typedef struct Cdirent Cdirent;
typedef struct Cdir Cdir;

struct Ctimespec
{
	vlong	sec;
	vlong	nsec;
};

/* glibc struct stat, x86-64: 144 bytes */
struct Cstat
{
	uvlong	dev;
	uvlong	ino;
	uvlong	nlink;
	uint	mode;
	uint	uid;
	uint	gid;
	int	pad0;
	uvlong	rdev;
	vlong	size;
	vlong	blksize;
	vlong	blocks;
	Ctimespec	atim;
	Ctimespec	mtim;
	Ctimespec	ctim;
	vlong	reserved[3];
};

/* glibc struct dirent, x86-64: d_name at offset 19 */
#pragma pack on
struct Cdirent
{
	uvlong	ino;
	vlong	off;
	ushort	reclen;
	uchar	type;
	char	name[256];
	uchar	pad[5];
};
#pragma pack off

/* DIR */
struct Cdir
{
	int	fd;
	Dir	*ents;
	long	n;
	long	i;
	Cdirent	ent;
};

/*
 * Set errno from the Plan 9 error string (9front's, or the host's under
 * goken).  goken's libc leaves it empty when a file does not exist.
 */
static void
seterrno(void)
{
	char e[ERRMAX];

	rerrstr(e, sizeof e);
	if(e[0] == 0 || strstr(e, "not exist") != nil || strstr(e, "No such file") != nil
	|| strstr(e, "not found") != nil)
		cerrno = Enoent;
	else if(strstr(e, "not empty") != nil)
		cerrno = Enotempty;
	else if(strstr(e, "exist") != nil || strstr(e, "File exists") != nil)
		cerrno = Eexist;
	else if(strstr(e, "not a directory") != nil || strstr(e, "Not a directory") != nil)
		cerrno = Enotdir;
	else if(strstr(e, "is a directory") != nil || strstr(e, "Is a directory") != nil)
		cerrno = Eisdir;
	else if(strstr(e, "permission denied") != nil || strstr(e, "Permission denied") != nil)
		cerrno = Eacces;
	else
		cerrno = Eio;
}

static void
dirtostat(Dir *d, Cstat *st)
{
	memset(st, 0, sizeof *st);
	st->ino = d->qid.path;
	st->nlink = 1;
	st->mode = d->mode & 0777;
	if(d->mode & DMDIR)
		st->mode |= Sifdir;
	else
		st->mode |= Sifreg;
	st->size = d->length;
	st->blksize = 8192;
	st->blocks = (d->length + 511) / 512;
	st->atim.sec = d->atime;
	st->mtim.sec = d->mtime;
	st->ctim.sec = d->mtime;
}

int
c_stat(char *path, Cstat *st)
{
	Dir *d;

	d = dirstat(path);
	if(d == nil){
		seterrno();
		return -1;
	}
	dirtostat(d, st);
	free(d);
	return 0;
}

int
c_lstat(char *path, Cstat *st)
{
	return c_stat(path, st);
}

int
c_fstat(int fd, Cstat *st)
{
	Dir *d;

	d = dirfstat(fd);
	if(d == nil){
		seterrno();
		return -1;
	}
	dirtostat(d, st);
	free(d);
	return 0;
}

int
c_mkdir(char *path, uint mode)
{
	Dir *d;
	int fd;

	d = dirstat(path);
	if(d != nil){
		free(d);
		cerrno = Eexist;
		return -1;
	}
	fd = create(path, OREAD, DMDIR | (mode & 0777));
	if(fd < 0){
		seterrno();
		return -1;
	}
	close(fd);
	return 0;
}

/*
 * After a failed remove the file's state says more than the error
 * string (which goken leaves empty): gone, a directory (so not empty),
 * or something else.
 */
int
c_remove(char *path)
{
	Dir *d;

	if(remove(path) == 0)
		return 0;
	d = dirstat(path);
	if(d == nil)
		cerrno = Enoent;
	else{
		if(d->mode & DMDIR)
			cerrno = Enotempty;
		else
			seterrno();
		free(d);
	}
	return -1;
}

/* split a path into a cleaned directory and the last element */
static void
splitpath(char *path, char *dir, char *base)
{
	char *s;

	strecpy(dir, dir + Pathmax, path);
	cleanname(dir);
	s = strrchr(dir, '/');
	if(s == nil){
		strecpy(base, base + Pathmax, dir);
		strcpy(dir, ".");
	}else{
		strecpy(base, base + Pathmax, s + 1);
		if(s == dir)
			s[1] = 0;
		else
			*s = 0;
	}
}

/*
 * Plan 9 renames by changing the name in place (wstat), so only within
 * a directory, and not over an existing file, which POSIX replaces.
 */
int
c_rename(char *old, char *new)
{
	char odir[Pathmax], obase[Pathmax], ndir[Pathmax], nbase[Pathmax];
	Dir d, *e;

	splitpath(old, odir, obase);
	splitpath(new, ndir, nbase);
	if(strcmp(odir, ndir) != 0){
		cerrno = Exdev;
		return -1;
	}
	if(strcmp(obase, nbase) == 0)
		return 0;
	e = dirstat(new);
	if(e != nil){
		free(e);
		if(remove(new) < 0){
			seterrno();
			return -1;
		}
	}
	nulldir(&d);
	d.name = nbase;
	if(dirwstat(old, &d) < 0){
		seterrno();
		return -1;
	}
	return 0;
}

char*
c_getcwd(char *buf, uvlong size)
{
	if(buf == nil){
		size = Pathmax;
		buf = malloc(size);
		if(buf == nil)
			return nil;
	}
	if(getwd(buf, size) == nil){
		seterrno();
		return nil;
	}
	return buf;
}

/* no symbolic links: the clean absolute name of an existing file */
char*
c_realpath(char *path, char *resolved)
{
	char buf[Pathmax];
	Dir *d;

	if(path[0] == '/')
		strecpy(buf, buf + sizeof buf, path);
	else{
		if(getwd(buf, sizeof buf) == nil){
			seterrno();
			return nil;
		}
		strecpy(buf + strlen(buf), buf + sizeof buf, "/");
		strecpy(buf + strlen(buf), buf + sizeof buf, path);
	}
	cleanname(buf);
	d = dirstat(buf);
	if(d == nil){
		seterrno();
		return nil;
	}
	free(d);
	if(resolved == nil){
		resolved = malloc(Pathmax);
		if(resolved == nil)
			return nil;
	}
	strecpy(resolved, resolved + Pathmax, buf);
	return resolved;
}

Cdir*
c_opendir(char *path)
{
	Cdir *dp;
	Dir *d;
	int fd;

	d = dirstat(path);
	if(d == nil){
		seterrno();
		return nil;
	}
	if((d->mode & DMDIR) == 0){
		free(d);
		cerrno = Enotdir;
		return nil;
	}
	free(d);
	fd = open(path, OREAD);
	if(fd < 0){
		seterrno();
		return nil;
	}
	dp = mallocz(sizeof *dp, 1);
	if(dp == nil){
		close(fd);
		return nil;
	}
	dp->fd = fd;
	return dp;
}

/* end of directory: nil with errno unchanged */
Cdirent*
c_readdir(Cdir *dp)
{
	Dir *d;

	if(dp->i >= dp->n){
		free(dp->ents);
		dp->ents = nil;
		dp->i = 0;
		dp->n = dirread(dp->fd, &dp->ents);
		if(dp->n <= 0){
			dp->n = 0;
			return nil;
		}
	}
	d = &dp->ents[dp->i++];
	memset(&dp->ent, 0, sizeof dp->ent);
	dp->ent.ino = d->qid.path;
	dp->ent.off = dp->i;
	dp->ent.reclen = sizeof dp->ent;
	dp->ent.type = (d->mode & DMDIR) ? Dtdir : Dtreg;
	strecpy(dp->ent.name, dp->ent.name + sizeof dp->ent.name, d->name);
	return &dp->ent;
}

int
c_closedir(Cdir *dp)
{
	close(dp->fd);
	free(dp->ents);
	free(dp);
	return 0;
}

/* GNU strerror_r (what glibc's headers select for C++): glibc's texts */
char*
strerror_r(int e, char *buf, uvlong size)
{
	char *s;

	switch(e){
	case 0:		s = "Success"; break;
	case Enoent:	s = "No such file or directory"; break;
	case Eio:	s = "Input/output error"; break;
	case Eacces:	s = "Permission denied"; break;
	case Eexist:	s = "File exists"; break;
	case Exdev:	s = "Invalid cross-device link"; break;
	case Enotdir:	s = "Not a directory"; break;
	case Eisdir:	s = "Is a directory"; break;
	case Einval:	s = "Invalid argument"; break;
	case Erange:	s = "Numerical result out of range"; break;
	case Enotempty:	s = "Directory not empty"; break;
	default:
		snprint(buf, size, "Unknown error %d", e);
		return buf;
	}
	return s;
}

/* clock_gettime: every clock is the time of day */
int
clock_gettime(int, Ctimespec *ts)
{
	vlong ns;

	ns = nsec();
	ts->sec = ns / 1000000000LL;
	ts->nsec = ns % 1000000000LL;
	return 0;
}

/*
 * Math functions not in Plan 9 libc.  The float versions go through double.
 */

double
exp2(double x)
{
	return pow(2.0, x);
}

float
sinf(float x)
{
	return sin(x);
}

float
cosf(float x)
{
	return cos(x);
}

float
sqrtf(float x)
{
	return sqrt(x);
}

float
fabsf(float x)
{
	return fabs(x);
}

float
floorf(float x)
{
	return floor(x);
}

float
ceilf(float x)
{
	return ceil(x);
}

/* LLVM rewrites memcmp(...) == 0 as bcmp */
int
bcmp(void *a, void *b, uvlong n)
{
	return memcmp(a, b, n);
}

/*
 * glibc's <ctype.h> makes toupper and tolower inline lookups
 * in tables indexed from -128 to 255.
 */

static int	uppertab[384];
static int	lowertab[384];
static int	*upperp;
static int	*lowerp;

static void
ctypeinit(void)
{
	int c, u, l;

	for(c = -128; c < 256; c++){
		u = c;
		l = c;
		if(c >= 'a' && c <= 'z')
			u = c - 'a' + 'A';
		if(c >= 'A' && c <= 'Z')
			l = c - 'A' + 'a';
		uppertab[c+128] = u;
		lowertab[c+128] = l;
	}
	upperp = uppertab + 128;
	lowerp = lowertab + 128;
}

int**
__ctype_toupper_loc(void)
{
	if(upperp == nil)
		ctypeinit();
	return &upperp;
}

int**
__ctype_tolower_loc(void)
{
	if(lowerp == nil)
		ctypeinit();
	return &lowerp;
}

/*
 * Process exit: flush stdout and turn the C status into an exits(2) string.
 * The program's main is renamed c_main, so its return value becomes the status.
 */

void
c_exit(int status)
{
	char buf[16];

	cflush(&cout);
	if(status == 0)
		exits(nil);
	snprint(buf, sizeof buf, "%d", status);
	exits(buf);
}

int	c_main(int, char**);
void	__p9_ctors(void);	/* static constructors, emitted by lower.py */

void
main(int argc, char **argv)
{
	__p9_ctors();
	c_exit(c_main(argc, argv));
}
