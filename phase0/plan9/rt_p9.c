/*
 * C++ runtime for translated programs, on Plan 9 libc:
 * operator new and delete, and the few libc++ / Itanium ABI
 * entry points the tests reach.  Same role as ../rt.c on Linux.
 * The C library side (printf, bcmp, ...) is in libc_p9.c.
 */
#include <u.h>
#include <libc.h>

/* operator new(size_t) */
void*
_Znwm(uvlong n)
{
	void *p;

	if(n == 0)
		n = 1;
	p = malloc(n);
	if(p == nil)
		abort();
	return p;
}

/* operator new[](size_t) */
void*
_Znam(uvlong n)
{
	return _Znwm(n);
}

/* operator new(size_t, const std::nothrow_t&) and new[] */
void*
_ZnwmRKSt9nothrow_t(uvlong n, void*)
{
	return malloc(n ? n : 1);
}

void*
_ZnamRKSt9nothrow_t(uvlong n, void*)
{
	return malloc(n ? n : 1);
}

/* std::nothrow */
char	_ZSt7nothrow;

/* std::__throw_bad_alloc(): out of memory */
void
_ZSt17__throw_bad_allocv(void)
{
	fprint(2, "out of memory\n");
	abort();
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
_ZdlPvm(void *p, uvlong)
{
	free(p);
}

/* operator delete[](void*, size_t) */
void
_ZdaPvm(void *p, uvlong)
{
	free(p);
}

void
__cxa_pure_virtual(void)
{
	fprint(2, "pure virtual call\n");
	abort();
}

/* static destructors are not run */
int
__cxa_atexit(void (*)(void*), void*, void*)
{
	return 0;
}

/* thread_local destructors are not run either (one thread) */
int
__cxa_thread_atexit(void (*)(void*), void*, void*)
{
	return 0;
}

/* identifies this module in __cxa_atexit registrations */
void	*__dso_handle = &__dso_handle;

/* std::__libcpp_verbose_abort(char const*, ...) */
void
_ZNSt3__122__libcpp_verbose_abortEPKcz(char*, ...)
{
	fprint(2, "libc++ abort\n");
	abort();
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
