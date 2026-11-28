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
 * Semaphores are the kernel's (semacquire); the atomics are
 * atom_p9.c's, on the system's (atom9f.c, atom9l.c).  errno is each
 * thread's.
 *
 * The program's processes end together, as libthread's (threadexitsall)
 * and rd's and vncv's do: every process forked here is listed, and
 * when the program exits (c_exit, _exit, or main's exits: p9killkin)
 * or a process dies of a note nobody takes (sig9f.c, whose handler
 * every process has from p9runmain on), the others are sent Kinnote,
 * which ends them quietly.  A helper is stopped alone the same way
 * (p9stopproc).  Not by atexit in each process: Plan 9's atexit table
 * is shared memory here, and too small for the threads.
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
void	noteinit(void);		/* sig9f.c */
int	*threaderrno(void);	/* thread_p9.c */

static Thread	**slot;		/* per process: the thread */
static Fork	**forkreq;	/* per process: its request */
static jmp_buf	forkpoint;	/* in p9runmain's frame, on each process's private stack */
static int	pids[Nthreads];	/* the processes forked here */
static int	mainpid;
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

char	Kinnote[] = "die openttd kin";

/* end the program's other processes */
void
p9killkin(void)
{
	int i, me;

	me = getpid();
	lock(&pidlk);
	for(i = 0; i < Nthreads; i++)
		if(pids[i] != 0 && pids[i] != me)
			postnote(PNPROC, pids[i], Kinnote);
	if(mainpid != me)
		postnote(PNPROC, mainpid, Kinnote);
	unlock(&pidlk);
}

/* end one of them */
void
p9stopproc(int pid)
{
	postnote(PNPROC, pid, Kinnote);
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

	mainpid = getpid();
	noteinit();		/* before any fork: every process has the handler */
	/*
	 * IEEE's default, which C and C++ assume: an invalid operation, a
	 * division by zero or an overflow makes a NaN or an infinity and
	 * goes on.  Plan 9 starts a program with those trapping ("sys: fp:
	 * invalid operation"); forks inherit the setting
	 */
	setfcr(getfcr() & ~(FPINVAL|FPZDIV|FPOVFL|FPUNFL|FPINEX));
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
			droppid(getpid());
			_exits(nil);
		}
		if(pid > 0)
			addpid(pid);	/* here, before it can run long */
		r->pid = pid;
		longjmp(r->ret, 1);
	}
	atexit(p9killkin);
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
	s.fn(s.t);
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
 * The process's args, as libthread's threadsetname: ps -a shows them
 * after its name in brackets, "6.openttd [savegame]".  OpenTTD's
 * prefix (ottd:savegame) says no more than that.
 */
void
tsetname(char *name)
{
	char buf[32];
	int fd;

	if(strncmp(name, "ottd:", 5) == 0)
		name += 5;

	snprint(buf, sizeof buf, "/proc/%d/args", getpid());
	fd = open(buf, OWRITE);
	if(fd < 0)
		return;
	write(fd, name, strlen(name));
	close(fd);
}
