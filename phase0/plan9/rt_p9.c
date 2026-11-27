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
 * std::exception_ptr, for what libc++ links (std::future): with no
 * threads there is never an exception to carry over, so the pointer
 * is always null.  An exception_ptr is one pointer; it has a
 * destructor, so it is returned and passed by address.
 */

void
_ZNSt13exception_ptrC1ERKS_(void **p, void **q)
{
	*p = *q;
}

void
_ZNSt13exception_ptrD1Ev(void**)
{
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
	*r = nil;
}

void
_ZSt17rethrow_exceptionSt13exception_ptr(void**)
{
	abort();
}
