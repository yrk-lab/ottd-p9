/* Minimal C++ runtime shim for translated code (Phase 0, gcc stand-in for the Plan 9 compiler).
 * Symbols use the Itanium-mangled names the translated C refers to.  Exceptions are eh/eh_rt.c;
 * the std exception classes and typeinfo come from libc++abi, translated with the program. */
#include <stdlib.h>
#include <stdio.h>
void *_Znwm(unsigned long n)            { void *p = malloc(n ? n : 1); if (!p) abort(); return p; } /* operator new(size_t)   */
void *_Znam(unsigned long n)            { return _Znwm(n); }                                      /* operator new[](size_t) */
void  _ZdlPv(void *p)                   { free(p); }                                              /* operator delete(void*) */
void  _ZdaPv(void *p)                   { free(p); }                                              /* operator delete[](void*)*/
void  _ZdlPvm(void *p, unsigned long n) { (void)n; free(p); }                                     /* sized delete           */
void  _ZdaPvm(void *p, unsigned long n) { (void)n; free(p); }
void  __cxa_pure_virtual(void)          { fputs("pure virtual call\n", stderr); abort(); }
int   __cxa_atexit(void (*f)(void *), void *a, void *d) { (void)f; (void)a; (void)d; return 0; }
/* libc++ hardening/verbose-abort hook (called on assertion/throw paths when exceptions are off) */
void _ZNSt3__122__libcpp_verbose_abortEPKcz(const char *fmt, ...) { fputs("libc++ abort\n", stderr); abort(); }
/* Single-threaded atomics fallbacks (cbe lowers atomics to libcalls). Plan 9 port: processes, not threads. */
#include <stdint.h>
int __atomic_compare_exchange_8(volatile void *p, void *expected, uint64_t desired, int s, int f) {
	volatile uint64_t *q = p; uint64_t *e = expected; (void)s; (void)f;
	if (*q == *e) { *q = desired; return 1; } *e = *q; return 0;
}
/* Guards for function-local statics (single threaded): byte 0 of the guard is set once initialized. */
int  __cxa_guard_acquire(uint64_t *g) { return *(volatile char *)g == 0; }
void __cxa_guard_release(uint64_t *g) { *(volatile char *)g = 1; }
void __cxa_guard_abort(uint64_t *g)   { (void)g; }
/* operator new(size_t, const std::nothrow_t&), std::nothrow, std::__throw_bad_alloc (out of memory: abort) */
void *_ZnwmRKSt9nothrow_t(unsigned long n, const void *t) { (void)t; return malloc(n ? n : 1); }
void *_ZnamRKSt9nothrow_t(unsigned long n, const void *t) { (void)t; return malloc(n ? n : 1); }
const char _ZSt7nothrow = 0;
void _ZSt17__throw_bad_allocv(void) { fputs("out of memory\n", stderr); abort(); }
/* thread_local destructors are not run (one thread, like static ones above) */
int  __cxa_thread_atexit(void (*f)(void *), void *a, void *d) { (void)f; (void)a; (void)d; return 0; }
