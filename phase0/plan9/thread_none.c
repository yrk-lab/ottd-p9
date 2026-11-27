/*
 * Threads where there are none: goken's Linux-hosted libc has no
 * rfork'd processes sharing memory.  Thread creation fails, as
 * ../../phase1/nothreads.c makes it on Linux, and everything else is
 * the single-threaded case: one errno, one exception state
 * (../eh/eh_rt.h, EH_TLS).  The pthread calls are the program's,
 * renamed c_pthread_* by lower.py.  On 9front and with plan9port,
 * thread_p9.c has real threads.  Structures are glibc's x86-64
 * layouts.
 */
#include <u.h>
#include <libc.h>
#include "eh_rt.h"

static Ehtls	ehstate;

/* the program's main: no other stack needed */
void
p9runmain(void (*f)(void*), void *arg)
{
	f(arg);
}

Ehtls*
eh_self(void)
{
	return &ehstate;
}

static int	errnum;

int*
__errno_location(void)
{
	return &errnum;
}

/*
 * __atomic_* library calls (cbe lowers atomic instructions to them;
 * libc++abi's reference-counted strings use fetch_add).  The program
 * is single threaded, so plain memory operations are atomic enough.
 * The last argument(s) are the C11 memory orders, ignored.
 */

uint
__atomic_fetch_add_4(uint *p, uint v, int)
{
	uint o;

	o = *p;
	*p = o + v;
	return o;
}

uint
__atomic_fetch_sub_4(uint *p, uint v, int)
{
	uint o;

	o = *p;
	*p = o - v;
	return o;
}

uvlong
__atomic_fetch_add_8(uvlong *p, uvlong v, int)
{
	uvlong o;

	o = *p;
	*p = o + v;
	return o;
}

uvlong
__atomic_fetch_sub_8(uvlong *p, uvlong v, int)
{
	uvlong o;

	o = *p;
	*p = o - v;
	return o;
}

uchar
__atomic_load_1(uchar *p, int)
{
	return *p;
}

uint
__atomic_load_4(uint *p, int)
{
	return *p;
}

uvlong
__atomic_load_8(uvlong *p, int)
{
	return *p;
}

void
__atomic_store_4(uint *p, uint v, int)
{
	*p = v;
}

void
__atomic_store_8(uvlong *p, uvlong v, int)
{
	*p = v;
}

uint
__atomic_exchange_4(uint *p, uint v, int)
{
	uint o;

	o = *p;
	*p = v;
	return o;
}

uvlong
__atomic_exchange_8(uvlong *p, uvlong v, int)
{
	uvlong o;

	o = *p;
	*p = v;
	return o;
}

int
__atomic_compare_exchange_4(uint *p, uint *expected, uint desired, int, int)
{
	if(*p == *expected){
		*p = desired;
		return 1;
	}
	*expected = *p;
	return 0;
}

int
__atomic_compare_exchange_8(uvlong *p, uvlong *expected, uvlong desired, int, int)
{
	if(*p == *expected){
		*p = desired;
		return 1;
	}
	*expected = *p;
	return 0;
}

/*
 * Guards for function-local statics with dynamic initialization.
 * Single threaded: byte 0 of the guard is set once initialized.
 */

int
__cxa_guard_acquire(uvlong *g)
{
	return *(uchar*)g == 0;
}

void
__cxa_guard_release(uvlong *g)
{
	*(uchar*)g = 1;
}

void
__cxa_guard_abort(uvlong*)
{
}

/*
 * 1-byte atomics: the program is single threaded, see above.
 */

uchar
__atomic_exchange_1(uchar *p, uchar v, int)
{
	uchar o;

	o = *p;
	*p = v;
	return o;
}

uchar
__atomic_fetch_and_1(uchar *p, uchar v, int)
{
	uchar o;

	o = *p;
	*p = o & v;
	return o;
}

uchar
__atomic_fetch_or_1(uchar *p, uchar v, int)
{
	uchar o;

	o = *p;
	*p = o | v;
	return o;
}

void
__atomic_store_1(uchar *p, uchar v, int)
{
	*p = v;
}

/*
 * pthreads, single threaded.  Thread creation fails (Linux EAGAIN), as
 * ../../phase1/nothreads.c makes it on Linux: libc++'s std::thread then
 * throws std::system_error, OpenTTD's StartNewThread catches it and runs
 * the work inline.  Mutexes and condition variables have nothing to wait
 * for; thread keys are a small table.
 */

enum
{
	Eagain	= 11,
	Nkeys	= 64,
};

static void	*keyval[Nkeys];
static int	nkeys;

int
c_pthread_create(void*, void*, void *(*)(void*), void*)
{
	return Eagain;
}

int
c_pthread_join(uvlong, void**)
{
	return 0;
}

int
c_pthread_detach(uvlong)
{
	return 0;
}

uvlong
c_pthread_self(void)
{
	return 1;
}

int
c_pthread_setname_np(uvlong, char*)
{
	return 0;
}

int
c_pthread_key_create(uint *key, void (*)(void*))
{
	if(nkeys == Nkeys)
		return Eagain;
	*key = nkeys++;
	return 0;
}

int
c_pthread_setspecific(uint key, void *v)
{
	if(key >= nkeys)
		return 22;	/* EINVAL */
	keyval[key] = v;
	return 0;
}

void*
c_pthread_getspecific(uint key)
{
	if(key >= nkeys)
		return nil;
	return keyval[key];
}

int
c_pthread_mutex_init(void*, void*)
{
	return 0;
}

int
c_pthread_mutex_destroy(void*)
{
	return 0;
}

int
c_pthread_mutex_lock(void*)
{
	return 0;
}

int
c_pthread_mutex_unlock(void*)
{
	return 0;
}

int
c_pthread_mutexattr_init(void*)
{
	return 0;
}

int
c_pthread_mutexattr_destroy(void*)
{
	return 0;
}

int
c_pthread_mutexattr_settype(void*, int)
{
	return 0;
}

int
c_pthread_cond_broadcast(void*)
{
	return 0;
}

int
c_pthread_cond_destroy(void*)
{
	return 0;
}

/* no other thread can signal: waiting would never end, return at once */
int
c_pthread_cond_wait(void*, void*)
{
	return 0;
}

int
c_pthread_cond_clockwait(void*, void*, int, void*)
{
	return 0;
}

int
c_pthread_cond_signal(void*)
{
	return 0;
}

/* no other thread could signal: the time runs out */
int
c_pthread_cond_timedwait(void*, void*, void*)
{
	return 110;	/* ETIMEDOUT */
}

int
c_pthread_mutex_trylock(void*)
{
	return 0;
}
