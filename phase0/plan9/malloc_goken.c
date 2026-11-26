/*
 * malloc for goken's libc, whose malloc is a placeholder (a 64 MB bump
 * allocator; free does nothing, realloc copies the new size): enough
 * for a benchmark, not for a program that frees and allocates for a
 * long time.  9front's libc has the real pool allocator: this file is
 * linked into goken builds only (phase2/build9.sh).
 *
 * Blocks come in power-of-two classes from 16 bytes up; a free block
 * goes on its class's list and is used again for the same class.
 * Each block has a 16-byte header holding its class, so blocks are
 * 16-byte aligned (as glibc's, which the translated code assumes).
 * Memory comes from the Linux break, in hunks of at least Hunk bytes:
 * not through goken's sbrk, which starts at the end of bss, while with
 * address randomization Linux puts the break higher (brk to a lower
 * address returns the current break, which sbrk takes as success, and
 * hands out unmapped memory).  It defines every symbol of goken's
 * port/minimal_malloc.c, so the linker never takes that one from libc.
 */
#include <u.h>
#include <libc.h>

enum
{
	Hdr	= 16,		/* header size, and alignment */
	Minclass	= 4,		/* smallest block: 16 bytes */
	Nclass	= 40,
	Hunk	= 16*1024*1024,
	Magic	= 0x6d616c6c,
};

typedef struct Block Block;
struct Block
{
	uint	magic;
	uint	class;
	Block	*next;		/* in the free list; header only */
};

extern uintptr	_sysbrk(void*);	/* goken: the brk system call */

static Block	*freelist[Nclass];
static uchar	*hunk;		/* unused part of the last hunk */
static uvlong	hunkleft;
static uintptr	brkcur;		/* the program break */

/* n more bytes at the program break */
static uchar*
grow(uvlong n)
{
	uintptr p;

	if(brkcur == 0)
		brkcur = _sysbrk(nil);
	p = brkcur;
	if(_sysbrk((void*)(p + n)) < p + n)
		return nil;
	brkcur = p + n;
	return (uchar*)p;
}

static uchar*
more(uvlong n)
{
	uchar *p;
	uvlong m;

	if(n > hunkleft){
		m = n + Hdr;
		if(m < Hunk)
			m = Hunk;
		p = grow(m);
		if(p == nil)
			return nil;
		/* align the start; the rest of the old hunk is dropped */
		hunk = (uchar*)(((uintptr)p + Hdr-1) & ~(uintptr)(Hdr-1));
		hunkleft = m - (hunk - p);
	}
	p = hunk;
	hunk += n;
	hunkleft -= n;
	return p;
}

static int
classof(uvlong n)
{
	int c;

	n += Hdr;
	for(c = Minclass; c < Nclass; c++)
		if(n <= 1ULL<<c)
			return c;
	return -1;
}

void*
malloc(ulong n)
{
	Block *b;
	int c;

	c = classof(n);
	if(c < 0)
		return nil;
	b = freelist[c];
	if(b != nil)
		freelist[c] = b->next;
	else{
		b = (Block*)more(1ULL<<c);
		if(b == nil)
			return nil;
		b->magic = Magic;
		b->class = c;
	}
	b->next = nil;
	return (uchar*)b + Hdr;
}

void
free(void *v)
{
	Block *b;

	if(v == nil)
		return;
	b = (Block*)((uchar*)v - Hdr);
	if(b->magic != Magic || b->class >= Nclass)
		abort();
	b->next = freelist[b->class];
	freelist[b->class] = b;
}

void*
calloc(ulong n, ulong size)
{
	void *v;
	uvlong m;

	m = (uvlong)n * size;
	if(size != 0 && m / size != n)
		return nil;
	v = malloc(m);
	if(v != nil)
		memset(v, 0, m);
	return v;
}

void*
realloc(void *v, ulong n)
{
	Block *b;
	uvlong old;
	void *w;

	if(v == nil)
		return malloc(n);
	b = (Block*)((uchar*)v - Hdr);
	if(b->magic != Magic || b->class >= Nclass)
		abort();
	old = (1ULL<<b->class) - Hdr;
	if(n <= old)
		return v;
	w = malloc(n);
	if(w == nil)
		return nil;
	memmove(w, v, old);
	free(v);
	return w;
}

void
setmalloctag(void*, ulong)
{
}

ulong
getmalloctag(void*)
{
	return 0;
}

ulong
getrealloctag(void*)
{
	return 0;
}
