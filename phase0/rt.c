/*
 * Minimal C++ runtime shim for translated code (Phase 0, gcc stand-in for
 * the Plan 9 compiler).  Symbols use the Itanium-mangled names the
 * translated C refers to.  Exceptions are eh/eh_rt.c; the std exception
 * classes and typeinfo come from libc++abi, translated with the program.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* operator new(size_t) */
void*
_Znwm(unsigned long n)
{
	void *p = malloc(n ? n : 1);

	if (!p)
		abort();
	return p;
}

/* operator new[](size_t) */
void*
_Znam(unsigned long n)
{
	return _Znwm(n);
}

/* operator delete(void*) */
void
_ZdlPv(void *p)
{
	free(p);
}

/* operator delete[](void*) */
void
_ZdaPv(void *p)
{
	free(p);
}

/* operator delete(void*, size_t) */
void
_ZdlPvm(void *p, unsigned long n)
{
	(void)n;
	free(p);
}

/* operator delete[](void*, size_t) */
void
_ZdaPvm(void *p, unsigned long n)
{
	(void)n;
	free(p);
}

void
__cxa_pure_virtual(void)
{
	fputs("pure virtual call\n", stderr);
	abort();
}

/* static destructors are not run */
int
__cxa_atexit(void (*f)(void *), void *a, void *d)
{
	(void)f;
	(void)a;
	(void)d;
	return 0;
}

/* libc++ hardening/verbose-abort hook (assertion and throw paths when exceptions are off) */
void
_ZNSt3__122__libcpp_verbose_abortEPKcz(const char *fmt, ...)
{
	fputs("libc++ abort\n", stderr);
	abort();
}

/*
 * Single-threaded atomics fallback (cbe lowers atomics to library calls).
 * The Plan 9 port runs one process, no threads.
 */
int
__atomic_compare_exchange_8(volatile void *p, void *expected, uint64_t desired, int s, int f)
{
	volatile uint64_t *q = p;
	uint64_t *e = expected;

	(void)s;
	(void)f;
	if (*q == *e) {
		*q = desired;
		return 1;
	}
	*e = *q;
	return 0;
}

/* Guards for function-local statics (single threaded): byte 0 of the guard is set once initialized. */
int
__cxa_guard_acquire(uint64_t *g)
{
	return *(volatile char *)g == 0;
}

void
__cxa_guard_release(uint64_t *g)
{
	*(volatile char *)g = 1;
}

void
__cxa_guard_abort(uint64_t *g)
{
	(void)g;
}

/* operator new(size_t, const std::nothrow_t&) and new[] */
void*
_ZnwmRKSt9nothrow_t(unsigned long n, const void *t)
{
	(void)t;
	return malloc(n ? n : 1);
}

void*
_ZnamRKSt9nothrow_t(unsigned long n, const void *t)
{
	(void)t;
	return malloc(n ? n : 1);
}

/* std::nothrow */
const char _ZSt7nothrow = 0;

/* std::__throw_bad_alloc(): out of memory */
void
_ZSt17__throw_bad_allocv(void)
{
	fputs("out of memory\n", stderr);
	abort();
}

/* thread_local destructors are not run (one thread, like static ones above) */
int
__cxa_thread_atexit(void (*f)(void *), void *a, void *d)
{
	(void)f;
	(void)a;
	(void)d;
	return 0;
}

/*
 * std::exception_ptr, for what libc++ links (std::future), as
 * plan9/rt_p9.c: with no threads there is never an exception to carry
 * over, so the pointer is always null.
 */

void
_ZNSt13exception_ptrC1ERKS_(void **p, void **q)
{
	*p = *q;
}

void
_ZNSt13exception_ptrD1Ev(void **p)
{
	(void)p;
}

void**
_ZNSt13exception_ptraSERKS_(void **p, void **q)
{
	*p = *q;
	return p;
}

void
_ZSt17current_exceptionv(void **r)
{
	*r = NULL;
}

void
_ZSt17rethrow_exceptionSt13exception_ptr(void **p)
{
	(void)p;
	abort();
}
