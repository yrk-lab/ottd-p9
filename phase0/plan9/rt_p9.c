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

/* std::__libcpp_verbose_abort(char const*, ...) */
void
_ZNSt3__122__libcpp_verbose_abortEPKcz(char*, ...)
{
	fprint(2, "libc++ abort\n");
	abort();
}
