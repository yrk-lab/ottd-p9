/*
 * The program's threads: its pthread calls (libc++'s std::thread,
 * std::mutex, std::condition_variable; lower.py renames them
 * c_pthread_*, so that plan9port's own use of glibc's threads is left
 * alone), on glibc's layouts, which the translated code allocates and
 * zeroes, over what a system part gives (thread.h): thread9f.c on
 * 9front, threadp9p.c with plan9port.  goken has no threads:
 * thread_none.c.
 *	pthread_mutex_t	(40 bytes) a count and a semaphore (a mutex is
 *			taken by counting in; who finds it taken waits on
 *			the semaphore, which the holder ups when it lets go
 *			to someone waiting), the owner and depth (recursive
 *			mutexes)
 *	pthread_cond_t	(48 bytes) a lock and a queue of the threads
 *			waiting, each on a semaphore of its own: a signal
 *			wakes the first, so a waiter that comes later
 *			cannot take its wakeup (one semaphore for all
 *			would let it); a signal may come without the
 *			mutex held
 * Each thread has its state (Thread): errno on Plan 9, its keys, its
 * exception state (../eh/eh_rt.h, EH_TLS: eh_self).  Function-local
 * statics are initialized once (__cxa_guard).
 */
#include <u.h>
#include <libc.h>
#include "eh_rt.h"
#include "thread.h"

enum
{
	Nkeys		= 64,

	/* Linux errno */
	Eagain		= 11,
	Ebusy		= 16,
	Einval		= 22,
	Etimedout	= 110,

	Recursive	= 1,	/* PTHREAD_MUTEX_RECURSIVE */
};

typedef struct Mutex Mutex;
typedef struct Cond Cond;

struct Thread
{
	int	errnum;
	Ehtls	eh;
	void	*key[Nkeys];
	void	*(*fn)(void*);
	void	*arg;
	void	*ret;
	int	done;		/* semaphore: upped when it has returned */
	int	lk;
	int	detached;
	int	exited;
	int	wsem;		/* waiting on a condition variable */
	Thread	*wnext;
};

struct Mutex
{
	int	n;		/* holder and waiters */
	int	sem;
	Thread	*owner;
	int	depth;		/* recursive: times taken */
	int	kind;		/* Recursive or 0 */
};

struct Cond
{
	int	lk;
	Thread	*head;		/* waiting, first to wake first */
	Thread	*tail;
};

static Thread	mainthread;
static int	keylk;
static void	(*keydtor[Nkeys])(void*);
static int	nkeys;
static int	guardlk;

static void
spinlock(int *l)
{
	while(!tcas(l, 0, 1))
		tyield();
}

static void
spinunlock(int *l)
{
	tcas(l, 1, 0);
}

static Thread*
cur(void)
{
	Thread *t;

	t = tself();
	return t != nil ? t : &mainthread;
}

/* the calling thread's exception state (../eh/eh_rt.h) */
Ehtls*
eh_self(void)
{
	return &cur()->eh;
}

/* the calling thread's errno, for the systems that keep it (thread9f.c) */
int*
threaderrno(void)
{
	return &cur()->errnum;
}

/* a new thread: run, let go of the keys' values, say done */
static void
threadmain(Thread *t)
{
	int i, d;
	void *v;

	t->ret = t->fn(t->arg);
	for(i = 0; i < nkeys; i++)
		if(keydtor[i] != nil && (v = t->key[i]) != nil){
			t->key[i] = nil;
			keydtor[i](v);
		}
	spinlock(&t->lk);
	t->exited = 1;
	d = t->detached;
	spinunlock(&t->lk);
	if(d)
		free(t);
	else
		semup(&t->done, 1);
}

int
c_pthread_create(uvlong *id, void*, void *(*fn)(void*), void *arg)
{
	Thread *t;

	t = mallocz(sizeof *t, 1);
	if(t == nil)
		return Eagain;
	t->fn = fn;
	t->arg = arg;
	if(tstart(t, threadmain) < 0){
		free(t);
		return Eagain;
	}
	*id = (uvlong)t;
	return 0;
}

int
c_pthread_join(uvlong id, void **ret)
{
	Thread *t;

	t = (Thread*)id;
	semdown(&t->done, -1);
	if(ret != nil)
		*ret = t->ret;
	free(t);
	return 0;
}

int
c_pthread_detach(uvlong id)
{
	Thread *t;
	int x;

	t = (Thread*)id;
	spinlock(&t->lk);
	t->detached = 1;
	x = t->exited;
	spinunlock(&t->lk);
	if(x)
		free(t);
	return 0;
}

uvlong
c_pthread_self(void)
{
	return (uvlong)cur();
}

/* the program names only itself (SetCurrentThreadName) */
int
c_pthread_setname_np(uvlong id, char *name)
{
	if((Thread*)id == cur())
		tsetname(name);
	return 0;
}

int
c_pthread_key_create(uint *key, void (*dtor)(void*))
{
	spinlock(&keylk);
	if(nkeys == Nkeys){
		spinunlock(&keylk);
		return Eagain;
	}
	keydtor[nkeys] = dtor;
	*key = nkeys++;
	spinunlock(&keylk);
	return 0;
}

int
c_pthread_setspecific(uint key, void *v)
{
	if(key >= Nkeys)
		return Einval;
	cur()->key[key] = v;
	return 0;
}

void*
c_pthread_getspecific(uint key)
{
	if(key >= Nkeys)
		return nil;
	return cur()->key[key];
}

/*
 * Mutexes
 */

int
c_pthread_mutexattr_init(int *a)
{
	*a = 0;
	return 0;
}

int
c_pthread_mutexattr_destroy(int*)
{
	return 0;
}

int
c_pthread_mutexattr_settype(int *a, int kind)
{
	*a = kind;
	return 0;
}

int
c_pthread_mutex_init(Mutex *m, int *a)
{
	memset(m, 0, sizeof *m);
	if(a != nil && *a == Recursive)
		m->kind = Recursive;
	return 0;
}

int
c_pthread_mutex_destroy(Mutex*)
{
	return 0;
}

int
c_pthread_mutex_lock(Mutex *m)
{
	Thread *me;

	me = cur();
	if(m->kind == Recursive && m->owner == me){
		m->depth++;
		return 0;
	}
	if(tadd(&m->n, 1) > 1)
		semdown(&m->sem, -1);
	m->owner = me;
	m->depth = 1;
	return 0;
}

int
c_pthread_mutex_trylock(Mutex *m)
{
	Thread *me;

	me = cur();
	if(m->kind == Recursive && m->owner == me){
		m->depth++;
		return 0;
	}
	if(!tcas(&m->n, 0, 1))
		return Ebusy;
	m->owner = me;
	m->depth = 1;
	return 0;
}

int
c_pthread_mutex_unlock(Mutex *m)
{
	if(m->kind == Recursive && --m->depth > 0)
		return 0;
	m->owner = nil;
	m->depth = 0;
	if(tadd(&m->n, -1) > 0)
		semup(&m->sem, 1);
	return 0;
}

/*
 * Condition variables
 */

int
c_pthread_cond_destroy(Cond*)
{
	return 0;
}

/* the first waiter off the queue, or nil; c->lk held */
static Thread*
dequeue(Cond *c)
{
	Thread *t;

	t = c->head;
	if(t != nil){
		c->head = t->wnext;
		if(c->head == nil)
			c->tail = nil;
		t->wnext = nil;
	}
	return t;
}

int
c_pthread_cond_signal(Cond *c)
{
	Thread *t;

	spinlock(&c->lk);
	if((t = dequeue(c)) != nil)
		semup(&t->wsem, 1);
	spinunlock(&c->lk);
	return 0;
}

int
c_pthread_cond_broadcast(Cond *c)
{
	Thread *t;

	spinlock(&c->lk);
	while((t = dequeue(c)) != nil)
		semup(&t->wsem, 1);
	spinunlock(&c->lk);
	return 0;
}

/*
 * Wait for a signal, until deadline (nanoseconds on clock; 0: no
 * end).  The waiter joins the queue before it lets the mutex go, so a
 * signal after that finds it.  When the time runs out it takes itself
 * off the queue; if a signal took it off first, that signal's wakeup
 * is its, and it is taken.
 */
static int
condwait(Cond *c, Mutex *m, int clock, vlong deadline)
{
	Thread *me, **l;
	int depth, r;
	vlong ms;

	me = cur();
	spinlock(&c->lk);
	me->wnext = nil;
	if(c->tail != nil)
		c->tail->wnext = me;
	else
		c->head = me;
	c->tail = me;
	spinunlock(&c->lk);
	depth = m->depth;
	m->depth = 1;
	c_pthread_mutex_unlock(m);
	r = 0;
	ms = -1;
	if(deadline != 0){
		ms = (deadline - tnow(clock)) / 1000000;
		if(ms < 0)
			ms = 0;
	}
	if(!semdown(&me->wsem, ms)){
		spinlock(&c->lk);
		for(l = &c->head; *l != nil; l = &(*l)->wnext)
			if(*l == me)
				break;
		if(*l == me){
			/* still queued: off it, the time ran out */
			*l = me->wnext;
			if(c->tail == me){
				c->tail = nil;
				for(l = &c->head; *l != nil; l = &(*l)->wnext)
					c->tail = *l;
			}
			me->wnext = nil;
			spinunlock(&c->lk);
			r = Etimedout;
		}else{
			/* a signal took it: its wakeup is ours */
			spinunlock(&c->lk);
			semdown(&me->wsem, -1);
		}
	}
	c_pthread_mutex_lock(m);
	m->depth = depth;
	return r;
}

int
c_pthread_cond_wait(Cond *c, Mutex *m)
{
	return condwait(c, m, 0, 0);
}

/* abstime: a glibc struct timespec on the clock (timedwait: CLOCK_REALTIME) */
int
c_pthread_cond_timedwait(Cond *c, Mutex *m, vlong *abstime)
{
	return condwait(c, m, 0, abstime[0]*1000000000LL + abstime[1] + 1);
}

int
c_pthread_cond_clockwait(Cond *c, Mutex *m, int clock, vlong *abstime)
{
	return condwait(c, m, clock, abstime[0]*1000000000LL + abstime[1] + 1);
}

/*
 * Function-local statics: byte 0 of the guard says initialized, byte
 * 1 that a thread is initializing it; the others wait for that one.
 */

int
__cxa_guard_acquire(uchar *g)
{
	for(;;){
		spinlock(&guardlk);
		if(g[0]){
			spinunlock(&guardlk);
			return 0;
		}
		if(!g[1]){
			g[1] = 1;
			spinunlock(&guardlk);
			return 1;
		}
		spinunlock(&guardlk);
		tyield();
	}
}

void
__cxa_guard_release(uchar *g)
{
	spinlock(&guardlk);
	g[0] = 1;
	g[1] = 0;
	spinunlock(&guardlk);
}

void
__cxa_guard_abort(uchar *g)
{
	spinlock(&guardlk);
	g[1] = 0;
	spinunlock(&guardlk);
}
