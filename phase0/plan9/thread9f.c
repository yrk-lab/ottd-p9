/*
 * The 9front part of the program's threads (thread_p9.c, thread.h):
 * a thread is a process sharing memory with the others (rfork
 * RFPROC|RFMEM), as net9f.c's and audio9f.c's helpers are.
 *
 * Plan 9 gives each such process a stack of its own, a private copy
 * at the same addresses.  C++ hands objects on its stack to other
 * threads (a lambda's captures, a condition variable a thread waits
 * on): so, as libthread does, the program runs on stacks in shared
 * memory, main's (p9runmain) and each new process's (p9proc), and the
 * private stacks are left for what must be per process: a slot there
 * (privalloc) has the same address in every thread and each thread's
 * contents, which is how a thread finds its Thread.  A process with
 * shared memory is forked from the forker's private stack (an rfork
 * on a shared stack would leave two processes on it), the point
 * p9runmain keeps for that.
 *
 * Semaphores are the kernel's (semacquire), atomics atomic(2).  errno
 * is each thread's.  At exit the other threads are stopped: they
 * would outlive the program.
 */
#include <u.h>
#include <libc.h>
#include "thread.h"

enum
{
	Nthreads	= 256,	/* running at once */
	Mainstack	= 32*1024*1024,
	Procstack	= 8*1024*1024,
};

typedef struct Launch Launch;
typedef struct Fork Fork;

/* a new process's work, run on its stack in shared memory */
struct Launch
{
	void	(*fn)(void*);
	void	*arg;
	jmp_buf	back;		/* on its private stack, to end there */
};

/* a request to fork, made on a shared stack */
struct Fork
{
	void	(*fn)(void*);
	void	*arg;
	jmp_buf	ret;		/* back to the requester */
	int	pid;
};

void	**privalloc(void);
int	*threaderrno(void);	/* thread_p9.c */

static Thread	**slot;		/* per process: the thread */
static Fork	**forkreq;	/* per process: its request */
static jmp_buf	forkpoint;	/* in p9runmain's frame, on each process's private stack */
static int	pids[Nthreads];
static Lock	pidlk;

Thread*
tself(void)
{
	if(slot == nil)
		return nil;
	return *slot;
}

int*
__errno_location(void)
{
	return threaderrno();
}

static void
killthreads(void)
{
	int i, me;

	me = getpid();
	lock(&pidlk);
	for(i = 0; i < Nthreads; i++)
		if(pids[i] != 0 && pids[i] != me)
			postnote(PNPROC, pids[i], "kill");
	unlock(&pidlk);
}

/* more than Nthreads at once: the rest are not stopped at exit */
static void
addpid(int pid)
{
	int i;

	lock(&pidlk);
	for(i = 0; i < Nthreads; i++)
		if(pids[i] == 0){
			pids[i] = pid;
			break;
		}
	unlock(&pidlk);
}

static void
droppid(int pid)
{
	int i;

	lock(&pidlk);
	for(i = 0; i < Nthreads; i++)
		if(pids[i] == pid)
			pids[i] = 0;
	unlock(&pidlk);
}

/* run f(arg) on the stack stk[size], as libthread starts its threads; f does not return */
static void
launcher(int, void (*f)(void*), void *arg)
{
	(*f)(arg);
	abort();
}

static void
onstack(uchar *stk, ulong size, void (*f)(void*), void *arg)
{
	jmp_buf j;
	uvlong *tos;

	tos = (uvlong*)&stk[size & ~7];
	*--tos = (uvlong)arg;
	*--tos = (uvlong)f;
	*--tos = 0;	/* launcher's first argument, which goes in a register */
	j[JMPBUFPC] = (uvlong)launcher + JMPBUFDPC;
	j[JMPBUFSP] = (uvlong)tos - 2*8;
	longjmp(j, 1);
}

/* a new process, on its stack: the work, then back to its private stack */
static void
launched(void *a)
{
	Launch *l;

	l = a;
	l->fn(l->arg);
	longjmp(l->back, 1);
}

/*
 * The program's main, f(arg), on a stack in shared memory; this
 * frame, on the private stack, is where processes are forked (p9proc
 * jumps here): every process has it, a copy of its forker's.
 */
void
p9runmain(void (*f)(void*), void *arg)
{
	Launch l;
	Fork *r;
	uchar *stk;
	int pid;

	slot = (Thread**)privalloc();
	forkreq = (Fork**)privalloc();
	if(slot == nil || forkreq == nil)
		sysfatal("threads: no private slots");
	*slot = nil;		/* thread_p9.c's main thread */
	*forkreq = nil;
	if(setjmp(forkpoint)){
		r = *forkreq;
		l.fn = r->fn;
		l.arg = r->arg;
		pid = rfork(RFPROC|RFMEM|RFNOWAIT);
		if(pid == 0){
			*forkreq = nil;
			stk = malloc(Procstack);
			if(stk == nil)
				_exits("no stack");
			if(setjmp(l.back) == 0)
				onstack(stk, Procstack, launched, &l);
			free(stk);
			_exits(nil);
		}
		r->pid = pid;
		longjmp(r->ret, 1);
	}
	atexit(killthreads);
	stk = malloc(Mainstack);
	if(stk == nil)
		sysfatal("no stack for main");
	onstack(stk, Mainstack, f, arg);
}

/* a process sharing memory, running fn(arg) on a stack of its own in it; its pid */
int
p9proc(void (*fn)(void*), void *arg)
{
	Fork r;

	r.fn = fn;
	r.arg = arg;
	r.pid = -1;
	*forkreq = &r;
	if(setjmp(r.ret) == 0)
		longjmp(forkpoint, 1);
	*forkreq = nil;
	return r.pid;
}

typedef struct Tstart Tstart;
struct Tstart
{
	Thread	*t;
	void	(*fn)(Thread*);
};

static void
threadstart(void *a)
{
	Tstart s;

	s = *(Tstart*)a;
	free(a);
	*slot = s.t;
	addpid(getpid());
	s.fn(s.t);
	droppid(getpid());
}

int
tstart(Thread *t, void (*fn)(Thread*))
{
	Tstart *s;

	s = malloc(sizeof *s);
	if(s == nil)
		return -1;
	s->t = t;
	s->fn = fn;
	if(p9proc(threadstart, s) < 0){
		free(s);
		return -1;
	}
	return 0;
}

void
semup(int *s, int n)
{
	semrelease((long*)s, n);
}

int
semdown(int *s, vlong ms)
{
	int r;

	for(;;){
		if(ms < 0)
			r = semacquire((long*)s, 1);
		else
			r = tsemacquire((long*)s, ms);
		if(r >= 0)
			return r > 0;
		/* a note: again */
	}
}

int
tcas(int *p, int old, int new)
{
	return acasl((Along*)p, old, new);
}

int
tadd(int *p, int v)
{
	return aincl((Along*)p, v);
}

void
tyield(void)
{
	sleep(0);
}

vlong
tnow(int)
{
	return nsec();
}

/*
 * __atomic_* library calls, as cbe makes them of atomic instructions,
 * on atomic(2), which is sequentially consistent: the memory orders
 * (the last arguments) are all served by it.  Aligned loads are
 * atomic on amd64.  A byte is changed by a compare and swap of the
 * aligned word around it.
 */

uint
__atomic_fetch_add_4(uint *p, uint v, int)
{
	return aincl((Along*)p, v) - v;
}

uint
__atomic_fetch_sub_4(uint *p, uint v, int)
{
	return aincl((Along*)p, -v) + v;
}

uvlong
__atomic_fetch_add_8(uvlong *p, uvlong v, int)
{
	return aincv((Avlong*)p, v) - v;
}

uvlong
__atomic_fetch_sub_8(uvlong *p, uvlong v, int)
{
	return aincv((Avlong*)p, -v) + v;
}

uchar
__atomic_load_1(uchar *p, int)
{
	return *(volatile uchar*)p;
}

uint
__atomic_load_4(uint *p, int)
{
	return agetl((Along*)p);
}

uvlong
__atomic_load_8(uvlong *p, int)
{
	return agetv((Avlong*)p);
}

void
__atomic_store_4(uint *p, uint v, int)
{
	aswapl((Along*)p, v);
}

void
__atomic_store_8(uvlong *p, uvlong v, int)
{
	aswapv((Avlong*)p, v);
}

uint
__atomic_exchange_4(uint *p, uint v, int)
{
	return aswapl((Along*)p, v);
}

uvlong
__atomic_exchange_8(uvlong *p, uvlong v, int)
{
	return aswapv((Avlong*)p, v);
}

int
__atomic_compare_exchange_4(uint *p, uint *expected, uint desired, int, int)
{
	uint v;

	for(;;){
		if(acasl((Along*)p, *expected, desired))
			return 1;
		v = agetl((Along*)p);
		if(v != *expected){
			*expected = v;
			return 0;
		}
	}
}

int
__atomic_compare_exchange_8(uvlong *p, uvlong *expected, uvlong desired, int, int)
{
	uvlong v;

	for(;;){
		if(acasv((Avlong*)p, *expected, desired))
			return 1;
		v = agetv((Avlong*)p);
		if(v != *expected){
			*expected = v;
			return 0;
		}
	}
}

/* op (0 store, 1 and, 2 or) v into the byte at p; the old byte */
static uchar
byteop(uchar *p, uchar v, int op)
{
	Along *w;
	int shift;
	ulong o, n;
	uchar b, nb;

	w = (Along*)((uintptr)p & ~3);
	shift = ((uintptr)p & 3) * 8;
	do{
		o = agetl(w);
		b = o >> shift;
		switch(op){
		default:
			nb = v;
			break;
		case 1:
			nb = b & v;
			break;
		case 2:
			nb = b | v;
			break;
		}
		n = (o & ~(0xFF << shift)) | (ulong)nb << shift;
	}while(!acasl(w, o, n));
	return b;
}

uchar
__atomic_exchange_1(uchar *p, uchar v, int)
{
	return byteop(p, v, 0);
}

uchar
__atomic_fetch_and_1(uchar *p, uchar v, int)
{
	return byteop(p, v, 1);
}

uchar
__atomic_fetch_or_1(uchar *p, uchar v, int)
{
	return byteop(p, v, 2);
}

void
__atomic_store_1(uchar *p, uchar v, int)
{
	byteop(p, v, 0);
}
