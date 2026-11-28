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

/*
 * With optimization, glibc's headers inline putc_unlocked and friends,
 * so translated code reads a FILE's _IO_write_ptr and _IO_write_end
 * directly and calls __overflow only when they meet.  A CFile starts with
 * the 216 bytes of a glibc FILE, all zero: the inline code always takes
 * the out-of-line path, which is ours.
 */
struct CFile
{
	uchar	glibc[216];
	int	fd;
	int	wr;		/* opened for writing */
	int	eof;
	int	n;		/* bytes in buf */
	int	p;		/* read position in buf */
	int	err;		/* ferror */
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

static void	seterrno(void);
void	c_seterrno(void);

static CFile	cin = { { 0 }, 0, 0 };
static CFile	cout = { { 0 }, 1, 1 };
static CFile	cerr = { { 0 }, 2, 1 };

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
	if(write(f->fd, f->buf, n) != n){
		f->err = 1;
		return -1;
	}
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

/* stdin's reads, where a reader process has it (cons9f.c); nil: read(2) */
long	(*c_stdinread)(void*, long);

static int
cgetc(CFile *f)
{
	if(f->p >= f->n){
		if(f->eof)
			return -1;
		if(f == &cin)
			cflush(&cout);
		if(f == &cin && c_stdinread != nil)
			f->n = c_stdinread(f->buf, Bufsize);
		else
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
	char dummy;
	int n;

	if(buf == nil)
		buf = &dummy;	/* counting only (size 0): nothing is written */
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
 * More stdio: binary reads, positioning, status, pushback, and glibc's
 * out-of-line halves of its inline macros.
 */

uvlong
c_fread(void *v, uvlong size, uvlong n, CFile *f)
{
	uchar *p;
	uvlong want, got;
	int c;

	p = v;
	want = size * n;
	got = 0;
	while(got < want){
		if(f->p < f->n){
			c = f->n - f->p;
			if(c > want - got)
				c = want - got;
			memmove(p + got, f->buf + f->p, c);
			f->p += c;
			got += c;
			continue;
		}
		c = cgetc(f);
		if(c < 0)
			break;
		p[got++] = c;
	}
	if(size == 0)
		return 0;
	return got / size;
}

/* the position the caller sees: the file's, less what is buffered unread */
vlong
c_ftello(CFile *f)
{
	vlong pos;

	pos = seek(f->fd, 0, 1);
	if(pos < 0)
		return -1;
	if(f->wr)
		return pos + f->n;
	return pos - (f->n - f->p);
}

vlong
c_ftell(CFile *f)
{
	return c_ftello(f);
}

int
c_fseeko(CFile *f, vlong off, int whence)
{
	if(f->wr && cflush(f) < 0)
		return -1;
	if(!f->wr && whence == 1)
		off -= f->n - f->p;	/* relative to what the caller has read */
	if(!f->wr)
		f->n = f->p = 0;
	if(seek(f->fd, off, whence) < 0){
		seterrno();
		return -1;
	}
	f->eof = 0;
	return 0;
}

int
c_fseek(CFile *f, vlong off, int whence)
{
	return c_fseeko(f, off, whence);
}

int
c_feof(CFile *f)
{
	return f->eof && f->p >= f->n;
}

int
c_ferror(CFile *f)
{
	return f->err;
}

void
c_clearerr(CFile *f)
{
	f->eof = 0;
	f->err = 0;
}

int
c_fileno(CFile *f)
{
	return f->fd;
}

void
c_flockfile(CFile*)
{
}

void
c_funlockfile(CFile*)
{
}

int
c_fflush_unlocked(CFile *f)
{
	return c_fflush(f);
}

int
c_ungetc(int c, CFile *f)
{
	if(c < 0)
		return -1;
	if(f->p > 0)
		f->buf[--f->p] = c;
	else{
		if(f->n >= Bufsize)
			return -1;
		memmove(f->buf + 1, f->buf, f->n);
		f->buf[0] = c;
		f->n++;
	}
	f->eof = 0;
	return c & 0xff;
}

/* glibc putc's slow path */
int
__overflow(CFile *f, int c)
{
	if(c < 0)
		return cflush(f);
	return c_putc(c, f);
}

/* wide-character stdio in the "C" locale: one byte is one character */
int
c_getwc(CFile *f)
{
	return cgetc(f);
}

int
c_ungetwc(int c, CFile *f)
{
	return c_ungetc(c, f);
}

int
c_fputwc(int c, CFile *f)
{
	return c_putc(c, f);
}

/* vasprintf: count, allocate, format */
int
c_vasprintf(char **out, char *fmt, void *ap)
{
	va_list a, b;
	int n;

	a = *(va_list*)ap;
	b = a;
	n = cvsnprintf(nil, 0, fmt, a);
	*out = malloc(n + 1);
	if(*out == nil)
		return -1;
	cvsnprintf(*out, n + 1, fmt, b);
	return n;
}

/*
 * errno.  Plan 9 has none; glibc's <errno.h> makes errno *__errno_location(),
 * and translated code compares it with Linux's values.
 */

enum
{
	Erange	= 34,	/* Linux ERANGE */
};

/* errno is each thread's: thread9f.c, thread_none.c */
int	*__errno_location(void);
#define	cerrno	(*__errno_location())

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

/* for os_p9.c */
void
c_seterrno(void)
{
	seterrno();
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
 * scanf, from a string or a CFile: %d %i %u %o %x %X %p %c %s %[set] %n
 * and %f %e %g (through strtod), with h hh l ll z j t L sizes, widths
 * and assignment suppression.  Returns the number of assignments, or -1
 * if the input ended before the first conversion.
 */

typedef struct Src Src;
struct Src
{
	char	*s;		/* a string, or */
	CFile	*f;		/* a file */
	int	count;		/* characters read, for %n */
};

static int
sget(Src *src)
{
	int c;

	if(src->f != nil)
		c = cgetc(src->f);
	else if(*src->s == 0)
		c = -1;
	else
		c = (uchar)*src->s++;
	if(c >= 0)
		src->count++;
	return c;
}

static void
sunget(Src *src, int c)
{
	if(c < 0)
		return;
	src->count--;
	if(src->f != nil)
		c_ungetc(c, src->f);
	else
		src->s--;
}

static int
isspc(int c)
{
	return c == ' ' || (c >= '\t' && c <= '\r');
}

/* store an integer of the given size (as in Spec.size) */
static void
storeint(void *p, int size, uvlong v)
{
	switch(size){
	case Schar:
		*(uchar*)p = v;
		break;
	case Sshort:
		*(ushort*)p = v;
		break;
	case Svlong:
		*(uvlong*)p = v;
		break;
	default:
		*(uint*)p = v;
		break;
	}
}

static int
cscanf(Src *src, char *fmt, va_list arg)
{
	char buf[Nbuf], set[256], *e, *p;
	int c, n, width, size, suppress, neg, base, i, inv, isdouble;
	uvlong v;
	double d;

	n = 0;
	for(; *fmt != 0; fmt++){
		if(isspc(*fmt)){
			while((c = sget(src)) >= 0 && isspc(c))
				;
			sunget(src, c);
			continue;
		}
		if(*fmt != '%' || fmt[1] == '%'){
			if(*fmt == '%')
				fmt++;
			c = sget(src);
			if(c != (uchar)*fmt){
				sunget(src, c);
				return c < 0 && n == 0 ? -1 : n;
			}
			continue;
		}
		fmt++;
		suppress = 0;
		if(*fmt == '*'){
			suppress = 1;
			fmt++;
		}
		width = 0;
		while(*fmt >= '0' && *fmt <= '9')
			width = width*10 + *fmt++ - '0';
		size = Sint;
		isdouble = 0;
		for(;; fmt++){
			if(*fmt == 'h')
				size = size == Sshort ? Schar : Sshort;
			else if(*fmt == 'l'){
				size = Svlong;
				isdouble = 1;
			}else if(*fmt == 'z' || *fmt == 'j' || *fmt == 't' || *fmt == 'q' || *fmt == 'L'){
				size = Svlong;
				isdouble = 1;
			}else
				break;
		}
		if(*fmt == 0)
			break;
		if(*fmt == 'n'){
			if(!suppress)
				storeint(va_arg(arg, void*), size, src->count);
			continue;
		}
		if(*fmt != 'c' && *fmt != '['){
			while((c = sget(src)) >= 0 && isspc(c))
				;
			sunget(src, c);
		}
		if(width == 0)
			width = *fmt == 'c' ? 1 : Nbuf - 1;
		if(width > Nbuf - 1)
			width = Nbuf - 1;
		switch(*fmt){
		case 'c':
			p = suppress ? nil : va_arg(arg, char*);
			for(i = 0; i < width; i++){
				c = sget(src);
				if(c < 0)
					return n == 0 && i == 0 ? -1 : n;
				if(p != nil)
					p[i] = c;
			}
			if(!suppress)
				n++;
			break;
		case 's':
		case '[':
			memset(set, 0, sizeof set);
			inv = 0;
			if(*fmt == 's'){
				for(i = 0; i < 256; i++)
					set[i] = !isspc(i);
			}else{
				fmt++;
				if(*fmt == '^'){
					inv = 1;
					fmt++;
				}
				if(*fmt == ']')
					set[(uchar)*fmt++] = 1;
				for(; *fmt != 0 && *fmt != ']'; fmt++)
					if(fmt[1] == '-' && fmt[2] != ']' && fmt[2] != 0){
						for(i = (uchar)fmt[0]; i <= (uchar)fmt[2]; i++)
							set[i] = 1;
						fmt += 2;
					}else
						set[(uchar)*fmt] = 1;
				if(*fmt == 0)
					return n;
				if(inv)
					for(i = 0; i < 256; i++)
						set[i] = !set[i];
			}
			p = suppress ? nil : va_arg(arg, char*);
			for(i = 0; i < width && (c = sget(src)) >= 0; i++){
				if(!set[c]){
					sunget(src, c);
					break;
				}
				if(p != nil)
					p[i] = c;
			}
			if(i == 0)
				return n == 0 && c < 0 ? -1 : n;
			if(p != nil){
				p[i] = 0;
				n++;
			}
			break;
		case 'f':
		case 'F':
		case 'e':
		case 'E':
		case 'g':
		case 'G':
		case 'a':
			for(i = 0; i < width && (c = sget(src)) >= 0; i++){
				if(!(c >= '0' && c <= '9') && c != '.' && c != 'e' && c != 'E' && c != '+' && c != '-'
				&& c != 'x' && c != 'X' && !(c >= 'a' && c <= 'f') && !(c >= 'A' && c <= 'F')
				&& c != 'i' && c != 'n' && c != 'N' && c != 'I'){
					sunget(src, c);
					break;
				}
				buf[i] = c;
			}
			buf[i] = 0;
			d = strtod(buf, &e);
			if(e == buf)
				return n == 0 && i == 0 ? -1 : n;
			if(!suppress){
				if(isdouble)
					*va_arg(arg, double*) = d;
				else
					*va_arg(arg, float*) = d;
				n++;
			}
			break;
		default:	/* d i u o x X p */
			base = 10;
			if(*fmt == 'o')
				base = 8;
			else if(*fmt == 'x' || *fmt == 'X' || *fmt == 'p'){
				base = 16;
				if(*fmt == 'p')
					size = Svlong;
			}else if(*fmt == 'i')
				base = 0;
			for(i = 0; i < width && (c = sget(src)) >= 0; i++){
				if(!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f') && !(c >= 'A' && c <= 'F')
				&& c != 'x' && c != 'X' && !(i == 0 && (c == '-' || c == '+'))){
					sunget(src, c);
					break;
				}
				buf[i] = c;
			}
			buf[i] = 0;
			neg = buf[0] == '-';
			v = cstrtou(buf, &e, base, ~(uvlong)0, &neg);
			if(e == buf)
				return n == 0 && i == 0 ? -1 : n;
			if(!suppress){
				storeint(va_arg(arg, void*), size, neg ? -v : v);
				n++;
			}
			break;
		}
	}
	return n;
}

int
__isoc23_sscanf(char *s, char *fmt, ...)
{
	va_list arg;
	Src src;
	int n;

	memset(&src, 0, sizeof src);
	src.s = s;
	va_start(arg, fmt);
	n = cscanf(&src, fmt, arg);
	va_end(arg);
	return n;
}

int
__isoc23_vsscanf(char *s, char *fmt, void *ap)
{
	Src src;

	memset(&src, 0, sizeof src);
	src.s = s;
	return cscanf(&src, fmt, *(va_list*)ap);
}

int
__isoc23_fscanf(CFile *f, char *fmt, ...)
{
	va_list arg;
	Src src;
	int n;

	memset(&src, 0, sizeof src);
	src.f = f;
	va_start(arg, fmt);
	n = cscanf(&src, fmt, arg);
	va_end(arg);
	return n;
}

/*
 * Single-precision math: through double, as the other float functions
 * above.  round() is not in Plan 9 libc.
 */

float
acosf(float x)
{
	return acos(x);
}

float
asinf(float x)
{
	return asin(x);
}

float
atanf(float x)
{
	return atan(x);
}

float
atan2f(float y, float x)
{
	return atan2(y, x);
}

float
expf(float x)
{
	return exp(x);
}

float
logf(float x)
{
	return log(x);
}

float
log10f(float x)
{
	return log10(x);
}

float
powf(float x, float y)
{
	return pow(x, y);
}

float
tanf(float x)
{
	return sin(x) / cos(x);	/* no tan in goken's libc */
}


float
modff(float x, float *ip)
{
	double i, f;

	f = modf(x, &i);
	*ip = i;
	return f;
}

/* not in goken's libc (9front has it): from sin and cos */
double
c_tan(double x)
{
	return sin(x) / cos(x);
}

/* halfway cases away from zero */
double
round(double x)
{
	if(x < 0)
		return -floor(-x + 0.5);
	return floor(x + 0.5);
}

/*
 * The "C" locale, the only one: locale objects are a dummy, conversions
 * ignore them, and the tables are glibc's layouts.
 */

enum
{
	Charmax	= 127,	/* CHAR_MAX: "not available" in struct lconv */
	Isupper	= 0x0100,	/* glibc <ctype.h> bits, little-endian */
	Islower	= 0x0200,
	Isalpha	= 0x0400,
	Isdigit	= 0x0800,
	Isxdigit	= 0x1000,
	Isspace	= 0x2000,
	Isprint	= 0x4000,
	Isgraph	= 0x8000,
	Isblank	= 0x0001,
	Iscntrl	= 0x0002,
	Ispunct	= 0x0004,
	Isalnum	= 0x0008,
};

ushort**	__ctype_b_loc(void);

/*
 * glibc struct __locale_struct, x86-64: libc++ reads the "C" locale's
 * tables from it (classic_table() is __cloc()->__ctype_b).
 */
typedef struct Clocale Clocale;
struct Clocale
{
	void	*locales[13];
	ushort	*ctypeb;
	int	*lowertab;
	int	*uppertab;
	char	*names[13];
};

static Clocale	clocale;	/* the one locale object */

void*
newlocale(int, char*, void*)
{
	int i;

	if(clocale.ctypeb == nil){
		clocale.ctypeb = *__ctype_b_loc();
		clocale.lowertab = *__ctype_tolower_loc();
		clocale.uppertab = *__ctype_toupper_loc();
		for(i = 0; i < nelem(clocale.names); i++)
			clocale.names[i] = "C";
	}
	return &clocale;
}

void
freelocale(void*)
{
}

/* the "previous" locale is the global one, ((locale_t)-1) */
void*
uselocale(void*)
{
	return (void*)-1;
}

/* glibc struct lconv, x86-64: 10 strings, then 14 chars */
typedef struct Clconv Clconv;
struct Clconv
{
	char	*decimal_point;
	char	*thousands_sep;
	char	*grouping;
	char	*int_curr_symbol;
	char	*currency_symbol;
	char	*mon_decimal_point;
	char	*mon_thousands_sep;
	char	*mon_grouping;
	char	*positive_sign;
	char	*negative_sign;
	char	frac[14];
};

static Clconv	lconv;

Clconv*
localeconv(void)
{
	if(lconv.decimal_point == nil){
		lconv.decimal_point = ".";
		lconv.thousands_sep = "";
		lconv.grouping = "";
		lconv.int_curr_symbol = "";
		lconv.currency_symbol = "";
		lconv.mon_decimal_point = "";
		lconv.mon_thousands_sep = "";
		lconv.mon_grouping = "";
		lconv.positive_sign = "";
		lconv.negative_sign = "";
		memset(lconv.frac, Charmax, sizeof lconv.frac);
	}
	return &lconv;
}

/* message catalogs: none (nl_catd -1), the default text is used */
void*
catopen(char*, int)
{
	return (void*)-1;
}

char*
catgets(void*, int, int, char *def)
{
	return def;
}

int
catclose(void*)
{
	return 0;
}

double
strtod_l(char *s, char **e, void*)
{
	return strtod(s, e);
}

float
strtof_l(char *s, char **e, void*)
{
	return strtod(s, e);
}

/* long double is double (-mlong-double-64) */
double
strtold_l(char *s, char **e, void*)
{
	return strtod(s, e);
}

vlong
__isoc23_strtoll_l(char *s, char **e, int base, void*)
{
	return cstrtoll(s, e, base);
}

uvlong
__isoc23_strtoull_l(char *s, char **e, int base, void*)
{
	return cstrtoull(s, e, base);
}

uvlong
__ctype_get_mb_cur_max(void)
{
	return 1;
}

/* glibc's classification table: unsigned short, indexed -128..255 */
static ushort	ctypebits[384];
static ushort	*ctypep;

static void
ctypebitsinit(void)
{
	int c, b;

	for(c = 0; c < 128; c++){
		b = 0;
		if(c >= 'A' && c <= 'Z')
			b |= Isupper | Isalpha | Isalnum | Isprint | Isgraph;
		if(c >= 'a' && c <= 'z')
			b |= Islower | Isalpha | Isalnum | Isprint | Isgraph;
		if(c >= '0' && c <= '9')
			b |= Isdigit | Isalnum | Isprint | Isgraph | Isxdigit;
		if((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
			b |= Isxdigit;
		if(c == ' ' || (c >= '\t' && c <= '\r'))
			b |= Isspace;
		if(c == ' ' || c == '\t')
			b |= Isblank;
		if(c < 32 || c == 127)
			b |= Iscntrl;
		if(c >= 32 && c < 127)
			b |= Isprint;
		if(c > 32 && c < 127){
			b |= Isgraph;
			if(!(b & Isalnum))
				b |= Ispunct;
		}
		ctypebits[c + 128] = b;
	}
	ctypep = ctypebits + 128;
}

ushort**
__ctype_b_loc(void)
{
	if(ctypep == nil)
		ctypebitsinit();
	return &ctypep;
}

/*
 * The rest of the "C" locale, for libc++'s *_byname facets and
 * conversions, which OpenTTD links but does not use: a character is a
 * byte, collation is byte order.
 */

static int
isbits(int c, int bits)
{
	if(c < 0 || c > 255)
		return 0;
	return ((*__ctype_b_loc())[c] & bits) != 0;
}

int
iswalpha_l(int c, void*)
{
	return isbits(c, Isalpha);
}

int
iswblank_l(int c, void*)
{
	return isbits(c, Isblank);
}

int
iswcntrl_l(int c, void*)
{
	return isbits(c, Iscntrl);
}

int
iswdigit_l(int c, void*)
{
	return isbits(c, Isdigit);
}

int
iswlower_l(int c, void*)
{
	return isbits(c, Islower);
}

int
iswprint_l(int c, void*)
{
	return isbits(c, Isprint);
}

int
iswpunct_l(int c, void*)
{
	return isbits(c, Ispunct);
}

int
iswspace_l(int c, void*)
{
	return isbits(c, Isspace);
}

int
iswupper_l(int c, void*)
{
	return isbits(c, Isupper);
}

int
iswxdigit_l(int c, void*)
{
	return isbits(c, Isxdigit);
}


int
toupper_l(int c, void*)
{
	return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}

int
tolower_l(int c, void*)
{
	return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

int
towupper_l(int c, void *l)
{
	return toupper_l(c, l);
}

int
towlower_l(int c, void *l)
{
	return tolower_l(c, l);
}

int
strcoll_l(char *a, char *b, void*)
{
	return strcmp(a, b);
}

uvlong
strxfrm_l(char *d, char *s, uvlong n, void*)
{
	uvlong len;

	len = strlen(s);
	if(len < n)
		memmove(d, s, len + 1);
	return len;
}

int
wmemcmp(int *a, int *b, uvlong n)
{
	for(; n > 0; n--, a++, b++)
		if(*a != *b)
			return *a < *b ? -1 : 1;
	return 0;
}

int
wcscoll_l(int *a, int *b, void*)
{
	for(; *a == *b; a++, b++)
		if(*a == 0)
			return 0;
	return *a < *b ? -1 : 1;
}

uvlong
wcsxfrm_l(int *d, int *s, uvlong n, void*)
{
	uvlong len;

	for(len = 0; s[len] != 0; len++)
		;
	if(len < n)
		memmove(d, s, (len + 1) * sizeof(int));
	return len;
}

/* EOF (-1) is WEOF; a byte is its own character */
uint
btowc(int c)
{
	return c == -1 ? 0xFFFFFFFF : (uchar)c;
}

int
wctob(uint c)
{
	return c < 256 ? c : -1;
}

char*
setlocale(int, char *name)
{
	if(name == nil || *name == 0 || strcmp(name, "C") == 0 || strcmp(name, "POSIX") == 0)
		return "C";
	return nil;
}

int
swprintf(int*, uvlong, int*, ...)
{
	return -1;
}

/* the wide string as bytes, up to the first character above 127 */
static char*
narrow(int *s, char *buf, int n)
{
	int i;

	for(i = 0; i < n - 1 && s[i] > 0 && s[i] < 128; i++)
		buf[i] = s[i];
	buf[i] = 0;
	return buf;
}

static void
wideend(int *s, char *buf, char *e, int **ep)
{
	if(ep != nil)
		*ep = s + (e - buf);
}

vlong
__isoc23_wcstoll(int *s, int **ep, int base)
{
	char buf[128], *e;
	vlong v;

	v = cstrtoll(narrow(s, buf, sizeof buf), &e, base);
	wideend(s, buf, e, ep);
	return v;
}

vlong
__isoc23_wcstol(int *s, int **ep, int base)
{
	return __isoc23_wcstoll(s, ep, base);
}

uvlong
__isoc23_wcstoull(int *s, int **ep, int base)
{
	char buf[128], *e;
	uvlong v;

	v = cstrtoull(narrow(s, buf, sizeof buf), &e, base);
	wideend(s, buf, e, ep);
	return v;
}

uvlong
__isoc23_wcstoul(int *s, int **ep, int base)
{
	return __isoc23_wcstoull(s, ep, base);
}

double
wcstod(int *s, int **ep)
{
	char buf[128], *e;
	double v;

	v = strtod(narrow(s, buf, sizeof buf), &e);
	wideend(s, buf, e, ep);
	return v;
}

float
wcstof(int *s, int **ep)
{
	return wcstod(s, ep);
}

double
wcstold(int *s, int **ep)
{
	return wcstod(s, ep);
}

float
strtof(char *s, char **e)
{
	return strtod(s, e);
}

double
strtold(char *s, char **e)
{
	return strtod(s, e);
}

/* glibc struct tm, x86-64 */
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

static char	*wdays[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static char	*months[] = { "January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December" };

/* strftime in the "C" locale: the common conversions */
uvlong
strftime_l(char *buf, uvlong size, char *fmt, Ctm *tm, void*)
{
	char *p, *e, tmp[64];

	p = buf;
	e = buf + size;
	for(; *fmt != 0 && p < e; fmt++){
		if(*fmt != '%'){
			*p++ = *fmt;
			continue;
		}
		fmt++;
		tmp[0] = 0;
		switch(*fmt){
		case 'Y': snprint(tmp, sizeof tmp, "%d", tm->year + 1900); break;
		case 'y': snprint(tmp, sizeof tmp, "%.2d", tm->year % 100); break;
		case 'm': snprint(tmp, sizeof tmp, "%.2d", tm->mon + 1); break;
		case 'd': snprint(tmp, sizeof tmp, "%.2d", tm->mday); break;
		case 'e': snprint(tmp, sizeof tmp, "%2d", tm->mday); break;
		case 'H': snprint(tmp, sizeof tmp, "%.2d", tm->hour); break;
		case 'M': snprint(tmp, sizeof tmp, "%.2d", tm->min); break;
		case 'S': snprint(tmp, sizeof tmp, "%.2d", tm->sec); break;
		case 'j': snprint(tmp, sizeof tmp, "%.3d", tm->yday + 1); break;
		case 'a': snprint(tmp, sizeof tmp, "%.3s", wdays[tm->wday % 7]); break;
		case 'A': snprint(tmp, sizeof tmp, "%s", wdays[tm->wday % 7]); break;
		case 'b':
		case 'h': snprint(tmp, sizeof tmp, "%.3s", months[tm->mon % 12]); break;
		case 'B': snprint(tmp, sizeof tmp, "%s", months[tm->mon % 12]); break;
		case 'p': snprint(tmp, sizeof tmp, "%s", tm->hour < 12 ? "AM" : "PM"); break;
		case 'F': snprint(tmp, sizeof tmp, "%d-%.2d-%.2d", tm->year + 1900, tm->mon + 1, tm->mday); break;
		case 'T': snprint(tmp, sizeof tmp, "%.2d:%.2d:%.2d", tm->hour, tm->min, tm->sec); break;
		case 'D': snprint(tmp, sizeof tmp, "%.2d/%.2d/%.2d", tm->mon + 1, tm->mday, tm->year % 100); break;
		case 'Z': snprint(tmp, sizeof tmp, "%s", tm->zone != nil ? tm->zone : "GMT"); break;
		case '%': snprint(tmp, sizeof tmp, "%%"); break;
		case 0: fmt--; break;
		default: snprint(tmp, sizeof tmp, "%%%c", *fmt); break;
		}
		if(p + strlen(tmp) >= e)
			return 0;
		strcpy(p, tmp);
		p += strlen(tmp);
	}
	if(p >= e)
		return 0;
	*p = 0;
	return p - buf;
}

/*
 * Wide characters in the "C" locale: wchar_t (glibc: 32 bits) holds one
 * byte, and each byte is one character.
 */

uvlong
mbrtowc(int *wc, char *s, uvlong n, void*)
{
	if(s == nil)
		return 0;
	if(n == 0)
		return -2;
	if(wc != nil)
		*wc = (uchar)*s;
	return *s != 0;
}

int
mbtowc(int *wc, char *s, uvlong n)
{
	if(s == nil)
		return 0;
	if(n == 0)
		return -1;
	if(wc != nil)
		*wc = (uchar)*s;
	return *s != 0;
}

uvlong
mbsnrtowcs(int *dst, char **src, uvlong nms, uvlong len, void*)
{
	uvlong i;
	char *s;

	s = *src;
	for(i = 0; i < nms && (dst == nil || i < len); i++){
		if(dst != nil)
			dst[i] = (uchar)s[i];
		if(s[i] == 0){
			if(dst != nil)
				*src = nil;
			return i;
		}
	}
	if(dst != nil)
		*src = s + i;
	return i;
}

uvlong
mbsrtowcs(int *dst, char **src, uvlong len, void *ps)
{
	return mbsnrtowcs(dst, src, ~(uvlong)0, len, ps);
}

uvlong
wcrtomb(char *s, int wc, void*)
{
	if(s == nil)
		return 1;
	if(wc < 0 || wc > 255){
		cerrno = 84;	/* EILSEQ */
		return -1;
	}
	*s = wc;
	return 1;
}

uvlong
wcsnrtombs(char *dst, int **src, uvlong nwc, uvlong len, void*)
{
	uvlong i;
	int *s;

	s = *src;
	for(i = 0; i < nwc && (dst == nil || i < len); i++){
		if(s[i] < 0 || s[i] > 255){
			cerrno = 84;
			return -1;
		}
		if(dst != nil)
			dst[i] = s[i];
		if(s[i] == 0){
			if(dst != nil)
				*src = nil;
			return i;
		}
	}
	if(dst != nil)
		*src = s + i;
	return i;
}

uvlong
wcslen(int *s)
{
	uvlong n;

	for(n = 0; s[n] != 0; n++)
		;
	return n;
}

int*
wmemchr(int *s, int c, uvlong n)
{
	uvlong i;

	for(i = 0; i < n; i++)
		if(s[i] == c)
			return s + i;
	return nil;
}

void	p9killkin(void);	/* thread9f.c; thread_none.c */

/*
 * Process exit: flush stdout and turn the C status into an exits(2) string.
 * The program's main is renamed c_main, so its return value becomes the status.
 */

void
c_exit(int status)
{
	char buf[16];

	cflush(&cout);
	p9killkin();	/* the program's other processes end with it */
	if(status == 0)
		exits(nil);
	snprint(buf, sizeof buf, "%d", status);
	exits(buf);
}

int	c_main(int, char**);
void	__p9_ctors(void);	/* static constructors, emitted by lower.py */
void	p9runmain(void (*)(void*), void*);	/* thread9f.c: on a stack the threads share; thread_none.c */

static int	margc;
static char	**margv;

static void
runmain(void*)
{
	__p9_ctors();
	c_exit(c_main(margc, margv));
}

void
main(int argc, char **argv)
{
	margc = argc;
	margv = argv;
	p9runmain(runmain, nil);
}
